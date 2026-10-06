#pragma once
#include <GL/glew.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>

namespace rendering {
enum class GpuWorkStage { TerrainField, TerrainExpansion, GrassMetadata, GrassAllocation, GrassPlacement };
enum class GpuWorkView { None, Main, Reflection, Unspecified };
inline constexpr std::array<const char*,4> gpuWorkViewNames{"", "main", "reflection", "unspecified"};
inline constexpr std::array<const char*,5> gpuWorkStageNames{
    "terrain_field", "terrain_expansion", "grass_metadata", "grass_allocation", "grass_placement"};
struct GpuWorkIdentity {
    std::uint64_t epoch=0,serial=0,body=std::numeric_limits<std::uint64_t>::max(),field=0,topology=0;
    std::uint32_t fieldVersion=0,topologyVersion=0,backend=0;
    std::uint64_t attempt=0;
};
// Optional context-thread timing. A fixed query pool owns submissions independently
// of terrain resources, so cancellation/retirement cannot lose or block a sample.
// These intervals can be nested in frame stages; never add them to gpu_ms.
class GpuWorkProfiler {
    using Clock=std::chrono::steady_clock;
    struct Event {
        std::array<GLuint,2> queries{};
        GpuWorkIdentity identity;
        Clock::time_point start;
        std::uint64_t number=0,frame=0,polls=0;
        GpuWorkStage stage=GpuWorkStage::TerrainField;
        GpuWorkView view=GpuWorkView::None;
        double cpuMs=0;
        bool active=false,pending=false,failed=false;
    };
public:
    static constexpr std::size_t capacity=64;
    struct Stats { std::uint64_t submitted=0,ready=0,dropped=0,missing=0,pending=0,peakPending=0; };
    explicit GpuWorkProfiler(const std::string& path="") {
        if(path.empty()) return;
        output_.open(path);
        if(!output_) throw std::runtime_error("Cannot open GPU work trace: "+path);
        output_ << "event,frame,stage,epoch,request_serial,body,field,topology,field_version,topology_version,backend,identity_valid,status,cpu_submit_ms,gpu_ms,query_polls,view,attempt\n"
                << std::setprecision(9);
    }
    ~GpuWorkProfiler() {
        collect();
        for(auto& e:events_) {
            if(e.pending || e.active) {write(e,"missing_shutdown",false);++stats_.missing;}
            if(e.queries[0]) glDeleteQueries(2,e.queries.data());
        }
    }
    GpuWorkProfiler(const GpuWorkProfiler&)=delete;
    GpuWorkProfiler& operator=(const GpuWorkProfiler&)=delete;
    bool enabled() const {return output_.is_open();}
    const Stats& stats() const {return stats_;}
    void frame(std::uint64_t number) {frame_=number;}
    void collect() {
        if(!enabled()) return;
        for(auto& e:events_) if(e.pending) {
            ++e.polls;
            GLint a=0,b=0;
            glGetQueryObjectiv(e.queries[0],GL_QUERY_RESULT_AVAILABLE,&a);
            glGetQueryObjectiv(e.queries[1],GL_QUERY_RESULT_AVAILABLE,&b);
            if(!a || !b) continue;
            GLuint64 start=0,end=0;
            glGetQueryObjectui64v(e.queries[0],GL_QUERY_RESULT,&start);
            glGetQueryObjectui64v(e.queries[1],GL_QUERY_RESULT,&end);
            write(e,e.failed ? "failed_ready" : "ready",true,end>=start ? (end-start)*1e-6 : 0);
            e.pending=false;--stats_.pending;++stats_.ready;
        }
    }
    // Bind only while the owning GL context is current. Nested bindings restore
    // the prior owner/identity; CPU worker threads never inherit the binding.
    class Binding {
        GpuWorkProfiler* previous_;
        GpuWorkIdentity previousIdentity_;
        GpuWorkProfiler* owner_=nullptr;
        std::uint64_t previousFrame_=0;
        bool overrideFrame_=false;
    public:
        explicit Binding(GpuWorkProfiler* owner,std::optional<std::uint64_t> frame=std::nullopt):previous_(current_),previousIdentity_(identity_) {
            current_=owner && owner->enabled() ? owner : nullptr;identity_={};
            owner_=current_;
            if(owner_ && frame) {previousFrame_=owner_->frame_;owner_->frame_=*frame;overrideFrame_=true;}
        }
        ~Binding() {
            if(overrideFrame_) owner_->frame_=previousFrame_;
            current_=previous_;identity_=previousIdentity_;
        }
        Binding(const Binding&)=delete;
        Binding& operator=(const Binding&)=delete;
    };
    class Request {
        GpuWorkIdentity previous_;
    public:
        Request(std::uint64_t epoch,std::uint64_t serial,std::uint64_t body):previous_(identity_) {
            identity_={};identity_.attempt=previous_.attempt;identity_.epoch=epoch;identity_.serial=serial;identity_.body=body;
        }
        ~Request() {identity_=previous_;}
        Request(const Request&)=delete;
        Request& operator=(const Request&)=delete;
    };
    class Attempt {
        std::uint64_t previous_;
    public:
        explicit Attempt(std::uint64_t attempt):previous_(identity_.attempt) {identity_.attempt=attempt;}
        ~Attempt() {identity_.attempt=previous_;}
        Attempt(const Attempt&)=delete;
        Attempt& operator=(const Attempt&)=delete;
    };
    static GpuWorkIdentity identity() {return identity_;}
    template<class Key> static GpuWorkIdentity generation(const Key& key,GpuWorkIdentity identity=identity_) {
        identity.field=key.field;identity.topology=key.topology;
        identity.fieldVersion=key.fieldVersion;identity.topologyVersion=key.topologyVersion;
        identity.backend=static_cast<std::uint32_t>(key.backend);return identity;
    }
    class Scope {
        GpuWorkProfiler* owner_;
        int token_=-1,exceptions_=std::uncaught_exceptions();
    public:
        Scope(GpuWorkStage stage,GpuWorkIdentity identity,GpuWorkView view=GpuWorkView::None):owner_(current_) {
            if(owner_) token_=owner_->begin(stage,identity,view);
        }
        ~Scope() {stop();}
        void stop() {
            if(owner_ && token_>=0) owner_->end(token_,std::uncaught_exceptions()>exceptions_);
            owner_=nullptr;
        }
        Scope(const Scope&)=delete;
        Scope& operator=(const Scope&)=delete;
    };
private:
    inline static thread_local GpuWorkProfiler* current_=nullptr;
    inline static thread_local GpuWorkIdentity identity_{};
    std::array<Event,capacity> events_{};
    std::ofstream output_;
    Stats stats_;
    std::uint64_t frame_=0,next_=0;
    int begin(GpuWorkStage stage,GpuWorkIdentity identity,GpuWorkView view) {
        Event initial;initial.number=next_++;initial.frame=frame_;initial.stage=stage;initial.identity=identity;initial.view=view;
        for(std::size_t i=0;i<events_.size();++i) if(!events_[i].pending && !events_[i].active) {
            auto& e=events_[i];initial.queries=e.queries;e=initial;
            if(!e.queries[0]) glGenQueries(2,e.queries.data());
            if(!e.queries[0] || !e.queries[1]) {
                if(e.queries[0] || e.queries[1]) glDeleteQueries(2,e.queries.data());
                e.queries={};write(e,"allocation_failed",false);++stats_.dropped;return -1;
            }
            e.start=Clock::now();e.active=true;
            glQueryCounter(e.queries[0],GL_TIMESTAMP);++stats_.submitted;return static_cast<int>(i);
        }
        write(initial,"dropped",false);++stats_.dropped;return -1;
    }
    void end(int token,bool failed) {
        auto& e=events_[token];
        e.cpuMs=std::chrono::duration<double,std::milli>(Clock::now()-e.start).count();
        glQueryCounter(e.queries[1],GL_TIMESTAMP);e.active=false;e.pending=true;e.failed=failed;
        ++stats_.pending;stats_.peakPending=std::max(stats_.peakPending,stats_.pending);
    }
    void write(const Event& e,const char* status,bool valid,double gpuMs=0) {
        const auto& k=e.identity;
        output_ << e.number << ',';
        if(e.frame!=std::numeric_limits<std::uint64_t>::max()) output_ << e.frame;
        output_ << ',' << gpuWorkStageNames[static_cast<int>(e.stage)] << ','
                << k.epoch << ',' << k.serial << ',';
        if(k.body!=std::numeric_limits<std::uint64_t>::max()) output_ << k.body;
        output_ << ',' << k.field << ',' << k.topology << ',' << k.fieldVersion << ',' << k.topologyVersion << ','
                << k.backend << ',' << bool(k.epoch && k.serial && k.body!=std::numeric_limits<std::uint64_t>::max())
                << ',' << status << ',' << e.cpuMs << ',';
        if(valid) output_ << gpuMs;
        output_ << ',' << e.polls << ',' << gpuWorkViewNames[static_cast<int>(e.view)] << ',' << k.attempt << '\n';
    }
};
}

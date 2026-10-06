#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <exception>
#include <limits>
#include <mutex>
#include <string>

namespace rendering {
// Scalar-only receipts. Workers update bounded metadata; only the context
// thread drains CSV rows. No GL calls, waits, futures or scene ownership.
class PublicationProfiler {
public:
    using Clock=std::chrono::steady_clock;
    using Now=Clock::time_point (*)();
    static constexpr std::size_t capacity=64;
    static constexpr std::uint64_t unknown=std::numeric_limits<std::uint64_t>::max();
    enum class Kind { Terrain, Grass, Reload, Handoff };
    enum class Outcome { Published, Rejected, Coalesced, Obsolete, CpuFailed, PreparationFailed,
                         ReplacedBeforeDraw, Shutdown, MissingShutdown, InvalidConfig, Superseded, TraceIncomplete };
    enum class Phase { CpuStart, CpuEnd, GpuSubmit, GpuReady, Count };
    struct Key {
        std::uint64_t epoch=0,serial=0,body=unknown,nameHash=0,field=0;
        std::uint32_t fieldVersion=0,topologyVersion=0,backend=0;
        bool resident=false;
        int mask=0;
        std::array<double,3> eye{};
        template<class Identity> static Key from(const Identity& k) {
            std::uint64_t hash=14695981039346656037ull;
            for(unsigned char c:k.bodyName) {hash^=c;hash*=1099511628211ull;}
            return {k.epoch,k.serial,k.bodyIndex,hash,k.field,k.fieldVersion,k.topologyVersion,
                static_cast<std::uint32_t>(k.backend),k.resident,k.localMask,{k.eye.x,k.eye.y,k.eye.z}};
        }
    };
    struct Generation {
        std::uint64_t landField=0,landTopology=0,waterField=0,waterTopology=0,landRevision=0,waterRevision=0;
        std::array<double,3> grassEye{};
        bool operator==(const Generation&) const = default;
    };
    struct Stats {std::uint64_t begun=0,finished=0,dropped=0;std::size_t active=0,peakActive=0,pendingRows=0;};
    explicit PublicationProfiler(const std::string& path="",Now now=Clock::now);
    ~PublicationProfiler();
    PublicationProfiler(const PublicationProfiler&)=delete;
    PublicationProfiler& operator=(const PublicationProfiler&)=delete;
    bool enabled() const {return output_.is_open();}
    void frame(std::uint64_t number);
    void captureMode(bool capture);
    std::uint64_t begin(Key key,Kind kind=Kind::Terrain,std::uint64_t parent=0);
    void phase(std::uint64_t attempt,Phase phase);
    void prepared(std::uint64_t attempt,Generation generation);
    void finish(std::uint64_t attempt,Outcome outcome);
    // Called only after the complete consumer draw, never at GPU readiness.
    void rendered(std::uint64_t epoch,std::uint64_t body,std::uint64_t serial,Generation generation,bool characterContacts=false);
    void contactBound(std::uint64_t attempt,std::uint64_t origin,Generation generation);
    void captureFailed(std::uint64_t epoch);
    // Reload children stay off-live until committed. Only a later complete
    // scene draw can publish the root, including a scene with zero bodies.
    std::uint64_t child(std::uint64_t parent,std::uint64_t body) const;
    std::uint64_t terrain(std::uint64_t epoch,std::uint64_t body,std::uint64_t serial) const;
    void committed(std::uint64_t parent);
    void sceneRendered(std::uint64_t epoch);
    void discardOlderEpochs(std::uint64_t epoch);
    void collect(); // Context thread only; never called by the CPU worker.
    Stats stats() const;
    // Stack-only failure closure for admitted synchronous preparations. A ready
    // receipt remains active until the consumer endpoint, not scope destruction.
    class Preparation {
        PublicationProfiler* owner_;
        std::uint64_t attempt_;
        bool ready_=false;
    public:
        Preparation(PublicationProfiler* owner,std::uint64_t attempt):owner_(owner),attempt_(attempt) {}
        ~Preparation() {if(owner_ && !ready_) owner_->finish(attempt_,Outcome::PreparationFailed);}
        Preparation(const Preparation&)=delete;
        void ready(Generation generation) {if(owner_) owner_->prepared(attempt_,generation);ready_=true;}
        void bound(std::uint64_t origin,Generation generation) {if(owner_) owner_->contactBound(attempt_,origin,generation);ready_=true;}
    };
    class CaptureFailure {
        PublicationProfiler* owner_;
        std::uint64_t epoch_;
        int exceptions_=std::uncaught_exceptions();
    public:
        CaptureFailure(PublicationProfiler& owner,std::uint64_t epoch):owner_(owner.enabled() ? &owner : nullptr),epoch_(epoch) {}
        ~CaptureFailure() {if(owner_ && std::uncaught_exceptions()>exceptions_) owner_->captureFailed(epoch_);}
        CaptureFailure(const CaptureFailure&)=delete;
    };
private:
    struct Record {
        std::uint64_t attempt=0,startFrame=unknown,endFrame=unknown;
        std::uint64_t parent=0,children=0,pendingChildren=0;
        std::uint64_t origin=unknown;
        Key key;
        Kind kind=Kind::Terrain;
        Outcome outcome=Outcome::MissingShutdown;
        Clock::time_point start{},end{};
        std::array<double,static_cast<int>(Phase::Count)> phases{-1,-1,-1,-1};
        double preparedMs=-1;
        double exchangeMs=-1;
        double contactMs=-1;
        Generation generation;
        bool complete=false;
        bool incomplete=false;
        bool capture=false;
    };
    std::ofstream output_;
    Now now_;
    mutable std::mutex mutex_;
    std::array<Record,capacity> records_{};
    std::uint64_t next_=1,frame_=unknown,undrainedDrops_=0;
    bool capture_=false;
    Stats stats_;
    Record* find(std::uint64_t attempt);
    void close(Record& record,Outcome outcome);
    void write(const Record& record);
};
}

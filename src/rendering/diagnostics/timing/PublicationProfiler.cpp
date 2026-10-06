#include "rendering/diagnostics/timing/PublicationProfiler.h"
#include <algorithm>
#include <iomanip>
#include <stdexcept>

namespace rendering {
namespace {
double milliseconds(PublicationProfiler::Clock::duration d) {return std::chrono::duration<double,std::milli>(d).count();}
bool sameTerrain(const PublicationProfiler::Generation& a,const PublicationProfiler::Generation& b) {
    return a.landField==b.landField && a.landTopology==b.landTopology &&
        a.waterField==b.waterField && a.waterTopology==b.waterTopology &&
        a.landRevision==b.landRevision && a.waterRevision==b.waterRevision;
}
constexpr std::array kinds{"terrain","grass","reload","handoff"};
constexpr std::array outcomes{"published","rejected","coalesced","obsolete","cpu_failed","preparation_failed",
    "replaced_before_draw","shutdown","missing_shutdown","invalid_config","superseded","trace_incomplete"};
}
PublicationProfiler::PublicationProfiler(const std::string& path,Now now):now_(now) {
    if(path.empty()) return;
    output_.open(path);
    if(!output_) throw std::runtime_error("Cannot open publication trace: "+path);
    output_ << "attempt,kind,epoch,request_serial,body,body_name_hash,field,field_version,topology_version,backend,resident,local_mask,eye_x,eye_y,eye_z,start_frame,end_frame,elapsed_ms,elapsed_frames,publication_ms,publication_frames,outcome,cpu_start_ms,cpu_end_ms,gpu_submit_ms,gpu_ready_ms,prepared_ms,land_field,land_topology,water_field,water_topology,land_revision,water_revision,grass_eye_x,grass_eye_y,grass_eye_z,dropped_starts,parent_attempt,scene_exchange_ms,child_attempts,capture_mode,origin_body,contact_bound_ms\n" << std::setprecision(17);
}
PublicationProfiler::~PublicationProfiler() {
    if(!enabled()) return;
    {std::lock_guard lock(mutex_);for(auto& r:records_) if(r.attempt && !r.complete) close(r,Outcome::MissingShutdown);}
    collect();
}
void PublicationProfiler::frame(std::uint64_t number) {
    if(!enabled()) return;
    std::lock_guard lock(mutex_);frame_=number;
}
void PublicationProfiler::captureMode(bool capture) {
    if(!enabled()) return;
    std::lock_guard lock(mutex_);capture_=capture;
}
std::uint64_t PublicationProfiler::begin(Key key,Kind kind,std::uint64_t parent) {
    if(!enabled()) return 0;
    std::lock_guard lock(mutex_);
    auto* root=find(parent);
    if(parent && (!root || root->kind!=Kind::Reload || root->exchangeMs>=0 || kind==Kind::Reload)) return 0;
    for(auto& r:records_) if(!r.attempt && next_!=unknown) {
        r=Record{};r.attempt=next_++;r.key=key;r.kind=kind;r.start=now_();r.startFrame=frame_;r.parent=parent;r.capture=capture_;
        if(root) {++root->children;++root->pendingChildren;}
        ++stats_.begun;++stats_.active;stats_.peakActive=std::max(stats_.peakActive,stats_.active);return r.attempt;
    }
    if(root) root->incomplete=true;
    ++stats_.dropped;++undrainedDrops_;return 0;
}
PublicationProfiler::Record* PublicationProfiler::find(std::uint64_t attempt) {
    if(!attempt) return nullptr;
    for(auto& r:records_) if(r.attempt==attempt && !r.complete) return &r;
    return nullptr; // Late worker notifications never attach to a reused slot.
}
void PublicationProfiler::phase(std::uint64_t attempt,Phase phase) {
    if(!enabled() || !attempt) return;
    std::lock_guard lock(mutex_);
    if(auto* r=find(attempt)) {
        auto& value=r->phases.at(static_cast<int>(phase));if(value<0) value=milliseconds(now_()-r->start);
    }
}
void PublicationProfiler::close(Record& r,Outcome outcome) {
    r.complete=true;r.outcome=outcome;r.end=now_();r.endFrame=frame_;
    ++stats_.finished;--stats_.active;++stats_.pendingRows;
    if(auto* root=find(r.parent)) {--root->pendingChildren;if(outcome!=Outcome::Published) root->incomplete=true;}
}
void PublicationProfiler::finish(std::uint64_t attempt,Outcome outcome) {
    if(!enabled() || !attempt) return;
    // Success is only available through a matching rendered generation.
    if(outcome==Outcome::Published) throw std::invalid_argument("Publication requires a complete consumer draw");
    std::lock_guard lock(mutex_);
    if(auto* r=find(attempt)) {
        for(auto& c:records_) if(c.attempt && !c.complete && c.parent==attempt) close(c,outcome);
        close(*r,outcome);
    }
}
void PublicationProfiler::prepared(std::uint64_t attempt,Generation generation) {
    if(!enabled() || !attempt) return;
    std::lock_guard lock(mutex_);
    auto* current=find(attempt);if(!current || current->kind==Kind::Reload) return;
    for(auto& r:records_) if(r.attempt && !r.complete && r.attempt!=attempt && r.preparedMs>=0 &&
        r.key.epoch==current->key.epoch && r.key.body==current->key.body && current->kind!=Kind::Handoff) {
        // A committed reload child can be replaced by capture setup with no
        // parent. Equivalent grass preparation still permits its matching draw;
        // a different serial/consumer generation closes the undrawn child.
        const auto* root=find(r.parent);
        const bool liveChildReplaced=root && root->exchangeMs>=0 &&
            (r.key.serial!=current->key.serial || (r.generation!=generation &&
                (r.key.resident || !sameTerrain(r.generation,generation))));
        if(r.kind==Kind::Handoff ? !sameTerrain(r.generation,generation) :
            r.parent==current->parent || liveChildReplaced)
            close(r,Outcome::ReplacedBeforeDraw);
    }
    current->generation=generation;current->preparedMs=milliseconds(now_()-current->start);
}
void PublicationProfiler::rendered(std::uint64_t epoch,std::uint64_t body,std::uint64_t serial,Generation generation,bool characterContacts) {
    if(!enabled()) return;
    std::lock_guard lock(mutex_);
    for(auto& r:records_) if(r.attempt && !r.complete && r.kind!=Kind::Reload && r.preparedMs>=0 &&
        r.key.epoch==epoch && r.key.body==body && r.key.serial==serial &&
        (r.kind!=Kind::Handoff || characterContacts) &&
        // CPU terrain admission precedes grass planning; its interval includes
        // that preparation but cannot require the previously installed anchor.
        (r.generation==generation || (((!r.key.resident && r.kind==Kind::Terrain) || r.kind==Kind::Handoff) && sameTerrain(r.generation,generation))) &&
        (!r.parent || (find(r.parent) && find(r.parent)->exchangeMs>=0))) close(r,Outcome::Published);
}
void PublicationProfiler::contactBound(std::uint64_t attempt,std::uint64_t origin,Generation generation) {
    if(!enabled() || !attempt) return;
    std::lock_guard lock(mutex_);
    if(auto* r=find(attempt);r && r->kind==Kind::Handoff) {
        for(auto& old:records_) if(old.attempt && !old.complete && old.attempt!=attempt && old.kind==Kind::Handoff &&
            old.preparedMs>=0 && old.key.epoch==r->key.epoch && old.key.body==r->key.body)
            close(old,Outcome::ReplacedBeforeDraw);
        r->origin=origin;r->generation=generation;
        r->contactMs=r->preparedMs=milliseconds(now_()-r->start);
    }
}
void PublicationProfiler::captureFailed(std::uint64_t epoch) {
    if(!enabled()) return;
    std::lock_guard lock(mutex_);
    for(auto& r:records_) if(r.attempt && !r.complete && r.capture && r.key.epoch==epoch)
        close(r,Outcome::PreparationFailed);
}
std::uint64_t PublicationProfiler::child(std::uint64_t parent,std::uint64_t body) const {
    if(!enabled() || !parent) return 0;
    std::lock_guard lock(mutex_);
    for(const auto& r:records_) if(r.attempt && !r.complete && r.parent==parent && r.key.body==body) return r.attempt;
    return 0;
}
void PublicationProfiler::committed(std::uint64_t parent) {
    if(!enabled() || !parent) return;
    std::lock_guard lock(mutex_);
    if(auto* r=find(parent);r && r->kind==Kind::Reload && r->exchangeMs<0) r->exchangeMs=milliseconds(now_()-r->start);
}
std::uint64_t PublicationProfiler::terrain(std::uint64_t epoch,std::uint64_t body,std::uint64_t serial) const {
    if(!enabled()) return 0;
    std::lock_guard lock(mutex_);
    for(const auto& r:records_) if(r.attempt && !r.complete && r.kind==Kind::Terrain &&
        r.key.epoch==epoch && r.key.body==body && r.key.serial==serial) return r.attempt;
    return 0;
}
void PublicationProfiler::sceneRendered(std::uint64_t epoch) {
    if(!enabled()) return;
    std::lock_guard lock(mutex_);
    for(auto& r:records_) if(r.attempt && !r.complete && r.kind==Kind::Reload && r.key.epoch==epoch &&
        r.exchangeMs>=0 && !r.pendingChildren) close(r,r.incomplete ? Outcome::TraceIncomplete : Outcome::Published);
}
void PublicationProfiler::discardOlderEpochs(std::uint64_t epoch) {
    if(!enabled()) return;
    std::lock_guard lock(mutex_);
    for(auto& r:records_) if(r.attempt && !r.complete && r.key.epoch<epoch) close(r,Outcome::Obsolete);
}
PublicationProfiler::Stats PublicationProfiler::stats() const {std::lock_guard lock(mutex_);return stats_;}
void PublicationProfiler::collect() {
    if(!enabled()) return;
    std::array<Record,capacity> completed{};std::size_t count=0;std::uint64_t drops=0;
    {std::lock_guard lock(mutex_);
        for(auto& r:records_) if(r.attempt && r.complete) {completed[count++]=r;r=Record{};--stats_.pendingRows;}
        drops=undrainedDrops_;undrainedDrops_=0;
    }
    for(std::size_t i=0;i<count;++i) write(completed[i]);
    // Overflow is missing evidence, never a successful zero-latency request.
    if(drops) {output_ << "0";for(int i=1;i<43;++i) {output_ << ',';if(i==21) output_ << "trace_overflow";if(i==36) output_ << drops;}output_ << '\n';}
    output_.flush();
}
void PublicationProfiler::write(const Record& r) {
    const auto& k=r.key;
    output_ << r.attempt << ',' << kinds[static_cast<int>(r.kind)] << ',' << k.epoch << ',' << k.serial << ',';
    if(k.body!=unknown) output_ << k.body;
    output_ << ',' << k.nameHash << ',' << k.field << ',' << k.fieldVersion << ',' << k.topologyVersion << ',' << k.backend << ',' << k.resident << ',' << k.mask;
    for(auto x:k.eye) output_ << ',' << x;
    const bool frames=r.startFrame!=unknown && r.endFrame!=unknown && r.endFrame>=r.startFrame;
    output_ << ',';if(r.startFrame!=unknown) output_ << r.startFrame;
    output_ << ',';if(r.endFrame!=unknown) output_ << r.endFrame;
    const auto elapsed=milliseconds(r.end-r.start);
    output_ << ',' << elapsed << ',';if(frames) output_ << r.endFrame-r.startFrame;
    output_ << ',';if(r.outcome==Outcome::Published) output_ << elapsed;
    output_ << ',';if(r.outcome==Outcome::Published && frames) output_ << r.endFrame-r.startFrame;
    output_ << ',' << outcomes[static_cast<int>(r.outcome)];
    for(auto value:r.phases) {output_ << ',';if(value>=0) output_ << value;}
    output_ << ',';if(r.preparedMs>=0) output_ << r.preparedMs;
    const auto& g=r.generation;
    for(auto value:{g.landField,g.landTopology,g.waterField,g.waterTopology,g.landRevision,g.waterRevision}) {output_ << ',';if(r.preparedMs>=0) output_ << value;}
    for(auto value:g.grassEye) {output_ << ',';if(r.preparedMs>=0 && (k.resident || r.kind==Kind::Grass)) output_ << value;}
    output_ << ",0,";if(r.parent) output_ << r.parent;
    output_ << ',';if(r.exchangeMs>=0) output_ << r.exchangeMs;
    output_ << ',';if(r.kind==Kind::Reload) output_ << r.children;
    output_ << ',' << r.capture << ',';if(r.origin!=unknown) output_ << r.origin;
    output_ << ',';if(r.contactMs>=0) output_ << r.contactMs;
    output_ << '\n';
}
}

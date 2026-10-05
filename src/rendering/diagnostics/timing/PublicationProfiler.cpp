#include "rendering/diagnostics/timing/PublicationProfiler.h"
#include <algorithm>
#include <iomanip>
#include <stdexcept>

namespace rendering {
namespace {
double milliseconds(PublicationProfiler::Clock::duration d) {return std::chrono::duration<double,std::milli>(d).count();}
constexpr std::array kinds{"terrain","grass"};
constexpr std::array outcomes{"published","rejected","coalesced","obsolete","cpu_failed","preparation_failed",
    "replaced_before_draw","shutdown","missing_shutdown"};
}
PublicationProfiler::PublicationProfiler(const std::string& path,Now now):now_(now) {
    if(path.empty()) return;
    output_.open(path);
    if(!output_) throw std::runtime_error("Cannot open publication trace: "+path);
    output_ << "attempt,kind,epoch,request_serial,body,body_name_hash,field,field_version,topology_version,backend,resident,local_mask,eye_x,eye_y,eye_z,start_frame,end_frame,elapsed_ms,elapsed_frames,publication_ms,publication_frames,outcome,cpu_start_ms,cpu_end_ms,gpu_submit_ms,gpu_ready_ms,prepared_ms,land_field,land_topology,water_field,water_topology,land_revision,water_revision,grass_eye_x,grass_eye_y,grass_eye_z,dropped_starts\n" << std::setprecision(17);
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
std::uint64_t PublicationProfiler::begin(Key key,Kind kind) {
    if(!enabled()) return 0;
    std::lock_guard lock(mutex_);
    for(auto& r:records_) if(!r.attempt && next_!=unknown) {
        r=Record{};r.attempt=next_++;r.key=key;r.kind=kind;r.start=now_();r.startFrame=frame_;
        ++stats_.begun;++stats_.active;stats_.peakActive=std::max(stats_.peakActive,stats_.active);return r.attempt;
    }
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
}
void PublicationProfiler::finish(std::uint64_t attempt,Outcome outcome) {
    if(!enabled() || !attempt) return;
    // Success is only available through a matching rendered generation.
    if(outcome==Outcome::Published) throw std::invalid_argument("Publication requires a complete consumer draw");
    std::lock_guard lock(mutex_);if(auto* r=find(attempt)) close(*r,outcome);
}
void PublicationProfiler::prepared(std::uint64_t attempt,Generation generation) {
    if(!enabled() || !attempt) return;
    std::lock_guard lock(mutex_);
    auto* current=find(attempt);if(!current) return;
    for(auto& r:records_) if(r.attempt && !r.complete && r.attempt!=attempt && r.preparedMs>=0 &&
        r.key.epoch==current->key.epoch && r.key.body==current->key.body) close(r,Outcome::ReplacedBeforeDraw);
    current->generation=generation;current->preparedMs=milliseconds(now_()-current->start);
}
void PublicationProfiler::rendered(std::uint64_t epoch,std::uint64_t body,std::uint64_t serial,Generation generation) {
    if(!enabled()) return;
    std::lock_guard lock(mutex_);
    for(auto& r:records_) if(r.attempt && !r.complete && r.preparedMs>=0 &&
        r.key.epoch==epoch && r.key.body==body && r.key.serial==serial && r.generation==generation) close(r,Outcome::Published);
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
    if(drops) {output_ << "0";for(int i=1;i<37;++i) output_ << ',' << (i==21 ? "trace_overflow" : "");output_ << drops << '\n';}
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
    for(auto value:g.grassEye) {output_ << ',';if(r.preparedMs>=0 && k.resident) output_ << value;}
    output_ << ",0\n";
}
}

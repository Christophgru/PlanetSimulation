#include "rendering/diagnostics/memory/MemorySampler.h"
#include <algorithm>
#include <fstream>
#include <stdexcept>
#if defined(__linux__)
#include <unistd.h>
#endif
namespace rendering {
std::uint64_t memoryClockNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
const char* memoryPhaseName(MemoryPhase phase) {
    static constexpr const char* names[]={"renderer_ready","steady","loading","minimized","capture",
        "reload_requested","reload_preparing","reload_exchange","reload_failed","reload_superseded",
        "retirement","shutdown"};
    return names[static_cast<unsigned>(phase)];
}
namespace {
std::string csvQuoted(const std::string& value) {
    std::string result="\"";
    for(char c:value) {if(c=='\"') result+='\"';result+=c;}
    return result+'\"';
}
std::pair<bool,std::uint64_t> residentBytes() {
#if defined(__linux__)
    unsigned long long pages=0,resident=0;std::ifstream file("/proc/self/statm");
    const long size=sysconf(_SC_PAGESIZE);
    if(size>0 && file>>pages>>resident) return {true,resident*static_cast<std::uint64_t>(size)};
#endif
    return {false,0};
}
}
MemorySampler::MemorySampler(const std::string& path,MemoryContext context,MemoryObservation initial,
    Factory factory,std::chrono::milliseconds period)
    :path_(path),context_(std::move(context)),factory_(std::move(factory)),period_(std::max(period,std::chrono::milliseconds(1))),
     initial_(initial),latest_(std::move(initial)) {
    // Fail synchronously for an unwritable diagnostic destination, before any worker exists.
    std::ofstream probe(path_);if(!probe) throw std::runtime_error("Cannot open memory trace: "+path_);
    worker_=std::thread([this]{run();});
}
MemorySampler::~MemorySampler() {
    if(worker_.joinable()) {
        MemoryObservation last;
        {std::lock_guard lock(mutex_);last=latest_;}
        last.phase=MemoryPhase::Shutdown;last.observedNs=memoryClockNs();stop(std::move(last));
    }
}
bool MemorySampler::observe(MemoryObservation observation,bool event) {
    if(stopping_.load(std::memory_order_acquire)) return false;
    const auto write=eventWrite_.load(std::memory_order_relaxed);
    const bool room=!event || write-eventRead_.load(std::memory_order_acquire)<events_.size();
    if(event && room) events_[write%events_.size()]=observation;
    bool latest=false;
    {
        std::unique_lock lock(mutex_,std::try_to_lock);
        latest=lock.owns_lock();
        if(latest) latest_=std::move(observation);
        else ++skippedLatest_;
    }
    if(event) {
        if(!room) {++dropped_;++droppedEvents_;return false;}
        eventWrite_.store(write+1,std::memory_order_release);changed_.notify_one();return true;
    }
    if(!latest) ++dropped_;
    return latest;
}
void MemorySampler::stop(MemoryObservation final) {
    if(!worker_.joinable()) return;
    {std::lock_guard lock(mutex_);final_=std::move(final);stopping_.store(true,std::memory_order_release);}
    changed_.notify_one();worker_.join();
}
void MemorySampler::run() {
    std::ofstream out(path_);
    out<<"kind,observed_ns,frame,epoch,serial,reload_attempt,replacement_epoch,phase,query_begin_ns,query_end_ns,query_ms,observation_age_ms,sample_age_ms,sample_after_observation,stale,nvml_status,driver_error,total_bytes,used_bytes,free_bytes,context_status,context_uuid,vendor,renderer,nvx_status,nvx_observed_ns,nvx_age_ms,nvx_dedicated_bytes,nvx_total_available_bytes,nvx_available_bytes,logical_status,live_reserved_bytes,replacement_reserved_bytes,overlap_reserved_bytes,cpu_scope,mesh_vector_bytes,scheduler_status,ready_vector_bytes,worker_running,worker_queued,worker_ready,rss_status,rss_observed_ns,process_rss_bytes,dropped_observations,dropped_events,skipped_latest_observations,physical_scope,transient_peak_scope\n";
    PhysicalMemory physical;MemoryReader reader;
    try {reader=factory_(context_);} catch(...) {physical.status="reader_exception";}
    std::uint64_t begin=0,end=0;
    auto write=[&](const char* kind,const MemoryObservation& o) {
        const auto now=memoryClockNs();const auto rss=residentBytes();
        const auto age=[&](std::uint64_t time){return time && now>=time ? (now-time)/1e6 : 0.;};
        out<<kind<<','<<o.observedNs<<','<<o.frame<<','<<o.epoch<<','<<o.serial<<','<<o.reloadAttempt<<','<<o.replacementEpoch<<','<<memoryPhaseName(o.phase)<<',';
        if(begin) out<<begin;out<<',';if(end) out<<end;
        out<<',';if(end) out<<(end-begin)/1e6;
        out<<','<<age(o.observedNs)<<',';if(end) out<<age(end);
        out<<','<<(end>o.observedNs)<<','<<(end && age(end)>2000)<<','<<physical.status<<','<<physical.driverError<<',';
        if(physical.status=="ok") out<<physical.total;
        out<<',';if(physical.status=="ok") out<<physical.used;
        out<<',';if(physical.status=="ok") out<<physical.free;
        out<<','<<context_.status<<','<<csvQuoted(context_.uuid)<<','<<csvQuoted(context_.vendor)<<','<<csvQuoted(context_.renderer)
           <<','<<o.nvx.status<<','<<o.nvx.observedNs<<','<<age(o.nvx.observedNs)<<',';
        if(o.nvx.status=="ok") out<<o.nvx.dedicated;
        out<<',';if(o.nvx.status=="ok") out<<o.nvx.totalAvailable;
        out<<',';if(o.nvx.status=="ok") out<<o.nvx.available;
        out<<','<<(o.managedLedger?"resident_reservation":"unavailable")<<',';
        if(o.managedLedger) out<<o.liveReserved;
        out<<',';if(o.managedLedger) out<<o.replacementReserved;
        out<<',';if(o.managedLedger) out<<o.overlapReserved;
        out<<",partial_owned_vectors,"<<o.meshVectorBytes<<','<<(o.schedulerObserved?"ok":"busy")<<',';
        if(o.schedulerObserved) out<<o.readyVectorBytes;
        out<<',';if(o.schedulerObserved) out<<o.running;
        out<<',';if(o.schedulerObserved) out<<o.queued;
        out<<',';if(o.schedulerObserved) out<<o.ready;
        out<<','<<(rss.first?"ok":"unsupported")<<','<<now<<',';if(rss.first) out<<rss.second;
        out<<','<<dropped_.load()<<','<<droppedEvents_.load()<<','<<skippedLatest_.load()<<",device_wide_other_processes_included,periodic_samples_may_miss_peaks\n";
    };
    auto sample=[&](const MemoryObservation& o) {
        begin=memoryClockNs();
        if(reader) try {physical=reader();} catch(...) {physical={};physical.status="reader_exception";}
        end=memoryClockNs();write("sample",o);
    };
    // The initial sample has renderer-ready metadata; its delay is explicit.
    sample(initial_);
    auto next=std::chrono::steady_clock::now()+period_;
    for(;;) {
        std::array<MemoryObservation,64> events;std::size_t count=0;
        MemoryObservation latest,final;bool stop=false;
        {
            std::unique_lock lock(mutex_);
            changed_.wait_until(lock,next,[&]{return stopping_.load(std::memory_order_acquire)||
                eventRead_.load(std::memory_order_relaxed)!=eventWrite_.load(std::memory_order_acquire);});
            latest=latest_;stop=stopping_.load(std::memory_order_acquire);final=final_;
        }
        // Only the context producer writes the ring; only this worker reads it.
        // Release consumed slots after the entire bounded batch has been moved.
        const auto read=eventRead_.load(std::memory_order_relaxed);
        const auto written=eventWrite_.load(std::memory_order_acquire);count=written-read;
        for(std::size_t i=0;i<count;++i) events[i]=std::move(events_[(read+i)%events_.size()]);
        eventRead_.store(written,std::memory_order_release);
        // Events retain their original metadata but reference a cached physical
        // reading. Never label this delayed read as an instantaneous event peak.
        for(std::size_t i=0;i<count;++i) write("event",events[i]);
        if(stop) {sample(final);out.flush();return;}
        if(std::chrono::steady_clock::now()>=next) {
            sample(latest);next=std::chrono::steady_clock::now()+period_;out.flush();
        }
    }
}
}

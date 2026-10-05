#pragma once
#include "rendering/geometry/jobs/TerrainBuild.h"
#include "rendering/diagnostics/timing/PublicationProfiler.h"
#include <condition_variable>
#include <exception>
#include <functional>
#include <mutex>
#include <thread>

namespace rendering {
struct TerrainBuildCompletion {
    TerrainBuildIdentity identity;
    std::optional<TerrainCpuBuild> build;
    std::exception_ptr error;
    std::uint64_t traceAttempt=0;
    TerrainCpuBuild take();
};
struct TerrainWorkerStats {
    std::uint64_t submitted=0,coalesced=0,obsolete=0,completed=0,failed=0;
    unsigned running=0,queued=0,ready=0,peakRunning=0,peakQueued=0;
    bool stopping=false;
};
// One persistent CPU executor, one latest queued snapshot and one ready result.
// The worker cannot begin another build until its ready result is consumed.
class TerrainBuildScheduler {
public:
    using Builder=std::function<TerrainCpuBuild(const TerrainBuildRequest&)>;
    explicit TerrainBuildScheduler(Builder builder=buildTerrainCpu,PublicationProfiler* trace=nullptr);
    ~TerrainBuildScheduler();
    TerrainBuildScheduler(const TerrainBuildScheduler&)=delete;
    TerrainBuildScheduler& operator=(const TerrainBuildScheduler&)=delete;
    bool submit(TerrainBuildRequest request);
    std::optional<TerrainBuildCompletion> poll();
    // Inspect without releasing the bounded completed slot/backpressure.
    std::optional<TerrainBuildIdentity> readyIdentity() const;
    // Capture-only wait requires an otherwise idle scheduler. Normal frames poll.
    TerrainBuildCompletion executeForCapture(TerrainBuildRequest request);
    // Exclusive capture reload preparation supersedes queued/ready live work,
    // waits for running ownership and uses a future epoch without changing the
    // live epoch. Normal-frame code must not call this explicit wait adapter.
    TerrainBuildCompletion executeForReload(TerrainBuildRequest request);
    // Nonblocking lease for future-scene work. Superseding keeps an executing
    // snapshot alive but drops its result; live epoch changes only on commit.
    void beginReplacement(std::uint64_t epoch);
    void abortReplacement(std::uint64_t epoch);
    void advanceEpoch(std::uint64_t epoch);
    std::optional<TerrainBuildIdentity> pendingFor(std::uint64_t epoch,std::size_t body) const;
    bool pending(std::uint64_t epoch) const;
    TerrainWorkerStats stats() const;
    void stop(); // Join only at shutdown; epoch changes retain executing ownership.
private:
    void run();
    TerrainBuildCompletion executeExclusive(TerrainBuildRequest request,bool replacement);
    Builder builder_;
    PublicationProfiler* trace_;
    std::uint64_t runningAttempt_=0;
    void discard(PublicationProfiler::Outcome outcome,std::uint64_t keepRunningEpoch=0);
    mutable std::mutex mutex_;
    std::condition_variable changed_;
    std::uint64_t epoch_=1,latestSerial_=0;
    std::optional<std::uint64_t> exclusiveEpoch_;
    std::optional<std::uint64_t> replacementEpoch_;
    std::uint64_t lastReplacementEpoch_=0;
    bool stopping_=false;
    std::optional<TerrainBuildRequest> queued_;
    std::optional<TerrainBuildIdentity> running_;
    std::optional<TerrainBuildCompletion> ready_;
    TerrainWorkerStats stats_;
    std::thread worker_;
};
}

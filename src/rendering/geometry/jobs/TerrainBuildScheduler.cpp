#include "rendering/geometry/jobs/TerrainBuildScheduler.h"
#include <stdexcept>
#include <utility>

namespace rendering {
TerrainCpuBuild TerrainBuildCompletion::take() {
    if(error) std::rethrow_exception(error);
    if(!build) throw std::logic_error("Missing terrain worker result");
    return std::move(*build);
}
TerrainBuildScheduler::TerrainBuildScheduler(Builder builder):builder_(std::move(builder)) {
    if(!builder_) throw std::invalid_argument("Missing terrain worker builder");
    worker_=std::thread([this]{run();});
}
TerrainBuildScheduler::~TerrainBuildScheduler() {stop();}
bool TerrainBuildScheduler::submit(TerrainBuildRequest request) {
    request.validate();
    {std::lock_guard lock(mutex_);
        if(stopping_ || exclusiveEpoch_ || request.identity.epoch!=epoch_ || request.identity.serial<=latestSerial_) return false;
        latestSerial_=request.identity.serial;++stats_.submitted;
        if(queued_) ++stats_.coalesced;
        queued_=std::move(request);stats_.peakQueued=1;
    }
    changed_.notify_all();return true;
}
std::optional<TerrainBuildCompletion> TerrainBuildScheduler::poll() {
    std::optional<TerrainBuildCompletion> result;
    {std::lock_guard lock(mutex_);result=std::move(ready_);ready_.reset();}
    changed_.notify_all();return result;
}
std::optional<TerrainBuildIdentity> TerrainBuildScheduler::readyIdentity() const {
    std::lock_guard lock(mutex_);
    return ready_ ? std::optional<TerrainBuildIdentity>(ready_->identity) : std::nullopt;
}
TerrainBuildCompletion TerrainBuildScheduler::executeForCapture(TerrainBuildRequest request) {
    return executeExclusive(std::move(request),false);
}
TerrainBuildCompletion TerrainBuildScheduler::executeForReload(TerrainBuildRequest request) {
    return executeExclusive(std::move(request),true);
}
TerrainBuildCompletion TerrainBuildScheduler::executeExclusive(TerrainBuildRequest request,bool replacement) {
    request.validate();const auto key=request.identity;
    std::unique_lock lock(mutex_);
    const auto liveEpoch=epoch_;
    if(stopping_ || (!replacement && (running_ || queued_ || ready_)) || exclusiveEpoch_ ||
       (replacement ? key.epoch<=epoch_ : key.epoch!=epoch_) || key.serial<=latestSerial_)
        throw std::logic_error("Capture terrain build requires an idle matching scheduler");
    if(replacement) {
        // Keep the live epoch usable on failure, but no earlier queued/ready
        // completion may obstruct this explicit replacement adapter. Running
        // snapshots retain ownership until they finish and are then discarded.
        stats_.obsolete+=unsigned(queued_.has_value())+unsigned(ready_.has_value());
        queued_.reset();ready_.reset();
    }
    exclusiveEpoch_=key.epoch;
    latestSerial_=key.serial;++stats_.submitted;queued_=std::move(request);stats_.peakQueued=1;
    changed_.notify_all();
    changed_.wait(lock,[&]{return stopping_ || epoch_!=liveEpoch || ready_.has_value();});
    exclusiveEpoch_.reset();
    if(stopping_ || epoch_!=liveEpoch || !ready_ || ready_->identity!=key)
        throw std::runtime_error("Capture terrain request became obsolete");
    auto result=std::move(*ready_);ready_.reset();lock.unlock();changed_.notify_all();return result;
}
void TerrainBuildScheduler::advanceEpoch(std::uint64_t epoch) {
    {std::lock_guard lock(mutex_);
        if(epoch<=epoch_) throw std::invalid_argument("Terrain scene epoch must increase");
        epoch_=epoch;latestSerial_=0;
        stats_.obsolete+=unsigned(queued_.has_value())+unsigned(ready_.has_value());
        queued_.reset();ready_.reset();
    }
    changed_.notify_all();
}
std::optional<TerrainBuildIdentity> TerrainBuildScheduler::pendingFor(std::uint64_t epoch,std::size_t body) const {
    std::lock_guard lock(mutex_);
    const auto matches=[&](const auto& k){return k.epoch==epoch && k.bodyIndex==body;};
    if(queued_ && matches(queued_->identity)) return queued_->identity;
    if(running_ && matches(*running_)) return *running_;
    if(ready_ && matches(ready_->identity)) return ready_->identity;
    return {};
}
bool TerrainBuildScheduler::pending(std::uint64_t epoch) const {
    std::lock_guard lock(mutex_);
    return (queued_ && queued_->identity.epoch==epoch) || (running_ && running_->epoch==epoch) ||
        (ready_ && ready_->identity.epoch==epoch);
}
TerrainWorkerStats TerrainBuildScheduler::stats() const {
    std::lock_guard lock(mutex_);auto s=stats_;
    s.running=running_.has_value();s.queued=queued_.has_value();s.ready=ready_.has_value();s.stopping=stopping_;return s;
}
void TerrainBuildScheduler::stop() {
    {std::lock_guard lock(mutex_);stopping_=true;queued_.reset();ready_.reset();}
    changed_.notify_all();if(worker_.joinable()) worker_.join();
}
void TerrainBuildScheduler::run() {
    std::unique_lock lock(mutex_);
    for(;;) {
        changed_.wait(lock,[&]{return stopping_ || (queued_ && !ready_);});
        if(stopping_) return;
        auto request=std::move(*queued_);queued_.reset();running_=request.identity;stats_.peakRunning=1;
        lock.unlock();TerrainBuildCompletion completion{request.identity,{},{}};
        try {completion.build=builder_(request);} catch(...) {completion.error=std::current_exception();}
        lock.lock();running_.reset();
        if(stopping_ || request.identity.epoch!=exclusiveEpoch_.value_or(epoch_)) ++stats_.obsolete;
        else {++stats_.completed;if(completion.error) ++stats_.failed;ready_=std::move(completion);}
        changed_.notify_all();
    }
}
}

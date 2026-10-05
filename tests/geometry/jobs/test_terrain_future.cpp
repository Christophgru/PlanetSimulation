#include "rendering/geometry/jobs/TerrainBuildScheduler.h"
#include <gtest/gtest.h>
#include <chrono>
#include <future>
using namespace rendering;
using namespace std::chrono_literals;
namespace {
TerrainBuildRequest request(std::uint64_t serial,std::uint64_t epoch) {
    config::PlanetConfig planet;planet.name="Earth";planet.radius=1;
    TerrainSurface surface({},planet.terrain_lod,1,1000,{},0);
    TerrainBuildIdentity k;k.epoch=epoch;k.serial=serial;k.bodyName=planet.name;
    k.field=surface.field().fingerprint();k.eye={1.01,0,0};k.localMask=1;
    return {k,std::move(surface),std::move(planet),{},1000};
}
struct Gate {
    std::promise<void> open;std::shared_future<void> released=open.get_future();
    bool done=false;
    void release(){if(!done){done=true;open.set_value();}}
    ~Gate(){release();}
};
struct Release {Gate& gate;~Release(){gate.release();}};
bool ready(TerrainBuildScheduler& jobs) {
    const auto deadline=std::chrono::steady_clock::now()+5s;
    while(!jobs.readyIdentity() && std::chrono::steady_clock::now()<deadline) std::this_thread::sleep_for(1ms);
    return jobs.readyIdentity().has_value();
}
}
TEST(TerrainFuture, SupersedesExecutingLiveAndQueuedFutureWorkWithoutWaitingOrChangingLiveEpoch) {
    Gate gate;std::promise<void> started;std::vector<std::uint64_t> calls;
    TerrainBuildScheduler jobs([&](const auto& r) {
        calls.push_back(r.identity.serial);
        if(r.identity.serial==1) {started.set_value();gate.released.wait();}
        return TerrainCpuBuild{};
    });
    Release release{gate};ASSERT_TRUE(jobs.submit(request(1,1)));
    ASSERT_EQ(started.get_future().wait_for(5s),std::future_status::ready);
    ASSERT_TRUE(jobs.submit(request(2,1)));jobs.beginReplacement(2);
    EXPECT_EQ(jobs.stats().running,1u);EXPECT_FALSE(jobs.submit(request(3,1)));
    ASSERT_TRUE(jobs.submit(request(3,2)));jobs.beginReplacement(3);
    EXPECT_EQ(jobs.stats().running,1u);ASSERT_TRUE(jobs.submit(request(4,3)));
    gate.release();ASSERT_TRUE(ready(jobs));EXPECT_EQ(jobs.poll()->identity.epoch,3u);
    EXPECT_EQ(calls,(std::vector<std::uint64_t>{1,4}));EXPECT_EQ(jobs.stats().obsolete,3u);
    jobs.advanceEpoch(3);EXPECT_FALSE(jobs.submit(request(5,1)));ASSERT_TRUE(jobs.submit(request(5,3)));
    ASSERT_TRUE(ready(jobs));EXPECT_NO_THROW(jobs.poll()->take());
    EXPECT_EQ(jobs.stats().peakRunning,1u);EXPECT_EQ(jobs.stats().peakQueued,1u);
}
TEST(TerrainFuture, OldAbortCannotCancelNewLeaseAndLateFutureCompletionIsDiscardedOnAbort) {
    Gate gate;std::promise<void> started;
    TerrainBuildScheduler jobs([&](const auto& r) {
        if(r.identity.serial==1) {started.set_value();gate.released.wait();}
        if(r.identity.serial==4) throw std::runtime_error("CPU replacement failure");
        return TerrainCpuBuild{};
    });
    Release release{gate};jobs.beginReplacement(2);ASSERT_TRUE(jobs.submit(request(1,2)));
    ASSERT_EQ(started.get_future().wait_for(5s),std::future_status::ready);
    jobs.beginReplacement(3);ASSERT_TRUE(jobs.submit(request(2,3)));jobs.abortReplacement(2);
    EXPECT_EQ(jobs.stats().queued,1u);jobs.abortReplacement(3);
    EXPECT_EQ(jobs.stats().running,1u);ASSERT_TRUE(jobs.submit(request(3,1)));
    gate.release();ASSERT_TRUE(ready(jobs));EXPECT_EQ(jobs.poll()->identity.epoch,1u);
    EXPECT_GE(jobs.stats().obsolete,2u);EXPECT_THROW(jobs.beginReplacement(3),std::invalid_argument);
    jobs.beginReplacement(4);ASSERT_TRUE(jobs.submit(request(4,4)));ASSERT_TRUE(ready(jobs));
    EXPECT_THROW(jobs.poll()->take(),std::runtime_error);jobs.abortReplacement(4);
    EXPECT_NO_THROW(jobs.executeForCapture(request(5,1)).take());EXPECT_EQ(jobs.stats().failed,1u);
}
TEST(TerrainFuture, ReadyBackpressureSurvivesWrongCommitAndSupersedingDropsReadyAndQueuedTogether) {
    std::vector<std::uint64_t> calls;
    TerrainBuildScheduler jobs([&](const auto& r){calls.push_back(r.identity.serial);return TerrainCpuBuild{};});
    jobs.beginReplacement(2);ASSERT_TRUE(jobs.submit(request(1,2)));ASSERT_TRUE(ready(jobs));
    ASSERT_TRUE(jobs.submit(request(2,2)));EXPECT_THROW(jobs.advanceEpoch(3),std::invalid_argument);
    jobs.abortReplacement(3);EXPECT_EQ(jobs.readyIdentity()->serial,1u);
    EXPECT_EQ(jobs.stats().ready,1u);EXPECT_EQ(jobs.stats().queued,1u);
    jobs.beginReplacement(3);EXPECT_EQ(jobs.stats().ready,0u);EXPECT_EQ(jobs.stats().queued,0u);
    ASSERT_TRUE(jobs.submit(request(3,3)));ASSERT_TRUE(ready(jobs));EXPECT_NO_THROW(jobs.poll()->take());
    EXPECT_EQ(calls,(std::vector<std::uint64_t>{1,3}));jobs.advanceEpoch(3);
    EXPECT_NO_THROW(jobs.executeForCapture(request(4,3)).take());
}

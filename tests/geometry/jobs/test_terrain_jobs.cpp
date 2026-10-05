#include "rendering/geometry/jobs/TerrainBuildScheduler.h"
#include "rendering/geometry/contacts/SparseTerrainContacts.h"
#include <gtest/gtest.h>
#include <chrono>
#include <future>
#include <limits>
#include <vector>

using namespace rendering;
using namespace std::chrono_literals;
namespace {
TerrainBuildRequest request(std::uint64_t serial=1,std::uint64_t epoch=1) {
    config::PlanetConfig p;p.name="Earth";p.radius=1;p.terrain_lod.max_triangle_budget=10000;
    p.terrain_lod.max_edge_segments=4;p.terrain_lod.medium_edge_segments=2;p.terrain_lod.steep_edge_segments=4;
    p.terrain_lod.shoreline_edge_m=0;
    TerrainSurface surface({},p.terrain_lod,p.radius,1000,{},0.0);
    TerrainBuildIdentity k;k.epoch=epoch;k.serial=serial;k.bodyName=p.name;k.field=surface.field().fingerprint();
    k.eye={1.01,0,0};k.localMask=1;
    return {k,std::move(surface),std::move(p),{},1000};
}
struct Gate {
    std::mutex mutex;std::condition_variable changed;bool open=false;
    void wait() {std::unique_lock lock(mutex);changed.wait(lock,[&]{return open;});}
    void release() {{std::lock_guard lock(mutex);open=true;}changed.notify_all();}
};
// Assertions cannot strand a blocked builder during stack unwinding.
struct Release {Gate& gate;~Release(){gate.release();}};
std::optional<TerrainBuildCompletion> collect(TerrainBuildScheduler& scheduler) {
    const auto deadline=std::chrono::steady_clock::now()+5s;
    while(std::chrono::steady_clock::now()<deadline) {
        if(auto result=scheduler.poll()) return result;
        std::this_thread::sleep_for(1ms);
    }
    return {};
}
}
TEST(TerrainJobs, BoundsCoalescingAndCompletionBackpressure) {
    Gate gate;std::promise<void> started,secondStarted;auto second=secondStarted.get_future();
    std::vector<std::uint64_t> calls;
    TerrainBuildScheduler jobs([&](const auto& r) {
        calls.push_back(r.identity.serial);
        if(r.identity.serial==1) {started.set_value();gate.wait();} else secondStarted.set_value();
        TerrainCpuBuild out;out.milliseconds=r.identity.serial;return out;
    });
    Release release{gate};EXPECT_TRUE(jobs.submit(request()));
    ASSERT_EQ(started.get_future().wait_for(5s),std::future_status::ready);
    EXPECT_TRUE(jobs.submit(request(2)));EXPECT_TRUE(jobs.submit(request(3)));EXPECT_TRUE(jobs.submit(request(4)));
    EXPECT_FALSE(jobs.submit(request(3)));EXPECT_FALSE(jobs.poll());
    auto s=jobs.stats();EXPECT_EQ(s.running,1);EXPECT_EQ(s.queued,1);EXPECT_EQ(s.coalesced,2);
    EXPECT_EQ(jobs.pendingFor(1,0)->serial,4);
    EXPECT_THROW(jobs.executeForCapture(request(5)),std::logic_error);
    gate.release();
    const auto deadline=std::chrono::steady_clock::now()+5s;
    while(!jobs.stats().ready && std::chrono::steady_clock::now()<deadline) std::this_thread::sleep_for(1ms);
    for(int i=0;i<10;++i) {
        ASSERT_TRUE(jobs.readyIdentity());EXPECT_EQ(jobs.readyIdentity()->serial,1);
        EXPECT_EQ(jobs.stats().ready,1);EXPECT_EQ(jobs.stats().queued,1);
    }
    EXPECT_EQ(second.wait_for(0s),std::future_status::timeout); // Ready output blocks another build.
    auto first=collect(jobs);ASSERT_TRUE(first);EXPECT_EQ(first->identity.serial,1);
    auto last=collect(jobs);ASSERT_TRUE(last);EXPECT_EQ(last->identity.serial,4);
    EXPECT_EQ(last->take().milliseconds,4);EXPECT_EQ(calls,(std::vector<std::uint64_t>{1,4}));
    s=jobs.stats();EXPECT_EQ(s.peakRunning,1);EXPECT_EQ(s.peakQueued,1);EXPECT_EQ(s.completed,2);
    EXPECT_FALSE(jobs.pending(1));
}
TEST(TerrainJobs, RepeatedReloadKeepsWorkerOwnershipAndDiscardsOldEpochs) {
    Gate gate;std::promise<void> started;std::vector<std::uint64_t> calls;
    TerrainBuildScheduler jobs([&](const auto& r) {
        calls.push_back(r.identity.serial);
        if(r.identity.epoch==1) {started.set_value();gate.wait();}
        return TerrainCpuBuild{};
    });
    Release release{gate};jobs.submit(request());ASSERT_EQ(started.get_future().wait_for(5s),std::future_status::ready);
    jobs.submit(request(2));jobs.advanceEpoch(2);jobs.submit(request(3,2));jobs.advanceEpoch(3);
    auto moon=request(4,3);moon.planet.name=moon.identity.bodyName="Moon";
    EXPECT_TRUE(jobs.submit(moon));EXPECT_FALSE(jobs.submit(request(5,1)));
    EXPECT_FALSE(jobs.pendingFor(3,1));EXPECT_EQ(jobs.pendingFor(3,0)->bodyName,"Moon");
    EXPECT_FALSE(jobs.poll());gate.release();
    auto current=collect(jobs);ASSERT_TRUE(current);EXPECT_EQ(current->identity,moon.identity);
    EXPECT_EQ(calls,(std::vector<std::uint64_t>{1,4}));EXPECT_EQ(jobs.stats().obsolete,3);
    EXPECT_THROW(jobs.advanceEpoch(3),std::invalid_argument);
}
TEST(TerrainJobs, ErrorsAreReturnedAndExecutorCanRecover) {
    const auto main=std::this_thread::get_id();std::thread::id execution;
    TerrainBuildScheduler jobs([&](const auto& r) {
        execution=std::this_thread::get_id();
        if(r.identity.serial==1) throw std::runtime_error("injected topology failure");
        return TerrainCpuBuild{};
    });
    jobs.submit(request());auto failed=collect(jobs);ASSERT_TRUE(failed);
    EXPECT_THROW(failed->take(),std::runtime_error);EXPECT_EQ(jobs.stats().failed,1);
    auto recovered=jobs.executeForCapture(request(2));EXPECT_NO_THROW(recovered.take());
    EXPECT_NE(execution,main);EXPECT_EQ(jobs.stats().completed,2);EXPECT_FALSE(jobs.pending(1));
}
TEST(TerrainJobs, ExclusiveReloadFailureLeavesTheLiveEpochUsableAndReusesTheSameWorker) {
    std::thread::id worker;
    TerrainBuildScheduler jobs([&](const auto& r) {
        const auto thread=std::this_thread::get_id();
        if(worker!=std::thread::id{}) EXPECT_EQ(worker,thread);worker=thread;
        if(r.identity.serial==1) throw std::runtime_error("replacement CPU failure");
        return TerrainCpuBuild{};
    });
    auto failed=jobs.executeForReload(request(1,2));EXPECT_THROW(failed.take(),std::runtime_error);
    EXPECT_NO_THROW(jobs.executeForCapture(request(2,1)).take());
    EXPECT_NO_THROW(jobs.executeForReload(request(3,2)).take());
    EXPECT_NO_THROW(jobs.executeForCapture(request(4,1)).take());
    jobs.advanceEpoch(2);EXPECT_NO_THROW(jobs.executeForCapture(request(5,2)).take());
    EXPECT_EQ(jobs.stats().peakRunning,1u);EXPECT_EQ(jobs.stats().peakQueued,1u);
    EXPECT_FALSE(jobs.pending(1));EXPECT_FALSE(jobs.pending(2));
    EXPECT_EQ(jobs.stats().failed,1u);EXPECT_EQ(jobs.stats().obsolete,0u);
}
TEST(TerrainJobs, ShutdownJoinsExecutingSnapshotAndDropsQueuedWork) {
    Gate gate;std::promise<void> started;std::vector<std::uint64_t> calls;
    TerrainBuildScheduler jobs([&](const auto& r) {
        calls.push_back(r.identity.serial);started.set_value();gate.wait();return TerrainCpuBuild{};
    });
    Release release{gate};jobs.submit(request());ASSERT_EQ(started.get_future().wait_for(5s),std::future_status::ready);
    jobs.submit(request(2));auto stopping=std::async(std::launch::async,[&]{jobs.stop();});
    const auto deadline=std::chrono::steady_clock::now()+5s;
    while(!jobs.stats().stopping && std::chrono::steady_clock::now()<deadline) std::this_thread::sleep_for(1ms);
    EXPECT_TRUE(jobs.stats().stopping);
    EXPECT_EQ(stopping.wait_for(0s),std::future_status::timeout);gate.release();
    ASSERT_EQ(stopping.wait_for(5s),std::future_status::ready);stopping.get();
    EXPECT_EQ(calls,(std::vector<std::uint64_t>{1}));EXPECT_FALSE(jobs.submit(request(3)));
    EXPECT_FALSE(jobs.pending(1));EXPECT_EQ(jobs.stats().running,0);
}
TEST(TerrainJobs, FutureReloadSupersedesRunningAndQueuedWorkWithoutChangingLiveEpochOnFailure) {
    Gate gate;std::promise<void> started;std::vector<std::uint64_t> calls;
    TerrainBuildScheduler jobs([&](const auto& r) {
        calls.push_back(r.identity.serial);
        if(r.identity.serial==1) {started.set_value();gate.wait();}
        if(r.identity.epoch==2) throw std::runtime_error("replacement failure");
        return TerrainCpuBuild{};
    });
    Release release{gate};ASSERT_TRUE(jobs.submit(request()));
    ASSERT_EQ(started.get_future().wait_for(5s),std::future_status::ready);
    ASSERT_TRUE(jobs.submit(request(2)));
    auto replacement=std::async(std::launch::async,[&]{return jobs.executeForReload(request(3,2));});
    const auto deadline=std::chrono::steady_clock::now()+5s;
    while(jobs.pendingFor(2,0)==std::nullopt && std::chrono::steady_clock::now()<deadline)
        std::this_thread::sleep_for(1ms);
    EXPECT_TRUE(jobs.pendingFor(2,0));EXPECT_FALSE(jobs.submit(request(4)));
    EXPECT_EQ(replacement.wait_for(0s),std::future_status::timeout);
    gate.release();ASSERT_EQ(replacement.wait_for(5s),std::future_status::ready);
    auto failed=replacement.get();EXPECT_THROW(failed.take(),std::runtime_error);
    EXPECT_FALSE(jobs.poll());EXPECT_EQ(jobs.stats().obsolete,2u);
    EXPECT_NO_THROW(jobs.executeForCapture(request(4)).take());
    EXPECT_EQ(calls,(std::vector<std::uint64_t>{1,3,4}));
    EXPECT_EQ(jobs.stats().peakRunning,1u);EXPECT_EQ(jobs.stats().peakQueued,1u);
}
TEST(TerrainJobs, FutureReloadDropsReadyAndQueuedResultsBeforeExclusivePreparation) {
    TerrainBuildScheduler jobs([](const auto&){return TerrainCpuBuild{};});
    ASSERT_TRUE(jobs.submit(request()));
    const auto deadline=std::chrono::steady_clock::now()+5s;
    while(!jobs.stats().ready && std::chrono::steady_clock::now()<deadline) std::this_thread::sleep_for(1ms);
    ASSERT_EQ(jobs.stats().ready,1u);ASSERT_TRUE(jobs.submit(request(2)));
    auto next=jobs.executeForReload(request(3,2));EXPECT_EQ(next.identity.epoch,2u);
    EXPECT_EQ(jobs.stats().obsolete,2u);EXPECT_FALSE(jobs.poll());
    jobs.advanceEpoch(2);EXPECT_NO_THROW(jobs.executeForCapture(request(4,2)).take());
}
TEST(TerrainJobs, PublicationRejectsBodyReorderingFieldModeEpochAndOlderSerials) {
    auto current=request(8).identity;auto completed=current;completed.serial=4;
    completed.eye={0,1.01,0};EXPECT_TRUE(terrainBuildMatches(completed,current,3));
    EXPECT_FALSE(terrainBuildMatches(completed,current,4));completed.serial=9;
    EXPECT_FALSE(terrainBuildMatches(completed,current,3));completed.serial=4;
    const auto reject=[&](auto change) {auto old=completed;change(old);EXPECT_FALSE(terrainBuildMatches(old,current,3));};
    reject([](auto& k){++k.epoch;});reject([](auto& k){++k.bodyIndex;});
    reject([](auto& k){k.bodyName="Moon";});reject([](auto& k){++k.field;});
    reject([](auto& k){++k.fieldVersion;});reject([](auto& k){++k.topologyVersion;});
    reject([](auto& k){k.backend=TerrainBackend::Compute;});reject([](auto& k){k.resident=true;});
    reject([](auto& k){k.localMask=0;});
}
TEST(TerrainJobs, ResidentBuildOwnsSnapshotsAndConstructsContactsWithoutRenderEvaluation) {
    auto r=request();r.identity.backend=TerrainBackend::Compute;r.identity.resident=true;
    r.planet.water.enabled=true;r.planet.water.level_m=2;
    const auto expected=r.surface.buildTopologyForEye(r.identity.eye,{0,0,0},nullptr,20);
    const auto identity=r.identity;TerrainBuildScheduler jobs;
    jobs.submit(r);r.planet.water.enabled=false;r.identity.eye={0,1.1,0};r.previousFaceZones.assign(320,0);
    auto completed=collect(jobs);ASSERT_TRUE(completed);EXPECT_EQ(completed->identity,identity);
    auto built=completed->take();ASSERT_TRUE(built.topology);ASSERT_TRUE(built.contacts);ASSERT_TRUE(built.waterTopology);
    EXPECT_EQ(built.topology->samples,expected.samples);EXPECT_EQ(built.topology->indices,expected.indices);
    EXPECT_EQ(built.geometry.evaluationQueries.requests,0);EXPECT_TRUE(built.geometry.vertices.empty());EXPECT_TRUE(built.geometry.indices.empty());
    ASSERT_TRUE(built.water);EXPECT_TRUE(built.water->vertices.empty());EXPECT_TRUE(built.water->indices.empty());
    auto key=expected.generation;key.backend=TerrainBackend::Compute;EXPECT_EQ(built.contacts->generation(),key);
    EXPECT_EQ(built.contacts->stats().heightEvaluations,0);EXPECT_EQ(built.contacts->stats().residentPositions,0);
    const auto candidates=built.contacts->candidates(glm::normalize(identity.eye));ASSERT_FALSE(candidates.empty());
    EXPECT_NO_THROW(built.contacts->triangle(candidates.front()));EXPECT_LE(built.contacts->stats().residentPositions,3);
}
TEST(TerrainJobs, CpuAndLegacyComputeKeepExactExpandedGeometry) {
    auto r=request();const auto expected=r.surface.buildGeometryForEye(r.identity.eye,{0,0,0},nullptr,20);
    auto cpu=buildTerrainCpu(r);EXPECT_EQ(cpu.geometry.vertices,expected.vertices);EXPECT_EQ(cpu.geometry.indices,expected.indices);
    EXPECT_FALSE(cpu.contacts);EXPECT_FALSE(cpu.topology);
    r.identity.backend=TerrainBackend::Compute;TerrainBuildScheduler jobs;
    auto compatibility=jobs.executeForCapture(r).take();EXPECT_EQ(compatibility.geometry.vertices,expected.vertices);
    EXPECT_EQ(compatibility.geometry.indices,expected.indices);EXPECT_GT(compatibility.geometry.evaluationQueries.requests,0);
    EXPECT_TRUE(compatibility.contacts);
}
TEST(TerrainJobs, InvalidSnapshotsDoNotReplacePendingWork) {
    TerrainBuildScheduler jobs;auto malformed=request();++malformed.identity.field;
    EXPECT_THROW(jobs.submit(malformed),std::invalid_argument);EXPECT_EQ(jobs.stats().submitted,0);
    malformed=request();malformed.identity.resident=true;EXPECT_THROW(jobs.submit(malformed),std::invalid_argument);
    malformed=request();malformed.identity.eye.x=std::numeric_limits<double>::infinity();
    EXPECT_THROW(jobs.submit(malformed),std::invalid_argument);
    EXPECT_FALSE(jobs.submit(request(1,2)));EXPECT_TRUE(jobs.submit(request()));
    auto result=collect(jobs);ASSERT_TRUE(result);EXPECT_NO_THROW(result->take());
}

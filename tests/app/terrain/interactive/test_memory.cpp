#include "FrameTestSupport.h"
#include "rendering/runtime/terrain/reload/interactive/PendingReload.h"
#include "rendering/diagnostics/memory/ContextMemory.h"
#include "../../../diagnostics/memory/Csv.h"
#include <set>
namespace {
void verifyMemory(const std::string& path,bool managed,const std::set<std::string>& expected) {
    const auto rows=memory_test::rows(path);ASSERT_GE(rows.size(),2u);std::set<std::string> phases;
    EXPECT_EQ(rows.front().at("phase"),"renderer_ready");EXPECT_EQ(rows.back().at("phase"),"shutdown");
    for(const auto& row:rows) {
        phases.insert(row.at("phase"));EXPECT_EQ(row.at("dropped_events"),"0");
        EXPECT_LE(std::stoull(row.at("dropped_observations")),std::stoull(row.at("skipped_latest_observations")));
        EXPECT_EQ(row.at("logical_status"),managed?"resident_reservation":"unavailable");
        if(managed) EXPECT_EQ(std::stoull(row.at("overlap_reserved_bytes")),std::max(std::stoull(row.at("live_reserved_bytes")),std::stoull(row.at("replacement_reserved_bytes"))));
        else EXPECT_TRUE(row.at("overlap_reserved_bytes").empty());
        if(row.at("nvml_status")=="ok") {
            EXPECT_EQ(row.at("context_status"),"uuid_verified");EXPECT_TRUE(rendering::validDeviceUuid(row.at("context_uuid")));
            EXPECT_LE(std::stoull(row.at("used_bytes"))+std::stoull(row.at("free_bytes")),std::stoull(row.at("total_bytes")));
        } else EXPECT_TRUE(row.at("used_bytes").empty());
        if(const auto* uuid=std::getenv("TEST_EXPECT_GPU_UUID")) {
            EXPECT_EQ(row.at("context_uuid"),uuid);EXPECT_EQ(row.at("nvml_status"),"ok");EXPECT_EQ(row.at("nvx_status"),"ok");
        }
        if(row.at("kind")=="event" && row.at("phase").starts_with("reload_")) {
            EXPECT_GT(std::stoull(row.at("reload_attempt")),0u);EXPECT_GT(std::stoull(row.at("replacement_epoch")),0u);
        }
        EXPECT_EQ(row.at("physical_scope"),"device_wide_other_processes_included");
        EXPECT_EQ(row.at("transient_peak_scope"),"periodic_samples_may_miss_peaks");
    }
    for(const auto& phase:expected) EXPECT_TRUE(phases.contains(phase))<<phase;
}
}
TEST(MemoryTracing, DisabledDoesNotCreateReaderOrOutput) {
    auto o=options("memory-disabled");rendering::Renderer renderer(o);auto& r=Probe::state(renderer);
    EXPECT_FALSE(r.memorySampler);EXPECT_FALSE(std::filesystem::exists(o.performanceTrace+".memory.csv"));
}
TEST(MemoryTracing, CpuCaptureReloadHasExplicitUnsupportedLedger) {
    auto o=options("memory-cpu");o.terrainBackend=o.terrainGrassPlanner="cpu";o.performanceTrace=o.configPath+".frames.csv";
    {
        rendering::Renderer renderer(o);EXPECT_EQ(renderer.run(),0);renderer.reload();EXPECT_EQ(renderer.run(),0);
        std::ofstream(o.configPath)<<"{}";EXPECT_THROW(renderer.reload(),std::exception);
    }
    verifyMemory(o.performanceTrace+".memory.csv",false,{"renderer_ready","reload_requested","reload_preparing","reload_exchange","reload_failed","shutdown"});
}
TEST(MemoryTracing, ResidentReloadRetainsOverlapSupersessionFailureAndRetirement) {
    auto o=options("memory-resident");o.renderTestMode=false;o.performanceTrace=o.configPath+".frames.csv";
    {
        rendering::Renderer renderer(o);auto& r=Probe::state(renderer);GlAudit audit;
        const auto frame=[&] {
            r.profiler.beginFrame(0);const bool exchanged=r.pollResidentReload();
            if(!exchanged) tick(r); // Inspect overlap before the next preparation can retire it.
            r.observeMemory(r.sceneReloadPreparing()?rendering::MemoryPhase::ReloadPreparing:rendering::MemoryPhase::Steady);
            r.profiler.endFrame();glFlush();
        };
        ASSERT_TRUE(until(frame,[&]{return idle(r);}));draw(r);
        const auto old=r.terrainSceneEpoch;r.requestResidentReload();r.requestResidentReload();
        ASSERT_TRUE(until(frame,[&]{return r.terrainSceneEpoch>old;}));
        ASSERT_TRUE(r.retiredResidentScene);const auto overlap=r.memorySnapshot(rendering::MemoryPhase::ReloadExchange);
        EXPECT_GT(overlap.liveReserved,0u);EXPECT_EQ(overlap.overlapReserved,overlap.liveReserved);
        ASSERT_TRUE(until([&]{r.retireSceneReload(false);glFlush();},[&]{return !r.retiredResidentScene;}));
        EXPECT_EQ(r.terrainPublication->externalBytes(),0u);
        auto invalid=r.source.document;invalid["planets"][0]["radius"]=-1;
        std::ofstream(o.configPath)<<invalid.dump();EXPECT_THROW(r.requestResidentReload(),std::invalid_argument);
        std::ofstream(o.configPath+".memory-audit.json")<<audit.state().dump(2);
    }
    verifyMemory(o.performanceTrace+".memory.csv",true,{"renderer_ready","reload_requested","reload_superseded","reload_preparing","reload_exchange","retirement","reload_failed","shutdown"});
}

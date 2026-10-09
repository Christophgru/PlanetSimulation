#include "app/CommandLineOptions.h"
#include <gtest/gtest.h>
#include <stdexcept>
#include <vector>

namespace {
app::CommandLineOptions parse(std::initializer_list<const char*> arguments) {
    std::vector<std::string> strings{"PlanetSimulation"};
    for (auto argument : arguments) strings.emplace_back(argument);
    std::vector<char*> argv;
    for (auto& value : strings) argv.push_back(value.data());
    return app::CommandLineOptions::parse(static_cast<int>(argv.size()), argv.data());
}
}

TEST(CommandLineOptions, CaptureAndExplicitOverridesSurviveExtraction) {
    const auto options = parse({"--surface-capture", "out.png", "--replay", "view.json",
        "--config", "scene.json", "--render-size", "960", "540", "--simulation-time", "-12.5",
        "--atmosphere-full-resolution", "--benchmark-frames", "2", "--benchmark-step", "0",
        "--benchmark-overlay", "--performance-trace", "trace.csv", "--cpu-trace", "cpu.json", "--video-memory-mb", "128"});
    EXPECT_TRUE(options.renderTestMode && options.surfaceRenderMode && options.captureOnly);
    EXPECT_FALSE(options.planetRenderMode);
    EXPECT_TRUE(options.explicitRenderSize && options.explicitAtmosphereQuality);
    EXPECT_TRUE(options.atmosphereFullResolution && options.benchmarkOverlay);
    EXPECT_EQ(options.outputImagePath, "out.png");
    EXPECT_EQ(options.replayPath, "view.json");
    EXPECT_EQ(options.configPath, "scene.json");
    EXPECT_EQ(options.performanceTrace, "trace.csv");
    EXPECT_EQ(options.cpuTrace, "cpu.json");
    EXPECT_EQ(options.renderTestWidth, 960);
    EXPECT_EQ(options.renderTestHeight, 540);
    EXPECT_EQ(options.commandLineTime, -12.5);
    EXPECT_EQ(options.benchmarkFrames, 2);
    EXPECT_EQ(options.benchmarkStep, 0);
    EXPECT_EQ(options.videoMemoryCapBytes, 128ULL * 1024 * 1024);
}

TEST(CommandLineOptions, InvalidValuesFailBeforeWindowStartup) {
    EXPECT_THROW(parse({"--cpu-trace"}), std::invalid_argument);
    EXPECT_THROW(parse({"--cpu-trace", "--benchmark-overlay"}), std::invalid_argument);
    EXPECT_THROW(parse({"--simulation-time", "nan"}), std::invalid_argument);
    EXPECT_THROW(parse({"--simulation-time", "1second"}), std::invalid_argument);
    EXPECT_THROW(parse({"--render-size", "63", "600"}), std::invalid_argument);
    EXPECT_THROW(parse({"--render-size", "800", "8193"}), std::invalid_argument);
    EXPECT_THROW(parse({"--video-memory-mb", "31"}), std::invalid_argument);
    EXPECT_THROW(parse({"--benchmark-step", "inf"}), std::invalid_argument);
    EXPECT_THROW(parse({"--benchmark-frames", "2"}), std::invalid_argument);
    EXPECT_THROW(parse({"--benchmark-walk-step", "nan"}), std::invalid_argument);
    EXPECT_THROW(parse({"--benchmark-walk-step", "2m"}), std::invalid_argument);
    EXPECT_THROW(parse({"--benchmark-walk-step", "-1"}), std::invalid_argument);
    EXPECT_THROW(parse({"--benchmark-walk-step", "101"}), std::invalid_argument);
    EXPECT_THROW(parse({"--benchmark-walk-step"}), std::invalid_argument);
    EXPECT_THROW(parse({"--benchmark-walk-step", "2"}), std::invalid_argument);
    const auto walk = parse({"--surface-capture", "walk.png", "--benchmark-frames", "6",
                             "--benchmark-walk-step", "2", "--benchmark-step", "0"});
    EXPECT_EQ(walk.benchmarkWalkStep, 2);
    EXPECT_EQ(walk.benchmarkStep, 0);
}
TEST(CommandLineOptions, UncappedPresentationDoesNotChangeSimulationSettings) {
    const auto defaults = parse({});
    const auto options = parse({"--uncapped"});
    EXPECT_FALSE(defaults.uncapped);
    EXPECT_TRUE(options.uncapped);
    EXPECT_FALSE(options.renderTestMode);
    EXPECT_EQ(options.benchmarkStep, defaults.benchmarkStep);
    EXPECT_EQ(options.terrainBackend, defaults.terrainBackend);
}
TEST(CommandLineOptions, AstronautCaptureUsesSurfaceReplayAndWalkingBenchmarks) {
    const auto options=parse({"--astronaut-capture","astronaut.png","--benchmark-frames","6",
                              "--benchmark-walk-step","0.08","--benchmark-step","0"});
    EXPECT_TRUE(options.thirdPersonRenderMode && options.surfaceRenderMode && options.captureOnly);
    EXPECT_EQ(options.benchmarkWalkStep,.08);
}
TEST(CommandLineOptions, CharacterBenchmarksValidateTheirClockAndInputOrder) {
    const auto options=parse({"--astronaut-capture","jet.png","--benchmark-frames","8",
        "--benchmark-character-step",".04","--benchmark-jump-frame","0","--benchmark-boost-frame","3"});
    EXPECT_EQ(options.benchmarkJumpFrame,0); EXPECT_EQ(options.benchmarkBoostFrame,3);
    EXPECT_EQ(options.benchmarkCharacterStep,.04);
    EXPECT_THROW(parse({"--benchmark-character-step","nan"}),std::invalid_argument);
    EXPECT_THROW(parse({"--astronaut-capture","jet.png","--benchmark-frames","8",
        "--benchmark-jump-frame","5","--benchmark-boost-frame","2"}),std::invalid_argument);
    EXPECT_THROW(parse({"--surface-capture","jet.png","--benchmark-frames","8",
        "--benchmark-jump-frame","0"}),std::invalid_argument);
}
TEST(CommandLineOptions, OfflineCaptureControlsValidateBeforeOpeningAWindow) {
    const auto options=parse({"--offline-render","studio.png","--foliage-distance-multiplier","12.5","--no-lens-flare"});
    EXPECT_TRUE(options.offlineQuality && options.surfaceRenderMode && options.captureOnly);
    EXPECT_TRUE(options.atmosphereFullResolution);
    EXPECT_EQ(options.foliageDistanceMultiplier,12.5);
    EXPECT_FALSE(options.lensFlare);
    EXPECT_THROW(parse({"--offline-render"}),std::invalid_argument);
    EXPECT_THROW(parse({"--offline-render","--render-size"}),std::invalid_argument);
    EXPECT_THROW(parse({"--offline-quality"}),std::invalid_argument);
    EXPECT_THROW(parse({"--offline-render","out.png","--foliage-distance-multiplier","nan"}),std::invalid_argument);
    EXPECT_THROW(parse({"--offline-render","out.png","--foliage-distance-multiplier","21"}),std::invalid_argument);
    EXPECT_THROW(parse({"--offline-render","out.png","--foliage-distance-multiplier","0"}),std::invalid_argument);
    EXPECT_THROW(parse({"--offline-render","out.png","--foliage-distance-multiplier","2x"}),std::invalid_argument);
    EXPECT_THROW(parse({"--no-lens-flare"}),std::invalid_argument);
}

TEST(CommandLineOptions, InteractiveComputeIsDefaultAndRequiresResidentGrass) {
    const auto defaults=parse({});
    EXPECT_EQ(defaults.terrainBackend,"compute");
    EXPECT_EQ(defaults.terrainGrassPlanner,"gpu-v1");
    EXPECT_FALSE(defaults.explicitTerrainBackend);
    const auto gpu=parse({"--surface-capture","out.png","--terrain-backend","compute"});
    EXPECT_EQ(gpu.terrainBackend,"compute");EXPECT_TRUE(gpu.explicitTerrainBackend);
    EXPECT_EQ(parse({"--terrain-backend","cpu"}).terrainBackend,"cpu");
    EXPECT_EQ(parse({"--surface-capture","out.png","--terrain-backend","compute","--terrain-grass-planner","gpu"}).terrainGrassPlanner,"gpu-v1");
    EXPECT_EQ(parse({"--terrain-backend","cpu","--terrain-grass-planner","cpu"}).terrainGrassPlanner,"cpu");
    EXPECT_THROW(parse({"--terrain-grass-planner","cpu"}),std::invalid_argument);
    EXPECT_THROW(parse({"--terrain-grass-planner"}),std::invalid_argument);
    EXPECT_THROW(parse({"--terrain-grass-planner","gpu-v2"}),std::invalid_argument);
    EXPECT_THROW(parse({"--terrain-backend"}),std::invalid_argument);
    EXPECT_THROW(parse({"--terrain-backend","fast"}),std::invalid_argument);
    const auto native=parse({"--terrain-backend","compute"});
    EXPECT_EQ(native.terrainBackend,"compute");EXPECT_TRUE(native.explicitTerrainBackend);
    EXPECT_EQ(native.terrainGrassPlanner,"gpu-v1");EXPECT_FALSE(native.renderTestMode);
    EXPECT_THROW(parse({"--terrain-backend","compute","--terrain-grass-planner","cpu"}),std::invalid_argument);
    EXPECT_EQ(parse({"--surface-capture","out.png","--terrain-backend","compute","--terrain-grass-planner","cpu"}).terrainGrassPlanner,"cpu");
}

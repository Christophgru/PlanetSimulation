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
        "--benchmark-overlay", "--performance-trace", "trace.jsonl", "--video-memory-mb", "128"});
    EXPECT_TRUE(options.renderTestMode && options.surfaceRenderMode && options.captureOnly);
    EXPECT_FALSE(options.planetRenderMode);
    EXPECT_TRUE(options.explicitRenderSize && options.explicitAtmosphereQuality);
    EXPECT_TRUE(options.atmosphereFullResolution && options.benchmarkOverlay);
    EXPECT_EQ(options.outputImagePath, "out.png");
    EXPECT_EQ(options.replayPath, "view.json");
    EXPECT_EQ(options.configPath, "scene.json");
    EXPECT_EQ(options.performanceTrace, "trace.jsonl");
    EXPECT_EQ(options.renderTestWidth, 960);
    EXPECT_EQ(options.renderTestHeight, 540);
    EXPECT_EQ(options.commandLineTime, -12.5);
    EXPECT_EQ(options.benchmarkFrames, 2);
    EXPECT_EQ(options.benchmarkStep, 0);
    EXPECT_EQ(options.videoMemoryCapBytes, 128ULL * 1024 * 1024);
}

TEST(CommandLineOptions, InvalidValuesFailBeforeWindowStartup) {
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

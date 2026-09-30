#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace app {
struct CommandLineOptions {
    bool renderTestMode = false;
    bool surfaceRenderMode = false;
    bool planetRenderMode = false;
    bool captureOnly = false;
    bool explicitRenderSize = false;
    std::string outputImagePath;
    int renderTestWidth = 800;
    int renderTestHeight = 600;
    std::optional<double> commandLineTime;
    std::string configPath = "configs/scenarios/solar_system.json";
    std::string replayPath;
    std::string performanceTrace;
    bool atmosphereFullResolution = false;
    bool explicitAtmosphereQuality = false;
    bool benchmarkOverlay = false;
    std::optional<std::uint64_t> videoMemoryCapBytes;
    int benchmarkFrames = 1;
    double benchmarkStep = 1.0 / 60.0;

    static CommandLineOptions parse(int argc, char** argv);
};
}

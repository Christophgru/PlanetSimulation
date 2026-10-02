#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace app {
struct CommandLineOptions {
    bool renderTestMode = false;
    bool surfaceRenderMode = false;
    bool planetRenderMode = false;
    bool thirdPersonRenderMode = false;
    bool captureOnly = false;
    bool offlineQuality = false;
    bool lensFlare = true;
    double foliageDistanceMultiplier = 20.0;
    bool explicitFoliageDistance = false;
    bool explicitLensFlare = false;
    bool explicitRenderSize = false;
    std::string outputImagePath;
    int renderTestWidth = 800;
    int renderTestHeight = 600;
    std::optional<double> commandLineTime;
    std::string configPath = "configs/scenarios/solar_system.json";
    std::string replayPath;
    std::string performanceTrace;
    std::string cpuTrace;
    bool atmosphereFullResolution = false;
    bool explicitAtmosphereQuality = false;
    bool benchmarkOverlay = false;
    std::optional<std::uint64_t> videoMemoryCapBytes;
    int benchmarkFrames = 1;
    double benchmarkStep = 1.0 / 60.0;
    double benchmarkWalkStep = 0.0; // Metres per frame, independent of orbit time.
    double benchmarkCharacterStep = 0.0; // Explicit local flight/gait time for captures.
    int benchmarkJumpFrame = -1, benchmarkBoostFrame = -1;

    static CommandLineOptions parse(int argc, char** argv);
};
}

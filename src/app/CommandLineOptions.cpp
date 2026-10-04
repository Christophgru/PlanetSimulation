#include "app/CommandLineOptions.h"
#include <cmath>
#include <stdexcept>

namespace app {
CommandLineOptions CommandLineOptions::parse(int argc, char** argv) {
    CommandLineOptions options;
    // Parse command-line arguments
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--offline-render") {
            if (i+1>=argc || std::string(argv[i+1]).empty() || std::string(argv[i+1]).starts_with("--"))
                throw std::invalid_argument("--offline-render needs an output PNG path");
            options.offlineQuality = options.renderTestMode = options.surfaceRenderMode = options.captureOnly = true;
            options.outputImagePath = argv[++i];
        } else if (std::string(argv[i]) == "--offline-quality") {
            options.offlineQuality = true;
        } else if (std::string(argv[i]) == "--no-lens-flare") {
            options.lensFlare = false;
            options.explicitLensFlare = true;
        } else if (std::string(argv[i]) == "--foliage-distance-multiplier") {
            try {
                if (i+1>=argc) throw std::invalid_argument("missing");
                const std::string value=argv[++i]; std::size_t used=0;
                options.foliageDistanceMultiplier=std::stod(value,&used);
                if (used!=value.size() || !std::isfinite(options.foliageDistanceMultiplier) ||
                    options.foliageDistanceMultiplier<1 || options.foliageDistanceMultiplier>20)
                    throw std::invalid_argument("range");
                options.explicitFoliageDistance=true;
            } catch (...) { throw std::invalid_argument("--foliage-distance-multiplier needs 1..20"); }
        } else if (std::string(argv[i]) == "--atmosphere-full-resolution") {
            options.atmosphereFullResolution = true;
            options.explicitAtmosphereQuality = true;
        } else if (std::string(argv[i]) == "--benchmark-overlay") {
            options.benchmarkOverlay = true;
        } else if (std::string(argv[i]) == "--video-memory-mb" && i + 1 < argc) {
            try {
                const std::string value = argv[++i];
                std::size_t used = 0;
                const auto mebibytes = std::stoull(value, &used);
                if (used != value.size() || mebibytes < 32 || mebibytes > 65536)
                    throw std::invalid_argument("range");
                options.videoMemoryCapBytes = mebibytes * 1024 * 1024;
            } catch (...) {
                throw std::invalid_argument("--video-memory-mb needs 32..65536 MiB");
            }
        } else if (std::string(argv[i]) == "--cpu-trace") {
            if (i + 1 >= argc || std::string(argv[i + 1]).empty() ||
                std::string(argv[i + 1]).starts_with("--"))
                throw std::invalid_argument("--cpu-trace needs an output JSON path");
            options.cpuTrace = argv[++i];
        } else if (std::string(argv[i]) == "--performance-trace" && i + 1 < argc) {
            options.performanceTrace = argv[++i];
        } else if (std::string(argv[i]) == "--benchmark-frames" && i + 1 < argc) {
            try { options.benchmarkFrames = std::stoi(argv[++i]); } catch (...) { options.benchmarkFrames = 0; }
            if (options.benchmarkFrames < 1 || options.benchmarkFrames > 100000) {
                throw std::invalid_argument("--benchmark-frames needs 1..100000 frames");
            }
        } else if (std::string(argv[i]) == "--benchmark-character-step") {
            try {
                if (i+1>=argc) throw std::invalid_argument("missing");
                const std::string value=argv[++i]; std::size_t used=0;
                options.benchmarkCharacterStep=std::stod(value,&used);
                if (used!=value.size() || !std::isfinite(options.benchmarkCharacterStep) ||
                    options.benchmarkCharacterStep<=0 || options.benchmarkCharacterStep>1)
                    throw std::invalid_argument("range");
            } catch (...) { throw std::invalid_argument("--benchmark-character-step needs 0..1 seconds, excluding zero"); }
        } else if (std::string(argv[i]) == "--benchmark-jump-frame" || std::string(argv[i]) == "--benchmark-boost-frame") {
            const bool boost=std::string(argv[i])=="--benchmark-boost-frame";
            try {
                if (i+1>=argc) throw std::invalid_argument("missing");
                const std::string value=argv[++i]; std::size_t used=0;
                const int frame=std::stoi(value,&used);
                if (used!=value.size() || frame<0 || frame>=100000) throw std::invalid_argument("range");
                (boost ? options.benchmarkBoostFrame : options.benchmarkJumpFrame)=frame;
            } catch (...) { throw std::invalid_argument("Character input frame needs 0..99999"); }
        } else if (std::string(argv[i]) == "--benchmark-walk-step") {
            try {
                if (i + 1 >= argc) throw std::invalid_argument("missing distance");
                const std::string value = argv[++i];
                std::size_t used = 0;
                options.benchmarkWalkStep = std::stod(value, &used);
                if (used != value.size() || !std::isfinite(options.benchmarkWalkStep) ||
                    options.benchmarkWalkStep < 0 || options.benchmarkWalkStep > 100)
                    throw std::invalid_argument("range");
            } catch (...) {
                throw std::invalid_argument("--benchmark-walk-step needs 0..100 metres per frame");
            }
        } else if (std::string(argv[i]) == "--benchmark-step" && i + 1 < argc) {
            try { options.benchmarkStep = std::stod(argv[++i]); } catch (...) { options.benchmarkStep = -1; }
            if (!std::isfinite(options.benchmarkStep) || options.benchmarkStep < 0 || options.benchmarkStep > 60) {
                throw std::invalid_argument("--benchmark-step needs 0..60 seconds");
            }
        } else if (std::string(argv[i]) == "--render-test" && i + 1 < argc) {
            options.renderTestMode = true;
            options.outputImagePath = argv[++i];
        } else if (std::string(argv[i]) == "--surface-render-test" && i + 1 < argc) {
            options.renderTestMode = true;
            options.surfaceRenderMode = true;
            options.outputImagePath = argv[++i];
        } else if (std::string(argv[i]) == "--surface-capture" && i + 1 < argc) {
            options.renderTestMode = options.surfaceRenderMode = options.captureOnly = true;
            options.outputImagePath = argv[++i];
        } else if (std::string(argv[i]) == "--astronaut-capture" && i + 1 < argc) {
            options.renderTestMode = options.surfaceRenderMode = options.captureOnly = options.thirdPersonRenderMode = true;
            options.outputImagePath = argv[++i];
        } else if (std::string(argv[i]) == "--planet-render-test" && i + 1 < argc) {
            options.renderTestMode = true;
            options.planetRenderMode = true;
            options.outputImagePath = argv[++i];
        } else if (std::string(argv[i]) == "--terrain-grass-planner") {
            if(i+1>=argc) throw std::invalid_argument("--terrain-grass-planner requires cpu or gpu");
            const std::string planner=argv[++i];
            if(planner!="cpu" && planner!="gpu") throw std::invalid_argument("--terrain-grass-planner requires cpu or gpu");
            options.terrainGrassPlanner=planner=="gpu" ? "gpu-v1" : "cpu";options.explicitTerrainGrassPlanner=true;
        } else if (std::string(argv[i]) == "--terrain-backend") {
            if(i+1>=argc) throw std::invalid_argument("--terrain-backend requires cpu or compute");
            options.terrainBackend=argv[++i];options.explicitTerrainBackend=true;
            if(options.terrainBackend!="cpu" && options.terrainBackend!="compute")
                throw std::invalid_argument("--terrain-backend requires cpu or compute");
        } else if (std::string(argv[i]) == "--config" && i + 1 < argc) {
            options.configPath = argv[++i];
        } else if (std::string(argv[i]) == "--replay" && i + 1 < argc) {
            options.replayPath = argv[++i];
        } else if (std::string(argv[i]) == "--simulation-time" && i + 1 < argc) {
            try {
                const std::string argument = argv[++i];
                std::size_t consumed = 0;
                options.commandLineTime = std::stod(argument, &consumed);
                if (!std::isfinite(*options.commandLineTime) || consumed != argument.size()) throw std::invalid_argument("time");
            } catch (const std::exception&) {
                throw std::invalid_argument("--simulation-time needs a finite number of seconds");
            }
        } else if (std::string(argv[i]) == "--render-size" && i + 2 < argc) {
            options.explicitRenderSize = true;
            try {
                options.renderTestWidth = std::stoi(argv[++i]);
                options.renderTestHeight = std::stoi(argv[++i]);
            } catch (const std::exception&) {
                throw std::invalid_argument("--render-size needs integer width and height");
            }
            if (options.renderTestWidth < 64 || options.renderTestHeight < 64 ||
                options.renderTestWidth > 8192 || options.renderTestHeight > 8192) {
                throw std::invalid_argument("--render-size must be between 64 and 8192 pixels");
            }
        }
    }

    if(options.terrainBackend=="compute" && !options.renderTestMode)
        throw std::invalid_argument("Experimental compute terrain requires a capture output until T3 consumers are ready");
    if (options.offlineQuality && !options.renderTestMode)
        throw std::invalid_argument("--offline-quality requires a render/capture output");
    if (!options.offlineQuality && (options.explicitFoliageDistance || options.explicitLensFlare) && options.replayPath.empty())
        throw std::invalid_argument("Offline distance and flare options require --offline-render or --offline-quality");
    if (options.offlineQuality) {
        options.captureOnly=true;
        options.atmosphereFullResolution=true;
    }
    if (options.benchmarkFrames > 1 && !options.renderTestMode) {
        throw std::invalid_argument("--benchmark-frames requires a render/capture output");
    }
    if (options.benchmarkWalkStep > 0 &&
        (!options.surfaceRenderMode || options.benchmarkFrames < 2))
        throw std::invalid_argument("--benchmark-walk-step requires a surface capture and at least two frames");
    if ((options.benchmarkCharacterStep>0 || options.benchmarkJumpFrame>=0 || options.benchmarkBoostFrame>=0) &&
        (!options.thirdPersonRenderMode || options.benchmarkFrames<2))
        throw std::invalid_argument("Character benchmarks require an astronaut capture with at least two frames");
    if (options.benchmarkJumpFrame>=options.benchmarkFrames || options.benchmarkBoostFrame>=options.benchmarkFrames ||
        (options.benchmarkBoostFrame>=0 && (options.benchmarkJumpFrame<0 || options.benchmarkBoostFrame<=options.benchmarkJumpFrame)))
        throw std::invalid_argument("Jump must precede boost within the captured frames");
    return options;
}
}

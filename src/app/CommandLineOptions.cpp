#include "app/CommandLineOptions.h"
#include <cmath>
#include <stdexcept>

namespace app {
CommandLineOptions CommandLineOptions::parse(int argc, char** argv) {
    CommandLineOptions options;
    // Parse command-line arguments
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--atmosphere-full-resolution") {
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
        } else if (std::string(argv[i]) == "--performance-trace" && i + 1 < argc) {
            options.performanceTrace = argv[++i];
        } else if (std::string(argv[i]) == "--benchmark-frames" && i + 1 < argc) {
            try { options.benchmarkFrames = std::stoi(argv[++i]); } catch (...) { options.benchmarkFrames = 0; }
            if (options.benchmarkFrames < 1 || options.benchmarkFrames > 100000) {
                throw std::invalid_argument("--benchmark-frames needs 1..100000 frames");
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
        } else if (std::string(argv[i]) == "--planet-render-test" && i + 1 < argc) {
            options.renderTestMode = true;
            options.planetRenderMode = true;
            options.outputImagePath = argv[++i];
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

    if (options.benchmarkFrames > 1 && !options.renderTestMode) {
        throw std::invalid_argument("--benchmark-frames requires a render/capture output");
    }
    return options;
}
}

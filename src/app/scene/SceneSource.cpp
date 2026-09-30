#include "app/scene/SceneSource.h"
#include "config/ScenarioConfig.h"
#include "config/SceneReplay.h"

namespace app {
SceneSource::SceneSource(CommandLineOptions& options)
    : watchedScenePath(options.replayPath.empty() ? options.configPath : options.replayPath),
      configPath(options.configPath), replayPath(options.replayPath) {
    try {
        document = load();
        (void)config::ScenarioConfig(config::Config(nlohmann::json(document)));
        if (!replayPath.empty()) {
            const auto replay = config::Config::load(replayPath).data();
            if (replay.contains("render") && !options.explicitRenderSize) {
                options.renderTestWidth = replay.at("render").at("width").get<int>();
                options.renderTestHeight = replay.at("render").at("height").get<int>();
                if (options.renderTestWidth < 64 || options.renderTestWidth > 8192 || options.renderTestHeight < 64 || options.renderTestHeight > 8192)
                    throw std::invalid_argument("Replay render dimensions must be in 64..8192");
            }
            if (!options.explicitAtmosphereQuality && replay.contains("render") &&
                replay["render"].contains("atmosphere_downsample")) {
                const auto& raw = replay["render"]["atmosphere_downsample"];
                if (!raw.is_number_integer() || (raw != 1 && raw != 4))
                    throw std::invalid_argument("Replay atmosphere_downsample must be 1 or 4");
                options.atmosphereFullResolution = raw == 1;
            }
        }
    } catch (const std::exception& error) {
        throw std::invalid_argument(std::string("Invalid scenario or replay: ") + error.what());
    }
}
nlohmann::json SceneSource::load() const {
    // A full image sidecar is self-contained; a console snippet uses --config.
    const nlohmann::json replay = replayPath.empty() ? nlohmann::json() : config::Config::load(replayPath).data();
    auto document = replay.is_object() && replay.contains("scenario") ? replay.at("scenario") :
                    config::Config::load(configPath).data();
    if (!replayPath.empty()) document = config::applyCameraReplay(std::move(document), replay);
    return document;
}
}

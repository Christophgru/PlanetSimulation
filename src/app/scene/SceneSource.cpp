#include "app/scene/SceneSource.h"
#include "config/ScenarioConfig.h"
#include "config/SceneReplay.h"
#include "rendering/geometry/terrain/TerrainTopology.h"

namespace app {
SceneSource SceneSource::forResidentReload(CommandLineOptions& options) {
    if(options.terrainBackend!="compute" || options.terrainGrassPlanner!="gpu-v1")
        throw std::invalid_argument("Resident reload requires compute terrain and GPU grass");
    return SceneSource(options);
}
SceneSource::SceneSource(CommandLineOptions& options)
    : watchedScenePath(options.replayPath.empty() ? options.configPath : options.replayPath),
      configPath(options.configPath), replayPath(options.replayPath) {
    try {
        replayDocument=replayPath.empty() ? nlohmann::json() : config::Config::load(replayPath).data();
        document = documentFor(replayDocument);
        (void)config::ScenarioConfig(config::Config(nlohmann::json(document)));
        if (!replayPath.empty()) {
            const auto& replay = replayDocument;
            if(replay.contains("render") && replay["render"].contains("terrain_plan_eyes_world_units")) {
                const auto& anchors=replay["render"]["terrain_plan_eyes_world_units"];
                if(!anchors.is_array() || anchors.size()!=document.at("planets").size())
                    throw std::invalid_argument("Terrain planning anchors require one eye per planet");
                for(const auto& eye:anchors) {
                    double length2=0;
                    if(!eye.is_array() || eye.size()!=3)
                        throw std::invalid_argument("Terrain planning anchors require finite three-vectors");
                    for(const auto& value:eye) {
                        if(!value.is_number() || !std::isfinite(value.get<double>()))
                            throw std::invalid_argument("Terrain planning anchors require finite three-vectors");
                        const double v=value.get<double>();length2+=v*v;
                    }
                    if(!std::isfinite(length2) || length2<=0)
                        throw std::invalid_argument("Terrain planning anchors cannot be at body centers");
                }
            }
            // Replays predating backend metadata were recorded with CPU terrain.
            if(!options.explicitTerrainBackend &&
               !(replay.contains("render") && replay["render"].contains("terrain_backend")))
                options.terrainBackend="cpu";
            if(replay.contains("render") && replay["render"].contains("terrain_backend")) {
                const auto& backend=replay["render"]["terrain_backend"];
                if(!backend.is_string() || (backend!="cpu" && backend!="compute"))
                    throw std::invalid_argument("Invalid terrain replay backend");
                if(backend=="compute" && (!options.explicitTerrainBackend || options.terrainBackend=="compute")) {
                    const auto& contract=replay["render"].at("terrain_contract");
                    const auto& field=contract.at("field_version");
                    const auto& topology=contract.at("topology_version");
                    if(!field.is_number_integer() || field!=rendering::PlanetField::version ||
                       !topology.is_number_integer() || topology<1 || topology>3 ||
                       !rendering::supportedTerrainTopology(topology.get<std::uint32_t>()))
                        throw std::invalid_argument("Unsupported terrain compute replay versions");
                }
                if(!options.explicitTerrainBackend) {
                    options.terrainBackend=backend.get<std::string>();
                    options.lockedTerrainBackend=options.terrainBackend=="compute";
                }
            }
            if(options.terrainBackend=="compute") {
                const auto planner=replay.contains("render") ?
                    replay.at("render").value("terrain_grass_planner",nlohmann::json("cpu")) : nlohmann::json("cpu");
                if(!planner.is_string() || (planner!="cpu" && planner!="gpu-v1"))
                    throw std::invalid_argument("Unsupported terrain grass replay planner");
                if(!options.explicitTerrainGrassPlanner) options.terrainGrassPlanner=planner.get<std::string>();
            }
            if (replay.contains("render") && !options.explicitRenderSize) {
                options.renderTestWidth = replay.at("render").at("width").get<int>();
                options.renderTestHeight = replay.at("render").at("height").get<int>();
                if (options.renderTestWidth < 64 || options.renderTestWidth > 8192 || options.renderTestHeight < 64 || options.renderTestHeight > 8192)
                    throw std::invalid_argument("Replay render dimensions must be in 64..8192");
            }
            if (replay.contains("render") && replay["render"].value("camera_mode",std::string{})=="third_person")
                options.thirdPersonRenderMode=true;
            if (options.renderTestMode && replay.contains("render") && replay["render"].contains("offline")) {
                const auto& saved=replay["render"]["offline"];
                if (!saved.is_object() || !saved.at("enabled").is_boolean() ||
                    !saved.at("lens_flare").is_boolean() || !saved.at("foliage_distance_multiplier").is_number())
                    throw std::invalid_argument("Invalid offline replay quality");
                const double multiplier=saved.at("foliage_distance_multiplier").get<double>();
                if (!std::isfinite(multiplier) || multiplier<1 || multiplier>20)
                    throw std::invalid_argument("Offline replay multiplier must be in 1..20");
                options.offlineQuality=options.offlineQuality || saved.at("enabled").get<bool>();
                if (!options.explicitFoliageDistance) options.foliageDistanceMultiplier=multiplier;
                if (!options.explicitLensFlare) options.lensFlare=saved.at("lens_flare").get<bool>();
            }
            if (!options.explicitAtmosphereQuality && replay.contains("render") &&
                replay["render"].contains("atmosphere_downsample")) {
                const auto& raw = replay["render"]["atmosphere_downsample"];
                if (!raw.is_number_integer() || (raw != 1 && raw != 4))
                    throw std::invalid_argument("Replay atmosphere_downsample must be 1 or 4");
                options.atmosphereFullResolution = raw == 1;
            }
            if (options.offlineQuality) options.atmosphereFullResolution=options.captureOnly=true;
        }
        if(options.terrainBackend=="compute" && !options.renderTestMode && options.terrainGrassPlanner!="gpu-v1")
            throw std::invalid_argument("Interactive compute terrain requires GPU grass planning; override legacy replay with --terrain-grass-planner gpu");
        if (!options.offlineQuality && (options.explicitFoliageDistance || options.explicitLensFlare))
            throw std::invalid_argument("Offline controls require an offline capture or replay");
    } catch (const std::exception& error) {
        throw std::invalid_argument(std::string("Invalid scenario or replay: ") + error.what());
    }
}
nlohmann::json SceneSource::load() const {
    // A full image sidecar is self-contained; a console snippet uses --config.
    const nlohmann::json replay = replayPath.empty() ? nlohmann::json() : config::Config::load(replayPath).data();
    return documentFor(replay);
}
nlohmann::json SceneSource::documentFor(const nlohmann::json& replay) const {
    auto document = replay.is_object() && replay.contains("scenario") ? replay.at("scenario") :
                    config::Config::load(configPath).data();
    if (!replayPath.empty()) document = config::applyCameraReplay(std::move(document), replay);
    return document;
}
}

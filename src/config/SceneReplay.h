#pragma once

#include "config/ScenarioConfig.h"

namespace config {

// A console snippet uses the supplied scenario. Image sidecars also contain
// the resolved scenario, so later edits to the working config cannot alter it.
inline nlohmann::json applyCameraReplay(nlohmann::json scenario, const nlohmann::json& replay) {
    if (!replay.is_object()) throw std::invalid_argument("Camera replay must be a JSON object");
    if (replay.contains("scenario")) {
        if (!replay["scenario"].is_object()) throw std::invalid_argument("Replay scenario must be an object");
        scenario = replay["scenario"];
    }
    const auto& camera = replay.contains("surface_camera") ? replay["surface_camera"] : replay;
    if (!camera.is_object() || !camera.contains("latitude_deg") || !camera.contains("longitude_deg"))
        throw std::invalid_argument("Camera replay needs latitude_deg and longitude_deg");
    scenario["surface_camera"] = camera;
    // Validate before replacing the live scene, including timestamp and vectors.
    (void)ScenarioConfig(Config(nlohmann::json(scenario)));
    return scenario;
}

inline double replayStartTime(const ScenarioConfig& scene, std::optional<double> commandLineTime = std::nullopt) {
    const double time = commandLineTime.value_or(scene.surface_camera.simulation_time_seconds.value_or(0.0));
    if (!std::isfinite(time)) throw std::invalid_argument("Simulation start time must be finite");
    return time;
}

} // namespace config

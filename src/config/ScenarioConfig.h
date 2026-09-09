#pragma once

#include "config/Config.h"
#include <string>
#include <vector>

namespace config {

struct SunConfig {
    std::vector<double> position = {0.0, 0.0, 0.0};
    double radius = 10.0;
    std::vector<double> color = {1.0, 0.9, 0.7};
    
    SunConfig() = default;
    SunConfig(const config::Config& cfg) {
        position = cfg.getArray("position", position);
        radius = cfg.getDouble("radius", radius);
        color = cfg.getArray("color", color);
    }
};

struct PlanetConfig {
    std::vector<double> position = {0.0, 0.0, 0.0};
    double orbit_radius = 5.0;
    double orbit_speed = 0.02;
    double radius = 1.5;
    std::vector<double> color = {0.3, 0.6, 0.9};
    int noise_seed = 42;
    bool atmosphere_enabled = false;
    double atmosphere_height = 0.3;
    
    PlanetConfig() = default;
    PlanetConfig(const config::Config& cfg) {
        position = cfg.getArray("position", position);
        orbit_radius = cfg.getDouble("orbit_radius", orbit_radius);
        orbit_speed = cfg.getDouble("orbit_speed", orbit_speed);
        radius = cfg.getDouble("radius", radius);
        color = cfg.getArray("color", color);
        noise_seed = cfg.getInt("noise_seed", noise_seed);
        atmosphere_enabled = cfg.getBool("atmosphere_enabled", atmosphere_enabled);
        atmosphere_height = cfg.getDouble("atmosphere_height", atmosphere_height);
    }
};

struct ScenarioConfig {
    std::string name = "Unnamed";
    SunConfig sun;
    std::vector<PlanetConfig> planets;
    
    ScenarioConfig() = default;
    ScenarioConfig(const config::Config& cfg) {
        name = cfg.get("scenario_name", name);
        sun = SunConfig(cfg);
        
        // Check for both "planet" (singular) and "planets" (plural) keys
        auto planet_array = cfg.getArray("planets", std::vector<PlanetConfig>{});
        if (planet_array.empty()) {
            // Try singular "planet" key - use public accessor
            const auto& raw_data = cfg.data();
            auto planet_json = raw_data["planet"];
            if (planet_json.is_object() && !planet_json.empty()) {
                planets.emplace_back(PlanetConfig{cfg});
            }
        } else {
            for (const auto& p : planet_array) {
                planets.emplace_back(p);
            }
        }
    }
};

} // namespace config

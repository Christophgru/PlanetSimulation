#pragma once

#include <string>
#include <vector>
#include "config/Config.h"

namespace config {

struct SunConfig {
    std::vector<double> position = {0.0, 0.0, 0.0};
    double radius = 10.0;
    std::vector<double> color = {1.0, 0.9, 0.7};
    
    SunConfig() = default;
    SunConfig(const config::Config& cfg) {
        auto pos_array = cfg.getArray("position", position);
        if (pos_array.size() >= 3) {
            position = {pos_array[0], pos_array[1], pos_array[2]};
        }
        radius = cfg.getDouble("radius", radius);
        auto col_array = cfg.getArray("color", color);
        if (col_array.size() >= 3) {
            color = {col_array[0], col_array[1], col_array[2]};
        }
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
        auto pos_array = cfg.getArray("position", position);
        if (pos_array.size() >= 3) {
            position = {pos_array[0], pos_array[1], pos_array[2]};
        }
        orbit_radius = cfg.getDouble("orbit_radius", orbit_radius);
        orbit_speed = cfg.getDouble("orbit_speed", orbit_speed);
        radius = cfg.getDouble("radius", radius);
        auto col_array = cfg.getArray("color", color);
        if (col_array.size() >= 3) {
            color = {col_array[0], col_array[1], col_array[2]};
        }
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
        
        // Only parse sun if it exists in the config
        const auto& raw_data = cfg.data();
        auto sun_it = raw_data.find("sun");
        if (sun_it != raw_data.end() && sun_it->is_object()) {
            config::Config sun_cfg{std::move(*sun_it)};
            sun = SunConfig(sun_cfg);
        }
        
        // Check for "planets" array first, then fall back to single "planet" object
        auto planets_it = raw_data.find("planets");
        if (planets_it != raw_data.end() && planets_it->is_array()) {
            // Iterate over each planet object in the array
            for (const auto& planet_json : planets_it.value()) {
                config::Config planet_cfg{std::move(*planet_json)};
                planets.emplace_back(PlanetConfig{planet_cfg});
            }
        } else {
            // Try singular "planet" key
            auto planet_it = raw_data.find("planet");
            if (planet_it != raw_data.end() && planet_it->is_object()) {
                config::Config planet_cfg{std::move(*planet_it)};
                planets.emplace_back(planet_cfg);
            }
        }
    }
};

} // namespace config

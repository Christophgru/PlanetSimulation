#pragma once

#include <array>
#include <cmath>
#include <optional>
#include <stdexcept>
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

struct SurfaceCameraConfig {
    bool enabled = false;
    std::string reference_frame = "planet_spherical_ned";
    int planet_index = 0;
    double latitude_deg = 0.0;
    double longitude_deg = 180.0;
    double altitude = 0.2;
    double fov = 60.0;
    std::optional<std::array<double, 3>> direction_ned;
    std::optional<std::array<double, 3>> up_ned;

    SurfaceCameraConfig() = default;
    explicit SurfaceCameraConfig(const config::Config& cfg) {
        enabled = true;
        reference_frame = cfg.get("reference_frame", reference_frame);
        planet_index = cfg.getInt("planet_index", planet_index);
        latitude_deg = cfg.getDouble("latitude_deg", latitude_deg);
        longitude_deg = cfg.getDouble("longitude_deg", longitude_deg);
        altitude = cfg.getDouble("altitude", altitude);
        fov = cfg.getDouble("fov", fov);
        auto parseNedVector = [&cfg](const char* key) {
            const auto& raw = cfg.data().at(key);
            if (!raw.is_array() || raw.size() != 3 ||
                !raw[0].is_number() || !raw[1].is_number() ||
                !raw[2].is_number()) {
                throw std::invalid_argument(std::string("surface_camera.") + key +
                                            " must be a nonzero 3-vector");
            }
            const std::array<double, 3> values{
                raw[0].get<double>(), raw[1].get<double>(), raw[2].get<double>()};
            const double magnitude = std::hypot(values[0], values[1], values[2]);
            if (!std::isfinite(magnitude) || magnitude <= 1e-12) {
                throw std::invalid_argument(std::string("surface_camera.") + key +
                                            " must be a nonzero finite 3-vector");
            }
            return values;
        };
        if (cfg.data().contains("direction_ned")) {
            direction_ned = parseNedVector("direction_ned");
        }
        if (cfg.data().contains("up_ned")) {
            up_ned = parseNedVector("up_ned");
            if (!direction_ned) {
                throw std::invalid_argument("surface_camera.up_ned requires direction_ned");
            }
            const double directionLength = std::hypot(
                (*direction_ned)[0], (*direction_ned)[1], (*direction_ned)[2]);
            const double upLength = std::hypot(
                (*up_ned)[0], (*up_ned)[1], (*up_ned)[2]);
            double dot = 0.0;
            for (int axis = 0; axis < 3; ++axis) {
                dot += ((*direction_ned)[axis] / directionLength) *
                       ((*up_ned)[axis] / upLength);
            }
            if (std::abs(dot) > 0.999999) {
                throw std::invalid_argument("surface_camera.up_ned must not align with direction_ned");
            }
        }
    }
};

struct ScenarioConfig {
    std::string name = "Unnamed";
    std::string distance_unit = "km";
    SunConfig sun;
    std::vector<PlanetConfig> planets;
    SurfaceCameraConfig surface_camera;
    
    ScenarioConfig() = default;
    double metersPerWorldUnit() const { return distance_unit == "km" ? 1000.0 : 1.0; }
    ScenarioConfig(const config::Config& cfg) {
        name = cfg.get("scenario_name", name);
        distance_unit = cfg.get("distance_unit", distance_unit);
        if (distance_unit != "km" && distance_unit != "m") {
            throw std::invalid_argument("distance_unit must be km or m");
        }
        
        // Only parse sun if it exists in the config
        const auto& raw_data = cfg.data();
        auto sun_it = raw_data.find("sun");
        if (sun_it != raw_data.end() && sun_it->is_object()) {
            // Copy the json value first, then move it to Config constructor
            config::Config sun_cfg{std::move(nlohmann::json(*sun_it))};
            sun = SunConfig(sun_cfg);
        }
        
        // Check for "planets" array first, then fall back to single "planet" object
        auto planets_it = raw_data.find("planets");
        if (planets_it != raw_data.end() && planets_it->is_array()) {
            // Iterate over each planet object in the array
            for (const auto& planet_json : planets_it.value()) {
                // Copy the json value first, then move it to Config constructor
                config::Config planet_cfg{std::move(nlohmann::json(planet_json))};
                planets.emplace_back(PlanetConfig{planet_cfg});
            }
        } else {
            // Try singular "planet" key
            auto planet_it = raw_data.find("planet");
            if (planet_it != raw_data.end() && planet_it->is_object()) {
                // Copy the json value first, then move it to Config constructor
                config::Config planet_cfg{std::move(nlohmann::json(*planet_it))};
                planets.emplace_back(planet_cfg);
            }
        }

        auto surface_it = raw_data.find("surface_camera");
        if (surface_it != raw_data.end()) {
            if (!surface_it->is_object()) {
                throw std::invalid_argument("surface_camera must be an object");
            }
            config::Config surface_cfg{nlohmann::json(*surface_it)};
            surface_camera = SurfaceCameraConfig(surface_cfg);
            if (surface_camera.reference_frame != "planet_spherical_ned" ||
                surface_camera.planet_index < 0 ||
                static_cast<size_t>(surface_camera.planet_index) >= planets.size() ||
                !std::isfinite(surface_camera.latitude_deg) ||
                surface_camera.latitude_deg < -90.0 || surface_camera.latitude_deg > 90.0 ||
                !std::isfinite(surface_camera.longitude_deg) ||
                !std::isfinite(surface_camera.altitude) || surface_camera.altitude < 0.0 ||
                !std::isfinite(surface_camera.fov) ||
                surface_camera.fov <= 0.0 || surface_camera.fov >= 180.0) {
                throw std::invalid_argument("Invalid surface_camera reference frame or coordinates");
            }
        }
    }
};

} // namespace config

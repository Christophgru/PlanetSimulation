#pragma once

#include <algorithm>
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
        if (position.size() != 3 ||
            !std::all_of(position.begin(), position.end(), [](double v) { return std::isfinite(v); }) ||
            !std::isfinite(radius) || radius <= 0.0 || color.size() != 3 ||
            !std::all_of(color.begin(), color.end(), [](double v) {
                return std::isfinite(v) && v >= 0.0 && v <= 1.0;
            }))
            throw std::invalid_argument("Invalid Sun position, radius or color");
    }
};

struct OrbitViewConfig {
    std::array<double, 3> position{12.0, 0.0, 0.5};
    std::array<double, 3> target{0.0, 0.0, 0.0};
    double fov = 60.0;

    OrbitViewConfig() = default;
    explicit OrbitViewConfig(const config::Config& cfg) {
        auto readVector = [&cfg](const char* key, std::array<double, 3>& value) {
            if (!cfg.data().contains(key)) return;
            const auto& raw = cfg.data().at(key);
            if (!raw.is_array() || raw.size() != 3 ||
                !std::all_of(raw.begin(), raw.end(), [](const auto& element) {
                    return element.is_number() && std::isfinite(element.template get<double>());
                })) {
                throw std::invalid_argument(std::string("camera.") + key +
                                            " must be a finite 3-vector");
            }
            for (int i = 0; i < 3; ++i) value[i] = raw[i].get<double>();
        };
        readVector("position", position);
        readVector("target", target);
        fov = cfg.getDouble("fov", fov);
        if (!std::isfinite(fov) || fov <= 0.0 || fov >= 180.0 ||
            std::hypot(position[0] - target[0], position[1] - target[1],
                       position[2] - target[2]) <= 1e-12)
            throw std::invalid_argument("Invalid camera position, target or field of view");
    }
};

struct PlanetConfig {
    struct SurfaceNoiseFunction {
        std::string type = "value_fbm";
        double amplitude_m = 0.0;
        double frequency = 4.0;
        int octaves = 4;
        double persistence = 0.5;
        double lacunarity = 2.0;
        int seed = 42;

        explicit SurfaceNoiseFunction(const config::Config& cfg, int defaultSeed = 42)
            : seed(defaultSeed) {
            type = cfg.get("type", type);
            amplitude_m = cfg.getDouble("amplitude_m", amplitude_m);
            frequency = cfg.getDouble("frequency", frequency);
            octaves = cfg.getInt("octaves", octaves);
            persistence = cfg.getDouble("persistence", persistence);
            lacunarity = cfg.getDouble("lacunarity", lacunarity);
            seed = cfg.getInt("seed", seed);
            validate();
        }
        SurfaceNoiseFunction() = default;

        void validate() const {
            if ((type != "value_fbm" && type != "ridged_fbm") ||
                !std::isfinite(amplitude_m) || amplitude_m < 0.0 ||
                !std::isfinite(frequency) || frequency <= 0.0 || frequency > 64.0 ||
                octaves < 1 || octaves > 6 ||
                !std::isfinite(persistence) || persistence <= 0.0 || persistence > 1.0 ||
                !std::isfinite(lacunarity) || lacunarity < 1.0 || lacunarity > 4.0) {
                throw std::invalid_argument("Invalid planet.surface_noise function");
            }
        }
    };

    struct TerrainLod {
        int base_edge_segments = 1;
        int max_edge_segments = 16;
        int medium_edge_segments = 8;
        int steep_edge_segments = 16;
        double steep_slope_threshold = 0.35;
        double near_surface_distance_m = 35.0;
        double mid_surface_distance_m = 110.0;
        int max_triangle_budget = 60000;
        double lod_near_diameters = 2.0;
        double lod_far_diameters = 8.0;

        explicit TerrainLod(const config::Config& cfg) {
            base_edge_segments = cfg.getInt("base_edge_segments", base_edge_segments);
            max_edge_segments = cfg.getInt("max_edge_segments", max_edge_segments);
            medium_edge_segments = cfg.getInt("medium_edge_segments",
                std::max(base_edge_segments, std::min(medium_edge_segments, max_edge_segments)));
            steep_edge_segments = cfg.getInt("steep_edge_segments", max_edge_segments);
            steep_slope_threshold = cfg.getDouble("steep_slope_threshold",
                                                  steep_slope_threshold);
            near_surface_distance_m = cfg.getDouble("near_surface_distance_m", near_surface_distance_m);
            mid_surface_distance_m = cfg.getDouble("mid_surface_distance_m", mid_surface_distance_m);
            max_triangle_budget = cfg.getInt("max_triangle_budget", max_triangle_budget);
            lod_near_diameters = cfg.getDouble("lod_near_diameters", lod_near_diameters);
            lod_far_diameters = cfg.getDouble("lod_far_diameters", lod_far_diameters);
            validate();
        }
        TerrainLod() = default;

        void validate() const {
            if (base_edge_segments < 1 || max_edge_segments < base_edge_segments ||
                max_edge_segments > 32 ||
                medium_edge_segments < base_edge_segments ||
                medium_edge_segments > max_edge_segments ||
                steep_edge_segments < max_edge_segments || steep_edge_segments > 32 ||
                !std::isfinite(steep_slope_threshold) ||
                steep_slope_threshold <= 0.0 || steep_slope_threshold > 4.0 ||
                !std::isfinite(near_surface_distance_m) || near_surface_distance_m <= 0.0 ||
                !std::isfinite(mid_surface_distance_m) ||
                mid_surface_distance_m <= near_surface_distance_m ||
                max_triangle_budget < 10000 || max_triangle_budget > 100000 ||
                320 * 3 * base_edge_segments *
                    (2 * std::max(1, (base_edge_segments + 1) / 2) - 1) > max_triangle_budget ||
                !std::isfinite(lod_near_diameters) || lod_near_diameters <= 0.0 ||
                !std::isfinite(lod_far_diameters) ||
                lod_far_diameters <= lod_near_diameters) {
                throw std::invalid_argument("Invalid planet.terrain_lod parameters");
            }
        }
    };

    struct TerrainLandscape {
        bool enabled = false;
        double elevation_offset_m = 0.0;
        double continent_amplitude_m = 0.0;
        double continent_frequency = 1.5;
        double plain_threshold = 0.45;
        double cliff_threshold = 0.62;
        double cliff_amplitude_m = 0.0;
        double cliff_frequency = 7.0;
        double ridge_smoothing = 0.0;
        int seed = 1001;

        TerrainLandscape() = default;
        explicit TerrainLandscape(const config::Config& cfg) {
            enabled = cfg.getBool("enabled", true);
            elevation_offset_m = cfg.getDouble("elevation_offset_m", elevation_offset_m);
            continent_amplitude_m = cfg.getDouble("continent_amplitude_m", continent_amplitude_m);
            continent_frequency = cfg.getDouble("continent_frequency", continent_frequency);
            plain_threshold = cfg.getDouble("plain_threshold", plain_threshold);
            cliff_threshold = cfg.getDouble("cliff_threshold", cliff_threshold);
            cliff_amplitude_m = cfg.getDouble("cliff_amplitude_m", cliff_amplitude_m);
            cliff_frequency = cfg.getDouble("cliff_frequency", cliff_frequency);
            ridge_smoothing = cfg.getDouble("ridge_smoothing", ridge_smoothing);
            seed = cfg.getInt("seed", seed);
            validate();
        }
        void validate() const {
            if (!std::isfinite(elevation_offset_m) ||
                !std::isfinite(continent_amplitude_m) || continent_amplitude_m < 0.0 ||
                !std::isfinite(continent_frequency) || continent_frequency <= 0.0 || continent_frequency > 16.0 ||
                !std::isfinite(plain_threshold) || plain_threshold <= 0.0 || plain_threshold >= 1.0 ||
                !std::isfinite(cliff_threshold) || cliff_threshold <= plain_threshold || cliff_threshold >= 1.0 ||
                !std::isfinite(cliff_amplitude_m) || cliff_amplitude_m < 0.0 ||
                !std::isfinite(cliff_frequency) || cliff_frequency <= 0.0 || cliff_frequency > 32.0 ||
                !std::isfinite(ridge_smoothing) || ridge_smoothing < 0.0 ||
                ridge_smoothing > 0.5)
                throw std::invalid_argument("Invalid planet.terrain_landscape parameters");
        }
        double maximumAbsoluteHeightMeters() const {
            return enabled ? std::abs(elevation_offset_m) + continent_amplitude_m + cliff_amplitude_m : 0.0;
        }
    };

    struct Water {
        bool enabled = false;
        double level_m = 0.0;
        double opacity = 0.5;
        double reflection_fraction = 0.5;
        std::vector<double> color = {0.05, 0.35, 0.6};

        Water() = default;
        explicit Water(const config::Config& cfg) {
            enabled = cfg.getBool("enabled", true);
            level_m = cfg.getDouble("level_m", level_m);
            opacity = cfg.getDouble("opacity", opacity);
            reflection_fraction = cfg.getDouble("reflection_fraction", reflection_fraction);
            const auto parsedColor = cfg.getArray("color", color);
            if (parsedColor.size() != 3) throw std::invalid_argument("planet.water.color must have three channels");
            color = parsedColor;
            validate();
        }
        void validate() const {
            if (!std::isfinite(level_m) || !std::isfinite(opacity) || opacity < 0.0 || opacity > 1.0 ||
                !std::isfinite(reflection_fraction) || reflection_fraction < 0.0 || reflection_fraction > 1.0 ||
                color.size() != 3 ||
                !std::all_of(color.begin(), color.end(), [](double c) { return std::isfinite(c) && c >= 0.0 && c <= 1.0; }))
                throw std::invalid_argument("Invalid planet.water parameters");
        }
    };

    std::vector<double> position = {0.0, 0.0, 0.0};
    double orbit_radius = 5.0;
    double orbit_speed = 0.02;
    double radius = 1.5;
    std::vector<double> color = {0.3, 0.6, 0.9};
    int noise_seed = 42;
    std::vector<SurfaceNoiseFunction> surface_noise;
    TerrainLod terrain_lod;
    TerrainLandscape terrain_landscape;
    Water water;
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
        if (cfg.data().contains("surface_noise")) {
            const auto& raw = cfg.data().at("surface_noise");
            if (!raw.is_array() || raw.size() > 8) {
                throw std::invalid_argument("planet.surface_noise must be an array of at most eight functions");
            }
            for (std::size_t i = 0; i < raw.size(); ++i) {
                if (!raw[i].is_object()) {
                    throw std::invalid_argument("planet.surface_noise entries must be objects");
                }
                surface_noise.emplace_back(config::Config{nlohmann::json(raw[i])},
                                           noise_seed);
            }
        }
        if (cfg.data().contains("terrain_lod")) {
            const auto& raw = cfg.data().at("terrain_lod");
            if (!raw.is_object()) {
                throw std::invalid_argument("planet.terrain_lod must be an object");
            }
            terrain_lod = TerrainLod(config::Config{nlohmann::json(raw)});
        }
        if (cfg.data().contains("terrain_landscape")) {
            const auto& raw = cfg.data().at("terrain_landscape");
            if (!raw.is_object()) throw std::invalid_argument("planet.terrain_landscape must be an object");
            terrain_landscape = TerrainLandscape(config::Config{nlohmann::json(raw)});
        }
        if (cfg.data().contains("water")) {
            const auto& raw = cfg.data().at("water");
            if (!raw.is_object()) throw std::invalid_argument("planet.water must be an object");
            water = Water(config::Config{nlohmann::json(raw)});
        }
        if (position.size() != 3 ||
            !std::all_of(position.begin(), position.end(), [](double v) { return std::isfinite(v); }) ||
            !std::isfinite(radius) || radius <= 0.0 || color.size() != 3 ||
            !std::all_of(color.begin(), color.end(), [](double v) {
                return std::isfinite(v) && v >= 0.0 && v <= 1.0;
            }))
            throw std::invalid_argument("Invalid planet position, radius or color");
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
    double walk_speed_mps = 8.0;
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
        walk_speed_mps = cfg.getDouble("walk_speed_mps", walk_speed_mps);
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
    OrbitViewConfig camera;
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

        for (int i = 0; i < 3; ++i) {
            camera.position[i] += sun.position[i];
            camera.target[i] = sun.position[i];
        }
        auto camera_it = raw_data.find("camera");
        if (camera_it != raw_data.end()) {
            if (!camera_it->is_object())
                throw std::invalid_argument("camera must be an object");
            camera = OrbitViewConfig(config::Config{nlohmann::json(*camera_it)});
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

        for (const auto& planet : planets) {
            double totalAmplitudeMeters = 0.0;
            for (const auto& function : planet.surface_noise)
                totalAmplitudeMeters += function.amplitude_m;
            totalAmplitudeMeters += planet.terrain_landscape.maximumAbsoluteHeightMeters();
            if (!std::isfinite(totalAmplitudeMeters) ||
                totalAmplitudeMeters >= planet.radius * metersPerWorldUnit()) {
                throw std::invalid_argument("Planet radius must exceed terrain height");
            }
            if (planet.water.enabled &&
                std::abs(planet.water.level_m) >= planet.radius * metersPerWorldUnit())
                throw std::invalid_argument("Water level must be within the planet radius");
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
                surface_camera.fov <= 0.0 || surface_camera.fov >= 180.0 ||
                !std::isfinite(surface_camera.walk_speed_mps) ||
                surface_camera.walk_speed_mps <= 0.0 ||
                surface_camera.walk_speed_mps > 100.0) {
                throw std::invalid_argument("Invalid surface_camera reference frame or coordinates");
            }
        }
    }
};

} // namespace config

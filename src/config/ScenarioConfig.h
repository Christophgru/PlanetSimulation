#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include "config/OrbitalConfig.h"
#include "config/LightingConfig.h"
#include "config/AtmosphereConfig.h"
#include "config/FoliageConfig.h"

namespace config {
class Config;

struct SunConfig {
    std::string name = "sun";
    double mass_kg = 1.0e19;
    double absolute_magnitude = 4.74; // Bolometric absolute magnitude (IAU zero point).
    std::vector<double> position = {0.0, 0.0, 0.0};
    double radius = 10.0;
    std::vector<double> color = {1.0, 0.9, 0.7};
    
    SunConfig() = default;
    SunConfig(const config::Config& cfg);
};

struct SkyboxConfig {
    bool enabled = true;
    int seed = 7429;
    double star_density = 0.003;
    double star_scale = 700.0;
    double star_brightness = 1.4;
    double ambient_light = 0.12;
    std::vector<double> background_color = {0.002, 0.004, 0.012};
    std::vector<double> star_color = {0.82, 0.9, 1.0};

    SkyboxConfig() = default;
    explicit SkyboxConfig(const config::Config& cfg);

    void validate() const {
        const auto validColor = [](const std::vector<double>& color) {
            return color.size() == 3 &&
                   std::all_of(color.begin(), color.end(), [](double channel) {
                       return std::isfinite(channel) && channel >= 0.0 && channel <= 1.0;
                   });
        };
        if (!std::isfinite(star_density) || star_density < 0.0 ||
            star_density > 0.25 || !std::isfinite(star_scale) ||
            star_scale < 1.0 || star_scale > 10000.0 ||
            !std::isfinite(star_brightness) || star_brightness < 0.0 ||
            star_brightness > 20.0 || !std::isfinite(ambient_light) ||
            ambient_light < 0.0 || ambient_light > 1.0 ||
            !validColor(background_color) || !validColor(star_color)) {
            throw std::invalid_argument("Invalid skybox parameters");
        }
    }
};

struct OrbitViewConfig {
    std::array<double, 3> position{12.0, 0.0, 0.5};
    std::array<double, 3> target{0.0, 0.0, 0.0};
    double fov = 60.0;

    OrbitViewConfig() = default;
    explicit OrbitViewConfig(const config::Config& cfg);
};

struct PlanetConfig {
    struct SurfaceNoiseFunction {
        std::string type = "value_fbm";
        double amplitude_m = 0.0;
        double frequency = 4.0;
        double wavelength_m = 0.0; // Zero selects legacy planet-relative frequency.
        int octaves = 4;
        double persistence = 0.5;
        double lacunarity = 2.0;
        int seed = 42;

        explicit SurfaceNoiseFunction(const config::Config& cfg, int defaultSeed = 42);
        SurfaceNoiseFunction() = default;

        void validate() const {
            if ((type != "value_fbm" && type != "ridged_fbm") ||
                !std::isfinite(amplitude_m) || amplitude_m < 0.0 ||
                !std::isfinite(frequency) || frequency <= 0.0 || frequency > 64.0 ||
                !std::isfinite(wavelength_m) || wavelength_m < 0 ||
                (wavelength_m > 0 && wavelength_m < .01) || wavelength_m > 1e9 ||
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
        double sink_depth_m = 1.0;
        bool relief_sinking = false; // Opt-in preserves historical replay geometry.
        double geometric_error_m = 0.0; // Zero retains the legacy face-only planner.
        double local_detail_radius_m = 0.0; // Opt-in ground-centered centimeter patch.
        double local_edge_m = .05, local_error_m = .01, local_transition_m = 3.0;
        double rebuildDistanceMeters() const {
            return local_detail_radius_m > 0 ? std::min(10.0,.3*local_detail_radius_m) : 10.0;
        }
        int max_triangle_budget = 60000;
        double shoreline_edge_m = 1.0;
        double shoreline_distance_m = 80.0;
        double lod_near_diameters = 2.0;
        double lod_far_diameters = 8.0;

        explicit TerrainLod(const config::Config& cfg);
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
                !std::isfinite(sink_depth_m) || sink_depth_m < 0.0 || sink_depth_m > 100.0 ||
                !std::isfinite(geometric_error_m) || geometric_error_m < 0.0 || geometric_error_m > 100.0 ||
                (geometric_error_m > 0.0 && !relief_sinking) ||
                !std::isfinite(local_detail_radius_m) || local_detail_radius_m < 0 || local_detail_radius_m > 10 ||
                (local_detail_radius_m > 0 && (!relief_sinking || geometric_error_m <= 0 || local_detail_radius_m >= near_surface_distance_m)) ||
                !std::isfinite(local_edge_m) || local_edge_m < .005 || local_edge_m > .5 ||
                !std::isfinite(local_error_m) || local_error_m < .001 || local_error_m > 1 ||
                !std::isfinite(local_transition_m) || local_transition_m < .1 || local_transition_m > 30 ||
                max_triangle_budget < 10000 || max_triangle_budget > 100000 ||
                !std::isfinite(shoreline_edge_m) || shoreline_edge_m < 0.0 || shoreline_edge_m > 100.0 ||
                (shoreline_edge_m > 0.0 && shoreline_edge_m < 0.1) ||
                !std::isfinite(shoreline_distance_m) || shoreline_distance_m < 1.0 || shoreline_distance_m > 1000.0 ||
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
        explicit TerrainLandscape(const config::Config& cfg);
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

    struct TerrainMaterial {
        // Angles from the local horizontal, independent of mesh LOD thresholds.
        double rock_start_degrees = 35.0;
        double rock_end_degrees = 55.0;

        TerrainMaterial() = default;
        explicit TerrainMaterial(const config::Config& cfg);
        void validate() const {
            if (!std::isfinite(rock_start_degrees) || !std::isfinite(rock_end_degrees) ||
                rock_start_degrees < 0.0 || rock_end_degrees > 90.0 ||
                rock_end_degrees - rock_start_degrees < 0.1)
                throw std::invalid_argument("planet.terrain_material needs 0 <= rock_start_degrees < rock_end_degrees <= 90, at least 0.1 degrees apart");
        }
        std::array<double, 2> slopeMetricRange() const {
            const double radiansPerDegree = std::acos(-1.0) / 180.0;
            return {1.0 - std::cos(rock_start_degrees * radiansPerDegree),
                    1.0 - std::cos(rock_end_degrees * radiansPerDegree)};
        }
    };

    struct Water {
        bool enabled = false;
        double level_m = 0.0;
        double opacity = 0.5;
        double reflection_fraction = 0.5;
        std::vector<double> color = {0.05, 0.35, 0.6};

        Water() = default;
        explicit Water(const config::Config& cfg);
        void validate() const {
            if (!std::isfinite(level_m) || !std::isfinite(opacity) || opacity < 0.0 || opacity > 1.0 ||
                !std::isfinite(reflection_fraction) || reflection_fraction < 0.0 || reflection_fraction > 1.0 ||
                color.size() != 3 ||
                !std::all_of(color.begin(), color.end(), [](double c) { return std::isfinite(c) && c >= 0.0 && c <= 1.0; }))
                throw std::invalid_argument("Invalid planet.water parameters");
        }
    };

    std::vector<double> position = {0.0, 0.0, 0.0};
    std::string name;
    double mass_kg = 1.0e16;
    OrbitConfig orbit;
    RotationConfig rotation;
    ReflectionConfig reflection;
    double orbit_radius = 5.0;
    double orbit_speed = 0.02;
    double radius = 1.5;
    std::vector<double> color = {0.3, 0.6, 0.9};
    int noise_seed = 42;
    std::vector<SurfaceNoiseFunction> surface_noise;
    TerrainLod terrain_lod;
    TerrainLandscape terrain_landscape;
    TerrainMaterial terrain_material;
    FoliageConfig foliage;
    Water water;
    AtmosphereConfig atmosphere;
    bool atmosphere_enabled = false;
    double atmosphere_height = 0.3;
    
    PlanetConfig() = default;
    PlanetConfig(const config::Config& cfg);
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
    std::optional<double> simulation_time_seconds;

    SurfaceCameraConfig() = default;
    explicit SurfaceCameraConfig(const config::Config& cfg);
};

struct ScenarioConfig {
    std::string name = "Unnamed";
    std::string distance_unit = "km";
    SunConfig sun;
    SkyboxConfig skybox;
    LightingConfig lighting;
    OrbitViewConfig camera;
    std::vector<PlanetConfig> planets;
    SurfaceCameraConfig surface_camera;
    
    ScenarioConfig() = default;
    double metersPerWorldUnit() const { return distance_unit == "km" ? 1000.0 : 1.0; }

    // Index zero is the Sun; subsequent indices match planets[index - 1].
    // Validate the complete graph, allowing parents to appear after children.
    std::vector<std::size_t> orbitalParents() const {
        validateMass(sun.mass_kg);
        if (sun.name.empty()) throw std::invalid_argument("Sun name must not be empty");
        std::unordered_map<std::string, std::size_t> names{{sun.name, 0}};
        for (std::size_t i = 0; i < planets.size(); ++i) {
            const auto& planet = planets[i];
            validateMass(planet.mass_kg);
            planet.orbit.validate();
            planet.rotation.validate();
            if (planet.name.empty() || !names.emplace(planet.name, i + 1).second)
                throw std::invalid_argument("Celestial body names must be nonempty and unique");
        }
        std::vector<std::size_t> parents(planets.size() + 1, 0);
        std::vector<std::vector<std::size_t>> children(parents.size());
        for (std::size_t i = 1; i < parents.size(); ++i) {
            const auto parent = names.find(planets[i - 1].orbit.parent);
            if (parent == names.end() || parent->second == i)
                throw std::invalid_argument("Unknown or self-referencing orbit parent for " + planets[i - 1].name);
            parents[i] = parent->second;
            children[parents[i]].push_back(i);
        }
        std::vector<std::size_t> reached{0};
        for (std::size_t i = 0; i < reached.size(); ++i)
            for (auto child : children[reached[i]]) reached.push_back(child);
        if (reached.size() != parents.size())
            throw std::invalid_argument("Orbit parents contain a cycle disconnected from the Sun");
        return parents;
    }
    ScenarioConfig(const config::Config& cfg);
};

} // namespace config

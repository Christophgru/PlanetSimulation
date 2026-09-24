#pragma once

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>
#include "config/Config.h"

namespace config {

struct TerrainShadowConfig {
    bool enabled = true;
    int resolution = 2048;
    double bias_texels = 0.5;

    TerrainShadowConfig() = default;
    explicit TerrainShadowConfig(const Config& cfg) {
        if (!cfg.data().is_object())
            throw std::invalid_argument("lighting.shadows must be an object");
        if (cfg.data().contains("enabled") && !cfg.data()["enabled"].is_boolean())
            throw std::invalid_argument("lighting.shadows.enabled must be a boolean");
        if (cfg.data().contains("resolution") && !cfg.data()["resolution"].is_number_integer())
            throw std::invalid_argument("lighting.shadows.resolution must be an integer");
        enabled = cfg.getBool("enabled", enabled);
        const double requestedResolution = cfg.getDouble("resolution", resolution);
        if (requestedResolution < 256 || requestedResolution > 4096)
            throw std::invalid_argument("Shadow resolution must be between 256 and 4096");
        resolution = static_cast<int>(requestedResolution);
        bias_texels = cfg.getDouble("bias_texels", bias_texels);
        validate();
    }

    void validate() const {
        if (resolution < 256 || resolution > 4096 || (resolution & (resolution - 1)) != 0 ||
            !std::isfinite(bias_texels) || bias_texels < 0.0 || bias_texels > 4.0)
            throw std::invalid_argument("Shadow resolution must be a power of two in 256..4096; bias_texels must be in 0..4");
    }
};

inline void validateAbsoluteMagnitude(double magnitude) {
    if (!std::isfinite(magnitude) || magnitude < -30.0 || magnitude > 30.0)
        throw std::invalid_argument("sun.absolute_magnitude must be finite and between -30 and 30");
}

struct LightingConfig {
    double ambient_light = 0.12;
    // Display normalization: one nominal solar luminosity has unit irradiance
    // at this distance, expressed in the scenario's distance_unit.
    double reference_distance = 10.0;
    double exposure = 1.0;
    bool reflections_enabled = true;
    TerrainShadowConfig shadows;

    LightingConfig() = default;
    explicit LightingConfig(const Config& cfg, double legacyAmbient = 0.12)
        : ambient_light(legacyAmbient) {
        if (!cfg.data().is_object()) throw std::invalid_argument("lighting must be an object");
        ambient_light = cfg.getDouble("ambient_light", ambient_light);
        reference_distance = cfg.getDouble("reference_distance", reference_distance);
        exposure = cfg.getDouble("exposure", exposure);
        if (cfg.data().contains("reflections_enabled") && !cfg.data()["reflections_enabled"].is_boolean())
            throw std::invalid_argument("lighting.reflections_enabled must be a boolean");
        reflections_enabled = cfg.getBool("reflections_enabled", reflections_enabled);
        if (cfg.data().contains("shadows"))
            shadows = TerrainShadowConfig(Config(nlohmann::json(cfg.data()["shadows"])));
        validate();
    }

    void validate() const {
        shadows.validate();
        if (!std::isfinite(ambient_light) || ambient_light < 0.0 || ambient_light > 1.0 ||
            !std::isfinite(reference_distance) || reference_distance <= 0.0 ||
            !std::isfinite(exposure) || exposure <= 0.0 || exposure > 100.0)
            throw std::invalid_argument("Invalid ambient light, reference distance or exposure");
    }
};

struct ReflectionConfig {
    double geometric_albedo = 0.12;
    // RGB fraction of the reflected light; independent of terrain's local tint.
    std::vector<double> color{1.0, 1.0, 1.0};

    ReflectionConfig() = default;
    explicit ReflectionConfig(const Config& cfg) {
        if (!cfg.data().is_object()) throw std::invalid_argument("planet.reflection must be an object");
        geometric_albedo = cfg.getDouble("geometric_albedo", geometric_albedo);
        if (cfg.data().contains("color") && !cfg.data()["color"].is_array())
            throw std::invalid_argument("planet.reflection.color must be an RGB array");
        color = cfg.getArray("color", color);
        validate();
    }

    void validate() const {
        if (!std::isfinite(geometric_albedo) || geometric_albedo < 0.0 || geometric_albedo > 1.0 ||
            color.size() != 3 || !std::all_of(color.begin(), color.end(), [](double channel) {
                return std::isfinite(channel) && channel >= 0.0 && channel <= 1.0;
            }))
            throw std::invalid_argument("Invalid geometric albedo or reflection color");
    }
};

} // namespace config

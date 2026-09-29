#pragma once

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace config {
class Config;

struct TerrainShadowConfig {
    bool enabled = true;
    int resolution = 2048;
    double bias_texels = 0.5;

    TerrainShadowConfig() = default;
    explicit TerrainShadowConfig(const Config& cfg);

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

struct AutoExposureConfig {
    bool enabled = false; // Legacy scenarios retain their fixed exposure.
    double min_exposure = 0.02;
    double max_exposure = 4096.0;
    double target_luminance = 0.12;
    double star_exposure = 64.0;

    AutoExposureConfig() = default;
    explicit AutoExposureConfig(const Config& cfg);
    void validate() const {
        if (!std::isfinite(min_exposure) || min_exposure <= 0.0 ||
            !std::isfinite(max_exposure) || max_exposure < min_exposure || max_exposure > 1e6 ||
            !std::isfinite(target_luminance) || target_luminance <= 0.0 || target_luminance > 1.0 ||
            !std::isfinite(star_exposure) || star_exposure <= 0.0 || star_exposure > 1e6)
            throw std::invalid_argument("Invalid automatic exposure limits, target luminance or star exposure");
    }
};

struct LightingConfig {
    double ambient_light = 0.12;
    // Display normalization: one nominal solar luminosity has unit irradiance
    // at this distance, expressed in the scenario's distance_unit.
    double reference_distance = 10.0;
    double exposure = 1.0;
    bool reflections_enabled = true;
    TerrainShadowConfig shadows;
    AutoExposureConfig auto_exposure;

    LightingConfig() = default;
    explicit LightingConfig(const Config& cfg, double legacyAmbient = 0.12);

    void validate() const {
        shadows.validate();
        auto_exposure.validate();
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
    explicit ReflectionConfig(const Config& cfg);

    void validate() const {
        if (!std::isfinite(geometric_albedo) || geometric_albedo < 0.0 || geometric_albedo > 1.0 ||
            color.size() != 3 || !std::all_of(color.begin(), color.end(), [](double channel) {
                return std::isfinite(channel) && channel >= 0.0 && channel <= 1.0;
            }))
            throw std::invalid_argument("Invalid geometric albedo or reflection color");
    }
};

} // namespace config

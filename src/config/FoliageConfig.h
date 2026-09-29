#pragma once
#include <cmath>
#include <stdexcept>
#include "config/Config.h"

namespace config {
struct FoliageConfig {
    bool enabled = false;
    double density_per_m2 = 30.72;
    double height_m = 1.5;
    double width_m = 0.1;
    double draw_distance_m = 40.0;
    double wind_strength = 1.0;
    int max_blades = 100000;
    int seed = 7321;

    FoliageConfig() = default;
    explicit FoliageConfig(const Config& cfg) {
        enabled = cfg.getBool("enabled", true);
        density_per_m2 = cfg.getDouble("density_per_m2", density_per_m2);
        height_m = cfg.getDouble("height_m", height_m);
        width_m = cfg.getDouble("width_m", width_m);
        draw_distance_m = cfg.getDouble("draw_distance_m", draw_distance_m);
        wind_strength = cfg.getDouble("wind_strength", wind_strength);
        max_blades = cfg.getInt("max_blades", max_blades);
        seed = cfg.getInt("seed", seed);
        validate();
    }
    void validate() const {
        if (!std::isfinite(density_per_m2) || density_per_m2 <= 0 || density_per_m2 > 64 ||
            !std::isfinite(height_m) || height_m < 0.05 || height_m > 3 ||
            !std::isfinite(width_m) || width_m < 0.005 || width_m > 0.3 ||
            !std::isfinite(draw_distance_m) || draw_distance_m < 5 || draw_distance_m > 100 ||
            !std::isfinite(wind_strength) || wind_strength < 0 || wind_strength > 2 ||
            max_blades < 1 || max_blades > 250000)
            throw std::invalid_argument("Invalid planet.foliage parameters");
    }
};
} // namespace config

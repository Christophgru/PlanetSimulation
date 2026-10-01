#pragma once
#include <cmath>
#include <stdexcept>

namespace config {
class Config;
struct FoliageConfig {
    bool enabled = false;
    bool near_enabled = true;
    double density_per_m2 = 30.72;
    double height_m = 1.5;
    double width_m = 0.1;
    double draw_distance_m = 40.0;
    double wind_strength = 1.0;
    int max_blades = 100000;
    int seed = 7321;
    double rebuild_distance_fraction = 0.15;
    double gaussian_sigma_fraction = 1.0 / 3.0;
    double budget_fraction = 0.8;
    int max_candidates_per_triangle = 8192;
    double green_ratio = 1.15;
    double water_clearance_m = 0.15;
    double root_offset_m = 0.005;
    double height_multiplier_min = 0.75;
    double height_multiplier_max = 1.5;
    double lean_min = 0.1;
    double lean_max = 0.4;
    bool horizon_enabled = true;
    double far_distance_m = 0.0; // Zero derives a conservative terrain horizon.
    double far_density_per_m2 = 0.35;
    int far_max_instances = 80000;
    double far_rebuild_distance_m = 10.0;
    int far_max_candidates_per_patch = 128;
    double far_height_scale = 1.0;
    double far_width_scale = 6.0;
    double far_min_width_m = 0.25;
    double far_fade_in_start_fraction = 0.5;
    double far_fade_in_end_fraction = 0.75;
    double far_fade_out_start_fraction = 0.85;

    FoliageConfig() = default;
    explicit FoliageConfig(const Config& cfg);
    void validate() const {
        const auto outside=[](double value,double low,double high) {
            return !std::isfinite(value) || value<low || value>high;
        };
        if (!std::isfinite(density_per_m2) || density_per_m2 <= 0 || density_per_m2 > 4096 ||
            !std::isfinite(height_m) || height_m < 0.05 || height_m > 3 ||
            !std::isfinite(width_m) || width_m < 0.005 || width_m > 0.3 ||
            !std::isfinite(draw_distance_m) || draw_distance_m < 5 || draw_distance_m > 400 ||
            !std::isfinite(wind_strength) || wind_strength < 0 || wind_strength > 2 ||
            max_blades < 1 || max_blades > 250000 ||
            !std::isfinite(far_distance_m) || far_distance_m < 0 || far_distance_m > 20000 ||
            (far_distance_m > 0 && far_distance_m <= draw_distance_m) ||
            !std::isfinite(far_density_per_m2) || far_density_per_m2 <= 0 || far_density_per_m2 > 4 ||
            far_max_instances < 1 || far_max_instances > 250000 ||
            outside(rebuild_distance_fraction,0.01,1.0) ||
            outside(gaussian_sigma_fraction,0.05,1.0) || outside(budget_fraction,0.01,1.0) ||
            max_candidates_per_triangle<1 || max_candidates_per_triangle>65536 ||
            outside(green_ratio,1.0,4.0) || outside(water_clearance_m,0,10) ||
            outside(root_offset_m,0,0.1) ||
            outside(height_multiplier_min,0.1,3.0) || outside(height_multiplier_max,height_multiplier_min,3.0) ||
            outside(lean_min,0,1) || outside(lean_max,lean_min,1) ||
            outside(far_rebuild_distance_m,0.1,1000) ||
            far_max_candidates_per_patch<1 || far_max_candidates_per_patch>128 ||
            (far_max_candidates_per_patch & (far_max_candidates_per_patch-1))!=0 ||
            outside(far_height_scale,0.1,4) || outside(far_width_scale,0.1,32) ||
            outside(far_min_width_m,0.005,3) ||
            outside(far_fade_in_start_fraction,0,0.99) ||
            outside(far_fade_in_end_fraction,0.01,1) ||
            far_fade_in_start_fraction>=far_fade_in_end_fraction ||
            outside(far_fade_out_start_fraction,0,0.99))
            throw std::invalid_argument("Invalid planet.foliage parameters");
    }
};
} // namespace config

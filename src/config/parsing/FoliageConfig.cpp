#include "config/FoliageConfig.h"
#include "config/Config.h"

namespace config {

FoliageConfig::FoliageConfig(const Config& cfg) {
    enabled = cfg.getBool("enabled", true);
    near_enabled = cfg.getBool("near_enabled", near_enabled);
    density_per_m2 = cfg.getDouble("density_per_m2", density_per_m2);
    height_m = cfg.getDouble("height_m", height_m);
    width_m = cfg.getDouble("width_m", width_m);
    draw_distance_m = cfg.getDouble("draw_distance_m", draw_distance_m);
    wind_strength = cfg.getDouble("wind_strength", wind_strength);
    max_blades = cfg.getInt("max_blades", max_blades);
    seed = cfg.getInt("seed", seed);
    rebuild_distance_fraction = cfg.getDouble("rebuild_distance_fraction", rebuild_distance_fraction);
    gaussian_sigma_fraction = cfg.getDouble("gaussian_sigma_fraction", gaussian_sigma_fraction);
    budget_fraction = cfg.getDouble("budget_fraction", budget_fraction);
    max_candidates_per_triangle = cfg.getInt("max_candidates_per_triangle", max_candidates_per_triangle);
    green_ratio = cfg.getDouble("green_ratio", green_ratio);
    water_clearance_m = cfg.getDouble("water_clearance_m", water_clearance_m);
    root_offset_m = cfg.getDouble("root_offset_m", root_offset_m);
    height_multiplier_min = cfg.getDouble("height_multiplier_min", height_multiplier_min);
    height_multiplier_max = cfg.getDouble("height_multiplier_max", height_multiplier_max);
    lean_min = cfg.getDouble("lean_min", lean_min);
    lean_max = cfg.getDouble("lean_max", lean_max);
    horizon_enabled = cfg.getBool("horizon_enabled", horizon_enabled);
    far_distance_m = cfg.getDouble("far_distance_m", far_distance_m);
    far_density_per_m2 = cfg.getDouble("far_density_per_m2", far_density_per_m2);
    far_max_instances = cfg.getInt("far_max_instances", far_max_instances);
    far_rebuild_distance_m = cfg.getDouble("far_rebuild_distance_m", far_rebuild_distance_m);
    far_max_candidates_per_patch = cfg.getInt("far_max_candidates_per_patch", far_max_candidates_per_patch);
    far_height_scale = cfg.getDouble("far_height_scale", far_height_scale);
    far_width_scale = cfg.getDouble("far_width_scale", far_width_scale);
    far_min_width_m = cfg.getDouble("far_min_width_m", far_min_width_m);
    far_fade_in_start_fraction = cfg.getDouble("far_fade_in_start_fraction", far_fade_in_start_fraction);
    far_fade_in_end_fraction = cfg.getDouble("far_fade_in_end_fraction", far_fade_in_end_fraction);
    far_fade_out_start_fraction = cfg.getDouble("far_fade_out_start_fraction", far_fade_out_start_fraction);
    validate();
}
} // namespace config

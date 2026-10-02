#include "config/FoliageConfig.h"
#include "config/Config.h"

namespace config {

FoliageConfig::FoliageConfig(const Config& cfg) {
    enabled = cfg.getBool("enabled", true);
    frustum_culling = cfg.getBool("frustum_culling", frustum_culling);
    compute_placement = cfg.getBool("compute_placement", compute_placement);
    density_per_m2 = cfg.getDouble("density_per_m2", density_per_m2);
    height_m = cfg.getDouble("height_m", height_m);
    width_m = cfg.getDouble("width_m", width_m);
    draw_distance_m = cfg.getDouble("draw_distance_m", draw_distance_m);
    quad_distance_m = cfg.getDouble("quad_distance_m", quad_distance_m);
    wind_strength = cfg.getDouble("wind_strength", wind_strength);
    if (cfg.data().contains("wind_noise")) {
        if (!cfg.data().at("wind_noise").is_object())
            throw std::invalid_argument("planet.foliage.wind_noise must be an object");
        const Config noise{nlohmann::json(cfg.data().at("wind_noise"))};
        wind_noise.gust_frequency=noise.getDouble("gust_frequency",wind_noise.gust_frequency);
        wind_noise.direction_frequency=noise.getDouble("direction_frequency",wind_noise.direction_frequency);
        wind_noise.flutter_frequency=noise.getDouble("flutter_frequency",wind_noise.flutter_frequency);
        wind_noise.speed_multiplier=noise.getDouble("speed_multiplier",wind_noise.speed_multiplier);
        wind_noise.seed=noise.getInt("seed",wind_noise.seed);
    }
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
    validate();
}
} // namespace config

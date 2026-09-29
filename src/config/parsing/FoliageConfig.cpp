#include "config/FoliageConfig.h"
#include "config/Config.h"

namespace config {

FoliageConfig::FoliageConfig(const Config& cfg) {
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
} // namespace config

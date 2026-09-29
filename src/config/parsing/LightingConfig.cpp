#include "config/LightingConfig.h"
#include "config/Config.h"

namespace config {

TerrainShadowConfig::TerrainShadowConfig(const Config& cfg) {
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

AutoExposureConfig::AutoExposureConfig(const Config& cfg) {
    if (!cfg.data().is_object()) throw std::invalid_argument("lighting.auto_exposure must be an object");
    if (cfg.data().contains("enabled") && !cfg.data()["enabled"].is_boolean())
        throw std::invalid_argument("lighting.auto_exposure.enabled must be a boolean");
    enabled = cfg.getBool("enabled", enabled);
    min_exposure = cfg.getDouble("min_exposure", min_exposure);
    max_exposure = cfg.getDouble("max_exposure", max_exposure);
    target_luminance = cfg.getDouble("target_luminance", target_luminance);
    star_exposure = cfg.getDouble("star_exposure", star_exposure);
    validate();
}

LightingConfig::LightingConfig(const Config& cfg, double legacyAmbient)
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
    if (cfg.data().contains("auto_exposure"))
        auto_exposure = AutoExposureConfig(Config(nlohmann::json(cfg.data()["auto_exposure"])));
    validate();
}

ReflectionConfig::ReflectionConfig(const Config& cfg) {
    if (!cfg.data().is_object()) throw std::invalid_argument("planet.reflection must be an object");
    geometric_albedo = cfg.getDouble("geometric_albedo", geometric_albedo);
    if (cfg.data().contains("color") && !cfg.data()["color"].is_array())
        throw std::invalid_argument("planet.reflection.color must be an RGB array");
    color = cfg.getArray("color", color);
    validate();
}
} // namespace config

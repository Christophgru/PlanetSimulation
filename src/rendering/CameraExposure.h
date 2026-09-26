#pragma once

#include <optional>
#include "rendering/CelestialLighting.h"

namespace rendering {

struct CameraExposure {
    double exposure = 1.0;
    double skySensitivity = 1.0;
    double directIlluminance = 0.0;
    double reflectedIlluminance = 0.0;
    double atmosphericIlluminance = 0.0;
    double meteredIlluminance = 0.0;
};

inline double luminance(const glm::dvec3& linearRgb) {
    return glm::dot(linearRgb, glm::dvec3(0.2126, 0.7152, 0.0722));
}

// A deterministic incident-light meter at the camera's radial position. It
// meters an 18% gray surface, so looking at a dark material cannot brighten
// the entire world. Night-side sunlight is excluded by the local horizon.
// Local terrain occlusion is left to the shadow maps, not this inexpensive meter.
inline CameraExposure cameraExposure(const config::LightingConfig& settings,
                                     const FrameLighting& lighting,
                                     const std::vector<simulation::BodyState>& bodies,
                                     const glm::dvec3& eyeWorld,
                                     std::optional<std::size_t> planetIndex = std::nullopt,
                                     double atmosphericIlluminance = 0.0) {
    settings.validate();
    if (!std::isfinite(atmosphericIlluminance) || atmosphericIlluminance < 0)
        throw std::invalid_argument("Atmospheric illuminance must be finite and nonnegative");
    CameraExposure result;
    result.exposure = settings.exposure;
    if (!planetIndex) return result; // Whole-system camera keeps manual exposure.
    if (*planetIndex >= lighting.planets.size() || *planetIndex + 1 >= bodies.size())
        throw std::invalid_argument("Exposure meter needs a configured planet");
    const auto radial = eyeWorld - bodies[*planetIndex + 1].position;
    const double distance = glm::length(radial);
    if (!std::isfinite(distance) || distance <= 0.0)
        throw std::invalid_argument("Exposure meter needs a finite camera position outside the center");
    const auto& light = lighting.planets[*planetIndex];
    result.directIlluminance = luminance(light.sunlight) *
        std::max(0.0, glm::dot(radial / distance, light.sunDirection));
    result.reflectedIlluminance = luminance(light.reflectedLight);
    result.atmosphericIlluminance = atmosphericIlluminance;
    result.meteredIlluminance = result.directIlluminance + result.reflectedIlluminance +
        result.atmosphericIlluminance + settings.ambient_light;
    if (!std::isfinite(result.meteredIlluminance) || result.meteredIlluminance < 0.0)
        throw std::invalid_argument("Exposure meter illumination must be finite and nonnegative");
    if (!settings.auto_exposure.enabled) return result;
    const auto& automatic = settings.auto_exposure;
    const double requested = settings.exposure * automatic.target_luminance /
                             (0.18 * std::max(result.meteredIlluminance, 1e-12));
    result.exposure = std::clamp(requested, automatic.min_exposure, automatic.max_exposure);
    result.skySensitivity = std::min(1.0, result.exposure / automatic.star_exposure);
    return result;
}

} // namespace rendering

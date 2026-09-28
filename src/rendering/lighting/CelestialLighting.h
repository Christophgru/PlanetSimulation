#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <vector>
#include <glm/glm.hpp>
#include "config/ScenarioConfig.h"
#include "simulation/OrbitalSystem.h"

namespace rendering {

// IAU 2015 Resolution B2: L = L0 * 10^(-0.4 M_bol).
inline constexpr double magnitudeZeroLuminosityWatts = 3.0128e28;
inline constexpr double nominalSolarLuminosityWatts = 3.828e26;

inline double luminosityWatts(double absoluteMagnitude) {
    config::validateAbsoluteMagnitude(absoluteMagnitude);
    return magnitudeZeroLuminosityWatts * std::pow(10.0, -0.4 * absoluteMagnitude);
}

inline glm::dvec3 rgb(const std::vector<double>& color) {
    return {color.at(0), color.at(1), color.at(2)};
}

// Diffuse sphere phase law: full = 1, quarter = 1/pi, new = 0.
// alpha is the Sun-reflector-receiver angle (not the orbital phase angle).
inline double lambertPhase(double cosinePhaseAngle) {
    if (!std::isfinite(cosinePhaseAngle))
        throw std::invalid_argument("Reflection phase must be finite");
    const double cosine = std::clamp(cosinePhaseAngle, -1.0, 1.0);
    if (cosine == -1.0) return 0.0;
    if (cosine == 1.0) return 1.0;
    const double angle = std::acos(cosine);
    return std::clamp((std::sin(angle) + (std::numbers::pi - angle) * cosine) /
                      std::numbers::pi, 0.0, 1.0);
}

// Display transform shared with the terrain/water shaders. All light is summed
// in linear RGB before exposure, an exponential shoulder, and sRGB encoding.
inline glm::dvec3 displayColor(const glm::dvec3& radiance, double exposure) {
    if (!std::isfinite(exposure) || exposure <= 0.0)
        throw std::invalid_argument("Exposure must be finite and positive");
    glm::dvec3 result;
    for (int channel = 0; channel < 3; ++channel) {
        if (!std::isfinite(radiance[channel]) || radiance[channel] < 0.0)
            throw std::invalid_argument("Radiance must be finite and nonnegative");
        const double mapped = -std::expm1(-radiance[channel] * exposure);
        result[channel] = mapped <= 0.0031308 ? 12.92 * mapped :
            1.055 * std::pow(mapped, 1.0 / 2.4) - 0.055;
    }
    return result;
}

struct BodyLighting {
    glm::dvec3 sunDirection{0.0};
    glm::dvec3 sunlight{0.0};
    glm::dvec3 reflectedLight{0.0}; // Averaged over the receiving sphere.
};

struct FrameLighting {
    glm::dvec3 sunEmission{0.0};
    std::vector<BodyLighting> planets;
};

// CPU-only, once per frame; reuse the result in the main view and every water
// reflection pass. No phase, distance, or inter-body loops run per fragment.
inline FrameLighting calculateLighting(const config::ScenarioConfig& scenario,
                                       const std::vector<simulation::BodyState>& bodies) {
    if (bodies.size() != scenario.planets.size() + 1)
        throw std::invalid_argument("Lighting requires one state per configured body");
    scenario.lighting.validate();
    FrameLighting result;
    result.sunEmission = rgb(scenario.sun.color) *
        (luminosityWatts(scenario.sun.absolute_magnitude) / nominalSolarLuminosityWatts);
    result.planets.resize(scenario.planets.size());
    for (std::size_t i = 0; i < result.planets.size(); ++i) {
        scenario.planets[i].reflection.validate();
        const glm::dvec3 offset = bodies[0].position - bodies[i + 1].position;
        const double distance = glm::length(offset);
        if (!std::isfinite(distance)) throw std::invalid_argument("Lighting positions must be finite");
        auto& light = result.planets[i];
        if (distance > 0.0) light.sunDirection = offset / distance;
        // A body inside the solar sphere uses surface flux rather than a
        // singular point-source flux. This also keeps overlapping configs safe.
        const double ratio = scenario.lighting.reference_distance /
                             std::max(distance, scenario.sun.radius);
        light.sunlight = result.sunEmission * ratio * ratio;
        if (!std::isfinite(glm::length(light.sunlight)))
            throw std::invalid_argument("Lighting exceeds numerical range");
    }
    if (!scenario.lighting.reflections_enabled) return result;
    for (std::size_t receiver = 0; receiver < result.planets.size(); ++receiver) {
        for (std::size_t reflector = 0; reflector < result.planets.size(); ++reflector) {
            if (receiver == reflector) continue;
            const auto& source = scenario.planets[reflector];
            if (source.reflection.geometric_albedo == 0.0) continue;
            const glm::dvec3 towardReceiver = bodies[receiver + 1].position - bodies[reflector + 1].position;
            const double distance = glm::length(towardReceiver);
            if (distance <= 0.0) continue;
            const double phase = lambertPhase(glm::dot(
                result.planets[reflector].sunDirection, towardReceiver / distance));
            const double ratio = source.radius / std::max(source.radius, distance);
            // Geometric-albedo flux ratio, with inverse-square falloff on
            // both light paths. Divide by four to spread intercepted flux
            // (cross section pi R²) over the whole receiving sphere (4 pi R²).
            const double fraction = 0.25 * source.reflection.geometric_albedo * phase * ratio * ratio;
            result.planets[receiver].reflectedLight +=
                result.planets[reflector].sunlight * rgb(source.reflection.color) * fraction;
        }
    }
    return result;
}

} // namespace rendering

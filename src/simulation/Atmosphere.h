#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>
#include <glm/glm.hpp>
#include "config/AtmosphereConfig.h"

namespace simulation {

// A weather/climate solver can supply this state at a planet-local location.
// Rendering consumes optical properties; it does not evolve climate or water.
struct LocalAirState {
    double pressurePa = 101325.0;
    double temperatureK = 293.15;
    double waterPercent = 0.0;
    double dustPercent = 0.0;
    std::optional<double> suspendedLiquidWaterGm3; // Optional weather-solver override.
};
inline LocalAirState referenceAir(const config::AtmosphereConfig& config) {
    return {config.surface_pressure_pa, config.temperature_k, config.water, config.red_dust};
}
struct AtmosphereOptics {
    glm::dvec3 rayleigh{0.0}, aerosolScattering{0.0}, aerosolAbsorption{0.0}; // Per planet radius.
    double molecularScaleHeight = 0.025, aerosolScaleHeight = 0.02; // In planet radii.
    double refractiveIndex = 1.0;
    double relativeHumidity = 0.0;
    double suspendedLiquidWaterGm3 = 0.0;
};

// Magnus approximation over water. This is a rendering/condensation estimate,
// not a laboratory equation of state or an ice microphysics model.
inline double saturationPressurePa(double temperatureK) {
    if (!std::isfinite(temperatureK) || temperatureK < 180 || temperatureK > 373.15)
        throw std::invalid_argument("Atmosphere temperature out of range");
    const double celsius = temperatureK - 273.15;
    return 610.94 * std::exp(17.625 * celsius / (celsius + 243.04));
}

inline AtmosphereOptics atmosphereOptics(const config::AtmosphereConfig& config,
                                         double radiusMeters, const LocalAirState& air) {
    config.validate();
    if (!std::isfinite(radiusMeters) || radiusMeters <= 0 ||
        !std::isfinite(air.pressurePa) || air.pressurePa < 0 || air.pressurePa > 1e7 ||
        !std::isfinite(air.waterPercent) || air.waterPercent < 0 || air.waterPercent > 100 ||
        !std::isfinite(air.dustPercent) || air.dustPercent < 0 || air.dustPercent > 100 ||
        (air.suspendedLiquidWaterGm3 && (!std::isfinite(*air.suspendedLiquidWaterGm3) ||
            *air.suspendedLiquidWaterGm3 < 0 || *air.suspendedLiquidWaterGm3 > 1000)))
        throw std::invalid_argument("Invalid local atmosphere state");
    const double saturated = saturationPressurePa(air.temperatureK);
    AtmosphereOptics result;
    const double thickness = config.radius_multiplier - 1.0;
    result.molecularScaleHeight = 0.25 * thickness;
    result.aerosolScaleHeight = 0.2 * thickness;
    if (!config.enabled || air.pressurePa == 0) return result;
    const double waterPressure = air.pressurePa * air.waterPercent / 100.0;
    result.relativeHumidity = waterPressure / saturated;
    const double vaporFraction = std::min(waterPressure, saturated) / air.pressurePa;
    const double dryTotal = config.nitrogen + config.oxygen + config.carbon_dioxide + config.argon + config.balancePercent();
    // Approximate visible-band gas refractivities at reference density. Keep
    // composition and density separate: scattering scales linearly with P/T.
    const double dryRefractivity = dryTotal > 0 ?
        (config.nitrogen * 2.98e-4 + config.oxygen * 2.71e-4 + config.carbon_dioxide * 4.49e-4 +
         (config.argon + config.balancePercent()) * 2.81e-4) / dryTotal : 0.0;
    const double referenceRefractivity = (1.0 - air.waterPercent / 100.0) * dryRefractivity + vaporFraction * 2.57e-4;
    const double density = (air.pressurePa / 101325.0) * (293.15 / air.temperatureK);
    result.refractiveIndex = 1.0 + referenceRefractivity * density;
    const double composition = std::pow(referenceRefractivity / 2.92e-4, 2);
    // Preserve an Earth-like molecular optical column in this miniature world.
    // 8 km reference column; RGB samples approximately 680, 550 and 440 nm.
    result.rayleigh = glm::dvec3(5.8e-6, 13.5e-6, 33.1e-6) *
        (8000.0 / result.molecularScaleHeight) * density * composition;
    // Red mineral aerosol is an explicit artistic optical approximation: its
    // short-wave absorption warms transmitted light, with dense loads opaque.
    const glm::dvec3 dustExtinction = glm::dvec3(0.025, 0.065, 0.16) *
        (air.dustPercent / 0.0001) * density / result.aerosolScaleHeight;
    result.aerosolScattering = dustExtinction * glm::dvec3(0.65, 0.25, 0.08);
    result.aerosolAbsorption = dustExtinction - result.aerosolScattering;
    const double excessPressure = std::max(0.0, waterPressure - saturated);
    // Ideal-gas condensable water mass; only a configured fraction stays aloft.
    result.suspendedLiquidWaterGm3 = excessPressure * 0.01801528 / (8.314462618 * air.temperatureK) *
        1000.0 * config.suspended_water_fraction;
    result.suspendedLiquidWaterGm3 = air.suspendedLiquidWaterGm3.value_or(result.suspendedLiquidWaterGm3);
    const double liquidKgM3 = result.suspendedLiquidWaterGm3 / 1000.0;
    const double dropletExtinction = 3.0 * liquidKgM3 / (2.0 * 1000.0 * config.droplet_radius_um * 1e-6);
    result.aerosolScattering += glm::dvec3(dropletExtinction * radiusMeters);
    return result;
}

inline glm::dvec3 atmosphereExtinction(const AtmosphereOptics& optics, double altitudeInRadii) {
    if (!std::isfinite(altitudeInRadii)) throw std::invalid_argument("Invalid atmospheric altitude");
    const double h = std::max(0.0, altitudeInRadii);
    return optics.rayleigh * std::exp(-h / optics.molecularScaleHeight) +
        (optics.aerosolScattering + optics.aerosolAbsorption) * std::exp(-h / optics.aerosolScaleHeight);
}

// Spherical refractivity and its radial derivative, with the same smooth
// vacuum boundary as the GPU density profile. Coordinates are planet radii.
inline glm::dvec2 atmosphericRefractivity(const config::AtmosphereConfig& config,
                                         const AtmosphereOptics& optics, double radius) {
    if (!config.enabled || !config.refraction_enabled || radius >= config.radius_multiplier)
        return {0, 0};
    const double h = std::max(0.0, radius - 1.0), top = config.radius_multiplier - 1.0;
    const double t = std::clamp((h - 0.85 * top) / (0.15 * top), 0.0, 1.0);
    const double taper = 1 - t*t*(3 - 2*t);
    const double derivative = -6*t*(1-t) / (0.15 * top);
    const double base = (optics.refractiveIndex - 1) * std::exp(-h / optics.molecularScaleHeight);
    return {base * taper, base * (derivative - taper / optics.molecularScaleHeight)};
}
struct AtmosphericRay {
    glm::dvec3 position, direction;
    double distance = 0;
    bool escaped = false, hitGround = false;
};
// Fermat/eikonal ray equation: d(direction)/ds = (grad(n) - direction *
// dot(direction, grad(n))) / n. A midpoint step bends toward denser air.
inline void advanceAtmosphericRay(const config::AtmosphereConfig& config,
                                  const AtmosphereOptics& optics, AtmosphericRay& ray, double step) {
    const auto curvature = [&](const glm::dvec3& p, const glm::dvec3& direction) {
        const auto nu = atmosphericRefractivity(config, optics, glm::length(p));
        const auto gradient = glm::normalize(p) * nu.y;
        return (gradient - direction * glm::dot(direction, gradient)) / (1 + nu.x);
    };
    const auto midpointDirection = glm::normalize(ray.direction + 0.5 * step * curvature(ray.position, ray.direction));
    const auto midpoint = ray.position + 0.5 * step * ray.direction;
    ray.direction = glm::normalize(ray.direction + step * curvature(midpoint, midpointDirection));
    ray.position += step * midpointDirection;
    ray.distance += step;
}
// CPU reference tracer for diagnostics, optical tests and future local weather.
// The renderer uses fewer bounded steps and the raster depth for terrain.
inline AtmosphericRay traceAtmosphericRay(const config::AtmosphereConfig& config,
                                          const AtmosphereOptics& optics, glm::dvec3 origin,
                                          glm::dvec3 direction, double step = 0.001) {
    config.validate();
    if (!std::isfinite(glm::length(origin)) || glm::length(origin) < 1.0 ||
        !std::isfinite(glm::length(direction)) || glm::length(direction) <= 0 ||
        !std::isfinite(step) || step <= 0 || step > 0.1)
        throw std::invalid_argument("Invalid atmospheric ray");
    AtmosphericRay ray{origin, glm::normalize(direction)};
    const double outer = config.radius_multiplier;
    const double b = glm::dot(ray.position, ray.direction);
    const double discriminant = b*b - glm::dot(origin, origin) + outer*outer;
    if (discriminant < 0 || -b + std::sqrt(discriminant) <= 0) {
        ray.escaped = true; return ray;
    }
    const double entry = std::max(0.0, -b - std::sqrt(discriminant));
    ray.position += entry * ray.direction; ray.distance = entry;
    const double limit = entry + 4 * outer; // Bound rays trapped by extreme density gradients.
    for (int i = 0; i < 100000 && ray.distance < limit; ++i) {
        if (glm::length(ray.position) >= outer - 1e-12 && glm::dot(ray.position, ray.direction) > 0) {
            ray.escaped = true; break;
        }
        if (glm::length(ray.position) <= 1.0 && glm::dot(ray.position, ray.direction) < 0) {
            ray.hitGround = true; break;
        }
        advanceAtmosphericRay(config, optics, ray, std::min(step, limit - ray.distance));
    }
    return ray;
}

// Approximate hemispheric skylight for the incident-light exposure meter.
// A bright twilight sky must prevent the night sensitivity limit from clipping
// sunsets. This estimate is stateless and intentionally does not adapt to dust
// darkness by increasing exposure. The renderer still integrates actual rays.
inline double atmosphericSkyIlluminance(const config::AtmosphereConfig& config,
                                        const AtmosphereOptics& optics,
                                        const glm::dvec3& eyeBody, const glm::dvec3& towardSun,
                                        const glm::dvec3& sunlight) {
    const double radius = glm::length(eyeBody);
    if (!config.enabled || radius <= 0 || radius >= config.radius_multiplier) return 0;
    const double mu = glm::dot(eyeBody / radius, towardSun);
    const double bottom = std::max(1.0, radius);
    const double step = (config.radius_multiplier - bottom) / 16.0;
    glm::dvec3 scatteringColumn(0);
    for (int i = 0; i < 16; ++i) {
        const double r = bottom + (i + 0.5) * step;
        const double horizon = -std::sqrt(std::max(0.0, 1.0 - 1.0 / (r*r)));
        const double visible = std::clamp((mu - horizon + 0.025) / 0.05, 0.0, 1.0);
        const double h = r - 1.0;
        scatteringColumn += visible * step * (optics.rayleigh * std::exp(-h / optics.molecularScaleHeight) +
            optics.aerosolScattering * std::exp(-h / optics.aerosolScaleHeight));
    }
    const glm::dvec3 sky = sunlight * (glm::dvec3(1) - glm::exp(-scatteringColumn)) * 0.5;
    return glm::dot(sky, glm::dvec3(0.2126, 0.7152, 0.0722));
}

// Physical forcing for a future climate solver, separate from display exposure.
inline double incidentSolarPowerWm2(double luminosityWatts, double distanceMeters,
                                    const glm::dvec3& outwardNormal, const glm::dvec3& towardSun) {
    if (!std::isfinite(luminosityWatts) || luminosityWatts < 0 ||
        !std::isfinite(distanceMeters) || distanceMeters <= 0 ||
        !std::isfinite(glm::length(outwardNormal)) || glm::length(outwardNormal) <= 0 ||
        !std::isfinite(glm::length(towardSun)) || glm::length(towardSun) <= 0)
        throw std::invalid_argument("Invalid solar forcing input");
    return luminosityWatts / (4.0 * std::numbers::pi * distanceMeters * distanceMeters) *
        std::max(0.0, glm::dot(glm::normalize(outwardNormal), glm::normalize(towardSun)));
}
} // namespace simulation

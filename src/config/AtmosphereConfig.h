#pragma once

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace config {
class Config;

struct AtmosphereConfig {
    bool enabled = false;
    bool refraction_enabled = true;
    double radius_multiplier = 1.1;
    double surface_pressure_pa = 101325.0;
    double temperature_k = 293.15;
    double suspended_water_fraction = 0.001;
    double droplet_radius_um = 10.0;
    // Percentages, not fractions. The last component is an aerosol loading.
    double nitrogen = 78.08, oxygen = 20.95, water = 0.0;
    double carbon_dioxide = 0.04, argon = 0.93, red_dust = 0.0;

    AtmosphereConfig() = default;
    explicit AtmosphereConfig(const Config& cfg);
    void validate() const {
        auto between = [](double v, double low, double high) { return std::isfinite(v) && v >= low && v <= high; };
        if (!between(radius_multiplier, 1.001, 2.0) || !between(surface_pressure_pa, 0.0, 1e7) ||
            !between(temperature_k, 180.0, 373.15) || !between(suspended_water_fraction, 0.0, 1.0) ||
            !between(droplet_radius_um, 0.1, 100.0))
            throw std::invalid_argument("Invalid atmosphere radius, pressure, temperature or droplets");
        double sum = 0;
        for (double v : {nitrogen, oxygen, water, carbon_dioxide, argon, red_dust}) {
            if (!between(v, 0.0, 100.0)) throw std::invalid_argument("Atmosphere percentages must be finite and in [0,100]");
            sum += v;
        }
        if (sum > 100.0 + 1e-9) throw std::invalid_argument("Atmosphere percentages exceed 100");
    }
    double balancePercent() const {
        return std::max(0.0, 100.0 - nitrogen - oxygen - water - carbon_dioxide - argon - red_dust);
    }
};
} // namespace config

#pragma once

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include "config/Config.h"

namespace config {

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
    explicit AtmosphereConfig(const Config& cfg) {
        const auto& raw = cfg.data();
        if (!raw.is_object()) throw std::invalid_argument("planet.atmosphere must be an object");
        enabled = true;
        if (raw.contains("enabled")) {
            if (!raw["enabled"].is_boolean()) throw std::invalid_argument("atmosphere.enabled must be boolean");
            enabled = raw["enabled"].get<bool>();
        }
        if (raw.contains("refraction_enabled")) {
            if (!raw["refraction_enabled"].is_boolean()) throw std::invalid_argument("atmosphere.refraction_enabled must be boolean");
            refraction_enabled = raw["refraction_enabled"].get<bool>();
        }
        auto number = [](const auto& object, const char* key, double& value) {
            if (!object.contains(key)) return;
            if (!object[key].is_number()) throw std::invalid_argument(std::string("atmosphere.") + key + " must be numeric");
            value = object[key].template get<double>();
        };
        number(raw, "radius_multiplier", radius_multiplier);
        number(raw, "surface_pressure_pa", surface_pressure_pa);
        number(raw, "temperature_k", temperature_k);
        number(raw, "suspended_water_fraction", suspended_water_fraction);
        number(raw, "droplet_radius_um", droplet_radius_um);
        if (raw.contains("gas-contents")) {
            const auto& gas = raw["gas-contents"];
            if (!gas.is_object()) throw std::invalid_argument("atmosphere.gas-contents must be an object");
            nitrogen = oxygen = water = carbon_dioxide = argon = red_dust = 0.0;
            for (auto it = gas.begin(); it != gas.end(); ++it) {
                if (it.key() != "nitrogen" && it.key() != "oxygen" && it.key() != "water" &&
                    it.key() != "carbon_dioxide" && it.key() != "argon" && it.key() != "red_dust" &&
                    it.key() != "description" && it.key() != "parameter_descriptions")
                    throw std::invalid_argument("Unknown atmosphere constituent: " + it.key());
            }
            number(gas, "nitrogen", nitrogen); number(gas, "oxygen", oxygen);
            number(gas, "water", water); number(gas, "carbon_dioxide", carbon_dioxide);
            number(gas, "argon", argon); number(gas, "red_dust", red_dust);
        }
        validate();
    }
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

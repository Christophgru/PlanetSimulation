#include "config/AtmosphereConfig.h"
#include "config/Config.h"

namespace config {

AtmosphereConfig::AtmosphereConfig(const Config& cfg) {
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
} // namespace config

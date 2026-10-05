#pragma once

#include <array>
#include <optional>
#include <vector>
#include "config/ScenarioConfig.h"
#include "simulation/Atmosphere.h"

namespace rendering {
// Reference-air coefficients are shared by every view of one prepared scene.
// Dynamic transforms and sunlight stay outside the key. Scene reload moves the
// cache with its configuration; changing any optical input refreshes one entry.
class AtmosphereOpticsCache {
    using Key = std::array<double, 14>;
    struct Entry { Key key; simulation::AtmosphereOptics optics; };
    std::vector<std::optional<Entry>> entries_;
    std::size_t evaluations_ = 0, reuses_ = 0;
public:
    explicit AtmosphereOpticsCache(const config::ScenarioConfig& scenario)
        : entries_(scenario.planets.size()) {
        for (std::size_t i = 0; i < scenario.planets.size(); ++i)
            get(i, scenario.planets[i], scenario.metersPerWorldUnit());
    }
    const simulation::AtmosphereOptics& get(std::size_t index,
                                            const config::PlanetConfig& planet,
                                            double metersPerUnit) {
        const auto& a = planet.atmosphere;
        const double radiusMeters = planet.radius * metersPerUnit;
        const Key key{double(a.enabled), double(a.refraction_enabled),
            a.radius_multiplier, a.surface_pressure_pa, a.temperature_k,
            a.suspended_water_fraction, a.droplet_radius_um, a.nitrogen, a.oxygen,
            a.water, a.carbon_dioxide, a.argon, a.red_dust, radiusMeters};
        auto& entry = entries_.at(index);
        if (entry && entry->key == key) {
            ++reuses_;
            return entry->optics;
        }
        // Validate and calculate before replacing a previously usable pack.
        const auto optics = simulation::atmosphereOptics(a, radiusMeters, simulation::referenceAir(a));
        entry = Entry{key, optics};
        ++evaluations_;
        return entry->optics;
    }
    std::size_t evaluations() const { return evaluations_; }
    std::size_t reuses() const { return reuses_; }
};
} // namespace rendering

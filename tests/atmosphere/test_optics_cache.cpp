#include "rendering/atmosphere/AtmosphereOpticsCache.h"
#include <gtest/gtest.h>
#include <functional>
#include <limits>

namespace {
config::ScenarioConfig atmosphereScene() {
    config::ScenarioConfig scene;
    scene.planets.resize(2);
    auto& air = scene.planets[0].atmosphere;
    air.enabled = true; air.nitrogen = 70; air.oxygen = 20; air.water = 1;
    return scene;
}
void sameOptics(const simulation::AtmosphereOptics& actual,
                const simulation::AtmosphereOptics& expected) {
    for (int component = 0; component < 3; ++component) {
        EXPECT_DOUBLE_EQ(actual.rayleigh[component], expected.rayleigh[component]);
        EXPECT_DOUBLE_EQ(actual.aerosolScattering[component], expected.aerosolScattering[component]);
        EXPECT_DOUBLE_EQ(actual.aerosolAbsorption[component], expected.aerosolAbsorption[component]);
    }
    EXPECT_DOUBLE_EQ(actual.molecularScaleHeight, expected.molecularScaleHeight);
    EXPECT_DOUBLE_EQ(actual.aerosolScaleHeight, expected.aerosolScaleHeight);
    EXPECT_DOUBLE_EQ(actual.refractiveIndex, expected.refractiveIndex);
    EXPECT_DOUBLE_EQ(actual.relativeHumidity, expected.relativeHumidity);
    EXPECT_DOUBLE_EQ(actual.suspendedLiquidWaterGm3, expected.suspendedLiquidWaterGm3);
}
}
TEST(AtmosphereOpticsCache, ReusesOnePackAcrossViewsAndMovingBodies) {
    auto scene = atmosphereScene();
    rendering::AtmosphereOpticsCache cache(scene);
    const auto* pack = &cache.get(0, scene.planets[0], 1000);
    for (int view = 0; view < 64; ++view) {
        scene.planets[0].position[0] += 3;
        scene.planets[0].color[0] = .1;
        scene.planets[0].water.enabled = !scene.planets[0].water.enabled;
        EXPECT_EQ(&cache.get(0, scene.planets[0], 1000), pack);
        EXPECT_EQ(cache.evaluations(), 2);
    }
    sameOptics(*pack, simulation::atmosphereOptics(scene.planets[0].atmosphere, 1500,
        simulation::referenceAir(scene.planets[0].atmosphere)));
    EXPECT_EQ(cache.reuses(), 65);
    const auto& airless = cache.get(1, scene.planets[1], 1000);
    EXPECT_EQ(airless.rayleigh, glm::dvec3(0));
    EXPECT_DOUBLE_EQ(airless.refractiveIndex, 1);
}
TEST(AtmosphereOpticsCache, RefreshesEveryOpticalInputAndMatchesTheReference) {
    using Planet = config::PlanetConfig;
    const std::vector<std::function<void(Planet&)>> changes{
        [](auto& p) {p.atmosphere.enabled = false;},
        [](auto& p) {p.atmosphere.refraction_enabled = false;},
        [](auto& p) {p.atmosphere.radius_multiplier = 1.2;},
        [](auto& p) {p.atmosphere.surface_pressure_pa *= .5;},
        [](auto& p) {p.atmosphere.surface_pressure_pa = 0;},
        [](auto& p) {p.atmosphere.temperature_k = 280;},
        [](auto& p) {p.atmosphere.suspended_water_fraction = .002;},
        [](auto& p) {p.atmosphere.droplet_radius_um = 8;},
        [](auto& p) {p.atmosphere.nitrogen -= 2;},
        [](auto& p) {p.atmosphere.oxygen += 2;},
        [](auto& p) {p.atmosphere.water = 5;},
        [](auto& p) {p.atmosphere.carbon_dioxide = .2;},
        [](auto& p) {p.atmosphere.argon = 2;},
        [](auto& p) {p.atmosphere.red_dust = .01;},
        [](auto& p) {p.radius = 2;},
    };
    for (std::size_t input = 0; input < changes.size(); ++input) {
        SCOPED_TRACE(input);
        auto scene = atmosphereScene();
        rendering::AtmosphereOpticsCache cache(scene);
        changes[input](scene.planets[0]);
        const auto& planet = scene.planets[0];
        sameOptics(cache.get(0, planet, 1000), simulation::atmosphereOptics(planet.atmosphere,
            planet.radius * 1000, simulation::referenceAir(planet.atmosphere)));
        EXPECT_EQ(cache.evaluations(), 3);
        cache.get(0, planet, 1000);
        EXPECT_EQ(cache.evaluations(), 3);
    }
}
TEST(AtmosphereOpticsCache, UsesPhysicalRadiusAndKeepsBodySlotsIndependent) {
    auto scene = atmosphereScene();
    scene.planets[0].atmosphere.water = 5; // Condensed droplets depend on SI radius.
    rendering::AtmosphereOpticsCache cache(scene);
    const auto original = cache.get(0, scene.planets[0], 1000);
    scene.planets[0].radius = 1500;
    sameOptics(cache.get(0, scene.planets[0], 1), original);
    EXPECT_EQ(cache.evaluations(), 2); // Same SI radius in metres instead of km.
    const auto other = cache.get(1, scene.planets[1], 1000);
    const auto changed = cache.get(0, scene.planets[0], 2);
    ASSERT_GT(original.suspendedLiquidWaterGm3, 0);
    EXPECT_NE(changed.aerosolScattering, original.aerosolScattering);
    sameOptics(changed, simulation::atmosphereOptics(scene.planets[0].atmosphere,
        3000, simulation::referenceAir(scene.planets[0].atmosphere)));
    EXPECT_EQ(cache.evaluations(), 3);
    sameOptics(cache.get(1, scene.planets[1], 1000), other);
    EXPECT_EQ(cache.evaluations(), 3);
}
TEST(AtmosphereOpticsCache, FailedRefreshPreservesThePreviousPackAndCanRecover) {
    auto scene = atmosphereScene();
    rendering::AtmosphereOpticsCache cache(scene);
    const auto& planet = scene.planets[0];
    const auto* retained = &cache.get(0, planet, 1000);
    const auto original = *retained;
    for (double pressure : {-1.0, std::numeric_limits<double>::quiet_NaN(),
                            std::numeric_limits<double>::infinity()}) {
        auto invalid = planet; invalid.atmosphere.surface_pressure_pa = pressure;
        EXPECT_THROW(cache.get(0, invalid, 1000), std::invalid_argument);
        sameOptics(*retained, original);
        EXPECT_EQ(cache.evaluations(), 2);
    }
    EXPECT_THROW(cache.get(0, planet, -1), std::invalid_argument);
    EXPECT_EQ(&cache.get(0, planet, 1000), retained);
    auto dusty = planet; dusty.atmosphere.red_dust = .1;
    const auto updated = cache.get(0, dusty, 1000);
    EXPECT_GT(updated.aerosolAbsorption.r, original.aerosolAbsorption.r);
    EXPECT_EQ(cache.evaluations(), 3);
}
TEST(AtmosphereOpticsCache, ReorderedReplacementOwnsItsPacksAndBoundsItsSlots) {
    auto scene = atmosphereScene();
    rendering::AtmosphereOpticsCache old(scene);
    const auto original = old.get(0, scene.planets[0], 1000);
    std::swap(scene.planets[0], scene.planets[1]);
    rendering::AtmosphereOpticsCache replacement(scene);
    auto moved = std::move(replacement);
    sameOptics(moved.get(1, scene.planets[1], 1000), original);
    sameOptics(old.get(0, scene.planets[1], 1000), original);
    EXPECT_EQ(moved.evaluations(), 2);
    EXPECT_EQ(old.evaluations(), 2);
    EXPECT_THROW(moved.get(2, scene.planets[0], 1000), std::out_of_range);
    config::ScenarioConfig empty;
    rendering::AtmosphereOpticsCache noBodies(empty);
    EXPECT_EQ(noBodies.evaluations(), 0);
    EXPECT_THROW(noBodies.get(0, scene.planets[0], 1000), std::out_of_range);
}

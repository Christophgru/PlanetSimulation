#include <gtest/gtest.h>
#include <limits>
#include "simulation/Atmosphere.h"
#include "config/ScenarioConfig.h"
#include "config/SceneReplay.h"

using nlohmann::json;
namespace {
config::AtmosphereConfig parse(const json& value) { return config::AtmosphereConfig(config::Config{json(value)}); }
}
TEST(AtmosphereConfig, OptionalBlockAndLegacyMetadataPreserveAirlessScenes) {
    const config::ScenarioConfig legacy(config::Config(json{{"planet", {{"atmosphere_enabled", true}}}}));
    EXPECT_TRUE(legacy.planets[0].atmosphere_enabled);
    EXPECT_FALSE(legacy.planets[0].atmosphere.enabled);
    const auto defaults = parse(json::object());
    EXPECT_TRUE(defaults.enabled);
    EXPECT_DOUBLE_EQ(defaults.radius_multiplier, 1.1);
    EXPECT_NEAR(defaults.balancePercent(), 0.0, 1e-10);
    const auto mixture = parse({{"gas-contents", {{"oxygen", 25}, {"water", 2}, {"nitrogen", 70}, {"red_dust", 0.0001}}}});
    EXPECT_NEAR(mixture.balancePercent(), 2.9999, 1e-10);
    EXPECT_DOUBLE_EQ(mixture.oxygen, 25);
    EXPECT_DOUBLE_EQ(mixture.carbon_dioxide, 0);
}
TEST(AtmosphereConfig, RejectsInvalidCompositionAndPhysicalInputs) {
    for (const auto& value : std::vector<json>{nullptr, false, 1, json::array(),
         {{"enabled", "yes"}}, {{"gas-contents", json::array()}}, {{"gas-contents", {{"oxgyen", 25}}}},
         {{"gas-contents", {{"oxygen", "25%"}}}}, {{"gas-contents", {{"oxygen", -1}}}},
         {{"gas-contents", {{"oxygen", 40}, {"nitrogen", 70}}}},
         {{"radius_multiplier", 1}}, {{"radius_multiplier", 4}}, {{"surface_pressure_pa", -1}},
         {{"temperature_k", 0}}, {{"suspended_water_fraction", 2}}, {{"droplet_radius_um", 0}}})
        EXPECT_THROW(parse(value), std::exception) << value;
    auto bad = parse(json::object());
    bad.water = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(bad.validate(), std::invalid_argument);
}
TEST(AtmosphereOptics, RayleighIsBlueAndDensityIsLinearRatherThanSquared) {
    const auto cfg = parse(json::object());
    auto air = simulation::referenceAir(cfg);
    const auto sea = simulation::atmosphereOptics(cfg, 1000, air);
    EXPECT_GT(sea.refractiveIndex, 1.0002);
    EXPECT_LT(sea.refractiveIndex, 1.0004);
    EXPECT_GT(sea.rayleigh.b, 5 * sea.rayleigh.r);
    air.pressurePa *= 0.5;
    const auto thin = simulation::atmosphereOptics(cfg, 1000, air);
    EXPECT_NEAR(thin.refractiveIndex - 1, 0.5 * (sea.refractiveIndex - 1), 1e-15);
    EXPECT_NEAR(thin.rayleigh.r, 0.5 * sea.rayleigh.r, 1e-12);
    air.pressurePa = 0;
    const auto vacuum = simulation::atmosphereOptics(cfg, 1000, air);
    EXPECT_DOUBLE_EQ(vacuum.refractiveIndex, 1);
    EXPECT_DOUBLE_EQ(glm::length(vacuum.rayleigh + vacuum.aerosolScattering + vacuum.aerosolAbsorption), 0);
}
TEST(AtmosphereOptics, CompositionAndTemperatureChangeRefractionAndScattering) {
    const auto nitrogen = parse({{"gas-contents", {{"nitrogen", 100}}}});
    const auto oxygen = parse({{"gas-contents", {{"oxygen", 100}}}});
    const auto n = simulation::atmosphereOptics(nitrogen, 1000, simulation::referenceAir(nitrogen));
    const auto o = simulation::atmosphereOptics(oxygen, 1000, simulation::referenceAir(oxygen));
    EXPECT_GT(n.refractiveIndex, o.refractiveIndex);
    EXPECT_GT(n.rayleigh.r, o.rayleigh.r);
    auto warmer = simulation::referenceAir(nitrogen); warmer.temperatureK = 330;
    EXPECT_LT(simulation::atmosphereOptics(nitrogen, 1000, warmer).rayleigh.r, n.rayleigh.r);
}
TEST(AtmosphereOptics, WaterVaporCondensesOnlyAboveSaturationAndDependsOnTemperature) {
    const auto cfg = parse({{"gas-contents", {{"nitrogen", 70}, {"oxygen", 25}, {"water", 2}}}});
    auto air = simulation::referenceAir(cfg);
    EXPECT_NEAR(simulation::saturationPressurePa(293.15), 2333.4, 5.0);
    const auto warm = simulation::atmosphereOptics(cfg, 1000, air);
    EXPECT_LT(warm.relativeHumidity, 1);
    EXPECT_DOUBLE_EQ(warm.suspendedLiquidWaterGm3, 0);
    air.temperatureK = 278.15;
    const auto cool = simulation::atmosphereOptics(cfg, 1000, air);
    EXPECT_GT(cool.relativeHumidity, 1);
    EXPECT_GT(cool.suspendedLiquidWaterGm3, 0);
    EXPECT_GT(cool.aerosolScattering.r, warm.aerosolScattering.r);
    EXPECT_DOUBLE_EQ(cool.aerosolScattering.r, cool.aerosolScattering.b);
    air.waterPercent = 20;
    const auto wet = simulation::atmosphereOptics(cfg, 1000, air);
    EXPECT_GT(wet.suspendedLiquidWaterGm3, cool.suspendedLiquidWaterGm3);
}
TEST(AtmosphereOptics, RedDustWarmsTransmissionAndHeavyDustExtinguishesIt) {
    const auto cfg = parse({{"gas-contents", {{"nitrogen", 75}, {"oxygen", 25}}}});
    auto air = simulation::referenceAir(cfg); air.dustPercent = 0.0001;
    const auto light = simulation::atmosphereOptics(cfg, 1000, air);
    const auto tau = simulation::atmosphereExtinction(light, 0) * 0.1;
    EXPECT_GT(std::exp(-tau.r), std::exp(-tau.b));
    air.dustPercent = 0.1;
    const auto heavy = simulation::atmosphereOptics(cfg, 1000, air);
    for (int c = 0; c < 3; ++c) EXPECT_LT(std::exp(-simulation::atmosphereExtinction(heavy, 0.03)[c] * 0.03), 1e-3); // Below an 8-bit display step.
    EXPECT_LT(simulation::atmosphereExtinction(light, 0.05).r, simulation::atmosphereExtinction(light, 0).r);
}
TEST(AtmosphereClimate, SolarForcingFollowsIncidenceAndInverseSquareDistance) {
    constexpr double luminosity = 3.828e26, distance = 149597870700.0;
    const auto noon = simulation::incidentSolarPowerWm2(luminosity, distance, {1,0,0}, {1,0,0});
    EXPECT_NEAR(noon, 1361.17, 0.1);
    EXPECT_NEAR(simulation::incidentSolarPowerWm2(luminosity, 2*distance, {1,0,0}, {1,0,0}), noon/4, 1e-10);
    EXPECT_DOUBLE_EQ(simulation::incidentSolarPowerWm2(luminosity, distance, {-1,0,0}, {1,0,0}), 0);
    EXPECT_DOUBLE_EQ(simulation::incidentSolarPowerWm2(luminosity, distance, {0,1,0}, {1,0,0}), 0);
    EXPECT_THROW(simulation::incidentSolarPowerWm2(luminosity, 0, {1,0,0}, {1,0,0}), std::invalid_argument);
}
TEST(AtmosphereReplay, ResolvedScenePreservesCompositionWhenWorkingConfigChanges) {
    json scene{{"planets", {{{"atmosphere", {{"gas-contents", {{"oxygen",25},{"nitrogen",70},{"water",2},{"red_dust",0.0001}}}}}}}},
               {"surface_camera", {{"latitude_deg",0},{"longitude_deg",180}}}};
    const json saved{{"scenario",scene},{"surface_camera",scene["surface_camera"]}};
    auto edited=scene; edited["planets"][0]["atmosphere"]["gas-contents"]["water"]=0;
    const auto restored=config::applyCameraReplay(edited,saved);
    const config::ScenarioConfig parsed(config::Config{json(restored)});
    EXPECT_DOUBLE_EQ(parsed.planets[0].atmosphere.water,2);
    EXPECT_DOUBLE_EQ(parsed.planets[0].atmosphere.red_dust,0.0001);
}

TEST(AtmosphereExposure, TwilightSkylightIsMeteredWithoutInventingLightAtDeepNight) {
    const auto cfg = parse(json::object());
    const auto optics = simulation::atmosphereOptics(cfg, 1000, simulation::referenceAir(cfg));
    const glm::dvec3 eye(0,0,1.03);
    const auto daylight = simulation::atmosphericSkyIlluminance(cfg, optics, eye, {0,0,1}, {1,1,1});
    const auto twilight = simulation::atmosphericSkyIlluminance(cfg, optics, eye, glm::normalize(glm::dvec3(1,0,-0.05)), {1,1,1});
    EXPECT_GT(twilight, 0.001);
    EXPECT_LE(twilight, daylight);
    EXPECT_DOUBLE_EQ(simulation::atmosphericSkyIlluminance(cfg, optics, eye, {0,0,-1}, {1,1,1}), 0);
    EXPECT_DOUBLE_EQ(simulation::atmosphericSkyIlluminance(cfg, optics, eye, {0,0,1}, {0,0,0}), 0);
}

TEST(AtmosphereClimate, LocalCloudWaterCanOverrideTheInitialCondensationEstimate) {
    const auto cfg = parse(json::object());
    auto air = simulation::referenceAir(cfg);
    const auto clear = simulation::atmosphereOptics(cfg, 1000, air);
    air.suspendedLiquidWaterGm3 = 0.3;
    const auto cloud = simulation::atmosphereOptics(cfg, 1000, air);
    EXPECT_DOUBLE_EQ(cloud.suspendedLiquidWaterGm3, 0.3);
    EXPECT_GT(cloud.aerosolScattering.r, clear.aerosolScattering.r);
    EXPECT_DOUBLE_EQ(cloud.refractiveIndex, clear.refractiveIndex);
    air.suspendedLiquidWaterGm3 = -1;
    EXPECT_THROW(simulation::atmosphereOptics(cfg, 1000, air), std::invalid_argument);
}

TEST(AtmosphereRefraction, VacuumDisabledAndRadialRaysRemainStraight) {
    auto cfg = parse(json::object());
    const auto optics = simulation::atmosphereOptics(cfg, 1000, simulation::referenceAir(cfg));
    const auto radial = simulation::traceAtmosphericRay(cfg, optics, {0,1.001,0}, {0,1,0});
    EXPECT_TRUE(radial.escaped);
    EXPECT_DOUBLE_EQ(radial.direction.y, 1);
    cfg.refraction_enabled = false;
    const auto disabled = simulation::traceAtmosphericRay(cfg, optics, {0,1.001,0}, {1,0,0});
    EXPECT_TRUE(disabled.escaped);
    EXPECT_DOUBLE_EQ(disabled.direction.y, 0);
    cfg.refraction_enabled = true; cfg.surface_pressure_pa = 0;
    const auto vacuum = simulation::atmosphereOptics(cfg, 1000, simulation::referenceAir(cfg));
    EXPECT_DOUBLE_EQ(simulation::traceAtmosphericRay(cfg, vacuum, {0,1.001,0}, {1,0,0}).direction.y, 0);
    EXPECT_THROW(parse({{"refraction_enabled", "yes"}}), std::invalid_argument);
}
TEST(AtmosphereRefraction, GrazingRaysBendTowardDenseAirAndConserveSphericalSnellInvariant) {
    const auto cfg = parse(json::object());
    const auto optics = simulation::atmosphereOptics(cfg, 1000, simulation::referenceAir(cfg));
    const glm::dvec3 origin(0,1.001,0), direction(1,0,0);
    const auto ray = simulation::traceAtmosphericRay(cfg, optics, origin, direction);
    ASSERT_TRUE(ray.escaped);
    EXPECT_LT(ray.direction.y, -0.001);
    EXPECT_GT(ray.direction.y, -0.01);
    const double invariant = (1 + simulation::atmosphericRefractivity(cfg, optics, glm::length(origin)).x) *
        glm::length(glm::cross(origin, direction));
    EXPECT_NEAR(glm::length(glm::cross(ray.position, ray.direction)), invariant, 2e-8);
    const auto finer = simulation::traceAtmosphericRay(cfg, optics, origin, direction, 0.0005);
    EXPECT_LT(glm::length(ray.direction - finer.direction), 1e-7);
    auto air = simulation::referenceAir(cfg); air.pressurePa *= 0.5;
    const auto thin = simulation::atmosphereOptics(cfg, 1000, air);
    const auto thinRay = simulation::traceAtmosphericRay(cfg, thin, origin, direction);
    EXPECT_NEAR(thinRay.direction.y / ray.direction.y, 0.5, 0.01);
    air = simulation::referenceAir(cfg); air.temperatureK = 330;
    const auto hot = simulation::atmosphereOptics(cfg, 1000, air);
    EXPECT_GT(simulation::traceAtmosphericRay(cfg, hot, origin, direction).direction.y, ray.direction.y);
}
TEST(AtmosphereRefraction, OrbitalLimbRaysAreReciprocalAndMissedShellsDoNotBend) {
    const auto cfg = parse(json::object());
    const auto optics = simulation::atmosphereOptics(cfg, 1000, simulation::referenceAir(cfg));
    const glm::dvec3 origin(-2,1.005,0), direction(1,0,0);
    const auto ray = simulation::traceAtmosphericRay(cfg, optics, origin, direction);
    ASSERT_TRUE(ray.escaped);
    EXPECT_LT(ray.direction.y, -0.002);
    EXPECT_NEAR(glm::length(glm::cross(ray.position, ray.direction)), 1.005, 3e-8);
    const auto reverse = simulation::traceAtmosphericRay(cfg, optics, ray.position, -ray.direction);
    ASSERT_TRUE(reverse.escaped);
    EXPECT_LT(glm::length(reverse.direction + direction), 2e-7);
    EXPECT_NEAR(reverse.position.y, origin.y, 2e-7);
    const auto miss = simulation::traceAtmosphericRay(cfg, optics, {-2,1.2,0}, {1,0,0});
    EXPECT_TRUE(miss.escaped);
    EXPECT_DOUBLE_EQ(miss.direction.y, 0);
    EXPECT_DOUBLE_EQ(miss.distance, 0);
    EXPECT_TRUE(simulation::traceAtmosphericRay(cfg, optics, {0,2,0}, {0,-1,0}).hitGround);
    EXPECT_THROW(simulation::traceAtmosphericRay(cfg, optics, {0,0,0}, {1,0,0}), std::invalid_argument);
    EXPECT_THROW(simulation::traceAtmosphericRay(cfg, optics, {0,2,0}, {0,0,0}), std::invalid_argument);
}
TEST(AtmosphereRefraction, ShellEdgeHasNoArtificialIndexJump) {
    const auto cfg = parse(json::object());
    const auto optics = simulation::atmosphereOptics(cfg, 1000, simulation::referenceAir(cfg));
    const auto edge = simulation::atmosphericRefractivity(cfg, optics, 1.1);
    EXPECT_DOUBLE_EQ(edge.x, 0); EXPECT_DOUBLE_EQ(edge.y, 0);
    const auto below = simulation::atmosphericRefractivity(cfg, optics, 1.1 - 1e-7);
    EXPECT_LT(below.x, 1e-14); EXPECT_LT(std::abs(below.y), 1e-7);
}

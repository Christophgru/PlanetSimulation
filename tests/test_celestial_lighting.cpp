#include <gtest/gtest.h>
#include <limits>
#include "rendering/CelestialLighting.h"

namespace {

config::ScenarioConfig parse(nlohmann::json raw) {
    return config::ScenarioConfig(config::Config{std::move(raw)});
}

config::ScenarioConfig lightingScene() {
    return parse(nlohmann::json::parse(R"({
        "sun":{"radius":1,"color":[1,0.8,0.6],"absolute_magnitude":4.74},
        "lighting":{"ambient_light":0,"reference_distance":10,"exposure":1},
        "planets":[
            {"name":"earth","radius":1,"reflection":{"geometric_albedo":0}},
            {"name":"moon","radius":0.5,"reflection":{"geometric_albedo":0.12,"color":[0.6,0.8,1]}}
        ]
    })"));
}

std::vector<simulation::BodyState> fullMoon() {
    std::vector<simulation::BodyState> states(3);
    states[0].position = {0, 0, 0};
    states[1].position = {8, 0, 0};
    states[2].position = {10, 0, 0};
    return states;
}

void expectVector(const glm::dvec3& actual, const glm::dvec3& expected, double tolerance = 1e-12) {
    for (int channel = 0; channel < 3; ++channel)
        EXPECT_NEAR(actual[channel], expected[channel], tolerance) << "channel " << channel;
}

nlohmann::json developmentJson() {
    return config::Config::load(std::string(PLANET_SOURCE_DIR) + "/configs/scenarios/solar_system.json").data();
}

} // namespace

TEST(AbsoluteMagnitude, UsesIauBolometricZeroPointAndSolarLuminosity) {
    EXPECT_DOUBLE_EQ(rendering::luminosityWatts(0), 3.0128e28);
    EXPECT_NEAR(rendering::luminosityWatts(4.74) / 3.828e26, 1.0, 5e-6);
    for (double magnitude : {-20.0, -5.0, 0.0, 3.5, 20.0}) {
        EXPECT_NEAR(rendering::luminosityWatts(magnitude - 5) /
                    rendering::luminosityWatts(magnitude), 100.0, 1e-10);
        EXPECT_NEAR(rendering::luminosityWatts(magnitude - 1) /
                    rendering::luminosityWatts(magnitude), 2.51188643150958, 1e-12);
    }
}

TEST(AbsoluteMagnitude, ChangesSunEmissionDirectAndReflectedLightTogether) {
    auto config = lightingScene();
    const auto states = fullMoon();
    const auto original = rendering::calculateLighting(config, states);
    config.sun.absolute_magnitude -= 5;
    const auto brighter = rendering::calculateLighting(config, states);
    expectVector(brighter.sunEmission, 100.0 * original.sunEmission);
    for (std::size_t i = 0; i < config.planets.size(); ++i) {
        expectVector(brighter.planets[i].sunlight, 100.0 * original.planets[i].sunlight);
        expectVector(brighter.planets[i].reflectedLight, 100.0 * original.planets[i].reflectedLight);
    }
    EXPECT_GT(rendering::displayColor(brighter.sunEmission, 1).r,
              rendering::displayColor(original.sunEmission, 1).r);
}

TEST(CelestialLighting, SunlightHasInverseSquareFalloffAndSharedDirection) {
    const auto config = lightingScene();
    auto states = fullMoon();
    const auto nearby = rendering::calculateLighting(config, states);
    expectVector(nearby.planets[0].sunDirection, {-1, 0, 0});
    expectVector(nearby.planets[0].sunlight, nearby.sunEmission * (100.0 / 64.0));
    states[1].position *= 2.0;
    const auto farther = rendering::calculateLighting(config, states);
    expectVector(farther.planets[0].sunlight, nearby.planets[0].sunlight / 4.0);
}

TEST(CelestialLighting, FullMoonReflectsSourceColorAlbedoRadiusAndSphereAverage) {
    const auto light = rendering::calculateLighting(lightingScene(), fullMoon());
    // Sun-Moon distance equals reference distance (10), lunar radius 0.5,
    // Moon-Earth distance 2. Average intercepted flux over 4 pi R_earth^2.
    const double expectedFactor = 0.12 * (0.5 * 0.5) / (2.0 * 2.0) / 4.0;
    expectVector(light.planets[0].reflectedLight,
                 light.sunEmission * glm::dvec3(0.6, 0.8, 1.0) * expectedFactor);
    EXPECT_GT(light.planets[0].reflectedLight.r, 0.0);
    // Earth's zero albedo sends no light to the Moon. No self-reflection.
    expectVector(light.planets[1].reflectedLight, glm::dvec3(0));
}

TEST(CelestialLighting, NewQuarterAndFullPhaseUseSunReflectorReceiverGeometry) {
    const auto config = lightingScene();
    auto states = fullMoon();
    const auto full = rendering::calculateLighting(config, states).planets[0].reflectedLight;
    states[1].position = {10, 2, 0};
    const auto quarter = rendering::calculateLighting(config, states).planets[0].reflectedLight;
    expectVector(quarter, full / std::numbers::pi);
    states[1].position = {12, 0, 0};
    expectVector(rendering::calculateLighting(config, states).planets[0].reflectedLight, glm::dvec3(0), 0);
    EXPECT_DOUBLE_EQ(rendering::lambertPhase(-1), 0);
    EXPECT_DOUBLE_EQ(rendering::lambertPhase(1), 1);
    EXPECT_NEAR(rendering::lambertPhase(0), 1.0 / std::numbers::pi, 1e-15);
}

TEST(CelestialLighting, PhaseIsFiniteBoundedAndMonotonicAcrossTheWholeCycle) {
    double previous = 0;
    for (int i = 0; i <= 10000; ++i) {
        const double phase = rendering::lambertPhase(-1.0 + i / 5000.0);
        EXPECT_GE(phase, previous);
        EXPECT_GE(phase, 0.0);
        EXPECT_LE(phase, 1.0);
        previous = phase;
    }
    EXPECT_DOUBLE_EQ(rendering::lambertPhase(-1.00000001), 0);
    EXPECT_DOUBLE_EQ(rendering::lambertPhase(1.00000001), 1);
}

TEST(CelestialLighting, ReflectionFallsOffOnBothLegsOfTheLightPath) {
    const auto config = lightingScene();
    auto states = fullMoon();
    const auto initial = rendering::calculateLighting(config, states).planets[0].reflectedLight;
    states[1].position = {6, 0, 0}; // Double Moon-receiver separation.
    expectVector(rendering::calculateLighting(config, states).planets[0].reflectedLight, initial / 4.0);
    states[0].position = {-10, 0, 0}; // Also double Sun-Moon separation.
    expectVector(rendering::calculateLighting(config, states).planets[0].reflectedLight, initial / 16.0);
}

TEST(CelestialLighting, ReflectedPowerScalesWithAlbedoAndReflectorArea) {
    auto config = lightingScene();
    const auto states = fullMoon();
    const auto initial = rendering::calculateLighting(config, states).planets[0].reflectedLight;
    config.planets[1].reflection.geometric_albedo *= 2;
    expectVector(rendering::calculateLighting(config, states).planets[0].reflectedLight, initial * 2.0);
    config.planets[1].radius *= 2;
    expectVector(rendering::calculateLighting(config, states).planets[0].reflectedLight, initial * 8.0);
    config.planets[1].reflection.geometric_albedo = 0;
    expectVector(rendering::calculateLighting(config, states).planets[0].reflectedLight, glm::dvec3(0));
}

TEST(CelestialLighting, MultipleReflectorsAddOneBounceWithoutRecursiveAmplification) {
    auto config = lightingScene();
    auto states = fullMoon();
    const auto initial = rendering::calculateLighting(config, states).planets[0].reflectedLight;
    config.planets.push_back(config.planets[1]);
    config.planets.back().name = "another_moon";
    states.push_back(states[2]);
    expectVector(rendering::calculateLighting(config, states).planets[0].reflectedLight, initial * 2.0);
    // Receiver albedo only affects outgoing light, never its incoming moonlight.
    config.planets[0].reflection.geometric_albedo = 1;
    expectVector(rendering::calculateLighting(config, states).planets[0].reflectedLight, initial * 2.0);
}

TEST(CelestialLighting, ReflectionToggleLeavesSunlightUnchanged) {
    auto config = lightingScene();
    const auto states = fullMoon();
    const auto enabled = rendering::calculateLighting(config, states);
    config.lighting.reflections_enabled = false;
    const auto disabled = rendering::calculateLighting(config, states);
    for (std::size_t i = 0; i < config.planets.size(); ++i) {
        expectVector(disabled.planets[i].sunlight, enabled.planets[i].sunlight, 0);
        expectVector(disabled.planets[i].reflectedLight, glm::dvec3(0), 0);
    }
}

TEST(CelestialLighting, UnitConversionTranslationAndPlanetOrderDoNotChangeIllumination) {
    auto config = lightingScene();
    auto states = fullMoon();
    const auto original = rendering::calculateLighting(config, states);
    config.distance_unit = "m";
    config.sun.radius *= 1000;
    config.lighting.reference_distance *= 1000;
    for (auto& planet : config.planets) planet.radius *= 1000;
    for (auto& state : states) state.position = 1000.0 * state.position + glm::dvec3(1e9, -2e9, 3e9);
    std::swap(config.planets[0], config.planets[1]);
    std::swap(states[1], states[2]);
    const auto converted = rendering::calculateLighting(config, states);
    expectVector(converted.planets[1].sunlight, original.planets[0].sunlight);
    expectVector(converted.planets[1].reflectedLight, original.planets[0].reflectedLight);
}

TEST(CelestialLighting, SingleBodyAndCoincidentCentersRemainFinite) {
    auto config = lightingScene();
    auto states = fullMoon();
    states[1].position = states[2].position = states[0].position;
    const auto coincident = rendering::calculateLighting(config, states);
    for (const auto& light : coincident.planets) {
        EXPECT_TRUE(std::isfinite(glm::length(light.sunlight)));
        expectVector(light.reflectedLight, glm::dvec3(0));
    }
    config.planets.resize(1);
    states.resize(2);
    expectVector(rendering::calculateLighting(config, states).planets[0].reflectedLight, glm::dvec3(0));
    config.planets.clear();
    states.resize(1);
    EXPECT_TRUE(rendering::calculateLighting(config, states).planets.empty());
    states.clear();
    EXPECT_THROW(rendering::calculateLighting(config, states), std::invalid_argument);
}

TEST(LightingDisplay, ExposureIsMonotonicPreservesBlackAndBoundsHighlights) {
    expectVector(rendering::displayColor(glm::dvec3(0), 1), glm::dvec3(0), 0);
    double previous = 0;
    for (double energy : {1e-6, 0.001, 0.01, 0.1, 1.0, 2.0, 5.0, 100.0}) {
        const auto color = rendering::displayColor(glm::dvec3(energy), 1);
        EXPECT_GT(color.r, previous);
        EXPECT_LE(color.r, 1.0);
        EXPECT_DOUBLE_EQ(color.r, color.g);
        EXPECT_DOUBLE_EQ(color.r, color.b);
        previous = color.r;
    }
    const auto light = glm::dvec3(0.1, 0.2, 0.5);
    expectVector(rendering::displayColor(light, 2), rendering::displayColor(2.0 * light, 1));
    EXPECT_GT(rendering::displayColor(light, 2).r, rendering::displayColor(light, 1).r);
}

TEST(LightingConfig, ParsesNewFieldsAndRetainsLegacyAmbientFallback) {
    auto config = parse(nlohmann::json::parse(R"({"skybox":{"ambient_light":0.4}})"));
    EXPECT_DOUBLE_EQ(config.lighting.ambient_light, 0.4);
    EXPECT_DOUBLE_EQ(config.sun.absolute_magnitude, 4.74);
    config = parse(nlohmann::json::parse(R"({
        "skybox":{"ambient_light":0.4},
        "lighting":{"ambient_light":0.01,"reference_distance":20,"exposure":2,"reflections_enabled":false},
        "sun":{"absolute_magnitude":-1},
        "planet":{"reflection":{"geometric_albedo":0.2,"color":[0.3,0.4,0.5]}}
    })"));
    EXPECT_DOUBLE_EQ(config.lighting.ambient_light, 0.01);
    EXPECT_DOUBLE_EQ(config.lighting.reference_distance, 20);
    EXPECT_DOUBLE_EQ(config.lighting.exposure, 2);
    EXPECT_FALSE(config.lighting.reflections_enabled);
    EXPECT_DOUBLE_EQ(config.sun.absolute_magnitude, -1);
    EXPECT_DOUBLE_EQ(config.planets[0].reflection.geometric_albedo, 0.2);
    EXPECT_EQ(config.planets[0].reflection.color, (std::vector<double>{0.3, 0.4, 0.5}));
}

TEST(LightingConfig, RejectsInvalidMagnitudeAlbedoExposureDistanceAndColor) {
    for (double bad : {-31.0, 31.0, std::numeric_limits<double>::infinity(),
                       std::numeric_limits<double>::quiet_NaN()}) {
        EXPECT_THROW(parse({{"sun", {{"absolute_magnitude", bad}}}}), std::invalid_argument);
    }
    for (double bad : {-0.01, 1.01, std::numeric_limits<double>::infinity(),
                       std::numeric_limits<double>::quiet_NaN()}) {
        EXPECT_THROW(parse({{"planet", {{"reflection", {{"geometric_albedo", bad}}}}}}), std::invalid_argument);
        EXPECT_THROW(parse({{"lighting", {{"ambient_light", bad}}}}), std::invalid_argument);
    }
    for (const char* field : {"exposure", "reference_distance"}) {
        for (double bad : {-1.0, 0.0, std::numeric_limits<double>::infinity(),
                           std::numeric_limits<double>::quiet_NaN()})
            EXPECT_THROW(parse({{"lighting", {{field, bad}}}}), std::invalid_argument);
    }
    EXPECT_THROW(parse({{"lighting", {{"exposure", 101}}}}), std::invalid_argument);
    EXPECT_THROW(parse({{"lighting", {{"reflections_enabled", "yes"}}}}), std::invalid_argument);
    for (const auto& bad : {nlohmann::json::array({1, 0}), nlohmann::json::array({1, 0, 2}),
                            nlohmann::json::array({-1, 0, 0}), nlohmann::json("white")})
        EXPECT_THROW(parse({{"planet", {{"reflection", {{"color", bad}}}}}}), std::invalid_argument);
    EXPECT_THROW(parse({{"lighting", 1}}), std::invalid_argument);
    EXPECT_THROW(parse({{"planet", {{"reflection", false}}}}), std::invalid_argument);
    EXPECT_THROW(parse({{"sun", {{"absolute_magnitude", "bright"}}}}), nlohmann::json::type_error);
}

TEST(LightingConfig, DescriptionMetadataDoesNotChangeParsingOrPhysics) {
    const auto original = developmentJson();
    auto stripped = original;
    auto strip = [](auto&& self, nlohmann::json& value) -> void {
        if (value.is_object()) {
            value.erase("description");
            value.erase("parameter_descriptions");
            for (auto& child : value) self(self, child);
        } else if (value.is_array()) {
            for (auto& child : value) self(self, child);
        }
    };
    strip(strip, stripped);
    const auto config = parse(original);
    const auto withoutMetadata = parse(stripped);
    const auto states = simulation::OrbitalSystem(config).at(37);
    const auto light = rendering::calculateLighting(config, states);
    const auto plainLight = rendering::calculateLighting(withoutMetadata,
        simulation::OrbitalSystem(withoutMetadata).at(37));
    for (std::size_t i = 0; i < light.planets.size(); ++i) {
        expectVector(light.planets[i].sunlight, plainLight.planets[i].sunlight, 0);
        expectVector(light.planets[i].reflectedLight, plainLight.planets[i].reflectedLight, 0);
    }
}

TEST(LightingConfig, EveryDevelopmentParameterHasAnInlineDescription) {
    const auto raw = developmentJson();
    auto check = [](auto&& self, const nlohmann::json& value) -> void {
        if (value.is_object()) {
            ASSERT_TRUE(value.contains("description"));
            ASSERT_TRUE(value["description"].is_string());
            ASSERT_TRUE(value.contains("parameter_descriptions"));
            for (auto it = value.begin(); it != value.end(); ++it) {
                if (it.key() == "description" || it.key() == "parameter_descriptions") continue;
                ASSERT_TRUE(value["parameter_descriptions"].contains(it.key())) << it.key();
                EXPECT_FALSE(value["parameter_descriptions"][it.key()].get<std::string>().empty());
                self(self, it.value());
            }
        } else if (value.is_array()) {
            for (const auto& child : value) self(self, child);
        }
    };
    check(check, raw);
}

TEST(DevelopmentLighting, BrighterWhiterSunAndVisiblePhaseDependentMoonlight) {
    const auto config = parse(developmentJson());
    EXPECT_LT(config.sun.absolute_magnitude, 4.74);
    EXPECT_GT(config.sun.color[1], 0.95);
    EXPECT_GT(config.sun.color[2], 0.95);
    const simulation::OrbitalSystem system(config);
    double minimum = 1;
    double maximum = 0;
    for (int seconds = 0; seconds <= 600; ++seconds) {
        const auto light = rendering::calculateLighting(config, system.at(seconds));
        const double brightness = glm::length(light.planets[0].reflectedLight);
        minimum = std::min(minimum, brightness);
        maximum = std::max(maximum, brightness);
        EXPECT_TRUE(std::isfinite(brightness));
    }
    EXPECT_GT(maximum, 0.0001);
    EXPECT_GT(maximum, 10 * minimum);
}

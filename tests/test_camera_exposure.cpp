#include <gtest/gtest.h>
#include "rendering/CameraExposure.h"

namespace {
config::LightingConfig settings() {
    config::LightingConfig result;
    result.ambient_light = 0;
    result.auto_exposure.enabled = true;
    return result;
}
rendering::FrameLighting lights(double reflected = 1e-4) {
    rendering::FrameLighting result;
    result.planets.push_back({{1, 0, 0}, glm::dvec3(1), glm::dvec3(reflected)});
    return result;
}
std::vector<simulation::BodyState> bodies(2);
}

TEST(CameraExposure, MoonlitNightIsMoreSensitiveThanDayAndHasVisibleTerrain) {
    const auto day = rendering::cameraExposure(settings(), lights(), bodies, {1, 0, 0}, 0);
    const auto night = rendering::cameraExposure(settings(), lights(), bodies, {-1, 0, 0}, 0);
    EXPECT_GT(night.exposure, day.exposure * 1000);
    EXPECT_GT(day.directIlluminance, 0.9);
    EXPECT_DOUBLE_EQ(night.directIlluminance, 0);
    EXPECT_DOUBLE_EQ(night.reflectedIlluminance, 1e-4);
    EXPECT_GT(rendering::displayColor(glm::dvec3(0.18e-4), night.exposure).r, 0.2);
    EXPECT_LT(day.skySensitivity, 0.02);
    EXPECT_DOUBLE_EQ(night.skySensitivity, 1);
}

TEST(CameraExposure, MoonlessNightStaysDarkEvenAtMaximumSensitivity) {
    const auto dark = rendering::cameraExposure(settings(), lights(0), bodies, {-1, 0, 0}, 0);
    EXPECT_DOUBLE_EQ(dark.exposure, settings().auto_exposure.max_exposure);
    EXPECT_EQ(rendering::displayColor(glm::dvec3(0), dark.exposure), glm::dvec3(0));
    auto ambient = settings();
    ambient.ambient_light = 1e-6;
    const auto faint = rendering::cameraExposure(ambient, lights(0), bodies, {-1, 0, 0}, 0);
    EXPECT_LT(rendering::displayColor(glm::dvec3(0.18e-6), faint.exposure).r, 0.02);
}

TEST(CameraExposure, IsStatelessAndMetersLocalHorizonContinuouslyAcrossSunset) {
    double previous = 0;
    for (int step = 0; step <= 180; ++step) {
        const double angle = glm::radians(double(step));
        const glm::dvec3 eye(std::cos(angle), std::sin(angle), 0);
        const auto exposure = rendering::cameraExposure(settings(), lights(), bodies, eye, 0);
        EXPECT_GE(exposure.exposure, previous);
        previous = exposure.exposure;
    }
    const auto before = rendering::cameraExposure(settings(), lights(), bodies, {-1, 0, 0}, 0);
    (void)rendering::cameraExposure(settings(), lights(), bodies, {1, 0, 0}, 0);
    const auto after = rendering::cameraExposure(settings(), lights(), bodies, {-1, 0, 0}, 0);
    EXPECT_DOUBLE_EQ(before.exposure, after.exposure);
}

TEST(CameraExposure, RespectsManualModeCompensationLimitsAndTranslation) {
    auto config = settings();
    config.auto_exposure.enabled = false;
    config.exposure = 50;
    EXPECT_DOUBLE_EQ(rendering::cameraExposure(config, lights(), bodies, {-1, 0, 0}, 0).exposure, 50);
    EXPECT_DOUBLE_EQ(rendering::cameraExposure(config, lights(), bodies, {-1, 0, 0}).exposure, 50);
    config = settings();
    config.exposure = 2;
    const auto doubled = rendering::cameraExposure(config, lights(), bodies, {1, 0, 0}, 0);
    EXPECT_DOUBLE_EQ(doubled.exposure, 2 * rendering::cameraExposure(settings(), lights(), bodies, {1, 0, 0}, 0).exposure);
    config.auto_exposure.min_exposure = 2;
    config.auto_exposure.max_exposure = 10;
    EXPECT_DOUBLE_EQ(rendering::cameraExposure(config, lights(), bodies, {1, 0, 0}, 0).exposure, 2);
    EXPECT_DOUBLE_EQ(rendering::cameraExposure(config, lights(), bodies, {-1, 0, 0}, 0).exposure, 10);
    auto moved = bodies;
    moved[1].position = {1e8, -1e9, 2e7};
    EXPECT_DOUBLE_EQ(rendering::cameraExposure(settings(), lights(), moved, moved[1].position + glm::dvec3(-1, 0, 0), 0).exposure,
                     rendering::cameraExposure(settings(), lights(), bodies, {-1, 0, 0}, 0).exposure);
}

TEST(CameraExposure, ParsesOverridesAndRejectsInvalidBoundsAndMeterInputs) {
    const config::LightingConfig parsed{config::Config{nlohmann::json{{"auto_exposure", {
        {"enabled", true}, {"min_exposure", 0.1}, {"max_exposure", 1000},
        {"target_luminance", 0.2}, {"star_exposure", 50}}}}}};
    EXPECT_TRUE(parsed.auto_exposure.enabled);
    EXPECT_DOUBLE_EQ(parsed.auto_exposure.min_exposure, 0.1);
    EXPECT_DOUBLE_EQ(parsed.auto_exposure.max_exposure, 1000);
    EXPECT_DOUBLE_EQ(parsed.auto_exposure.target_luminance, 0.2);
    EXPECT_DOUBLE_EQ(parsed.auto_exposure.star_exposure, 50);
    for (const auto* key : {"min_exposure", "max_exposure", "target_luminance", "star_exposure"}) {
        for (double value : {0.0, -1.0, std::numeric_limits<double>::infinity()})
            EXPECT_ANY_THROW(config::AutoExposureConfig(config::Config(nlohmann::json{{key, value}})));
    }
    EXPECT_ANY_THROW(config::AutoExposureConfig(config::Config(nlohmann::json{{"enabled", 1}})));
    EXPECT_ANY_THROW(config::AutoExposureConfig(config::Config(nlohmann::json{{"max_exposure", 0.01}})));
    EXPECT_THROW(rendering::cameraExposure(settings(), lights(), bodies, {0, 0, 0}, 0), std::invalid_argument);
    EXPECT_THROW(rendering::cameraExposure(settings(), lights(), bodies, {1, 0, 0}, 10), std::invalid_argument);
}

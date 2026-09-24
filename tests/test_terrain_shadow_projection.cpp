#include <gtest/gtest.h>
#include <limits>
#include "config/LightingConfig.h"
#include "rendering/TerrainShadowProjection.h"

TEST(TerrainShadowProjection, EnclosesTerrainForAllSunDirectionsIncludingPoles) {
    for (int latitude = -90; latitude <= 90; latitude += 5) {
        for (int longitude = 0; longitude < 360; longitude += 15) {
            const double lat = glm::radians(double(latitude)), lon = glm::radians(double(longitude));
            const glm::dvec3 direction(std::cos(lat) * std::cos(lon), std::cos(lat) * std::sin(lon), std::sin(lat));
            const auto projection = rendering::terrainShadowProjection(direction, 1.8);
            for (const auto point : {glm::dvec3(0), glm::dvec3(1.8, 0, 0), glm::dvec3(-1.8, 0, 0),
                                    glm::dvec3(0, 1.8, 0), glm::dvec3(0, -1.8, 0),
                                    glm::dvec3(0, 0, 1.8), glm::dvec3(0, 0, -1.8)}) {
                const auto clip = projection * glm::dvec4(point, 1);
                for (int axis = 0; axis < 3; ++axis) {
                    EXPECT_TRUE(std::isfinite(clip[axis]));
                    EXPECT_LT(std::abs(clip[axis]), 1.0);
                }
            }
        }
    }
}

TEST(TerrainShadowProjection, CloserMountainWinsTheSameLightRay) {
    const glm::dvec3 sun = glm::normalize(glm::dvec3(-1, 0, 1));
    const auto projection = rendering::terrainShadowProjection(sun, 2);
    const glm::dvec3 receiver(0.15, 0, 0.075);
    const auto rear = projection * glm::dvec4(receiver, 1);
    const auto front = projection * glm::dvec4(receiver + 0.7 * sun, 1);
    EXPECT_NEAR(front.x, rear.x, 1e-14);
    EXPECT_NEAR(front.y, rear.y, 1e-14);
    EXPECT_LT(front.z, rear.z);
}

TEST(TerrainShadowProjection, LocalSunDirectionFollowsBodySpinAndTilt) {
    const auto rotation = glm::dmat3(glm::rotate(glm::dmat4(1), glm::radians(20.0), glm::dvec3(1, 0, 0)) *
                                    glm::rotate(glm::dmat4(1), glm::radians(75.0), glm::dvec3(0, 0, 1)));
    const glm::dvec3 sun = glm::normalize(glm::dvec3(-1, 0.2, 1));
    const glm::dvec3 localSun = glm::transpose(rotation) * sun;
    const auto projection = rendering::terrainShadowProjection(localSun, 2);
    const auto rear = projection * glm::dvec4(0.15, 0, 0.075, 1);
    const auto front = projection * glm::dvec4(glm::dvec3(0.15, 0, 0.075) + localSun, 1);
    EXPECT_NEAR(front.x, rear.x, 1e-14);
    EXPECT_NEAR(front.y, rear.y, 1e-14);
    EXPECT_LT(front.z, rear.z);
    const auto scaled = rendering::terrainShadowProjection(1000.0 * localSun, 2000) *
                        glm::dvec4(150, 0, 75, 1);
    for (int axis = 0; axis < 3; ++axis) EXPECT_NEAR(scaled[axis], rear[axis], 1e-14);
}

TEST(TerrainShadowProjection, RejectsInvalidDirectionAndExtent) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (double extent : {0.0, -1.0, nan, inf})
        EXPECT_THROW(rendering::terrainShadowProjection({1, 0, 0}, extent), std::invalid_argument);
    for (auto direction : {glm::dvec3(0), glm::dvec3(nan, 0, 1), glm::dvec3(1, inf, 0)})
        EXPECT_THROW(rendering::terrainShadowProjection(direction, 1), std::invalid_argument);
}

TEST(TerrainShadowConfig, DefaultsAndOverrides) {
    const config::LightingConfig defaults(config::Config(nlohmann::json::object()));
    EXPECT_TRUE(defaults.shadows.enabled);
    EXPECT_EQ(defaults.shadows.resolution, 2048);
    EXPECT_DOUBLE_EQ(defaults.shadows.bias_texels, 0.5);
    for (int resolution : {256, 512, 1024, 2048, 4096}) {
        const config::LightingConfig config(config::Config(nlohmann::json{
            {"shadows", {{"enabled", false}, {"resolution", resolution}, {"bias_texels", 0.25}}}}));
        EXPECT_FALSE(config.shadows.enabled);
        EXPECT_EQ(config.shadows.resolution, resolution);
        EXPECT_DOUBLE_EQ(config.shadows.bias_texels, 0.25);
    }
}

TEST(TerrainShadowConfig, RejectsMalformedAndUnboundedSettings) {
    for (auto value : {nlohmann::json(255), nlohmann::json(4097), nlohmann::json(1000),
                       nlohmann::json(1e30), nlohmann::json(512.5), nlohmann::json("512"),
                       nlohmann::json(false), nlohmann::json(nullptr)}) {
        EXPECT_ANY_THROW(config::TerrainShadowConfig(config::Config(nlohmann::json{{"resolution", value}})));
    }
    for (double bias : {-0.1, 4.1, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
        EXPECT_ANY_THROW(config::TerrainShadowConfig(config::Config(nlohmann::json{{"bias_texels", bias}})));
    EXPECT_THROW(config::TerrainShadowConfig(config::Config(nlohmann::json{{"enabled", 1}})), std::invalid_argument);
    EXPECT_THROW(config::TerrainShadowConfig(config::Config(nlohmann::json::array())), std::invalid_argument);
    EXPECT_THROW(config::LightingConfig(config::Config(nlohmann::json{{"shadows", nullptr}})), std::invalid_argument);
}

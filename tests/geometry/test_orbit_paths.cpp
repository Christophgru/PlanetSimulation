#include "config/Config.h"
#include <gtest/gtest.h>
#include "rendering/geometry/OrbitPaths.h"

namespace {
config::ScenarioConfig scene() {
    return config::ScenarioConfig(config::Config{nlohmann::json::parse(R"({
        "sun":{"name":"sun","mass_kg":1e19},
        "planets":[
            {"name":"earth","mass_kg":6e17,"color":[0.2,0.4,0.8],
             "orbit":{"parent":"sun","semi_major_axis":12.5,"semi_minor_axis":10}},
            {"name":"moon","mass_kg":1e16,
             "orbit":{"parent":"earth","semi_major_axis":2.5,"semi_minor_axis":2,
                      "inclination_deg":20}}
        ]
    })")});
}
}

TEST(OrbitPaths, SamplesTenPastRevolutionsOfMovingHierarchy) {
    const auto config = scene();
    const simulation::OrbitalSystem dynamics(config);
    constexpr double epoch = 17.0;
    const auto trails = rendering::pastOrbitTrails(dynamics, epoch);
    ASSERT_EQ(trails.size(), 2);
    for (std::size_t i = 0; i < trails.size(); ++i) {
        ASSERT_EQ(trails[i].positions.size(), 321);
        ASSERT_EQ(trails[i].opacity.size(), 321);
        EXPECT_NEAR(glm::length(trails[i].positions.back() - dynamics.at(epoch)[i+1].position), 0, 1e-9);
        const double oldest = epoch - 10.0 * trails[i].periodSeconds;
        EXPECT_NEAR(glm::length(trails[i].positions.front() - dynamics.at(oldest)[i+1].position), 0, 1e-9);
        EXPECT_NEAR(glm::length(trails[i].positions[312] - dynamics.at(epoch - 0.25 * trails[i].periodSeconds)[i+1].position), 0, 1e-9);
        EXPECT_FLOAT_EQ(trails[i].opacity.front(), 0.0f);
        EXPECT_NEAR(trails[i].opacity[32], 0.5f, 1e-6f);
        EXPECT_FLOAT_EQ(trails[i].opacity[64], 1.0f);
        EXPECT_FLOAT_EQ(trails[i].opacity.back(), 1.0f);
    }
    // The lunar trail must follow Earth's translation rather than replaying
    // the same local ellipse at a fixed world position.
    const auto firstMoonLoop = trails[1].positions[288];
    const auto secondMoonLoop = trails[1].positions[320];
    EXPECT_GT(glm::length(firstMoonLoop - secondMoonLoop), 0.1);
}

TEST(OrbitPaths, SurfaceTintAveragesRenderedVertexColors) {
    const auto config = scene();
    std::vector<float> vertices = {
        0,0,0,0,0,1, 0.5f,0.25f,1.0f,
        0,0,0,0,0,1, 1.0f,0.75f,0.5f
    };
    const auto color = rendering::averageSurfaceColor(config.planets[0], vertices, true);
    EXPECT_NEAR(color.r, 0.15f, 1e-6f);
    EXPECT_NEAR(color.g, 0.20f, 1e-6f);
    EXPECT_NEAR(color.b, 0.60f, 1e-6f);
}

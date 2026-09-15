#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include "rendering/Terrain.h"

namespace {
rendering::TerrainSurface makeTerrain(int seed = 42) {
    config::PlanetConfig::SurfaceNoise noise;
    noise.amplitude_m = 1.0;
    noise.seed = seed;
    return {noise, 0.025, 1000.0};
}

glm::dvec3 vertex(const rendering::TerrainGeometry& geometry, int index) {
    const std::size_t offset = static_cast<std::size_t>(index) * 9;
    return {geometry.vertices[offset], geometry.vertices[offset + 1],
            geometry.vertices[offset + 2]};
}
}

TEST(TerrainTest, NoiseIsDeterministicContinuousAndBoundedByAmplitude) {
    const auto a = makeTerrain();
    const auto same = makeTerrain();
    const auto otherSeed = makeTerrain(43);
    const glm::dvec3 point = glm::normalize(glm::dvec3(0.3, 0.7, -0.4));
    EXPECT_DOUBLE_EQ(a.heightAt(point), same.heightAt(point));
    EXPECT_GT(std::abs(a.heightAt(point) - otherSeed.heightAt(point)), 1e-8);
    EXPECT_LT(std::abs(a.heightAt(point) -
                       a.heightAt(glm::normalize(point + glm::dvec3(1e-5, 0, 0)))),
              0.0001);
    for (int x = -4; x <= 4; ++x) {
        for (int y = -4; y <= 4; ++y) {
            const glm::dvec3 radial = glm::normalize(glm::dvec3(x + 0.3, y + 0.7, 2.1));
            EXPECT_LE(std::abs(a.heightAt(radial)), 0.001);
        }
    }
    EXPECT_THROW(a.heightAt(glm::dvec3(0)), std::invalid_argument);
}

TEST(TerrainTest, CappedSubdivisionsIncreaseTriangleCountByFour) {
    const auto terrain = makeTerrain();
    const auto coarse = terrain.buildGeometry(2);
    const auto near = terrain.buildGeometry(5);
    EXPECT_EQ(coarse.triangleCount(), 320);
    EXPECT_EQ(near.triangleCount(), 20480);
    EXPECT_EQ(near.vertices.size(), static_cast<std::size_t>(near.triangleCount()) * 27);
    EXPECT_EQ(near.indices.size(), static_cast<std::size_t>(near.triangleCount()) * 3);
    EXPECT_THROW(terrain.buildGeometry(6), std::invalid_argument);
}

TEST(TerrainTest, LodGetsDenserNearCameraAndRemainsCapped) {
    const auto terrain = makeTerrain();
    EXPECT_EQ(terrain.lodLevel(1.0), 2);      // Farther than eight diameters.
    EXPECT_EQ(terrain.lodLevel(0.45), 2);
    EXPECT_GT(terrain.lodLevel(0.15), terrain.lodLevel(0.45));
    EXPECT_EQ(terrain.lodLevel(0.025), 5);    // On the 50 m planet.
    EXPECT_EQ(terrain.lodLevel(0.0), 5);
    EXPECT_THROW(terrain.lodLevel(-1.0), std::invalid_argument);
}

TEST(TerrainTest, TrianglesCarrySampledHeightsOutwardNormalsAndFlatElevationTint) {
    const auto terrain = makeTerrain();
    const auto geometry = terrain.buildGeometry(3);
    double lowest = 1e9;
    double highest = -1e9;
    float lowTint = 0.0f;
    float highTint = 0.0f;
    for (int face = 0; face < geometry.triangleCount(); ++face) {
        const int first = face * 3;
        const glm::dvec3 a = vertex(geometry, first);
        const glm::dvec3 b = vertex(geometry, first + 1);
        const glm::dvec3 c = vertex(geometry, first + 2);
        const glm::dvec3 normal = glm::normalize(glm::cross(b - a, c - a));
        EXPECT_GT(glm::dot(normal, a + b + c), 0.0);
        double elevation = 0.0;
        for (int corner = 0; corner < 3; ++corner) {
            const int index = first + corner;
            const auto point = vertex(geometry, index);
            const double sampled = terrain.heightAt(glm::normalize(point));
            EXPECT_NEAR(glm::length(point) * 0.025 - 0.025, sampled, 2e-8);
            const std::size_t offset = static_cast<std::size_t>(index) * 9;
            EXPECT_NEAR(glm::length(glm::dvec3(geometry.vertices[offset + 3],
                                             geometry.vertices[offset + 4],
                                             geometry.vertices[offset + 5]) - normal),
                        0.0, 1e-5);
            for (int channel = 7; channel <= 8; ++channel)
                EXPECT_FLOAT_EQ(geometry.vertices[offset + channel],
                                geometry.vertices[offset + 6]);
            EXPECT_FLOAT_EQ(geometry.vertices[offset + 6],
                            geometry.vertices[static_cast<std::size_t>(first) * 9 + 6]);
            elevation += sampled;
        }
        elevation /= 3.0;
        const float tint = geometry.vertices[static_cast<std::size_t>(first) * 9 + 6];
        if (elevation < lowest) { lowest = elevation; lowTint = tint; }
        if (elevation > highest) { highest = elevation; highTint = tint; }
    }
    EXPECT_LT(lowest, highest);
    EXPECT_LT(lowTint, highTint);
    EXPECT_LT(lowTint, 0.72f);
    EXPECT_GT(highTint, 0.72f);
}

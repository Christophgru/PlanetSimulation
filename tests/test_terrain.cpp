#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include "rendering/Terrain.h"

namespace {
rendering::TerrainSurface makeTerrain(int seed = 42) {
    config::PlanetConfig::SurfaceNoiseFunction noise;
    noise.amplitude_m = 1.0;
    noise.seed = seed;
    return {{noise}, config::PlanetConfig::TerrainLod{}, 0.025, 1000.0};
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

TEST(TerrainTest, OverlappingFunctionsAddTheirIndependentHeightFields) {
    config::PlanetConfig::SurfaceNoiseFunction hills;
    hills.amplitude_m = 0.8;
    hills.frequency = 3.0;
    config::PlanetConfig::SurfaceNoiseFunction ridges;
    ridges.type = "ridged_fbm";
    ridges.amplitude_m = 0.35;
    ridges.frequency = 18.0;
    ridges.seed = 771;
    const config::PlanetConfig::TerrainLod lod;
    const rendering::TerrainSurface hillOnly({hills}, lod, 0.025, 1000.0);
    const rendering::TerrainSurface ridgeOnly({ridges}, lod, 0.025, 1000.0);
    const rendering::TerrainSurface combined({hills, ridges}, lod, 0.025, 1000.0);
    for (const glm::dvec3& point : {glm::dvec3(1, 0, 0),
                                    glm::dvec3(0.2, -0.4, 0.9),
                                    glm::dvec3(-0.8, 0.2, -0.6)}) {
        EXPECT_NEAR(combined.heightAt(point),
                    hillOnly.heightAt(point) + ridgeOnly.heightAt(point), 1e-15);
        EXPECT_LE(std::abs(combined.heightAt(point)), 0.00115);
    }
    EXPECT_NE(hillOnly.heightAt(glm::dvec3(0.2, -0.4, 0.9)),
              ridgeOnly.heightAt(glm::dvec3(0.2, -0.4, 0.9)));
}

TEST(TerrainTest, RejectsInvalidDirectNoiseAndLodSettings) {
    config::PlanetConfig::SurfaceNoiseFunction function;
    function.amplitude_m = -1.0;
    EXPECT_THROW(rendering::TerrainSurface({function},
                 config::PlanetConfig::TerrainLod{}, 0.025, 1000.0),
                 std::invalid_argument);
    function.amplitude_m = 1.0;
    function.octaves = 0;
    EXPECT_THROW(rendering::TerrainSurface({function},
                 config::PlanetConfig::TerrainLod{}, 0.025, 1000.0),
                 std::invalid_argument);
    function.octaves = 4;
    config::PlanetConfig::TerrainLod lod;
    lod.max_edge_segments = 17;
    EXPECT_THROW(rendering::TerrainSurface({function}, lod, 0.025, 1000.0),
                 std::invalid_argument);
}

TEST(TerrainTest, EdgeSegmentsGiveFineDensityStepsWithAHardTriangleCap) {
    const auto terrain = makeTerrain();
    const auto coarse = terrain.buildGeometry(1);
    const auto three = terrain.buildGeometry(3);
    const auto four = terrain.buildGeometry(4);
    const auto near = terrain.buildGeometry(16);
    EXPECT_EQ(coarse.triangleCount(), 320);
    EXPECT_EQ(three.triangleCount(), 2880);
    EXPECT_EQ(four.triangleCount(), 5120);
    EXPECT_LT(static_cast<double>(four.triangleCount()) / three.triangleCount(), 2.0);
    EXPECT_EQ(near.triangleCount(), 81920);
    EXPECT_EQ(near.vertices.size(), static_cast<std::size_t>(near.triangleCount()) * 27);
    EXPECT_EQ(near.indices.size(), static_cast<std::size_t>(near.triangleCount()) * 3);
    EXPECT_THROW(terrain.buildGeometry(17), std::invalid_argument);
}

TEST(TerrainTest, LodGetsDenserNearCameraAndRemainsCapped) {
    const auto terrain = makeTerrain();
    EXPECT_EQ(terrain.lodLevel(1.0), 1);      // Farther than eight diameters.
    EXPECT_EQ(terrain.lodLevel(0.45), 1);
    EXPECT_GT(terrain.lodLevel(0.15), terrain.lodLevel(0.45));
    EXPECT_EQ(terrain.lodLevel(0.025), 16);    // On the 50 m planet.
    EXPECT_EQ(terrain.lodLevel(0.0), 16);
    EXPECT_THROW(terrain.lodLevel(-1.0), std::invalid_argument);
    int last = terrain.lodLevel(0.4);
    std::array<bool, 17> observed{};
    observed[last] = true;
    for (int step = 1; step <= 600; ++step) {
        const double distance = (8.0 - 0.01 * step) * 0.05;
        const int level = terrain.lodLevel(distance);
        EXPECT_GE(level, last);
        EXPECT_LE(level - last, 1);
        observed[level] = true;
        last = level;
    }
    for (int level = 1; level <= 16; ++level) EXPECT_TRUE(observed[level]);
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

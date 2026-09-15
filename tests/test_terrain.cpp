#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <map>
#include <tuple>
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

TEST(TerrainTest, LandscapeSeedChangesBroadTerrainEvenWithFixedSurfaceNoiseSeeds) {
    config::PlanetConfig::TerrainLandscape first;
    first.enabled = true;
    first.continent_amplitude_m = 12.0;
    first.cliff_amplitude_m = 18.0;
    first.seed = 1001;
    auto second = first;
    second.seed = 2002;
    config::PlanetConfig::SurfaceNoiseFunction detail;
    detail.amplitude_m = 0.8;
    detail.seed = 771;
    const config::PlanetConfig::TerrainLod lod;
    const rendering::TerrainSurface a({detail}, lod, 0.1, 1000.0, first);
    const rendering::TerrainSurface b({detail}, lod, 0.1, 1000.0, second);
    double largestDifferenceMeters = 0.0;
    for (const auto& radial : {glm::dvec3(1, 0, 0), glm::dvec3(0.2, 0.8, 0.5),
                               glm::dvec3(-0.6, 0.1, 0.7)}) {
        largestDifferenceMeters = std::max(largestDifferenceMeters,
            1000.0 * std::abs(a.heightAt(radial) - b.heightAt(radial)));
    }
    EXPECT_GT(largestDifferenceMeters, 1.0);
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

TEST(TerrainTest, TrianglesCarrySampledHeightsAndSmoothSharedCornerTint) {
    const auto terrain = makeTerrain();
    const auto geometry = terrain.buildGeometry(3);
    double lowest = 1e9;
    double highest = -1e9;
    float lowTint = 0.0f;
    float highTint = 0.0f;
    int gradientFaces = 0;
    for (int face = 0; face < geometry.triangleCount(); ++face) {
        const int first = face * 3;
        const glm::dvec3 a = vertex(geometry, first);
        const glm::dvec3 b = vertex(geometry, first + 1);
        const glm::dvec3 c = vertex(geometry, first + 2);
        const glm::dvec3 normal = glm::normalize(glm::cross(b - a, c - a));
        EXPECT_GT(glm::dot(normal, a + b + c), 0.0);
        float firstTint = 0.0f;
        float greatestTintDifference = 0.0f;
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
            const float tint = geometry.vertices[offset + 6];
            EXPECT_NEAR(tint, 0.72 + 0.45 * sampled / 0.001, 1e-5);
            if (corner == 0) firstTint = tint;
            else greatestTintDifference = std::max(greatestTintDifference,
                                                   std::abs(tint - firstTint));
            if (sampled < lowest) { lowest = sampled; lowTint = tint; }
            if (sampled > highest) { highest = sampled; highTint = tint; }
        }
        if (greatestTintDifference > 1e-5f) ++gradientFaces;
    }
    EXPECT_GT(gradientFaces, 0);
    // Neighboring faces duplicate vertices, but their shared-edge tints agree.
    EXPECT_NEAR(glm::length(vertex(geometry, 1) - vertex(geometry, 3)), 0.0, 1e-12);
    EXPECT_NEAR(glm::length(vertex(geometry, 2) - vertex(geometry, 5)), 0.0, 1e-12);
    EXPECT_FLOAT_EQ(geometry.vertices[1 * 9 + 6], geometry.vertices[3 * 9 + 6]);
    EXPECT_FLOAT_EQ(geometry.vertices[2 * 9 + 6], geometry.vertices[5 * 9 + 6]);
    EXPECT_LT(lowest, highest);
    EXPECT_LT(lowTint, highTint);
    EXPECT_LT(lowTint, 0.72f);
    EXPECT_GT(highTint, 0.72f);
}

TEST(TerrainTest, DevelopmentLandscapeHasPlainsCliffsLandAndOceanBasins) {
    const auto scenario = config::ScenarioConfig(config::Config::load(
        std::string(PLANET_SOURCE_DIR) + "/configs/scenarios/solar_system.json"));
    const auto& planet = scenario.planets[0];
    const rendering::TerrainSurface terrain(planet.surface_noise, planet.terrain_lod,
        planet.radius, scenario.metersPerWorldUnit(), planet.terrain_landscape);
    int ocean = 0, land = 0, plains = 0, cliffs = 0;
    double plainSlope = 0.0, cliffSlope = 0.0;
    int plainPairs = 0, cliffPairs = 0;
    for (int latitude = -80; latitude <= 80; latitude += 5) {
        for (int longitude = 0; longitude < 360; longitude += 5) {
            const double lat = glm::radians(static_cast<double>(latitude));
            const double lon = glm::radians(static_cast<double>(longitude));
            const glm::dvec3 radial(std::cos(lat) * std::cos(lon),
                                    std::cos(lat) * std::sin(lon), std::sin(lat));
            const double height = terrain.heightAt(radial) * scenario.metersPerWorldUnit();
            if (height < planet.water.level_m) ++ocean;
            else ++land;
            const glm::dvec3 nearby = glm::normalize(radial + glm::dvec3(0.002, 0.001, 0));
            const double slope = std::abs(terrain.heightAt(nearby) *
                scenario.metersPerWorldUnit() - height);
            if (terrain.regionPlainWeight(radial) > 0.85 && height > 0.0) {
                ++plains;
                plainSlope += slope;
                ++plainPairs;
            }
            if (terrain.regionCliffWeight(radial) > 0.75 && height > 0.0) {
                ++cliffs;
                cliffSlope += slope;
                ++cliffPairs;
            }
        }
    }
    EXPECT_GT(ocean, 50);
    EXPECT_GT(land, 50);
    EXPECT_GT(plains, 10);
    EXPECT_GT(cliffs, 10);
    ASSERT_GT(plainPairs, 0);
    ASSERT_GT(cliffPairs, 0);
    EXPECT_GT(cliffSlope / cliffPairs, plainSlope / plainPairs);
}

TEST(TerrainTest, LocalZonesAreDenserWatertightAndStayWithinBudget) {
    const auto scenario = config::ScenarioConfig(config::Config::load(
        std::string(PLANET_SOURCE_DIR) + "/configs/scenarios/solar_system.json"));
    const auto& planet = scenario.planets[0];
    const rendering::TerrainSurface terrain(planet.surface_noise, planet.terrain_lod,
        planet.radius, scenario.metersPerWorldUnit(), planet.terrain_landscape);
    const glm::dvec3 center(planet.position[0], planet.position[1], planet.position[2]);
    const auto distant = terrain.buildGeometryForEye(center + glm::dvec3(2, 0, 0), center);
    const auto orbit = terrain.buildGeometryForEye(center + glm::dvec3(0.28, 0, 0), center);
    const auto nearby = terrain.buildGeometryForEye(center + glm::dvec3(0.102, 0, 0), center);
    EXPECT_EQ(distant.zoneFaces, (std::array<int, 3>{320, 0, 0}));
    EXPECT_EQ(distant.fineNoiseSamples, 0);
    EXPECT_GT(orbit.zoneFaces[2], 0);
    EXPECT_GT(orbit.fineNoiseSamples, 0);
    EXPECT_LE(orbit.triangleCount(), planet.terrain_lod.max_triangle_budget);
    EXPECT_GT(nearby.zoneFaces[1], 0);
    EXPECT_GT(nearby.zoneFaces[2], 0);
    EXPECT_GT(nearby.fineNoiseSamples, 0);
    EXPECT_GT(nearby.triangleCount(), distant.triangleCount());
    EXPECT_LE(nearby.triangleCount(), planet.terrain_lod.max_triangle_budget);
    EXPECT_EQ(nearby.zoneFaces[0] + nearby.zoneFaces[1] + nearby.zoneFaces[2], 320);

    using Position = std::array<int, 3>;
    using Edge = std::pair<Position, Position>;
    std::map<Edge, int> edges;
    auto position = [&](unsigned int index) {
        const auto offset = static_cast<std::size_t>(index) * 9;
        return Position{static_cast<int>(std::lround(nearby.vertices[offset] * 1e6)),
                        static_cast<int>(std::lround(nearby.vertices[offset + 1] * 1e6)),
                        static_cast<int>(std::lround(nearby.vertices[offset + 2] * 1e6))};
    };
    for (std::size_t i = 0; i < nearby.indices.size(); i += 3) {
        const Position corners[3] = {position(nearby.indices[i]),
                                     position(nearby.indices[i + 1]),
                                     position(nearby.indices[i + 2])};
        const glm::dvec3 a = vertex(nearby, nearby.indices[i]);
        const glm::dvec3 b = vertex(nearby, nearby.indices[i + 1]);
        const glm::dvec3 c = vertex(nearby, nearby.indices[i + 2]);
        EXPECT_GT(glm::dot(glm::cross(b - a, c - a), a + b + c), 0.0);
        for (int side = 0; side < 3; ++side) {
            const auto& a = corners[side];
            const auto& b = corners[(side + 1) % 3];
            ++edges[a < b ? Edge{a, b} : Edge{b, a}];
        }
    }
    for (const auto& [edge, occurrences] : edges)
        EXPECT_EQ(occurrences, 2) << "Open or overlapping terrain edge";
}

TEST(TerrainTest, ZoneHysteresisRetainsDetailWhenEyeCrossesABoundary) {
    const auto scenario = config::ScenarioConfig(config::Config::load(
        std::string(PLANET_SOURCE_DIR) + "/configs/scenarios/solar_system.json"));
    const auto& planet = scenario.planets[0];
    const rendering::TerrainSurface terrain(planet.surface_noise, planet.terrain_lod,
        planet.radius, scenario.metersPerWorldUnit(), planet.terrain_landscape);
    const glm::dvec3 center(planet.position[0], planet.position[1], planet.position[2]);
    const glm::dvec3 startEye = center + glm::dvec3(0.102, 0.0, 0.0);
    const glm::dvec3 movedEye = center + 0.102 *
        glm::dvec3(std::cos(0.1), std::sin(0.1), 0.0); // about 10 m along the surface
    const auto first = terrain.buildGeometryForEye(startEye, center);
    const auto withoutHysteresis = terrain.buildGeometryForEye(movedEye, center);
    const auto protectedMesh = terrain.buildGeometryForEye(
        movedEye, center, &first.faceZones, 20.0);
    ASSERT_EQ(first.faceZones.size(), 320u);
    ASSERT_EQ(protectedMesh.faceZones.size(), first.faceZones.size());
    int retainedNearFaces = 0;
    for (std::size_t i = 0; i < first.faceZones.size(); ++i) {
        if (first.faceZones[i] >= 1)
            EXPECT_GE(protectedMesh.faceZones[i], first.faceZones[i]) << i;
        if (first.faceZones[i] == 2 && withoutHysteresis.faceZones[i] < 2 &&
            protectedMesh.faceZones[i] == 2) ++retainedNearFaces;
    }
    EXPECT_GT(retainedNearFaces, 0);
    EXPECT_LE(protectedMesh.triangleCount(), planet.terrain_lod.max_triangle_budget);
    EXPECT_THROW(terrain.buildGeometryForEye(movedEye, center,
        &first.faceZones, -1.0), std::invalid_argument);
}

TEST(TerrainTest, TriangleBudgetDowngradesDistantFineFaces) {
    config::PlanetConfig::TerrainLod lod;
    lod.base_edge_segments = 3;
    lod.medium_edge_segments = 8;
    lod.max_edge_segments = 16;
    lod.near_surface_distance_m = 1000.0;
    lod.mid_surface_distance_m = 1100.0;
    lod.max_triangle_budget = 10000;
    const rendering::TerrainSurface terrain({}, lod, 0.1, 1000.0);
    const auto geometry = terrain.buildGeometryForEye(
        glm::dvec3(10.102, 0, 0), glm::dvec3(10, 0, 0));
    EXPECT_LE(geometry.triangleCount(), 10000);
    EXPECT_GT(geometry.zoneFaces[0], 0);
    EXPECT_GT(geometry.zoneFaces[2], 0);
    EXPECT_EQ(geometry.zoneFaces[0] + geometry.zoneFaces[1] +
              geometry.zoneFaces[2], 320);
}

TEST(TerrainTest, SteepNearbyMeshDoesNotRiseThroughTwoMeterEyeAtFaceCenters) {
    const auto scenario = config::ScenarioConfig(config::Config::load(
        std::string(PLANET_SOURCE_DIR) + "/configs/scenarios/solar_system.json"));
    const auto& planet = scenario.planets[0];
    const rendering::TerrainSurface terrain(planet.surface_noise, planet.terrain_lod,
        planet.radius, scenario.metersPerWorldUnit(), planet.terrain_landscape);
    glm::dvec3 cliffRadial(0.0);
    for (int latitude = -75; latitude <= 75 && glm::length(cliffRadial) == 0.0;
         latitude += 10) {
        for (int longitude = 0; longitude < 360; longitude += 10) {
            const double lat = glm::radians(static_cast<double>(latitude));
            const double lon = glm::radians(static_cast<double>(longitude));
            const glm::dvec3 radial(std::cos(lat) * std::cos(lon),
                                    std::cos(lat) * std::sin(lon), std::sin(lat));
            if (terrain.regionCliffWeight(radial) > 0.8 &&
                terrain.heightAt(radial) > 0.005) {
                cliffRadial = radial;
                break;
            }
        }
    }
    ASSERT_GT(glm::length(cliffRadial), 0.0);
    const glm::dvec3 center(planet.position[0], planet.position[1], planet.position[2]);
    const glm::dvec3 eye = center +
        (planet.radius + terrain.heightAt(cliffRadial) + 0.002) * cliffRadial;
    const auto geometry = terrain.buildGeometryForEye(eye, center);
    double maximumMeshExcessMeters = -1e9;
    int closeFaces = 0;
    for (std::size_t i = 0; i < geometry.indices.size(); i += 3) {
        const glm::dvec3 midpoint = (vertex(geometry, geometry.indices[i]) +
                                     vertex(geometry, geometry.indices[i + 1]) +
                                     vertex(geometry, geometry.indices[i + 2])) / 3.0;
        const glm::dvec3 radial = glm::normalize(midpoint);
        const double arcMeters = planet.radius * scenario.metersPerWorldUnit() *
            std::acos(std::clamp(glm::dot(radial, cliffRadial), -1.0, 1.0));
        if (arcMeters > 10.0) continue;
        ++closeFaces;
        const double meshHeightMeters = (glm::length(midpoint) - 1.0) *
            planet.radius * scenario.metersPerWorldUnit();
        const double sampledHeightMeters = terrain.heightAt(radial) *
            scenario.metersPerWorldUnit();
        maximumMeshExcessMeters = std::max(maximumMeshExcessMeters,
            meshHeightMeters - sampledHeightMeters);
    }
    ASSERT_GT(closeFaces, 0);
    EXPECT_LE(maximumMeshExcessMeters, 2.0);
}

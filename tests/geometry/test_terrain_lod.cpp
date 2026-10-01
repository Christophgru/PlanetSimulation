#include "config/Config.h"
#include "rendering/geometry/Terrain.h"
#include "rendering/geometry/TerrainLod.h"
#include <gtest/gtest.h>
#include <limits>
#include <map>
#include <numeric>
#include <set>

namespace {
glm::dvec3 position(const rendering::TerrainGeometry& mesh, unsigned index) {
    return {mesh.vertices[index*9], mesh.vertices[index*9+1], mesh.vertices[index*9+2]};
}

void expectClosedSingleShell(const rendering::TerrainGeometry& mesh) {
    using Point = std::array<float, 3>;
    using Edge = std::pair<Point, Point>;
    std::map<Edge, int> edges;
    std::set<std::array<Point, 3>> triangles;
    std::set<Point> vertices;
    for (std::size_t t = 0; t < mesh.indices.size(); t += 3) {
        std::array<Point, 3> points;
        for (int i = 0; i < 3; ++i) {
            const auto p = position(mesh, mesh.indices[t+i]);
            points[i] = {float(p.x), float(p.y), float(p.z)};
            vertices.insert(points[i]);
        }
        const auto a = position(mesh, mesh.indices[t]);
        const auto b = position(mesh, mesh.indices[t+1]);
        const auto c = position(mesh, mesh.indices[t+2]);
        ASSERT_GT(glm::dot(glm::cross(b-a, c-a), a+b+c), 0.0);
        for (int i = 0; i < 3; ++i) {
            const auto& a = points[i];
            const auto& b = points[(i+1)%3];
            ++edges[a < b ? Edge{a,b} : Edge{b,a}];
        }
        std::sort(points.begin(), points.end());
        ASSERT_TRUE(triangles.insert(points).second) << "Duplicate terrain face";
    }
    for (const auto& [edge, count] : edges) ASSERT_EQ(count, 2) << "Crack or overlapping edge";
    // A closed spherical triangulation has Euler characteristic 2; stacked
    // closed LOD shells would each contribute another 2.
    EXPECT_EQ(static_cast<long long>(vertices.size()) - static_cast<long long>(edges.size()) +
        static_cast<long long>(triangles.size()), 2);
    EXPECT_EQ(std::accumulate(mesh.lodFaces.begin(), mesh.lodFaces.end(), 0), 320);
}
}

TEST(TerrainLod, EightMonotoneLevelsKeepConfigAnchorsAndExactBoundaries) {
    config::PlanetConfig::TerrainLod lod;
    lod.base_edge_segments = 3;
    const rendering::TerrainLodBands bands(lod);
    EXPECT_EQ(bands.segments, (std::array<int, 8>{3,5,6,8,10,12,14,16}));
    EXPECT_EQ(bands.levelAt(0), 7);
    EXPECT_EQ(bands.levelAt(lod.mid_surface_distance_m), 0);
    for (int level = 1; level < 8; ++level) {
        const double boundary = bands.outerDistanceMeters[level-1];
        EXPECT_EQ(bands.levelAt(boundary), level-1);
        EXPECT_EQ(bands.levelAt(boundary-1e-6), level);
        EXPECT_GE(bands.sinkMeters(bands.segments[level-1], 1.0),
                  bands.sinkMeters(bands.segments[level], 1.0));
    }
    EXPECT_EQ(bands.sinkMeters(16, 1), 0);
    EXPECT_EQ(bands.sinkMeters(32, 1), 0);
    EXPECT_EQ(bands.sinkMeters(3, 1), 1);
    lod.base_edge_segments = lod.medium_edge_segments = lod.max_edge_segments = 4;
    const rendering::TerrainLodBands constant(lod);
    for (int segments : constant.segments) EXPECT_EQ(constant.sinkMeters(segments, 1), 0);
}

TEST(TerrainLod, SinkingIsConfiguredAndValidated) {
    const config::PlanetConfig::TerrainLod lod(config::Config{nlohmann::json{{"sink_depth_m", 2.5}}});
    EXPECT_EQ(lod.sink_depth_m, 2.5);
    for (double invalid : {-1.0, 100.1, std::numeric_limits<double>::infinity()}) {
        auto bad = lod; bad.sink_depth_m = invalid;
        EXPECT_THROW(bad.validate(), std::invalid_argument);
    }
}

TEST(TerrainLod, AllEightLevelsFormOneWatertightShellAtEquatorAndPoles) {
    config::PlanetConfig::TerrainLod lod;
    lod.near_surface_distance_m = 50;
    lod.mid_surface_distance_m = 1600;
    lod.max_triangle_budget = 100000;
    const rendering::TerrainSurface terrain({}, lod, 1, 1000);
    for (const auto& radial : {glm::dvec3(1,0,0), glm::dvec3(0,0,1), glm::dvec3(0,0,-1)}) {
        const glm::dvec3 center(4,-2,3);
        const auto mesh = terrain.buildGeometryForEye(center + radial*1.002, center);
        for (int faces : mesh.lodFaces) EXPECT_GT(faces, 0);
        EXPECT_LE(mesh.triangleCount(), lod.max_triangle_budget);
        expectClosedSingleShell(mesh);
    }
}

TEST(TerrainLod, CoarseVerticesSinkAndFinestVerticesKeepTheirSampledHeight) {
    config::PlanetConfig::TerrainLod lod;
    lod.near_surface_distance_m = 50;
    lod.mid_surface_distance_m = 1600;
    const rendering::TerrainSurface terrain({}, lod, 1, 1000);
    const auto mesh = terrain.buildGeometryForEye({1.002,0,0}, {0,0,0});
    lod.sink_depth_m = 0;
    const rendering::TerrainSurface unsunk({}, lod, 1, 1000);
    const auto control = unsunk.buildGeometryForEye({1.002,0,0}, {0,0,0});
    ASSERT_EQ(mesh.indices, control.indices);
    ASSERT_EQ(mesh.faceZones, control.faceZones);
    ASSERT_EQ(mesh.lodSinkMeters.size()*9, mesh.vertices.size());
    int finest = 0, coarse = 0;
    for (std::size_t v = 0; v < mesh.lodSinkMeters.size(); ++v) {
        const double distance = glm::length(position(control, v)) - glm::length(position(mesh, v));
        EXPECT_NEAR(distance*1000, mesh.lodSinkMeters[v], 0.00015);
        EXPECT_GE(mesh.lodSinkMeters[v], 0);
        EXPECT_LE(mesh.lodSinkMeters[v], 1);
        if (mesh.lodSinkMeters[v] == 0) {
            ++finest;
            EXPECT_EQ(position(mesh, v), position(control, v));
        } else ++coarse;
        if (std::acos(std::clamp(glm::normalize(position(mesh,v)).x, -1.0, 1.0))*1000 < 40)
            EXPECT_EQ(mesh.lodSinkMeters[v], 0) << "Near-camera detail must remain highest";
    }
    EXPECT_GT(finest, 0); EXPECT_GT(coarse, 0);
    const auto distant = terrain.buildGeometryForEye({4,0,0}, {0,0,0});
    EXPECT_EQ(distant.lodFaces[0], 320);
    EXPECT_EQ(distant.triangleCount(), 960);
    for (float sink : distant.lodSinkMeters) EXPECT_EQ(sink, 1);
    expectClosedSingleShell(distant);
}

TEST(TerrainLod, ShoreRefinementPreservesTheSunkShellAndBudget) {
    config::PlanetConfig::TerrainLod lod;
    lod.near_surface_distance_m = 10;
    lod.mid_surface_distance_m = 100;
    lod.max_edge_segments = lod.steep_edge_segments = 8;
    lod.medium_edge_segments = 4;
    lod.shoreline_distance_m = 300;
    lod.shoreline_edge_m = 8;
    const rendering::TerrainSurface terrain({}, lod, 1, 1000, {}, 0.0);
    const auto mesh = terrain.buildGeometryForEye({1.002,0,0}, {0,0,0});
    EXPECT_GT(mesh.shorelineAddedTriangles, 0);
    EXPECT_LE(mesh.triangleCount(), lod.max_triangle_budget);
    expectClosedSingleShell(mesh);
    bool sunkRefinement = false;
    for (std::size_t v = 0; v < mesh.lodSinkMeters.size(); ++v) {
        EXPECT_NEAR((1-glm::length(position(mesh,v)))*1000, mesh.lodSinkMeters[v], 0.0002);
        if (mesh.lodSinkMeters[v] > 0 && mesh.lodSinkMeters[v] < 1) sunkRefinement = true;
    }
    EXPECT_TRUE(sunkRefinement);
}

TEST(TerrainLod, InvalidPreviousLevelsAreRejectedIncludingDistantViews) {
    const rendering::TerrainSurface terrain({}, {}, 1, 1000);
    std::vector<int> zones(320, 8);
    EXPECT_THROW(terrain.buildGeometryForEye({4,0,0}, {0,0,0}, &zones), std::invalid_argument);
    zones.assign(319, 0);
    EXPECT_THROW(terrain.buildGeometryForEye({1.002,0,0}, {0,0,0}, &zones), std::invalid_argument);
}

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

TEST(TerrainLod, ReliefSinkingKeepsNearGeometryAndSharedShorelineEndpoints) {
    auto lod=config::PlanetConfig::TerrainLod{};
    lod.relief_sinking=true;lod.near_surface_distance_m=50;lod.mid_surface_distance_m=1600;
    lod.max_edge_segments=lod.steep_edge_segments=8;lod.medium_edge_segments=4;
    lod.shoreline_distance_m=300;lod.shoreline_edge_m=8;
    config::PlanetConfig::SurfaceNoiseFunction noise;
    noise.amplitude_m=8;noise.frequency=64;noise.octaves=6;
    const rendering::TerrainSurface terrain({noise},lod,1,1000,{},0);
    for(const auto eye:{glm::dvec3(1.002,0,0),glm::dvec3(0,0,1.002),glm::dvec3(0,0,-1.002)}) {
        const auto topology=terrain.buildTopologyForEye(eye,{});
        const auto mesh=terrain.evaluateTopology(topology);
        EXPECT_EQ(topology.generation.topologyVersion,2u);
        EXPECT_LE(mesh.triangleCount(),lod.max_triangle_budget);
        expectClosedSingleShell(mesh);
        int near=0,sunk=0;
        for(const auto& sample:topology.samples) {
            const glm::dvec3 radial(sample.radial[0],sample.radial[1],sample.radial[2]);
            EXPECT_GE(sample.sinkMeters,0);EXPECT_LE(sample.sinkMeters,lod.sink_depth_m);
            if(glm::dot(radial,glm::normalize(eye))>std::cos(.05)) {
                ++near;EXPECT_EQ(sample.sinkMeters,0);
                EXPECT_EQ(terrain.field().heightAt(radial,topology.surfacePolicy),terrain.heightAt(radial));
            }
            if(sample.sinkMeters>0) ++sunk;
        }
        EXPECT_GT(near,0);EXPECT_GT(sunk,0);
    }
}

TEST(TerrainLod, NoiseMorphIsContinuousAtEveryParentBoundaryAndBoundsOmittedRelief) {
    config::PlanetConfig::TerrainLod lod;lod.relief_sinking=true;
    lod.near_surface_distance_m=100;lod.mid_surface_distance_m=1700;
    config::PlanetConfig::SurfaceNoiseFunction noise;
    noise.amplitude_m=.05;noise.frequency=64;noise.octaves=6;noise.lacunarity=4;
    const rendering::TerrainSurface terrain({noise},lod,1,1000);
    const auto topology=terrain.buildTopologyForEye({1.002,0,0},{});
    for(int boundary=0;boundary<=7;++boundary) {
        const double distance=100+1600.0*boundary/7;
        const auto radial=[&](double delta) {const double angle=(distance+delta)/1000;return glm::dvec3(std::cos(angle),std::sin(angle),0);};
        const auto a=radial(-1e-4),b=radial(1e-4);
        EXPECT_NEAR(terrain.field().heightAt(a,topology.surfacePolicy),terrain.field().heightAt(b,topology.surfacePolicy),1e-7);
        for(const auto r:{a,b}) {
            const auto filtered=terrain.field().heightAt(r,topology.surfacePolicy);
            const auto bound=terrain.field().omittedReliefMeters(topology.surfacePolicy.profile(r));
            EXPECT_LE(std::abs(filtered-terrain.heightAt(r))*1000,bound+1e-10);
        }
    }
    const auto fine=topology.surfacePolicy.profile({1,0,0});
    EXPECT_EQ(terrain.field().omittedReliefMeters(fine),0);
    EXPECT_GT(terrain.field().omittedReliefMeters(topology.surfacePolicy.profile({-1,0,0})),.04);
}

TEST(TerrainLod, ReliefSinkScalesWithOmittedAmplitudeAndPreservesLegacyContract) {
    rendering::TerrainSurfacePolicy policy;
    policy.eyeEdge={1,0,0,300};policy.distances={100,1700,1000,1};policy.spacing.fill(10);policy.spacing[7]=0;
    config::PlanetConfig::SurfaceNoiseFunction noise;noise.amplitude_m=.01;noise.frequency=64;
    rendering::PlanetField small({noise},1,1000);
    noise.amplitude_m=.02;rendering::PlanetField large({noise},1,1000);
    const auto profile=policy.profile({-1,0,0});
    EXPECT_NEAR(large.omittedReliefMeters(profile),2*small.omittedReliefMeters(profile),1e-15);
    EXPECT_GT(small.omittedReliefMeters(profile),0);
    const config::PlanetConfig::TerrainLod parsed(config::Config{nlohmann::json{{"relief_sinking",true}}});
    EXPECT_TRUE(parsed.relief_sinking);EXPECT_FALSE(config::PlanetConfig::TerrainLod{}.relief_sinking);
    auto topology=rendering::TerrainSurface({}, {},1,1000).buildTopologyForEye({1.002,0,0},{});
    EXPECT_EQ(topology.generation.topologyVersion,1u);EXPECT_FALSE(topology.surfacePolicy.enabled());
    topology.surfacePolicy=policy;EXPECT_THROW(topology.validate(),std::invalid_argument);
}

TEST(TerrainLod, VaryingSinkContributesToSurfaceNormalsEvenWithoutNoise) {
    rendering::TerrainSurfacePolicy policy;
    policy.eyeEdge={1,0,0,300};policy.distances={100,1700,1000,1};
    policy.spacing.fill(10);policy.spacing[7]=0;
    rendering::PlanetField field({},1,1000);
    const glm::dvec3 radial(std::cos(.2),std::sin(.2),0);
    const auto sampled=field.sample(radial,field.heightAt(radial,policy),nullptr,policy);
    EXPECT_GT(glm::length(sampled.normal-radial),1e-6);
    const auto legacy=field.sample(radial,field.heightAt(radial));
    EXPECT_NEAR(glm::length(legacy.normal-radial),0,1e-12);
}

TEST(TerrainLod, ZeroSinkDepthRetainsFilteringWithoutMovingTheSurfaceInward) {
    config::PlanetConfig::TerrainLod lod;lod.relief_sinking=true;lod.sink_depth_m=0;
    config::PlanetConfig::SurfaceNoiseFunction noise;noise.amplitude_m=8;noise.frequency=64;
    const rendering::TerrainSurface terrain({noise},lod,1,1000);
    const auto topology=terrain.buildTopologyForEye({4,0,0},{});
    bool filtered=false;
    for(const auto& sample:topology.samples) {
        EXPECT_EQ(sample.sinkMeters,0);
        const glm::dvec3 radial(sample.radial[0],sample.radial[1],sample.radial[2]);
        if(std::abs(terrain.field().heightAt(radial,topology.surfacePolicy)-terrain.heightAt(radial))>1e-6) filtered=true;
    }
    EXPECT_TRUE(filtered);EXPECT_EQ(topology.generation.topologyVersion,2u);
}

namespace {
double meanLocalProbeError(const rendering::TerrainSurface& terrain,
    const rendering::TerrainTopology& topology,const glm::dvec3& eye) {
    const auto& field=terrain.field();const double scale=field.radiusWorld()*field.metersPerUnit();
    const auto point=[&](const glm::dvec3& radial,double sink) {
        return radial*(1+(field.heightAt(radial,topology.surfacePolicy)*field.metersPerUnit()-sink)/scale);
    };
    double error=0;int count=0;
    for(std::size_t t=0;t<topology.indices.size();t+=3) {
        std::array<glm::dvec3,3> p;
        for(int i=0;i<3;++i) {
            const auto& s=topology.samples[topology.indices[t+i]];
            p[i]=point({s.radial[0],s.radial[1],s.radial[2]},s.sinkMeters);
        }
        const auto center=(p[0]+p[1]+p[2])/3.0;
        if(glm::length(center-eye)*scale>120) continue;
        // Independent interior probes (not the planner's center/midpoints).
        for(const auto weights:{glm::dvec3(.6,.2,.2),glm::dvec3(.2,.6,.2),glm::dvec3(.2,.2,.6)}) {
            const auto linear=p[0]*weights.x+p[1]*weights.y+p[2]*weights.z;
            error+=glm::length(point(glm::normalize(linear),0)-linear)*scale;++count;
        }
    }
    EXPECT_GT(count,0);return count ? error/count : 1e9;
}
}

TEST(TerrainLod, SurfaceErrorRefinementImprovesUnprobedInteriorHeightsWithinTheSameCap) {
    config::PlanetConfig::TerrainLod lod;lod.relief_sinking=true;
    lod.near_surface_distance_m=200;lod.mid_surface_distance_m=400;lod.max_triangle_budget=10000;
    lod.max_edge_segments=lod.steep_edge_segments=8;lod.medium_edge_segments=4;
    config::PlanetConfig::SurfaceNoiseFunction noise;
    noise.amplitude_m=8;noise.frequency=64;noise.type="ridged_fbm";noise.seed=35;
    const glm::dvec3 radial=glm::normalize(glm::dvec3(1,.1,.2));
    const rendering::TerrainSurface baseline({noise},lod,1,1000);
    const auto eye=radial*(1.002+baseline.heightAt(radial));
    const auto coarse=baseline.buildTopologyForEye(eye,{});
    lod.geometric_error_m=.05;
    const rendering::TerrainSurface refined({noise},lod,1,1000);
    const auto fine=refined.buildTopologyForEye(eye,{});
    EXPECT_GT(fine.errorRefinedTriangles,0);
    EXPECT_LE(fine.triangleCount(),lod.max_triangle_budget);
    const auto fineError=meanLocalProbeError(refined,fine,eye),coarseError=meanLocalProbeError(baseline,coarse,eye);
    RecordProperty("refined_mean_error_m",std::to_string(fineError));
    RecordProperty("legacy_mean_error_m",std::to_string(coarseError));
    EXPECT_LT(fineError,.5*coarseError);
    const rendering::TerrainSurface smooth({},lod,1,1000);
    EXPECT_GT(fine.errorRefinedTriangles,smooth.buildTopologyForEye(radial*1.002,{}).errorRefinedTriangles);
    EXPECT_EQ(fine.generation,refined.buildTopologyForEye(eye,{}).generation);
    expectClosedSingleShell(refined.evaluateTopology(fine));
}

TEST(TerrainLod, ErrorRefinementClosesSharedEdgesAtPolesShoreAndAnExhaustedBudget) {
    config::PlanetConfig::TerrainLod lod;lod.relief_sinking=true;lod.geometric_error_m=.001;
    lod.near_surface_distance_m=100;lod.mid_surface_distance_m=400;lod.max_triangle_budget=10000;
    lod.max_edge_segments=lod.steep_edge_segments=8;lod.medium_edge_segments=4;
    lod.shoreline_edge_m=8;lod.shoreline_distance_m=300;
    const rendering::TerrainSurface terrain({},lod,1,1000,{},0);
    for(const auto eye:{glm::dvec3(0,0,1.002),glm::dvec3(0,0,-1.002),glm::dvec3(1.002,0,0)}) {
        const auto topology=terrain.buildTopologyForEye(eye,{});
        EXPECT_GT(topology.shorelineAddedTriangles,0);
        EXPECT_GT(topology.errorRefinedTriangles,0);
        EXPECT_LE(topology.triangleCount(),lod.max_triangle_budget);
        expectClosedSingleShell(terrain.evaluateTopology(topology));
    }
    lod.geometric_error_m=1e-6;
    config::PlanetConfig::SurfaceNoiseFunction noise;noise.frequency=64;noise.amplitude_m=8;
    const rendering::TerrainSurface rough({noise},lod,1,1000);
    const auto limited=rough.buildTopologyForEye({1.01,0,0},{});
    EXPECT_TRUE(limited.errorBudgetLimited);EXPECT_GT(limited.remainingErrorRatio,1);
    EXPECT_GE(limited.triangleCount(),lod.max_triangle_budget-1);
    const auto orbit=rough.buildTopologyForEye({4,0,0},{});
    EXPECT_EQ(orbit.errorRefinedTriangles,0);EXPECT_FALSE(orbit.errorBudgetLimited);
}

TEST(TerrainLod, SurfaceErrorSettingIsOptInAndRejectsInvalidOrUnfilteredTargets) {
    EXPECT_EQ(config::PlanetConfig::TerrainLod{}.geometric_error_m,0);
    const config::PlanetConfig::TerrainLod parsed(config::Config{nlohmann::json{
        {"relief_sinking",true},{"geometric_error_m",.05}}});
    EXPECT_EQ(parsed.geometric_error_m,.05);
    for(double value:{-1.0,101.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
        auto bad=parsed;bad.geometric_error_m=value;EXPECT_THROW(bad.validate(),std::invalid_argument);
    }
    auto bad=parsed;bad.relief_sinking=false;EXPECT_THROW(bad.validate(),std::invalid_argument);
}

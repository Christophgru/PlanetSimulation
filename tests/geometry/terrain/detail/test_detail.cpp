#include "config/Config.h"
#include "rendering/geometry/Terrain.h"
#include "rendering/geometry/terrain/detail/TerrainMorph.h"
#include "rendering/character/SurfaceContact.h"
#include <gtest/gtest.h>
#include <map>
#include <iostream>
#include <limits>
#include <numbers>
using namespace rendering;
namespace {
config::PlanetConfig::TerrainLod detailLod() {
    config::PlanetConfig::TerrainLod l;
    l.max_edge_segments=l.steep_edge_segments=8;l.medium_edge_segments=4;
    l.max_triangle_budget=30000;l.shoreline_edge_m=0;
    l.relief_sinking=true;l.geometric_error_m=.05;
    l.local_detail_radius_m=.5;l.local_edge_m=.05;l.local_error_m=.01;
    l.local_transition_m=.5;return l;
}
config::PlanetConfig::SurfaceNoiseFunction microNoise() {
    config::PlanetConfig::SurfaceNoiseFunction n;
    n.amplitude_m=.01;n.wavelength_m=.25;n.octaves=1;n.seed=-71;return n;
}
}
TEST(TerrainDetail, PhysicalNoiseUsesMetersAndProtectsLatticePrecision) {
    auto n=microNoise();
    const PlanetField km({n},1,1000),meters({n},1000,1);
    EXPECT_DOUBLE_EQ(km.parameters().noise[0].frequency,4000);
    EXPECT_DOUBLE_EQ(km.parameters().gradient[2],.0125);
    for(auto r:{glm::dvec3(0,0,1),glm::dvec3(0,0,-1),glm::normalize(glm::dvec3(-1,-2,.8))}) {
        EXPECT_DOUBLE_EQ(km.heightAt(r)*1000,meters.heightAt(r));
        const auto a=km.sample(r,km.heightAt(r)),b=meters.sample(r,meters.heightAt(r));
        EXPECT_EQ(a.normal,b.normal);EXPECT_EQ(a.position,b.position);
    }
    n.wavelength_m=.01;n.octaves=6;n.lacunarity=4;
    EXPECT_THROW(PlanetField({n},100,1000),std::invalid_argument);
}
TEST(TerrainDetail, ConfigurationRejectsAmbiguousNoiseAndInvalidLocalTargets) {
    config::Config noise(nlohmann::json{{"amplitude_m",.01},{"wavelength_m",.25},{"octaves",1}});
    EXPECT_DOUBLE_EQ(config::PlanetConfig::SurfaceNoiseFunction(noise,3).wavelength_m,.25);
    config::Config ambiguous(nlohmann::json{{"frequency",8},{"wavelength_m",.25}});
    EXPECT_THROW(config::PlanetConfig::SurfaceNoiseFunction(ambiguous,3),std::invalid_argument);
    auto l=detailLod();EXPECT_NO_THROW(l.validate());
    EXPECT_DOUBLE_EQ(l.rebuildDistanceMeters(),.15);
    l.relief_sinking=false;EXPECT_THROW(l.validate(),std::invalid_argument);
    l=detailLod();l.local_detail_radius_m=std::numeric_limits<double>::infinity();
    EXPECT_THROW(l.validate(),std::invalid_argument);
    l=detailLod();l.local_edge_m=0;EXPECT_THROW(l.validate(),std::invalid_argument);
    auto n=microNoise();n.wavelength_m=-.1;EXPECT_THROW(n.validate(),std::invalid_argument);
    config::Config parsed(nlohmann::json{{"relief_sinking",true},{"geometric_error_m",.05},
        {"local_detail_radius_m",.5},{"local_edge_m",.025},{"local_error_m",.007},{"local_transition_m",1.0}});
    const config::PlanetConfig::TerrainLod settings(parsed);
    EXPECT_DOUBLE_EQ(settings.local_detail_radius_m,.5);EXPECT_DOUBLE_EQ(settings.local_edge_m,.025);
    EXPECT_DOUBLE_EQ(settings.local_error_m,.007);EXPECT_DOUBLE_EQ(settings.local_transition_m,1);
}
TEST(TerrainDetail, MicroReliefNormalsAgreeWithIndependentSmallCentralDifferences) {
    const PlanetField field({microNoise()},1,1000);
    for(int i=0;i<23;++i) {
        const auto r=glm::normalize(glm::dvec3(.137+i*.017,-.53,1));
        const auto a=glm::normalize(glm::cross(glm::dvec3(0,1,0),r)),b=glm::cross(r,a);
        const auto slope=[&](const glm::dvec3& tangent) {
            const double step=.001;
            return (field.heightAt(glm::normalize(r+tangent*(step/1000)))-
                field.heightAt(glm::normalize(r-tangent*(step/1000))))*1000/(2*step);
        };
        const auto oracle=glm::normalize(r-a*slope(a)-b*slope(b));
        const auto normal=field.sample(r,field.heightAt(r)).normal;
        EXPECT_LE(std::acos(std::clamp(glm::dot(oracle,normal),-1.0,1.0)),.02);
    }
}
TEST(TerrainDetail, CentimeterMeshIsClosedAtEquatorAndPoleAndHitsIndependentProbes) {
    const auto l=detailLod();const TerrainSurface s({microNoise()},l,1,1000);
    for(const auto radial:{glm::dvec3(1,0,0),glm::dvec3(0,0,1)}) {
        const auto t=s.buildTopologyForEye(radial*1.002,{});
        EXPECT_EQ(t.generation.topologyVersion,3u);EXPECT_FALSE(t.localDetailLimited);
        EXPECT_GT(t.localDetailTriangles,0);EXPECT_LE(t.localMaxEdgeMeters,l.local_edge_m);
        EXPECT_LE(t.localRemainingErrorRatio,1);EXPECT_LE(t.triangleCount(),l.max_triangle_budget);
        EXPECT_EQ(s.buildTopologyForEye(radial*1.002,{}).generation,t.generation);
        std::map<std::pair<unsigned,unsigned>,int> edges;
        for(std::size_t i=0;i<t.indices.size();i+=3)
            for(int j=0;j<3;++j) ++edges[std::minmax(t.indices[i+j],t.indices[i+(j+1)%3])];
        for(const auto& [edge,count]:edges) ASSERT_EQ(count,2);
        EXPECT_EQ(static_cast<long long>(t.samples.size())-edges.size()+t.triangleCount(),2);
        const auto mesh=s.evaluateTopology(t);
        SurfaceContact contact;contact.bind(mesh.vertices,mesh.indices,1,1000);
        const GroundQuery missing=[](const auto&) -> GroundContact {throw std::logic_error("Missing triangle");};
        const auto east=glm::normalize(glm::cross(radial,glm::dvec3(0,1,0)));
        const auto north=glm::cross(radial,east);
        // Fixed off-center rings, independent of the planner's midpoint probes.
        for(int ring=1;ring<=4;++ring) for(int p=0;p<19;++p) {
            const double angle=(p+.317)*2*std::numbers::pi/19;
            const auto r=glm::normalize(radial+(east*std::cos(angle)+north*std::sin(angle))*(ring*.103/1000));
            const double selected=s.field().heightAt(r,t.surfacePolicy)*1000;
            EXPECT_DOUBLE_EQ(selected,s.field().heightAt(r)*1000); // Unsunk, fully resolved core.
            EXPECT_LE(std::abs(glm::length(contact.sample(r,missing).position)-1000-selected),l.local_error_m);
        }
        // Check physical edges in the actual float render mesh, including slope.
        for(std::size_t i=0;i<mesh.indices.size();i+=3) {
            std::array<glm::dvec3,3> p;
            bool inside=false;
            for(int j=0;j<3;++j) {
                const auto offset=mesh.indices[i+j]*9;
                p[j]={mesh.vertices[offset],mesh.vertices[offset+1],mesh.vertices[offset+2]};
                inside|=glm::length(glm::normalize(p[j])-radial)*1000<l.local_detail_radius_m;
            }
            ASSERT_GT(glm::dot(glm::cross(p[1]-p[0],p[2]-p[0]),p[0]+p[1]+p[2]),0);
            if(inside) for(int j=0;j<3;++j)
                EXPECT_LE(glm::length(p[j]-p[(j+1)%3])*1000,l.local_edge_m+.0002);
        }
    }
}
TEST(TerrainDetail, InfeasibleDetailStaysBoundedAndReportsItsRemainingError) {
    auto l=detailLod();l.max_triangle_budget=10000;l.local_detail_radius_m=5;l.local_edge_m=.025;
    const TerrainSurface s({microNoise()},l,1,1000);
    const auto t=s.buildTopologyForEye({1.002,0,0},{});
    EXPECT_EQ(t.triangleCount(),10000);EXPECT_TRUE(t.errorBudgetLimited);EXPECT_TRUE(t.localDetailLimited);
    EXPECT_GT(t.localMaxEdgeMeters,l.local_edge_m);
    EXPECT_EQ(s.buildTopologyForEye({1.002,0,0},{}).generation,t.generation);
    EXPECT_TRUE(supportedTerrainTopology(1));EXPECT_TRUE(supportedTerrainTopology(2));
    EXPECT_TRUE(supportedTerrainTopology(3));EXPECT_TRUE(supportedTerrainTopology(4));EXPECT_FALSE(supportedTerrainTopology(5));
    auto bad=t;bad.surfacePolicy.spacing[7]=0;
    EXPECT_THROW(s.evaluateTopology(bad),std::invalid_argument);
}

TEST(TerrainDetail, StreamingReusesBoundedParentAndProjectsTransitionToItsTriangles) {
    auto lod=detailLod();lod.local_detail_streaming=true;lod.local_transition_m=.5;
    TerrainSurface surface({microNoise()},lod,1,1000);
    auto previous=surface.buildTopologyForEye({1.002,0,0},{});
    EXPECT_EQ(previous.generation.topologyVersion,4u);EXPECT_FALSE(previous.parentReused);
    for(int step=1;step<=3;++step) {
        const auto t=surface.buildTopologyForEye({1.002,step*.00015,0},{});
        EXPECT_TRUE(t.parentReused);EXPECT_LE(t.triangleCount(),lod.max_triangle_budget);
        EXPECT_EQ(t.morph.size(),t.samples.size());EXPECT_FALSE(t.localDetailLimited);
        EXPECT_EQ(t.parentPolicy,previous.parentPolicy);
        std::map<std::pair<unsigned,unsigned>,int> edges;
        for(std::size_t i=0;i<t.indices.size();i+=3) for(int j=0;j<3;++j)
            ++edges[std::minmax(t.indices[i+j],t.indices[i+(j+1)%3])];
        for(auto [edge,count]:edges) ASSERT_EQ(count,2);
        const auto mesh=surface.evaluateTopology(t);
        SurfaceContact contact;contact.bind(mesh.vertices,mesh.indices,1,1000);
        const auto r=glm::normalize(glm::dvec3(1.002,step*.00015,0));
        EXPECT_LE(std::abs(glm::length(contact.sample(r,{}).position)-1000-surface.field().heightAt(r)*1000),lod.local_error_m);
        for(std::size_t i=0;i<t.samples.size();++i) if(t.morph[i].blend[0]==1) {
            const auto& m=t.morph[i];
            if(m.parent[0]==m.parent[1]) continue;
            const auto a=terrainPosition(surface.field(),m.parent[0],t.parentPolicy);
            const auto b=terrainPosition(surface.field(),m.parent[1],t.parentPolicy);
            const auto c=terrainPosition(surface.field(),m.parent[2],t.parentPolicy);
            const auto p=terrainMorphPosition(surface.field(),t.samples[i],t.surfacePolicy,m,t.parentPolicy);
            const auto n=glm::normalize(glm::cross(b-a,c-a));
            EXPECT_LE(std::abs(glm::dot(glm::dvec3(p)-a,n))*1000,.0002);
        }
        previous=t;
    }
    const auto rebased=surface.buildTopologyForEye({1.002,.04,0},{});
    EXPECT_FALSE(rebased.parentReused);EXPECT_NE(rebased.parentPolicy,previous.parentPolicy);
    EXPECT_TRUE(surface.buildTopologyForEye({1.002,.04,0},{}).parentReused);
    const auto orbit=surface.buildTopologyForEye({5,0,0},{});
    EXPECT_EQ(orbit.generation.topologyVersion,4u);EXPECT_FALSE(orbit.parentReused);
    EXPECT_TRUE(surface.buildTopologyForEye({5,0,0},{}).parentReused);
    auto invalid=rebased;invalid.morph.front().blend[0]=2;
    EXPECT_THROW(invalid.canonicalize(surface.field().fingerprint()),std::invalid_argument);
    invalid=rebased;invalid.parentPolicy.distances[1]=0;
    EXPECT_THROW(invalid.canonicalize(surface.field().fingerprint()),std::invalid_argument);
}

TEST(TerrainDetail, ProbeReuseSurvivesWorkerSnapshotDestructionAndPreservesDeterminism) {
    auto lod=detailLod();lod.local_detail_streaming=true;
    std::optional<TerrainSurface> survivor;
    TerrainTopology first;
    {
        TerrainSurface snapshot({microNoise()},lod,1,1000);
        first=snapshot.buildTopologyForEye({1.002,0,0},{});
        survivor=snapshot;
    }
    const auto repeat=survivor->buildTopologyForEye({1.002,0,0},{});
    EXPECT_EQ(repeat.generation,first.generation);
    EXPECT_EQ(repeat.samples,first.samples);
    EXPECT_EQ(repeat.planningQueries.evaluations,0u);
    EXPECT_GT(repeat.planningQueries.hits,0u);
    const auto moved=survivor->buildTopologyForEye({1.002,.00015,0},{});
    EXPECT_TRUE(moved.parentReused);EXPECT_FALSE(moved.localDetailLimited);
    EXPECT_GT(moved.planningQueries.hits,0u);
}

TEST(TerrainDetail, StreamingProbeCacheRetainsAWidePlanWorkingSetWithoutChangingGeometry) {
    auto lod=detailLod();lod.local_detail_streaming=true;lod.local_detail_radius_m=1.5;
    // The native movement fixture's wider bands put enough triangles in the
    // fine working set to exceed the old two-times-budget cache capacity.
    lod.near_surface_distance_m=300;lod.mid_surface_distance_m=1000;
    const glm::dvec3 eye(-.21005697617230545,.909856724515421,-.36613409503800903);
    TerrainSurface surface({microNoise()},lod,1,1000);
    const auto initial=surface.buildTopologyForEye(eye,{});
    const auto repeated=surface.buildTopologyForEye(eye,{});
    ASSERT_TRUE(repeated.parentReused);
    EXPECT_EQ(repeated.generation,initial.generation);
    EXPECT_EQ(repeated.samples,initial.samples);EXPECT_EQ(repeated.indices,initial.indices);
    EXPECT_EQ(repeated.planningQueries.evaluations,0u);
    EXPECT_GT(repeated.planningQueries.hits,0u);
    const auto east=glm::normalize(glm::cross(eye,glm::dvec3(0,0,1)));
    const auto moved=surface.buildTopologyForEye(eye+east*.00015,{});
    EXPECT_TRUE(moved.parentReused);EXPECT_GT(moved.planningQueries.hits,0u);
    TerrainSurface fresh({microNoise()},lod,1,1000);
    (void)fresh.buildTopologyForEye(eye,{});
    EXPECT_EQ(fresh.buildTopologyForEye(eye+east*.00015,{}).generation,moved.generation);
    std::cout << "DETAIL_PROBE_WORKING_SET first_evaluations=" << initial.planningQueries.evaluations
        << " repeat_evaluations=" << repeated.planningQueries.evaluations
        << " moved_evaluations=" << moved.planningQueries.evaluations
        << " moved_hits=" << moved.planningQueries.hits << '\n';
}

TEST(TerrainDetail, CachedAdjacencyRemainsClosedAcrossTurnsAndMatchesFreshParentReplay) {
    auto lod=detailLod();lod.local_detail_streaming=true;
    const auto anchor=glm::normalize(glm::dvec3(.7,-.2,1))*1.002;
    const auto east=glm::normalize(glm::cross(anchor,glm::dvec3(0,0,1)));
    const auto north=glm::normalize(glm::cross(anchor,east));
    TerrainSurface moving({microNoise()},lod,1,1000);
    const auto initial=moving.buildTopologyForEye(anchor,{});
    EXPECT_EQ(initial.refinementParentEdges*2,3*(initial.triangleCount()-initial.errorRefinedTriangles));
    for(const auto offset:{glm::dvec2(.15,0),glm::dvec2(.3,.15),glm::dvec2(.15,.3),
                          glm::dvec2(-.15,.15),glm::dvec2(0,0)}) {
        const auto eye=anchor+(east*offset.x+north*offset.y)/1000.;
        const auto t=moving.buildTopologyForEye(eye,{});
        ASSERT_TRUE(t.parentReused);ASSERT_FALSE(t.localDetailLimited);
        ASSERT_GT(t.refinementParentEdges,0u);
        ASSERT_LE(t.refinementMutableEdges,std::size_t(lod.max_triangle_budget)*3/2);
        std::map<std::pair<unsigned,unsigned>,int> edges;
        for(std::size_t i=0;i<t.indices.size();i+=3) for(int j=0;j<3;++j)
            ++edges[std::minmax(t.indices[i+j],t.indices[i+(j+1)%3])];
        for(const auto& [edge,count]:edges) ASSERT_EQ(count,2);
        ASSERT_EQ(static_cast<long long>(t.samples.size())-edges.size()+t.triangleCount(),2);
        TerrainSurface replay({microNoise()},lod,1,1000);
        (void)replay.buildTopologyForEye(anchor,{});
        const auto restored=replay.buildTopologyForEye(eye,{});
        EXPECT_EQ(t.generation,restored.generation);
        EXPECT_EQ(t.samples,restored.samples);EXPECT_EQ(t.indices,restored.indices);
        auto corrupt=t;corrupt.morph.front().blend[0]=.371;
        EXPECT_THROW(corrupt.validate(),std::invalid_argument);
    }
}

TEST(TerrainDetail, StreamingRebasesBeforeAShortNearBandFiltersOrSinksTheMovingCore) {
    auto lod=detailLod();lod.local_detail_streaming=true;
    lod.near_surface_distance_m=2;lod.mid_surface_distance_m=20;
    TerrainSurface surface({microNoise()},lod,1,1000);
    (void)surface.buildTopologyForEye({1.002,0,0},{});
    for(double offset:{.15,.3,3.,3.15,6.}) {
        const glm::dvec3 eye(1.002,offset/1000,0);
        const auto t=surface.buildTopologyForEye(eye,{});
        ASSERT_FALSE(t.localDetailLimited);
        std::size_t checked=0;
        for(std::size_t i=0;i<t.samples.size();++i) {
            const auto& s=t.samples[i];const glm::dvec3 radial(s.radial[0],s.radial[1],s.radial[2]);
            if(glm::length(radial-glm::normalize(eye))*1000>=lod.local_detail_radius_m) continue;
            ++checked;
            ASSERT_DOUBLE_EQ(t.morph[i].blend[0],0);
            ASSERT_DOUBLE_EQ(s.sinkMeters,0) << "core moved " << offset << " m";
            ASSERT_DOUBLE_EQ(surface.field().heightAt(radial,t.surfacePolicy),surface.field().heightAt(radial));
        }
        ASSERT_GT(checked,0u);
    }
}

TEST(TerrainDetail, VerticalMovementRebasesTheParentAndPreservesRecordedSnapshots) {
    auto lod=detailLod();lod.local_detail_streaming=true;
    for(const auto units:{glm::dvec2(1,1000),glm::dvec2(1000,1)}) {
        const auto radius=units.x,meters=units.y;
        SCOPED_TRACE(meters);
        const glm::dvec3 high(1.5*radius,0,0),low(1.002*radius,0,0);
        TerrainSurface surface({microNoise()},lod,radius,meters);
        const auto airborne=surface.buildTopologyForEye(high,{});
        // Ground footprint alone does not qualify a high-altitude face for
        // the nearby error domain.
        ASSERT_EQ(airborne.localDetailTriangles,0);
        const auto landed=surface.buildTopologyForEye(low,{});
        EXPECT_FALSE(landed.parentReused);
        EXPECT_NE(landed.parentEye,airborne.parentEye);
        TerrainSurface fresh({microNoise()},lod,radius,meters);
        const auto expected=fresh.buildTopologyForEye(low,{});
        EXPECT_EQ(landed.generation,expected.generation);
        EXPECT_EQ(landed.samples,expected.samples);EXPECT_EQ(landed.indices,expected.indices);
        EXPECT_GT(landed.localDetailTriangles,0);EXPECT_FALSE(landed.localDetailLimited);
        const auto ascending=surface.buildTopologyForEye(high,{});
        EXPECT_FALSE(ascending.parentReused);EXPECT_EQ(ascending.parentEye,airborne.parentEye);
        EXPECT_EQ(ascending.generation,airborne.generation);
        // A saved historical parent still wins for one explicit replay build;
        // subsequent ordinary planning must regain the descent safety rule.
        surface.prepareReplayParent(high);
        const auto recorded=surface.buildTopologyForEye(low,{});
        EXPECT_TRUE(recorded.parentReused);EXPECT_EQ(recorded.parentEye,airborne.parentEye);
        const auto resumed=surface.buildTopologyForEye(low,{});
        EXPECT_FALSE(resumed.parentReused);EXPECT_EQ(resumed.generation,expected.generation);
        std::cout << "DETAIL_DESCENT meters_per_unit=" << meters
            << " high_parent_faces=" << 2*airborne.refinementParentEdges/3
            << " low_parent_faces=" << 2*landed.refinementParentEdges/3 << '\n';
    }
}

TEST(TerrainDetail, RecordedParentsOverrideDistanceOnceAndRebuildTheExactRequestedAnchor) {
    auto lod=detailLod();lod.local_detail_streaming=true;
    lod.near_surface_distance_m=2;lod.mid_surface_distance_m=20;
    TerrainSurface surface({microNoise()},lod,1,1000);
    const glm::dvec3 parent(1.002,0,0),eye(1.002,.003,0);
    surface.prepareReplayParent(parent);
    auto snapshot=surface; // The worker consumes the same recorded parent.
    const auto restored=snapshot.buildTopologyForEye(eye,{});
    EXPECT_TRUE(restored.parentReused);
    EXPECT_EQ(restored.parentEye,(std::array<double,3>{parent.x,parent.y,parent.z}));
    // Historical snapshots retain their original filtered/sunk surface too.
    bool sunk=false;
    for(const auto& s:restored.samples) {
        const glm::dvec3 r(s.radial[0],s.radial[1],s.radial[2]);
        if(glm::length(r-glm::normalize(eye))*1000<lod.local_detail_radius_m) sunk|=s.sinkMeters>0;
    }
    EXPECT_TRUE(sunk);
    const auto normal=surface.buildTopologyForEye(eye,{});
    EXPECT_FALSE(normal.parentReused);EXPECT_NE(normal.generation,restored.generation);
    // Preparing a nearby recorded parent must rebuild exactly, rather than
    // silently retaining a parent already present in the cache.
    const auto nearby=parent+glm::dvec3(0,.00015,0);
    surface.prepareReplayParent(parent);surface.prepareReplayParent(nearby);
    const auto replay=surface.buildTopologyForEye(eye,{});
    EXPECT_EQ(replay.parentEye,(std::array<double,3>{nearby.x,nearby.y,nearby.z}));
    TerrainSurface fresh({microNoise()},lod,1,1000);fresh.prepareReplayParent(nearby);
    EXPECT_EQ(fresh.buildTopologyForEye(eye,{}).generation,replay.generation);
    EXPECT_THROW(surface.prepareReplayParent({0,0,0}),std::invalid_argument);
    auto plain=detailLod();plain.local_detail_streaming=false;
    EXPECT_THROW(TerrainSurface({microNoise()},plain,1,1000).prepareReplayParent(parent),std::invalid_argument);
}

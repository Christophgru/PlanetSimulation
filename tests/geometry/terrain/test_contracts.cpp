#include "rendering/geometry/Terrain.h"
#include <gtest/gtest.h>
#include <bit>
#include <limits>
using namespace rendering;
namespace {
std::uint64_t legacyMeshHash(const TerrainGeometry& g) {
    auto h=14695981039346656037ull;
    for(float v:g.vertices) h=terrainHashWord(h,std::bit_cast<std::uint32_t>(v),4);
    for(unsigned v:g.indices) h=terrainHashWord(h,v,4);
    for(float v:g.lodSinkMeters) h=terrainHashWord(h,std::bit_cast<std::uint32_t>(v),4);
    return h;
}
config::PlanetConfig::TerrainLod fixtureLod() {
    config::PlanetConfig::TerrainLod l;
    l.max_edge_segments=l.steep_edge_segments=8;l.medium_edge_segments=4;
    l.max_triangle_budget=10000;l.shoreline_edge_m=8;l.shoreline_distance_m=300;
    return l;
}
config::PlanetConfig::SurfaceNoiseFunction noise() {
    config::PlanetConfig::SurfaceNoiseFunction n;
    n.seed=-71;n.frequency=8;n.octaves=3;n.amplitude_m=8;n.type="ridged_fbm";return n;
}
}
TEST(TerrainContracts, LegacyMeshBytesRemainExactAtEquatorPolesShoreAndOrbit) {
    // Independently captured from pre-refactor commit 4b1c2ff, before changing
    // the implementation. Covers positions/normals/colors, indices and sinks.
    const std::array<std::array<std::uint64_t,5>,3> hashes{{
        {{0xe8d1d65b08e11115ull,0xe005b00884000bd1ull,0xa0cafd647582fc09ull,0x683781815126fffdull,0xfbeb02868c161eb5ull}},
        {{0x6b86485e8bbacf48ull,0x86001e64721e9302ull,0x6c53d941f1496d80ull,0x82d85320ef3ee7c3ull,0x07766d77aac151aaull}},
        {{0x14fb9eb0be9d3d4cull,0xb4e6c82e62c1fba2ull,0x2b078b4e36352e23ull,0xde9f9c0078b4e516ull,0xa233431c67f13752ull}}}};
    for(int kind=0;kind<3;++kind) {
        const double radius=kind==2?.27:1;
        const TerrainSurface t(kind==0?std::vector<config::PlanetConfig::SurfaceNoiseFunction>{}:std::vector{noise()},
            fixtureLod(),radius,1000,{},kind==0?std::optional<double>(0):std::nullopt);
        int i=0;
        for(auto radial:{glm::dvec3(1.002,0,0),glm::dvec3(0,0,1.002),glm::dvec3(0,0,-1.002),glm::dvec3(4,0,0)})
            EXPECT_EQ(legacyMeshHash(t.buildGeometryForEye(radial*radius,{0,0,0})),hashes[kind][i++]);
        EXPECT_EQ(legacyMeshHash(t.buildGeometry(3)),hashes[kind][4]);
    }
}
TEST(TerrainContracts, TopologyIsRadialIndexedAndSeparatelyEvaluated) {
    const TerrainSurface t({},fixtureLod(),1,1000,{},0);
    const auto plan=t.buildTopologyForEye({1.002,0,0},{0,0,0});
    ASSERT_TRUE(plan.planningPositions.empty());
    EXPECT_LT(plan.samples.size(),plan.indices.size());
    EXPECT_EQ(plan.topologyInputBytes,32*plan.samples.size()+4*plan.indices.size());
    EXPECT_GT(plan.planningQueries.requests,0u);
    const auto g=t.evaluateTopology(plan);
    EXPECT_EQ(g.generation,plan.generation);
    EXPECT_EQ(g.evaluationQueries.requests,3*plan.samples.size());
    EXPECT_EQ(g.vertices.size(),9*plan.indices.size());
    EXPECT_EQ(legacyMeshHash(g),0xe8d1d65b08e11115ull);
    EXPECT_EQ(t.buildTopologyForEye({1.002,0,0},{0,0,0}).generation,plan.generation);
}
TEST(TerrainContracts, ParameterPackingCarriesAllBitsAndFieldChangesInvalidateTopology) {
    auto n=noise();n.seed=std::numeric_limits<int>::min();
    const TerrainSurface t({n},fixtureLod(),1,1000);
    const auto& p=t.field().parameters();
    EXPECT_EQ(p.flags[0],PlanetField::version);EXPECT_EQ(p.flags[1],1u);
    EXPECT_EQ(p.noise[0].seed,0x80000000u);EXPECT_EQ(p.noise[0].type,1u);
    EXPECT_EQ(p.scale[0],1);EXPECT_EQ(p.scale[1],1000);EXPECT_EQ(p.scale[2],8);
    EXPECT_EQ(p.noise[1].frequency,0);EXPECT_EQ(p.noise[0].padding,(std::array<double,2>{}));
    const auto plan=t.buildTopology(1);
    const TerrainSurface identical({n},fixtureLod(),1,1000);
    EXPECT_EQ(identical.field().fingerprint(),t.field().fingerprint());
    ++n.seed;const TerrainSurface changed({n},fixtureLod(),1,1000);
    EXPECT_NE(changed.field().fingerprint(),t.field().fingerprint());
    EXPECT_THROW(changed.evaluateTopology(plan),std::invalid_argument);
    EXPECT_EQ(identical.evaluateTopology(plan).generation,plan.generation);
}
TEST(TerrainContracts, SparseQueriesRemainBoundedExactAndFieldLocal) {
    const PlanetField f({noise()},1,1000);
    TerrainQueryCache q(f,2);
    const auto a=glm::normalize(glm::dvec3(-.7,.2,.5));
    EXPECT_EQ(q.heightAt(a),f.heightAt(a));EXPECT_EQ(q.heightAt(a),f.heightAt(a));
    EXPECT_EQ(q.stats().requests,2u);EXPECT_EQ(q.stats().evaluations,1u);EXPECT_EQ(q.stats().hits,1u);
    q.heightAt({0,0,1});q.heightAt({0,0,-1});
    EXPECT_LE(q.size(),2u);EXPECT_EQ(q.heightAt(a),f.heightAt(a));
    const PlanetField other({},1,1000);
    EXPECT_THROW(other.sample(a,0,&q),std::invalid_argument);
    EXPECT_THROW(q.heightAt({0,0,0}),std::invalid_argument);
    TerrainQueryCache uncached(f,0);uncached.heightAt(a);uncached.heightAt(a);
    EXPECT_EQ(uncached.stats().hits,0u);EXPECT_EQ(uncached.size(),0u);
    PlanetField replaced({noise()},1,1000);TerrainQueryCache reload(replaced);
    const double previous=reload.heightAt(a);
    replaced=PlanetField({},1,1000);
    EXPECT_NE(previous,reload.heightAt(a));EXPECT_EQ(reload.heightAt(a),0);
    EXPECT_EQ(reload.stats().evaluations,2u);
}
TEST(TerrainContracts, UnitChangesPreserveSiHeightAndGradientAtPoles) {
    const PlanetField km({noise()},1,1000), metres({noise()},1000,1);
    for(auto r:{glm::dvec3(0,0,1),glm::dvec3(0,0,-1),glm::normalize(glm::dvec3(-1,-2,.8))}) {
        const double h=km.heightAt(r);
        EXPECT_DOUBLE_EQ(h*1000,metres.heightAt(r));
        const auto a=km.sample(r,h),b=metres.sample(r,metres.heightAt(r));
        EXPECT_EQ(a.position,b.position);EXPECT_EQ(a.normal,b.normal);EXPECT_EQ(a.color,b.color);
    }
}
TEST(TerrainContracts, HighestNoiseFrequencyAndSeedOverflowStayFiniteAndDeterministic) {
    auto n=noise();n.frequency=64;n.lacunarity=4;n.octaves=6;n.seed=std::numeric_limits<int>::max();
    config::PlanetConfig::TerrainLandscape l;l.enabled=true;l.seed=std::numeric_limits<int>::max();
    l.continent_amplitude_m=10;l.cliff_amplitude_m=20;
    const PlanetField f({n},1,1000,l);
    for(auto r:{glm::dvec3(-1,0,0),glm::dvec3(0,0,1),glm::normalize(glm::dvec3(-.5,.4,-.8))}) {
        const double h=f.heightAt(r);EXPECT_TRUE(std::isfinite(h));EXPECT_EQ(h,f.heightAt(r));
        EXPECT_TRUE(std::isfinite(f.sample(r,h).normal.x));
    }
}
TEST(TerrainContracts, CorruptTopologyAndUnknownBackendAreRejected) {
    const TerrainSurface t({},fixtureLod(),1,1000);const auto good=t.buildTopology(1);
    auto bad=good;bad.indices[0]=bad.samples.size();EXPECT_THROW(t.evaluateTopology(bad),std::invalid_argument);
    bad=good;bad.samples[0].sinkMeters=-1;EXPECT_THROW(t.evaluateTopology(bad),std::invalid_argument);
    bad=good;bad.samples[0].radial[0]=std::numeric_limits<double>::quiet_NaN();EXPECT_THROW(t.evaluateTopology(bad),std::invalid_argument);
    bad=good;bad.generation.backend=static_cast<TerrainBackend>(99);EXPECT_THROW(t.evaluateTopology(bad),std::invalid_argument);
    bad=good;++bad.generation.topologyVersion;EXPECT_THROW(t.evaluateTopology(bad),std::invalid_argument);
    bad=good;bad.samples[0].sinkMeters+=.5;EXPECT_THROW(t.evaluateTopology(bad),std::invalid_argument);
    bad=good;std::swap(bad.indices[0],bad.indices[1]);EXPECT_THROW(t.evaluateTopology(bad),std::invalid_argument);
}

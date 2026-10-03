#include "rendering/character/SurfaceContact.h"
#include "rendering/geometry/contacts/SparseTerrainContacts.h"
#include "rendering/geometry/Terrain.h"
#include "config/Config.h"
#include <gtest/gtest.h>
#include <iostream>
#include <limits>

using namespace rendering;
namespace {
config::PlanetConfig::TerrainLod lod() {
    config::PlanetConfig::TerrainLod l;l.max_triangle_budget=10000;
    l.max_edge_segments=l.steep_edge_segments=8;l.medium_edge_segments=4;
    l.shoreline_edge_m=8;l.shoreline_distance_m=300;return l;
}
const GroundQuery missing=[](const glm::dvec3&) -> GroundContact {
    throw std::logic_error("Closed contact mesh lost coverage");
};
std::vector<glm::dvec3> directions(int count) {
    std::vector<glm::dvec3> result{{0,0,1},{0,0,-1},{1,0,0},{-1,0,0},{0,1,0},{0,-1,0}};
    const double angle=std::acos(-1.0)*(3-std::sqrt(5.0));
    for(int i=0;i<count;++i) {
        const double z=1-2*(i+.5)/count,r=std::sqrt(1-z*z);
        result.push_back({r*std::cos(angle*i),r*std::sin(angle*i),z});
    }
    return result;
}
}
TEST(SparseTerrainContacts, MatchesRenderedPlanesAcrossPolesLodShorelineSinksAndNoiseScales) {
    for(int kind=0;kind<4;++kind) {
        SCOPED_TRACE(kind);
        config::PlanetConfig::SurfaceNoiseFunction noise;noise.amplitude_m=8;noise.frequency=64;
        noise.octaves=6;noise.lacunarity=4;noise.seed=std::numeric_limits<int>::min();
        noise.type=kind%2 ? "ridged_fbm" : "value_fbm";
        const TerrainSurface surface(kind==0 ? std::vector<config::PlanetConfig::SurfaceNoiseFunction>{} : std::vector{noise},
            lod(),kind==2 ? .27 : kind==3 ? 1000 : 1,kind==3 ? 1 : 1000,{},0);
        auto topology=surface.buildTopologyForEye({surface.field().radiusWorld()*1.002,0,0},{0,0,0});
        const auto full=surface.evaluateTopology(topology);
        auto source=std::make_shared<SparseTerrainContacts>(surface.field(),topology);
        EXPECT_EQ(source->stats().heightEvaluations,0u);EXPECT_EQ(source->stats().residentPositions,0u);
        SurfaceContact legacy,sparse;legacy.bind(full.vertices,full.indices,1,source->radiusMeters());sparse.bind(source,1);
        auto points=directions(192);
        // Exact vertices, near shared edges and the local refined shore.
        for(std::size_t i=0;i<topology.samples.size();i+=211) {
            const auto& r=topology.samples[i].radial;points.push_back({r[0],r[1],r[2]});
        }
        for(const auto& p:points) {
            const auto expected=legacy.sample(p,missing),actual=sparse.sample(p,missing);
            EXPECT_EQ(actual.position,expected.position);EXPECT_EQ(actual.normal,expected.normal);
        }
        // Positions are rounded exactly once before and once after sinking.
        for(unsigned triangle=0;triangle<full.triangleCount();triangle+=137) {
            const auto positions=source->triangle(triangle);
            for(int j=0;j<3;++j) {
                const auto offset=std::size_t(triangle*3+j)*9;
                EXPECT_EQ(positions[j],glm::dvec3(full.vertices[offset],full.vertices[offset+1],full.vertices[offset+2]));
            }
        }
        EXPECT_LE(source->stats().residentPositions,SparseTerrainContacts::positionCapacity);
    }
}
TEST(SparseTerrainContacts, ProductionQueriesAreIndexedLazyBoundedAndExtendAcrossThePlanet) {
    const config::ScenarioConfig config(config::Config::load("configs/scenarios/solar_system.json"));
    const auto& p=config.planets.front();
    const TerrainSurface surface(p.surface_noise,p.terrain_lod,p.radius,config.metersPerWorldUnit(),
        p.terrain_landscape,p.water.level_m,p.terrain_material);
    auto topology=surface.buildTopologyForEye({p.radius*1.012,0,0},{0,0,0});
    ASSERT_EQ(topology.triangleCount(),100000);
    auto source=std::make_shared<SparseTerrainContacts>(surface.field(),std::move(topology));
    EXPECT_EQ(source->stats().heightEvaluations,0u);
    SurfaceContact ground;ground.bind(source,1);
    for(const auto& direction:directions(256)) {
        const auto contact=ground.sample(direction,missing);
        EXPECT_GT(glm::length(contact.position),source->radiusMeters()*.5);
    }
    const auto distributed=source->stats();
    EXPECT_GT(distributed.heightEvaluations,SparseTerrainContacts::positionCapacity);
    EXPECT_LT(distributed.heightEvaluations,50002u/2);
    EXPECT_LT(distributed.candidateTriangles,distributed.queries*512);
    EXPECT_LT(distributed.nodesVisited,distributed.queries*1024);
    EXPECT_LE(distributed.residentPositions,SparseTerrainContacts::positionCapacity);
    ground.sample({1,0,0},missing);const auto resting=source->stats();
    for(int i=0;i<100;++i) ground.sample({1,0,0},missing);
    EXPECT_EQ(source->stats().heightEvaluations,resting.heightEvaluations);
    EXPECT_EQ(source->stats().queries,resting.queries);
    std::cout << "SPARSE_CONTACT triangles=100000 queries=" << distributed.queries
        << " height_evaluations=" << distributed.heightEvaluations << " resident=" << distributed.residentPositions
        << " candidates=" << distributed.candidateTriangles << " nodes=" << distributed.nodesVisited
        << " topology_bytes=" << distributed.topologyBytes << " index_bytes=" << distributed.indexBytes
        << " build_ms=" << distributed.buildMilliseconds << '\n';
}
TEST(SparseTerrainContacts, RejectsStaleMalformedAndCollapsedContractsBeforeBinding) {
    const TerrainSurface surface({},lod(),1,1000),other({},lod(),1,2000);
    const auto topology=surface.buildTopology(1);
    EXPECT_THROW(SparseTerrainContacts(other.field(),topology),std::invalid_argument);
    auto malformed=topology;malformed.indices[0]=malformed.samples.size();
    EXPECT_THROW(SparseTerrainContacts(surface.field(),malformed),std::invalid_argument);
    auto stale=topology;stale.samples[0].sinkMeters+=1;
    EXPECT_THROW(SparseTerrainContacts(surface.field(),stale),std::invalid_argument);
    auto collapsed=topology;for(auto& sample:collapsed.samples) sample.sinkMeters=1000;
    collapsed.canonicalize(surface.field().fingerprint());
    EXPECT_THROW(SparseTerrainContacts(surface.field(),collapsed),std::invalid_argument);
    auto source=std::make_shared<SparseTerrainContacts>(surface.field(),topology);
    SurfaceContact ground;ground.bind(source,1);const auto expected=ground.sample({1,0,0},missing);
    EXPECT_THROW(ground.bind(nullptr,2),std::invalid_argument);
    EXPECT_EQ(ground.sample({1,0,0},missing).position,expected.position);
    EXPECT_THROW(ground.sample({0,0,0},missing),std::invalid_argument);
    EXPECT_THROW(source->candidates({2,0,0}),std::invalid_argument);
    EXPECT_THROW(source->triangle(topology.triangleCount()),std::out_of_range);
    auto partial=topology;partial.indices.resize(3);partial.canonicalize(surface.field().fingerprint());
    auto uncovered=std::make_shared<SparseTerrainContacts>(surface.field(),partial);
    ground.bind(uncovered,2);
    glm::dvec3 opposite(0);
    for(const auto& sample:partial.samples) opposite-=glm::dvec3(sample.radial[0],sample.radial[1],sample.radial[2]);
    EXPECT_THROW(ground.sample(opposite,[](const auto& r) {return GroundContact{r*999.0,r};}),std::runtime_error);
}
TEST(SparseTerrainContacts, RebindingAndClearingReleasePreviousGenerationAndInvalidateCachedTriangles) {
    const TerrainSurface surface({},lod(),1,1000);
    auto topology=surface.buildTopology(1);
    auto first=std::make_shared<SparseTerrainContacts>(surface.field(),topology);
    SurfaceContact ground;ground.bind(first,1);
    const auto before=ground.sample({1,0,0},missing);
    std::weak_ptr<SparseTerrainContacts> old=first;first.reset();EXPECT_FALSE(old.expired());
    for(auto& sample:topology.samples) sample.sinkMeters=2;
    topology.canonicalize(surface.field().fingerprint());
    auto replacement=std::make_shared<SparseTerrainContacts>(surface.field(),topology);
    ground.bind(replacement,2);EXPECT_TRUE(old.expired());
    EXPECT_LT(glm::length(ground.sample({1,0,0},missing).position),glm::length(before.position)-1.9);
    std::weak_ptr<SparseTerrainContacts> installed=replacement;replacement.reset();ground.clear();EXPECT_TRUE(installed.expired());
    EXPECT_EQ(ground.sample({0,0,1},[](const auto& r){return GroundContact{r*900.0,r};}).position,glm::dvec3(0,0,900));
}

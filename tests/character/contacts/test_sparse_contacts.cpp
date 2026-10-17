#include "rendering/character/SurfaceContact.h"
#include "rendering/geometry/contacts/SparseTerrainContacts.h"
#include "rendering/geometry/Terrain.h"
#include "rendering/geometry/terrain/detail/TerrainMorph.h"
#include "config/Config.h"
#include <gtest/gtest.h>
#include <algorithm>
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
TEST(SparseTerrainContacts, PartialMeshesMatchExhaustiveContactsAcrossMedianAndLeafBoundaries) {
    const TerrainSurface surface({},lod(),1,1000);
    const auto shell=surface.buildTopology(1);
    for(const unsigned count:{1,7,8,9,15,16,17,31,32,33}) {
        SCOPED_TRACE(count);
        auto topology=shell;topology.indices.clear();
        // Distribute disconnected triangles across axes and hemispheres. Odd
        // sizes exercise nonzero partition offsets and unequal child ranges.
        for(unsigned i=0;i<count;++i) {
            const auto triangle=(37*i)%shell.triangleCount();
            for(int j=0;j<3;++j) topology.indices.push_back(shell.indices[3*triangle+j]);
        }
        topology.canonicalize(surface.field().fingerprint());
        const auto mesh=surface.evaluateTopology(topology);
        auto source=std::make_shared<SparseTerrainContacts>(surface.field(),topology);
        SurfaceContact exhaustive,indexed;
        exhaustive.bind(mesh.vertices,mesh.indices,1,1000);indexed.bind(source,1);
        for(unsigned triangle=0;triangle<count;++triangle) {
            glm::dvec3 center(0);
            for(int j=0;j<3;++j) {
                const auto& r=topology.samples[topology.indices[3*triangle+j]].radial;
                center+=glm::dvec3(r[0],r[1],r[2]);
            }
            const auto direction=glm::normalize(center);
            const auto candidates=source->candidates(direction);
            EXPECT_TRUE(std::binary_search(candidates.begin(),candidates.end(),triangle));
            const auto expected=exhaustive.sample(direction,missing),actual=indexed.sample(direction,missing);
            EXPECT_EQ(actual.position,expected.position);EXPECT_EQ(actual.normal,expected.normal);
        }
    }
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
TEST(SparseTerrainContacts, StreamingParentIndexMatchesFreshIndexAndSurvivesRebaseAndSnapshotDestruction) {
    auto settings=lod();settings.shoreline_edge_m=0;settings.max_triangle_budget=30000;
    settings.relief_sinking=true;settings.geometric_error_m=.05;settings.local_detail_streaming=true;
    settings.local_detail_radius_m=.5;settings.local_edge_m=.05;settings.local_error_m=.01;settings.local_transition_m=.5;
    config::PlanetConfig::SurfaceNoiseFunction noise;noise.amplitude_m=.01;noise.wavelength_m=.25;
    noise.octaves=1;noise.seed=-71;
    std::shared_ptr<SparseTerrainContacts> survivor;
    glm::dvec3 savedDirection;
    GroundContact savedContact;
    {
        TerrainSurface surface({noise},settings,1,1000);
        const auto anchor=glm::normalize(glm::dvec3(.7,-.2,1));
        const auto east=glm::normalize(glm::cross(anchor,glm::dvec3(0,0,1)));
        const auto north=glm::normalize(glm::cross(anchor,east));
        for(const auto step:{glm::dvec2(0,0),glm::dvec2(.15,.15),glm::dvec2(-.3,.15),glm::dvec2(40,0)}) {
            const auto eye=anchor*1.002+(east*step.x+north*step.y)/1000.;
            const auto topology=surface.buildTopologyForEye(eye,{});
            auto source=std::make_shared<SparseTerrainContacts>(surface.field(),topology);
            ASSERT_GT(source->stats().parentTriangles,0u);
            ASSERT_GT(source->stats().sharedParentBytes,0u);
            ASSERT_LT(source->stats().localTriangles,std::size_t(topology.triangleCount()));
            ASSERT_EQ(source->stats().heightEvaluations,0u);
            auto uncached=topology;
            // Public re-canonicalization invalidates derived acceleration, even
            // when all render data stays identical. This is the independent
            // whole-topology index path, with exactly the same render contract.
            uncached.canonicalize(surface.field().fingerprint());
            ASSERT_EQ(uncached.generation,topology.generation);
            auto fresh=std::make_shared<SparseTerrainContacts>(surface.field(),uncached);
            ASSERT_EQ(fresh->stats().parentTriangles,0u);
            SurfaceContact reused,reference;
            reused.bind(source,1);reference.bind(fresh,1);
            auto points=directions(48);
            const auto radial=glm::normalize(eye);
            for(int ring=0;ring<5;++ring) for(int i=0;i<17;++i) {
                const double angle=(i+.317)*2*std::acos(-1.)/17;
                points.push_back(glm::normalize(radial+(east*std::cos(angle)+north*std::sin(angle))*(ring*.317/1000)));
            }
            for(std::size_t i=0;i<topology.samples.size();i+=127) {
                const auto& r=topology.samples[i].radial;points.push_back({r[0],r[1],r[2]});
            }
            for(const auto direction:points) {
                const auto expected=reference.sample(direction,missing),actual=reused.sample(direction,missing);
                EXPECT_EQ(actual.position,expected.position);EXPECT_EQ(actual.normal,expected.normal);
                const auto candidates=source->candidates(direction);
                EXPECT_TRUE(std::is_sorted(candidates.begin(),candidates.end()));
                EXPECT_EQ(std::adjacent_find(candidates.begin(),candidates.end()),candidates.end());
            }
            if(!survivor) {
                survivor=source;savedDirection=radial;savedContact=reused.sample(radial,missing);
            }
            // A manually resealed changed contract must also bypass stale
            // acceleration without relying on canonicalization to clear it.
            auto changed=topology;
            std::rotate(changed.indices.begin(),changed.indices.begin()+3,changed.indices.end());
            changed.generation.topology=changed.contentFingerprint();
            const SparseTerrainContacts resealed(surface.field(),changed);
            EXPECT_EQ(resealed.stats().parentTriangles,0u);
        }
    }
    SurfaceContact retained;retained.bind(survivor,7);
    const auto after=retained.sample(savedDirection,missing);
    EXPECT_EQ(after.position,savedContact.position);EXPECT_EQ(after.normal,savedContact.normal);
}
TEST(SparseTerrainContacts, ParentIndexAloneKeepsCoverageWhenTheBaseConsumesTheWholeBudget) {
    auto settings=lod();
    settings.base_edge_segments=settings.medium_edge_segments=settings.max_edge_segments=settings.steep_edge_segments=4;
    settings.max_triangle_budget=11520;settings.shoreline_edge_m=0;
    settings.relief_sinking=true;settings.geometric_error_m=.05;settings.local_detail_streaming=true;
    settings.local_detail_radius_m=.5;settings.local_edge_m=.05;settings.local_error_m=.01;settings.local_transition_m=.5;
    const TerrainSurface surface({},settings,1,1000);
    const auto topology=surface.buildTopologyForEye({1.002,0,0},{});
    ASSERT_EQ(topology.triangleCount(),settings.max_triangle_budget);
    const auto mesh=surface.evaluateTopology(topology);
    auto source=std::make_shared<SparseTerrainContacts>(surface.field(),topology);
    EXPECT_EQ(source->stats().parentTriangles,11520u);EXPECT_EQ(source->stats().localTriangles,0u);
    SurfaceContact exhaustive,indexed;
    exhaustive.bind(mesh.vertices,mesh.indices,1,1000);indexed.bind(source,1);
    for(const auto direction:directions(96)) {
        const auto expected=exhaustive.sample(direction,missing),actual=indexed.sample(direction,missing);
        EXPECT_EQ(actual.position,expected.position);EXPECT_EQ(actual.normal,expected.normal);
    }
}
TEST(SparseTerrainContacts, SharedDeformationMatchesRenderedPlanesWithoutChangingStaticSnapshots) {
    auto settings=lod();settings.shoreline_edge_m=0;settings.max_triangle_budget=100000;
    settings.relief_sinking=true;settings.geometric_error_m=.05;settings.local_detail_streaming=true;
    settings.local_detail_radius_m=1.5;settings.local_edge_m=.05;settings.local_error_m=.01;settings.local_transition_m=.5;
    settings.near_surface_distance_m=300;settings.mid_surface_distance_m=1000;
    config::PlanetConfig::SurfaceNoiseFunction noise;noise.amplitude_m=.01;noise.wavelength_m=.25;
    noise.octaves=1;noise.seed=-71;
    const TerrainSurface surface({noise},settings,1,1000);
    const auto anchor=glm::normalize(glm::dvec3(.7,-.2,1));
    const auto east=glm::normalize(glm::cross(anchor,glm::dvec3(0,0,1)));
    const auto north=glm::normalize(glm::cross(anchor,east));
    const auto topology=surface.buildTopologyForEye(anchor*1.002,{});
    ASSERT_FALSE(topology.localDetailLimited);
    auto source=std::make_shared<SparseTerrainContacts>(surface.field(),topology);
    auto snapshot=SparseTerrainContacts::topologySnapshot(source);
    const auto fingerprint=snapshot->contentFingerprint();
    const auto staticTriangle=source->triangle(0);
    auto phase=std::make_shared<TerrainMorphState>();
    SurfaceContact shared;shared.bind(source,1,phase);
    EXPECT_EQ(source->triangle(0,*phase),staticTriangle);
    std::vector<glm::dvec3> rays;
    for(double radius:{.35,.55,.75,.95,1.1}) for(int i=0;i<9;++i) {
        const double angle=(i+.317)*2*std::acos(-1.)/9;
        rays.push_back(glm::normalize(anchor+(east*std::cos(angle)+north*std::sin(angle))*(radius/1000)));
    }
    const auto original=surface.evaluateTopology(topology);
    SurfaceContact staticContact;staticContact.bind(original.vertices,original.indices,1,1000);
    double changed=0;
    for(const auto offset:{glm::dvec2(0,0),glm::dvec2(.15,.15),glm::dvec2(-.3,.15),glm::dvec2(0,0)}) {
        const TerrainMorphFootprint footprint{glm::normalize(anchor+(east*offset.x+north*offset.y)/1000.),1000,.5,.5};
        const auto revision=phase->revision();phase->set(footprint);EXPECT_EQ(phase->revision(),revision+1);
        phase->set(footprint);EXPECT_EQ(phase->revision(),revision+1);
        // Runtime callers may rebind the same source/state each frame. That
        // must not hide a phase change from triangle edge-tie invalidation.
        shared.bind(source,1,phase);
        // Independent oracle uses the public CPU evaluator and a freshly sealed
        // copy, rather than the sparse target cache or its weight helper.
        auto reference=topology;
        for(std::size_t i=0;i<reference.samples.size();++i) {
            const auto& r=reference.samples[i].radial;
            const double distance=glm::length(glm::dvec3(r[0],r[1],r[2])-footprint.eyeRadial)*1000;
            const double blend=std::clamp((distance-.5)/.5,0.,1.);
            reference.morph[i].blend[0]=blend*blend*(3-2*blend);
        }
        reference.canonicalize(surface.field().fingerprint());
        const auto mesh=surface.evaluateTopology(reference);
        SurfaceContact expected;expected.bind(mesh.vertices,mesh.indices,1,1000);
        for(const auto ray:rays) {
            const auto actual=shared.sample(ray,missing),rendered=expected.sample(ray,missing);
            EXPECT_EQ(actual.position,rendered.position);EXPECT_EQ(actual.normal,rendered.normal);
            changed=std::max(changed,glm::length(actual.position-staticContact.sample(ray,missing).position));
        }
    }
    EXPECT_GT(changed,.0001); // The queries actually cross moving surface planes.
    EXPECT_EQ(source->triangle(0),staticTriangle);
    EXPECT_EQ(snapshot->contentFingerprint(),fingerprint);EXPECT_EQ(snapshot->generation,topology.generation);
    EXPECT_NO_THROW(snapshot->validate());
    // Once both endpoints of these three targets are resident, phase-only
    // changes reuse them without evaluating any noise again.
    phase->set({anchor,1000,0,.1});source->triangle(0,*phase);
    const auto warm=source->stats().heightEvaluations;
    phase->set({glm::normalize(anchor+east*.0001),1000,0,.1});source->triangle(0,*phase);
    EXPECT_EQ(source->stats().heightEvaluations,warm);
    shared.sample(anchor,missing);
    const auto cached=source->stats().queries;
    shared.bind(source,1,phase);shared.sample(anchor,missing);EXPECT_EQ(source->stats().queries,cached);
    phase->set({anchor,1000,0,.2});
    shared.bind(source,1,phase);shared.sample(anchor,missing);EXPECT_EQ(source->stats().queries,cached+1);
    for(unsigned face=0;face<topology.triangleCount();face+=13) {
        source->triangle(face);source->triangle(face,*phase);
        ASSERT_LE(source->stats().residentPositions,SparseTerrainContacts::positionCapacity);
    }
    std::weak_ptr<TerrainMorphState> heldPhase=phase;
    std::weak_ptr<SparseTerrainContacts> heldSource=source;
    snapshot.reset();source.reset();phase.reset();
    EXPECT_FALSE(heldPhase.expired());EXPECT_FALSE(heldSource.expired());
    shared.sample(anchor,missing);shared.clear();
    EXPECT_TRUE(heldPhase.expired());EXPECT_TRUE(heldSource.expired());
}
TEST(SparseTerrainContacts, RejectsIncompatibleDeformationWithoutChangingTheActivePhase) {
    TerrainMorphState phase;
    const TerrainMorphFootprint valid{{1,0,0},1000,.5,.5};phase.set(valid);
    for(int kind=0;kind<5;++kind) {
        auto invalid=valid;
        if(kind==0) invalid.eyeRadial={0,0,0};
        if(kind==1) invalid.metersPerRadius=0;
        if(kind==2) invalid.radiusMeters=-1;
        if(kind==3) invalid.transitionMeters=0;
        if(kind==4) invalid.eyeRadial.x=std::numeric_limits<double>::quiet_NaN();
        EXPECT_THROW(phase.set(invalid),std::invalid_argument);
        EXPECT_EQ(phase.revision(),1u);EXPECT_EQ(*phase.footprint(),valid);
    }
    const TerrainSurface legacy({},lod(),1,1000);
    SparseTerrainContacts source(legacy.field(),legacy.buildTopology(1));
    EXPECT_THROW(source.triangle(0,phase),std::invalid_argument);
    auto settings=lod();settings.local_detail_streaming=true;settings.relief_sinking=true;
    settings.geometric_error_m=.05;settings.local_detail_radius_m=.5;settings.local_edge_m=.05;
    settings.local_error_m=.01;settings.local_transition_m=.5;
    const TerrainSurface detail({},settings,1,2000);
    auto topology=detail.buildTopologyForEye({1.001,0,0},{});
    SparseTerrainContacts differentScale(detail.field(),topology);
    EXPECT_THROW(differentScale.triangle(0,phase),std::invalid_argument);
    EXPECT_THROW(differentScale.triangle(topology.triangleCount(),phase),std::out_of_range);
}

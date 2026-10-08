#include "rendering/foliage/planning/GrassAllocation.h"
#include "rendering/foliage/procedural/GrassPlan.h"
#include "rendering/foliage/procedural/ProceduralGrass.h"
#include "rendering/foliage/GrassPlacement.h"
#include "rendering/geometry/Mesh.h"
#include "rendering/geometry/contacts/SparseTerrainContacts.h"
#include "config/Config.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>
#include <bit>
using namespace rendering;
namespace {
struct Oracle {std::vector<std::uint32_t> ids;std::array<std::uint32_t,17> counts{},first{};double density=0;std::size_t work=0;};
Oracle oracle(const std::vector<double>& weights,const std::vector<double>& distances,double requested,unsigned cap,unsigned budget) {
    Oracle r;std::vector<std::uint32_t> eligible;
    for(std::size_t i=0;i<weights.size();++i) if(weights[i]>0) eligible.push_back(i);
    if(eligible.size()>budget) {
        std::sort(eligible.begin(),eligible.end(),[](auto a,auto b){return grassHash(a)<grassHash(b);});eligible.resize(budget);
    }
    const auto level=[&](double area,double density) {int l=0;while(l<16 && (1u<<l)<cap && double(1u<<l)<area*density) ++l;return l;};
    const auto work=[&](double density) {std::size_t n=0;for(auto i:eligible) n+=1u<<level(weights[i],density);return n;};
    double density=eligible.empty() ? 0 : requested;
    if(work(density)>budget) {
        double area=0;for(auto i:eligible) area+=weights[i];double low=0,high=std::min(density,double(budget)/area);
        for(int i=0;i<32;++i) {double mid=(low+high)*.5;if(work(mid)<=budget) low=mid;else high=mid;}
        density=low;
    }
    std::sort(eligible.begin(),eligible.end(),[&](auto a,auto b) {
        return distances[a]<distances[b] || (distances[a]==distances[b] && a<b);
    });
    for(int l=0;l<17;++l) {
        r.first[l]=r.ids.size();
        for(auto i:eligible) if(level(weights[i],density)==l) {r.ids.push_back(i);++r.counts[l];r.work+=1u<<l;}
    }
    r.density=density;return r;
}
void fixture(const std::vector<double>& weights,double requested,unsigned slots,unsigned budget,bool chunks=false,bool restricted=false) {
    GrassMetadataBuffers metadata;metadata.generation={123,456,1,1,TerrainBackend::Compute};metadata.triangles=weights.size();
    metadata.planningEye={1.002,0,0};
    GrassMetadataParameters p;p.ranges[3]=requested;p.flags[3]=weights.size();
    std::vector<GrassTriangleMetadata> triangles(weights.size());
    std::vector<double> distances(weights.size());
    for(std::size_t i=0;i<weights.size();++i) {
        // Repeated and adjacent-double keys exercise stable ties and prevent
        // a float-rounded sort from silently passing the independent oracle.
        distances[i]=double((i*37)%19)*.125;
        if(i%3==0) distances[i]=std::nextafter(distances[i],1e9);
        triangles[i].areaDistance={weights[i],distances[i]};
        triangles[i].identity={std::uint32_t(i),std::uint32_t(weights[i]>0),0,0};
    }
    glGenBuffers(1,&metadata.descriptors);glBindBuffer(GL_SHADER_STORAGE_BUFFER,metadata.descriptors);
    glBufferData(GL_SHADER_STORAGE_BUFFER,triangles.size()*64,triangles.data(),GL_STATIC_DRAW);
    glGenBuffers(1,&metadata.parameters);glBindBuffer(GL_SHADER_STORAGE_BUFFER,metadata.parameters);
    glBufferData(GL_SHADER_STORAGE_BUFFER,160,&p,GL_STATIC_DRAW);
    config::FoliageConfig f;f.max_blades=budget;f.max_candidates_per_triangle=slots;
    auto limits=TerrainComputeLimits::query();ASSERT_TRUE(limits.unavailable.empty());if(chunks) limits.groups=1;
    if(restricted) limits.blockBytes=std::max<std::uint64_t>(weights.size()*64,224);
    const unsigned effective=std::min<std::uint64_t>(budget,limits.blockBytes/128);
    GrassAllocationCompute compute(limits);auto allocation=compute.generate(metadata,f);
    EXPECT_THROW(allocation->readSummary(),std::logic_error);allocation->waitForCapture();
    const auto actual=allocation->readSummary();const auto expected=oracle(weights,distances,requested,std::bit_floor(slots),effective);
    EXPECT_EQ(actual.counts,expected.counts);EXPECT_EQ(actual.first,expected.first);
    EXPECT_EQ(actual.totals[0],expected.ids.size());EXPECT_EQ(actual.totals[1],expected.work);
    EXPECT_LE(actual.totals[1],effective);EXPECT_EQ(actual.control[0],effective);
    EXPECT_NEAR(actual.densitySearch[0],expected.density,std::max(1e-12,expected.density*1e-9));
    const auto ids=allocation->readReferencesForValidation(actual.totals[0]);EXPECT_EQ(ids,expected.ids);
    auto replay=compute.generate(metadata,f);replay->waitForCapture();const auto second=replay->readSummary();
    EXPECT_EQ(second.counts,actual.counts);EXPECT_EQ(second.densitySearch[0],actual.densitySearch[0]);
    EXPECT_EQ(replay->readReferencesForValidation(second.totals[0]),ids);
    EXPECT_EQ(allocation->inputBytes,228+12*allocation->dispatches+4*std::bit_width(weights.size()-1));EXPECT_EQ(allocation->summaryReadBytes,224);
    EXPECT_EQ(allocation->diagnosticReadBytes,ids.size()*4);EXPECT_EQ(allocation->generation,metadata.generation);
    std::cout << "GRASS_ALLOCATION triangles=" << weights.size() << " patches=" << actual.totals[0]
        << " candidates=" << actual.totals[1] << " budget=" << effective << " density=" << actual.densitySearch[0]
        << " input=" << allocation->inputBytes << " working=" << allocation->workingBytes
        << " dispatches=" << allocation->dispatches << '\n';
    EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
}
}
TEST(GrassAllocation, RoundedSlotsBoundariesAndNonPowerCapsMatchIndependentOracle) {
    fixture({0,.125,.5,1,2,4,8,16,32,64,128},1,70,1000);
    fixture({1,std::nextafter(1.0,0.0),std::nextafter(1.0,2.0),2,4,8},1,65536,1000);
    std::vector<double> weights(137);for(std::size_t i=0;i<weights.size();++i) weights[i]=(i%9+1)*.25;
    fixture(weights,32,8192,500,true);fixture(weights,32,8192,500,false,true);
    // Non-power-of-two runs across many work groups, including empty levels.
    fixture(std::vector<double>(1025,1),1,8192,2048,true);
    fixture({1},1,1,1);
}
TEST(GrassAllocation, TinyBudgetsChooseStableLowestHashesAndEmptyEligibilityStaysEmpty) {
    std::vector<double> weights(73,64);
    for(unsigned budget:{1,2,7,72,73}) fixture(weights,32,8192,budget);
    fixture(std::vector<double>(65,0),32,8192,10);
}
TEST(GrassAllocation, InvalidSourcesAndQueriedLimitsPreservePreviousOutput) {
    GrassMetadataBuffers metadata;metadata.generation={1,2,1,1,TerrainBackend::Compute};metadata.triangles=1;
    GrassTriangleMetadata triangle;triangle.areaDistance[0]=1;triangle.identity={0,1,0,0};
    GrassMetadataParameters p;p.flags[3]=1;p.ranges[3]=1;
    glGenBuffers(1,&metadata.descriptors);glBindBuffer(GL_SHADER_STORAGE_BUFFER,metadata.descriptors);glBufferData(GL_SHADER_STORAGE_BUFFER,64,&triangle,GL_STATIC_DRAW);
    glGenBuffers(1,&metadata.parameters);glBindBuffer(GL_SHADER_STORAGE_BUFFER,metadata.parameters);glBufferData(GL_SHADER_STORAGE_BUFFER,160,&p,GL_STATIC_DRAW);
    GrassAllocationCompute compute;config::FoliageConfig f;
    auto previous=compute.generate(metadata,f);previous->waitForCapture();const auto before=previous->readSummary();
    const auto generation=metadata.generation;metadata.generation.backend=TerrainBackend::Cpu;
    EXPECT_THROW(compute.generate(metadata,f),std::invalid_argument);metadata.generation=generation;
    metadata.triangles=2;EXPECT_THROW(compute.generate(metadata,f),std::invalid_argument);metadata.triangles=1;
    auto limits=TerrainComputeLimits::query();limits.groups=0;GrassAllocationCompute zero(limits);
    EXPECT_THROW(zero.generate(metadata,f),std::runtime_error);limits.groups=1;limits.blockBytes=223;GrassAllocationCompute tiny(limits);
    EXPECT_THROW(tiny.generate(metadata,f),std::runtime_error);
    EXPECT_EQ(previous->readSummary().totals,before.totals);EXPECT_TRUE(glIsBuffer(previous->references));
    EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
}
TEST(GrassAllocation, ProductionPlanningAndResidentTerrainNeedNoCpuRenderVectors) {
    config::ScenarioConfig scene(config::Config::load("configs/scenarios/solar_system.json"));auto p=scene.planets[0];
    const TerrainSurface s(p.surface_noise,p.terrain_lod,p.radius,scene.metersPerWorldUnit(),p.terrain_landscape,p.water.level_m,p.terrain_material);
    const glm::dvec3 eye(1.012,0,0);const auto topology=s.buildTopologyForEye(eye,{0,0,0});
    TerrainCompute terrain;auto output=terrain.generate(s.field(),topology);output->waitForCapture();
    output->contacts=std::make_shared<SparseTerrainContacts>(s.field(),topology);
    TerrainGeometry header;static_cast<TerrainBuildStats&>(header)=topology;
    Mesh mesh;mesh.loadComputedTerrain(std::move(header),*output,false);
    EXPECT_TRUE(mesh.vertices.empty());EXPECT_TRUE(mesh.indices.empty());EXPECT_EQ(mesh.indexCount,topology.indices.size());
    EXPECT_EQ(mesh.terrainStats.evaluationQueries.requests,0u);EXPECT_TRUE(mesh.contacts);
    ProceduralGrass grass;const auto prepared=grass.prepare(0,mesh,p,scene.metersPerWorldUnit(),eye);const auto stats=grass.stats(0);
    EXPECT_GT(stats.patches,0u);EXPECT_GT(stats.candidates,0u);EXPECT_LE(stats.candidates,p.foliage.max_blades);
    EXPECT_EQ(stats.patchBytes,0u);EXPECT_EQ(stats.summaryReadBytes,224u);EXPECT_EQ(stats.metadataReadBytes,0u);
    EXPECT_EQ(prepared.uploadedBytes,stats.metadataInputBytes+stats.allocationInputBytes);
    EXPECT_EQ(grass.prepare(0,mesh,p,scene.metersPerWorldUnit(),eye).uploadedBytes,0u);
    std::cout << "RESIDENT_PLAN triangles=" << topology.triangleCount() << " patches=" << stats.patches
        << " candidates=" << stats.candidates << " input=" << prepared.uploadedBytes
        << " metadata=" << stats.metadataBytes << " allocation=" << stats.allocationBytes << '\n';
    p.foliage.enabled=false;grass.prepare(0,mesh,p,scene.metersPerWorldUnit(),eye);EXPECT_EQ(grass.stats(0).allocationBytes,0u);
    grass.clear();mesh.destroy();EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
}
namespace {
struct AdaptiveResult {GrassAllocationSummary summary;std::vector<std::uint32_t> ids;};
AdaptiveResult adaptiveFixture(unsigned capacity,unsigned target,unsigned slotCap=128,const GrassAllocationSummary* replay=nullptr) {
    GrassMetadataBuffers metadata;metadata.adaptive=true;metadata.generation={123,456,1,1,TerrainBackend::Compute};
    // Three protected triangles and many distant triangles. Far pressure cannot
    // silently lower the requested eight blades/m² on any protected triangle.
    std::vector<GrassTriangleMetadata> triangles(103);metadata.triangles=triangles.size();
    for(unsigned i=0;i<triangles.size();++i) {
        triangles[i].areaDistance={1.,i<3?double(i+1):11.+(i-3)*.8};triangles[i].identity={i,1,0,0};
    }
    GrassMetadataParameters p;p.eyeScale[3]=1;p.ranges={100,0,40,8};p.flags[3]=triangles.size();
    p.padding={1u,std::bit_cast<std::uint32_t>(10.f),0u,target};
    if(replay) {
        p.padding[0]=3u|(replay->padding[0]?4u:0u);p.padding[3]=replay->control[0];
        p.ranges[3]=replay->densitySearch[0];p.ranges[2]=replay->densitySearch[3];
    }
    glGenBuffers(1,&metadata.descriptors);glBindBuffer(GL_SHADER_STORAGE_BUFFER,metadata.descriptors);
    glBufferData(GL_SHADER_STORAGE_BUFFER,triangles.size()*64,triangles.data(),GL_STATIC_DRAW);
    glGenBuffers(1,&metadata.parameters);glBindBuffer(GL_SHADER_STORAGE_BUFFER,metadata.parameters);
    glBufferData(GL_SHADER_STORAGE_BUFFER,160,&p,GL_STATIC_DRAW);
    config::FoliageConfig f;f.max_blades=capacity;f.max_candidates_per_triangle=slotCap;
    auto limits=TerrainComputeLimits::query();limits.groups=1;
    GrassAllocationCompute compute(limits);auto allocation=compute.generate(metadata,f);allocation->waitForCapture();
    AdaptiveResult r;r.summary=allocation->readSummary();r.ids=allocation->readReferencesForValidation(r.summary.totals[0]);
    EXPECT_LE(r.summary.totals[1],capacity);EXPECT_LE(r.summary.totals[1],r.summary.control[0]);
    EXPECT_EQ(allocation->summaryReadBytes,224u);EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
    return r;
}
void replayAdaptive(const AdaptiveResult& r,unsigned capacity,unsigned cap=128) {
    const auto locked=adaptiveFixture(capacity,1,cap,&r.summary);
    EXPECT_EQ(locked.ids,r.ids);EXPECT_EQ(locked.summary.counts,r.summary.counts);
    EXPECT_EQ(locked.summary.totals,r.summary.totals);EXPECT_EQ(locked.summary.control[0],r.summary.control[0]);
    EXPECT_EQ(locked.summary.densitySearch[0],r.summary.densitySearch[0]);
    EXPECT_EQ(locked.summary.densitySearch[3],r.summary.densitySearch[3]);EXPECT_EQ(locked.summary.padding[0],r.summary.padding[0]);
}
}
TEST(GrassAllocation, AdaptiveFalloffProtectsConfiguredNearDensityAndFitsFarTail) {
    const auto normal=adaptiveFixture(128,80);const auto slower=adaptiveFixture(128,40);
    for(const auto* r:{&normal,&slower}) {
        EXPECT_EQ(r->summary.densitySearch[0],8.);EXPECT_EQ(r->summary.padding[0],0u);
        EXPECT_GT(r->summary.densitySearch[3],0.);EXPECT_LT(r->summary.densitySearch[3],40.);
        for(unsigned id=0;id<3;++id) EXPECT_NE(std::find(r->ids.begin(),r->ids.end(),id),r->ids.end());
        // The protected triangles are all in the eight-slot batch.
        const auto begin=r->ids.begin()+r->summary.first[3],end=begin+r->summary.counts[3];
        for(unsigned id=0;id<3;++id) EXPECT_NE(std::find(begin,end,id),end);
        replayAdaptive(*r,128);
    }
    EXPECT_LT(slower.summary.densitySearch[3],normal.summary.densitySearch[3]);
    const auto wide=adaptiveFixture(10000,8000);EXPECT_DOUBLE_EQ(wide.summary.densitySearch[3],40.);
    replayAdaptive(wide,10000);
    const auto nearOnly=adaptiveFixture(128,1);EXPECT_EQ(nearOnly.summary.control[0],24u);
    EXPECT_EQ(nearOnly.summary.densitySearch[0],8.);EXPECT_EQ(nearOnly.summary.totals[1],24u);
    EXPECT_EQ(nearOnly.ids.size(),3u);replayAdaptive(nearOnly,128);
    std::cout << "ADAPTIVE_FALLOFF rho=" << normal.summary.densitySearch[0] << " sigma80=" << normal.summary.densitySearch[3]
        << " sigma40=" << slower.summary.densitySearch[3] << " near_only_budget=" << nearOnly.summary.control[0] << '\n';
}
TEST(GrassAllocation, AdaptiveTinyAndPerTriangleCapsReportInfeasibleNearDensity) {
    for(unsigned capacity:{1u,2u,3u,10u}) {
        const auto r=adaptiveFixture(capacity,1);EXPECT_EQ(r.summary.padding[0],1u);
        EXPECT_EQ(r.summary.densitySearch[3],0.);EXPECT_GT(r.summary.totals[1],0u);
        for(auto id:r.ids) EXPECT_LT(id,3u); // Distant work never displaces protected patches.
        replayAdaptive(r,capacity);
    }
    const auto capped=adaptiveFixture(128,80,4);EXPECT_EQ(capped.summary.padding[0],1u);
    replayAdaptive(capped,128,4);
}

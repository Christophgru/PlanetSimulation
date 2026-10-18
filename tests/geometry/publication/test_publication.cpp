#include "rendering/geometry/publication/TerrainPublication.h"
#include "rendering/geometry/contacts/SparseTerrainContacts.h"
#include "rendering/character/SurfaceContact.h"
#include "config/Config.h"
#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <iostream>
#include <numbers>

using namespace rendering;
namespace {
TerrainBuildRequest request(bool water=true,bool foliage=true,std::size_t body=0) {
    config::ScenarioConfig scene(config::Config::load("tests/scenarios/foliage/surface.json"));
    auto p=scene.planets[0];p.water.enabled=water;p.foliage.enabled=foliage;p.foliage.max_blades=128;
    p.name=body ? "Moon" : "Earth";
    p.terrain_lod.max_triangle_budget=10000;p.terrain_lod.shoreline_edge_m=0;
    TerrainSurface surface(p.surface_noise,p.terrain_lod,p.radius,scene.metersPerWorldUnit(),
        p.terrain_landscape,p.water.level_m,p.terrain_material);
    TerrainBuildIdentity key;key.serial=1;key.bodyIndex=body;key.bodyName=p.name;key.field=surface.field().fingerprint();
    key.backend=TerrainBackend::Compute;key.resident=true;key.eye={1.012,0,0};key.localMask=1;
    return {key,std::move(surface),p,{},scene.metersPerWorldUnit()};
}
TerrainBuildRequest preparedRequest(const glm::dvec3& radial) {
    auto r=request(false,true);auto& p=r.planet;p.radius=1;p.color={.2,.6,.1};p.terrain_landscape.enabled=false;
    auto& l=p.terrain_lod;l.local_detail_streaming=true;l.local_detail_radius_m=1.5;
    l.local_edge_m=.05;l.local_error_m=.01;l.local_transition_m=.5;
    l.relief_sinking=true;l.geometric_error_m=.05;l.max_triangle_budget=100000;
    l.base_edge_segments=1;l.max_edge_segments=l.steep_edge_segments=8;l.medium_edge_segments=4;
    l.near_surface_distance_m=300;l.mid_surface_distance_m=1000;l.shoreline_edge_m=0;
    config::PlanetConfig::SurfaceNoiseFunction noise;
    noise.amplitude_m=.01;noise.wavelength_m=.25;noise.octaves=1;noise.seed=-71;p.surface_noise={noise};
    auto& f=p.foliage;f.enabled=f.compute_placement=true;f.frustum_culling=false;
    f.draw_distance_m=5;f.quad_distance_m=1;f.max_blades=100000;f.max_candidates_per_triangle=65536;f.density_per_m2=2;
    r.metersPerUnit=1000;r.surface=TerrainSurface(p.surface_noise,l,1,1000,{},std::nullopt,p.terrain_material);
    r.identity.field=r.surface.field().fingerprint();r.identity.topologyVersion=4;
    r.identity.eye=glm::normalize(radial)*1.002;return r;
}
struct OwnedMesh:Mesh {~OwnedMesh(){destroy();}};
bool submit(TerrainPublication& p,const TerrainBuildRequest& r,const Mesh& land,const Mesh& water,TerrainCompute& compute,
    const glm::dvec3& eye=glm::dvec3(1.012,0,0)) {
    return p.submit(buildTerrainCpu(r),r.identity,r.planet,r.metersPerUnit,eye,land,water,compute);
}
void retire(TerrainPublication& p,std::size_t body) {
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(10);glFlush();
    while(p.retiring(body) && std::chrono::steady_clock::now()<end) {
        p.pollRetired();glFlush();std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_FALSE(p.retiring(body));
}
void install(TerrainPublication& p,const TerrainBuildRequest& r,Mesh& land,Mesh& water,TerrainCompute& compute) {
    ASSERT_TRUE(submit(p,r,land,water,compute));p.waitForCapture();
    ASSERT_TRUE(p.publish(r.identity,land,water));retire(p,r.identity.bodyIndex);
}
struct Snapshot {
    GLuint landBuffer,waterBuffer;
    std::uint64_t landRevision,waterRevision,serial,bytes;
    std::shared_ptr<SparseTerrainContacts> contacts;
    std::optional<TerrainGenerationKey> grassKey;
    std::optional<glm::dvec3> grassEye;
    std::size_t candidates;
    Snapshot(const TerrainPublication& p,const ProceduralGrass& grass,const Mesh& land,const Mesh& water,std::size_t i=0)
        :landBuffer(land.vbo),waterBuffer(water.vbo),landRevision(land.revision),waterRevision(water.revision),
        serial(p.installed(i).identity.serial),bytes(p.reservedBytes()),contacts(land.contacts),
        grassKey(grass.residentGeneration(i)),grassEye(grass.planningEye(i)),candidates(grass.stats(i).candidates) {}
    void unchanged(const TerrainPublication& p,const ProceduralGrass& grass,const Mesh& land,const Mesh& water,std::size_t i=0) const {
        EXPECT_EQ(land.vbo,landBuffer);EXPECT_EQ(water.vbo,waterBuffer);
        EXPECT_EQ(land.revision,landRevision);EXPECT_EQ(water.revision,waterRevision);
        EXPECT_EQ(land.contacts,contacts);EXPECT_EQ(p.installed(i).identity.serial,serial);
        EXPECT_EQ(grass.residentGeneration(i),grassKey);EXPECT_EQ(grass.planningEye(i),grassEye);
        EXPECT_EQ(grass.stats(i).candidates,candidates);
        if(landBuffer) EXPECT_TRUE(glIsBuffer(landBuffer));
        if(waterBuffer) EXPECT_TRUE(glIsBuffer(waterBuffer));
    }
};
unsigned fenceCalls=0,failAt=0,pollCalls=0,blockingCalls=0;
PFNGLFENCESYNCPROC originalFence=nullptr;
GLsync GLAPIENTRY failFence(GLenum condition,GLbitfield flags) {
    if(++fenceCalls==failAt) return nullptr;
    return originalFence(condition,flags);
}
struct FenceFault {
    explicit FenceFault(unsigned call) {fenceCalls=0;failAt=call;originalFence=__glewFenceSync;__glewFenceSync=failFence;}
    ~FenceFault(){__glewFenceSync=originalFence;}
};
GLenum GLAPIENTRY delayFence(GLsync,GLbitfield flags,GLuint64 timeout) {
    ++pollCalls;if(flags || timeout) ++blockingCalls;return GL_TIMEOUT_EXPIRED;
}
struct FenceDelay {
    PFNGLCLIENTWAITSYNCPROC original=__glewClientWaitSync;
    FenceDelay(){pollCalls=blockingCalls=0;__glewClientWaitSync=delayFence;}
    ~FenceDelay(){__glewClientWaitSync=original;}
};
PFNGLGENBUFFERSPROC originalGenBuffers=nullptr;
std::vector<GLuint> generated;
void GLAPIENTRY recordBuffers(GLsizei count,GLuint* buffers) {
    originalGenBuffers(count,buffers);generated.insert(generated.end(),buffers,buffers+count);
}
struct BufferProbe {
    BufferProbe(){generated.clear();originalGenBuffers=__glewGenBuffers;__glewGenBuffers=recordBuffers;}
    ~BufferProbe(){__glewGenBuffers=originalGenBuffers;}
};
PFNGLDISPATCHCOMPUTEPROC originalDispatch=nullptr;
void GLAPIENTRY failDispatch(GLuint x,GLuint y,GLuint z) {
    originalDispatch(x,y,z);glBindBuffer(GLenum(0),0); // GL_INVALID_ENUM after actual private writes.
}
struct DispatchFault {
    DispatchFault(){originalDispatch=__glewDispatchCompute;__glewDispatchCompute=failDispatch;}
    ~DispatchFault(){__glewDispatchCompute=originalDispatch;}
};
}
TEST(TerrainPublication, CompleteTransferChangesEveryConsumerOnceAndRetiresTheirOldBuffersTogether) {
    auto r=request();TerrainCompute compute;ProceduralGrass grass;OwnedMesh land,water;
    TerrainPublication p(grass,1);install(p,r,land,water,compute);Snapshot old(p,grass,land,water);
    const auto bytes=p.reservedBytes();++r.identity.serial;r.identity.eye={1.012,.02,0};
    const glm::dvec3 chaseEye{1.02,.01,.03};
    ASSERT_TRUE(submit(p,r,land,water,compute,chaseEye));const auto newBytes=p.reservedBytes()-bytes;
    EXPECT_GT(newBytes,0u);EXPECT_LE(bytes+newBytes,2*ProceduralGrass::defaultStageBytes);
    old.unchanged(p,grass,land,water);p.waitForCapture();old.unchanged(p,grass,land,water);
    // Queue real uses of the old terrain/grass before the retirement fence.
    grass.shader.use();grass.shader.setInt("uTerrainVertices",8);grass.shader.setInt("uTerrainIndices",9);
    grass.shader.setInt("uShadowMap",1);grass.shader.setInt("uAtmColumns",7);grass.shader.setInt("uTrailTree",10);
    glEnable(GL_RASTERIZER_DISCARD);
    land.draw();water.draw();EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
    GrassPass pass;pass.mainEyeBody=chaseEye;grass.draw(0,&pass);EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
    glDisable(GL_RASTERIZER_DISCARD);glUseProgram(0);
    ASSERT_TRUE(p.publish(r.identity,land,water));EXPECT_FALSE(p.pending());EXPECT_TRUE(p.retiring(0));
    EXPECT_NE(land.vbo,old.landBuffer);EXPECT_NE(water.vbo,old.waterBuffer);
    EXPECT_EQ(land.revision,old.landRevision+1);EXPECT_EQ(water.revision,old.waterRevision+1);
    EXPECT_EQ(land.contacts->generation(),land.terrainStats.generation);
    EXPECT_EQ(grass.residentGeneration(0),land.terrainStats.generation);EXPECT_EQ(grass.residentRevision(0),land.revision);
    EXPECT_EQ(grass.planningEye(0),chaseEye);EXPECT_EQ(p.installed(0).grassEye,chaseEye);
    EXPECT_EQ(p.installed(0).identity,r.identity);EXPECT_EQ(p.installed(0).land,land.terrainStats.generation);
    EXPECT_EQ(p.installed(0).water,water.terrainStats.generation);
    EXPECT_TRUE(land.vertices.empty());EXPECT_TRUE(water.vertices.empty());
    EXPECT_TRUE(glIsBuffer(old.landBuffer));EXPECT_TRUE(glIsBuffer(old.waterBuffer));
    {FenceDelay delay;for(int i=0;i<3;++i) p.pollRetired();EXPECT_EQ(pollCalls,3u);EXPECT_EQ(blockingCalls,0u);}
    EXPECT_FALSE(p.canSubmit(0));EXPECT_EQ(p.reservedBytes(),bytes+newBytes);
    retire(p,0);EXPECT_FALSE(glIsBuffer(old.landBuffer));EXPECT_FALSE(glIsBuffer(old.waterBuffer));
    EXPECT_TRUE(p.canSubmit(0));EXPECT_EQ(p.reservedBytes(),newBytes);EXPECT_EQ(p.stats().peakReservedBytes,bytes+newBytes);
    EXPECT_EQ(p.stats().published,2u);EXPECT_EQ(p.stats().retired,2u);EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
}
TEST(TerrainPublication, FenceFailuresAtEveryStagePreserveLiveConsumersAndReleaseStagingBuffers) {
    auto r=request();TerrainCompute compute;ProceduralGrass grass;OwnedMesh land,water;
    TerrainPublication p(grass,1);install(p,r,land,water,compute);Snapshot old(p,grass,land,water);++r.identity.serial;
    // Land, water, metadata, allocation, draw resources, retirement.
    for(unsigned fault=1;fault<=6;++fault) {
        SCOPED_TRACE(fault);BufferProbe buffers;
        {FenceFault fences(fault);
            EXPECT_THROW({submit(p,r,land,water,compute);p.waitForCapture();p.publish(r.identity,land,water);},std::runtime_error);
            EXPECT_EQ(fenceCalls,fault);
        }
        EXPECT_FALSE(p.pending());EXPECT_FALSE(p.retiring(0));EXPECT_TRUE(p.canSubmit(0));
        old.unchanged(p,grass,land,water);EXPECT_EQ(p.reservedBytes(),old.bytes);
        for(GLuint buffer:generated) EXPECT_FALSE(glIsBuffer(buffer));
        EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
    }
    EXPECT_EQ(p.stats().failed,6u);
    install(p,r,land,water,compute);EXPECT_EQ(land.revision,old.landRevision+1);EXPECT_EQ(p.stats().published,2u);
}
TEST(TerrainPublication, DelayedPreparationDoesNotPublishOrReadSummaryAndPollsWithoutWaiting) {
    auto r=request();TerrainCompute compute;ProceduralGrass grass;OwnedMesh land,water;
    TerrainPublication p(grass,1);install(p,r,land,water,compute);Snapshot old(p,grass,land,water);++r.identity.serial;
    ASSERT_TRUE(submit(p,r,land,water,compute));
    {FenceDelay delay;for(int i=0;i<3;++i) EXPECT_FALSE(p.poll());EXPECT_EQ(pollCalls,9u);EXPECT_EQ(blockingCalls,0u);}
    EXPECT_FALSE(p.ready());old.unchanged(p,grass,land,water);EXPECT_THROW(p.publish(r.identity,land,water),std::logic_error);
    p.waitForCapture();ASSERT_TRUE(p.publish(r.identity,land,water));retire(p,0);
    EXPECT_EQ(grass.stats(0).summaryReadBytes,224u);EXPECT_EQ(grass.stats(0).metadataReadBytes,0u);
    EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
}
TEST(TerrainPublication, EpochBodyFieldModeAndSerialChangesRejectReadyBundlesWithoutChangingTheScene) {
    auto r=request(false,false);TerrainCompute compute;ProceduralGrass grass;OwnedMesh land,water;
    TerrainPublication p(grass,1);install(p,r,land,water,compute);Snapshot old(p,grass,land,water);++r.identity.serial;
    for(int mutation=0;mutation<10;++mutation) {
        SCOPED_TRACE(mutation);ASSERT_TRUE(submit(p,r,land,water,compute));p.waitForCapture();auto current=r.identity;
        switch(mutation) {
        case 0:++current.epoch;break;case 1:++current.bodyIndex;break;case 2:current.bodyName="Moon";break;
        case 3:++current.field;break;case 4:++current.fieldVersion;break;case 5:++current.topologyVersion;break;
        case 6:current.backend=TerrainBackend::Cpu;break;case 7:current.localMask=0;break;
        case 8:current.serial=1;break;
        case 9:current.resident=false;break;
        }
        EXPECT_FALSE(p.publish(current,land,water));old.unchanged(p,grass,land,water);
        EXPECT_EQ(p.reservedBytes(),old.bytes);EXPECT_FALSE(p.pending());
    }
    EXPECT_EQ(p.stats().obsolete,10u);
    ASSERT_TRUE(submit(p,r,land,water,compute));p.waitForCapture();auto moved=r.identity;
    moved.eye={0,1.02,0};++moved.serial; // A useful completed anchor does not starve on camera motion.
    EXPECT_TRUE(p.publish(moved,land,water));retire(p,0);
    EXPECT_EQ(p.installed(0).identity.eye,r.identity.eye);EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
}
TEST(TerrainPublication, GlobalPreparationAndPerBodyRetirementSlotsStayBoundedAcrossBodySwitches) {
    auto earth=request(false,false),moon=request(false,false,1);TerrainCompute compute;ProceduralGrass grass;
    OwnedMesh land[2],water[2];TerrainPublication p(grass,2);
    ASSERT_TRUE(submit(p,earth,land[0],water[0],compute));EXPECT_FALSE(p.canSubmit(1));
    EXPECT_FALSE(submit(p,moon,land[1],water[1],compute));p.waitForCapture();ASSERT_TRUE(p.publish(earth.identity,land[0],water[0]));
    EXPECT_FALSE(p.canSubmit(0));EXPECT_TRUE(p.canSubmit(1));
    ASSERT_TRUE(submit(p,moon,land[1],water[1],compute));p.waitForCapture();ASSERT_TRUE(p.publish(moon.identity,land[1],water[1]));
    {FenceDelay delay;p.pollRetired();EXPECT_EQ(pollCalls,2u);EXPECT_FALSE(p.canSubmit(0));EXPECT_FALSE(p.canSubmit(1));}
    retire(p,0);retire(p,1);
    const auto firstBytes=p.reservedBytes();
    for(int i=0;i<6;++i) {
        auto& r=i%2 ? moon : earth;auto body=r.identity.bodyIndex;++r.identity.serial;
        r.identity.eye={1.012,.01*i,0};install(p,r,land[body],water[body],compute);
        EXPECT_EQ(p.reservedBytes(),firstBytes);EXPECT_EQ(p.installed(body).identity.bodyName,r.planet.name);
        EXPECT_EQ(grass.residentGeneration(body),land[body].terrainStats.generation);
    }
    EXPECT_EQ(p.stats().published,8u);EXPECT_EQ(p.stats().retired,8u);EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
}
TEST(TerrainPublication, OverlapAdmissionRejectsBeforeDispatchAndLiveMutationRejectsBeforeTransfer) {
    auto r=request(false,false);auto build=buildTerrainCpu(r);TerrainCompute compute;ProceduralGrass grass;OwnedMesh land,water;
    const auto bytes=TerrainGpuPreparation::requiredBytes(build,r.identity,r.planet,compute.limits());
    TerrainPublication p(grass,1,2*bytes-1);install(p,r,land,water,compute);Snapshot old(p,grass,land,water);++r.identity.serial;
    {FenceFault probe(100);EXPECT_THROW(submit(p,r,land,water,compute),std::runtime_error);EXPECT_EQ(fenceCalls,0u);}
    old.unchanged(p,grass,land,water);EXPECT_EQ(p.reservedBytes(),old.bytes);
    // A separate body owner's stage exercises validation of externally changed live handles/revisions.
    ProceduralGrass nextGrass;OwnedMesh nextLand,nextWater;TerrainPublication next(nextGrass,1);
    ASSERT_TRUE(submit(next,r,nextLand,nextWater,compute));next.waitForCapture();
    ++nextLand.revision;EXPECT_THROW(next.publish(r.identity,nextLand,nextWater),std::invalid_argument);
    --nextLand.revision;nextLand.vbo=land.vbo;
    EXPECT_THROW(next.publish(r.identity,nextLand,nextWater),std::invalid_argument);nextLand.vbo=0;
    EXPECT_EQ(next.installed(0).identity.serial,0u);EXPECT_FALSE(nextGrass.residentGeneration(0));
    EXPECT_TRUE(next.publish(r.identity,nextLand,nextWater));retire(next,0);
    EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
}
TEST(TerrainPublication, DisabledConsumersClearPreviousWaterAndGrassAtTheSameBoundary) {
    auto r=request();TerrainCompute compute;ProceduralGrass grass;OwnedMesh land,water;
    TerrainPublication p(grass,1);install(p,r,land,water,compute);const auto waterBuffer=water.vbo;
    ASSERT_GT(grass.stats(0).candidates,0u);
    r.planet.water.enabled=false;r.planet.foliage.enabled=false;r.planet.foliage.compute_placement=false;++r.identity.serial;
    ASSERT_TRUE(submit(p,r,land,water,compute));p.waitForCapture();ASSERT_TRUE(p.publish(r.identity,land,water));
    EXPECT_FALSE(p.installed(0).waterEnabled);EXPECT_EQ(water.vbo,0u);EXPECT_EQ(water.indexCount,0u);
    EXPECT_EQ(grass.stats(0).candidates,0u);EXPECT_EQ(grass.stats(0).summaryReadBytes,0u);
    EXPECT_EQ(grass.residentGeneration(0),land.terrainStats.generation);EXPECT_EQ(grass.residentRevision(0),land.revision);
    EXPECT_TRUE(glIsBuffer(waterBuffer));retire(p,0);EXPECT_FALSE(glIsBuffer(waterBuffer));
    EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
}
TEST(TerrainPublication, ChangedLiveGrassIsRejectedBeforeAnyTerrainOrContactTransfer) {
    auto r=request();TerrainCompute compute;ProceduralGrass grass;OwnedMesh land,water;
    TerrainPublication p(grass,1);install(p,r,land,water,compute);Snapshot old(p,grass,land,water);++r.identity.serial;
    ASSERT_TRUE(submit(p,r,land,water,compute));p.waitForCapture();
    const auto changedEye=r.identity.eye+glm::dvec3(0,.08,0);
    grass.prepare(0,land,r.planet,r.metersPerUnit,changedEye);
    ASSERT_EQ(grass.planningEye(0),changedEye);
    EXPECT_THROW(p.publish(r.identity,land,water),std::invalid_argument);
    EXPECT_EQ(land.vbo,old.landBuffer);EXPECT_EQ(water.vbo,old.waterBuffer);EXPECT_EQ(land.contacts,old.contacts);
    EXPECT_EQ(p.installed(0).identity.serial,old.serial);EXPECT_EQ(grass.planningEye(0),changedEye);
    p.cancel();EXPECT_EQ(p.reservedBytes(),old.bytes);EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
}
TEST(TerrainPublication, GrassOnlyReplacementRetainsTerrainAndContactsThroughPublicationAndRetirement) {
    auto r=request();TerrainCompute compute;ProceduralGrass grass;OwnedMesh land,water;
    TerrainPublication p(grass,1);install(p,r,land,water,compute);Snapshot old(p,grass,land,water);
    const auto eye=r.identity.eye+glm::dvec3(0,.08,0);
    ASSERT_TRUE(p.submitGrass(0,r.planet,r.metersPerUnit,eye,land,water));p.waitForCapture();
    old.unchanged(p,grass,land,water);ASSERT_TRUE(p.publish(r.identity,land,water));
    EXPECT_EQ(land.vbo,old.landBuffer);EXPECT_EQ(water.vbo,old.waterBuffer);EXPECT_EQ(land.contacts,old.contacts);
    EXPECT_EQ(land.revision,old.landRevision);EXPECT_EQ(water.revision,old.waterRevision);
    EXPECT_EQ(p.installed(0).identity.serial,old.serial);EXPECT_EQ(grass.planningEye(0),eye);
    EXPECT_EQ(grass.residentGeneration(0),old.grassKey);EXPECT_EQ(grass.residentRevision(0),old.landRevision);
    {FenceDelay delay;p.pollRetired();EXPECT_EQ(pollCalls,1u);EXPECT_EQ(blockingCalls,0u);}
    EXPECT_FALSE(p.canSubmit(0));p.waitRetiredForCapture(0);
    EXPECT_TRUE(glIsBuffer(old.landBuffer));EXPECT_TRUE(glIsBuffer(old.waterBuffer));
    EXPECT_EQ(p.stats().grassOnlyPublished,1u);EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
}
TEST(TerrainPublication, GrassOnlyFenceFailuresAndStaleEpochsRetainAllPublishedConsumers) {
    auto r=request();TerrainCompute compute;ProceduralGrass grass;OwnedMesh land,water;
    TerrainPublication p(grass,1);install(p,r,land,water,compute);Snapshot old(p,grass,land,water);
    const auto eye=r.identity.eye+glm::dvec3(0,.08,0);
    for(unsigned fault=1;fault<=4;++fault) {
        BufferProbe buffers;
        {FenceFault fences(fault);
            EXPECT_THROW({p.submitGrass(0,r.planet,r.metersPerUnit,eye,land,water);p.waitForCapture();p.publish(r.identity,land,water);},std::runtime_error);
        }
        old.unchanged(p,grass,land,water);EXPECT_FALSE(p.pending());EXPECT_EQ(p.reservedBytes(),old.bytes);
        for(GLuint buffer:generated) EXPECT_FALSE(glIsBuffer(buffer));
    }
    ASSERT_TRUE(p.submitGrass(0,r.planet,r.metersPerUnit,eye,land,water));p.waitForCapture();auto stale=r.identity;++stale.epoch;
    EXPECT_FALSE(p.publish(stale,land,water));old.unchanged(p,grass,land,water);
    EXPECT_EQ(p.stats().failed,4u);EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
}
TEST(TerrainPublication, AdaptiveAdmissionFitsCompleteOverlapAndLocksEffectiveReplay) {
    auto r=request(false);r.planet.foliage.max_blades=1000000;
    TerrainCompute compute;ProceduralGrass grass;grass.enableAdaptiveBudget();OwnedMesh land,water;
    auto build=buildTerrainCpu(r);auto disabled=r.planet;disabled.foliage.enabled=false;
    const auto block=compute.limits().blockBytes;
    auto minimal=r.planet.foliage;minimal.max_blades=1;
    const auto fixed=TerrainGpuPreparation::requiredBytes(build,r.identity,disabled,compute.limits())+
        ProceduralGrass::stageBytes(build.topology->triangleCount(),minimal,block)-grassComputedCandidateBytes;
    const auto allowance=fixed+128*grassComputedCandidateBytes+17;constexpr std::uint64_t external=1024*1024;
    TerrainPublication p(grass,1,external+2*allowance,allowance);p.reserveExternal(external);
    GrassBudgetSignals signals;signals.nowNs=signals.memoryNs=10000000000ull;signals.freeBytes=1024ull*1024*1024;
    signals.sampleReserved=external;signals.capBytes=external+allowance;grass.observeBudget(signals);
    ASSERT_TRUE(p.submit(std::move(build),r.identity,r.planet,r.metersPerUnit,r.identity.eye,land,water,compute));
    EXPECT_LE(p.reservedBytes(),external+allowance);p.waitForCapture();ASSERT_TRUE(p.publish(r.identity,land,water));retire(p,0);
    const auto policy=grass.policy(0);ASSERT_FALSE(policy.is_null());EXPECT_EQ(policy.at("capacity"),128);
    EXPECT_EQ(p.installed(0).foliage.max_blades,128);EXPECT_LE(grass.stats(0).candidates,128u);
    EXPECT_TRUE(policy.at("near_infeasible").get<bool>());
    auto saved=policy;saved["body"]=r.planet.name;
    nlohmann::json replay={{"render",{{"foliage_policy",nlohmann::json::array({saved})}}}};
    ProceduralGrass restored;restored.enableAdaptiveBudget();restored.restorePolicies(replay,{r.planet});
    auto generous=signals;generous.capBytes.reset();restored.observeBudget(generous);
    const auto fitted=restored.budgetPlanet(r.planet,land.indexCount/3,block,0,allowance*2,land.terrainStats.gpuWorkingBytes);
    EXPECT_EQ(fitted.foliage.max_blades,128); // Replay ignores a more generous live budget.
    generous.freeBytes=0;restored.observeBudget(generous);
    EXPECT_THROW(restored.budgetPlanet(r.planet,land.indexCount/3,block,0,allowance*2,land.terrainStats.gpuWorkingBytes),std::runtime_error);
    const auto before=Snapshot(p,grass,land,water);
    EXPECT_THROW(p.submitGrass(0,r.planet,r.metersPerUnit,r.identity.eye,land,water),std::runtime_error);
    before.unchanged(p,grass,land,water);EXPECT_FALSE(p.pending());
    // Missing telemetry remains usable within its conservative stage ceiling.
    ProceduralGrass missing;missing.enableAdaptiveBudget();
    const auto fallback=missing.budgetPlanet(r.planet,land.indexCount/3,block,0,1024ull*1024*1024,land.terrainStats.gpuWorkingBytes);
    EXPECT_LE(land.terrainStats.gpuWorkingBytes+ProceduralGrass::stageBytes(land.indexCount/3,fallback.foliage,block),64ull*1024*1024);
    EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
}
TEST(TerrainPublication, EquivalentTelemetryPolicyReusesLivePlanButChangedBudgetsAndInputsDoNot) {
    auto r=request(false);r.planet.foliage.max_blades=4096;
    TerrainCompute compute;ProceduralGrass grass;grass.enableAdaptiveBudget();OwnedMesh land,water;
    TerrainPublication p(grass,1);
    GrassBudgetSignals signals;signals.nowNs=signals.memoryNs=10000000000ull;
    signals.freeBytes=1024ull*1024*1024;grass.observeBudget(signals);
    ASSERT_TRUE(p.submit(buildTerrainCpu(r),r.identity,r.planet,r.metersPerUnit,r.identity.eye,land,water,compute));
    p.waitForCapture();ASSERT_TRUE(p.publish(r.identity,land,water));retire(p,0);
    const auto old=Snapshot(p,grass,land,water);const auto submitted=p.stats().submitted;
    signals.nowNs+=3000000000ull;signals.memoryNs=signals.nowNs;signals.sampleReserved=p.reservedBytes();
    signals.freeBytes=2ull*1024*1024*1024;grass.observeBudget(signals);
    ASSERT_TRUE(grass.policyChanged(0));
    ASSERT_TRUE(p.reuseGrassPolicy(0,r.planet,r.metersPerUnit,land,water));
    EXPECT_FALSE(grass.policyChanged(0));EXPECT_FALSE(p.pending());EXPECT_EQ(p.stats().submitted,submitted);
    old.unchanged(p,grass,land,water);
    auto changed=r.planet;changed.foliage.max_blades=2048;
    EXPECT_FALSE(p.reuseGrassPolicy(0,changed,r.metersPerUnit,land,water));
    changed=r.planet;changed.foliage.width_m+=.01;
    EXPECT_FALSE(p.reuseGrassPolicy(0,changed,r.metersPerUnit,land,water));
    changed=r.planet;changed.foliage.green_ratio+=.1;
    EXPECT_FALSE(p.reuseGrassPolicy(0,changed,r.metersPerUnit,land,water));
    for(unsigned i=0;i<3;++i) {
        signals.nowNs+=3000000000ull;signals.memoryNs=signals.gpuNs=signals.nowNs;
        signals.gpuMs=60;signals.utilization=100;signals.gpuSample=i+1;grass.observeBudget(signals);
    }
    EXPECT_TRUE(grass.policyChanged(0));
    EXPECT_FALSE(p.reuseGrassPolicy(0,r.planet,r.metersPerUnit,land,water));
    old.unchanged(p,grass,land,water);
    signals.nowNs+=3000000000ull;signals.memoryNs=signals.nowNs;signals.freeBytes=0;grass.observeBudget(signals);
    EXPECT_FALSE(p.reuseGrassPolicy(0,r.planet,r.metersPerUnit,land,water));
    old.unchanged(p,grass,land,water);EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
}

TEST(TerrainPublication, ReplayPhaseStagesPrivatelyAndPublishesSharedLandGrassAndContactVersion) {
    auto r=preparedRequest({.3,-.7,1});auto& p=r.planet;
    const auto topology=r.surface.buildTopologyForEye(r.identity.eye,{},nullptr,20);
    const TerrainMorphFootprint phase{glm::normalize(r.identity.eye),1000,.2,.4};
    TerrainCompute compute;ProceduralGrass grass;grass.enableAdaptiveBudget();
    TerrainPublication publication(grass,1);OwnedMesh land,water;
    struct ProgramScope {~ProgramScope(){glUseProgram(0);}} programScope;
    for(const bool active:{false,true}) {
        SCOPED_TRACE(active);r.deformation=TerrainMorphReplay{topology.generation,1000,active?std::optional(phase):std::nullopt};
        const auto oldState=land.morphState;const auto oldRevision=land.revision;
        auto built=buildTerrainCpu(r);const auto preview=built.previewPhase();EXPECT_EQ(bool(preview),active);
        ASSERT_TRUE(publication.submit(std::move(built),r.identity,p,1000,r.identity.eye,land,water,compute));
        EXPECT_EQ(land.revision,oldRevision);EXPECT_EQ(land.morphState,oldState);
        publication.waitForCapture();EXPECT_EQ(land.revision,oldRevision);EXPECT_EQ(land.morphState,oldState);
        EXPECT_FALSE(publication.updatePreparedPhase(0,phase,compute)); // Static replay has no moving halo admission.
        ASSERT_TRUE(publication.publish(r.identity,land,water));retire(publication,0);
        ASSERT_TRUE(land.morphState);EXPECT_EQ(land.deformationRevision(),active?1u:0u);
        EXPECT_EQ(grass.residentDeformationRevision(0),land.deformationRevision());
        ASSERT_GT(grass.stats(0).candidates,0u);EXPECT_TRUE(land.morphSource());
        if(active) {
            EXPECT_EQ(*land.morphState->footprint(),phase);ASSERT_TRUE(oldState);
            EXPECT_NE(land.morphState,oldState);EXPECT_EQ(oldState->revision(),0u);
            EXPECT_EQ(*preview->footprint(),*land.morphState->footprint());
        }
        grass.shader.use();const glm::mat4 identity(1);
        for(const char* name:{"model","view","projection"}) grass.shader.setMat4(name,&identity[0][0]);
        grass.shader.setFloat("uMetersPerRadius",1000);grass.shader.setFloat("uGrassHeight",1);
        grass.shader.setFloat("uGrassWidth",.08);grass.shader.setFloat("uWindStrength",0);
        GrassPass pass;pass.mainEyeBody=r.identity.eye;
        glEnable(GL_RASTERIZER_DISCARD);grass.draw(0,&pass);glDisable(GL_RASTERIZER_DISCARD);
        EXPECT_EQ(grass.drawnDeformationRevision(0),land.deformationRevision());
        const auto expected=active?r.surface.evaluateTopology(topology,phase):r.surface.evaluateTopology(topology);
        SurfaceContact actual,rendered;actual.bind(land.contacts,land.revision,land.morphState);
        rendered.bind(expected.vertices,expected.indices,land.revision,1000);
        const auto missing=[](const auto&) -> GroundContact {throw std::logic_error("Missing replay publication contact");};
        EXPECT_LE(glm::length(actual.sample(glm::normalize(r.identity.eye),missing).position-
            rendered.sample(glm::normalize(r.identity.eye),missing).position),.002);
        ++r.identity.serial;
    }
    auto invalid=buildTerrainCpu(r);invalid.deformation->generation.topology++;
    const auto state=land.morphState;const auto revision=land.revision;
    EXPECT_THROW(publication.submit(std::move(invalid),r.identity,p,1000,r.identity.eye,land,water,compute),std::invalid_argument);
    EXPECT_EQ(land.morphState,state);EXPECT_EQ(land.revision,revision);EXPECT_FALSE(publication.pending());
    EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
}

TEST(TerrainPublication, PreparedMovingPhaseTransfersGpuPlanesGrassAndContactsTogether) {
    const auto read=[](GLuint buffer,void* data,GLsizeiptr bytes,GLintptr offset=0) {
        GLint previous=0;glGetIntegerv(GL_COPY_READ_BUFFER_BINDING,&previous);
        glBindBuffer(GL_COPY_READ_BUFFER,buffer);glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
        glGetBufferSubData(GL_COPY_READ_BUFFER,offset,bytes,data);glBindBuffer(GL_COPY_READ_BUFFER,previous);
    };
    const auto gpuMesh=[&](const Mesh& mesh) {
        TerrainGeometry geometry;geometry.vertices.resize(mesh.indexCount*9);geometry.indices.resize(mesh.indexCount);
        read(mesh.vbo,geometry.vertices.data(),geometry.vertices.size()*sizeof(float));
        read(mesh.ebo,geometry.indices.data(),geometry.indices.size()*sizeof(unsigned));return geometry;
    };
    const GroundQuery missing=[](const auto&) -> GroundContact {throw std::logic_error("Missing prepared GPU plane");};
    std::size_t swaps=0,probes=0,roots=0,visibleRoots=0;double maxSwap=0,maxContact=0,maxRoot=0;
    TerrainCompute compute;
    for(const auto anchor:{glm::dvec3(-.21005697617230545,.909856724515421,-.36613409503800903),glm::dvec3(0,0,1.002)}) {
        auto r=preparedRequest(anchor);r.planet.foliage.root_offset_m=0;
        const auto east=glm::normalize(glm::cross(anchor,glm::dvec3(0,1,0))),north=glm::normalize(glm::cross(anchor,east));
        ProceduralGrass grass;grass.enableAdaptiveBudget();TerrainPublication publication(grass,1);OwnedMesh land,water;
        struct ProgramScope {~ProgramScope(){glUseProgram(0);}} programScope;
        for(const auto offset:{glm::dvec2(0,0),glm::dvec2(.15,0),glm::dvec2(.3,0),glm::dvec2(.3,.3),glm::dvec2(0,.3),glm::dvec2(0,0)}) {
            r.identity.eye=anchor+(east*offset.x+north*offset.y)/1000.;
            const TerrainMorphFootprint footprint{glm::normalize(r.identity.eye),1000,.2,.4};
            std::optional<TerrainGeometry> before;
            if(land.morphState) {compute.deform(land,footprint);before=gpuMesh(land);}
            const auto oldBuffer=land.vbo,oldTargets=land.morphTargets;const auto oldRevision=land.revision;
            const auto oldState=land.morphState;const auto oldPhase=land.deformationRevision();
            auto initial=footprint;initial.eyeRadial=glm::normalize(footprint.eyeRadial-east*.0001);
            r.preparedPhase=initial;
            auto built=buildTerrainCpu(r);ASSERT_FALSE(built.topology->localDetailLimited);
            if(before) ASSERT_TRUE(built.topology->parentReused);
            ASSERT_TRUE(built.deformation);EXPECT_EQ(built.deformation->phase,initial);
            EXPECT_TRUE(built.deformation->matches(built.topology->generation,1000));
            ASSERT_TRUE(publication.submit(std::move(built),r.identity,r.planet,1000,r.identity.eye,land,water,compute));
            EXPECT_FALSE(publication.updatePreparedPhase(0,footprint,compute));
            publication.waitForCapture();
            EXPECT_EQ(land.vbo,oldBuffer);EXPECT_EQ(land.morphTargets,oldTargets);EXPECT_EQ(land.revision,oldRevision);
            EXPECT_EQ(land.morphState,oldState);EXPECT_EQ(land.deformationRevision(),oldPhase);
            auto outside=footprint;outside.transitionMeters=2;
            EXPECT_THROW(publication.updatePreparedPhase(0,outside,compute),std::invalid_argument);
            EXPECT_THROW(publication.updatePreparedPhase(1,footprint,compute),std::out_of_range);
            ASSERT_TRUE(publication.ready());
            {BufferProbe buffers;FenceDelay noWait;FenceFault noFence(1);
                EXPECT_TRUE(publication.updatePreparedPhase(0,footprint,compute));
                {DispatchFault noDispatch;EXPECT_TRUE(publication.updatePreparedPhase(0,footprint,compute));}
                EXPECT_TRUE(generated.empty());EXPECT_EQ(pollCalls,0u);EXPECT_EQ(fenceCalls,0u);
            }
            EXPECT_EQ(land.morphState,oldState);EXPECT_EQ(land.deformationRevision(),oldPhase);
            if(before) EXPECT_EQ(grass.residentDeformationRevision(0),oldPhase);
            ASSERT_TRUE(publication.publish(r.identity,land,water));
            EXPECT_EQ(land.revision,oldRevision+1);ASSERT_TRUE(land.morphState);
            EXPECT_EQ(*land.morphState->footprint(),footprint);EXPECT_EQ(land.deformationRevision(),2u);
            EXPECT_DOUBLE_EQ(publication.installed(0).preparedRadiusMeters,1.5);
            EXPECT_EQ(grass.residentGeneration(0),land.terrainStats.generation);
            EXPECT_EQ(grass.residentRevision(0),land.revision);EXPECT_EQ(grass.residentDeformationRevision(0),2u);
            if(oldState) {EXPECT_EQ(oldState->revision(),oldPhase);EXPECT_EQ(*oldState->footprint(),footprint);}
            const auto after=gpuMesh(land);SurfaceContact contact,rendered,previous;
            contact.bind(land.contacts,land.revision,land.morphState);
            rendered.bind(after.vertices,after.indices,land.revision,1000);
            if(before) previous.bind(before->vertices,before->indices,oldRevision,1000);
            for(const double distance:{.137,.199,.201,.317,.499,.599,.601,.83,1.13,1.49,1.51,1.83,2.1,3.17,10.})
                for(int i=0;i<7;++i) {
                    const double angle=(i+.317)*2*std::numbers::pi/7;
                    const auto radial=glm::normalize(footprint.eyeRadial+
                        (east*std::cos(angle)+north*std::sin(angle))*(distance/1000));
                    const auto actual=rendered.sample(radial,missing);
                    maxContact=std::max(maxContact,glm::length(actual.position-contact.sample(radial,missing).position));
                    if(before) maxSwap=std::max(maxSwap,glm::length(actual.position-previous.sample(radial,missing).position));++probes;
                }
            grass.shader.use();const glm::mat4 identity(1);
            for(const char* name:{"model","view","projection"}) grass.shader.setMat4(name,&identity[0][0]);
            grass.shader.setFloat("uMetersPerRadius",1000);grass.shader.setFloat("uGrassHeight",1);
            grass.shader.setFloat("uGrassWidth",.08);grass.shader.setFloat("uWindStrength",0);
            GrassPass pass;pass.mainEyeBody=r.identity.eye;
            glEnable(GL_RASTERIZER_DISCARD);grass.draw(0,&pass);glDisable(GL_RASTERIZER_DISCARD);
            EXPECT_EQ(grass.drawnDeformationRevision(0),2u);
            struct Blade {glm::vec4 root,up,variation;glm::vec3 wind;unsigned index;};static_assert(sizeof(Blade)==64);
            GLint buffer=0;glGetIntegeri_v(GL_SHADER_STORAGE_BUFFER_BINDING,2,&buffer);
            const auto counts=grass.computedCounts(0);ASSERT_GT(counts[0]+counts[1],0u);
            visibleRoots+=counts[0]+counts[1];
            for(unsigned queue=0;queue<2;++queue) {
                std::vector<Blade> blades(counts[queue]);if(blades.empty()) continue;
                read(buffer,blades.data(),blades.size()*sizeof(Blade),queue*grass.stats(0).candidates*sizeof(Blade));
                // Bound full-mesh ray searches, with a spread across both draw
                // queues rather than only checking their first patch.
                const auto sampled=std::min<std::size_t>(64,blades.size());
                for(std::size_t i=0;i<sampled;++i) {
                    const auto& blade=blades[i*blades.size()/sampled];
                    const glm::dvec3 root(blade.root);maxRoot=std::max(maxRoot,
                        glm::length(root*1000.0-rendered.sample(glm::normalize(root),missing).position));++roots;
                }
            }
            if(before) ++swaps;retire(publication,0);++r.identity.serial;
            std::cout<<"PREPARED_GPU_STEP publications="<<r.identity.serial-1<<" sampled_roots="<<roots<<std::endl;
        }
        ASSERT_TRUE(publication.submitGrass(0,r.planet,1000,r.identity.eye,land,water));
        publication.waitForCapture();
        EXPECT_FALSE(publication.updatePreparedPhase(0,*land.morphState->footprint(),compute));
        publication.cancel();EXPECT_EQ(land.deformationRevision(),2u);
    }
    EXPECT_EQ(swaps,10u);EXPECT_EQ(probes,1260u);EXPECT_GT(roots,0u);
    EXPECT_LE(maxSwap,.001);EXPECT_LE(maxContact,.002);EXPECT_LE(maxRoot,.002);
    std::cout<<"PREPARED_GPU swaps="<<swaps<<" probes="<<probes<<" sampled_roots="<<roots<<" visible_roots="<<visibleRoots
        <<" max_swap_m="<<maxSwap<<" max_contact_m="<<maxContact<<" max_root_m="<<maxRoot<<'\n';
    EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
}

TEST(TerrainPublication, PreparedPhaseDispatchFailureDiscardsPrivateWritesAndKeepsLiveConsumersUsable) {
    auto r=preparedRequest({.3,-.7,1});const auto anchor=glm::normalize(r.identity.eye);
    const auto east=glm::normalize(glm::cross(anchor,glm::dvec3(0,1,0)));
    r.preparedPhase=TerrainMorphFootprint{anchor,1000,.2,.4};
    TerrainCompute compute;ProceduralGrass grass;grass.enableAdaptiveBudget();TerrainPublication publication(grass,1);
    OwnedMesh land,water;EXPECT_FALSE(publication.updatePreparedPhase(0,*r.preparedPhase,compute));
    ASSERT_TRUE(submit(publication,r,land,water,compute,r.identity.eye));publication.waitForCapture();
    ASSERT_TRUE(publication.publish(r.identity,land,water));retire(publication,0);
    const Snapshot old(publication,grass,land,water);const auto state=land.morphState;
    const auto livePositions=[&] {
        GLint previous=0;glGetIntegerv(GL_COPY_READ_BUFFER_BINDING,&previous);
        glBindBuffer(GL_COPY_READ_BUFFER,land.vbo);glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
        std::vector<float> values(land.indexCount*9);
        glGetBufferSubData(GL_COPY_READ_BUFFER,0,values.size()*sizeof(float),values.data());
        glBindBuffer(GL_COPY_READ_BUFFER,previous);return values;
    };
    const auto oldPositions=livePositions();
    ASSERT_TRUE(state);EXPECT_EQ(state->revision(),1u);
    ++r.identity.serial;r.identity.eye+=east*.00015;
    auto latest=*r.preparedPhase;latest.eyeRadial=glm::normalize(r.identity.eye+east*.0001);
    {BufferProbe buffers;
        const auto original=buildTerrainCpu(r);
        for(const double radius:{std::numeric_limits<double>::quiet_NaN(),4.0}) {
            auto malformed=original;malformed.preparedRadiusMeters=radius;
            EXPECT_THROW(publication.submit(std::move(malformed),r.identity,r.planet,1000,r.identity.eye,land,water,compute),std::invalid_argument);
        }
        EXPECT_TRUE(generated.empty());old.unchanged(publication,grass,land,water);
        ASSERT_TRUE(submit(publication,r,land,water,compute,r.identity.eye));publication.waitForCapture();
        {DispatchFault fault;EXPECT_THROW(publication.updatePreparedPhase(0,latest,compute),std::runtime_error);}
        EXPECT_FALSE(publication.pending());EXPECT_TRUE(publication.canSubmit(0));
        old.unchanged(publication,grass,land,water);EXPECT_EQ(land.morphState,state);
        EXPECT_EQ(livePositions(),oldPositions);
        EXPECT_EQ(state->revision(),1u);EXPECT_EQ(*state->footprint(),*r.preparedPhase);
        EXPECT_EQ(publication.reservedBytes(),old.bytes);
        for(const auto buffer:generated) EXPECT_FALSE(glIsBuffer(buffer));
    }
    EXPECT_EQ(publication.stats().failed,3u);EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
    ASSERT_TRUE(submit(publication,r,land,water,compute,r.identity.eye));publication.waitForCapture();
    ASSERT_TRUE(publication.updatePreparedPhase(0,latest,compute));
    ASSERT_TRUE(publication.publish(r.identity,land,water));retire(publication,0);
    EXPECT_EQ(land.revision,old.landRevision+1);ASSERT_TRUE(land.morphState);
    EXPECT_EQ(land.deformationRevision(),2u);EXPECT_EQ(*land.morphState->footprint(),latest);
    EXPECT_EQ(grass.residentDeformationRevision(0),2u);EXPECT_EQ(state->revision(),1u);
    EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
}

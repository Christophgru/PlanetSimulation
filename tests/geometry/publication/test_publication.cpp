#include "rendering/geometry/publication/TerrainPublication.h"
#include "rendering/geometry/contacts/SparseTerrainContacts.h"
#include "config/Config.h"
#include <gtest/gtest.h>
#include <chrono>
#include <thread>

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

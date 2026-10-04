#include "rendering/geometry/compute/TerrainGpuPreparation.h"
#include "rendering/foliage/procedural/ProceduralGrass.h"
#include "rendering/foliage/planning/GrassAllocation.h"
#include "rendering/geometry/Mesh.h"
#include "config/Config.h"
#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <limits>

using namespace rendering;
namespace {
TerrainBuildRequest request(bool water=false,bool foliage=true) {
    config::ScenarioConfig scene(config::Config::load("tests/scenarios/foliage/surface.json"));
    auto p=scene.planets[0];p.water.enabled=water;p.foliage.enabled=foliage;p.foliage.max_blades=512;
    p.terrain_lod.max_triangle_budget=10000;p.terrain_lod.shoreline_edge_m=0;
    TerrainSurface surface(p.surface_noise,p.terrain_lod,p.radius,scene.metersPerWorldUnit(),
        p.terrain_landscape,p.water.level_m,p.terrain_material);
    TerrainBuildIdentity key;key.serial=1;key.bodyName=p.name;key.field=surface.field().fingerprint();
    key.backend=TerrainBackend::Compute;key.resident=true;key.eye={1.012,0,0};key.localMask=1;
    return {key,std::move(surface),p,{},scene.metersPerWorldUnit()};
}
bool finishByPolling(TerrainGpuPreparation& terrain,ProceduralGrass& grass,ProceduralGrass::Preparation& foliage) {
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(10);glFlush();
    do {
        const bool a=terrain.poll(),b=grass.poll(foliage);
        if(a && b) return true;
        glFlush();std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while(std::chrono::steady_clock::now()<end);
    return false;
}
struct OwnedMesh:Mesh {~OwnedMesh(){destroy();}};
unsigned delayedPolls=0,blockingPolls=0;
GLenum GLAPIENTRY delayedFence(GLsync,GLbitfield flags,GLuint64 timeout) {
    ++delayedPolls;if(flags || timeout) ++blockingPolls;
    return GL_TIMEOUT_EXPIRED;
}
struct FenceDelay {
    PFNGLCLIENTWAITSYNCPROC previous=__glewClientWaitSync;
    FenceDelay() {delayedPolls=blockingPolls=0;__glewClientWaitSync=delayedFence;}
    ~FenceDelay() {__glewClientWaitSync=previous;}
};
}
TEST(GpuPreparation, ChainsGrassBeforeTerrainPollingAndPreallocatesDrawResources) {
    auto r=request();TerrainCompute compute;ProceduralGrass grass;OwnedMesh mesh;
    TerrainGpuPreparation terrain(buildTerrainCpu(r),r.identity,r.planet,compute);
    EXPECT_FALSE(terrain.land->complete);EXPECT_FALSE(terrain.ready);
    auto foliage=grass.submitResident(terrain.land->vbo,terrain.land->ebo,terrain.land->stats,r.planet,
        r.metersPerUnit,r.identity.eye,1,terrain.land->stats.gpuWorkingBytes);
    grass.reserve(0);EXPECT_FALSE(foliage->ready());EXPECT_EQ(foliage->stats().summaryReadBytes,0u);
    EXPECT_THROW(grass.commit(0,*foliage,mesh),std::invalid_argument);EXPECT_EQ(grass.stats(0).candidates,0u);
    ASSERT_TRUE(finishByPolling(terrain,grass,*foliage));const auto prepared=foliage->stats();
    EXPECT_GT(prepared.candidates,0u);EXPECT_LE(prepared.candidates,512u);
    EXPECT_TRUE(prepared.drawResourcesPrepared);EXPECT_EQ(prepared.drawResourceBytes,prepared.candidates*128+32);
    EXPECT_EQ(prepared.summaryReadBytes,224u);EXPECT_EQ(prepared.metadataReadBytes,0u);
    EXPECT_LE(terrain.admittedBytes,ProceduralGrass::defaultStageBytes);
    EXPECT_GE(terrain.admittedBytes,prepared.gpuBytes+terrain.land->stats.gpuWorkingBytes);
    EXPECT_TRUE(grass.poll(*foliage));EXPECT_EQ(foliage->stats().summaryReadBytes,224u);
    mesh.loadComputedTerrain(std::move(terrain.cpu.geometry),*terrain.land,false);
    grass.commit(0,*foliage,mesh);EXPECT_EQ(grass.stats(0).candidates,prepared.candidates);
    EXPECT_TRUE(mesh.vertices.empty());EXPECT_TRUE(mesh.indices.empty());
    GrassPass pass;pass.mainEyeBody=r.identity.eye;grass.shader.use();
    glEnable(GL_RASTERIZER_DISCARD);grass.draw(0,&pass);glDisable(GL_RASTERIZER_DISCARD);
    EXPECT_EQ(grass.stats(0).drawResourceBytes,prepared.drawResourceBytes);
    EXPECT_EQ(grass.stats(0).summaryReadBytes,224u);
    EXPECT_THROW(grass.commit(0,*foliage,mesh),std::invalid_argument);
    EXPECT_THROW(grass.poll(*foliage),std::logic_error);EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
    glUseProgram(0); // Release the test draw's program before its owner dies.
}
TEST(GpuPreparation, PreviousPatchSurvivesDelayedConsumptionBudgetFailureAndStaleCommit) {
    auto r=request();TerrainCompute compute;ProceduralGrass grass;OwnedMesh mesh;
    TerrainGpuPreparation terrain(buildTerrainCpu(r),r.identity,r.planet,compute);terrain.waitForCapture();
    mesh.loadComputedTerrain(std::move(terrain.cpu.geometry),*terrain.land,false);
    grass.prepare(0,mesh,r.planet,r.metersPerUnit,r.identity.eye);const auto before=grass.stats(0);
    const auto eye=r.identity.eye+glm::dvec3(0,.02,0);
    const auto submit=[&](std::uint64_t limit) {return grass.submitResident(mesh.vbo,mesh.ebo,mesh.terrainStats,
        r.planet,r.metersPerUnit,eye,mesh.revision,mesh.terrainStats.gpuWorkingBytes,limit);};
    const auto bytes=mesh.terrainStats.gpuWorkingBytes+ProceduralGrass::stageBytes(mesh.indexCount/3,r.planet.foliage,compute.limits().blockBytes);
    EXPECT_THROW(submit(bytes-1),std::runtime_error);auto staged=submit(bytes);
    EXPECT_EQ(staged->admittedBytes,bytes);EXPECT_EQ(grass.planningEye(0),r.identity.eye);
    EXPECT_EQ(grass.stats(0).gpuBytes,before.gpuBytes);EXPECT_EQ(grass.stats(0).summaryReadBytes,before.summaryReadBytes);
    EXPECT_THROW(grass.commit(0,*staged,mesh),std::invalid_argument);
    grass.waitForCapture(*staged);EXPECT_EQ(grass.planningEye(0),r.identity.eye);
    ++mesh.revision;EXPECT_THROW(grass.commit(0,*staged,mesh),std::invalid_argument);--mesh.revision;
    ++mesh.terrainStats.generation.field;EXPECT_THROW(grass.commit(0,*staged,mesh),std::invalid_argument);--mesh.terrainStats.generation.field;
    EXPECT_THROW(grass.commit(12,*staged,mesh),std::invalid_argument);EXPECT_EQ(grass.stats(0).candidates,before.candidates);
    grass.commit(0,*staged,mesh);EXPECT_EQ(grass.planningEye(0),eye);EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
}
TEST(GpuPreparation, DisabledGrassAndWaterKeepExplicitMatchingConsumers) {
    auto r=request(true,false);r.planet.foliage.compute_placement=false;
    TerrainCompute compute;ProceduralGrass grass;OwnedMesh mesh,water;
    TerrainGpuPreparation terrain(buildTerrainCpu(r),r.identity,r.planet,compute);
    ASSERT_TRUE(terrain.water);EXPECT_NE(terrain.water->stats.generation.field,terrain.land->stats.generation.field);
    auto empty=grass.submitResident(terrain.land->vbo,terrain.land->ebo,terrain.land->stats,r.planet,
        r.metersPerUnit,r.identity.eye,1,terrain.land->stats.gpuWorkingBytes+terrain.water->stats.gpuWorkingBytes);
    EXPECT_TRUE(empty->ready());EXPECT_EQ(empty->stats().gpuBytes,0u);EXPECT_EQ(empty->stats().summaryReadBytes,0u);
    ASSERT_TRUE(finishByPolling(terrain,grass,*empty));
    water.loadComputedTerrain(std::move(*terrain.cpu.water),*terrain.water,false);
    mesh.loadComputedTerrain(std::move(terrain.cpu.geometry),*terrain.land,false);grass.reserve(0);grass.commit(0,*empty,mesh);
    EXPECT_EQ(grass.stats(0).candidates,0u);EXPECT_EQ(grass.stats(0).allocationBytes,0u);
    EXPECT_TRUE(water.vertices.empty());EXPECT_TRUE(mesh.vertices.empty());EXPECT_TRUE(mesh.contacts);
    EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
}
TEST(GpuPreparation, MissingConsumersAndGenerationByteLimitsPreservePreviousMesh) {
    auto r=request();TerrainCompute compute;OwnedMesh mesh;
    TerrainGpuPreparation good(buildTerrainCpu(r),r.identity,r.planet,compute);good.waitForCapture();
    const auto admitted=good.admittedBytes;mesh.loadComputedTerrain(std::move(good.cpu.geometry),*good.land,false);
    const auto vbo=mesh.vbo;const auto revision=mesh.revision;
    EXPECT_THROW(TerrainGpuPreparation(buildTerrainCpu(r),r.identity,r.planet,compute,admitted-1),std::runtime_error);
    auto broken=buildTerrainCpu(r);broken.contacts.reset();
    EXPECT_THROW(TerrainGpuPreparation(std::move(broken),r.identity,r.planet,compute),std::invalid_argument);
    auto wet=request(true);broken=buildTerrainCpu(wet);broken.waterTopology.reset();
    EXPECT_THROW(TerrainGpuPreparation(std::move(broken),wet.identity,wet.planet,compute),std::invalid_argument);
    auto limits=compute.limits();limits.blockBytes=703;TerrainCompute tiny(limits);
    EXPECT_THROW(TerrainGpuPreparation(buildTerrainCpu(r),r.identity,r.planet,tiny),std::runtime_error);
    EXPECT_EQ(mesh.vbo,vbo);EXPECT_EQ(mesh.revision,revision);EXPECT_TRUE(glIsBuffer(vbo));
    EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
}
TEST(GpuPreparation, CorruptSummaryPrefixesSlotsAndCandidateTotalsAreRejected) {
    auto r=request();TerrainCompute compute;
    TerrainGpuPreparation terrain(buildTerrainCpu(r),r.identity,r.planet,compute);terrain.waitForCapture();
    GrassMetadataCompute metadataCompute;auto metadata=metadataCompute.generate(terrain.land->vbo,terrain.land->ebo,
        terrain.land->stats,r.planet,r.metersPerUnit,r.identity.eye);
    GrassAllocationCompute allocationCompute;auto allocation=allocationCompute.generate(*metadata,r.planet.foliage);
    allocation->waitForCapture();const auto valid=allocation->readSummary();
    const auto reject=[&](auto mutate) {
        auto bad=valid;mutate(bad);glBindBuffer(GL_COPY_WRITE_BUFFER,allocation->state);
        glBufferSubData(GL_COPY_WRITE_BUFFER,0,sizeof(bad),&bad);glBindBuffer(GL_COPY_WRITE_BUFFER,0);
        EXPECT_THROW(allocation->readSummary(),std::runtime_error);
    };
    reject([](auto& s){++s.first[16];});reject([](auto& s){++s.counts[0];});
    reject([](auto& s){++s.totals[1];});reject([](auto& s){++s.control[0];});
    reject([](auto& s){++s.control[1];});
    reject([](auto& s){s.densitySearch[0]=std::numeric_limits<double>::quiet_NaN();});
    reject([](auto& s){s.status[2]=1;s.status[3]=0;});
    glBindBuffer(GL_COPY_WRITE_BUFFER,allocation->state);glBufferSubData(GL_COPY_WRITE_BUFFER,0,sizeof(valid),&valid);
    glBindBuffer(GL_COPY_WRITE_BUFFER,0);EXPECT_EQ(allocation->readSummary().totals,valid.totals);
    EXPECT_EQ(allocation->diagnosticReadBytes,0u);EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
}
TEST(GpuPreparation, SubmissionAndResourcePollingRestoreContextBindingsAndRanges) {
    auto r=request();TerrainCompute compute;ProceduralGrass grass;
    GLuint buffer=0,vao=0;glGenBuffers(1,&buffer);glGenVertexArrays(1,&vao);
    GLint alignment=0;glGetIntegerv(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT,&alignment);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER,buffer);glBufferData(GL_SHADER_STORAGE_BUFFER,alignment*8+1024,nullptr,GL_STATIC_DRAW);
    for(int i=0;i<7;++i) glBindBufferRange(GL_SHADER_STORAGE_BUFFER,i,buffer,alignment*i,256);
    glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,buffer);grass.shader.use();glActiveTexture(GL_TEXTURE3);
    TerrainGpuPreparation terrain(buildTerrainCpu(r),r.identity,r.planet,compute);
    auto staged=grass.submitResident(terrain.land->vbo,terrain.land->ebo,terrain.land->stats,r.planet,
        r.metersPerUnit,r.identity.eye,1,terrain.land->stats.gpuWorkingBytes);
    ASSERT_TRUE(finishByPolling(terrain,grass,*staged));
    for(int i=0;i<7;++i) {
        GLint id=0;GLint64 start=0,size=0;glGetIntegeri_v(GL_SHADER_STORAGE_BUFFER_BINDING,i,&id);
        glGetInteger64i_v(GL_SHADER_STORAGE_BUFFER_START,i,&start);glGetInteger64i_v(GL_SHADER_STORAGE_BUFFER_SIZE,i,&size);
        EXPECT_EQ(id,buffer);EXPECT_EQ(start,alignment*i);EXPECT_EQ(size,256);
    }
    GLint bound=0;glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&bound);EXPECT_EQ(bound,vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&bound);EXPECT_EQ(bound,buffer);
    glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING,&bound);EXPECT_EQ(bound,buffer);
    glGetIntegerv(GL_CURRENT_PROGRAM,&bound);EXPECT_EQ(bound,grass.shader.id);
    glGetIntegerv(GL_ACTIVE_TEXTURE,&bound);EXPECT_EQ(bound,GL_TEXTURE3);
    glBindVertexArray(0);glBindBuffer(GL_ARRAY_BUFFER,0);glActiveTexture(GL_TEXTURE0);glUseProgram(0);
    glDeleteBuffers(1,&buffer);glDeleteVertexArrays(1,&vao);EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
}
TEST(GpuPreparation, DelayedFencePollsNeverWaitReadSummaryOrPublish) {
    auto r=request();TerrainCompute compute;ProceduralGrass grass;OwnedMesh mesh;
    TerrainGpuPreparation terrain(buildTerrainCpu(r),r.identity,r.planet,compute);
    auto staged=grass.submitResident(terrain.land->vbo,terrain.land->ebo,terrain.land->stats,r.planet,
        r.metersPerUnit,r.identity.eye,1,terrain.land->stats.gpuWorkingBytes);
    grass.reserve(0);
    {
        // Simulate delayed driver completion at the GL boundary. The actual GPU
        // work remains real; RAII restores the loader before normal completion.
        FenceDelay delay;
        for(int frame=0;frame<3;++frame) {
            EXPECT_FALSE(terrain.poll());EXPECT_FALSE(grass.poll(*staged));
            EXPECT_EQ(staged->stats().summaryReadBytes,0u);EXPECT_FALSE(staged->ready());
            EXPECT_THROW(grass.commit(0,*staged,mesh),std::invalid_argument);
            EXPECT_EQ(grass.stats(0).candidates,0u);
        }
        EXPECT_EQ(delayedPolls,6u);EXPECT_EQ(blockingPolls,0u);
    }
    ASSERT_TRUE(finishByPolling(terrain,grass,*staged));
    EXPECT_EQ(staged->stats().summaryReadBytes,224u);
    mesh.loadComputedTerrain(std::move(terrain.cpu.geometry),*terrain.land,false);
    grass.commit(0,*staged,mesh);EXPECT_GT(grass.stats(0).candidates,0u);
    EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
}

#pragma once
#include "rendering/runtime/RendererState.h"
#include "rendering/geometry/contacts/SparseTerrainContacts.h"
#include <GLFW/glfw3.h>
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

namespace rendering {
struct RendererRecoveryProbe {static auto& state(Renderer& r) {return *r.impl_;}};
}
namespace {
using Probe=rendering::RendererRecoveryProbe;
using nlohmann::json;
app::CommandLineOptions options(const std::string& name) {
    const auto out=std::filesystem::path(PLANET_TEST_OUTPUT)/name;
    std::filesystem::create_directories(out);
    json scene;std::ifstream(std::string(PLANET_SOURCE_DIR)+"/tests/scenarios/foliage/surface.json")>>scene;
    for(auto& p:scene["planets"]) p["terrain_lod"]["max_triangle_budget"]=10000;
    scene["planets"][0]["foliage"]["max_blades"]=128;
    scene["planets"][0]["foliage"]["rebuild_distance_fraction"]=0.01;
    scene["lighting"]["shadows"]["resolution"]=256;
    app::CommandLineOptions o;o.renderTestMode=o.surfaceRenderMode=o.thirdPersonRenderMode=o.captureOnly=true;
    o.terrainBackend="compute";o.terrainGrassPlanner="gpu-v1";
    o.configPath=(out/"scene.json").string();std::ofstream(o.configPath)<<scene.dump(2);
    o.outputImagePath=(out/"frame.png").string();o.renderTestWidth=320;o.renderTestHeight=180;return o;
}
PFNGLCLIENTWAITSYNCPROC originalPoll;
PFNGLWAITSYNCPROC originalWait;
PFNGLGETBUFFERSUBDATAPROC originalRead;
PFNGLFENCESYNCPROC originalFence;
bool delay=false,failPoll=false,failFence=false;
unsigned polls=0,blockingPolls=0,serverWaits=0,bulkReads=0;
GLenum GLAPIENTRY poll(GLsync sync,GLbitfield flags,GLuint64 timeout) {
    ++polls;if(flags || timeout) ++blockingPolls;
    if(failPoll) {failPoll=false;return GL_WAIT_FAILED;}
    if(delay) return GL_TIMEOUT_EXPIRED;
    return originalPoll(sync,flags,timeout);
}
void GLAPIENTRY wait(GLsync sync,GLbitfield flags,GLuint64 timeout) {
    ++serverWaits;originalWait(sync,flags,timeout);
}
void GLAPIENTRY read(GLenum target,GLintptr offset,GLsizeiptr size,void* data) {
    if(size>224) ++bulkReads;originalRead(target,offset,size,data);
}
GLsync GLAPIENTRY fence(GLenum condition,GLbitfield flags) {
    if(failFence) {failFence=false;return nullptr;}return originalFence(condition,flags);
}
struct GlAudit {
    GlAudit() {
        polls=blockingPolls=serverWaits=bulkReads=0;delay=failPoll=failFence=false;
        originalPoll=__glewClientWaitSync;originalWait=__glewWaitSync;
        originalRead=__glewGetBufferSubData;originalFence=__glewFenceSync;
        __glewClientWaitSync=poll;__glewWaitSync=wait;__glewGetBufferSubData=read;__glewFenceSync=fence;
    }
    ~GlAudit() {
        __glewClientWaitSync=originalPoll;__glewWaitSync=originalWait;
        __glewGetBufferSubData=originalRead;__glewFenceSync=originalFence;
        EXPECT_GT(polls,0u);EXPECT_EQ(blockingPolls,0u);EXPECT_EQ(serverWaits,0u);EXPECT_EQ(bulkReads,0u);
    }
    json state() const {return {{"fence_polls",polls},{"positive_timeout_or_flush_polls",blockingPolls},
        {"server_waits",serverWaits},{"bulk_readbacks",bulkReads}};}
};
template<class Tick,class Done> bool until(Tick tick,Done done) {
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(45);
    do {tick();if(done()) return true;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    while(std::chrono::steady_clock::now()<end);return false;
}
auto eye(auto& r) {return r.scene.surfaceCamera->position();}
void tick(auto& r) {r.preparePlanetMeshes(eye(r),true,0.0);}
bool idle(auto& r) {return r.residentSceneReady() && !r.terrainPublication->pending() && !r.terrainJobs.pending(r.terrainSceneEpoch);}
void draw(auto& r,bool character=true) {
    ASSERT_TRUE(r.residentSceneReady());
    character=character && r.scene.surfaceCamera.has_value();
    if(character) r.prepareAstronaut(0);
    const auto e=character ? r.astronautView.eye : r.scene.surfaceCamera ? eye(r) : glm::dvec3(r.scene.sunCamera.position);
    const auto view=character ? glm::mat4(glm::lookAt(e,r.astronautView.target,r.astronautView.up)) : r.scene.surfaceCamera ? r.scene.surfaceCamera->getViewMatrix() : r.scene.sunCamera.getViewMatrix();
    rendering::renderScene(r.scene.scenario,r.scene.bodies,view,r.scene.surfaceCamera ? r.scene.surfaceCamera->fov() : r.scene.sunCamera.fov,e,
        r.shader,r.waterShader,r.skyboxShader,r.waterReflection,r.shadowShader,r.terrainShadows,
        r.atmosphereShader,r.atmosphere,r.reflectionAtmosphere,r.atmosphereColumns,r.meshes.sunMesh,r.meshes.skyboxMesh,
        r.meshes.planetMeshes,r.meshes.waterMeshes,320,180,r.surfaceClip,r.scene.scenario.planets.empty() ? std::nullopt : std::optional<std::size_t>(r.scene.orbitPlanetIndex),
        false,nullptr,false,0,&r.grass,r.characterWindTime,character ? &r.astronaut : nullptr,nullptr,
        r.terrainPublication.get(),&r.terrainConsumers);
    const json state=r.terrainPublicationState();
    for(const auto& c:state["consumers"]) {
        EXPECT_EQ(c["land"],c["grass"]);EXPECT_EQ(c["land"],c["contacts"]);
        EXPECT_EQ(c["land_revision"],c["main_revision"]);EXPECT_EQ(c["land_revision"],c["shadow_revision"]);
        EXPECT_EQ(c["land_revision"],c["grass_revision"]);
        if(c["reflection_revision"]!=0) EXPECT_EQ(c["land_revision"],c["reflection_revision"]);
        if(c["water_enabled"].get<bool>()) EXPECT_EQ(c["water_revision"],c["water_draw_revision"]);
        if(c["grass_draw_revision"]!=0) EXPECT_EQ(c["grass_revision"],c["grass_draw_revision"]);
    }
    if(character) EXPECT_EQ(r.astronautGround.revision(),r.meshes.planetMeshes[r.scene.scenario.surface_camera.planet_index].revision);
    EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
}
void save(const app::CommandLineOptions& o,const json& trace,const GlAudit& audit,auto& r) {
    const auto w=r.terrainJobs.stats();EXPECT_LE(w.running,1u);EXPECT_LE(w.queued,1u);EXPECT_LE(w.ready,1u);
    EXPECT_EQ(w.peakRunning,1u);EXPECT_EQ(w.peakQueued,1u);
    EXPECT_LE(r.terrainPublication->reservedBytes(),1024ull*1024*1024);
    std::ofstream(o.configPath+".results.json")<<json{{"frames",trace},{"gl",audit.state()},
        {"worker",{{"peak_running",w.peakRunning},{"peak_queued",w.peakQueued},{"ready",w.ready}}},
        {"peak_reserved_bytes",r.terrainPublication->stats().peakReservedBytes}}.dump(2);
}
}

#include "rendering/runtime/RendererState.h"
#include <GLFW/glfw3.h>
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

namespace rendering {
// Access only for deterministic recovery acceptance; no product test flags.
struct RendererRecoveryProbe {
    static auto& state(Renderer& r) {return *r.impl_;}
};
}
namespace {
using nlohmann::json;
namespace fs=std::filesystem;
using Probe=rendering::RendererRecoveryProbe;
json read(const std::string& path) {json j;std::ifstream(path)>>j;return j;}
void write(const std::string& path,const json& j) {std::ofstream(path)<<j.dump(2);}
std::string bytes(const std::string& path) {
    std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};
}
glm::dvec3 vector(const json& j) {return {j[0].get<double>(),j[1].get<double>(),j[2].get<double>()};}
json vector(const glm::dvec3& v) {return {v.x,v.y,v.z};}
app::CommandLineOptions options(const std::string& name) {
    const auto out=fs::path(PLANET_TEST_OUTPUT)/name;fs::create_directories(out);
    auto j=read(std::string(PLANET_SOURCE_DIR)+"/tests/scenarios/foliage/surface.json");
    for(auto& p:j["planets"]) p["terrain_lod"]["max_triangle_budget"]=10000;
    j["planets"][0]["foliage"]["max_blades"]=128;
    j["lighting"]["shadows"]["resolution"]=256;
    app::CommandLineOptions o;o.renderTestMode=o.surfaceRenderMode=o.thirdPersonRenderMode=o.captureOnly=true;
    o.terrainBackend="compute";o.terrainGrassPlanner="gpu-v1";
    o.configPath=(out/"scene.json").string();write(o.configPath,j);
    o.outputImagePath=(out/"capture.png").string();o.renderTestWidth=320;o.renderTestHeight=180;
    o.benchmarkStep=0;return o;
}
void consistent(const json& frame,std::uint64_t epoch) {
    const auto& p=frame["render"]["terrain_publication"];
    ASSERT_TRUE(p["managed"]);ASSERT_FALSE(p["pending"]);
    EXPECT_LE(p["peak_reserved_bytes"].get<std::uint64_t>(),1024ull*1024*1024);
    for(const auto& c:p["consumers"]) {
        EXPECT_EQ(c["epoch"],epoch);EXPECT_EQ(c["land"],c["grass"]);EXPECT_EQ(c["land"],c["contacts"]);
        EXPECT_EQ(c["land_revision"],c["grass_revision"]);EXPECT_EQ(c["land_revision"],c["main_revision"]);
        EXPECT_EQ(c["land_revision"],c["shadow_revision"]);
        if(c["reflection_revision"]!=0) EXPECT_EQ(c["land_revision"],c["reflection_revision"]);
        if(c["water_enabled"].get<bool>()) EXPECT_EQ(c["water_revision"],c["water_draw_revision"]);
        if(c["grass_draw_revision"]!=0) EXPECT_EQ(c["grass_revision"],c["grass_draw_revision"]);
    }
    const auto selected=frame["surface_camera"]["planet_index"].get<std::size_t>();
    EXPECT_EQ(p["astronaut_contact_revision"],p["consumers"][selected]["land_revision"]);
    const auto& r=frame["render"]["scene_reload"];
    EXPECT_EQ(r["epoch"],epoch);EXPECT_FALSE(r["retiring"]);EXPECT_EQ(r["external_bytes"],0);
    const auto& w=frame["render"]["terrain_cpu_worker"];
    EXPECT_EQ(w["peak_running"],1);EXPECT_EQ(w["peak_queued"],1);EXPECT_FALSE(w["pending"]);
}
PFNGLFENCESYNCPROC originalFence=nullptr;
unsigned fenceCalls=0;
GLsync GLAPIENTRY failFence(GLenum condition,GLbitfield flags) {
    if(++fenceCalls==1) return nullptr;
    return originalFence(condition,flags);
}
struct FenceFault {
    FenceFault(){fenceCalls=0;originalFence=__glewFenceSync;__glewFenceSync=failFence;}
    ~FenceFault(){__glewFenceSync=originalFence;}
};
void queueOld(rendering::Renderer& renderer) {
    auto& r=Probe::state(renderer);const auto i=r.scene.scenario.surface_camera.planet_index;
    const auto& p=r.scene.scenario.planets[i];const auto eye=r.lastTerrainEyes[i];
    rendering::TerrainBuildIdentity k;k.epoch=r.terrainSceneEpoch;k.serial=++r.terrainRequestSerial;
    k.bodyIndex=i;k.bodyName=p.name;k.field=r.scene.terrainSurfaces[i].field().fingerprint();
    k.eye=eye;k.localMask=r.lastLocalMask[i];k.backend=rendering::TerrainBackend::Compute;k.resident=true;
    ASSERT_TRUE(r.terrainJobs.submit({k,r.scene.terrainSurfaces[i],p,r.lastFaceZones[i],r.scene.scenario.metersPerWorldUnit()}));
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while(!r.terrainJobs.stats().ready && std::chrono::steady_clock::now()<deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    ASSERT_EQ(r.terrainJobs.stats().ready,1u);
}
}

TEST(TerrainRecovery, MovingReloadFailureRetainsPoseAndDrawsThenRepeatedBodyReordersRecover) {
    auto o=options("movement");o.benchmarkFrames=13;o.benchmarkWalkStep=1;o.benchmarkCharacterStep=1.0/12;
    json trace=json::array(),boundaries=json::array();
    {
        rendering::Renderer renderer(o);ASSERT_EQ(renderer.run(),0);
        auto& r=Probe::state(renderer);auto before=read(o.outputImagePath+".json");consistent(before,1);trace.push_back(before);
        EXPECT_GT(before["astronaut_pose"]["walked_m"].get<double>(),10);
        EXPECT_GT(before["render"]["terrain_publication"]["consumers"][0]["land_revision"].get<unsigned>(),1u);
        EXPECT_GT(before["render"]["terrain_publication"]["published"].get<unsigned>(),2u);
        const auto pose=r.astronautState();const auto image=bytes(o.outputImagePath);
        auto j=read(o.configPath);j["planets"][0]["radius"]=-1;write(o.configPath,j);
        EXPECT_THROW(renderer.reload(),std::invalid_argument);EXPECT_EQ(r.astronautState(),pose);
        EXPECT_EQ(read(o.outputImagePath+".json"),before);EXPECT_EQ(bytes(o.outputImagePath),image);
        j["planets"][0]["radius"]=1;write(o.configPath,j);
        {FenceFault fault;EXPECT_THROW(renderer.reload(),std::runtime_error);}
        EXPECT_EQ(r.astronautState(),pose);EXPECT_EQ(r.astronautGround.revision(),before["render"]["terrain_publication"]["astronaut_contact_revision"]);
        for(const auto& c:before["render"]["terrain_publication"]["consumers"])
            EXPECT_TRUE(glIsBuffer(c["land_buffer"].get<GLuint>()));
        for(unsigned epoch=2;epoch<=4;++epoch) {
            std::swap(j["planets"][0],j["planets"][1]);
            for(auto& p:j["planets"]) if(p["name"]=="earth") p["surface_noise"][0]["seed"]=117+epoch;
            j["scenario_name"]="Moving recovery "+std::to_string(epoch);
            write(o.configPath,j);renderer.reload();
            auto boundary=r.sceneReloadState();boundary["reserved_bytes"]=r.terrainPublication->reservedBytes();
            boundaries.push_back(boundary);
            EXPECT_TRUE(boundary["retiring"]);EXPECT_GT(boundary["external_bytes"].get<std::uint64_t>(),0u);
            EXPECT_LE(r.terrainPublication->reservedBytes(),1024ull*1024*1024);
            for(const auto& c:before["render"]["terrain_publication"]["consumers"])
                EXPECT_TRUE(glIsBuffer(c["land_buffer"].get<GLuint>()));
            r.retireSceneReload(true);
            for(const auto& c:before["render"]["terrain_publication"]["consumers"])
                EXPECT_FALSE(glIsBuffer(c["land_buffer"].get<GLuint>()));
            ASSERT_EQ(renderer.run(),0);
            auto frame=read(o.outputImagePath+".json");consistent(frame,epoch);trace.push_back(frame);
            EXPECT_EQ(frame["render"]["scene_reload"]["failed"],2);
            EXPECT_EQ(frame["scenario"]["planets"][0]["name"],j["planets"][0]["name"]);
            before=frame;
        }
        EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
    }
    EXPECT_EQ(glfwGetCurrentContext(),nullptr);write(o.outputImagePath+".trace.json",trace);
    write(o.outputImagePath+".boundaries.json",boundaries);
}

TEST(TerrainRecovery, ReadyOldCompletionCannotCrossFailedOrSuccessfulReplacement) {
    auto o=options("stale");o.benchmarkFrames=13;o.benchmarkWalkStep=1;o.benchmarkCharacterStep=1.0/12;
    json trace=json::array();
    {
        rendering::Renderer renderer(o);ASSERT_EQ(renderer.run(),0);
        auto& r=Probe::state(renderer);const auto pose=r.astronautState();queueOld(renderer);
        auto j=read(o.configPath);std::swap(j["planets"][0],j["planets"][1]);write(o.configPath,j);
        {FenceFault fault;EXPECT_THROW(renderer.reload(),std::runtime_error);}
        EXPECT_EQ(r.astronautState(),pose);EXPECT_EQ(r.terrainSceneEpoch,1u);
        EXPECT_EQ(r.terrainJobs.stats().ready,0u);EXPECT_GE(r.terrainJobs.stats().obsolete,1u);
        // A failed future epoch leaves the live epoch able to finish current movement.
        ASSERT_EQ(renderer.run(),0);auto frame=read(o.outputImagePath+".json");consistent(frame,1);trace.push_back(frame);
        queueOld(renderer);const auto old=r.terrainJobs.stats().obsolete;
        renderer.reload();ASSERT_EQ(renderer.run(),0);frame=read(o.outputImagePath+".json");consistent(frame,2);trace.push_back(frame);
        EXPECT_GT(r.terrainJobs.stats().obsolete,old);EXPECT_FALSE(r.terrainJobs.pending(1));
        EXPECT_EQ(r.terrainRejectedBuilds,0u);EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
    }
    EXPECT_EQ(glfwGetCurrentContext(),nullptr);write(o.outputImagePath+".trace.json",trace);
}

TEST(TerrainRecovery, MoonArrivalBindsDestinationContactsOnTheHandoffFrameAndReplaysExactly) {
    auto o=options("moon");o.benchmarkFrames=2;o.benchmarkCharacterStep=.04;o.benchmarkJumpFrame=0;
    json seed;
    {rendering::Renderer launch(o);ASSERT_EQ(launch.run(),0);seed=read(o.outputImagePath+".json");}
    o.benchmarkJumpFrame=-1;
    auto& pose=seed["astronaut_pose"];auto& nav=pose["navigation"];
    const auto document=read(o.configPath);
    app::PreparedScene scene(config::ScenarioConfig{config::Config{json(document)}});
    scene.updateSimulation(seed["surface_camera"]["simulation_time_seconds"].get<double>());
    const double units=scene.scenario.metersPerWorldUnit();
    const auto world=scene.bodies[2].position*units+glm::dvec3(0,0,1.5*scene.scenario.planets[1].radius*units);
    const auto root=scene.bodies[1].toLocalPoint(world/units)*units;
    const auto delta=root-vector(pose["root"]);
    pose["root"]=vector(root);pose["velocity_mps"]={0,0,0};pose["boosting"]=pose["jetpack_armed"]=false;pose["thrust_n"]=0;
    nav["position_m"]=vector(world);nav["velocity_mps"]={0,0,0};nav["outer_space"]=true;nav["reference_body"]=1;
    for(auto& foot:pose["feet"]) for(const auto* key:{"position","start","target","hip","knee","ankle"})
        foot[key]=vector(vector(foot[key])+delta);
    // Remove launch-only local grass anchor; the prospective chase eye plans every body.
    pose.erase("grass_plan_eye");
    o.replayPath=o.configPath+".replay.json";write(o.replayPath,seed);
    o.explicitTerrainBackend=o.explicitTerrainGrassPlanner=o.explicitRenderSize=true;
    json trace=json::array();std::string image;json arrived;
    {
        rendering::Renderer renderer(o);ASSERT_EQ(renderer.run(),0);arrived=read(o.outputImagePath+".json");
        consistent(arrived,1);trace.push_back(arrived);
        EXPECT_EQ(arrived["surface_camera"]["planet_index"],1);
        EXPECT_EQ(arrived["astronaut_pose"]["navigation"]["reference_body"],2);
        EXPECT_FALSE(arrived["astronaut_pose"]["navigation"]["outer_space"]);
        EXPECT_LT(glm::length(vector(arrived["astronaut_pose"]["navigation"]["position_m"])-world),1);
        image=bytes(o.outputImagePath);
        // Reload the saved destination pose, then fail and recover without losing it.
        write(o.replayPath,arrived);auto& r=Probe::state(renderer);r.options.benchmarkFrames=1;
        renderer.reload();ASSERT_EQ(renderer.run(),0);auto frame=read(o.outputImagePath+".json");consistent(frame,2);trace.push_back(frame);
        EXPECT_TRUE(bytes(o.outputImagePath)==image) << "Saved Moon pose changed on reload";
        const auto saved=r.astronautState();{FenceFault fault;EXPECT_THROW(renderer.reload(),std::runtime_error);}
        EXPECT_EQ(r.astronautState(),saved);ASSERT_EQ(renderer.run(),0);
        frame=read(o.outputImagePath+".json");consistent(frame,2);trace.push_back(frame);
        EXPECT_EQ(frame["astronaut_pose"]["navigation"]["position_m"],saved["navigation"]["position_m"]);
        EXPECT_EQ(frame["astronaut_pose"]["effect_s"],saved["effect_s"]);
        renderer.reload();ASSERT_EQ(renderer.run(),0);frame=read(o.outputImagePath+".json");consistent(frame,3);trace.push_back(frame);
        EXPECT_TRUE(bytes(o.outputImagePath)==image) << "Repeated Moon reload changed saved image";
        EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
    }
    o.benchmarkFrames=1;o.outputImagePath+=".fresh.png";
    {rendering::Renderer replay(o);ASSERT_EQ(replay.run(),0);EXPECT_TRUE(bytes(o.outputImagePath)==image) << "Fresh Moon replay differs";}
    EXPECT_EQ(glfwGetCurrentContext(),nullptr);write(std::string(PLANET_TEST_OUTPUT)+"/moon/trace.json",trace);
}

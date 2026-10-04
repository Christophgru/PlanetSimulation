#include "app/CommandLineOptions.h"
#include "rendering/Renderer.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>

namespace {
namespace fs=std::filesystem;
using nlohmann::json;
json read(const std::string& path) {json j;std::ifstream(path)>>j;return j;}
void write(const std::string& path,const json& j) {std::ofstream(path)<<j.dump(2);}
std::string bytes(const std::string& path) {
    std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};
}
app::CommandLineOptions options(const std::string& name) {
    const auto out=fs::path(PLANET_TEST_OUTPUT)/("reload-"+name);fs::create_directories(out);
    auto j=read(std::string(PLANET_SOURCE_DIR)+"/tests/scenarios/foliage/surface.json");
    for(auto& p:j["planets"]) p["terrain_lod"]["max_triangle_budget"]=10000;
    j["planets"][0]["foliage"]["max_blades"]=128;
    j["lighting"]["shadows"]["resolution"]=256;
    app::CommandLineOptions o;o.renderTestMode=o.surfaceRenderMode=o.captureOnly=true;
    o.terrainBackend="compute";o.terrainGrassPlanner="gpu-v1";
    o.configPath=(out/"scene.json").string();write(o.configPath,j);
    o.outputImagePath=(out/"capture.png").string();o.renderTestWidth=320;o.renderTestHeight=180;
    o.benchmarkStep=0;return o;
}
void consistent(const json& state) {
    ASSERT_TRUE(state["managed"]);ASSERT_FALSE(state["pending"]);
    for(const auto& c:state["consumers"]) {
        EXPECT_EQ(c["land"],c["grass"]);EXPECT_EQ(c["land"],c["contacts"]);
        EXPECT_EQ(c["land_revision"],c["grass_revision"]);EXPECT_EQ(c["land_revision"],c["main_revision"]);
        EXPECT_EQ(c["land_revision"],c["shadow_revision"]);EXPECT_EQ(c["land_revision"],c["reflection_revision"]);
        if(c["water_enabled"].get<bool>()) EXPECT_EQ(c["water_revision"],c["water_draw_revision"]);
    }
}
void committed(const json& frame,unsigned failures=0) {
    const auto& r=frame["render"]["scene_reload"];
    EXPECT_EQ(r["published"],1);EXPECT_EQ(r["failed"],failures);EXPECT_EQ(r["epoch"],2);
    EXPECT_FALSE(r["retiring"]);EXPECT_EQ(r["external_bytes"],0);
    if(frame["render"]["terrain_publication"]["managed"].get<bool>()) {
        for(const auto& c:frame["render"]["terrain_publication"]["consumers"]) EXPECT_EQ(c["epoch"],2);
        EXPECT_EQ(frame["render"]["terrain_cpu_worker"]["peak_running"],1);
        EXPECT_EQ(frame["render"]["terrain_cpu_worker"]["peak_queued"],1);
        EXPECT_FALSE(frame["render"]["terrain_cpu_worker"]["pending"]);
    }
}
void freshMatches(app::CommandLineOptions o,const std::string& expected) {
    o.outputImagePath+= ".fresh.png";
    {rendering::Renderer fresh(o);ASSERT_EQ(fresh.run(),0);EXPECT_EQ(bytes(o.outputImagePath),expected);}
    EXPECT_EQ(glfwGetCurrentContext(),nullptr);
}
PFNGLFENCESYNCPROC originalFence=nullptr;
unsigned fenceCalls=0,failAt=0;
GLsync GLAPIENTRY failFence(GLenum condition,GLbitfield flags) {
    if(++fenceCalls==failAt) return nullptr;
    return originalFence(condition,flags);
}
struct FenceFault {
    explicit FenceFault(unsigned call){fenceCalls=0;failAt=call;originalFence=__glewFenceSync;__glewFenceSync=failFence;}
    ~FenceFault(){__glewFenceSync=originalFence;}
};
PFNGLGENBUFFERSPROC originalGenBuffers=nullptr;
std::vector<GLuint> generated;
void GLAPIENTRY recordBuffers(GLsizei n,GLuint* buffers) {
    originalGenBuffers(n,buffers);generated.insert(generated.end(),buffers,buffers+n);
}
struct BufferProbe {
    BufferProbe(){generated.clear();originalGenBuffers=__glewGenBuffers;__glewGenBuffers=recordBuffers;}
    ~BufferProbe(){__glewGenBuffers=originalGenBuffers;}
};
}

TEST(RendererReload, ResidentReloadChangesBodyCountFieldsAndDrawConsumersBeforeCapture) {
    auto o=options("resident");std::string replacementImage;
    {
        rendering::Renderer renderer(o);ASSERT_EQ(renderer.run(),0);
        const auto before=read(o.outputImagePath+".json");
        auto j=read(o.configPath);j["planets"].erase(1);j["scenario_name"]="Reloaded Earth";
        j["planets"][0]["surface_noise"][0]["seed"]=117;
        j["surface_camera"]["latitude_deg"]=-19;write(o.configPath,j);
        renderer.reload();EXPECT_EQ(read(o.outputImagePath+".json"),before);
        ASSERT_EQ(renderer.run(),0);const auto after=read(o.outputImagePath+".json");
        committed(after);consistent(after["render"]["terrain_publication"]);
        EXPECT_EQ(after["scenario"]["scenario_name"],"Reloaded Earth");EXPECT_EQ(after["scenario"]["planets"].size(),1u);
        for(const auto& c:before["render"]["terrain_publication"]["consumers"])
            EXPECT_FALSE(glIsBuffer(c["land_buffer"].get<GLuint>()));
        replacementImage=bytes(o.outputImagePath);EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
    }
    EXPECT_EQ(glfwGetCurrentContext(),nullptr);freshMatches(o,replacementImage);
}

TEST(RendererReload, SevenFenceFailuresRetainOldSceneAndSameRendererRecovers) {
    auto o=options("failures");
    {
        rendering::Renderer renderer(o);ASSERT_EQ(renderer.run(),0);
        const auto before=read(o.outputImagePath+".json");const auto image=bytes(o.outputImagePath);
        auto j=read(o.configPath);j["planets"].erase(1);j["scenario_name"]="Candidate";
        write(o.configPath,j); // Reuse the warmed field-parameter cache; probe generation-owned buffers.
        for(unsigned failure=1;failure<=7;++failure) {
            const auto sidecar=read(o.outputImagePath+".json");
            {BufferProbe buffers;FenceFault fault(failure);EXPECT_THROW(renderer.reload(),std::runtime_error);
                EXPECT_EQ(fenceCalls,failure);EXPECT_EQ(read(o.outputImagePath+".json"),sidecar);}
            for(auto buffer:generated) EXPECT_FALSE(glIsBuffer(buffer));
            for(const auto& c:before["render"]["terrain_publication"]["consumers"])
                EXPECT_TRUE(glIsBuffer(c["land_buffer"].get<GLuint>()));
            ASSERT_EQ(renderer.run(),0);EXPECT_EQ(bytes(o.outputImagePath),image);
            const auto retained=read(o.outputImagePath+".json");
            EXPECT_EQ(retained["scenario"],before["scenario"]);
            EXPECT_EQ(retained["render"]["scene_reload"]["epoch"],1);
            EXPECT_EQ(retained["render"]["scene_reload"]["failed"],failure);
            consistent(retained["render"]["terrain_publication"]);
        }
        renderer.reload();ASSERT_EQ(renderer.run(),0);const auto after=read(o.outputImagePath+".json");
        committed(after,7);consistent(after["render"]["terrain_publication"]);
        EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
    }
    EXPECT_EQ(glfwGetCurrentContext(),nullptr);
}

TEST(RendererReload, CpuAndLegacyReloadMatchFreshCapturesAndRejectInvalidConfigs) {
    for(const auto* backend:{"cpu","compute"}) {
        SCOPED_TRACE(backend);auto o=options(backend);o.terrainBackend=backend;o.terrainGrassPlanner="cpu";
        std::string replacementImage;
        {
            rendering::Renderer renderer(o);ASSERT_EQ(renderer.run(),0);
            const auto before=read(o.outputImagePath+".json");const auto image=bytes(o.outputImagePath);
            auto j=read(o.configPath);j["planets"][0]["radius"]=-1;write(o.configPath,j);
            EXPECT_THROW(renderer.reload(),std::invalid_argument);EXPECT_EQ(read(o.outputImagePath+".json"),before);
            ASSERT_EQ(renderer.run(),0);EXPECT_EQ(bytes(o.outputImagePath),image);
            j["planets"][0]["radius"]=1;j["planets"].erase(1);j["scenario_name"]="Legacy reload";
            j["planets"][0]["surface_noise"][0]["seed"]=119;j["surface_camera"]["latitude_deg"]=-18;
            write(o.configPath,j);renderer.reload();ASSERT_EQ(renderer.run(),0);
            const auto after=read(o.outputImagePath+".json");committed(after,1);
            EXPECT_FALSE(after["render"]["terrain_publication"]["managed"]);
            replacementImage=bytes(o.outputImagePath);EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
        }
        EXPECT_EQ(glfwGetCurrentContext(),nullptr);freshMatches(o,replacementImage);
    }
}

TEST(RendererReload, ThirdPersonReplayReloadUsesSnapshotAndExactTerrainGrassAnchors) {
    auto o=options("replay");o.thirdPersonRenderMode=true;
    json replay;std::string image;
    {rendering::Renderer seed(o);ASSERT_EQ(seed.run(),0);replay=read(o.outputImagePath+".json");image=bytes(o.outputImagePath);}
    o.replayPath=o.configPath+".replay.json";write(o.replayPath,replay);
    o.explicitTerrainBackend=o.explicitTerrainGrassPlanner=o.explicitRenderSize=true;
    {
        rendering::Renderer renderer(o);ASSERT_EQ(renderer.run(),0);EXPECT_EQ(bytes(o.outputImagePath),image);
        auto malformed=replay;malformed["astronaut_pose"]["exhaust"]["emission_phase_s"]=-1;
        write(o.replayPath,malformed);EXPECT_THROW(renderer.reload(),std::invalid_argument);
        malformed=replay;malformed["astronaut_pose"]["grass_trail"]={{{0,0,0},{10,0,0}}};
        write(o.replayPath,malformed);EXPECT_THROW(renderer.reload(),std::invalid_argument);
        malformed=replay;malformed["render"]["atmosphere_downsample"]=1;
        write(o.replayPath,malformed);EXPECT_THROW(renderer.reload(),std::invalid_argument);
        replay["scenario"]["scenario_name"]="Reloaded replay";write(o.replayPath,replay);renderer.reload();
        auto invalid=replay;invalid["render"]["terrain_contract"]["field_version"]=999;write(o.replayPath,invalid);
        ASSERT_EQ(renderer.run(),0);EXPECT_EQ(bytes(o.outputImagePath),image);
        const auto after=read(o.outputImagePath+".json");committed(after,3);consistent(after["render"]["terrain_publication"]);
        EXPECT_EQ(after["astronaut_pose"]["grass_plan_eye"],replay["astronaut_pose"]["grass_plan_eye"]);
        EXPECT_EQ(after["astronaut_pose"].at("terrain_plan_eye_world_units"),replay["astronaut_pose"].at("terrain_plan_eye_world_units"));
        EXPECT_EQ(after["scenario"]["scenario_name"],"Reloaded replay");
        EXPECT_THROW(renderer.reload(),std::invalid_argument);EXPECT_EQ(read(o.outputImagePath+".json"),after);
        ASSERT_EQ(renderer.run(),0);EXPECT_EQ(bytes(o.outputImagePath),image);EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
    }
    EXPECT_EQ(glfwGetCurrentContext(),nullptr);
}

TEST(RendererReload, RemovingOptionalSurfaceCameraKeepsPlanetOrbitAndRepeatedReloadsUsable) {
    auto o=options("optional-camera");o.terrainBackend=o.terrainGrassPlanner="cpu";
    o.surfaceRenderMode=false;o.planetRenderMode=true;std::string replacementImage;
    {
        rendering::Renderer renderer(o);ASSERT_EQ(renderer.run(),0);
        auto j=read(o.configPath);j.erase("surface_camera");j["planets"].erase(1);
        j["scenario_name"]="Orbit without surface camera";write(o.configPath,j);
        renderer.reload();ASSERT_EQ(renderer.run(),0);
        replacementImage=bytes(o.outputImagePath);
        renderer.reload();ASSERT_EQ(renderer.run(),0);EXPECT_EQ(bytes(o.outputImagePath),replacementImage);
        EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
    }
    EXPECT_EQ(glfwGetCurrentContext(),nullptr);freshMatches(o,replacementImage);
}

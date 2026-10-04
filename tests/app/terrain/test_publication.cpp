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
app::CommandLineOptions options(const std::string& name) {
    const auto out=fs::path(PLANET_TEST_OUTPUT)/name;fs::create_directories(out);
    json scene;std::ifstream(std::string(PLANET_SOURCE_DIR)+"/tests/scenarios/foliage/surface.json")>>scene;
    for(auto& planet:scene["planets"]) planet["terrain_lod"]["max_triangle_budget"]=10000;
    scene["planets"][0]["foliage"]["max_blades"]=128;
    scene["lighting"]["shadows"]["resolution"]=256;
    const auto config=out/"scene.json";std::ofstream(config)<<scene.dump(2);
    app::CommandLineOptions o;o.renderTestMode=o.surfaceRenderMode=o.captureOnly=true;
    o.terrainBackend="compute";o.terrainGrassPlanner="gpu-v1";o.configPath=config.string();
    o.outputImagePath=(out/"capture.png").string();o.renderTestWidth=320;o.renderTestHeight=180;
    o.benchmarkStep=0;o.benchmarkFrames=2;o.benchmarkWalkStep=7;
    return o;
}
json read(const std::string& path) {json j;std::ifstream(path)>>j;return j;}
void consistent(const json& state) {
    ASSERT_TRUE(state["managed"]);ASSERT_FALSE(state["pending"]);
    for(const auto& c:state["consumers"]) {
        EXPECT_EQ(c["land"],c["grass"]);EXPECT_EQ(c["land"],c["contacts"]);
        EXPECT_EQ(c["land_revision"],c["grass_revision"]);EXPECT_EQ(c["land_revision"],c["main_revision"]);
        EXPECT_EQ(c["land_revision"],c["shadow_revision"]);EXPECT_EQ(c["land_revision"],c["reflection_revision"]);
        if(c["water_enabled"].get<bool>()) EXPECT_EQ(c["water_revision"],c["water_draw_revision"]);
    }
}
PFNGLFENCESYNCPROC originalFence=nullptr;
unsigned calls=0;
GLsync GLAPIENTRY failPublication(GLenum condition,GLbitfield flags) {
    if(++calls==6) return nullptr;
    return originalFence(condition,flags);
}
struct FenceFault {
    FenceFault(){calls=0;originalFence=__glewFenceSync;__glewFenceSync=failPublication;}
    ~FenceFault(){__glewFenceSync=originalFence;}
};
}
TEST(TerrainPublicationRenderer, DrawConsumersSharePublishedGenerationsAfterGrassOnlyReplanning) {
    auto o=options("publication-draws");
    {
        rendering::Renderer renderer(o);ASSERT_EQ(renderer.run(),0);
        const auto state=read(o.outputImagePath+".json")["render"]["terrain_publication"];
        consistent(state);EXPECT_GT(state["grass_only_published"].get<unsigned>(),0u);
        EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
    }
    EXPECT_EQ(glfwGetCurrentContext(),nullptr);
}
TEST(TerrainPublicationRenderer, FailedReplacementRetainsOldBuffersAndTheSameRendererRecovers) {
    auto o=options("publication-recovery");
    {
        rendering::Renderer renderer(o);ASSERT_EQ(renderer.run(),0);
        const auto before=read(o.outputImagePath+".json");
        {FenceFault fault;EXPECT_THROW(renderer.run(),std::runtime_error);EXPECT_EQ(calls,6u);}
        EXPECT_EQ(read(o.outputImagePath+".json"),before);
        for(const auto& c:before["render"]["terrain_publication"]["consumers"]) {
            EXPECT_TRUE(glIsBuffer(c["land_buffer"].get<GLuint>()));
            if(c["water_enabled"].get<bool>()) EXPECT_TRUE(glIsBuffer(c["water_buffer"].get<GLuint>()));
        }
        EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));ASSERT_EQ(renderer.run(),0);
        const auto state=read(o.outputImagePath+".json")["render"]["terrain_publication"];
        consistent(state);EXPECT_EQ(state["failed"],1);EXPECT_EQ(state["obsolete"],0);
        EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
    }
    EXPECT_EQ(glfwGetCurrentContext(),nullptr);
}

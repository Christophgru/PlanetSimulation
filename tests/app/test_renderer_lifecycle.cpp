#include "app/CommandLineOptions.h"
#include "app/Window.h"
#include "rendering/Renderer.h"
#include "rendering/runtime/ResourceOwners.h"
#include <GLFW/glfw3.h>
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
namespace {
class RendererLifecycle : public testing::Test {
protected:
    fs::path output{PLANET_TEST_OUTPUT};
    fs::path originalDirectory;
    app::CommandLineOptions options;
    void SetUp() override {
        originalDirectory = fs::current_path();
        fs::remove_all(output);
        fs::create_directories(output);
        options.renderTestMode = true;
        options.configPath = std::string(PLANET_SOURCE_DIR) +
            "/tests/fixtures/development/configs/scenarios/solar_system.json";
        options.outputImagePath = (output / "capture.png").string();
    }
    void TearDown() override { fs::current_path(originalDirectory); }
};
}

TEST_F(RendererLifecycle, ConstructAndDestroyWithoutRunningTwice) {
    for (int cycle = 0; cycle < 2; ++cycle) {
        {
            rendering::Renderer renderer(options);
            ASSERT_NE(glfwGetCurrentContext(), nullptr);
        }
        EXPECT_EQ(glfwGetCurrentContext(), nullptr);
    }
}

TEST_F(RendererLifecycle, LateConstructorFailureReleasesContextAndCanRecover) {
    // Fail after several GPU owners have initialized, during quality shader load.
    fs::copy(fs::path(PLANET_SOURCE_DIR) / "shaders", output / "shaders", fs::copy_options::recursive);
    fs::remove(output / "shaders/diagnostics/quality_present.frag");
    fs::current_path(output);
    EXPECT_THROW({ rendering::Renderer renderer(options); }, std::runtime_error);
    EXPECT_EQ(glfwGetCurrentContext(), nullptr);
    fs::current_path(originalDirectory);
    {
        rendering::Renderer renderer(options);
        EXPECT_EQ(renderer.run(), 0);
    }
    EXPECT_EQ(glfwGetCurrentContext(), nullptr);
    EXPECT_GT(fs::file_size(options.outputImagePath), 1000);
}

TEST_F(RendererLifecycle, CaptureWriteFailureUnwindsAndCanRecover) {
    options.outputImagePath = (output / "missing-parent/capture.png").string();
    EXPECT_THROW({
        rendering::Renderer renderer(options);
        renderer.run();
    }, std::runtime_error);
    EXPECT_EQ(glfwGetCurrentContext(), nullptr);
    options.outputImagePath = (output / "recovered.png").string();
    {
        rendering::Renderer renderer(options);
        EXPECT_EQ(renderer.run(), 0);
    }
    EXPECT_EQ(glfwGetCurrentContext(), nullptr);
}

TEST_F(RendererLifecycle, GpuOwnersDeleteObjectsWhileContextRemainsAlive) {
    app::Window window(options);
    GLuint program, vao, buffer;
    {
        rendering::OwnedShader shader("shaders/skybox/skybox.vert", "shaders/skybox/skybox.frag");
        rendering::SceneMeshes meshes(1);
        meshes.planetMeshes[0].generateSphere(8);
        program = shader.id;
        vao = meshes.planetMeshes[0].vao;
        buffer = meshes.planetMeshes[0].vbo;
        EXPECT_TRUE(glIsProgram(program));
        EXPECT_TRUE(glIsVertexArray(vao));
        EXPECT_TRUE(glIsBuffer(buffer));
        GLint attached = -1;
        glGetProgramiv(program, GL_ATTACHED_SHADERS, &attached);
        EXPECT_EQ(attached, 0);
    }
    EXPECT_FALSE(glIsProgram(program));
    EXPECT_FALSE(glIsVertexArray(vao));
    EXPECT_FALSE(glIsBuffer(buffer));
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(RendererLifecycle, ShaderCompileFailureThrowsAndLeavesContextUsable) {
    app::Window window(options);
    const auto broken = (output / "broken.frag").string();
    std::ofstream(broken) << "#version 330 core\ninvalid shader source\n";
    EXPECT_THROW({ rendering::OwnedShader shader("shaders/skybox/skybox.vert", broken.c_str()); },
                 std::runtime_error);
    rendering::OwnedShader recovered("shaders/skybox/skybox.vert", "shaders/skybox/skybox.frag");
    EXPECT_TRUE(glIsProgram(recovered.id));
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

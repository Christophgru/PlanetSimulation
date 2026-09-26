#include <gtest/gtest.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <array>
#include <vector>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "rendering/Mesh.h"
#include "rendering/TerrainShadowMaps.h"
#include "rendering/CelestialLighting.h"

namespace {
constexpr int size = 256;
using Image = std::vector<unsigned char>;

std::array<int, 3> pixel(const Image& image, double x, double y = 0) {
    const int column = static_cast<int>((x + 1) * size / 2);
    const int row = static_cast<int>((y + 1) * size / 2);
    const int offset = 3 * (row * size + column);
    return {image[offset], image[offset + 1], image[offset + 2]};
}

// Two extruded mountain ridges, with a taller ridge toward the Sun (-X,+Z).
// The smaller ridge's sun-facing slope has positive Lambert shading but is
// physically hidden from the Sun by the first ridge.
void buildRidges(Mesh& mesh, bool foreground) {
    mesh.vertices.clear();
    mesh.indices.clear();
    const std::array<glm::vec2, 6> profile{{{-1, 0}, {-0.3f, foreground ? 0.8f : 0},
                                          {0, 0}, {0.3f, 0.15f}, {0.6f, 0}, {1, 0}}};
    for (std::size_t i = 0; i + 1 < profile.size(); ++i) {
        const auto a = profile[i], b = profile[i + 1];
        const glm::vec3 normal = glm::normalize(glm::vec3(a.y - b.y, 0, b.x - a.x));
        const unsigned start = static_cast<unsigned>(mesh.vertices.size() / 6);
        for (auto p : {glm::vec3(a.x, -0.6, a.y), glm::vec3(b.x, -0.6, b.y),
                       glm::vec3(b.x, 0.6, b.y), glm::vec3(a.x, 0.6, a.y)})
            mesh.addVertex(p.x, p.y, p.z, normal.x, normal.y, normal.z);
        mesh.addTriangle(start, start + 1, start + 2);
        mesh.addTriangle(start, start + 2, start + 3);
    }
    mesh.upload();
}

class ShadowScene {
public:
    Shader terrain{"shaders/basic.vert", "shaders/basic.frag", "shaders/terrain_shadow.glsl"};
    Shader water{"shaders/water.vert", "shaders/water.frag", "shaders/terrain_shadow.glsl"};
    Shader depth{"shaders/terrain_shadow.vert", "shaders/terrain_shadow.frag"};
    rendering::TerrainShadowMaps maps;
    config::TerrainShadowConfig settings;
    Mesh ridges, sea;
    glm::dvec3 sun = glm::normalize(glm::dvec3(-1, 0, 1));

    GLuint framebuffer = 0, colorBuffer = 0, depthBuffer = 0;

    ShadowScene() {
        // Hidden windows can have an unallocated default framebuffer on native
        // drivers. Give these pixel assertions a fixed offscreen render target.
        glGenFramebuffers(1, &framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glGenRenderbuffers(1, &colorBuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, colorBuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB8, size, size);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorBuffer);
        glGenRenderbuffers(1, &depthBuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, size, size);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            throw std::runtime_error("Shadow test framebuffer is incomplete");
        settings.resolution = 512;
        buildRidges(ridges, true);
        for (auto p : {glm::vec3(-1, -1, 0.01), glm::vec3(1, -1, 0.01),
                       glm::vec3(1, 1, 0.01), glm::vec3(-1, 1, 0.01)})
            sea.addVertex(p.x, p.y, p.z, 0, 0, 1);
        sea.addTriangle(0, 1, 2);
        sea.addTriangle(0, 2, 3);
        sea.upload();
    }
    ~ShadowScene() {
        ridges.destroy(); sea.destroy();
        glDeleteProgram(terrain.id); glDeleteProgram(water.id); glDeleteProgram(depth.id);
        glDeleteFramebuffers(1, &framebuffer);
        glDeleteRenderbuffers(1, &colorBuffer);
        glDeleteRenderbuffers(1, &depthBuffer);
    }

    Image render(bool waterSurface = false, glm::dvec3 indirect = glm::dvec3(0),
                 float waterRadiusScale = 1.0f, bool secondBody = false,
                 bool planeReceiver = false, float exposure = 1.0f) {
        maps.ensure(secondBody ? 2 : 1, settings);
        if (settings.enabled) {
            maps.begin(0, depth, sun, 1.5);
            ridges.draw();
            if (secondBody) {
                // Deliberately overwrite the other body's depth with a different Sun direction.
                maps.begin(1, depth, {1, 0, 1}, 1.5);
                ridges.draw();
            }
        }
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glViewport(0, 0, size, size);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        const Shader& shader = waterSurface ? water : terrain;
        shader.use();
        GLint linked = GL_FALSE;
        glGetProgramiv(shader.id, GL_LINK_STATUS, &linked);
        EXPECT_EQ(linked, GL_TRUE);
        glGetProgramiv(depth.id, GL_LINK_STATUS, &linked);
        EXPECT_EQ(linked, GL_TRUE);
        shader.setMat4("model", glm::value_ptr(glm::scale(glm::mat4(1), glm::vec3(waterSurface ? waterRadiusScale : 1))));
        shader.setMat4("view", glm::value_ptr(glm::lookAt(glm::vec3(0, 0, 3), glm::vec3(0), glm::vec3(0, 1, 0))));
        shader.setMat4("projection", glm::value_ptr(glm::ortho(-1.f, 1.f, -1.f, 1.f, 0.1f, 10.f)));
        shader.setFloat3("uColor", 1, 1, 1);
        shader.setFloat3("uSunDirection", sun.x, sun.y, sun.z);
        shader.setFloat3("uSunlight", 1, 1, 1);
        shader.setFloat3("uIndirectLight", indirect.x, indirect.y, indirect.z);
        shader.setFloat("uExposure", exposure);
        shader.setFloat("uEmissive", 0);
        shader.setFloat("uClipRadius", -1);
        maps.bindForShading(0, shader, settings, waterSurface ? waterRadiusScale : 1);
        if (waterSurface) {
            shader.setFloat3("uPlanetCenter", 0, 0, -10);
            shader.setFloat3("uCameraPosition", 0, 0, 3);
            shader.setFloat3("uWaterColor", 1, 1, 1);
            shader.setFloat("uOpacity", 1);
            shader.setFloat("uReflectionFraction", 0);
            shader.setInt("uReflectionTexture", 0);
            shader.setMat4("uReflectionViewProjection", glm::value_ptr(glm::mat4(1)));
            sea.draw();
        } else if (planeReceiver) sea.draw();
        else ridges.draw();
        Image result(size * size * 3);
        glReadPixels(0, 0, size, size, GL_RGB, GL_UNSIGNED_BYTE, result.data());
        EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
        return result;
    }
};
} // namespace

TEST(TerrainShadowRender, ForegroundRidgeBlocksSunFacingRearRidge) {
    ShadowScene scene;
    const auto shadowed = scene.render();
    EXPECT_LT(pixel(shadowed, 0.15)[0], 3);
    EXPECT_GT(pixel(shadowed, -0.5)[0], 160); // First mountain still sees the Sun.
    EXPECT_GT(pixel(shadowed, 0.85)[0], 160); // Beyond the shadow, no false darkness.
    scene.settings.enabled = false;
    const auto disabled = scene.render();
    EXPECT_GT(pixel(disabled, 0.15)[0], 160);
    for (double x : {-0.7, -0.6, -0.5, 0.56, 0.57, 0.58, 0.59, 0.8})
        EXPECT_NEAR(pixel(shadowed, x)[0], pixel(disabled, x)[0], 2) << "Unoccluded slope at " << x;
    scene.settings.enabled = true;
    buildRidges(scene.ridges, false);
    EXPECT_GT(pixel(scene.render(), 0.15)[0], 160); // Removing the occluder restores light.

    // Visual artifact: shadows enabled on left, disabled on right; flip OpenGL rows.
    Image comparison(2 * size * size * 3);
    for (int y = 0; y < size; ++y) {
        std::copy_n(shadowed.data() + (size - 1 - y) * size * 3, size * 3, comparison.data() + y * size * 6);
        std::copy_n(disabled.data() + (size - 1 - y) * size * 3, size * 3, comparison.data() + y * size * 6 + size * 3);
    }
    EXPECT_NE(stbi_write_png(PLANET_SHADOW_IMAGE, 2 * size, size, 3, comparison.data(), 2 * size * 3), 0);
}

TEST(TerrainShadowRender, ShadowPreservesReflectedAndAmbientColor) {
    ShadowScene scene;
    const glm::dvec3 indirect(0.04, 0.02, 0.01);
    const auto actual = pixel(scene.render(false, indirect), 0.15);
    const auto expected = rendering::displayColor(indirect, 1) * 255.0;
    for (int channel = 0; channel < 3; ++channel) EXPECT_NEAR(actual[channel], expected[channel], 1.5);
}

TEST(TerrainShadowRender, ShadowsWaterAndAccountsForItsDifferentRadius) {
    ShadowScene scene;
    for (float radiusScale : {1.0f, 2.0f}) {
        const auto image = scene.render(true, glm::dvec3(0), radiusScale);
        EXPECT_LT(pixel(image, 0.2)[0], 3);
        EXPECT_GT(pixel(image, 0.85)[0], 160);
        const glm::dvec3 indirect(0.04, 0.02, 0.01);
        const auto actual = pixel(scene.render(true, indirect, radiusScale), 0.2);
        const auto expected = rendering::displayColor(indirect, 1) * 255.0;
        for (int channel = 0; channel < 3; ++channel) EXPECT_NEAR(actual[channel], expected[channel], 1.5);
    }
}

TEST(TerrainShadowRender, SunMotionResolutionReloadAndOtherBodiesDoNotLeaveStaleMaps) {
    ShadowScene scene;
    for (int resolution : {256, 1024, 512}) {
        scene.settings.resolution = resolution;
        EXPECT_LT(pixel(scene.render(false, glm::dvec3(0), 1, true), 0.15)[0], 3);
        scene.sun = glm::normalize(glm::dvec3(1, 0, 1));
        EXPECT_GT(pixel(scene.render(), 0.15)[0], 100);
        scene.sun = glm::normalize(glm::dvec3(-1, 0, 1));
    }
}

TEST(TerrainShadowRender, GrazingSunCannotLeakThroughAnOpaqueForegroundWall) {
    ShadowScene scene;
    // Horizontal receiver at z=0.01. Its whole visible area is behind a tall
    // vertical wall, including all neighboring shadow-map samples. Even a
    // near-parallel Sun ray must hit that wall before reaching the receiver.
    const glm::dvec3 indirect(0.00001, 0.000005, 0.0000025);
    const auto expected = rendering::displayColor(indirect, 4096) * 255.0;
    for (float side : {-1.0f, 1.0f}) {
        scene.ridges.vertices.clear();
        scene.ridges.indices.clear();
        for (auto p : {glm::vec3(side * 1.1, -1.2, -1), glm::vec3(side * 1.1, 1.2, -1),
                       glm::vec3(side * 1.1, 1.2, 1), glm::vec3(side * 1.1, -1.2, 1)})
            scene.ridges.addVertex(p.x, p.y, p.z, -side, 0, 0);
        scene.ridges.addTriangle(0, 1, 2);
        scene.ridges.addTriangle(0, 2, 3);
        scene.ridges.upload();
        for (int resolution : {256, 512, 2048}) {
            scene.settings.resolution = resolution;
            for (double elevation : {-0.003, -0.00003, 0.0, 0.00003, 0.0003, 0.003, 0.03}) {
                scene.sun = glm::normalize(glm::dvec3(side, 0, elevation));
                for (bool water : {false, true}) {
                    const auto image = scene.render(water, indirect, 1, false, true, 4096);
                    for (double x : {-0.5, 0.0, 0.5}) {
                        const auto actual = pixel(image, x);
                        for (int channel = 0; channel < 3; ++channel)
                            EXPECT_NEAR(actual[channel], expected[channel], 1.5)
                                << "side=" << side << ", water=" << water << ", resolution=" << resolution
                                << ", elevation=" << elevation << ", x=" << x;
                    }
                }
            }
        }
    }
    scene.ridges.indices.clear();
    scene.ridges.upload();
    scene.sun = glm::normalize(glm::dvec3(-1, 0, 0.0003));
    const auto unblocked = scene.render(false, indirect, 1, false, true, 4096);
    EXPECT_GT(pixel(unblocked, 0)[0], expected.r + 100); // Preserve real grazing sunlight.
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(size, size, "Terrain shadow regression", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) { glfwDestroyWindow(window); glfwTerminate(); return 1; }
    while (glGetError() != GL_NO_ERROR) {} // GLEW's core-profile probing may set GL_INVALID_ENUM.
    const int result = RUN_ALL_TESTS();
    glfwDestroyWindow(window);
    glfwTerminate();
    return result;
}

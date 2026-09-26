#include <gtest/gtest.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <fstream>
#include "rendering/FrameProfiler.h"
#include "rendering/PerformanceOverlay.h"
#include "rendering/AtmosphereRenderer.h"
#include "rendering/Shader.h"
#include "simulation/Atmosphere.h"

namespace {
constexpr int size = 129; // An exact central ray, with a known vertical colour ramp.
class AtmosphereRender : public testing::Test {
protected:
    Shader shader{"shaders/atmosphere.vert", "shaders/atmosphere.frag", "shaders/atmosphere.glsl"};
    GLuint framebuffer = 0, output = 0, source = 0, depth = 0, vao = 0;
    glm::mat4 projection = glm::perspective(glm::radians(10.f), 1.f, 0.01f, 100.f);
    glm::mat4 cameraToBody = glm::mat4(glm::mat3(glm::vec3(0,0,-1), glm::vec3(0,1,0), glm::vec3(-1,0,0)));
    void SetUp() override {
        GLint linked = 0; glGetProgramiv(shader.id, GL_LINK_STATUS, &linked); ASSERT_EQ(linked, GL_TRUE);
        glGenVertexArrays(1, &vao); glBindVertexArray(vao);
        glGenFramebuffers(1, &framebuffer); glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glGenTextures(1, &output); glBindTexture(GL_TEXTURE_2D, output);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, size, size, 0, GL_RGBA, GL_FLOAT, nullptr);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, output, 0);
        ASSERT_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER), GLenum(GL_FRAMEBUFFER_COMPLETE));
        std::vector<float> ramp(size * size * 3), depths(size * size, 1.f);
        for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x)
            for (int c = 0; c < 3; ++c) ramp[3*(y*size+x)+c] = (y+0.5f)/size;
        glGenTextures(1, &source); glBindTexture(GL_TEXTURE_2D, source);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, size, size, 0, GL_RGB, GL_FLOAT, ramp.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glGenTextures(1, &depth); glBindTexture(GL_TEXTURE_2D, depth);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, size, size, 0, GL_RED, GL_FLOAT, depths.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    void TearDown() override {
        glDeleteFramebuffers(1, &framebuffer); glDeleteTextures(1, &output);
        glDeleteTextures(1, &source); glDeleteTextures(1, &depth); glDeleteVertexArrays(1, &vao);
        glDeleteProgram(shader.id);
    }
    float render(const config::AtmosphereConfig& cfg, glm::dvec3 eye) {
        const auto optics = simulation::atmosphereOptics(cfg, 1000, simulation::referenceAir(cfg));
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glViewport(0,0,size,size); glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND);
        shader.use(); shader.setInt("uToneMap", 0);
        shader.setInt("uSceneColor", 0); shader.setInt("uSceneDepth", 2);
        shader.setFloat("uRadius", 1); shader.setInt("uAtmEnabled", 1);
        shader.setFloat("uAtmOuter", cfg.radius_multiplier);
        shader.setFloat("uAtmRefractivity", cfg.refraction_enabled ? optics.refractiveIndex - 1 : 0);
        shader.setFloat2("uAtmHeights", optics.molecularScaleHeight, optics.aerosolScaleHeight);
        shader.setFloat3("uAtmSunDirection", 0,1,0);
        shader.setFloat3("uEyeBody", eye.x,eye.y,eye.z);
        // Isolate displacement from scattering: the source ramp encodes UV.
        shader.setFloat3("uAtmRayleigh",0,0,0); shader.setFloat3("uAtmScatter",0,0,0);
        shader.setFloat3("uAtmAbsorb",0,0,0); shader.setFloat3("uAtmSunlight",0,0,0);
        shader.setFloat3("uAtmIndirect",0,0,0);
        shader.setMat4("uProjection",glm::value_ptr(projection));
        shader.setMat4("uInverseProjection",glm::value_ptr(glm::inverse(projection)));
        shader.setMat4("uCameraToBody",glm::value_ptr(cameraToBody));
        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D,depth);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,source);
        glDrawArrays(GL_TRIANGLES,0,3);
        std::vector<float> pixels(size*size*3);
        glReadPixels(0,0,size,size,GL_RGB,GL_FLOAT,pixels.data());
        EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
        for (float v : pixels) { EXPECT_TRUE(std::isfinite(v)); EXPECT_GE(v,0); EXPECT_LE(v,1); }
        return pixels[3*((size/2)*size+size/2)];
    }
};
TEST_F(AtmosphereRender, SurfaceAndOrbitalSkyDisplacementMatchesReferenceRay) {
    config::AtmosphereConfig cfg; cfg.enabled = true;
    const auto optics = simulation::atmosphereOptics(cfg,1000,simulation::referenceAir(cfg));
    for (const glm::dvec3 eye : {glm::dvec3(0,1.001,0), glm::dvec3(-2,1.005,0)}) {
        cfg.refraction_enabled = false;
        EXPECT_NEAR(render(cfg,eye),0.5,1e-6);
        cfg.refraction_enabled = true;
        const float observed = render(cfg,eye);
        const auto ray = simulation::traceAtmosphericRay(cfg,optics,eye,{1,0,0});
        ASSERT_TRUE(ray.escaped);
        const double expected = 0.5 + 0.5 * projection[1][1] * ray.direction.y / ray.direction.x;
        EXPECT_LT(observed,0.495); // A real displacement, not only changed brightness.
        EXPECT_NEAR(observed,expected,0.0003); // Better than 0.04 pixel here.
    }
}
TEST_F(AtmosphereRender, VacuumMissedShellAndNearGeometryStayUnwarped) {
    config::AtmosphereConfig cfg; cfg.enabled = true; cfg.surface_pressure_pa = 0;
    EXPECT_NEAR(render(cfg,{0,1.001,0}),0.5,1e-6);
    cfg.surface_pressure_pa = 101325;
    EXPECT_NEAR(render(cfg,{-2,1.2,0}),0.5,1e-6);
    // A nearby opaque surface ends the atmospheric path; do not warp its UV.
    const auto projected = projection * glm::vec4(0,0,-0.01,1);
    std::vector<float> depths(size*size,0.5f + 0.5f * projected.z/projected.w);
    glBindTexture(GL_TEXTURE_2D,depth);
    glTexSubImage2D(GL_TEXTURE_2D,0,0,0,size,size,GL_RED,GL_FLOAT,depths.data());
    EXPECT_NEAR(render(cfg,{0,1.001,0}),0.5,1e-6);
}
TEST_F(AtmosphereRender, DenseAtmosphereDoesNotLeakTrappedRaysToBackground) {
    config::AtmosphereConfig cfg; cfg.enabled = true; cfg.surface_pressure_pa = 1e7;
    EXPECT_FLOAT_EQ(render(cfg,{0,1.001,0}),0);
}
TEST_F(AtmosphereRender, PerformanceOverlayIsVisibleOnlyWhileRequested) {
    config::AtmosphereConfig cfg; cfg.enabled=true; cfg.surface_pressure_pa=0;
    render(cfg,{0,1.001,0});
    rendering::PerformanceOverlay overlay;
    auto pixel = [&](int x,int y) {
        std::array<float,3> value{}; glReadPixels(x,y,1,1,GL_RGB,GL_FLOAT,value.data()); return value;
    };
    const auto before=pixel(25,size-23);
    overlay.draw(false,size,size,60,16.7,10,true);
    EXPECT_EQ(pixel(25,size-23),before);
    overlay.draw(true,size,size,60,16.7,10,true);
    EXPECT_NEAR(pixel(25,size-23)[0],0.95,1e-5); // Lit F glyph.
    EXPECT_NEAR(pixel(5,size/2)[0],0.5,1e-5); // Outside panel unchanged.
    glBindVertexArray(vao);
    render(cfg,{0,1.001,0});
    overlay.draw(false,size,size,60,16.7,10,true);
    EXPECT_EQ(pixel(25,size-23),before);
    EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
}
TEST_F(AtmosphereRender, ProfilerWritesEveryFrameAndResolvesGpuQueriesWithoutPollingWaits) {
    {
        rendering::FrameProfiler profiler(PLANET_PERFORMANCE_TRACE);
        for(int i=0;i<24;++i) {
            profiler.beginFrame(i/60.0);
            { rendering::FrameProfiler::Scope scope(&profiler,rendering::FrameStage::Opaque);
              glClearColor(0.1f,0.2f,0.3f,1); glClear(GL_COLOR_BUFFER_BIT); }
            profiler.endFrame();
        }
        glFinish(); // Explicit test synchronization, never inside the profiler.
        profiler.collect(); EXPECT_TRUE(profiler.gpuReady()); EXPECT_GE(profiler.gpuMilliseconds,0);
    }
    std::ifstream stream(PLANET_PERFORMANCE_TRACE); std::string row;
    ASSERT_TRUE(std::getline(stream,row)); EXPECT_NE(row.find("gpu_opaque_ms"),std::string::npos);
    int count=0; while(std::getline(stream,row)) ++count;
    EXPECT_EQ(count,24); EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
}
TEST_F(AtmosphereRender, ReducedAtmosphereAndLookupPreserveSharpDepthEdges) {
    auto scene=config::ScenarioConfig(config::Config::load("tests/scenarios/atmosphere/base.json"));
    std::vector<simulation::BodyState> bodies(scene.planets.size()+1);
    bodies[0].position={30,0,0}; bodies[1].position={0,0,0}; bodies[2].position={10,10,10};
    const auto light=rendering::calculateLighting(scene,bodies);
    rendering::AtmosphereTransmittance table; table.ensure(scene);
    const glm::dvec3 eye(0,1.03,0);
    const auto view=glm::lookAt(glm::vec3(eye),glm::vec3(eye)+glm::vec3(1,0,0),glm::vec3(0,1,0));
    auto capture=[&](int downsample,bool lookup) {
        rendering::AtmosphereRenderer atmosphere(downsample); atmosphere.begin(size,size);
        glDisable(GL_SCISSOR_TEST); glDepthMask(GL_TRUE); glClearDepth(1);
        glClearColor(0.3f,0.4f,0.5f,1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
        glEnable(GL_SCISSOR_TEST); glScissor(0,0,size/2,size);
        const auto clip=projection*glm::vec4(0,0,-0.03,1);
        glClearDepth(0.5+0.5*clip.z/clip.w); glClearColor(0.15f,0.6f,0.2f,1);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT); glDisable(GL_SCISSOR_TEST); glClearDepth(1);
        atmosphere.finish(shader,scene,bodies,light,1,view,projection,eye,framebuffer,false,lookup ? &table : nullptr);
        std::vector<float> pixels(size*size*3);
        glReadPixels(0,0,size,size,GL_RGB,GL_FLOAT,pixels.data());
        EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR)); return pixels;
    };
    const auto exact=capture(1,false), lookup=capture(1,true), reduced=capture(4,true);
    double tableError=0,reducedError=0;
    for(std::size_t i=0;i<exact.size();++i) {
        ASSERT_TRUE(std::isfinite(reduced[i]));
        tableError+=std::abs(exact[i]-lookup[i]); reducedError+=std::abs(exact[i]-reduced[i]);
    }
    EXPECT_LT(tableError/exact.size(),0.0005);
    EXPECT_LT(reducedError/exact.size(),0.001);
    for(int x : {size/2-1,size/2}) for(int c=0;c<3;++c) {
        const int offset=3*((size/2)*size+x)+c;
        EXPECT_NEAR(reduced[offset],exact[offset],0.002); // No bright/dark edge halo.
    }
}
} // namespace
int main(int argc,char** argv) {
    testing::InitGoogleTest(&argc,argv);
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    auto* window = glfwCreateWindow(size,size,"Atmospheric refraction tests",nullptr,nullptr);
    if (!window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window); glewExperimental=GL_TRUE;
    if (glewInit()!=GLEW_OK) { glfwDestroyWindow(window); glfwTerminate(); return 1; }
    while (glGetError()!=GL_NO_ERROR) {}
    const int result=RUN_ALL_TESTS();
    glfwDestroyWindow(window); glfwTerminate(); return result;
}

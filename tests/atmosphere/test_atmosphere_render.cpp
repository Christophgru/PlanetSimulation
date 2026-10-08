#include "config/Config.h"
#include <gtest/gtest.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <fstream>
#include <chrono>
#include <random>
#include <cstdlib>
#include <glm/gtc/packing.hpp>
#include "rendering/diagnostics/FrameProfiler.h"
#include "rendering/diagnostics/PerformanceOverlay.h"
#include "rendering/atmosphere/AtmosphereRenderer.h"
#include "rendering/Shader.h"
#include "simulation/Atmosphere.h"

namespace {
constexpr int size = 129; // An exact central ray, with a known vertical colour ramp.
class AtmosphereRender : public testing::Test {
protected:
    Shader shader{"shaders/atmosphere/atmosphere.vert", "shaders/atmosphere/atmosphere.frag", "shaders/atmosphere/atmosphere.glsl"};
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
    float render(const config::AtmosphereConfig& cfg, glm::dvec3 eye,
                 bool directSun = false, bool terrainShadows = false,
                 bool scatteringMaterial = false) {
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
        shader.setFloat3("uAtmRayleigh",(directSun || scatteringMaterial) ? .1f : 0.f,0,0);
        shader.setFloat3("uAtmScatter",0,0,0);
        shader.setInt("uAtmTerrainShadowsEnabled", terrainShadows ? 1 : 0);
        shader.setFloat3("uAtmAbsorb",0,0,0);
        shader.setFloat3("uAtmSunlight",directSun ? 5.f : 0.f,0,0);
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
TEST_F(AtmosphereRender, SharedOpticsBindExactCoefficientsAndRefreshDisabledAir) {
    auto scene = config::ScenarioConfig(config::Config::load("tests/scenarios/atmosphere/base.json"));
    rendering::AtmosphereOpticsCache cache(scene);
    auto& planet = scene.planets[0];
    shader.use();
    const auto evaluations = cache.evaluations();
    for (const glm::dvec3 sun : {glm::dvec3(0,1,0), glm::dvec3(1,0,0)}) {
        const auto& optics = cache.get(0, planet, scene.metersPerWorldUnit());
        rendering::bindAtmosphere(shader, planet, optics, sun);
        float rayleigh[3]{}, direction[3]{};
        glGetUniformfv(shader.id, glGetUniformLocation(shader.id, "uAtmRayleigh"), rayleigh);
        glGetUniformfv(shader.id, glGetUniformLocation(shader.id, "uAtmSunDirection"), direction);
        for (int c = 0; c < 3; ++c) {
            EXPECT_FLOAT_EQ(rayleigh[c], float(optics.rayleigh[c]));
            EXPECT_FLOAT_EQ(direction[c], float(sun[c]));
        }
        EXPECT_EQ(cache.evaluations(), evaluations);
    }
    planet.atmosphere.enabled = false;
    rendering::bindAtmosphere(shader, planet, cache.get(0, planet, scene.metersPerWorldUnit()), {0,1,0});
    GLint enabled = 1; GLfloat refractivity = 1;
    glGetUniformiv(shader.id, glGetUniformLocation(shader.id, "uAtmEnabled"), &enabled);
    glGetUniformfv(shader.id, glGetUniformLocation(shader.id, "uAtmRefractivity"), &refractivity);
    EXPECT_EQ(enabled, 0); EXPECT_FLOAT_EQ(refractivity, 0);
    EXPECT_EQ(cache.evaluations(), evaluations + 1);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}
TEST_F(AtmosphereRender, TerrainShadowBlocksDirectScatteringButPreservesAmbient) {
    config::AtmosphereConfig cfg; cfg.enabled = true; cfg.refraction_enabled = false;
    const glm::dvec3 eye(0,1.01,0);
    GLuint shadow = 0;
    glGenTextures(1, &shadow);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, shadow);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    const float border[] = {0,0,0,0};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    shader.use(); shader.setInt("uAtmTerrainShadowMap",1);
    shader.setMat4("uAtmTerrainShadowMatrix",
        glm::value_ptr(glm::scale(glm::mat4(1),glm::vec3(.5f))));
    shader.setFloat("uAtmTerrainShadowBias",0);
    auto sample = [&](float depthValue, bool directSun, bool shadows) {
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D,shadow);
        std::vector<float> depths(16 * 16, depthValue);
        glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,16,16,0,
                     GL_DEPTH_COMPONENT,GL_FLOAT,depths.data());
        return render(cfg,eye,directSun,shadows,true);
    };
    const float lit = sample(1.0f,true,true);
    const float blocked = sample(0.1f,true,true);
    const float noShadow = sample(0.1f,true,false);
    const float ambientOnly = sample(0.1f,false,true);
    EXPECT_GT(lit, blocked + 0.0002f);
    EXPECT_NEAR(lit,noShadow,0.001f);
    EXPECT_NEAR(blocked,ambientOnly,0.001f);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D,0);
    glDeleteTextures(1,&shadow);
    glActiveTexture(GL_TEXTURE0);
}
TEST_F(AtmosphereRender, TerrainShadowMapBindsForAtmosphereAndCanBeDisabled) {
    rendering::TerrainShadowMaps maps;
    config::TerrainShadowConfig settings;
    settings.enabled = true; settings.resolution = 256;
    maps.ensure(1,settings);
    Shader depthShader("shaders/terrain/terrain_shadow.vert", "shaders/terrain/terrain_shadow.frag");
    maps.begin(0,depthShader,{0,1,0},1.2);
    shader.use(); maps.bindForAtmosphere(0,shader,settings);
    GLint enabled = 0, bound = 0;
    glGetUniformiv(shader.id,glGetUniformLocation(shader.id,"uAtmTerrainShadowsEnabled"),&enabled);
    glActiveTexture(GL_TEXTURE1); glGetIntegerv(GL_TEXTURE_BINDING_2D,&bound);
    EXPECT_EQ(enabled,1); EXPECT_NE(bound,0);
    settings.enabled = false;
    shader.use(); maps.bindForAtmosphere(0,shader,settings);
    glGetUniformiv(shader.id,glGetUniformLocation(shader.id,"uAtmTerrainShadowsEnabled"),&enabled);
    EXPECT_EQ(enabled,0);
    glActiveTexture(GL_TEXTURE0);
    glDeleteProgram(depthShader.id);
    EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
}
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
TEST_F(AtmosphereRender, RasterizedDistantBodyKeepsItsSilhouette) {
    config::AtmosphereConfig cfg; cfg.enabled = true;
    // The same visible color ramp represents a distant opaque disk rather
    // than sky. Its finite depth must prevent image-space reprojection from
    // cutting dark bands through the original rasterized geometry.
    const auto clip = projection * glm::vec4(0, 0, -5, 1);
    std::vector<float> depths(size * size, 0.5f + 0.5f * clip.z / clip.w);
    glBindTexture(GL_TEXTURE_2D, depth);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, size, size, GL_RED, GL_FLOAT, depths.data());
    EXPECT_NEAR(render(cfg, {0, 1.001, 0}), 0.5f, 1e-6f);
}
TEST_F(AtmosphereRender, JsonTemperatureChangesRenderedSkyDisplacement) {
    const auto cold = config::AtmosphereConfig(config::Config{
        nlohmann::json{{"temperature_k", 250.0}}});
    const auto warm = config::AtmosphereConfig(config::Config{
        nlohmann::json{{"temperature_k", 330.0}}});
    const float coldPixel = render(cold, {0,1.001,0});
    const float warmPixel = render(warm, {0,1.001,0});
    EXPECT_LT(coldPixel, warmPixel - 0.001f);
    auto disabled = cold;
    disabled.refraction_enabled = false;
    EXPECT_NEAR(render(disabled, {0,1.001,0}), 0.5f, 1e-6f);
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
    rendering::AtmosphereOpticsCache optics(scene);
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
        atmosphere.finish(shader,scene,optics,bodies,light,1,view,projection,eye,framebuffer,false,lookup ? &table : nullptr);
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
TEST_F(AtmosphereRender, HighlightProtectionLimitsWhitePixelsAndPreservesCachedExposure) {
    auto scene = config::ScenarioConfig(config::Config::load("tests/scenarios/atmosphere/base.json"));
    for (auto& planet : scene.planets) planet.atmosphere.enabled = false;
    const std::vector<simulation::BodyState> bodies(scene.planets.size() + 1);
    const rendering::FrameLighting lighting;
    rendering::AtmosphereOpticsCache optics(scene);
    rendering::AtmosphereRenderer renderer;
    auto read = [&](int width, int height) {
        std::vector<unsigned char> pixels(width * height * 4);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        return pixels;
    };
    // Match the output depth/stencil format for the final blit.
    GLuint outputDepth = 0;
    glGenRenderbuffers(1, &outputDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, outputDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, size, size);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, outputDepth);
    for (int width : {size, 121}) { // Resize and partial 8x8 edge tiles.
        for (int brightWidth : {0, 4, 8, width}) {
            renderer.begin(width, size);
            glDisable(GL_SCISSOR_TEST); glDepthMask(GL_TRUE); glClearDepth(1);
            glClearColor(0.1f, 0.1f, 0.1f, 1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
            glEnable(GL_SCISSOR_TEST); glScissor(width - brightWidth, 0, brightWidth, size);
            glClearColor(10000, 10000, 10000, 1); glClear(GL_COLOR_BUFFER_BIT);
            glDisable(GL_SCISSOR_TEST);
            const double exposure = renderer.finish(shader, scene, optics, bodies, lighting, 1,
                glm::mat4(1), projection, {0, 0, 0}, framebuffer, true, nullptr, true);
            const auto pixels = read(width, size);
            int clipped = 0;
            for (std::size_t i = 0; i < pixels.size(); i += 4)
                if (pixels[i] >= 250 && pixels[i + 1] >= 250 && pixels[i + 2] >= 250) ++clipped;
            EXPECT_LE(clipped, width * size / 20);
            if (brightWidth <= 4) EXPECT_DOUBLE_EQ(exposure, 1);
            if (brightWidth >= 8) EXPECT_LT(exposure, 0.02);
            renderer.presentCached(shader, framebuffer);
            EXPECT_EQ(read(width, size), pixels);
            EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
        }
    }
    // Explicit manual output bypasses highlight protection.
    renderer.begin(size, size);
    glClearColor(10000, 10000, 10000, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    EXPECT_DOUBLE_EQ(renderer.finish(shader, scene, optics, bodies, lighting, 1,
        glm::mat4(1), projection, {0, 0, 0}, framebuffer, true), 1);
    const auto manual = read(size, size);
    EXPECT_EQ(manual[0], 255);
    glDeleteRenderbuffers(1, &outputDepth);
}
// Compare the GPU selection with the independent existing CPU sort, including
// tied/zero peaks, half subnormals, partial tiles and the exact 5% boundary.
TEST_F(AtmosphereRender, GpuHighlightSelectionMatchesWeightedReferenceAndSparseGuard) {
    if (!GLEW_VERSION_4_3) GTEST_SKIP() << "GPU reduction requires GL 4.3; fallback is covered above";
    rendering::HighlightReduction reduction;
    GLuint tiles = 0;
    glGenTextures(1, &tiles);
    std::mt19937 random(41892);
    for (auto extent : {glm::ivec2(1,1), glm::ivec2(8,8), glm::ivec2(17,9),
                        glm::ivec2(121,129), glm::ivec2(640,480), glm::ivec2(1921,1081)}) {
        const int width = (extent.x+7)/8, height = (extent.y+7)/8;
        for (int trial=0; trial<12; ++trial) {
            std::vector<float> data(width*height*2);
            std::vector<rendering::HighlightTile> reference;
            std::size_t clipped = 0;
            for (int y=0; y<height; ++y) for (int x=0; x<width; ++x) {
                const auto weight = std::size_t(std::min(8,extent.x-x*8)*std::min(8,extent.y-y*8));
                const auto key = trial<3 ? 0u : trial<6 ? 0x4900u : trial==6 ? 1u :
                    trial==7 ? 0x7c00u : trial==8 ? 0x7bffu : trial==9 ? 0x0400u : random()%0x7c01u;
                const float peak = glm::unpackHalf1x16(static_cast<glm::uint16>(key));
                // At/below/above the guard boundary, then densely clipped.
                const auto remaining = (trial%4<3 ? std::size_t(extent.x)*extent.y/20+trial%4 :
                    std::size_t(extent.x)*extent.y) - clipped;
                const auto count = std::min(weight,remaining);
                data[2*(y*width+x)] = peak; data[2*(y*width+x)+1] = count;
                clipped += count; reference.push_back({peak,weight});
            }
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,tiles);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RG32F,width,height,0,GL_RG,GL_FLOAT,data.data());
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
            const double requested = trial%2 ? 4096.123 : 0.00123456789;
            const double expected = clipped<=std::size_t(extent.x)*extent.y/20 ? requested :
                rendering::highlightLimitedExposure(requested,reference);
            const double actual = reduction.reduce(tiles,extent.x,extent.y,requested);
            EXPECT_DOUBLE_EQ(actual,expected) << extent.x << 'x' << extent.y << " trial " << trial;
            if (expected==requested) EXPECT_DOUBLE_EQ(actual,requested);
            EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
        }
    }
    glDeleteTextures(1,&tiles);
}

// Opt-in focused old/new full-frame comparison. The legacy capability override
// is local to this single-threaded test, never exposed as a product setting.
TEST_F(AtmosphereRender, DISABLED_HighlightFrameCostComparison) {
    if (!GLEW_VERSION_4_3) GTEST_SKIP();
    const char* destination = std::getenv("PLANET_HIGHLIGHT_BENCHMARK");
    ASSERT_NE(destination,nullptr);
    std::ofstream report(destination);
    report << "width,height,pattern,backend,frame,cpu_ms,gpu_ms,readback_bytes\n";
    auto scene = config::ScenarioConfig(config::Config::load("tests/scenarios/atmosphere/base.json"));
    for (auto& planet:scene.planets) planet.atmosphere.enabled=false;
    const std::vector<simulation::BodyState> bodies(scene.planets.size()+1);
    const rendering::FrameLighting lighting;
    rendering::AtmosphereOpticsCache optics(scene);
    GLuint target = 0, color = 0, outputDepth = 0, query = 0;
    glGenFramebuffers(1,&target); glGenTextures(1,&color);
    glGenRenderbuffers(1,&outputDepth); glGenQueries(1,&query);
    for (auto extent:{glm::ivec2(129,121),glm::ivec2(640,480),glm::ivec2(1920,1080)}) {
        glBindFramebuffer(GL_FRAMEBUFFER,target);
        glBindTexture(GL_TEXTURE_2D,color);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,extent.x,extent.y,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
        glBindRenderbuffer(GL_RENDERBUFFER,outputDepth);
        glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH24_STENCIL8,extent.x,extent.y);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER,outputDepth);
        ASSERT_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER),GLenum(GL_FRAMEBUFFER_COMPLETE));
        for (int pattern=0;pattern<2;++pattern) {
            std::vector<unsigned char> reference;
            double referenceExposure=0;
            for (int pair=0;pair<3;++pair) for (int backend=0;backend<2;++backend) {
                rendering::AtmosphereRenderer renderer;
                for (int frame=0;frame<50;++frame) {
                    renderer.begin(extent.x,extent.y);
                    glDisable(GL_SCISSOR_TEST); glDepthMask(GL_TRUE);
                    glClearColor(pattern ? 10000 : 0.1f,pattern ? 10000 : 0.1f,pattern ? 10000 : 0.1f,1);
                    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
                    // A small actual bright area touches many tiles.
                    if (!pattern) {
                        glEnable(GL_SCISSOR_TEST); glScissor(0,0,extent.x,1);
                        glClearColor(10000,10000,10000,1); glClear(GL_COLOR_BUFFER_BIT); glDisable(GL_SCISSOR_TEST);
                    }
                    const auto start=std::chrono::steady_clock::now();
                    glBeginQuery(GL_TIME_ELAPSED,query);
                    const GLboolean capability=GLEW_VERSION_4_3;
                    if (!backend) __GLEW_VERSION_4_3=GL_FALSE;
                    const double exposure=renderer.finish(shader,scene,optics,bodies,lighting,1,
                        glm::mat4(1),projection,{0,0,0},target,true,nullptr,true);
                    __GLEW_VERSION_4_3=capability;
                    glEndQuery(GL_TIME_ELAPSED);
                    // Deliberately synchronous benchmark query after the timed
                    // call; application queries retain their existing policy.
                    GLuint64 elapsed=0; glGetQueryObjectui64v(query,GL_QUERY_RESULT,&elapsed);
                    const double cpu=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
                    if (frame==0) {
                        std::vector<unsigned char> pixels(extent.x*extent.y*4);
                        glReadPixels(0,0,extent.x,extent.y,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
                        if (!backend) {reference=pixels;referenceExposure=exposure;}
                        else {EXPECT_EQ(pixels,reference);EXPECT_FLOAT_EQ(float(exposure),float(referenceExposure));}
                    }
                    if (frame>=10) report << extent.x << ',' << extent.y << ',' << pattern << ',' << backend << ','
                        << pair*40+frame-10 << ',' << cpu << ',' << elapsed/1e6 << ','
                        << (backend ? 8 : ((extent.x+7)/8)*((extent.y+7)/8)*8) << '\n';
                    ASSERT_EQ(glGetError(),GLenum(GL_NO_ERROR));
                }
            }
        }
    }
    glDeleteQueries(1,&query); glDeleteFramebuffers(1,&target); glDeleteTextures(1,&color);
    glDeleteRenderbuffers(1,&outputDepth);
}
} // namespace
int main(int argc,char** argv) {
    testing::InitGoogleTest(&argc,argv);
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,4); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    auto* window = glfwCreateWindow(size,size,"Atmospheric refraction tests",nullptr,nullptr);
    if (!window) {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
        window=glfwCreateWindow(size,size,"Atmospheric fallback tests",nullptr,nullptr);
    }
    if (!window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window); glewExperimental=GL_TRUE;
    if (glewInit()!=GLEW_OK) { glfwDestroyWindow(window); glfwTerminate(); return 1; }
    while (glGetError()!=GL_NO_ERROR) {}
    const int result=RUN_ALL_TESTS();
    glfwDestroyWindow(window); glfwTerminate(); return result;
}

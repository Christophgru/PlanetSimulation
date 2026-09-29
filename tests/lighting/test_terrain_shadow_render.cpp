#include <gtest/gtest.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <array>
#include <vector>
#include "rendering/diagnostics/PngWriter.h"
#include "rendering/geometry/Mesh.h"
#include "rendering/lighting/TerrainShadowMaps.h"
#include "rendering/lighting/CelestialLighting.h"
#include "rendering/foliage/GrassRenderer.h"

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
    Shader terrain{"shaders/terrain/basic.vert", "shaders/terrain/basic.frag", "shaders/terrain/terrain_shadow.glsl"};
    Shader water{"shaders/water/water.vert", "shaders/water/water.frag", "shaders/terrain/terrain_shadow.glsl"};
    Shader depth{"shaders/terrain/terrain_shadow.vert", "shaders/terrain/terrain_shadow.frag"};
    rendering::TerrainShadowMaps maps;
    config::TerrainShadowConfig settings;
    Mesh ridges, sea;
    glm::dvec3 sun = glm::normalize(glm::dvec3(-1, 0, 1));

    GLuint framebuffer = 0, colorBuffer = 0, depthBuffer = 0, reflectionTexture = 0;

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
        glGenTextures(1, &reflectionTexture);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, reflectionTexture);
        const float reflected[] = {0.8f, 0.8f, 0.8f, 1.0f};
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 1, 1, 0, GL_RGBA, GL_FLOAT, reflected);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
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
        glDeleteTextures(1, &reflectionTexture);
    }

    Image render(bool waterSurface = false, glm::dvec3 indirect = glm::dvec3(0),
                 float waterRadiusScale = 1.0f, bool secondBody = false,
                 bool planeReceiver = false, float exposure = 1.0f,
                 float reflectionFraction = 0.0f) {
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
            shader.setFloat("uReflectionFraction", reflectionFraction);
            shader.setInt("uReflectionTexture", 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, reflectionTexture);
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

TEST(TerrainMaterialRender, DetailPreservesGeometryAndFollowsTheBody) {
    ShadowScene scene;
    Mesh patch;
    auto render = [&](float detailScale, glm::vec3 normal, bool direct,
                      float rotation = 0.0f, bool night = false,
                      const config::PlanetConfig::TerrainMaterial& material = {}) {
        patch.vertices.clear(); patch.indices.clear();
        for (auto p : {glm::vec3(-.02,-.02,1), glm::vec3(.02,-.02,1),
                       glm::vec3(.02,.02,1), glm::vec3(-.02,.02,1)})
            patch.addVertex(p.x,p.y,p.z,normal.x,normal.y,normal.z);
        patch.addTriangle(0,1,2); patch.addTriangle(0,2,3); patch.upload();
        glBindFramebuffer(GL_FRAMEBUFFER,scene.framebuffer);
        glViewport(0,0,size,size); glEnable(GL_DEPTH_TEST); glDisable(GL_BLEND);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        const auto model = glm::rotate(glm::mat4(1), rotation, glm::vec3(0,1,0));
        auto& shader = scene.terrain; shader.use();
        shader.setMat4("model",glm::value_ptr(model));
        shader.setMat4("view",glm::value_ptr(glm::lookAt(glm::vec3(0,0,3),glm::vec3(0),glm::vec3(0,1,0))*glm::inverse(model)));
        shader.setMat4("projection",glm::value_ptr(glm::ortho(-.02f,.02f,-.02f,.02f,.1f,10.f)));
        shader.setInt("uLinearOutput",1); shader.setInt("uShadowsEnabled",0);
        shader.setFloat("uClipRadius",-1); shader.setFloat("uEmissive",0);
        shader.setFloat("uTerrainMetersPerRadius",detailScale);
        const auto rockRange = material.slopeMetricRange();
        shader.setFloat2("uTerrainRockRange",rockRange[0],rockRange[1]);
        shader.setFloat3("uTerrainEyeBody",0,0,3);
        shader.setFloat3("uColor",.2f,.5f,.1f);
        shader.setFloat3("uIndirectLight",direct ? 0 : 1,direct ? 0 : 1,direct ? 0 : 1);
        shader.setFloat3("uSunlight",direct ? 1 : 0,direct ? 1 : 0,direct ? 1 : 0);
        const auto sun = glm::mat3(model)*glm::normalize(glm::vec3(.6,0,night ? -1 : 1));
        shader.setFloat3("uSunDirection",sun.x,sun.y,sun.z);
        patch.draw();
        Image image(size*size*3);
        glReadPixels(0,0,size,size,GL_RGB,GL_UNSIGNED_BYTE,image.data());
        EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
        return image;
    };
    const auto plain = render(0,{0,0,1},false);
    std::vector<float> beforeDepth(size*size), afterDepth(size*size);
    glReadPixels(0,0,size,size,GL_DEPTH_COMPONENT,GL_FLOAT,beforeDepth.data());
    const auto textured = render(100,{0,0,1},false);
    glReadPixels(0,0,size,size,GL_DEPTH_COMPONENT,GL_FLOAT,afterDepth.data());
    EXPECT_EQ(beforeDepth,afterDepth);
    EXPECT_EQ(patch.indices.size(),6u);
    EXPECT_NE(plain,textured);
    int low=255, high=0;
    for (std::size_t i=1;i<textured.size();i+=3) {
        low=std::min(low,int(textured[i])); high=std::max(high,int(textured[i]));
    }
    EXPECT_GT(high-low,5); // Detail exists within a pair of flat triangles.
    const auto rock = render(100,{.8660254f,0,.5f},false);
    const auto rockPixel = pixel(rock,0);
    EXPECT_NEAR(rockPixel[0],rockPixel[1],1);
    EXPECT_NEAR(rockPixel[1],rockPixel[2],1);
    const glm::vec3 hillNormal(std::sin(glm::radians(25.f)), 0, std::cos(glm::radians(25.f)));
    const auto grassyHill = pixel(render(100,hillNormal,false),0);
    EXPECT_GT(grassyHill[1],2 * grassyHill[0]);
    config::PlanetConfig::TerrainMaterial earlyRock;
    earlyRock.rock_start_degrees = 10;
    earlyRock.rock_end_degrees = 20;
    const auto rockyHill = pixel(render(100,hillNormal,false,0,false,earlyRock),0);
    EXPECT_NEAR(rockyHill[0],rockyHill[1],1);
    const auto halfway = pixel(render(100,{.7071068f,0,.7071068f},false),0);
    EXPECT_GT(halfway[1]-halfway[0],2);
    EXPECT_LT(halfway[1]-halfway[0],grassyHill[1]-grassyHill[0]);
    const auto lit = render(100,{0,0,1},true);
    const auto rotated = render(100,{0,0,1},true,.7f);
    double error=0;
    for (std::size_t i=0;i<lit.size();++i) error+=std::abs(int(lit[i])-int(rotated[i]));
    EXPECT_LT(error/lit.size(),.1); // Rotating the body/light/camera retains the material.
    const auto night = render(100,{0,0,1},true,0,true);
    EXPECT_EQ(*std::max_element(night.begin(),night.end()),0);
    const auto distant = render(1e6,{0,0,1},false);
    EXPECT_EQ(distant,plain); // Unresolved wavelengths do not shimmer.
    patch.destroy();
}

TEST(TerrainMaterialRender, BeachBandIsNarrowInsideCoarseTriangles) {
    ShadowScene scene;
    Mesh patch;
    for (auto p: {glm::vec3(-.02,-.02,.999),glm::vec3(.02,-.02,1.001),
                  glm::vec3(.02,.02,1.001),glm::vec3(-.02,.02,.999)})
        patch.addVertex(p.x,p.y,p.z,0,0,1);
    patch.addTriangle(0,1,2); patch.addTriangle(0,2,3); patch.upload();
    glBindFramebuffer(GL_FRAMEBUFFER,scene.framebuffer);
    glViewport(0,0,size,size); glEnable(GL_DEPTH_TEST); glDisable(GL_BLEND);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    auto& shader=scene.terrain; shader.use();
    shader.setMat4("model",glm::value_ptr(glm::mat4(1)));
    shader.setMat4("view",glm::value_ptr(glm::lookAt(glm::vec3(0,0,3),glm::vec3(0),glm::vec3(0,1,0))));
    shader.setMat4("projection",glm::value_ptr(glm::ortho(-.02f,.02f,-.02f,.02f,.1f,10.f)));
    shader.setInt("uLinearOutput",1); shader.setInt("uShadowsEnabled",0);
    shader.setFloat("uClipRadius",-1); shader.setFloat("uEmissive",0);
    shader.setFloat("uTerrainMetersPerRadius",1000);
    shader.setFloat2("uTerrainRockRange",.18,.42);
    shader.setFloat3("uTerrainEyeBody",0,0,3);
    shader.setFloat3("uColor",.1,.2,.1);
    shader.setFloat3("uIndirectLight",1,1,1); shader.setFloat3("uSunlight",0,0,0);
    shader.setFloat3("uSunDirection",0,0,1);
    shader.setInt("uLandscapeEnabled",1); shader.setFloat3("uLandscapeLevels",0,.1,40);
    patch.draw();
    Image image(size*size*3);
    glReadPixels(0,0,size,size,GL_RGB,GL_UNSIGNED_BYTE,image.data());
    const auto sand=pixel(image,.08), grass=pixel(image,.7), seabed=pixel(image,-.7);
    EXPECT_GT(sand[0],sand[1]); EXPECT_GT(sand[0],2*grass[0]);
    EXPECT_GT(grass[1],2*grass[0]); EXPECT_LT(seabed[0],grass[0]);
    int sandPixels=0;
    for (int x=0;x<size;++x) {
        const auto offset=(size/2*size+x)*3;
        if (image[offset]>image[offset+1]) ++sandPixels;
    }
    EXPECT_GT(sandPixels,3); EXPECT_LT(sandPixels,25);
    EXPECT_EQ(patch.indices.size(),6u);
    patch.destroy();
}

TEST(GrassRender, WindMovesBladesWhileNightAndClippingRemainDark) {
    ShadowScene scene;
    rendering::GrassRenderer grass;
    config::PlanetConfig planet;
    planet.radius=1; planet.color={.2,.6,.1}; planet.foliage.enabled=true;
    planet.foliage.draw_distance_m=10; planet.foliage.max_blades=4000;
    Mesh ground;
    ground.hasVertexColors=true;
    for (glm::vec3 p : {glm::vec3(-.1,-.1,1),glm::vec3(.1,-.1,1),glm::vec3(.1,.1,1),glm::vec3(-.1,.1,1)}) {
        for (const auto v : {p,glm::vec3(0,0,1),glm::vec3(1)})
            for (int c=0;c<3;++c) ground.vertices.push_back(v[c]);
    }
    ground.indices={0,1,2,0,2,3};
    const glm::vec3 eye(0,-5,102);
    grass.prepare(0,ground,planet,100,glm::dvec3(eye)/100.0);
    ASSERT_GT(grass.count(0),500u);
    GLuint occluder=0;
    glGenTextures(1,&occluder); glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D,occluder);
    const float depth=0;
    glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,1,1,0,GL_DEPTH_COMPONENT,GL_FLOAT,&depth);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_COMPARE_MODE,GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_COMPARE_FUNC,GL_LEQUAL);
    auto render=[&](float time,bool night=false,float clip=-1.0f,bool shadow=false) {
        glBindFramebuffer(GL_FRAMEBUFFER,scene.framebuffer);
        glViewport(0,0,size,size); glEnable(GL_DEPTH_TEST); glDisable(GL_BLEND);
        glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        auto& s=grass.shader; s.use();
        GLint linked=GL_FALSE; glGetProgramiv(s.id,GL_LINK_STATUS,&linked); EXPECT_EQ(linked,GL_TRUE);
        s.setMat4("model",glm::value_ptr(glm::scale(glm::mat4(1),glm::vec3(100))));
        s.setMat4("view",glm::value_ptr(glm::lookAt(eye,glm::vec3(0,3,100.5),glm::vec3(0,0,1))));
        s.setMat4("projection",glm::value_ptr(glm::perspective(glm::radians(60.f),1.f,.1f,30.f)));
        s.setFloat3("uGrassEyeBody",0,-.05f,1.02f);
        s.setFloat3("uViewEyeWorld",eye.x,eye.y,eye.z);
        s.setFloat3("uClipCenter",0,0,0); s.setFloat("uClipRadius",clip);
        s.setFloat("uMetersPerRadius",100); s.setFloat("uGrassHeight",1.5f);
        s.setFloat("uGrassWidth",.1f); s.setFloat("uDrawDistance",10);
        s.setFloat("uWindStrength",1); s.setFloat("uTime",time);
        s.setInt("uShadowsEnabled",shadow); s.setInt("uLinearOutput",1);
        s.setInt("uShadowMap",4); s.setFloat("uShadowBias",.0001f);
        s.setMat4("uShadowMatrix",glm::value_ptr(glm::translate(glm::mat4(1),glm::vec3(0,0,-1))));
        s.setFloat3("uSunDirection",0,0,night ? -1 : 1);
        s.setFloat3("uSunlight",1,1,1); s.setFloat3("uIndirectLight",0,0,0);
        grass.draw(0);
        Image result(size*size*3);
        glReadPixels(0,0,size,size,GL_RGB,GL_UNSIGNED_BYTE,result.data());
        EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
        return result;
    };
    const auto first=render(0);
    EXPECT_GT(std::count_if(first.begin(),first.end(),[](auto c) { return c>20; }),1000);
    EXPECT_EQ(first,render(0));
    EXPECT_NE(first,render(2));
    const auto night=render(0,true),clipped=render(0,false,200);
    EXPECT_EQ(*std::max_element(night.begin(),night.end()),0);
    EXPECT_EQ(*std::max_element(clipped.begin(),clipped.end()),0);
    const auto shadow=render(0,false,-1,true);
    EXPECT_EQ(*std::max_element(shadow.begin(),shadow.end()),0);
    glDeleteTextures(1,&occluder);
    grass.clear(); EXPECT_EQ(grass.count(0),0u);
}

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
    EXPECT_NO_THROW(rendering::writePng(PLANET_SHADOW_IMAGE, 2 * size, size, 3, comparison));
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

TEST(TerrainShadowRender, NightOceanDoesNotMirrorBrightDayScene) {
    ShadowScene scene;
    const glm::dvec3 indirect(0.001, 0.001, 0.001);
    scene.sun = {0,0,-1}; // Sea faces the camera, but the Sun is behind it.
    const auto night = scene.render(true, indirect, 1, false, false, 1, 1);
    for (int x : {-0.7f, 0.0f, 0.7f}) {
        const auto value = pixel(night,x);
        for (int c : value) EXPECT_LT(c, 25) << "night water x=" << x;
    }
    scene.sun = {0,0,1};
    const auto day = scene.render(true, indirect, 1, false, false, 1, 1);
    EXPECT_GT(pixel(day,0.7)[0], 150);
}
TEST(TerrainMaterialRender, WaterKeepsSmoothSharpReflections) {
    ShadowScene scene;
    scene.settings.enabled = false;
    scene.sun = {0,0,1};
    // A sharp two-color source must retain its clean boundary on the sea.
    // Terrain's procedural relief and matte response must not enter this pass.
    const float reflected[] = {.2f,.4f,.8f,1, .8f,.4f,.2f,1};
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,scene.reflectionTexture);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,2,1,0,GL_RGBA,GL_FLOAT,reflected);
    const auto mirror = scene.render(true,glm::dvec3(0),1,false,false,1,1);
    for (double y : {-.7,0.,.7}) {
        for (double x : {-.7,-.1,-.01,.01,.1,.7}) {
            const auto actual = pixel(mirror,x,y);
            const std::array<int,3> expected = x < 0 ?
                std::array<int,3>{51,102,204} : std::array<int,3>{204,102,51};
            for (int c=0;c<3;++c) EXPECT_NEAR(actual[c],expected[c],1);
        }
    }
    const auto diffuse = scene.render(true);
    EXPECT_NE(mirror,diffuse);
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

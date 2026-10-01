#include "rendering/foliage/horizon/HorizonGrass.h"
#include "rendering/foliage/GrassWind.h"
#include "rendering/geometry/Mesh.h"
#include "config/ScenarioConfig.h"
#include <gtest/gtest.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace {
// Link the production vertex source for feedback, then use the production
// descriptor VAOs/divisors/draws. This checks real fetch rates and GPU roots.
class HorizonProbe {
    GLuint buffer_=0;
public:
    rendering::HorizonGrass grass;
    config::PlanetConfig planet;
    Mesh mesh;
    HorizonProbe(float pole=1) {
        planet.radius=1; planet.color={.2,.6,.1}; planet.foliage.enabled=true;
        planet.foliage.draw_distance_m=10; planet.foliage.far_distance_m=100;
        planet.foliage.far_max_instances=4096;
        mesh.hasVertexColors=true; mesh.revision=1;
        for (auto p:{glm::vec3(.2,0,pole),glm::vec3(.3,0,pole),glm::vec3(.2,.15,pole),
                    glm::vec3(-.6,0,pole),glm::vec3(-.3,0,pole),glm::vec3(-.6,.3,pole)}) {
            for (const auto v:{p,glm::vec3(0,0,pole),glm::vec3(1)})
                for (int c=0;c<3;++c) mesh.vertices.push_back(v[c]);
        }
        mesh.indices={0,1,2,3,4,5};
        const auto prepared=grass.prepare(0,mesh,planet,100,{0,0,pole*1.02});
        EXPECT_EQ(prepared.rebuilds,1);
        EXPECT_EQ(grass.stats(0).patches,2);
        EXPECT_GT(grass.stats(0).batches,1);
        // Shader releases its attached objects after linking. Reattach the
        // production vertex source before linking the feedback executable.
        std::ifstream file("shaders/foliage/horizon.vert");
        const std::string source{std::istreambuf_iterator<char>(file), {}};
        const char* code=source.c_str();
        const GLuint vertex=glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex,1,&code,nullptr);
        glCompileShader(vertex);
        GLint ok=0; glGetShaderiv(vertex,GL_COMPILE_STATUS,&ok);
        if (!ok) { glDeleteShader(vertex); throw std::runtime_error("Cannot compile horizon feedback shader"); }
        glAttachShader(grass.shader.id,vertex);
        const char* outputs[]={"vRoot","vBodyPosition","vWorldPosition","vVisible"};
        glTransformFeedbackVaryings(grass.shader.id,4,outputs,GL_INTERLEAVED_ATTRIBS);
        glLinkProgram(grass.shader.id);
        glDetachShader(grass.shader.id,vertex); glDeleteShader(vertex);
        glGetProgramiv(grass.shader.id,GL_LINK_STATUS,&ok);
        if (!ok) throw std::runtime_error("Cannot link horizon feedback shader");
        grass.shader.use();
        for (const char* name:{"model","view","projection","uShadowMatrix"})
            grass.shader.setMat4(name,glm::value_ptr(glm::mat4(1)));
        grass.shader.setFloat("uMetersPerRadius",100);
        grass.shader.setFloat("uDrawDistance",10); grass.shader.setFloat("uFarDistance",100);
        grass.shader.setFloat("uGrassHeight",1); grass.shader.setFloat("uGrassWidth",.08);
        grass.shader.setFloat("uWindStrength",1);
        grass.shader.setFloat3("uGrassEyeBody",0,0,pole*1.02);
        grass.shader.setFloat3("uFacingEyeBody",0,0,pole*1.02);
        grass.shader.setFloat3("uPlanetColor",.2,.6,.1);
        grass.shader.setFloat2("uTerrainRockRange",.35,.5);
        grass.shader.setFloat3("uLandscapeLevels",0,.1,100);
        glGenBuffers(1,&buffer_);
    }
    ~HorizonProbe() { glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,0); glDeleteBuffers(1,&buffer_); }
    std::vector<float> capture(double seconds=0) {
        grass.shader.use(); grass.shader.setFloat("uTime",rendering::grassWindTime(seconds));
        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,buffer_);
        std::vector<float> result(grass.stats(0).candidates*3*10);
        glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,result.size()*sizeof(float),nullptr,GL_STREAM_READ);
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,buffer_);
        glEnable(GL_RASTERIZER_DISCARD); glBeginTransformFeedback(GL_TRIANGLES);
        grass.draw(0);
        glEndTransformFeedback(); glDisable(GL_RASTERIZER_DISCARD);
        glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER,0,result.size()*sizeof(float),result.data());
        EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
        for (float f:result) EXPECT_TRUE(std::isfinite(f));
        return result;
    }
};
}

TEST(HorizonGrassRender, GPUPlacesStableRootsOnTheUploadedTrianglesUsingRealDivisors) {
    HorizonProbe probe;
    const auto first=probe.capture(), repeated=probe.capture();
    ASSERT_EQ(first,repeated);
    unsigned visible=0, positive=0, negative=0;
    for (std::size_t i=0;i<first.size();i+=30) {
        const float x=first[i],y=first[i+1];
        EXPECT_NEAR(first[i+2],1,1e-6f);
        // The two patches deliberately use different slots per descriptor.
        // Every root must remain in one of their barycentric triangles.
        if (x>0) {
            ++positive; EXPECT_GE(x,.2f); EXPECT_LE(x,.3f); EXPECT_GE(y,0);
            EXPECT_LE((x-.2f)/.1f+y/.15f,1.00001f);
        } else {
            ++negative; EXPECT_GE(x,-.6f); EXPECT_LE(x,-.3f); EXPECT_GE(y,0);
            EXPECT_LE((x+.6f)/.3f+y/.3f,1.00001f);
        }
        for (int v=1;v<3;++v) for (int c=0;c<3;++c) EXPECT_EQ(first[i+c],first[i+10*v+c]);
        if (first[i+9]>0) {
            ++visible;
            EXPECT_GT(glm::length(glm::vec3(x,y,first[i+2])-glm::vec3(0,0,1.02))*100,10);
        }
    }
    EXPECT_GT(positive,0); EXPECT_GT(negative,0); EXPECT_GT(visible,10);
    EXPECT_EQ(probe.grass.stats(0).vertices,probe.grass.stats(0).candidates*3);
    EXPECT_EQ(probe.grass.stats(0).patchBytes,2*sizeof(rendering::HorizonGrassPatch));
    const auto cached=probe.grass.prepare(0,probe.mesh,probe.planet,100,{.001,0,1.02});
    EXPECT_EQ(cached.rebuilds,0);
    // Crossing the near layer's 1.5 m threshold must not replan distant roots.
    EXPECT_EQ(probe.grass.prepare(0,probe.mesh,probe.planet,100,{.05,0,1.02}).rebuilds,0);
    EXPECT_EQ(probe.grass.prepare(0,probe.mesh,probe.planet,100,{.11,0,1.02}).rebuilds,1);
    ++probe.mesh.revision;
    EXPECT_EQ(probe.grass.prepare(0,probe.mesh,probe.planet,100,{.11,0,1.02}).rebuilds,1);
    probe.planet.foliage.horizon_enabled=false;
    probe.grass.prepare(0,probe.mesh,probe.planet,100,{0,0,1.02});
    EXPECT_EQ(probe.grass.stats(0).candidates,0); probe.grass.clear();
    EXPECT_EQ(probe.grass.stats(0).patchBytes,0);
}

TEST(HorizonGrassRender, WaterSnowAndDistanceRejectTuftsBeforeWindWork) {
    HorizonProbe probe;
    auto invisible=[&] {
        const auto data=probe.capture();
        for (std::size_t i=9;i<data.size();i+=10) EXPECT_EQ(data[i],0);
    };
    probe.grass.shader.setInt("uWaterEnabled",1);
    probe.grass.shader.setFloat3("uLandscapeLevels",100,.1,100); invisible();
    probe.grass.shader.setInt("uWaterEnabled",0);
    probe.grass.shader.setInt("uLandscapeEnabled",1);
    probe.grass.shader.setFloat3("uLandscapeLevels",-100,.1,0); invisible();
    probe.grass.shader.setInt("uLandscapeEnabled",0);
    probe.grass.shader.setFloat("uFarDistance",10); invisible();
    probe.grass.shader.setFloat("uFarDistance",100);
    probe.grass.shader.setFloat("uDrawDistance",200); invisible();
}

TEST(HorizonGrassRender, ConfiguredShapeFadeAndBiomeControlsReachTheGPU) {
    HorizonProbe probe;
    const auto original=probe.capture();
    probe.grass.shader.setFloat3("uFarShape",2,10,.5);
    EXPECT_NE(original,probe.capture());
    probe.grass.shader.setFloat3("uFarFadeFractions",.5,.75,.1);
    EXPECT_NE(original,probe.capture());
    probe.grass.shader.setFloat("uGreenRatio",4);
    const auto rejected=probe.capture();
    for (std::size_t i=9;i<rejected.size();i+=10) EXPECT_EQ(rejected[i],0);
}

TEST(HorizonGrassRender, WindIsBodyLocalAndContinuousAtTheClockWrapAndPoles) {
    for (float pole:{-1.f,1.f}) {
        HorizonProbe probe(pole);
        const auto still=probe.capture(0), moving=probe.capture(2);
        EXPECT_NE(still,moving);
        for (std::size_t i=0;i<still.size();i+=10) for (int c=0;c<3;++c) EXPECT_EQ(still[i+c],moving[i+c]);
        EXPECT_EQ(probe.capture(2),probe.capture(2+rendering::grassWindPeriodSeconds));
        const auto before=probe.capture(4096-.001), after=probe.capture(4096+.001);
        for (std::size_t i=0;i<before.size();i+=10) for (int c=3;c<6;++c) EXPECT_NEAR(before[i+c],after[i+c],.00003f);
        probe.grass.shader.setFloat("uWindStrength",0);
        EXPECT_EQ(probe.capture(0),probe.capture(2));
        const auto local=probe.capture(0);
        const auto transform=glm::rotate(glm::translate(glm::mat4(1),{12,-8,3}),.7f,glm::vec3(0,1,0));
        probe.grass.shader.setMat4("model",glm::value_ptr(transform));
        const auto transformed=probe.capture(0);
        for (std::size_t i=0;i<local.size();i+=10) {
            for (int c=0;c<6;++c) EXPECT_EQ(local[i+c],transformed[i+c]);
            const auto expected=transform*glm::vec4(local[i+3],local[i+4],local[i+5],1);
            for (int c=0;c<3;++c) EXPECT_NEAR(transformed[i+6+c],expected[c],.00001f);
        }
    }
}

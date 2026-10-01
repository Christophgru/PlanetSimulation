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
    float pole_=1;
public:
    rendering::HorizonGrass grass;
    config::PlanetConfig planet;
    Mesh mesh;
    HorizonProbe(float pole=1,bool detailed=false) : pole_(pole), grass(detailed) {
        planet.radius=1; planet.color={.2,.6,.1}; planet.foliage.enabled=true;
        planet.foliage.draw_distance_m=10; planet.foliage.far_distance_m=100;
        planet.foliage.far_max_instances=4096;
        planet.foliage.height_m=1; planet.foliage.width_m=.08; planet.foliage.root_offset_m=0;
        if (detailed) {
            planet.foliage.draw_distance_m=100; planet.foliage.far_distance_m=150;
            planet.foliage.max_blades=4096;
        }
        mesh.hasVertexColors=true; mesh.revision=1;
        for (auto p:{glm::vec3(.2,0,pole),glm::vec3(.3,0,pole),glm::vec3(.2,.15,pole),
                    glm::vec3(-.6,0,pole),glm::vec3(-.3,0,pole),glm::vec3(-.6,.3,pole)}) {
            for (const auto v:{p,glm::vec3(0,0,pole),glm::vec3(1)})
                for (int c=0;c<3;++c) mesh.vertices.push_back(v[c]);
        }
        if (detailed) for (std::size_t v=0;v<3;++v) mesh.vertices[v*9]+=.05f;
        mesh.indices={0,1,2,3,4,5};
        mesh.upload();
        const auto prepared=grass.prepare(0,mesh,planet,100,{0,0,pole*1.02});
        EXPECT_EQ(prepared.rebuilds,1);
        EXPECT_EQ(grass.stats(0).patches,2);
        EXPECT_GT(grass.stats(0).batches,1);
        // Shader releases its attached objects after linking. Reattach the
        // production vertex source before linking the feedback executable.
        std::ifstream file(detailed ? "shaders/foliage/grass.vert" : "shaders/foliage/horizon.vert");
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
        grass.shader.setFloat("uDrawDistance",planet.foliage.draw_distance_m); grass.shader.setFloat("uFarDistance",100);
        grass.shader.setFloat("uGrassHeight",1); grass.shader.setFloat("uGrassWidth",.08);
        grass.shader.setFloat("uWindStrength",1);
        grass.shader.setFloat3("uGrassEyeBody",0,0,pole*1.02);
        grass.shader.setFloat3("uFacingEyeBody",0,0,pole*1.02);
        grass.shader.setFloat3("uPlanetColor",.2,.6,.1);
        grass.shader.setFloat2("uTerrainRockRange",.35,.5);
        grass.shader.setFloat3("uLandscapeLevels",0,.1,100);
        grass.shader.setFloat("uGaussianSigma",planet.foliage.draw_distance_m/3);
        grass.shader.setFloat("uRootOffset",0);
        glGenBuffers(1,&buffer_);
    }
    ~HorizonProbe() { mesh.destroy(); glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,0); glDeleteBuffers(1,&buffer_); }
    std::vector<float> capture(double seconds=0,bool compute=false) {
        grass.shader.use(); grass.shader.setFloat("uTime",rendering::grassWindTime(seconds));
        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,buffer_);
        std::vector<float> result(grass.stats(0).triangles*3*10);
        glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,result.size()*sizeof(float),nullptr,GL_STREAM_READ);
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,buffer_);
        glEnable(GL_RASTERIZER_DISCARD); glBeginTransformFeedback(GL_TRIANGLES);
        rendering::GrassPass pass;
        pass.mainEyeBody={0,0,pole_*1.02}; pass.windTime=rendering::grassWindTime(seconds); pass.feedback=true;
        grass.draw(0,compute ? &pass : nullptr);
        glEndTransformFeedback(); glDisable(GL_RASTERIZER_DISCARD);
        glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER,0,result.size()*sizeof(float),result.data());
        if (grass.usesCompute(0)) {
            const auto count=grass.computedCounts(0);
            result.resize((count[0]*12+count[1]*2)*3*10);
        }
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
    for (std::size_t i=0;i<first.size();i+=60) {
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
        for (int v=1;v<6;++v) for (int c=0;c<3;++c) EXPECT_EQ(first[i+c],first[i+10*v+c]);
        if (first[i+9]>0) {
            ++visible;
            EXPECT_GT(glm::length(glm::vec3(x,y,first[i+2])-glm::vec3(0,0,1.02))*100,10);
        }
    }
    EXPECT_GT(positive,0); EXPECT_GT(negative,0); EXPECT_GT(visible,10);
    EXPECT_EQ(probe.grass.stats(0).vertices,probe.grass.stats(0).candidates*4);
    EXPECT_EQ(probe.grass.stats(0).patchBytes,2*sizeof(std::uint32_t));
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

TEST(ProceduralGrassRender, GPUGeneratedNearRootsHotSwapWithoutAnUpload) {
    HorizonProbe probe(1,true);
    const auto original=probe.capture();
    const auto stats=probe.grass.stats(0);
    ASSERT_EQ(stats.vertices,stats.candidates*4);
    EXPECT_LE(stats.candidates,4096);
    EXPECT_LT(stats.patchBytes,stats.candidates*40);
    unsigned visible=0;
    for (std::size_t i=0;i<original.size();i+=60) {
        const glm::vec3 root(original[i],original[i+1],original[i+2]);
        EXPECT_NEAR(root.z,1,1e-6f);
        for (int v=1;v<6;++v) for (int c=0;c<3;++c) EXPECT_EQ(original[i+c],original[i+v*10+c]);
        if (original[i+9]>0) {
            ++visible;
            const glm::vec3 left(original[i+3],original[i+4],original[i+5]);
            const glm::vec3 right(original[i+13],original[i+14],original[i+15]);
            EXPECT_LT(glm::length((left+right)*.5f-root),1e-6);
            const glm::vec3 topLeft(original[i+23],original[i+24],original[i+25]);
            const glm::vec3 topRight(original[i+53],original[i+54],original[i+55]);
            EXPECT_GT(glm::length(topLeft-topRight),1e-6);
            EXPECT_GT(glm::length(glm::cross(right-left,topLeft-left)),1e-9);
            EXPECT_GT(glm::length(glm::cross(right-topLeft,topRight-topLeft)),1e-9);
        }
    }
    EXPECT_GT(visible,10);
    const auto moved=probe.grass.prepare(0,probe.mesh,probe.planet,100,{.11,0,1.02});
    EXPECT_EQ(moved.rebuilds,0); EXPECT_EQ(moved.uploadMs,0);
    const auto detailed=probe.grass.stats(0);
    EXPECT_EQ(detailed.patchBytes,stats.patchBytes); EXPECT_EQ(detailed.candidates,stats.candidates);
    EXPECT_GT(detailed.vertices,stats.vertices);
    EXPECT_EQ(probe.grass.prepare(0,probe.mesh,probe.planet,100,{0,0,1.02}).rebuilds,0);
    EXPECT_EQ(probe.grass.stats(0).vertices,stats.vertices);
    EXPECT_EQ(probe.capture(),original);
}

TEST(ProceduralGrassRender, FrustumRejectionIncludesBladeReachAndChecksTheActivePass) {
    for (bool detailed:{false,true}) {
        HorizonProbe probe(1,detailed);
        const auto original=probe.capture();
        probe.grass.shader.setInt("uFrustumCull",1);
        probe.grass.shader.setFloat("uCullExtent",.05);
        EXPECT_EQ(probe.capture(),original); // A patch crossing far plane stays.
        const auto outside=glm::translate(glm::mat4(1),glm::vec3(4,0,0));
        probe.grass.shader.setMat4("projection",glm::value_ptr(outside));
        const auto culled=probe.capture();
        for (std::size_t i=9;i<culled.size();i+=10) EXPECT_EQ(culled[i],0);
        probe.grass.shader.setMat4("projection",glm::value_ptr(glm::mat4(1)));
        EXPECT_EQ(probe.capture(),original);
    }
}

TEST(ProceduralGrassRender, RootsReadResidentTerrainBuffersAndRespondToGPUReupload) {
    HorizonProbe probe;
    const auto original=probe.capture();
    for (std::size_t v=0;v<probe.mesh.vertices.size();v+=9) probe.mesh.vertices[v]+=.3f;
    ++probe.mesh.revision;
    probe.grass.prepare(0,probe.mesh,probe.planet,100,{0,0,1.02});
    // Changing only CPU geometry cannot change the GPU's root source.
    EXPECT_EQ(probe.capture(),original);
    probe.mesh.upload();
    probe.grass.prepare(0,probe.mesh,probe.planet,100,{0,0,1.02});
    const auto moved=probe.capture();
    ASSERT_EQ(moved.size(),original.size());
    for (std::size_t i=0;i<moved.size();i+=10) {
        EXPECT_NEAR(moved[i],original[i]+.3f,1e-6f);
        EXPECT_NEAR(moved[i+1],original[i+1],1e-6f);
        EXPECT_NEAR(moved[i+2],original[i+2],1e-6f);
    }
}

TEST(ProceduralGrassCompute, CompactedRootsAndWindMatchTheVertexFallbackAtBothPoles) {
    if (!GLEW_VERSION_4_3) return;
    for (bool detailed:{false,true}) for (float pole:{-1.f,1.f}) {
        HorizonProbe probe(pole,detailed);
        const auto reference=probe.capture(3);
        const auto computed=probe.capture(3,true);
        ASSERT_TRUE(probe.grass.usesCompute(0));
        const auto counts=probe.grass.computedCounts(0);
        EXPECT_GT(counts[0]+counts[1],10); EXPECT_LE(counts[0]+counts[1],probe.grass.stats(0).candidates);
        EXPECT_LT(computed.size(),reference.size());
        // Compaction can reorder instances. Match each generated vertex to
        // its fallback root and position, excluding retired candidates.
        for (std::size_t i=0;i<computed.size();i+=10) {
            bool found=false;
            for (std::size_t j=0;j<reference.size();j+=10) {
                float delta=0;
                for (int c=0;c<6;++c) delta=std::max(delta,std::abs(computed[i+c]-reference[j+c]));
                if (delta<2e-6f && reference[j+9]>0) { found=true; break; }
            }
            ASSERT_TRUE(found) << "Unmatched generated vertex at " << i;
        }
        probe.planet.foliage.compute_placement=false;
        probe.grass.prepare(0,probe.mesh,probe.planet,100,{0,0,pole*1.02});
        EXPECT_EQ(probe.capture(3,true),reference);
        EXPECT_FALSE(probe.grass.usesCompute(0));
    }
}

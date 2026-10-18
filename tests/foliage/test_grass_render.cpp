#include "rendering/foliage/procedural/ProceduralGrass.h"
#include "rendering/foliage/GrassWind.h"
#include "rendering/geometry/Mesh.h"
#include "rendering/geometry/Terrain.h"
#include "rendering/runtime/ResourceOwners.h"
#include "rendering/foliage/cells/CellIdentity.h"
#include "config/ScenarioConfig.h"
#include "rendering/quality/OfflineQuality.h"
#include <gtest/gtest.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <iostream>
#include <stdexcept>

namespace {
// Link the production vertex source for feedback, then use the production
// descriptor VAOs/divisors/draws. This checks real fetch rates and GPU roots.
class GrassProbe {
    GLuint buffer_=0;
    float pole_=1;
public:
    rendering::ProceduralGrass grass;
    config::PlanetConfig planet;
    Mesh mesh;
    GrassProbe(float pole=1) : pole_(pole) {
        planet.radius=1; planet.color={.2,.6,.1}; planet.foliage.enabled=true;
        planet.foliage.draw_distance_m=100; planet.foliage.max_blades=4096;
        planet.foliage.height_m=1; planet.foliage.width_m=.08; planet.foliage.root_offset_m=0;
        mesh.hasVertexColors=true; mesh.revision=1;
        for (auto p:{glm::vec3(.2,0,pole),glm::vec3(.3,0,pole),glm::vec3(.2,.15,pole),
                    glm::vec3(-.6,0,pole),glm::vec3(-.3,0,pole),glm::vec3(-.6,.3,pole)}) {
            for (const auto v:{p,glm::vec3(0,0,pole),glm::vec3(1)})
                for (int c=0;c<3;++c) mesh.vertices.push_back(v[c]);
        }
        for (std::size_t v=0;v<3;++v) mesh.vertices[v*9]+=.05f;
        mesh.indices={0,1,2,3,4,5};
        mesh.upload();
        const auto prepared=grass.prepare(0,mesh,planet,100,{0,0,pole*1.02});
        EXPECT_EQ(prepared.rebuilds,1);
        EXPECT_EQ(grass.stats(0).patches,2);
        EXPECT_GT(grass.stats(0).batches,1);
        // Shader releases its attached objects after linking. Reattach the
        // production vertex source before linking the feedback executable.
        const std::string source=Shader::source("shaders/foliage/grass.vert");
        const char* code=source.c_str();
        const GLuint vertex=glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex,1,&code,nullptr);
        glCompileShader(vertex);
        GLint ok=0; glGetShaderiv(vertex,GL_COMPILE_STATUS,&ok);
        if (!ok) { glDeleteShader(vertex); throw std::runtime_error("Cannot compile grass feedback shader"); }
        glAttachShader(grass.shader.id,vertex);
        const char* outputs[]={"vRoot","vBodyPosition","vWorldPosition","vVisible"};
        glTransformFeedbackVaryings(grass.shader.id,4,outputs,GL_INTERLEAVED_ATTRIBS);
        glLinkProgram(grass.shader.id);
        glDetachShader(grass.shader.id,vertex); glDeleteShader(vertex);
        glGetProgramiv(grass.shader.id,GL_LINK_STATUS,&ok);
        if (!ok) throw std::runtime_error("Cannot link grass feedback shader");
        grass.shader.use();
        for (const char* name:{"model","view","projection","uShadowMatrix"})
            grass.shader.setMat4(name,glm::value_ptr(glm::mat4(1)));
        grass.shader.setFloat("uMetersPerRadius",100);
        grass.shader.setFloat("uDrawDistance",planet.foliage.draw_distance_m);
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
    ~GrassProbe() { mesh.destroy(); glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,0); glDeleteBuffers(1,&buffer_); }
    std::vector<float> capture(double seconds=0,bool compute=false,const rendering::GrassPass* view=nullptr) {
        grass.shader.use(); grass.shader.setFloat("uTime",rendering::grassWindTime(seconds));
        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,buffer_);
        std::vector<float> result(grass.stats(0).triangles*3*10);
        glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,result.size()*sizeof(float),nullptr,GL_STREAM_READ);
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,buffer_);
        glEnable(GL_RASTERIZER_DISCARD); glBeginTransformFeedback(GL_TRIANGLES);
        rendering::GrassPass pass=view?*view:rendering::GrassPass{};
        if(!view) pass.mainEyeBody={0,0,pole_*1.02};
        pass.windTime=rendering::grassWindTime(seconds); pass.feedback=true;
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

TEST(ProceduralGrassRender, CachedPlansRebuildOnlyForMovementOrMeshRevision) {
    GrassProbe probe;
    EXPECT_EQ(probe.capture(),probe.capture());
    EXPECT_EQ(probe.grass.stats(0).patchBytes,2*sizeof(std::uint32_t));
    EXPECT_EQ(probe.grass.prepare(0,probe.mesh,probe.planet,100,{.11,0,1.02}).rebuilds,0);
    EXPECT_EQ(probe.grass.prepare(0,probe.mesh,probe.planet,100,{.16,0,1.02}).rebuilds,1);
    ++probe.mesh.revision;
    EXPECT_EQ(probe.grass.prepare(0,probe.mesh,probe.planet,100,{.16,0,1.02}).rebuilds,1);
    probe.planet.foliage.enabled=false;
    probe.grass.prepare(0,probe.mesh,probe.planet,100,{0,0,1.02});
    EXPECT_EQ(probe.grass.stats(0).candidates,0);
    probe.grass.clear(); EXPECT_EQ(probe.grass.stats(0).patchBytes,0);
}

TEST(ProceduralGrassRender, WaterSnowAndDistanceRejectBladesBeforeWindWork) {
    GrassProbe probe;
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
    probe.grass.shader.setFloat("uDrawDistance",10); invisible();
}

TEST(ProceduralGrassRender, ConfiguredShapeFadeAndBiomeControlsReachTheGPU) {
    GrassProbe probe;
    const auto original=probe.capture();
    probe.grass.shader.setFloat("uGrassHeight",2);
    EXPECT_NE(original,probe.capture());
    probe.grass.shader.setFloat("uDrawDistance",40);
    EXPECT_NE(original,probe.capture());
    probe.grass.shader.setFloat("uGreenRatio",4);
    const auto rejected=probe.capture();
    for (std::size_t i=9;i<rejected.size();i+=10) EXPECT_EQ(rejected[i],0);
}

TEST(ProceduralGrassRender, WindIsBodyLocalAndContinuousAtTheClockWrapAndPoles) {
    for (float pole:{-1.f,1.f}) {
        GrassProbe probe(pole);
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
    GrassProbe probe;
    const auto original=probe.capture();
    const auto stats=probe.grass.stats(0);
    ASSERT_EQ(stats.vertices,stats.candidates*4);
    EXPECT_LE(stats.candidates,4096);
    EXPECT_LT(stats.patchBytes,stats.candidates*40);
    unsigned visible=0;
    for (std::size_t i=0;i<original.size();i+=60) {
        const glm::vec3 root(original[i],original[i+1],original[i+2]);
        if (original[i+9]>0) EXPECT_NEAR(root.z,1,1e-6f);
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
    {
        GrassProbe probe;
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
    GrassProbe probe;
    const auto original=probe.capture();
    // Edit only the resident terrain buffer: the CPU plan and triangle IDs stay fixed.
    auto shifted=probe.mesh.vertices;
    for (std::size_t v=0;v<shifted.size();v+=9) shifted[v]+=.01f;
    glBindBuffer(GL_ARRAY_BUFFER,probe.mesh.vbo);
    glBufferSubData(GL_ARRAY_BUFFER,0,shifted.size()*sizeof(float),shifted.data());
    const auto moved=probe.capture();
    ASSERT_EQ(moved.size(),original.size());
    for (std::size_t i=0;i<moved.size();i+=10) {
        // Rejected candidates have zeroed roots in the vertex fallback.
        if (original[i+9]<=0 || moved[i+9]<=0) continue;
        EXPECT_NEAR(moved[i],original[i]+.01f,1e-6f);
        EXPECT_NEAR(moved[i+1],original[i+1],1e-6f);
        EXPECT_NEAR(moved[i+2],original[i+2],1e-6f);
    }
    EXPECT_NE(moved,original);
}

TEST(ProceduralGrassCompute, CompactedRootsAndWindMatchTheVertexFallbackAtBothPoles) {
    if (!GLEW_VERSION_4_3) return;
    for (float pole:{-1.f,1.f}) {
        GrassProbe probe(pole);
        probe.planet.foliage.quad_distance_m=35;
        probe.grass.prepare(0,probe.mesh,probe.planet,100,{0,0,pole*1.02});
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

TEST(ProceduralGrassCompute, CachedRootsStayImmutableAcrossWindViewsAndCameraMotionAndInvalidateWithInputs) {
    if(!GLEW_VERSION_4_3) GTEST_SKIP() << "GPU root cache needs GL 4.3";
    GrassProbe probe;
    probe.capture(0,true);
    const auto original=probe.grass.cachedRootsForValidation(0);
    const auto initial=probe.grass.stats(0);
    ASSERT_EQ(original.size(),initial.candidates);ASSERT_TRUE(initial.rootsPrepared);
    EXPECT_EQ(initial.rootBytes,initial.candidates*64+initial.patches*16);
    const auto equal=[](const auto& a,const auto& b) {
        ASSERT_EQ(a.size(),b.size());
        for(std::size_t i=0;i<a.size();++i) {
            EXPECT_EQ(a[i].rootExpected,b[i].rootExpected);EXPECT_EQ(a[i].upRank,b[i].upRank);
            EXPECT_EQ(a[i].normalSlot,b[i].normalSlot);EXPECT_EQ(a[i].variation,b[i].variation);
        }
    };
    probe.capture(3,true); // Wind evolves without rewriting root records.
    EXPECT_EQ(probe.grass.prepare(0,probe.mesh,probe.planet,100,{.05,0,1.02}).rebuilds,0);
    rendering::GrassPass pass;pass.mainEyeBody={.05,0,1.02};pass.windTime=7;
    pass.view=glm::scale(glm::mat4(1),glm::vec3(1,-1,1)); // A separate reflection view.
    probe.grass.shader.use();probe.grass.draw(0,&pass);
    equal(probe.grass.cachedRootsForValidation(0),original);
    EXPECT_EQ(probe.grass.stats(0).rootDispatches,initial.rootDispatches);
    ++probe.planet.foliage.seed;
    EXPECT_EQ(probe.grass.prepare(0,probe.mesh,probe.planet,100,{0,0,1.02}).rebuilds,1);
    EXPECT_FALSE(probe.grass.stats(0).rootsPrepared);probe.capture(0,true);
    const auto seeded=probe.grass.cachedRootsForValidation(0);
    ASSERT_EQ(seeded.size(),original.size());
    EXPECT_NE(seeded[0].rootExpected,original[0].rootExpected);
    const auto dispatches=probe.grass.stats(0).rootDispatches;
    for(std::size_t i=0;i<probe.mesh.vertices.size();i+=9) probe.mesh.vertices[i]+=.01f;
    probe.mesh.upload();++probe.mesh.revision;
    EXPECT_EQ(probe.grass.prepare(0,probe.mesh,probe.planet,100,{0,0,1.02}).rebuilds,1);
    probe.capture(0,true);EXPECT_GT(probe.grass.stats(0).rootDispatches,dispatches);
    EXPECT_NE(probe.grass.cachedRootsForValidation(0)[0].rootExpected,seeded[0].rootExpected);
    probe.grass.clear();EXPECT_TRUE(probe.grass.cachedRootsForValidation(0).empty());
    EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
}

TEST(ProceduralGrassCompute, WindAndTrailsAreEvaluatedOnceAcrossViewsAndInvalidateWithTheirInputs) {
    if(!GLEW_VERSION_4_3) GTEST_SKIP() << "Shared animation needs GL 4.3";
    GrassProbe probe;
    probe.capture(1,true);
    const auto counts=probe.grass.computedCounts(0);
    EXPECT_EQ(probe.grass.animationUpdatesForValidation(0),counts[0]+counts[1]);
    auto samples=probe.grass.cachedAnimationForValidation(0);
    const auto token=probe.grass.stats(0).animationFrame;
    EXPECT_EQ(probe.grass.stats(0).animationBytes,samples.size()*32);
    rendering::GrassPass reflection;reflection.view=glm::scale(glm::mat4(1),glm::vec3(1,-1,1));
    reflection.mainEyeBody={0,0,1.02};
    probe.capture(1,true,&reflection);
    EXPECT_EQ(probe.grass.animationUpdatesForValidation(0),0u);
    EXPECT_EQ(probe.grass.stats(0).animationFrame,token);
    const auto reused=probe.grass.cachedAnimationForValidation(0);
    ASSERT_EQ(samples.size(),reused.size());
    for(std::size_t i=0;i<samples.size();++i) {
        EXPECT_EQ(samples[i].wind,reused[i].wind);EXPECT_EQ(samples[i].trail,reused[i].trail);
        EXPECT_EQ(samples[i].frame,reused[i].frame);
    }
    probe.capture(2,true);EXPECT_GT(probe.grass.animationUpdatesForValidation(0),0u);
    EXPECT_NE(probe.grass.stats(0).animationFrame,token);
    samples=probe.grass.cachedAnimationForValidation(0);
    const auto roots=probe.grass.cachedRootsForValidation(0);
    const auto chosen=std::find_if(samples.begin(),samples.end(),[&](const auto& s) {
        return s.frame==probe.grass.stats(0).animationFrame;
    });
    ASSERT_NE(chosen,samples.end());const auto index=chosen-samples.begin();
    const glm::dvec3 root(roots[index].rootExpected);const auto up=glm::normalize(root);
    const auto along=glm::normalize(glm::cross(up,glm::dvec3(0,1,0)));
    probe.grass.trail(0).observe(root*100.0-along*.3,true);
    probe.grass.trail(0).observe(root*100.0+along*.3,true);
    probe.capture(2,true);EXPECT_GT(probe.grass.animationUpdatesForValidation(0),0u);
    const auto pressed=probe.grass.cachedAnimationForValidation(0);
    EXPECT_EQ(pressed[index].wind,samples[index].wind);EXPECT_NEAR(pressed[index].trail.w,1.f,1e-6);
    probe.capture(2,true);EXPECT_EQ(probe.grass.animationUpdatesForValidation(0),0u);
    // A root replacement at unchanged wind/history must also invalidate samples.
    ++probe.planet.foliage.seed;
    probe.grass.prepare(0,probe.mesh,probe.planet,100,{0,0,1.02});probe.capture(2,true);
    EXPECT_GT(probe.grass.animationUpdatesForValidation(0),0u);
    probe.planet.foliage.compute_placement=false;
    probe.grass.prepare(0,probe.mesh,probe.planet,100,{0,0,1.02});
    EXPECT_EQ(probe.grass.stats(0).animationBytes,0u);
}

TEST(ProceduralGrassCompute, ReflectionOnlyBladesReceiveAnimationAfterAnEmptyMainView) {
    if(!GLEW_VERSION_4_3) GTEST_SKIP() << "Shared animation needs GL 4.3";
    GrassProbe probe;probe.planet.foliage.frustum_culling=true;
    probe.grass.prepare(0,probe.mesh,probe.planet,100,{0,0,1.02});
    rendering::GrassPass invisible;invisible.mainEyeBody={0,0,1.02};
    invisible.view=glm::translate(glm::mat4(1),glm::vec3(10,0,0));
    EXPECT_TRUE(probe.capture(3,true,&invisible).empty());
    EXPECT_EQ(probe.grass.animationUpdatesForValidation(0),0u);
    const auto token=probe.grass.stats(0).animationFrame;
    EXPECT_FALSE(probe.capture(3,true).empty());
    EXPECT_EQ(probe.grass.stats(0).animationFrame,token);
    const auto counts=probe.grass.computedCounts(0);
    EXPECT_GT(counts[0]+counts[1],0u);
    EXPECT_EQ(probe.grass.animationUpdatesForValidation(0),counts[0]+counts[1]);
    probe.capture(3,true);EXPECT_EQ(probe.grass.animationUpdatesForValidation(0),0u);
}

TEST(ProceduralGrassRender, GrowingCandidateBatchesRetainsExistingBlades) {
    GrassProbe probe;
    probe.planet.foliage.max_blades=100000;
    probe.planet.foliage.density_per_m2=.2;
    ++probe.mesh.revision;
    probe.grass.prepare(0,probe.mesh,probe.planet,100,{0,0,1.02});
    auto before=probe.capture();
    auto candidates=probe.grass.stats(0).candidates;
    bool crossed=false;
    for (int step=1;step<=100;++step) {
        probe.planet.foliage.density_per_m2=.2+step*.002;
        ++probe.mesh.revision;
        probe.grass.prepare(0,probe.mesh,probe.planet,100,{0,0,1.02});
        const auto next=probe.grass.stats(0).candidates;
        auto after=probe.capture();
        if (next>candidates) {
            crossed=true;
            unsigned checked=0;
            for (std::size_t i=0;i<before.size();i+=60) {
                if (before[i+9]<=0) continue;
                bool retained=false;
                for (std::size_t j=0;j<after.size();j+=60) {
                    float delta=0;
                    for (int c=0;c<3;++c) delta=std::max(delta,std::abs(before[i+c]-after[j+c]));
                    if (delta<1e-6f && after[j+9]+1e-5f>=before[i+9]) { retained=true; break; }
                }
                EXPECT_TRUE(retained) << "Batch growth replaced a previously visible root";
                ++checked;
            }
            EXPECT_GT(checked,0u);
            break;
        }
        before=std::move(after); candidates=next;
    }
    EXPECT_TRUE(crossed);
}

TEST(ProceduralGrassRender, ConfigurableQuadDistanceChangesGeometryWithoutReplacingRoots) {
    GrassProbe probe;
    using Coverage=std::map<std::array<float,3>,float>;
    const auto roots=[](const std::vector<float>& vertices) {
        Coverage result;
        for (std::size_t i=0;i<vertices.size();i+=10)
            if (vertices[i+9]>0) result[{vertices[i],vertices[i+1],vertices[i+2]}]=vertices[i+9];
        return result;
    };
    Coverage reference;
    for (double distance:{1.,100.}) {
        probe.planet.foliage.quad_distance_m=distance;
        const auto prepared=probe.grass.prepare(0,probe.mesh,probe.planet,100,{0,0,1.02});
        EXPECT_EQ(prepared.rebuilds,0);
        const auto stats=probe.grass.stats(0);
        EXPECT_EQ(stats.vertices,stats.candidates*(distance==1 ? 4 : 14));
        const auto fallback=roots(probe.capture(3));
        ASSERT_FALSE(fallback.empty());
        if (reference.empty()) reference=fallback;
        else EXPECT_EQ(fallback,reference);
        if (GLEW_VERSION_4_3) {
            const auto computed=roots(probe.capture(3,true));
            EXPECT_EQ(computed,fallback);
            const auto counts=probe.grass.computedCounts(0);
            EXPECT_EQ(counts[distance==1 ? 0 : 1],0u);
            EXPECT_EQ(counts[distance==1 ? 1 : 0],fallback.size());
        }
    }
}

TEST(ProceduralGrassRender, OfflineRadiusGeneratesVisibleRootsBeyondTheNormalCutoff) {
    GrassProbe probe;
    probe.planet.foliage.draw_distance_m=30;
    probe.planet.foliage.max_blades=128;
    config::ScenarioConfig scene; scene.planets={probe.planet};
    app::CommandLineOptions options; options.offlineQuality=options.renderTestMode=true;
    const auto offline=rendering::offlineScenario(scene,options);
    const auto farRoots=[&](const config::PlanetConfig& planet,bool compute) {
        probe.planet=planet;
        probe.grass.clear();
        probe.grass.prepare(0,probe.mesh,planet,100,{0,0,1.02});
        probe.grass.shader.use();
        probe.grass.shader.setFloat("uDrawDistance",planet.foliage.draw_distance_m);
        probe.grass.shader.setFloat("uGaussianSigma",planet.foliage.draw_distance_m/3);
        const auto vertices=probe.capture(0,compute);
        std::size_t found=0;
        for (std::size_t i=0;i<vertices.size();i+=10) {
            const glm::dvec3 root(vertices[i],vertices[i+1],vertices[i+2]);
            if (vertices[i+9]>0 && glm::length(root-glm::dvec3(0,0,1.02))*100>31) ++found;
        }
        return found;
    };
    for (bool compute:{false,true}) {
        if (compute && !GLEW_VERSION_4_3) continue;
        EXPECT_EQ(farRoots(scene.planets[0],compute),0u);
        EXPECT_GT(farRoots(offline.planets[0],compute),0u);
    }
}

TEST(ProceduralGrassRender, PersistentTrailsFlattenTipsKeepRootsAndSuppressWindOnBothPaths) {
    for (float pole:{-1.f,1.f}) for (double quadDistance:{1.,100.}) {
        GrassProbe probe(pole);
        probe.planet.foliage.quad_distance_m=quadDistance;
        probe.grass.prepare(0,probe.mesh,probe.planet,100,{0,0,pole*1.02});
        const auto original=probe.capture(0);
        glm::dvec3 chosen(0); bool found=false;
        for (std::size_t i=0;i<original.size();i+=10) if (original[i+9]>.5) {
            chosen={original[i],original[i+1],original[i+2]}; found=true; break;
        }
        ASSERT_TRUE(found);
        const auto up=glm::normalize(chosen);
        const auto along=glm::normalize(glm::cross(up,glm::dvec3(0,1,0)));
        auto& trail=probe.grass.trail(0);
        trail.observe(chosen*100.0-along*.3,true);
        trail.observe(chosen*100.0+along*.3,true);
        unsigned checked=0,unchanged=0;
        for (bool compute:{false,true}) {
            if (compute && !GLEW_VERSION_4_3) continue;
            const auto pressed=probe.capture(0,compute),windy=probe.capture(3,compute);
            const auto centre=[&](const std::vector<float>& data) {
                std::vector<std::array<float,3>> positions;
                for (std::size_t i=0;i<data.size();i+=10) {
                    const glm::dvec3 root(data[i],data[i+1],data[i+2]);
                    if (glm::length(root-chosen)<1e-6 && data[i+9]>0) {
                        const glm::dvec3 body(data[i+3],data[i+4],data[i+5]);
                        EXPECT_LT(glm::dot(body-root,up)*100,.14);
                        positions.push_back({data[i+3],data[i+4],data[i+5]});
                    }
                }
                std::sort(positions.begin(),positions.end()); return positions;
            };
            const auto a=centre(pressed),b=centre(windy);
            ASSERT_FALSE(a.empty()); EXPECT_EQ(a,b); checked+=a.size();
            if (!compute) {
                ASSERT_EQ(original.size(),pressed.size());
                for (std::size_t i=0;i<original.size();i+=10) {
                    for (int c=0;c<3;++c) EXPECT_EQ(original[i+c],pressed[i+c]);
                    EXPECT_EQ(original[i+9],pressed[i+9]);
                    const glm::dvec3 root(original[i],original[i+1],original[i+2]);
                    if (glm::length(root-chosen)*100>1 && original[i+9]>0) {
                        for (int c=3;c<6;++c) EXPECT_EQ(original[i+c],pressed[i+c]);
                        ++unchanged;
                    }
                }
            }
        }
        EXPECT_GT(checked,0); EXPECT_GT(unchanged,0);
        const auto local=probe.capture(0);
        const auto reflection=glm::rotate(glm::translate(glm::mat4(1),{12,-8,3}),.7f,glm::vec3(0,1,0));
        probe.grass.shader.setMat4("model",glm::value_ptr(reflection));
        const auto transformed=probe.capture(0);
        for (std::size_t i=0;i<local.size();i+=10) {
            for (int c=0;c<6;++c) EXPECT_EQ(local[i+c],transformed[i+c]);
            if (local[i+9]<=0) continue;
            const auto expected=reflection*glm::vec4(local[i+3],local[i+4],local[i+5],1);
            for (int c=0;c<3;++c) EXPECT_NEAR(transformed[i+6+c],expected[c],.00001f);
        }
        // The same saved marks survive CPU plan/terrain revision changes.
        const auto revision=trail.revision(); ++probe.mesh.revision;
        probe.grass.prepare(0,probe.mesh,probe.planet,100,{0,0,pole*1.02});
        EXPECT_EQ(trail.revision(),revision); EXPECT_EQ(probe.capture(0),transformed);
    }
}

TEST(ProceduralGrassRender, PressedTipsFollowRisingGroundInsteadOfDisappearingBelowIt) {
    GrassProbe probe;
    const glm::dvec3 normal=glm::normalize(glm::dvec3(0,-.5,1));
    for (std::size_t i=0;i<probe.mesh.vertices.size();i+=9) {
        probe.mesh.vertices[i+2]+=probe.mesh.vertices[i+1]*.5f;
        for (int c=0;c<3;++c) probe.mesh.vertices[i+3+c]=normal[c];
    }
    probe.mesh.upload(); ++probe.mesh.revision;
    probe.grass.prepare(0,probe.mesh,probe.planet,100,{0,0,1.02});
    const auto original=probe.capture();
    glm::dvec3 chosen(0); bool found=false;
    for (std::size_t i=0;i<original.size();i+=10) if (original[i+9]>.5) {
        chosen={original[i],original[i+1],original[i+2]}; found=true; break;
    }
    ASSERT_TRUE(found);
    const auto along=glm::normalize(glm::dvec3(0,1,.5));
    probe.grass.trail(0).observe(chosen*100.0-along*.3,true);
    probe.grass.trail(0).observe(chosen*100.0+along*.3,true);
    for (bool compute:{false,true}) {
        if (compute && !GLEW_VERSION_4_3) continue;
        const auto data=probe.capture(3,compute);
        unsigned tips=0;
        for (std::size_t i=0;i<data.size();i+=10) {
            const glm::dvec3 root(data[i],data[i+1],data[i+2]);
            if (glm::length(root-chosen)>1e-6 || data[i+9]<=0) continue;
            const glm::dvec3 offset=(glm::dvec3(data[i+3],data[i+4],data[i+5])-root)*100.0;
            if (glm::dot(offset,along)<.6) continue;
            EXPECT_GT(glm::dot(offset,normal),.01); // Clear of the actual slope.
            EXPECT_LT(glm::dot(offset,normal),.14); // Still pressed against it.
            ++tips;
        }
        EXPECT_GT(tips,0);
    }
}

TEST(ProceduralGrassRender, ResidentProtectedRegionKeepsConfiguredCoverageWithoutLodOrOuterFade) {
    if(!GLEW_VERSION_4_3) GTEST_SKIP()<<"Resident foliage requires OpenGL 4.3";
    GrassProbe probe;
    probe.planet.foliage.density_per_m2=4;
    probe.planet.foliage.quad_distance_m=probe.planet.foliage.draw_distance_m;
    probe.planet.foliage.frustum_culling=false;
    probe.mesh.terrainStats.generation={123,456,1,1,rendering::TerrainBackend::Compute};
    probe.mesh.terrainStats.gpuCorners=probe.mesh.indices.size();
    probe.grass.enableAdaptiveBudget();
    auto staged=probe.grass.submitResident(probe.mesh.vbo,probe.mesh.ebo,probe.mesh.terrainStats,
        probe.planet,100,{0,0,1.02},probe.mesh.revision);
    probe.grass.waitForCapture(*staged);probe.grass.commit(0,*staged,probe.mesh);
    EXPECT_FALSE(probe.grass.policy(0).at("near_infeasible").get<bool>());
    EXPECT_EQ(probe.grass.stats(0).density,4.);
    const auto values=probe.capture(0,true);const auto counts=probe.grass.computedCounts(0);
    EXPECT_EQ(counts[1],0u);EXPECT_NEAR(double(counts[0]),2100.,2.);
    double coverage=0;std::size_t fractional=0;
    // All roots are protected; each detailed blade produces 12 triangles.
    for(std::size_t i=0;i<values.size();i+=12*3*10) {
        const auto fade=values[i+9];coverage+=fade;fractional+=fade<1;
        EXPECT_GT(fade,0.f);EXPECT_LE(fade,1.f);
        EXPECT_LT(glm::length(glm::dvec3(values[i],values[i+1],values[i+2])-glm::dvec3(0,0,1.02))*100,100);
    }
    EXPECT_LE(fractional,2u);EXPECT_NEAR(coverage,2100.,.01);
    EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
}

TEST(ProceduralGrassRender, CpuProtectedCoverageSurvivesVertexFallbackAndLockedReplay) {
    GrassProbe probe;
    probe.planet.foliage.density_per_m2=4;
    probe.planet.foliage.quad_distance_m=probe.planet.foliage.draw_distance_m;
    probe.planet.foliage.frustum_culling=false;
    probe.grass.enableAdaptiveBudget();
    ++probe.mesh.revision;
    probe.grass.prepare(0,probe.mesh,probe.planet,100,{0,0,1.02});
    const auto policy=probe.grass.policy(0);
    EXPECT_FALSE(policy.at("near_infeasible").get<bool>());
    EXPECT_EQ(probe.grass.stats(0).density,4.);
    const auto vertex=probe.capture();
    double coverage=0;
    for(std::size_t i=0;i<vertex.size();i+=12*3*10) coverage+=vertex[i+9];
    EXPECT_NEAR(coverage,2100.,.01);
    if(GLEW_VERSION_4_3) {
        const auto computed=probe.capture(0,true);
        double computedCoverage=0;
        for(std::size_t i=0;i<computed.size();i+=12*3*10) computedCoverage+=computed[i+9];
        EXPECT_NEAR(computedCoverage,coverage,.01);
    }
    auto saved=policy;saved["body"]=probe.planet.name;
    probe.grass.restorePolicies({{"render",{{"foliage_policy",nlohmann::json::array({saved})}}}}, {probe.planet});
    ++probe.mesh.revision;
    probe.grass.prepare(0,probe.mesh,probe.planet,100,{0,0,1.02});
    EXPECT_EQ(probe.grass.policy(0),policy);
    EXPECT_EQ(probe.capture(),vertex);
    EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
}

TEST(ProceduralGrassRender, CpuAdmissionChargesLiveTerrainAndReplacementBeforeLockedReplay) {
    GrassProbe probe;
    probe.grass.enableAdaptiveBudget();
    probe.grass.terrainBudgetBytes(1024);
    probe.grass.externalBudgetBytes(2048);
    ++probe.mesh.revision;
    probe.grass.prepare(0,probe.mesh,probe.planet,100,{0,0,1.02});
    EXPECT_EQ(probe.grass.reservedBytes(),3072+probe.grass.ownedBytes());
    const bool cached=GLEW_VERSION_4_3 && probe.planet.foliage.compute_placement;
    EXPECT_EQ(probe.grass.ownedBytes(),4096*(cached?224:128)+2*(cached?20:4)+(cached?rendering::grassCellBytes(2):0)+rendering::grassComputedCommandBytes);
    const auto before=probe.grass.policy(0);
    auto saved=before;saved["body"]=probe.planet.name;
    probe.grass.restorePolicies({{"render",{{"foliage_policy",nlohmann::json::array({saved})}}}}, {probe.planet});
    auto signals=probe.grass.budgetSignals();
    signals.capBytes=probe.grass.reservedBytes()+2*4+32+128;
    probe.grass.observeBudget(signals);
    EXPECT_NO_THROW(probe.grass.admitCpuTerrain(2*4+32+128));
    EXPECT_THROW(probe.grass.admitCpuTerrain(2*4+32+129),std::runtime_error);
    ++probe.mesh.revision;
    EXPECT_THROW(probe.grass.prepare(0,probe.mesh,probe.planet,100,{0,0,1.02}),std::runtime_error);
    EXPECT_EQ(probe.grass.policy(0),before);
    EXPECT_EQ(probe.grass.reservedBytes(),3072+probe.grass.ownedBytes());
}

TEST(TerrainMesh, CpuRetainedUploadPreservesIntegerSourceAndBakedPoseThroughSwapAndFallback) {
    using namespace rendering;
    config::PlanetConfig::TerrainLod lod;
    lod.base_edge_segments=1;lod.medium_edge_segments=4;lod.max_edge_segments=lod.steep_edge_segments=8;
    lod.max_triangle_budget=100000;lod.shoreline_edge_m=0;
    lod.near_surface_distance_m=300;lod.mid_surface_distance_m=1000;
    lod.relief_sinking=lod.local_detail_streaming=true;lod.geometric_error_m=.05;
    lod.local_detail_radius_m=1.5;lod.local_edge_m=.05;lod.local_error_m=.01;lod.local_transition_m=.5;
    config::PlanetConfig::SurfaceNoiseFunction noise;
    noise.amplitude_m=.01;noise.wavelength_m=.25;noise.octaves=1;noise.seed=-71;
    const TerrainSurface surface({noise},lod,1,1000);
    const auto radial=glm::normalize(glm::dvec3(.3,-.7,1));
    auto topology=surface.buildTopologyForEye(radial*1.002,{});
    SceneMeshes meshes(2);auto& mesh=meshes.planetMeshes[0];
    for(const bool explicitPhase:{false,true}) {
        SCOPED_TRACE(explicitPhase);
        const TerrainMorphFootprint phase{radial,1000,.2,.4};
        auto geometry=explicitPhase?surface.evaluateTopology(topology,phase,TerrainMorphStorage::Retained):
            surface.evaluateTopology(topology,TerrainMorphStorage::Retained);
        auto source=geometry.morphData;ASSERT_TRUE(source);
        std::weak_ptr<const TerrainMorphData> lifetime=source;
        const auto expected=geometry.vertices;const auto revision=mesh.revision;
        mesh.loadTerrain(geometry);EXPECT_EQ(mesh.revision,revision+1);EXPECT_EQ(mesh.morphData,source);
        EXPECT_FALSE(mesh.residentTerrain);EXPECT_FALSE(mesh.morphState);EXPECT_EQ(mesh.deformationRevision(),0u);
        EXPECT_EQ(mesh.vertices,expected);EXPECT_TRUE(mesh.morphSource());
        EXPECT_EQ(mesh.morphSource().generation,source->generation);
        EXPECT_EQ(mesh.cpuMorphBytes(),(source->targets.capacity()+source->corners.capacity())*4);
        EXPECT_EQ(mesh.gpuMorphBytes(),source->bytes());
        const auto storage=geometry.vertices.size()*4+geometry.indices.size()*4+geometry.grassCellKeys.size()*16+source->bytes();
        EXPECT_EQ(terrainStorageBytes(meshes.planetMeshes),storage);
        const auto read=[&](GLuint buffer,const auto& input) {
            using Values=std::decay_t<decltype(input)>;Values actual(input.size());
            GLint previous=0;glGetIntegerv(GL_COPY_READ_BUFFER_BINDING,&previous);
            glBindBuffer(GL_COPY_READ_BUFFER,buffer);glGetBufferSubData(GL_COPY_READ_BUFFER,0,actual.size()*sizeof(actual[0]),actual.data());
            glBindBuffer(GL_COPY_READ_BUFFER,previous);EXPECT_EQ(actual,input);
        };
        read(mesh.morphTargets,source->targets);read(mesh.morphCorners,source->corners);read(mesh.vbo,expected);
        const auto targetBuffer=mesh.morphTargets,cornerBuffer=mesh.morphCorners;
        const auto liveRevision=mesh.revision;const auto liveVbo=mesh.vbo;
        const auto rejected=[&](auto corrupt) {
            auto bad=geometry;auto data=std::make_shared<TerrainMorphData>(*source);bad.morphData=data;
            corrupt(bad,*data);EXPECT_THROW(mesh.loadTerrain(std::move(bad)),std::invalid_argument);
            EXPECT_EQ(mesh.revision,liveRevision);EXPECT_EQ(mesh.vbo,liveVbo);EXPECT_EQ(mesh.morphTargets,targetBuffer);
        };
        rejected([](auto&,auto& d){d.targets.pop_back();});
        rejected([](auto&,auto& d){d.corners[0]=d.samples();});
        rejected([](auto& g,auto&){g.indices[0]=1;});
        rejected([](auto& g,auto&){g.grassCellKeys.pop_back();});
        rejected([](auto&,auto& d){++d.generation.topology;});
        rejected([](auto& g,auto&){g.vertices[0]+=.01f;});
        rejected([](auto&,auto& d){d.metersPerRadius=0;});
        rejected([&](auto&,auto& d){d.phase=phase;d.phase->metersPerRadius=999;});
        mesh.swap(meshes.planetMeshes[1]);EXPECT_FALSE(mesh.morphData);EXPECT_FALSE(mesh.morphSource());
        auto& exchanged=meshes.planetMeshes[1];EXPECT_EQ(exchanged.morphData,source);
        EXPECT_EQ(exchanged.morphTargets,targetBuffer);EXPECT_EQ(exchanged.morphCorners,cornerBuffer);
        EXPECT_EQ(terrainStorageBytes(meshes.planetMeshes),storage);
        exchanged.loadTerrain(surface.evaluateTopology(topology));
        EXPECT_FALSE(exchanged.morphData);EXPECT_FALSE(exchanged.morphSource());
        EXPECT_EQ(exchanged.cpuMorphBytes(),0u);EXPECT_EQ(exchanged.gpuMorphBytes(),0u);
        EXPECT_FALSE(glIsBuffer(targetBuffer));EXPECT_FALSE(glIsBuffer(cornerBuffer));
        geometry={};EXPECT_FALSE(lifetime.expired()); // Local owner still holds this source.
        source.reset();EXPECT_TRUE(lifetime.expired());
        exchanged.destroy();mesh.destroy();
        exchanged=Mesh{};mesh=Mesh{};
        EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
    }
}

TEST(ProceduralGrassRender, RetainedCpuSourceKeepsRootsOnBakedPlanesAcrossViewsAndPlacementPaths) {
    using namespace rendering;
    std::size_t checked=0,moved=0,recordComparisons=0;double maximumPlaneError=0;
    for(const auto radial:{glm::dvec3(0,0,1),glm::normalize(glm::dvec3(.7,-.2,1))}) {
        glUseProgram(0);GrassProbe probe;
        struct ProgramScope {~ProgramScope(){glUseProgram(0);}} programScope;
        auto& p=probe.planet;p.radius=1;p.color={1,1,1};p.surface_noise.clear();
        p.terrain_landscape.enabled=false;p.water.enabled=true;p.water.level_m=0;
        auto& f=p.foliage;f.enabled=f.compute_placement=true;f.frustum_culling=false;
        f.draw_distance_m=100;f.quad_distance_m=50;f.density_per_m2=2;
        f.max_blades=f.max_candidates_per_triangle=4096;f.root_offset_m=f.water_clearance_m=0;
        probe.grass.enableAdaptiveBudget();const auto eye=radial*1.002;
        auto& shader=probe.grass.shader;shader.use();
        shader.setFloat("uMetersPerRadius",1000);shader.setFloat("uDrawDistance",100);
        shader.setFloat3("uGrassEyeBody",eye.x,eye.y,eye.z);shader.setFloat3("uFacingEyeBody",eye.x,eye.y,eye.z);
        shader.setFloat3("uPlanetColor",1,1,1);shader.setInt("uWaterEnabled",1);shader.setInt("uLandscapeEnabled",0);
        shader.setFloat3("uLandscapeLevels",0,.1,0);shader.setFloat("uWaterClearance",0);
        shader.setFloat("uGreenRatio",f.green_ratio);
        shader.setFloat2("uHeightMultiplierRange",f.height_multiplier_min,f.height_multiplier_max);
        shader.setFloat2("uLeanRange",f.lean_min,f.lean_max);
        const auto east=glm::normalize(glm::cross(radial,glm::dvec3(0,1,0)));
        const glm::dmat3 rotation(east,glm::cross(radial,east),radial);
        TerrainMorphData base;base.generation={123,456,PlanetField::version,4,TerrainBackend::Cpu};
        base.metersPerRadius=1000;base.targets.resize(54);base.corners={2,0,1};
        const std::array<glm::dvec3,3> triangle{{{-.01,-.01,1.002},{.01,-.01,1.002},{0,.01,1.002}}};
        std::array<std::array<double,3>,3> directions;
        for(int i=0;i<3;++i) {
            const glm::dvec3 fine(glm::vec3(rotation*triangle[i])),parent=fine*(.998/1.002);
            const auto r=glm::normalize(fine);const auto sample=base.corners[i];
            for(int j=0;j<3;++j) {
                directions[i][j]=r[j];const auto rb=std::bit_cast<std::uint64_t>(r[j]),pb=std::bit_cast<std::uint64_t>(parent[j]);
                base.targets[9*sample+2*j]=unsigned(rb);base.targets[9*sample+2*j+1]=unsigned(rb>>32);
                base.targets[9*sample+6+j]=std::bit_cast<unsigned>(float(fine[j]));
                base.targets[27+9*sample+2*j]=unsigned(pb);base.targets[27+9*sample+2*j+1]=unsigned(pb>>32);
                base.targets[27+9*sample+6+j]=std::bit_cast<unsigned>(float(parent[j]));
            }
        }
        const auto geometry=[&](std::optional<TerrainMorphFootprint> phase) {
            TerrainGeometry g;g.generation=base.generation;g.uniqueSamples=3;g.indices={0,1,2};
            g.grassCellKeys={grassCellIdentity(directions)};
            auto data=std::make_shared<TerrainMorphData>(base);data->phase=phase;g.morphData=data;
            for(auto sample:base.corners) {
                auto position=base.original(sample);
                if(phase) {
                    const auto fine=glm::dvec3(base.fine(sample));const auto weight=terrainMorphWeight(base.radial(sample),*phase);
                    position=weight==0?glm::vec3(fine):glm::vec3(fine+(base.parent(sample)-fine)*weight);
                }
                for(const auto v:{position,glm::vec3(radial),glm::vec3(.2,.8,.2)})
                    for(int j=0;j<3;++j) g.vertices.push_back(v[j]);
            }
            return g;
        };
        using Roots=std::set<std::array<float,3>>;
        const auto roots=[&](const std::vector<float>& capture) {
            EXPECT_EQ(capture.size()%360,0u);Roots result;
            for(std::size_t first=0;first<capture.size();first+=360) if(capture[first+9]>0) {
                const std::array<float,3> root{capture[first],capture[first+1],capture[first+2]};
                EXPECT_TRUE(result.insert(root).second);
                for(int vertex=1;vertex<36;++vertex) for(int j=0;j<3;++j) EXPECT_EQ(capture[first+10*vertex+j],root[j]);
            }
            return result;
        };
        GrassPass pass;pass.mainEyeBody=eye;pass.windTime=.75;
        probe.mesh.loadTerrain(geometry({}));
        EXPECT_EQ(probe.grass.prepare(0,probe.mesh,p,1000,eye).rebuilds,1u);
        const auto candidates=probe.grass.stats(0).candidates;ASSERT_GT(candidates,0u);
        EXPECT_TRUE(roots(probe.capture(.75,false,&pass)).empty());
        std::vector<GrassRootRecord> cached;Roots originalFine,last;TerrainGeometry latest;
        for(const auto band:{glm::dvec2(1000,1),glm::dvec2(9,15),glm::dvec2(5,15),glm::dvec2(0,.1),glm::dvec2(1000,1)}) {
            latest=geometry(TerrainMorphFootprint{radial,1000,band.x,band.y});probe.mesh.loadTerrain(latest);
            EXPECT_EQ(probe.grass.prepare(0,probe.mesh,p,1000,eye).rebuilds,1u);
            EXPECT_EQ(probe.grass.stats(0).candidates,candidates);
            const auto visible=roots(probe.capture(.75,false,&pass));
            if(band.x==1000) {ASSERT_FALSE(visible.empty());if(originalFine.empty()) originalFine=visible;else EXPECT_EQ(visible,originalFine);}
            if(band.x==0) EXPECT_TRUE(visible.empty());
            const auto& v=probe.mesh.vertices;const glm::dvec3 a(v[0],v[1],v[2]),b(v[9],v[10],v[11]),c(v[18],v[19],v[20]);
            const auto normal=glm::normalize(glm::cross(b-a,c-a));const auto plane=glm::dot(normal,a);
            for(const auto& root:visible) {
                const double error=std::abs(glm::dot(normal,glm::dvec3(root[0],root[1],root[2]))-plane)*1000;
                maximumPlaneError=std::max(maximumPlaneError,error);EXPECT_LE(error,.001);++checked;
            }
            moved+=!last.empty() && visible!=last;last=visible;
            if(GLEW_VERSION_4_3) {
                const auto before=probe.grass.stats(0).animationFrame;
                EXPECT_EQ(roots(probe.capture(.75,true,&pass)),visible);EXPECT_TRUE(probe.grass.usesCompute(0));
                EXPECT_EQ(probe.grass.computedCounts(0)[1],0u);EXPECT_EQ(probe.grass.stats(0).animationFrame,before+1);
                const auto records=probe.grass.cachedRootsForValidation(0);ASSERT_EQ(records.size(),candidates);
                if(cached.empty()) cached=records;
                for(std::size_t i=0;i<cached.size();++i) {
                    EXPECT_EQ(records[i].rootExpected,cached[i].rootExpected);EXPECT_EQ(records[i].upRank,cached[i].upRank);
                    EXPECT_EQ(records[i].normalSlot,cached[i].normalSlot);EXPECT_EQ(records[i].variation,cached[i].variation);
                    ++recordComparisons;
                }
                auto reflection=pass;reflection.view[0][0]=-1;
                const auto frame=probe.grass.stats(0).animationFrame;
                EXPECT_EQ(roots(probe.capture(.75,true,&reflection)),visible);
                EXPECT_EQ(probe.grass.stats(0).animationFrame,frame);
            }
            EXPECT_EQ(probe.grass.prepare(0,probe.mesh,p,1000,eye).rebuilds,0u);
        }
        f.compute_placement=false;probe.grass.prepare(0,probe.mesh,p,1000,eye);
        EXPECT_EQ(roots(probe.capture(.75,true,&pass)),originalFine);EXPECT_FALSE(probe.grass.usesCompute(0));
        latest.morphData.reset();probe.mesh.loadTerrain(latest);probe.grass.prepare(0,probe.mesh,p,1000,eye);
        EXPECT_FALSE(probe.mesh.morphSource());EXPECT_FALSE(roots(probe.capture(.75,false,&pass)).empty());
        f.enabled=false;probe.grass.prepare(0,probe.mesh,p,1000,eye);probe.grass.draw(0,&pass);
        EXPECT_FALSE(probe.grass.drawnDeformationRevision(0));EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
    }
    EXPECT_GT(checked,0u);EXPECT_GT(moved,0u);
    std::cout << "CPU_GRASS_SOURCE visible_queries=" << checked << " changed_views=" << moved
        << " cached_record_comparisons=" << recordComparisons << " maximum_plane_error_m=" << maximumPlaneError
        << " compute_available=" << bool(GLEW_VERSION_4_3) << '\n';
}

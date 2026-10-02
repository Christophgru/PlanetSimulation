#include "rendering/foliage/procedural/ProceduralGrass.h"
#include "rendering/foliage/GrassWind.h"
#include "rendering/geometry/Mesh.h"
#include "config/ScenarioConfig.h"
#include "rendering/quality/OfflineQuality.h"
#include <gtest/gtest.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <fstream>
#include <iterator>
#include <map>
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
        std::ifstream file("shaders/foliage/grass.vert");
        const std::string source{std::istreambuf_iterator<char>(file), {}};
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

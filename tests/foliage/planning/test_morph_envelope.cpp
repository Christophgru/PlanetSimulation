#include "rendering/foliage/planning/GrassAllocation.h"
#include "config/Config.h"
#include <gtest/gtest.h>
#include <bit>
#include <iostream>
#include <numeric>
#include <random>

using namespace rendering;

TEST(GrassMetadata, MorphEnvelopeBoundsIndependentRoundedCornerMotionAndReservesEnoughSlots) {
    config::ScenarioConfig scene(config::Config::load("configs/scenarios/solar_system.json"));
    auto p=scene.planets[0];p.radius=1;p.surface_noise.clear();p.terrain_landscape.enabled=false;
    p.water.enabled=true;p.water.level_m=.5;p.color={1,1,1};
    auto& f=p.foliage;f.enabled=true;f.draw_distance_m=50;f.quad_distance_m=40;
    f.density_per_m2=4;f.max_blades=100000;f.max_candidates_per_triangle=65536;
    GrassFalloff policy;policy.enabled=true;policy.protectedMeters=40;policy.budget=f.max_blades;
    policy.sigmaMeters=f.draw_distance_m*f.gaussian_sigma_fraction;
    constexpr unsigned triangles=16,corners=3*triangles;
    TerrainBuildStats stats;stats.generation={123,456,PlanetField::version,4,TerrainBackend::Compute};
    stats.gpuCorners=stats.uniqueSamples=corners;
    struct Owner {
        std::array<GLuint,4> buffers{};
        Owner() {glGenBuffers(buffers.size(),buffers.data());}
        ~Owner() {glDeleteBuffers(buffers.size(),buffers.data());}
    } owner;
    const auto [vbo,ebo,targets,map]=owner.buffers;
    const TerrainMorphSource source{targets,map,corners,stats.generation};
    const auto upload=[](GLuint buffer,const auto& values) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER,buffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER,values.size()*sizeof(values[0]),values.data(),GL_STATIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER,0);
    };
    std::vector<unsigned> indices(corners),mapping(corners);std::iota(indices.begin(),indices.end(),0);
    std::iota(mapping.rbegin(),mapping.rend(),0);upload(ebo,indices);upload(map,mapping);
    GrassMetadataCompute compute;GrassAllocationCompute allocation;
    EXPECT_THROW(compute.generate(vbo,ebo,stats,p,1000,{0,0,1.002},{},{},{},GrassMetadataMode::MorphEnvelope),std::invalid_argument);
    EXPECT_THROW(compute.generate(vbo,ebo,stats,p,1000,{0,0,1.002},{},{},source,GrassMetadataMode(99)),std::invalid_argument);
    std::mt19937 random(39475);
    std::uint64_t checked=0;unsigned mixedLarger=0,originalRejected=0;
    for(const auto radial:{glm::dvec3(0,0,1),glm::dvec3(0,0,-1),glm::dvec3(1,0,0),glm::normalize(glm::dvec3(.7,-.2,1))}) {
        const auto tangent=glm::normalize(glm::cross(radial,glm::dvec3(0,1,0)));
        const glm::dmat3 rotation(tangent,glm::cross(radial,tangent),radial);
        std::array<std::array<glm::dvec3,2>,corners> endpoints;
        std::vector<float> vertices(9*corners);std::vector<unsigned> words(18*corners);
        for(unsigned t=0;t<triangles;++t) {
            const glm::dvec3 offset((int(t%4)-2)*.003,(int(t/4)-2)*.003,1);
            for(unsigned c=0;c<3;++c) {
                const double x=double(int(c)-1)*.001;
                glm::dvec3 fine(x,0,0),parent(0,x,0);
                if(t%4==1) {fine={x,c==1?.002:0,0};parent={x*.1,0,-.002};}
                if(t%4==2) {fine={0,0,0};parent={x,c==1?.002:0,.002};}
                if(t%4==3) {fine={x*.001,c==1?.000002:0,0};parent={x*.002,c==1?-.000003:0,.00000013};}
                const unsigned corner=3*t+c,sample=mapping[corner];
                fine=glm::dvec3(glm::vec3(rotation*(offset+fine)));parent=rotation*(offset+parent);
                endpoints[corner]={fine,parent};
                const glm::vec3 original(fine); // Includes collapsed and dry/wet source geometry.
                for(unsigned axis=0;axis<3;++axis) {
                    vertices[9*corner+axis]=original[axis];vertices[9*corner+3+axis]=radial[axis];
                    vertices[9*corner+6+axis]=axis==1?.8f:.2f;
                    words[9*sample+6+axis]=std::bit_cast<unsigned>(float(fine[axis]));
                    const auto bits=std::bit_cast<std::uint64_t>(parent[axis]);
                    words[9*corners+9*sample+2*axis]=unsigned(bits);
                    words[9*corners+9*sample+2*axis+1]=unsigned(bits>>32);
                    words[9*corners+9*sample+6+axis]=std::bit_cast<unsigned>(original[axis]);
                }
            }
        }
        upload(vbo,vertices);upload(targets,words);const auto eye=radial*1.002;
        auto original=compute.generate(vbo,ebo,stats,p,1000,eye,policy,{},source);
        original->waitForCapture();const auto legacy=original->readForValidation();
        auto envelope=compute.generate(vbo,ebo,stats,p,1000,eye,policy,original->geometry,source,GrassMetadataMode::MorphEnvelope);
        envelope->waitForCapture();EXPECT_FALSE(envelope->geometryReused);
        EXPECT_EQ(envelope->workingBytes,original->workingBytes);EXPECT_EQ(envelope->inputBytes,original->inputBytes);
        const auto metadata=envelope->readForValidation();
        auto reused=compute.generate(vbo,ebo,stats,p,1000,eye,policy,envelope->geometry,source,GrassMetadataMode::MorphEnvelope);
        reused->waitForCapture();ASSERT_TRUE(reused->geometryReused);
        const auto reusedMetadata=reused->readForValidation();
        auto back=compute.generate(vbo,ebo,stats,p,1000,eye,policy,envelope->geometry,source);
        back->waitForCapture();EXPECT_FALSE(back->geometryReused);
        const auto restored=back->readForValidation();
        auto plan=allocation.generate(*envelope,f);plan->waitForCapture();const auto summary=plan->readSummary();
        EXPECT_EQ(summary.padding[0],0u);EXPECT_DOUBLE_EQ(summary.densitySearch[0],f.density_per_m2);
        const auto references=plan->readReferencesForValidation(summary.totals[0]);
        std::array<unsigned,triangles> slots{};
        for(unsigned level=0;level<summary.counts.size();++level)
            for(unsigned i=0;i<summary.counts[level];++i) slots[references[summary.first[level]+i]]=1u<<level;
        for(unsigned t=0;t<triangles;++t) {
            SCOPED_TRACE(t);const auto& m=metadata[t];ASSERT_EQ(m.identity[1],1u);
            originalRejected+=legacy[t].identity[1]==0;
            EXPECT_EQ(reusedMetadata[t].centerReach,m.centerReach);EXPECT_EQ(reusedMetadata[t].areaDistance,m.areaDistance);
            EXPECT_EQ(reusedMetadata[t].identity,m.identity);
            EXPECT_EQ(restored[t].centerReach,legacy[t].centerReach);EXPECT_EQ(restored[t].areaDistance,legacy[t].areaDistance);
            EXPECT_EQ(restored[t].identity,legacy[t].identity);
            const glm::dvec3 center(m.centerReach[0],m.centerReach[1],m.centerReach[2]);
            double maxMixed=0,maxUniform=0;
            for(unsigned mask=0;mask<8;++mask) {
                const auto a=endpoints[3*t][mask&1],b=endpoints[3*t+1][(mask>>1)&1],c=endpoints[3*t+2][(mask>>2)&1];
                const double area=.5*glm::length(glm::cross(b-a,c-a))*1e6;
                maxMixed=std::max(maxMixed,area);if(mask==0 || mask==7) maxUniform=std::max(maxUniform,area);
            }
            mixedLarger+=maxMixed>maxUniform*1.5+1e-8;
            // Grid includes all eight independent endpoint combinations; random
            // phases check float-rounding plateaus inside the interpolation.
            for(unsigned phase=0;phase<729+1024;++phase) {
                std::array<glm::dvec3,3> positions;
                for(unsigned c=0;c<3;++c) {
                    const unsigned divisor=c==0?1:c==1?9:81;
                    const double weight=phase<729 ? double((phase/divisor)%9)/8 : double(random())/double(random.max());
                    const auto& e=endpoints[3*t+c];positions[c]=glm::dvec3(glm::vec3(e[0]+(e[1]-e[0])*weight));
                    ASSERT_LE(glm::length(positions[c]-center),m.centerReach[3]);
                }
                const double area=.5*glm::length(glm::cross(positions[1]-positions[0],positions[2]-positions[0]))*1e6;
                ASSERT_LE(area,m.areaDistance[0]);ASSERT_LE(area*f.density_per_m2,slots[t]);++checked;
            }
        }
        auto limited=f;limited.max_candidates_per_triangle=1;
        auto infeasible=allocation.generate(*envelope,limited);infeasible->waitForCapture();
        EXPECT_EQ(infeasible->readSummary().padding[0],1u); // Existing explicit protected-density failure.
        EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
    }
    EXPECT_GT(mixedLarger,0u);EXPECT_GT(originalRejected,0u);
    std::cout << "MORPH_GRASS_ENVELOPE checked_phases=" << checked << " mixed_larger=" << mixedLarger
        << " original_rejected=" << originalRejected << " extra_gpu_storage_bytes=0\n";
}

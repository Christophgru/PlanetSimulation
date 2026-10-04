#include "rendering/foliage/procedural/ProceduralGrass.h"
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace rendering {
namespace {
// Matches placement.comp's std430 Blade. Allocated on GPU, never uploaded.
struct GpuBlade { glm::vec4 rootFade,upLow,variation,wind; };
static_assert(sizeof(GpuBlade)==64);
}
void ProceduralGrass::allocateComputed(const Patch& patch,std::size_t count) const {
    if(!count) return;
    GLint64 blockBytes=0;glGetInteger64v(GL_MAX_SHADER_STORAGE_BLOCK_SIZE,&blockBytes);
    if(blockBytes<=0 || (patch.allocation && count>std::uint64_t(blockBytes)/128) || count>std::numeric_limits<GLsizei>::max())
        throw std::runtime_error("Grass placement exceeds queried storage capacity");
    if (!patch.gpuBlades) glGenBuffers(1,&patch.gpuBlades);
    if (!patch.commands) {
        glGenBuffers(1,&patch.commands);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER,patch.commands);
        glBufferData(GL_SHADER_STORAGE_BUFFER,32,nullptr,GL_DYNAMIC_DRAW);
    }
    if (patch.gpuCapacity<count) {
        glBindBuffer(GL_ARRAY_BUFFER,patch.gpuBlades);
        glBufferData(GL_ARRAY_BUFFER,2*count*sizeof(GpuBlade),nullptr,GL_DYNAMIC_DRAW);
        if(glGetError()!=GL_NO_ERROR) throw std::runtime_error("Grass placement allocation failed");
        patch.gpuCapacity=count;
        for (int queue=0;queue<2;++queue) {
            if (!patch.gpuVaos[queue]) glGenVertexArrays(1,&patch.gpuVaos[queue]);
            glBindVertexArray(patch.gpuVaos[queue]);
            const std::size_t base=queue*count*sizeof(GpuBlade);
            const auto attribute=[&](int location,int components,std::size_t offset) {
                glEnableVertexAttribArray(location);
                glVertexAttribPointer(location,components,GL_FLOAT,GL_FALSE,sizeof(GpuBlade),
                    reinterpret_cast<void*>(base+offset));
                glVertexAttribDivisor(location,1);
            };
            attribute(0,3,0); attribute(1,3,16); attribute(2,4,32);
            attribute(4,3,48); attribute(5,1,12); attribute(6,1,28);
        }
        glBindVertexArray(0);
    }
}
void ProceduralGrass::drawComputed(const Patch& patch,const GrassPass& pass) const {
    if (pass.feedback) glPauseTransformFeedback();
    std::size_t count=0;
    for (const auto& batch:patch.batches) count+=batch.count;
    if (!count) { if (pass.feedback) glResumeTransformFeedback(); return; }
    if(!compute_) {
        if(patch.allocation) throw std::logic_error("Resident grass placement program was not prepared");
        compute_=std::make_unique<Shader>("shaders/foliage/placement.comp");
    }
    GLint64 blockBytes=0;GLint maxGroups=0;
    glGetInteger64v(GL_MAX_SHADER_STORAGE_BLOCK_SIZE,&blockBytes);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT,0,&maxGroups);
    if(maxGroups<1 || (patch.allocation && count>std::uint64_t(blockBytes)/128) || count>std::numeric_limits<GLsizei>::max())
        throw std::runtime_error("Grass placement exceeds queried storage/dispatch capacity");
    if(patch.allocation) {
        if(patch.gpuCapacity<count || !patch.gpuBlades || !patch.commands || !patch.gpuVaos[0] || !patch.gpuVaos[1])
            throw std::logic_error("Resident grass draw resources were not prepared");
    } else allocateComputed(patch,count);
    auto& c=*compute_;
    c.use();
    c.setInt("uCapacity",patch.gpuCapacity);
    c.setInt("uTerrainVertices",8); c.setInt("uTerrainIndices",9);
    glActiveTexture(GL_TEXTURE8); glBindTexture(GL_TEXTURE_BUFFER,patch.vertexTexture);
    glActiveTexture(GL_TEXTURE9); glBindTexture(GL_TEXTURE_BUFFER,patch.indexTexture);
    glActiveTexture(GL_TEXTURE0);
    c.setMat4("model",glm::value_ptr(pass.model));
    c.setMat4("view",glm::value_ptr(pass.view));
    c.setMat4("projection",glm::value_ptr(pass.projection));
    c.setFloat3("uGrassEyeBody",pass.mainEyeBody.x,pass.mainEyeBody.y,pass.mainEyeBody.z);
    c.setFloat3("uPlanetColor",patch.color.x,patch.color.y,patch.color.z);
    c.setFloat3("uLandscapeLevels",patch.landscapeLevels.x,patch.landscapeLevels.y,patch.landscapeLevels.z);
    c.setFloat2("uTerrainRockRange",patch.rockRange.x,patch.rockRange.y);
    c.setInt("uLandscapeEnabled",patch.landscape); c.setInt("uWaterEnabled",patch.water);
    const auto& f=patch.settings;
    c.setFloat("uMetersPerRadius",patch.scale); c.setFloat("uDrawDistance",f.draw_distance_m);
    c.setFloat("uQuadDistance",f.quadDistanceMeters());
    c.setFloat("uPlacementDensity",patch.density);
    c.setFloat("uGaussianSigma",f.draw_distance_m*f.gaussian_sigma_fraction);
    c.setFloat("uGrassHeight",f.height_m); c.setFloat("uGrassWidth",f.width_m);
    c.setFloat("uRootOffset",f.root_offset_m); c.setFloat("uGreenRatio",f.green_ratio);
    c.setFloat("uWaterClearance",f.water_clearance_m);
    c.setFloat2("uHeightMultiplierRange",f.height_multiplier_min,f.height_multiplier_max);
    c.setFloat2("uLeanRange",f.lean_min,f.lean_max);
    c.setInt("uGrassSeed",f.seed); c.setInt("uWindSeed",f.wind_noise.seed);
    c.setFloat3("uWindFrequencies",f.wind_noise.gust_frequency,f.wind_noise.direction_frequency,f.wind_noise.flutter_frequency);
    c.setFloat("uWindStrength",f.wind_strength);
    c.setFloat("uTime",pass.windTime);
    c.setInt("uFrustumCull",f.frustum_culling);
    const double reach=f.height_m*f.height_multiplier_max+f.width_m+f.root_offset_m;
    c.setFloat("uCullExtent",reach/patch.scale);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER,0,patch.buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER,1,patch.commands);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER,2,patch.gpuBlades);
    c.setInt("uReset",1); glDispatchCompute(1,1,1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    c.setInt("uReset",0);
    std::size_t first=0;
    for (std::size_t level=0;level<grassCandidateSlots.size();++level) {
        const auto candidates=patch.batches[level].count;
        if (candidates) {
            c.setInt("uFirstPatch",first); c.setInt("uCandidateCount",candidates);
            c.setInt("uSlotsPerPatch",grassCandidateSlots[level]);
            const std::size_t capacity=std::size_t(maxGroups)*128;
            const std::size_t chunk=patch.allocation ? std::min<std::size_t>(capacity,65536) : capacity;
            for(std::size_t offset=0;offset<std::size_t(candidates);offset+=chunk) {
                c.setInt("uCandidateOffset",offset);
                glDispatchCompute((std::min(chunk,std::size_t(candidates)-offset)+127)/128,1,1);
                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
            }
        }
        first+=candidates/grassCandidateSlots[level];
    }
    glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT|GL_COMMAND_BARRIER_BIT|GL_SHADER_STORAGE_BARRIER_BIT);
    shader.use(); shader.setInt("uGpuInstances",1);
    shader.setInt("uProcedural",0);
    if (pass.feedback) glResumeTransformFeedback();
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER,patch.commands);
    for (int queue=0;queue<2;++queue) {
        shader.setInt("uSegments",queue==0 ? 6 : 1);
        glBindVertexArray(patch.gpuVaos[queue]);
        glDrawArraysIndirect(GL_TRIANGLE_STRIP,reinterpret_cast<void*>(queue*16));
    }
    glBindVertexArray(0); glBindBuffer(GL_DRAW_INDIRECT_BUFFER,0);
}
std::array<std::size_t,2> ProceduralGrass::computedCounts(std::size_t index) const {
    if (index>=patches_.size() || !patches_[index] || !patches_[index]->ready || !patches_[index]->computeUsed) return {};
    std::array<std::uint32_t,8> commands{};
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    if(!patches_[index]->commands || std::none_of(patches_[index]->batches.begin(),patches_[index]->batches.end(),
        [](const auto& batch) {return batch.count!=0;})) return {};
    glBindBuffer(GL_COPY_READ_BUFFER,patches_[index]->commands);
    glGetBufferSubData(GL_COPY_READ_BUFFER,0,sizeof(commands),commands.data());
    glBindBuffer(GL_COPY_READ_BUFFER,0);
    return {commands[1],commands[5]};
}
} // namespace rendering

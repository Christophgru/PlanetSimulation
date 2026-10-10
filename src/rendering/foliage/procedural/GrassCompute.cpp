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
    if(!patch.gpuRoots) glGenBuffers(1,&patch.gpuRoots);
    if(!patch.gpuBounds) glGenBuffers(1,&patch.gpuBounds);
    if(patch.rootCapacity<count) {
        glBindBuffer(GL_ARRAY_BUFFER,patch.gpuRoots);
        glBufferData(GL_ARRAY_BUFFER,count*sizeof(GrassRootRecord),nullptr,GL_STATIC_DRAW);
        patch.rootCapacity=count;patch.rootsReady=false;
    }
    if(patch.boundsCapacity!=patch.patches) {
        glBindBuffer(GL_ARRAY_BUFFER,patch.gpuBounds);
        glBufferData(GL_ARRAY_BUFFER,patch.patches*sizeof(glm::vec4),nullptr,GL_STATIC_DRAW);
        patch.boundsCapacity=patch.patches;patch.rootsReady=false;
    }
    if(!patch.gpuRoots || !patch.gpuBounds || glGetError()!=GL_NO_ERROR)
        throw std::runtime_error("Grass root cache allocation failed");
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
        if(patch.gpuCapacity<count || !patch.gpuBlades || !patch.commands || !patch.gpuVaos[0] || !patch.gpuVaos[1] || !patch.rootsReady)
            throw std::logic_error("Resident grass draw resources were not prepared");
    } else allocateComputed(patch,count);
    bindComputedInputs(patch);
    if(!patch.rootsReady) generateRoots(patch);
    auto& c=*compute_;c.use();c.setInt("uBuildRoots",0);
    c.setMat4("model",glm::value_ptr(pass.model));
    c.setMat4("view",glm::value_ptr(pass.view));
    c.setMat4("projection",glm::value_ptr(pass.projection));
    c.setFloat3("uGrassEyeBody",pass.mainEyeBody.x,pass.mainEyeBody.y,pass.mainEyeBody.z);
    c.setFloat("uTime",pass.windTime);
    GpuWorkProfiler::Scope timing(GpuWorkStage::GrassPlacement,patch.workIdentity,pass.timingView);
    c.setInt("uReset",1); glDispatchCompute(1,1,1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    c.setInt("uReset",0);
    dispatchCandidates(patch,maxGroups);
    glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT|GL_COMMAND_BARRIER_BIT|GL_SHADER_STORAGE_BARRIER_BIT);
    timing.stop();
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

#include "rendering/foliage/horizon/HorizonGrass.h"
#include "rendering/foliage/GrassPlacement.h"
#include "rendering/geometry/Mesh.h"
#include "rendering/diagnostics/tracing/CpuTrace.h"
#include "config/ScenarioConfig.h"
#include <chrono>
#include <cstddef>

namespace rendering {
void HorizonGrass::upload(Patch& patch,const HorizonGrassPlan& plan) {
    CpuTrace::Scope scope("HorizonGrass::upload");
    if (!patch.buffer) glGenBuffers(1,&patch.buffer);
    glBindBuffer(GL_ARRAY_BUFFER,patch.buffer);
    glBufferData(GL_ARRAY_BUFFER,plan.patches.size()*sizeof(HorizonGrassPatch),plan.patches.data(),GL_STATIC_DRAW);
    const std::size_t offsets[]={offsetof(HorizonGrassPatch,a),offsetof(HorizonGrassPatch,b),
        offsetof(HorizonGrassPatch,c),offsetof(HorizonGrassPatch,normal),offsetof(HorizonGrassPatch,color),
        offsetof(HorizonGrassPatch,expectedCandidates),offsetof(HorizonGrassPatch,seed)};
    for (int level=0;level<8;++level) {
        auto& batch=patch.batches[level];
        batch.count=GLsizei(plan.batches[level].count*horizonGrassSlots[level]);
        if (!batch.count) continue;
        if (!batch.vao) glGenVertexArrays(1,&batch.vao);
        glBindVertexArray(batch.vao);
        const std::size_t start=plan.batches[level].first*sizeof(HorizonGrassPatch);
        for (int location=0;location<7;++location) {
            glEnableVertexAttribArray(location);
            const auto pointer=reinterpret_cast<void*>(start+offsets[location]);
            if (location==6) glVertexAttribIPointer(location,1,GL_UNSIGNED_INT,sizeof(HorizonGrassPatch),pointer);
            else glVertexAttribPointer(location,location==5 ? 1 : 3,GL_FLOAT,GL_FALSE,sizeof(HorizonGrassPatch),pointer);
            // GL 3.3 advances this descriptor once per N instances. The shader
            // uses instanceID % N to generate N independent roots on it.
            glVertexAttribDivisor(location,horizonGrassSlots[level]);
        }
    }
    glBindVertexArray(0);
    patch.patches=plan.patches.size(); patch.distanceMeters=plan.distanceMeters;
}
HorizonGrass::~HorizonGrass() { clear(); glDeleteProgram(shader.id); }
void HorizonGrass::clear() {
    for (auto& patch:patches_) {
        for (auto& batch:patch.batches) glDeleteVertexArrays(1,&batch.vao);
        glDeleteBuffers(1,&patch.buffer);
    }
    patches_.clear();
}
GrassPreparationStats HorizonGrass::prepare(std::size_t index,const Mesh& mesh,const config::PlanetConfig& planet,
    double metersPerWorldUnit,const glm::dvec3& eyeBody) {
    CpuTrace::Scope scope("HorizonGrass::prepare");
    if (patches_.size()<=index) patches_.resize(index+1);
    auto& patch=patches_[index];
    if (!planet.foliage.enabled || !planet.foliage.horizon_enabled || !mesh.hasVertexColors) {
        for (auto& batch:patch.batches) batch.count=0;
        patch.patches=0; patch.distanceMeters=0; patch.ready=false; return {};
    }
    const double scale=planet.radius*metersPerWorldUnit;
    if (patch.ready && patch.revision==mesh.revision &&
        glm::length(eyeBody-patch.eye)*scale<planet.foliage.far_rebuild_distance_m) return {};
    const auto start=std::chrono::steady_clock::now();
    const auto plan=planHorizonGrass(mesh.vertices,mesh.indices,planet,metersPerWorldUnit,eyeBody);
    const auto planned=std::chrono::steady_clock::now();
    upload(patch,plan);
    const auto uploaded=std::chrono::steady_clock::now();
    patch.eye=eyeBody; patch.revision=mesh.revision; patch.ready=true;
    return {std::chrono::duration<double,std::milli>(planned-start).count(),0,
        std::chrono::duration<double,std::milli>(uploaded-planned).count(),1};
}
HorizonGrassStats HorizonGrass::stats(std::size_t index) const {
    HorizonGrassStats result;
    if (index>=patches_.size()) return result;
    const auto& patch=patches_[index];
    result.patches=patch.patches; result.patchBytes=patch.patches*sizeof(HorizonGrassPatch);
    result.distanceMeters=patch.distanceMeters;
    for (const auto& batch:patch.batches) { result.candidates+=batch.count; result.batches+=batch.count>0; }
    result.vertices=3*result.candidates; result.triangles=result.candidates;
    return result;
}
void HorizonGrass::draw(std::size_t index) const {
    CpuTrace::Scope scope("HorizonGrass::draw");
    if (index>=patches_.size()) return;
    const bool culled=glIsEnabled(GL_CULL_FACE); glDisable(GL_CULL_FACE);
    for (int level=0;level<8;++level) {
        const auto& batch=patches_[index].batches[level]; if (!batch.count) continue;
        shader.setInt("uSlotsPerPatch",horizonGrassSlots[level]);
        glBindVertexArray(batch.vao); glDrawArraysInstanced(GL_TRIANGLES,0,3,batch.count);
    }
    glBindVertexArray(0); if (culled) glEnable(GL_CULL_FACE);
}
}

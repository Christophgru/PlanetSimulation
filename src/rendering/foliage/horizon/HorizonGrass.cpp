#include "rendering/foliage/horizon/HorizonGrass.h"
#include "rendering/foliage/GrassPlacement.h"
#include "rendering/geometry/Mesh.h"
#include "rendering/diagnostics/tracing/CpuTrace.h"
#include "config/ScenarioConfig.h"
#include <chrono>
#include <algorithm>
#include "rendering/foliage/GrassLod.h"
#include <cstddef>

namespace rendering {
HorizonGrass::HorizonGrass(bool detailed) : detailed_(detailed),
    shader(detailed ? "shaders/foliage/grass.vert" : "shaders/foliage/horizon.vert",
        "shaders/foliage/grass.frag", "shaders/terrain/terrain_shadow.glsl",
        "shaders/atmosphere/atmosphere.glsl") {}

void HorizonGrass::attributes(std::size_t first,int level) const {
    const int location=detailed_ ? 3 : 0;
    glEnableVertexAttribArray(location);
    glVertexAttribIPointer(location,1,GL_UNSIGNED_INT,sizeof(std::uint32_t),
        reinterpret_cast<void*>(first*sizeof(std::uint32_t)));
    glVertexAttribDivisor(location,horizonGrassSlots[level]);
}
void HorizonGrass::upload(Patch& patch,const HorizonGrassPlan& plan) {
    CpuTrace::Scope scope("HorizonGrass::upload");
    if (!patch.buffer) glGenBuffers(1,&patch.buffer);
    glBindBuffer(GL_ARRAY_BUFFER,patch.buffer);
    // Terrain is already resident. Upload only triangle IDs; density, seed
    // and wind rules are uniforms. No grass vertices, roots or noise fields.
    std::vector<std::uint32_t> references;
    references.reserve(plan.patches.size());
    patch.bounds.clear();
    if (detailed_) patch.bounds.reserve(plan.patches.size());
    for (const auto& descriptor:plan.patches) {
        references.push_back(descriptor.triangle);
        if (detailed_) {
            const auto center=(glm::dvec3(descriptor.a)+glm::dvec3(descriptor.b)+glm::dvec3(descriptor.c))/3.0;
            const auto radius=std::max({glm::length(glm::dvec3(descriptor.a)-center),
                glm::length(glm::dvec3(descriptor.b)-center),glm::length(glm::dvec3(descriptor.c)-center)});
            patch.bounds.push_back({center,radius});
        }
    }
    glBufferData(GL_ARRAY_BUFFER,references.size()*sizeof(std::uint32_t),references.data(),GL_STATIC_DRAW);
    patch.density=plan.density;
    patch.draws.clear();
    for (std::size_t level=0;level<horizonGrassSlots.size();++level) {
        auto& batch=patch.batches[level];
        batch.count=GLsizei(plan.batches[level].count*horizonGrassSlots[level]);
        if (!batch.count) continue;
        if (!batch.vao) glGenVertexArrays(1,&batch.vao);
        glBindVertexArray(batch.vao);
        attributes(plan.batches[level].first,level);
    }
    glBindVertexArray(0);
    patch.patches=plan.patches.size(); patch.distanceMeters=plan.distanceMeters;
}
void HorizonGrass::updateDraws(Patch& patch,double scale,double nearDistance,const glm::dvec3& eyeBody) {
    // Select geometry from current triangle bounds every frame, without
    // regenerating roots or uploading descriptors. Reflections reuse it.
    patch.draws.clear();
    std::size_t first=0;
    for (std::size_t level=0;level<horizonGrassSlots.size();++level) {
        const std::size_t count=patch.batches[level].count/horizonGrassSlots[level];
        if (!detailed_) {
            if (count) patch.draws.push_back({first,count,int(level),1});
            first+=count;
            continue;
        }
        for (std::size_t i=first;i<first+count;++i) {
            const auto& bound=patch.bounds[i];
            const int segments=(glm::length(bound.center-eyeBody)-bound.radius)*scale<nearDistance ? 6 : 1;
            if (!patch.draws.empty() && patch.draws.back().level==int(level) && patch.draws.back().segments==segments)
                ++patch.draws.back().patches;
            else patch.draws.push_back({i,1,int(level),segments});
        }
        first+=count;
    }
}
HorizonGrass::~HorizonGrass() { clear(); if (compute_) glDeleteProgram(compute_->id); glDeleteProgram(shader.id); }
void HorizonGrass::clear() {
    for (auto& patch:patches_) {
        for (auto& batch:patch.batches) glDeleteVertexArrays(1,&batch.vao);
        glDeleteBuffers(1,&patch.buffer);
        glDeleteTextures(1,&patch.vertexTexture); glDeleteTextures(1,&patch.indexTexture);
        glDeleteBuffers(1,&patch.gpuBlades); glDeleteBuffers(1,&patch.commands);
        glDeleteVertexArrays(patch.gpuVaos.size(),patch.gpuVaos.data());
    }
    patches_.clear();
}
GrassPreparationStats HorizonGrass::prepare(std::size_t index,const Mesh& mesh,const config::PlanetConfig& planet,
    double metersPerWorldUnit,const glm::dvec3& eyeBody) {
    CpuTrace::Scope scope("HorizonGrass::prepare");
    if (patches_.size()<=index) patches_.resize(index+1);
    auto& patch=patches_[index];
    patch.settings=planet.foliage; patch.color={planet.color[0],planet.color[1],planet.color[2]};
    const auto rockRange=planet.terrain_material.slopeMetricRange();
    patch.rockRange={rockRange[0],rockRange[1]};
    double relief=planet.terrain_landscape.maximumAbsoluteHeightMeters();
    for (const auto& noise:planet.surface_noise) relief+=noise.amplitude_m;
    patch.landscapeLevels={planet.water.enabled ? planet.water.level_m : 0,.1,relief};
    patch.water=planet.water.enabled; patch.landscape=planet.terrain_landscape.enabled;
    patch.scale=planet.radius*metersPerWorldUnit;
    if (!planet.foliage.enabled || !(detailed_ ? planet.foliage.near_enabled : planet.foliage.horizon_enabled) || !mesh.hasVertexColors) {
        for (auto& batch:patch.batches) batch.count=0;
        patch.patches=0; patch.distanceMeters=0; patch.ready=false; patch.computeUsed=false; patch.draws.clear(); return {};
    }
    const double scale=planet.radius*metersPerWorldUnit;
    const double margin=detailed_ ? grassRebuildDistance(planet.foliage) : planet.foliage.far_rebuild_distance_m;
    if (patch.ready && patch.revision==mesh.revision && glm::length(eyeBody-patch.eye)*scale<margin) {
        updateDraws(patch,scale,grassLodNearDistance(planet.foliage.draw_distance_m),eyeBody);
        return {};
    }
    if (!mesh.vbo || !mesh.ebo) throw std::logic_error("Procedural grass requires uploaded terrain");
    if (!patch.vertexTexture) glGenTextures(1,&patch.vertexTexture);
    if (!patch.indexTexture) glGenTextures(1,&patch.indexTexture);
    // Buffer textures are zero-copy views of the mesh's existing VBO/EBO.
    glActiveTexture(GL_TEXTURE8); glBindTexture(GL_TEXTURE_BUFFER,patch.vertexTexture);
    glTexBuffer(GL_TEXTURE_BUFFER,GL_R32F,mesh.vbo);
    glActiveTexture(GL_TEXTURE9); glBindTexture(GL_TEXTURE_BUFFER,patch.indexTexture);
    glTexBuffer(GL_TEXTURE_BUFFER,GL_R32UI,mesh.ebo);
    glActiveTexture(GL_TEXTURE0);
    patch.seed=planet.foliage.seed;
    const auto start=std::chrono::steady_clock::now();
    const auto plan=planHorizonGrass(mesh.vertices,mesh.indices,planet,metersPerWorldUnit,eyeBody,detailed_);
    const auto planned=std::chrono::steady_clock::now();
    upload(patch,plan);
    const auto uploaded=std::chrono::steady_clock::now();
    patch.eye=eyeBody; patch.revision=mesh.revision; patch.ready=true; patch.movementMargin=margin;
    updateDraws(patch,scale,grassLodNearDistance(planet.foliage.draw_distance_m),eyeBody);
    return {std::chrono::duration<double,std::milli>(planned-start).count(),0,
        std::chrono::duration<double,std::milli>(uploaded-planned).count(),1,
        plan.patches.size()*sizeof(std::uint32_t)};
}
HorizonGrassStats HorizonGrass::stats(std::size_t index) const {
    HorizonGrassStats result;
    if (index>=patches_.size()) return result;
    const auto& patch=patches_[index];
    result.patches=patch.patches; result.patchBytes=patch.patches*sizeof(std::uint32_t);
    result.distanceMeters=patch.distanceMeters;
    result.gpuBytes=patch.gpuCapacity*128+(patch.commands ? 32 : 0);
    for (const auto& draw:patch.draws) {
        const auto count=draw.patches*horizonGrassSlots[draw.level];
        result.candidates+=count; ++result.batches;
        result.vertices+=count*(2*draw.segments+2);
        result.triangles+=count*2*draw.segments;
    }
    return result;
}
bool HorizonGrass::usesCompute(std::size_t index) const {
    return index<patches_.size() && patches_[index].computeUsed;
}
void HorizonGrass::draw(std::size_t index,const GrassPass* pass) const {
    CpuTrace::Scope scope("HorizonGrass::draw");
    if (index>=patches_.size()) return;
    const bool culled=glIsEnabled(GL_CULL_FACE); glDisable(GL_CULL_FACE);
    const auto& patch=patches_[index];
    patch.computeUsed=pass && patch.settings.compute_placement && GLEW_VERSION_4_3;
    if (patch.computeUsed) {
        drawComputed(patch,*pass);
        if (culled) glEnable(GL_CULL_FACE);
        return;
    }
    shader.setInt("uGpuInstances",0);
    glBindBuffer(GL_ARRAY_BUFFER,patch.buffer);
    shader.setInt("uTerrainVertices",8); shader.setInt("uTerrainIndices",9);
    shader.setInt("uGrassSeed",patch.seed);
    shader.setFloat("uPlacementDensity",patch.density);
    glActiveTexture(GL_TEXTURE8); glBindTexture(GL_TEXTURE_BUFFER,patch.vertexTexture);
    glActiveTexture(GL_TEXTURE9); glBindTexture(GL_TEXTURE_BUFFER,patch.indexTexture);
    glActiveTexture(GL_TEXTURE0);
    if (detailed_) {
        shader.setInt("uProcedural",1);
        shader.setFloat3("uPlacementEyeBody",patch.eye.x,patch.eye.y,patch.eye.z);
        shader.setFloat("uPlacementMargin",patch.movementMargin);
    }
    for (const auto& draw:patch.draws) {
        shader.setInt("uSlotsPerPatch",horizonGrassSlots[draw.level]);
        shader.setInt("uSegments",draw.segments);
        glBindVertexArray(patch.batches[draw.level].vao);
        attributes(draw.first,draw.level);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP,0,2*draw.segments+2,
            GLsizei(draw.patches*horizonGrassSlots[draw.level]));
    }
    glBindVertexArray(0); if (culled) glEnable(GL_CULL_FACE);
}
}

#include "rendering/foliage/procedural/ProceduralGrass.h"
#include "rendering/foliage/GrassPlacement.h"
#include "rendering/geometry/Mesh.h"
#include "rendering/diagnostics/tracing/CpuTrace.h"
#include "config/ScenarioConfig.h"
#include <chrono>
#include <algorithm>
#include <cstddef>
#include <cmath>

namespace rendering {
ProceduralGrass::ProceduralGrass() :
    shader("shaders/foliage/grass.vert",
        "shaders/foliage/grass.frag", "shaders/terrain/terrain_shadow.glsl",
        "shaders/atmosphere/atmosphere.glsl", "shaders/foliage/palette.glsl") {}

void ProceduralGrass::attributes(std::size_t first,int level) const {
    const int location=3;
    glEnableVertexAttribArray(location);
    glVertexAttribIPointer(location,1,GL_UNSIGNED_INT,sizeof(std::uint32_t),
        reinterpret_cast<void*>(first*sizeof(std::uint32_t)));
    glVertexAttribDivisor(location,grassCandidateSlots[level]);
}
void ProceduralGrass::upload(Patch& patch,const GrassPlan& plan) {
    CpuTrace::Scope scope("ProceduralGrass::upload");
    if (!patch.buffer) glGenBuffers(1,&patch.buffer);
    glBindBuffer(GL_ARRAY_BUFFER,patch.buffer);
    // Terrain is already resident. Upload only triangle IDs; density, seed
    // and wind rules are uniforms. No grass vertices, roots or noise fields.
    std::vector<std::uint32_t> references;
    references.reserve(plan.patches.size());
    patch.bounds.clear();
    patch.bounds.reserve(plan.patches.size());
    for (const auto& descriptor:plan.patches) {
        references.push_back(descriptor.triangle);
        const auto center=(glm::dvec3(descriptor.a)+glm::dvec3(descriptor.b)+glm::dvec3(descriptor.c))/3.0;
        const auto radius=std::max({glm::length(glm::dvec3(descriptor.a)-center),
            glm::length(glm::dvec3(descriptor.b)-center),glm::length(glm::dvec3(descriptor.c)-center)});
        patch.bounds.push_back({center,radius});
    }
    glBufferData(GL_ARRAY_BUFFER,references.size()*sizeof(std::uint32_t),references.data(),GL_STATIC_DRAW);
    patch.density=plan.density;
    patch.draws.clear();
    for (std::size_t level=0;level<grassCandidateSlots.size();++level) {
        auto& batch=patch.batches[level];
        batch.count=GLsizei(plan.batches[level].count*grassCandidateSlots[level]);
        if (!batch.count) continue;
        if (!batch.vao) glGenVertexArrays(1,&batch.vao);
        glBindVertexArray(batch.vao);
        attributes(plan.batches[level].first,level);
    }
    glBindVertexArray(0);
    patch.patches=plan.patches.size(); patch.distanceMeters=plan.distanceMeters;
}
void ProceduralGrass::updateDraws(Patch& patch,double scale,double nearDistance,const glm::dvec3& eyeBody) {
    // Select geometry from current triangle bounds every frame, without
    // regenerating roots or uploading descriptors. Reflections reuse it.
    patch.draws.clear();
    std::size_t first=0;
    for (std::size_t level=0;level<grassCandidateSlots.size();++level) {
        const std::size_t count=patch.batches[level].count/grassCandidateSlots[level];
        for (std::size_t i=first;i<first+count;++i) {
            const auto& bound=patch.bounds[i];
            const int segments=(glm::length(bound.center-eyeBody)-bound.radius)*scale<nearDistance+patch.settings.root_offset_m ? 6 : 1;
            if (!patch.draws.empty() && patch.draws.back().level==int(level) && patch.draws.back().segments==segments)
                ++patch.draws.back().patches;
            else patch.draws.push_back({i,1,int(level),segments});
        }
        first+=count;
    }
}
ProceduralGrass::~ProceduralGrass() { clear(); if (compute_) glDeleteProgram(compute_->id); glDeleteProgram(shader.id); }
void ProceduralGrass::clear() {
    for (auto& patch:patches_) {
        for (auto& batch:patch.batches) glDeleteVertexArrays(1,&batch.vao);
        glDeleteBuffers(1,&patch.buffer);
        glDeleteTextures(1,&patch.vertexTexture); glDeleteTextures(1,&patch.indexTexture);
        glDeleteBuffers(1,&patch.gpuBlades); glDeleteBuffers(1,&patch.commands);
        glDeleteVertexArrays(patch.gpuVaos.size(),patch.gpuVaos.data());
    }
    patches_.clear();
    for (const auto& [index,data]:trails_) {
        glDeleteBuffers(1,&data.buffer); glDeleteTextures(1,&data.texture);
    }
    trails_.clear(); replayPlanEyes_.clear();
}
GrassPreparationStats ProceduralGrass::prepare(std::size_t index,const Mesh& mesh,const config::PlanetConfig& planet,
    double metersPerWorldUnit,const glm::dvec3& eyeBody) {
    CpuTrace::Scope scope("ProceduralGrass::prepare");
    if (patches_.size()<=index) patches_.resize(index+1);
    auto& patch=patches_[index];
    patch.computeUsed=false;
    patch.settings=planet.foliage; patch.color={planet.color[0],planet.color[1],planet.color[2]};
    const auto rockRange=planet.terrain_material.slopeMetricRange();
    patch.rockRange={rockRange[0],rockRange[1]};
    double relief=planet.terrain_landscape.maximumAbsoluteHeightMeters();
    for (const auto& noise:planet.surface_noise) relief+=noise.amplitude_m;
    patch.landscapeLevels={planet.water.enabled ? planet.water.level_m : 0,.1,relief};
    patch.water=planet.water.enabled; patch.landscape=planet.terrain_landscape.enabled;
    patch.scale=planet.radius*metersPerWorldUnit;
    if (!planet.foliage.enabled || !mesh.hasVertexColors) {
        for (auto& batch:patch.batches) batch.count=0;
        patch.patches=0; patch.distanceMeters=0; patch.ready=false; patch.computeUsed=false; patch.draws.clear(); return {};
    }
    const double scale=planet.radius*metersPerWorldUnit;
    const double margin=grassRebuildDistance(planet.foliage);
    if (patch.ready && patch.revision==mesh.revision && glm::length(eyeBody-patch.eye)*scale<margin) {
        updateDraws(patch,scale,planet.foliage.quadDistanceMeters(),eyeBody);
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
    auto planningEye=eyeBody;
    const auto savedEye=replayPlanEyes_.find(index);
    if (savedEye!=replayPlanEyes_.end()) {
        planningEye=savedEye->second; replayPlanEyes_.erase(savedEye);
    }
    const auto plan=planGrass(mesh.vertices,mesh.indices,planet,metersPerWorldUnit,planningEye);
    const auto planned=std::chrono::steady_clock::now();
    upload(patch,plan);
    const auto uploaded=std::chrono::steady_clock::now();
    patch.eye=planningEye; patch.revision=mesh.revision; patch.ready=true;
    updateDraws(patch,scale,planet.foliage.quadDistanceMeters(),eyeBody);
    return {std::chrono::duration<double,std::milli>(planned-start).count(),0,
        std::chrono::duration<double,std::milli>(uploaded-planned).count(),1,
        plan.patches.size()*sizeof(std::uint32_t)};
}
ProceduralGrassStats ProceduralGrass::stats(std::size_t index) const {
    ProceduralGrassStats result;
    if (index>=patches_.size()) return result;
    const auto& patch=patches_[index];
    result.patches=patch.patches; result.patchBytes=patch.patches*sizeof(std::uint32_t);
    result.distanceMeters=patch.distanceMeters;
    result.gpuBytes=patch.gpuCapacity*128+(patch.commands ? 32 : 0);
    for (const auto& draw:patch.draws) {
        const auto count=draw.patches*grassCandidateSlots[draw.level];
        result.candidates+=count; ++result.batches;
        result.vertices+=count*(2*draw.segments+2);
        result.triangles+=count*2*draw.segments;
    }
    return result;
}
std::optional<glm::dvec3> ProceduralGrass::planningEye(std::size_t index) const {
    if (index>=patches_.size() || !patches_[index].ready) return std::nullopt;
    return patches_[index].eye;
}
void ProceduralGrass::restorePlanningEye(std::size_t index,const glm::dvec3& eye) {
    if (!std::isfinite(eye.x)||!std::isfinite(eye.y)||!std::isfinite(eye.z))
        throw std::invalid_argument("Grass replay planning eye must be finite");
    replayPlanEyes_[index]=eye;
    if (index<patches_.size()) patches_[index].ready=false;
}
bool ProceduralGrass::usesCompute(std::size_t index) const {
    return index<patches_.size() && patches_[index].computeUsed;
}
void ProceduralGrass::draw(std::size_t index,const GrassPass* pass) const {
    CpuTrace::Scope scope("ProceduralGrass::draw");
    if (index>=patches_.size()) return;
    const bool culled=glIsEnabled(GL_CULL_FACE); glDisable(GL_CULL_FACE);
    const auto& patch=patches_[index];
    bindTrail(index,patch.scale);
    shader.setFloat("uQuadDistance",patch.settings.quadDistanceMeters());
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
    shader.setInt("uProcedural",1);
    for (const auto& draw:patch.draws) {
        shader.setInt("uSlotsPerPatch",grassCandidateSlots[draw.level]);
        shader.setInt("uSegments",draw.segments);
        glBindVertexArray(patch.batches[draw.level].vao);
        attributes(draw.first,draw.level);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP,0,2*draw.segments+2,
            GLsizei(draw.patches*grassCandidateSlots[draw.level]));
    }
    glBindVertexArray(0); if (culled) glEnable(GL_CULL_FACE);
}
}

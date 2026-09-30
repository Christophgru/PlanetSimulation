#include "rendering/foliage/GrassRenderer.h"
#include "rendering/foliage/GrassPlacement.h"
#include "rendering/foliage/GrassLod.h"
#include "rendering/geometry/Mesh.h"
#include "config/ScenarioConfig.h"
#include <chrono>

namespace rendering {

void GrassRenderer::upload(Patch& patch, const GrassLodPlan& plan) {
    if (!patch.buffer) glGenBuffers(1,&patch.buffer);
    glBindBuffer(GL_ARRAY_BUFFER,patch.buffer);
    // One contiguous instance upload per planet. GL 3.3 lacks base-instance
    // draws, so each LOD VAO points at a range of the same buffer.
    glBufferData(GL_ARRAY_BUFFER,plan.blades.size()*sizeof(GrassBlade),plan.blades.data(),GL_STATIC_DRAW);
    for (std::size_t level=0; level<patch.batches.size(); ++level) {
        auto& batch=patch.batches[level];
        batch.count=0;
        // The last three density levels share one-triangle geometry. Their
        // contiguous ranges can be submitted together without extra draws.
        if (level>0 && grassLodSegments[level]==grassLodSegments[level-1]) continue;
        std::size_t first=0;
        for (std::size_t end=level; end<patch.batches.size() && grassLodSegments[end]==grassLodSegments[level]; ++end) {
            if (!batch.count) first=plan.batches[end].first;
            batch.count+=static_cast<GLsizei>(plan.batches[end].count);
        }
        if (!batch.count) continue;
        if (!batch.vao) glGenVertexArrays(1,&batch.vao);
        glBindVertexArray(batch.vao);
        const auto start=first*sizeof(GrassBlade);
        for (int location=0;location<3;++location) {
            const std::size_t offset = location==0 ? offsetof(GrassBlade,root) :
                location==1 ? offsetof(GrassBlade,up) : offsetof(GrassBlade,variation);
            glEnableVertexAttribArray(location);
            glVertexAttribPointer(location,location==2 ? 4 : 3,GL_FLOAT,GL_FALSE,sizeof(GrassBlade),reinterpret_cast<void*>(start+offset));
            glVertexAttribDivisor(location,1);
        }
    }
    glBindVertexArray(0);
}

GrassRenderer::~GrassRenderer() { clear(); glDeleteProgram(shader.id); }

void GrassRenderer::clear() {
    for (auto& patch : patches_) {
        for (auto& batch : patch.batches) glDeleteVertexArrays(1,&batch.vao);
        glDeleteBuffers(1,&patch.buffer);
    }
    patches_.clear();
}

GrassPreparationStats GrassRenderer::prepare(std::size_t index,const Mesh& mesh,const config::PlanetConfig& planet,
             double metersPerWorldUnit,const glm::dvec3& eyeBody) {
    if (patches_.size()<=index) patches_.resize(index+1);
    auto& patch=patches_[index];
    if (!planet.foliage.enabled || !mesh.hasVertexColors) {
        for (auto& batch:patch.batches) batch.count=0;
        patch.ready=false; return {};
    }
    const double scale=planet.radius*metersPerWorldUnit;
    const double margin=grassRebuildDistance(planet.foliage);
    if (patch.ready && patch.revision==mesh.revision && glm::length(eyeBody-patch.eye)*scale<margin) return {};
    const auto start = std::chrono::steady_clock::now();
    auto blades=placeGrass(mesh.vertices,mesh.indices,planet,metersPerWorldUnit,eyeBody);
    const auto placed = std::chrono::steady_clock::now();
    const auto plan=batchGrass(blades,eyeBody,scale,planet.foliage.draw_distance_m,margin);
    const auto sorted = std::chrono::steady_clock::now();
    upload(patch,plan);
    const auto uploaded = std::chrono::steady_clock::now();
    patch.eye=eyeBody; patch.revision=mesh.revision; patch.ready=true;
    return {std::chrono::duration<double,std::milli>(placed-start).count(),
            std::chrono::duration<double,std::milli>(sorted-placed).count(),
            std::chrono::duration<double,std::milli>(uploaded-sorted).count(), 1};
}

std::size_t GrassRenderer::count(std::size_t index) const {
    return drawStats(index).blades;
}

GrassDrawStats GrassRenderer::drawStats(std::size_t index) const {
    GrassDrawStats stats;
    if (index>=patches_.size()) return stats;
    for (std::size_t level=0; level<grassLodSegments.size(); ++level) {
        const auto count=static_cast<std::size_t>(patches_[index].batches[level].count);
        stats.blades+=count;
        stats.vertices+=count*grassLodVertices(level);
        stats.triangles+=count*(grassLodVertices(level)-2);
        stats.batches+=count>0;
    }
    stats.instanceBytes=stats.blades*sizeof(GrassBlade);
    return stats;
}

void GrassRenderer::draw(std::size_t index) const {
    if (index>=patches_.size()) return;
    const bool culled=glIsEnabled(GL_CULL_FACE);
    glDisable(GL_CULL_FACE); // Two-sided blades, without duplicate geometry.
    const auto& patch=patches_[index];
    for (std::size_t level=0; level<grassLodSegments.size(); ++level) {
        const auto& batch=patch.batches[level];
        if (!batch.count) continue;
        shader.setInt("uSegments",grassLodSegments[level]);
        glBindVertexArray(batch.vao);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP,0,grassLodVertices(level),batch.count);
    }
    glBindVertexArray(0);
    if (culled) glEnable(GL_CULL_FACE);
}
} // namespace rendering

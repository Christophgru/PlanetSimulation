#include "rendering/foliage/GrassRenderer.h"
#include "rendering/foliage/GrassPlacement.h"
#include "rendering/geometry/Mesh.h"
#include "config/ScenarioConfig.h"
#include <algorithm>
#include <utility>
#include <chrono>

namespace rendering {

void GrassRenderer::upload(Batch& batch, const std::vector<GrassBlade>& blades) {
    if (!batch.vao) glGenVertexArrays(1,&batch.vao);
    if (!batch.buffer) glGenBuffers(1,&batch.buffer);
    glBindVertexArray(batch.vao);
    glBindBuffer(GL_ARRAY_BUFFER,batch.buffer);
    glBufferData(GL_ARRAY_BUFFER,blades.size()*sizeof(GrassBlade),blades.data(),GL_STATIC_DRAW);
    for (int location=0;location<3;++location) {
        const std::size_t offset = location==0 ? offsetof(GrassBlade,root) :
            location==1 ? offsetof(GrassBlade,up) : offsetof(GrassBlade,variation);
        glEnableVertexAttribArray(location);
        glVertexAttribPointer(location,location==2 ? 4 : 3,GL_FLOAT,GL_FALSE,sizeof(GrassBlade),reinterpret_cast<void*>(offset));
        glVertexAttribDivisor(location,1);
    }
    batch.count=static_cast<GLsizei>(blades.size());
    glBindVertexArray(0);
}

GrassRenderer::~GrassRenderer() { clear(); glDeleteProgram(shader.id); }

void GrassRenderer::clear() {
    for (auto& patch : patches_) for (auto* batch : {&patch.near,&patch.far}) {
        glDeleteVertexArrays(1,&batch->vao); glDeleteBuffers(1,&batch->buffer);
    }
    patches_.clear();
}

GrassPreparationStats GrassRenderer::prepare(std::size_t index,const Mesh& mesh,const config::PlanetConfig& planet,
             double metersPerWorldUnit,const glm::dvec3& eyeBody) {
    if (patches_.size()<=index) patches_.resize(index+1);
    auto& patch=patches_[index];
    if (!planet.foliage.enabled || !mesh.hasVertexColors) {
        patch.near.count=patch.far.count=0; patch.ready=false; return {};
    }
    const double scale=planet.radius*metersPerWorldUnit;
    const double margin=grassRebuildDistance(planet.foliage);
    if (patch.ready && patch.revision==mesh.revision && glm::length(eyeBody-patch.eye)*scale<margin) return {};
    const auto start = std::chrono::steady_clock::now();
    auto blades=placeGrass(mesh.vertices,mesh.indices,planet,metersPerWorldUnit,eyeBody);
    const auto placed = std::chrono::steady_clock::now();
    struct DistanceKey { std::size_t index; double squaredDistance; };
    std::vector<DistanceKey> nearKeys,farKeys;
    for (std::size_t i = 0; i < blades.size(); ++i) {
        const auto& blade = blades[i];
        // The margin guarantees that a low-detail blade cannot approach
        // within the detailed range before the next patch update.
        const auto offset = glm::dvec3(blade.root)-eyeBody;
        const double squaredDistance = glm::dot(offset, offset);
        const double distance=std::sqrt(squaredDistance)*scale;
        (distance<=15.0+margin ? nearKeys : farKeys).push_back({i, squaredDistance});
    }
    // Front-to-back blades let depth testing reject the dense layers behind
    // them before running atmospheric/material shading.
    // Sort compact keys; recomputing double-precision distances inside every
    // comparator made this O(N log N) geometry work on each patch rebuild.
    const auto nearer=[](const DistanceKey& a,const DistanceKey& b) {
        return a.squaredDistance < b.squaredDistance;
    };
    std::sort(nearKeys.begin(),nearKeys.end(),nearer);
    std::sort(farKeys.begin(),farKeys.end(),nearer);
    std::vector<GrassBlade> near,far;
    near.reserve(nearKeys.size()); far.reserve(farKeys.size());
    for (const auto& key : nearKeys) near.push_back(blades[key.index]);
    for (const auto& key : farKeys) far.push_back(blades[key.index]);
    const auto sorted = std::chrono::steady_clock::now();
    upload(patch.near,near); upload(patch.far,far);
    const auto uploaded = std::chrono::steady_clock::now();
    patch.eye=eyeBody; patch.revision=mesh.revision; patch.ready=true;
    return {std::chrono::duration<double,std::milli>(placed-start).count(),
            std::chrono::duration<double,std::milli>(sorted-placed).count(),
            std::chrono::duration<double,std::milli>(uploaded-sorted).count(), 1};
}

std::size_t GrassRenderer::count(std::size_t index) const {
    return index<patches_.size() ? patches_[index].near.count+patches_[index].far.count : 0;
}

void GrassRenderer::draw(std::size_t index) const {
    if (index>=patches_.size()) return;
    const bool culled=glIsEnabled(GL_CULL_FACE);
    glDisable(GL_CULL_FACE); // Two-sided blades, without duplicate geometry.
    const auto& patch=patches_[index];
    for (auto [batch,segments] : {std::pair{&patch.near,6},std::pair{&patch.far,1}}) {
        if (!batch->count) continue;
        shader.setInt("uSegments",segments);
        glBindVertexArray(batch->vao);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP,0,2*(segments+1),batch->count);
    }
    glBindVertexArray(0);
    if (culled) glEnable(GL_CULL_FACE);
}
} // namespace rendering

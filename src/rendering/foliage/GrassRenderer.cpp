#include "rendering/foliage/GrassRenderer.h"

namespace rendering {
void GrassRenderer::clear() { near.clear(); horizon.clear(); }
GrassPreparationStats GrassRenderer::prepare(std::size_t index,const Mesh& mesh,const config::PlanetConfig& planet,
    double metersPerWorldUnit,const glm::dvec3& eyeBody) {
    const auto a=near.prepare(index,mesh,planet,metersPerWorldUnit,eyeBody);
    const auto b=horizon.prepare(index,mesh,planet,metersPerWorldUnit,eyeBody);
    return {a.placementMs+b.placementMs,a.sortMs+b.sortMs,a.uploadMs+b.uploadMs,a.rebuilds+b.rebuilds,
        a.uploadedBytes+b.uploadedBytes};
}
std::size_t GrassRenderer::count(std::size_t index) const { return near.stats(index).candidates; }
GrassDrawStats GrassRenderer::drawStats(std::size_t index) const {
    const auto s=near.stats(index);
    // instanceBytes is the actual descriptor payload, not one record per root.
    return {s.candidates,s.vertices,s.triangles,s.batches,s.patchBytes};
}
void GrassRenderer::draw(std::size_t index,const GrassPass* pass) const { near.draw(index,pass); }
} // namespace rendering

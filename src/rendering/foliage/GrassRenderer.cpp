#include "rendering/foliage/GrassRenderer.h"

namespace rendering {
void GrassRenderer::clear() { procedural.clear(); }
GrassPreparationStats GrassRenderer::prepare(std::size_t index,const Mesh& mesh,const config::PlanetConfig& planet,
    double metersPerWorldUnit,const glm::dvec3& eyeBody) {
    return procedural.prepare(index,mesh,planet,metersPerWorldUnit,eyeBody);
}
std::size_t GrassRenderer::count(std::size_t index) const { return procedural.stats(index).candidates; }
GrassDrawStats GrassRenderer::drawStats(std::size_t index) const {
    const auto s=procedural.stats(index);
    // instanceBytes is the actual descriptor payload, not one record per root.
    return {s.candidates,s.vertices,s.triangles,s.batches,s.patchBytes};
}
void GrassRenderer::draw(std::size_t index,const GrassPass* pass) const { procedural.draw(index,pass); }
} // namespace rendering

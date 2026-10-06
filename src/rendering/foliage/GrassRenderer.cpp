#include "rendering/foliage/GrassRenderer.h"
#include "rendering/geometry/Mesh.h"
#include "rendering/geometry/jobs/TerrainBuild.h"

namespace rendering {
void GrassRenderer::clear() { procedural.clear(); }
GrassPreparationStats GrassRenderer::prepare(std::size_t index,const Mesh& mesh,const config::PlanetConfig& planet,
    double metersPerWorldUnit,const glm::dvec3& eyeBody,std::uint64_t otherTerrainBytes) {
    CpuGrassTrace trace;
    if(trace_ && index<serials_->size() && index<masks_->size() && index<water_->size()) {
        const auto& l=mesh.terrainStats.generation;const auto& water=(*water_)[index];const auto& w=water.terrainStats.generation;
        TerrainBuildIdentity k;k.epoch=*epoch_;k.serial=(*serials_)[index];k.bodyIndex=index;k.bodyName=planet.name;
        k.field=l.field;k.fieldVersion=l.fieldVersion;k.topologyVersion=l.topologyVersion;k.backend=l.backend;
        k.eye=eyeBody;k.localMask=(*masks_)[index];
        trace.owner=trace_;trace.key=PublicationProfiler::Key::from(k);
        trace.generation={l.field,l.topology,w.field,w.topology,mesh.revision,water.revision};
    }
    return procedural.prepare(index,mesh,planet,metersPerWorldUnit,eyeBody,otherTerrainBytes,trace.owner ? &trace : nullptr);
}
std::size_t GrassRenderer::count(std::size_t index) const { return procedural.stats(index).candidates; }
GrassDrawStats GrassRenderer::drawStats(std::size_t index) const {
    const auto s=procedural.stats(index);
    // instanceBytes is the actual descriptor payload, not one record per root.
    return {s.candidates,s.vertices,s.triangles,s.batches,s.patchBytes};
}
void GrassRenderer::draw(std::size_t index,const GrassPass* pass) const { procedural.draw(index,pass); }
} // namespace rendering

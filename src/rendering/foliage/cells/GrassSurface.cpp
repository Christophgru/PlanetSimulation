#include "rendering/foliage/procedural/ProceduralGrass.h"
#include "rendering/geometry/terrain/detail/TerrainMorph.h"
#include <stdexcept>

namespace rendering {
void ProceduralGrass::refreshSurface(const Patch& patch) const {
    const auto revision=patch.morphState?patch.morphState->revision():0;
    if(patch.surfaceRevision==revision) return;
    if(!surface_) throw std::logic_error("Resident grass surface program was not prepared");
    GLint groups=0;glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT,0,&groups);
    if(groups<1) throw std::runtime_error("Grass surface dispatch unavailable");
    auto& s=*surface_;s.use();s.setInt("uTerrainVertices",8);s.setInt("uTerrainIndices",9);
    s.setFloat("uScale",patch.scale);s.setFloat("uDensity",patch.density);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER,0,patch.buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER,1,patch.cells->cells);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER,2,patch.gpuBounds);
    const auto chunk=std::min<std::size_t>(std::size_t(groups)*64,65536);
    GpuWorkProfiler::Scope timing(GpuWorkStage::GrassCells,patch.workIdentity);
    for(std::size_t first=0;first<patch.patches;first+=chunk) {
        s.setInt("uFirst",first);s.setInt("uEnd",std::min(first+chunk,patch.patches));
        glDispatchCompute((std::min(chunk,patch.patches-first)+63)/64,1,1);
    }
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    if(glGetError()!=GL_NO_ERROR) throw std::runtime_error("Grass surface refresh failed");
    patch.surfaceRevision=revision;
    bindComputedInputs(patch);
}
}

#include "rendering/foliage/procedural/ProceduralGrass.h"
#include <algorithm>
#include <stdexcept>

namespace rendering {
void ProceduralGrass::bindComputedInputs(const Patch& patch) const {
    auto& c=*compute_;
    c.use();
    c.setInt("uCapacity",patch.gpuCapacity);
    c.setInt("uCellFirst",patch.cellKeyOffset);
    c.setInt("uTerrainVertices",8); c.setInt("uTerrainIndices",9);
    glActiveTexture(GL_TEXTURE8); glBindTexture(GL_TEXTURE_BUFFER,patch.vertexTexture);
    glActiveTexture(GL_TEXTURE9); glBindTexture(GL_TEXTURE_BUFFER,patch.indexTexture);
    glActiveTexture(GL_TEXTURE0);
    c.setFloat3("uPlanetColor",patch.color.x,patch.color.y,patch.color.z);
    c.setFloat3("uLandscapeLevels",patch.landscapeLevels.x,patch.landscapeLevels.y,patch.landscapeLevels.z);
    c.setFloat2("uTerrainRockRange",patch.rockRange.x,patch.rockRange.y);
    c.setInt("uLandscapeEnabled",patch.landscape); c.setInt("uWaterEnabled",patch.water);
    const auto& f=patch.settings;
    c.setFloat("uMetersPerRadius",patch.scale); c.setFloat("uDrawDistance",f.draw_distance_m);
    c.setFloat("uQuadDistance",f.quadDistanceMeters());
    c.setFloat("uPlacementDensity",patch.density);
    c.setInt("uAdaptiveFalloff",patch.falloff.enabled);
    c.setFloat("uGaussianSigma",patch.falloff.enabled?patch.falloff.sigmaMeters:f.draw_distance_m*f.gaussian_sigma_fraction);
    c.setFloat("uGrassHeight",f.height_m); c.setFloat("uGrassWidth",f.width_m);
    c.setFloat("uRootOffset",f.root_offset_m); c.setFloat("uGreenRatio",f.green_ratio);
    c.setFloat("uWaterClearance",f.water_clearance_m);
    c.setFloat2("uHeightMultiplierRange",f.height_multiplier_min,f.height_multiplier_max);
    c.setFloat2("uLeanRange",f.lean_min,f.lean_max);
    c.setInt("uGrassSeed",f.seed);
    c.setInt("uFrustumCull",f.frustum_culling);
    const double reach=f.height_m*f.height_multiplier_max+f.width_m+f.root_offset_m;
    c.setFloat("uCullExtent",reach/patch.scale);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER,0,patch.buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER,1,patch.commands);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER,2,patch.gpuBlades);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER,3,patch.gpuRoots);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER,4,patch.gpuBounds);
}
std::uint64_t ProceduralGrass::dispatchCandidates(const Patch& patch,int maxGroups) const {
    auto& c=*compute_;std::size_t first=0,root=0;std::uint64_t dispatches=0;
    for(std::size_t level=0;level<grassCandidateSlots.size();++level) {
        const auto candidates=patch.batches[level].count;
        if(candidates) {
            c.setInt("uFirstPatch",first);c.setInt("uFirstRoot",root);
            c.setInt("uCandidateCount",candidates);c.setInt("uSlotsPerPatch",grassCandidateSlots[level]);
            const auto chunk=std::min<std::size_t>(std::size_t(maxGroups)*128,65536);
            for(std::size_t offset=0;offset<std::size_t(candidates);offset+=chunk) {
                c.setInt("uCandidateOffset",offset);
                glDispatchCompute((std::min(chunk,std::size_t(candidates)-offset)+127)/128,1,1);
                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);++dispatches;
            }
        }
        first+=candidates/grassCandidateSlots[level];root+=candidates;
    }
    return dispatches;
}
void ProceduralGrass::generateRoots(const Patch& patch) const {
    if(!patch.gpuRoots) return;
    prepareCells(patch,nullptr,false);
    std::size_t level=0,offset=0;
    while(!generateRootsStep(patch,nullptr,level,offset,4)) {}
    // Cold CPU-planned path needs no GPU summary read in the interactive draw.
    std::size_t count=0;for(const auto& batch:patch.batches) count+=batch.count;
    patch.cells->totals={0,std::uint32_t(patch.patches),0,0,0,std::uint32_t(count),0,0};
    if(glGetError()!=GL_NO_ERROR) throw std::runtime_error("Grass root generation failed");
    patch.rootsReady=true;patch.animationTime=std::numeric_limits<float>::quiet_NaN();
}
std::vector<GrassRootRecord> ProceduralGrass::cachedRootsForValidation(std::size_t index) const {
    if(index>=patches_.size() || !patches_[index] || !patches_[index]->rootsReady) return {};
    const auto& patch=*patches_[index];std::size_t count=0;
    for(const auto& batch:patch.batches) count+=batch.count;
    std::vector<GrassRootRecord> records(count);GLint previous=0;
    glGetIntegerv(GL_COPY_READ_BUFFER_BINDING,&previous);glBindBuffer(GL_COPY_READ_BUFFER,patch.gpuRoots);
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    glGetBufferSubData(GL_COPY_READ_BUFFER,0,records.size()*sizeof(GrassRootRecord),records.data());
    glBindBuffer(GL_COPY_READ_BUFFER,previous);return records;
}
std::vector<GrassAnimationRecord> ProceduralGrass::cachedAnimationForValidation(std::size_t index) const {
    if(index>=patches_.size() || !patches_[index] || !patches_[index]->animationCapacity) return {};
    const auto& patch=*patches_[index];std::size_t count=0;
    for(const auto& batch:patch.batches) count+=batch.count;
    std::vector<GrassAnimationRecord> records(count);GLint previous=0;
    glGetIntegerv(GL_COPY_READ_BUFFER_BINDING,&previous);glBindBuffer(GL_COPY_READ_BUFFER,patch.gpuAnimation);
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    glGetBufferSubData(GL_COPY_READ_BUFFER,0,records.size()*sizeof(GrassAnimationRecord),records.data());
    glBindBuffer(GL_COPY_READ_BUFFER,previous);return records;
}
std::uint32_t ProceduralGrass::animationUpdatesForValidation(std::size_t index) const {
    if(index>=patches_.size() || !patches_[index] || !patches_[index]->commands) return 0;
    std::uint32_t updates=0;GLint previous=0;glGetIntegerv(GL_COPY_READ_BUFFER_BINDING,&previous);
    glBindBuffer(GL_COPY_READ_BUFFER,patches_[index]->commands);glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    glGetBufferSubData(GL_COPY_READ_BUFFER,44,sizeof(updates),&updates);
    glBindBuffer(GL_COPY_READ_BUFFER,previous);return updates;
}

}

#include "rendering/foliage/procedural/ProceduralGrass.h"
#include <algorithm>
#include <stdexcept>

namespace rendering {
GrassCellCache::~GrassCellCache() {
    for(auto buffer:{roots,bounds,cells,table,summary}) glDeleteBuffers(1,&buffer);
}
std::pair<std::shared_ptr<GrassCellCache>,bool> ProceduralGrass::previousCells(const Patch& next) const {
    for(const auto& old:patches_) {
        if(!old || !old->ready || !old->rootsReady || !old->cells || !old->cells->count ||
           old->bodyName!=next.bodyName || old->scale!=next.scale ||
           bool(old->cellKeyOffset)!=bool(next.cellKeyOffset) ||
           (!next.cellKeyOffset && old->generation!=next.generation)) continue;
        const auto& a=old->settings;const auto& b=next.settings;
        // These inputs affect immutable root/variation/biome records. Density,
        // slot budgets, LOD, dimensions, wind and visibility remain dynamic.
        const bool reuse=a.seed==b.seed && a.green_ratio==b.green_ratio &&
            a.water_clearance_m==b.water_clearance_m &&
            a.height_multiplier_min==b.height_multiplier_min && a.height_multiplier_max==b.height_multiplier_max &&
            a.lean_min==b.lean_min && a.lean_max==b.lean_max &&
            old->color==next.color && old->rockRange==next.rockRange &&
            old->landscapeLevels==next.landscapeLevels && old->water==next.water && old->landscape==next.landscape;
        return {old->cells,reuse};
    }
    return {};
}
void ProceduralGrass::prepareCells(const Patch& patch,const GrassCellCache* previous,bool reuse) const {
    if(!patch.gpuRoots) return;
    auto& cache=*patch.cells;cache.count=patch.patches;cache.tableSlots=grassCellTableSlots(cache.count);
    GLint64 block=0;glGetInteger64v(GL_MAX_SHADER_STORAGE_BLOCK_SIZE,&block);
    if(cache.count*sizeof(GrassCellRecord)>std::uint64_t(block) || cache.tableSlots*4>std::uint64_t(block))
        throw std::runtime_error("Grass cells exceed storage capacity");
    if(!cells_) cells_=std::make_unique<Shader>("shaders/foliage/cells.comp");
    const auto create=[&](GLuint& buffer,std::size_t bytes,bool clear) {
        if(!buffer) glGenBuffers(1,&buffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER,buffer);glBufferData(GL_SHADER_STORAGE_BUFFER,bytes,nullptr,GL_STATIC_DRAW);
        if(clear) {const std::uint32_t zero=0;glClearBufferData(GL_SHADER_STORAGE_BUFFER,GL_R32UI,GL_RED_INTEGER,GL_UNSIGNED_INT,&zero);}
    };
    create(cache.cells,cache.count*sizeof(GrassCellRecord),false);
    create(cache.table,cache.tableSlots*4,true);create(cache.summary,32,true);
    if(glGetError()!=GL_NO_ERROR) throw std::runtime_error("Grass cell allocation failed");
    bindComputedInputs(patch);
    const std::array<GLuint,7> buffers{patch.buffer,cache.cells,cache.table,previous?previous->cells:0,
        previous?previous->table:0,cache.summary,patch.gpuBounds};
    for(int i=0;i<7;++i) glBindBufferBase(GL_SHADER_STORAGE_BUFFER,i,buffers[i]);
    auto& c=*cells_;c.use();c.setInt("uTerrainVertices",8);c.setInt("uTerrainIndices",9);
    c.setInt("uCellFirst",patch.cellKeyOffset);c.setFloat("uScale",patch.scale);c.setFloat("uDensity",patch.density);
    c.setInt("uTableSlots",cache.tableSlots);c.setInt("uPreviousSlots",previous?previous->tableSlots:0);
    c.setInt("uCanReuse",reuse);
    GLint groups=0;glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT,0,&groups);
    if(groups<1) throw std::runtime_error("Grass cell dispatch unavailable");
    const auto chunk=std::min<std::size_t>(groups*64ull,65536);
    std::size_t first=0,root=0;
    GpuWorkProfiler::Scope timing(GpuWorkStage::GrassCells,patch.workIdentity);
    for(std::size_t level=0;level<grassCandidateSlots.size();++level) {
        const auto slots=grassCandidateSlots[level],n=patch.batches[level].count/slots;
        c.setInt("uFirstPatch",first);c.setInt("uFirstRoot",root);c.setInt("uSlots",slots);
        for(std::size_t offset=0;offset<std::size_t(n);offset+=chunk) {
            c.setInt("uOffset",offset);c.setInt("uEnd",std::min<std::size_t>(n,offset+chunk));
            glDispatchCompute((std::min(chunk,std::size_t(n)-offset)+63)/64,1,1);
        }
        first+=n;root+=patch.batches[level].count;
    }
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}
bool ProceduralGrass::generateRootsStep(const Patch& patch,const GrassCellCache* previous,
    std::size_t& level,std::size_t& offset,unsigned dispatchLimit) const {
    if(!patch.gpuRoots) return true;
    GLint groups=0;glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT,0,&groups);
    if(groups<1) throw std::runtime_error("Grass root dispatch unavailable");
    bindComputedInputs(patch);compute_->setInt("uReset",0);compute_->setInt("uBuildRoots",1);
    compute_->setInt("uCellMode",1);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER,6,patch.cells->cells);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER,7,previous?previous->roots:0);
    const auto chunk=std::min<std::size_t>(groups*128ull,65536);
    std::size_t first=0,root=0;
    for(std::size_t i=0;i<level;++i) {first+=patch.batches[i].count/grassCandidateSlots[i];root+=patch.batches[i].count;}
    GpuWorkProfiler::Scope timing(GpuWorkStage::GrassRoots,patch.workIdentity);
    for(unsigned dispatched=0;level<grassCandidateSlots.size();) {
        const auto n=std::size_t(patch.batches[level].count);
        if(offset<n) {
            if(dispatched==dispatchLimit) return false;
            const auto amount=std::min(chunk,n-offset);
            auto& c=*compute_;c.setInt("uFirstPatch",first);c.setInt("uFirstRoot",root);
            c.setInt("uSlotsPerPatch",grassCandidateSlots[level]);c.setInt("uCandidateCount",n);c.setInt("uCandidateOffset",offset);
            glDispatchCompute((amount+127)/128,1,1);glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
            offset+=amount;++dispatched;++patch.rootDispatches;
        } else {first+=n/grassCandidateSlots[level];root+=n;++level;offset=0;}
    }
    return true;
}
void ProceduralGrass::finishCells(const Patch& patch,const GrassCellCache* previous) const {
    if(!patch.cells) return;
    auto& cache=*patch.cells;GLint binding=0;glGetIntegerv(GL_COPY_READ_BUFFER_BINDING,&binding);
    glBindBuffer(GL_COPY_READ_BUFFER,cache.summary);glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    glGetBufferSubData(GL_COPY_READ_BUFFER,0,32,cache.totals.data());glBindBuffer(GL_COPY_READ_BUFFER,binding);
    patch.cellSummaryReadBytes+=32;
    const auto oldCount=previous?previous->count:0;
    if(cache.totals[6] || cache.totals[7]>oldCount ||
       cache.totals[0]+cache.totals[1]+cache.totals[2]!=cache.count)
        throw std::runtime_error("Invalid grass cell update summary");
    cache.totals[3]=oldCount-cache.totals[7];
}
std::vector<GrassCellRecord> ProceduralGrass::cachedCellsForValidation(std::size_t index) const {
    if(index>=patches_.size() || !patches_[index] || !patches_[index]->cells) return {};
    const auto& cache=*patches_[index]->cells;std::vector<GrassCellRecord> result(cache.count);
    GLint binding=0;glGetIntegerv(GL_COPY_READ_BUFFER_BINDING,&binding);
    glBindBuffer(GL_COPY_READ_BUFFER,cache.cells);glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    glGetBufferSubData(GL_COPY_READ_BUFFER,0,result.size()*sizeof(GrassCellRecord),result.data());
    glBindBuffer(GL_COPY_READ_BUFFER,binding);return result;
}
}

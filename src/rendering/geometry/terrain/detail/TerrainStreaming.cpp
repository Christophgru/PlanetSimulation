#include "rendering/geometry/Terrain.h"
#include "rendering/diagnostics/tracing/CpuTrace.h"
#include "rendering/geometry/contacts/SparseTerrainContacts.h"
#include <cmath>
#include <stdexcept>
#include <mutex>

namespace rendering {
struct TerrainSurface::DetailCache {
    std::mutex mutex;
    std::optional<TerrainTopology> parent;
    std::shared_ptr<const RefinementParent> adjacency;
    std::shared_ptr<const SparseTerrainContacts> contacts;
    glm::dvec3 eye{0};
    bool recordedParentNext=false;
    // The query cache owns its referenced immutable field through this optional;
    // worker snapshots can disappear without leaving a dangling field reference.
    std::optional<PlanetField> queryField;
    std::unique_ptr<TerrainQueryCache> queries;
};
std::shared_ptr<TerrainSurface::DetailCache> TerrainSurface::newDetailCache() {
    return std::make_shared<DetailCache>();
}
void TerrainSurface::prepareReplayParent(const glm::dvec3& parentEyeWorldUnits) const {
    if(!detailCache_) throw std::invalid_argument("Recorded parent requires streaming terrain");
    (void)streamDetail(parentEyeWorldUnits/radius_,true);
}
TerrainTopology TerrainSurface::streamDetail(const glm::dvec3& eye,bool restoreParent) const {
    CpuTrace::Scope scope("terrain.stream_detail");
    if(!std::isfinite(glm::length(eye)) || glm::length(eye)<=0)
        throw std::invalid_argument("Invalid streaming terrain eye");
    // One parent shell shared by worker snapshots. Replacement drops the previous
    // cache rather than accumulating a trail of detailed mesh generations.
    std::lock_guard lock(detailCache_->mutex);
    const bool recorded=detailCache_->recordedParentNext;
    detailCache_->recordedParentNext=false;
    const bool local=glm::length(eye)<3;
    const double movement=!detailCache_->parent?0:glm::length(glm::normalize(eye)-glm::normalize(detailCache_->eye))*radius_*metersPerUnit_;
    // The fixed fine-field policy must still cover the whole moving core.
    // A ten-metre minimum alone can carry a short near band past its finest
    // footprint, silently filtering/sinking vertices inside that core.
    const double reuseDistance=.5*(lod_.near_surface_distance_m-lod_.local_detail_radius_m);
    const bool reuse=!restoreParent && detailCache_->parent && local==(glm::length(detailCache_->eye)<3) &&
        (recorded || !local || movement<reuseDistance);
    if(!reuse) {
        auto coarse=lod_;coarse.local_detail_radius_m=0;coarse.local_detail_streaming=false;
        const int minimum=320*3*coarse.base_edge_segments*(2*std::max(1,(coarse.base_edge_segments+1)/2)-1);
        coarse.max_triangle_budget=std::max({10000,minimum,lod_.max_triangle_budget/2});
        // Preserve the complete field (landscape/material included).
        auto source=*this;source.lod_=coarse;source.detailCache_.reset();
        auto parent=source.buildTopologyForEye(eye*radius_,{});
        auto adjacency=local?prepareRefinementParent(parent,radius_*metersPerUnit_):nullptr;
        auto contacts=local?std::make_shared<SparseTerrainContacts>(field_,parent):nullptr;
        // Publish the pair only after both allocations/builds succeed. A failed
        // rebase must not associate an older graph with a replacement shell.
        detailCache_->parent=std::move(parent);detailCache_->adjacency=std::move(adjacency);
        detailCache_->contacts=std::move(contacts);
        detailCache_->eye=eye;
        detailCache_->queries.reset();detailCache_->queryField.reset();
    }
    auto result=*detailCache_->parent;result.parentReused=reuse;
    for(int j=0;j<3;++j) result.parentEye[j]=detailCache_->eye[j]*radius_;
    result.parentPolicy=result.surfacePolicy;
    if(reuse) result.planningQueries={};
    std::vector<unsigned> contactParents;
    if(local) {
        // Fine field weights stay planet-fixed for the parent's lifetime.
        // The geometric core/transition still follows the actual eye. At the
        // fully morphed boundary the original parent supplies the whole surface.
        result.surfacePolicy.spacing[7]=lod_.local_edge_m;
        for(int i=6;i>=0;--i) result.surfacePolicy.spacing[i]=std::max(result.surfacePolicy.spacing[i],result.surfacePolicy.spacing[i+1]);
        if(!reuse || !detailCache_->queries) {
            detailCache_->queryField=field_;
            detailCache_->queries=std::make_unique<TerrainQueryCache>(*detailCache_->queryField,
                2*lod_.max_triangle_budget,result.surfacePolicy);
        }
        refineSurfaceError(result,eye,true,detailCache_->queries.get(),detailCache_->adjacency.get(),&contactParents);
    } else {
        detailCache_->queries.reset();detailCache_->queryField.reset();
        result.morph.resize(result.samples.size());
        for(std::size_t i=0;i<result.samples.size();++i) result.morph[i].parent.fill(result.samples[i]);
    }
    {CpuTrace::Scope canonical("terrain.detail_canonicalize");result.canonicalize(field_.fingerprint());}
    if(local) {
        result.contactParent_=detailCache_->contacts;
        result.contactParentFaces_=std::move(contactParents);
        result.contactTopology_=result.generation.topology;
    }
    // Recorded anchors predate cache-distance refinements. Honor them for one
    // capture plan; ordinary subsequent planning regains the safe reuse bound.
    if(restoreParent) detailCache_->recordedParentNext=true;
    return result;
}
}

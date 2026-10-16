#include "rendering/geometry/Terrain.h"
#include <cmath>
#include <stdexcept>
#include <mutex>

namespace rendering {
struct TerrainSurface::DetailCache {
    std::mutex mutex;
    std::optional<TerrainTopology> parent;
    glm::dvec3 eye{0};
    // The query cache owns its referenced immutable field through this optional;
    // worker snapshots can disappear without leaving a dangling field reference.
    std::optional<PlanetField> queryField;
    std::unique_ptr<TerrainQueryCache> queries;
};
std::shared_ptr<TerrainSurface::DetailCache> TerrainSurface::newDetailCache() {
    return std::make_shared<DetailCache>();
}
TerrainTopology TerrainSurface::streamDetail(const glm::dvec3& eye) const {
    if(!std::isfinite(glm::length(eye)) || glm::length(eye)<=0)
        throw std::invalid_argument("Invalid streaming terrain eye");
    // One parent shell shared by worker snapshots. Replacement drops the previous
    // cache rather than accumulating a trail of detailed mesh generations.
    std::lock_guard lock(detailCache_->mutex);
    const bool local=glm::length(eye)<3;
    const double movement=!detailCache_->parent?0:glm::length(glm::normalize(eye)-glm::normalize(detailCache_->eye))*radius_*metersPerUnit_;
    const bool reuse=detailCache_->parent && local==(glm::length(detailCache_->eye)<3) &&
        (!local || movement<std::max(10.0,lod_.near_surface_distance_m*.5));
    if(!reuse) {
        auto coarse=lod_;coarse.local_detail_radius_m=0;coarse.local_detail_streaming=false;
        const int minimum=320*3*coarse.base_edge_segments*(2*std::max(1,(coarse.base_edge_segments+1)/2)-1);
        coarse.max_triangle_budget=std::max({10000,minimum,lod_.max_triangle_budget/2});
        // Preserve the complete field (landscape/material included).
        auto source=*this;source.lod_=coarse;source.detailCache_.reset();
        detailCache_->parent=source.buildTopologyForEye(eye*radius_,{});
        detailCache_->eye=eye;
    }
    auto result=*detailCache_->parent;result.parentReused=reuse;
    for(int j=0;j<3;++j) result.parentEye[j]=detailCache_->eye[j]*radius_;
    result.parentPolicy=result.surfacePolicy;
    if(reuse) result.planningQueries={};
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
        refineSurfaceError(result,eye,true,detailCache_->queries.get());
    } else {
        detailCache_->queries.reset();detailCache_->queryField.reset();
        result.morph.resize(result.samples.size());
        for(std::size_t i=0;i<result.samples.size();++i) result.morph[i].parent.fill(result.samples[i]);
    }
    result.canonicalize(field_.fingerprint());
    return result;
}
}

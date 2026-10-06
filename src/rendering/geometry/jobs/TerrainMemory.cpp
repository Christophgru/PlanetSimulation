#include "rendering/geometry/jobs/TerrainBuildScheduler.h"
namespace rendering {
namespace {
template<class T> std::uint64_t bytes(const std::vector<T>& v) {return v.capacity()*sizeof(T);}
std::uint64_t geometryBytes(const TerrainGeometry& g) {
    return bytes(g.vertices)+bytes(g.indices)+bytes(g.lodSinkMeters)+bytes(g.faceZones);
}
std::uint64_t topologyBytes(const TerrainTopology& t) {
    return bytes(t.samples)+bytes(t.indices)+bytes(t.planningPositions)+bytes(t.faceZones);
}
}
std::optional<TerrainMemorySnapshot> TerrainBuildScheduler::memorySnapshot() const {
    std::unique_lock lock(mutex_,std::try_to_lock);if(!lock.owns_lock()) return {};
    TerrainMemorySnapshot result;result.running=bool(running_);result.queued=bool(queued_);result.ready=bool(ready_);
    if(ready_ && ready_->build) {
        const auto& b=*ready_->build;result.readyVectorBytes=geometryBytes(b.geometry);
        if(b.water) result.readyVectorBytes+=geometryBytes(*b.water);
        if(b.topology) result.readyVectorBytes+=topologyBytes(*b.topology);
        if(b.waterTopology) result.readyVectorBytes+=topologyBytes(*b.waterTopology);
    }
    // Fields, sparse contacts, queued config and running worker scratch are
    // excluded; process RSS independently includes allocator and worker pages.
    return result;
}
}

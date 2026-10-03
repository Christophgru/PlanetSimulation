#pragma once
#include "rendering/geometry/terrain/TerrainTopology.h"
#include <memory>
#include <unordered_map>

namespace rendering {
struct SparseContactStats {
    std::uint64_t queries=0, nodesVisited=0, candidateTriangles=0, heightEvaluations=0;
    std::size_t residentPositions=0, topologyBytes=0, indexBytes=0;
    double buildMilliseconds=0;
};

// Owns only immutable radial topology/field plus a bounded, lazy position cache.
// The index finds triangles for any radial query, so a fast step or teleport can
// extend contact coverage synchronously without a full shaped mesh or GPU read.
// Used on the simulation thread; positions follow the render oracle's float/sink
// order. The generation must be installed with its matching terrain buffers.
class SparseTerrainContacts {
public:
    static constexpr std::size_t positionCapacity=1024;
    SparseTerrainContacts(PlanetField field,TerrainTopology topology);
    const TerrainGenerationKey& generation() const { return generation_; }
    double radiusMeters() const { return field_.radiusWorld()*field_.metersPerUnit(); }
    std::vector<unsigned> candidates(const glm::dvec3& unitRadial);
    std::array<glm::dvec3,3> triangle(unsigned id);
    SparseContactStats stats() const;
private:
    struct Node {
        glm::dvec3 low{0},high{0};
        unsigned first=0,count=0,left=0,right=0;
    };
    unsigned build(unsigned first,unsigned count);
    bool intersects(const Node& node,const glm::dvec3& radial) const;
    glm::vec3 position(unsigned id);
    PlanetField field_;
    TerrainTopology topology_;
    TerrainGenerationKey generation_;
    std::vector<unsigned> order_;
    std::vector<Node> nodes_;
    std::unordered_map<unsigned,glm::vec3> positions_;
    SparseContactStats stats_;
};
}

#pragma once
#include "rendering/geometry/Terrain.h"
#include <memory>
#include <string>

namespace rendering {
class SparseTerrainContacts;
struct TerrainBuildIdentity {
    std::uint64_t epoch=1,serial=0,field=0;
    std::size_t bodyIndex=0;
    std::string bodyName;
    std::uint32_t fieldVersion=PlanetField::version,topologyVersion=1;
    TerrainBackend backend=TerrainBackend::Cpu;
    bool resident=false;
    glm::dvec3 eye{0};
    int localMask=0;
    bool operator==(const TerrainBuildIdentity&) const = default;
};
// A worker owns value snapshots. No GL handles or borrowed scene data.
struct TerrainBuildRequest {
    TerrainBuildIdentity identity;
    TerrainSurface surface;
    config::PlanetConfig planet;
    std::vector<int> previousFaceZones;
    double metersPerUnit=1;
    void validate() const;
};
struct TerrainCpuBuild {
    TerrainGeometry geometry;
    std::optional<TerrainGeometry> water;
    std::optional<TerrainTopology> topology,waterTopology;
    std::optional<PlanetField> field,waterField;
    std::shared_ptr<SparseTerrainContacts> contacts;
    double milliseconds=0;
};
TerrainCpuBuild buildTerrainCpu(const TerrainBuildRequest& request);
// Eye motion may require a follow-up without starving a useful completed anchor.
// Epoch/body/field/mode mismatches and older installed serials never publish.
bool terrainBuildMatches(const TerrainBuildIdentity& completed,
    const TerrainBuildIdentity& current,std::uint64_t installedSerial);
}

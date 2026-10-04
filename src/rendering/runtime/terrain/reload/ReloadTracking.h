#pragma once
#include "rendering/runtime/ScenePass.h"
#include "rendering/geometry/jobs/TerrainBuild.h"

namespace rendering {
// Allocate every tracking slot/face-zone copy before whole-scene publication.
struct ReloadTracking {
    std::vector<bool> ready;
    std::vector<int> masks,triangles,steep;
    std::vector<std::vector<int>> zones;
    std::vector<glm::dvec3> eyes;
    std::vector<std::array<int,3>> zoneCounts;
    std::vector<std::uint64_t> serials;
    std::vector<std::optional<TerrainBuildIdentity>> failures;
    std::vector<SceneTerrainConsumers> consumers;
    explicit ReloadTracking(std::size_t count):ready(count,true),masks(count),triangles(count),steep(count),
        zones(count),eyes(count),zoneCounts(count),serials(count),failures(count),consumers(count) {}
    void record(std::size_t i,const TerrainCpuBuild& b,const TerrainBuildIdentity& k) {
        zones[i]=b.geometry.faceZones;zoneCounts[i]=b.geometry.zoneFaces;
        steep[i]=b.geometry.steepRefinedFaces;masks[i]=k.localMask;eyes[i]=k.eye;serials[i]=k.serial;
    }
    template<class State> void exchange(State& r) noexcept {
        ready.swap(r.meshReady);masks.swap(r.lastLocalMask);triangles.swap(r.meshTriangles);
        steep.swap(r.meshSteepRefinedFaces);zones.swap(r.lastFaceZones);eyes.swap(r.lastTerrainEyes);
        zoneCounts.swap(r.meshZoneFaces);serials.swap(r.installedTerrainSerial);failures.swap(r.terrainFailures);
        consumers.swap(r.terrainConsumers);
    }
};
}

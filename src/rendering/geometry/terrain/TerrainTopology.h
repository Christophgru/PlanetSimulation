#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
#include "rendering/geometry/terrain/TerrainQueryCache.h"
namespace rendering {
enum class TerrainBackend : std::uint32_t { Cpu=1 };
struct TerrainGenerationKey {
    std::uint64_t field=0,topology=0;
    std::uint32_t fieldVersion=PlanetField::version,topologyVersion=1;
    TerrainBackend backend=TerrainBackend::Cpu;
    bool operator==(const TerrainGenerationKey&) const = default;
};
struct TerrainBuildStats {
    std::array<int,3> zoneFaces{};
    std::array<int,8> lodFaces{};
    std::vector<int> faceZones;
    int steepRefinedFaces=0,shorelineAddedTriangles=0;
    int coarseNoiseSamples=0,fineNoiseSamples=0;
    TerrainQueryStats planningQueries{},evaluationQueries{};
    std::size_t topologyInputBytes=0,uniqueSamples=0;
    TerrainGenerationKey generation{};
};
struct alignas(32) TerrainInputSample {
    std::array<double,3> radial{};
    double sinkMeters=0;
    bool operator==(const TerrainInputSample&) const = default;
};
static_assert(sizeof(TerrainInputSample)==32);
static_assert(offsetof(TerrainInputSample,sinkMeters)==24);
// Stable first-use canonical IDs and triangle order. CPU evaluation expands
// corners for the old mesh/grass/replay layout; future GPU output can stay indexed.
struct TerrainTopology : TerrainBuildStats {
    std::vector<TerrainInputSample> samples;
    std::vector<std::uint32_t> indices;
    // CPU planning scratch for legacy float-rounded shoreline decisions;
    // discarded before publishing or uploading the radial contract.
    std::vector<glm::vec3> planningPositions;
    void canonicalize(std::uint64_t fieldFingerprint);
    void validate() const;
    std::uint64_t contentFingerprint() const;
    int triangleCount() const { return static_cast<int>(indices.size()/3); }
};
std::uint64_t terrainHashWord(std::uint64_t hash,std::uint64_t word,int bytes=8);
}

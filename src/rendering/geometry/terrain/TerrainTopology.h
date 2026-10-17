#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>
#include "rendering/geometry/terrain/TerrainQueryCache.h"
namespace rendering {
class SparseTerrainContacts;
inline bool supportedTerrainTopology(std::uint32_t version) { return version>=1 && version<=4; }
enum class TerrainBackend : std::uint32_t { Cpu=1, Compute=2 };
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
    int errorRefinedTriangles=0;
    double remainingErrorRatio=0;
    bool errorBudgetLimited=false;
    int localDetailTriangles=0;
    bool parentReused=false;
    std::size_t refinementParentEdges=0,refinementMutableEdges=0;
    std::array<double,3> parentEye{};
    double localMaxEdgeMeters=0,localRemainingErrorRatio=0;
    bool localDetailLimited=false;
    TerrainQueryStats planningQueries{},evaluationQueries{};
    std::size_t topologyInputBytes=0,uniqueSamples=0;
    std::uint64_t gpuInputBytes=0,gpuWorkingBytes=0,gpuDispatches=0,gpuCorners=0;
    std::uint64_t gpuCellKeys=0;
    std::uint64_t gpuStageAdmittedBytes=0;
    double gpuMilliseconds=0;
    TerrainGenerationKey generation{};
};
struct alignas(32) TerrainInputSample {
    std::array<double,3> radial{};
    double sinkMeters=0;
    bool operator==(const TerrainInputSample&) const = default;
};
static_assert(sizeof(TerrainInputSample)==32);
static_assert(offsetof(TerrainInputSample,sinkMeters)==24);
struct alignas(32) TerrainMorphSample {
    std::array<TerrainInputSample,3> parent;
    std::array<double,4> blend{}; // parent weight; remaining words reserved
};
static_assert(sizeof(TerrainMorphSample)==128);
// Stable first-use canonical IDs and triangle order. CPU evaluation expands
// corners for the old mesh/grass/replay layout; future GPU output can stay indexed.
struct TerrainTopology : TerrainBuildStats {
    TerrainSurfacePolicy surfacePolicy, parentPolicy;
    std::vector<TerrainMorphSample> morph; // Empty for historical v1-v3.
    std::uint32_t version() const { return morph.empty()?surfacePolicy.version():4; }
    std::vector<TerrainInputSample> samples;
    std::vector<std::uint32_t> indices;
    // CPU planning scratch for legacy float-rounded shoreline decisions;
    // discarded before publishing or uploading the radial contract.
    std::vector<glm::vec3> planningPositions;
    void canonicalize(std::uint64_t fieldFingerprint);
    void validate() const;
    std::uint64_t contentFingerprint() const;
    int triangleCount() const { return static_cast<int>(indices.size()/3); }
private:
    // Derived CPU acceleration only; excluded from the render/replay contract.
    // Canonicalization invalidates it, and consumers check the sealed hash.
    friend class TerrainSurface;
    friend class SparseTerrainContacts;
    std::shared_ptr<const SparseTerrainContacts> contactParent_;
    std::vector<unsigned> contactParentFaces_;
    std::uint64_t contactTopology_=0;
    void validateStructure() const;
};
// Validated, owned and immutable after construction. Staging may copy this
// handle without repeating the full fingerprint scan on the context thread.
// Mutable TerrainTopology entry points continue to validate their inputs.
class TerrainTopologySnapshot {
public:
    TerrainTopologySnapshot()=default;
    explicit TerrainTopologySnapshot(TerrainTopology topology);
    explicit operator bool() const {return bool(topology_);}
    const TerrainTopology& operator*() const {return *topology_;}
    const TerrainTopology* operator->() const {return topology_.get();}
    void reset() {topology_.reset();}
private:
    friend class SparseTerrainContacts;
    // The contact oracle already validated its private topology. An aliasing
    // owner keeps that oracle and its immutable descriptors alive together.
    explicit TerrainTopologySnapshot(std::shared_ptr<const TerrainTopology> topology)
        :topology_(std::move(topology)) {}
    std::shared_ptr<const TerrainTopology> topology_;
};
std::uint64_t terrainHashWord(std::uint64_t hash,std::uint64_t word,int bytes=8);
}

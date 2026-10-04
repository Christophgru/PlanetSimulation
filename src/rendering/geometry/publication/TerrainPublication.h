#pragma once
#include "rendering/geometry/compute/TerrainGpuPreparation.h"
#include "rendering/geometry/Mesh.h"
#include "rendering/foliage/procedural/ProceduralGrass.h"

namespace rendering {
// The renderer uses this receipt to refresh contact/shadow/frame consumers at
// its frame boundary. Grass's planning eye is independent of the terrain eye.
struct TerrainPublishedGeneration {
    TerrainBuildIdentity identity;
    TerrainGenerationKey land,water;
    std::uint64_t landRevision=0,waterRevision=0,admittedBytes=0;
    glm::dvec3 grassEye{0};
    config::FoliageConfig foliage;
    std::vector<int> faceZones;
    std::array<int,3> zoneFaces{};
    int triangles=0,steepRefinedFaces=0;
    bool waterEnabled=false;
};
struct TerrainPublicationStats {
    std::uint64_t submitted=0,published=0,failed=0,obsolete=0,retired=0,peakReservedBytes=0;
    std::uint64_t grassOnlyPublished=0;
};
// Context-thread transaction, with one global preparation and one spare per
// body. Caller owns the live meshes and grass and keeps them/context alive.
// Capture uses this at its frame boundary; interactive opt-in remains gated.
class TerrainPublication {
    struct Stage;
    struct Body {
        TerrainPublishedGeneration installed;
        std::unique_ptr<Stage> retiring;
    };
    ProceduralGrass& grass_;
    std::vector<Body> bodies_;
    std::unique_ptr<Stage> staged_;
    std::uint64_t totalLimit_,perSetLimit_,externalBytes_=0;
    bool sceneReplacementOwned_=false;
    TerrainPublicationStats stats_;
public:
    explicit TerrainPublication(ProceduralGrass& grass,std::size_t bodies,
        std::uint64_t totalLimit=2*ProceduralGrass::defaultStageBytes,
        std::uint64_t perSetLimit=ProceduralGrass::defaultStageBytes);
    ~TerrainPublication();
    TerrainPublication(const TerrainPublication&)=delete;
    TerrainPublication& operator=(const TerrainPublication&)=delete;
    bool canSubmit(std::size_t index) const noexcept;
    // Busy returns false; errors discard only the new staging resources.
    bool submit(TerrainCpuBuild build,const TerrainBuildIdentity& request,
        const config::PlanetConfig& planet,double metersPerUnit,const glm::dvec3& grassEye,
        const Mesh& liveLand,const Mesh& liveWater,TerrainCompute& compute);
    bool submitGrass(std::size_t index,const config::PlanetConfig& planet,double metersPerUnit,
        const glm::dvec3& grassEye,const Mesh& liveLand,const Mesh& liveWater);
    bool poll();
    void waitForCapture();
    // Returns false for stale work. All validation/fence creation precedes the
    // no-throw land/water/grass/contact/receipt exchange. Old resources retire
    // together after the last-use fence signals.
    bool publish(const TerrainBuildIdentity& current,Mesh& liveLand,Mesh& liveWater);
    void cancel() noexcept;
    void pollRetired();
    void waitRetiredForCapture(std::size_t index);
    bool pending() const noexcept {return bool(staged_);}
    bool ready() const noexcept;
    bool retiring(std::size_t index) const;
    std::uint64_t reservedBytes() const noexcept;
    // A replacement scene reserves the old scene before its first dispatch.
    // Clear only after the old scene's last-use retirement fence signals.
    void reserveExternal(std::uint64_t bytes);
    bool acquireSceneReplacement() noexcept;
    void releaseSceneReplacement() noexcept {sceneReplacementOwned_=false;}
    std::uint64_t externalBytes() const noexcept {return externalBytes_;}
    // Caller exchanges the matching grass state and meshes at the same boundary.
    // Cancels unfinished old work; grass references retain their stable owners.
    void swapInstalledState(TerrainPublication& other) noexcept;
    const TerrainPublishedGeneration& installed(std::size_t index) const;
    const TerrainPublicationStats& stats() const noexcept {return stats_;}
};
}

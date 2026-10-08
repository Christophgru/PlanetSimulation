#pragma once
#include "config/Config.h"
#include "app/scene/PreparedScene.h"
#include "rendering/geometry/publication/TerrainPublication.h"

namespace rendering {
// Borrowed live destinations. Their owner/context must outlive the transaction.
struct SceneTerrainDestination {
    app::PreparedScene& scene;
    nlohmann::json& document;
    std::vector<Mesh>& land;
    std::vector<Mesh>& water;
    ProceduralGrass& grass;
    TerrainPublication& publication;
    std::uint64_t& epoch;
};
// One complete replacement, then one retired scene. CPU topology is supplied by
// the existing scheduler; this context-thread owner never starts another worker.
// Renderer integration and interactive acceptance remain separate checkpoints.
class SceneTerrainReplacement {
    struct LiveBody {
        GLuint land=0,water=0,landVao=0,waterVao=0,landEbo=0,waterEbo=0;
        std::uint64_t landRevision=0,waterRevision=0;
        TerrainGenerationKey landKey,waterKey;
        const SparseTerrainContacts* contacts=nullptr;
        std::string name;
        std::uint64_t field=0;
        std::optional<TerrainGenerationKey> grassKey;
        std::optional<std::uint64_t> grassRevision;
        std::optional<glm::dvec3> grassEye;
    };
    nlohmann::json previousDocument_;
    nlohmann::json document_;
    app::PreparedScene scene_;
    std::vector<Mesh> land_,water_;
    ProceduralGrass grass_;
    std::unique_ptr<TerrainPublication> publication_;
    std::vector<std::optional<TerrainBuildIdentity>> requests_;
    std::vector<LiveBody> previous_;
    SceneTerrainDestination live_;
    std::uint64_t epoch_,previousEpoch_;
    std::optional<std::size_t> preparing_;
    GLsync retirementFence_=nullptr;
    bool published_=false,retired_=false;
    void validateLive() const;
public:
    SceneTerrainReplacement(nlohmann::json document,SceneTerrainDestination live,
        std::uint64_t epoch,double simulationTime,
        std::uint64_t aggregateLimit=2*ProceduralGrass::defaultStageBytes);
    SceneTerrainReplacement(nlohmann::json document,app::PreparedScene prepared,
        SceneTerrainDestination live,std::uint64_t epoch,double simulationTime,
        std::uint64_t aggregateLimit=2*ProceduralGrass::defaultStageBytes);
    ~SceneTerrainReplacement();
    SceneTerrainReplacement(const SceneTerrainReplacement&)=delete;
    SceneTerrainReplacement& operator=(const SceneTerrainReplacement&)=delete;
    TerrainBuildRequest request(std::size_t index,const glm::dvec3& worldEye,std::uint64_t serial,
        std::vector<int> previousZones={});
    TerrainBuildRequest requestLocal(std::size_t index,const glm::dvec3& localEye,std::uint64_t serial,
        std::vector<int> previousZones={});
    void restorePolicies(const nlohmann::json& replay) {grass_.restorePolicies(replay,scene_.scenario.planets);}
    bool submit(TerrainCpuBuild build,const TerrainBuildIdentity& identity,
        const glm::dvec3& grassEye,TerrainCompute& compute);
    bool poll(); // Zero-timeout GPU polls; only exchanges off-live body consumers.
    bool replanGrass(std::size_t index,const glm::dvec3& eye);
    bool preparing() const noexcept {return preparing_.has_value();}
    void waitForCapture();
    bool ready() const noexcept;
    // False for a superseded epoch. All checks/fence allocation precede exchange.
    bool publish(std::uint64_t currentEpoch);
    bool pollRetired(); // Caller keeps this owner alive until true.
    void waitRetiredForCapture();
    std::uint64_t reservedBytes() const noexcept {return publication_ ? publication_->reservedBytes() : 0;}
    const app::PreparedScene& scene() const noexcept {return scene_;}
    const Mesh& land(std::size_t i) const {return land_.at(i);}
    const Mesh& water(std::size_t i) const {return water_.at(i);}
    const ProceduralGrass& grass() const noexcept {return grass_;}
};
}

#pragma once
#include "rendering/diagnostics/timing/GpuWorkProfiler.h"
#include "rendering/diagnostics/timing/PublicationProfiler.h"
#include "rendering/Shader.h"
#include "rendering/foliage/GrassStats.h"
#include "rendering/foliage/procedural/GrassPlan.h"
#include "config/FoliageConfig.h"
#include "rendering/foliage/planning/GrassBudget.h"
#include "rendering/foliage/cells/GrassCellCache.h"
#include <memory>
#include <map>
#include <optional>
#include <limits>
#include "rendering/foliage/trails/GrassTrail.h"
#include "rendering/geometry/terrain/TerrainTopology.h"
#include "rendering/geometry/compute/TerrainCompute.h"
class Mesh;
namespace rendering {
class TerrainMorphState;
class GrassMetadataCompute;
class GrassAllocationCompute;
struct CpuGrassTrace {
    PublicationProfiler* owner=nullptr;
    PublicationProfiler::Key key;
    PublicationProfiler::Generation generation;
};
struct GrassAllocationBuffers;
struct GrassMetadataBuffers;
// std430 static candidate record; generated on GPU before resident publication.
struct GrassRootRecord { glm::vec4 rootExpected,upRank,normalSlot,variation; };
static_assert(sizeof(GrassRootRecord)==64);
struct GrassAnimationRecord { glm::vec3 wind;std::uint32_t frame;glm::vec4 trail; };
static_assert(sizeof(GrassAnimationRecord)==32);
inline constexpr std::uint64_t grassComputedCandidateBytes=128+sizeof(GrassRootRecord)+sizeof(GrassAnimationRecord);
inline constexpr std::uint64_t grassComputedCommandBytes=48;
struct GrassPass {
    glm::mat4 model{1}, view{1}, projection{1};
    glm::dvec3 mainEyeBody{0};
    float windTime=0;
    bool feedback=false; // Test probes pause feedback during compute dispatch.
    GpuWorkView timingView=GpuWorkView::Unspecified; // Diagnostics only.
};
struct ProceduralGrassStats {
    std::size_t candidates=0, patches=0, vertices=0, triangles=0, batches=0, patchBytes=0;
    double distanceMeters=0,density=0;
    std::uint64_t allocationBudget=0;
    std::size_t gpuBytes=0;
    std::uint64_t metadataBytes=0, metadataInputBytes=0, metadataDispatches=0, metadataReadBytes=0;
    std::uint64_t allocationBytes=0,allocationInputBytes=0,allocationDispatches=0,summaryReadBytes=0;
    std::uint64_t drawResourceBytes=0,stageAdmittedBytes=0;
    bool drawResourcesPrepared=false;
    std::uint64_t rootBytes=0,rootDispatches=0;
    bool rootsPrepared=false;
    std::uint64_t animationBytes=0,animationDispatches=0;
    std::uint32_t animationFrame=0;
    std::uint64_t cellBytes=0,cellSummaryReadBytes=0;
    std::array<std::uint32_t,8> cellUpdates{};
};
class ProceduralGrass {
public:
    class Preparation;
private:
    friend class TerrainPublication;
    void commitPrepared(std::size_t index,Preparation& preparation,const Mesh& mesh) noexcept;
    struct Batch { GLuint vao=0; GLsizei count=0; };
    struct Draw { std::size_t first=0, patches=0; int level=0, segments=1; };
    struct Bound { glm::dvec3 center; double radius=0; };
    struct Patch {
        ~Patch();
        std::array<Batch,grassCandidateSlots.size()> batches{};
        std::vector<Bound> bounds;
        std::vector<Draw> draws;
        GLuint buffer=0, vertexTexture=0, indexTexture=0;
        TerrainMorphSource morphSource;
        std::shared_ptr<const TerrainMorphState> morphState;
        mutable std::uint64_t surfaceRevision=std::numeric_limits<std::uint64_t>::max();
        mutable std::uint64_t animationMorphRevision=0;
        std::array<GLuint,2> morphTextures{};
        std::unique_ptr<GrassMetadataBuffers> metadata;
        std::unique_ptr<GrassAllocationBuffers> allocation;
        mutable GLuint gpuBlades=0, commands=0;
        mutable GLuint gpuRoots=0,gpuBounds=0;
        mutable std::shared_ptr<GrassCellCache> cells;
        std::string bodyName;
        std::size_t cellKeyOffset=0;
        mutable std::uint64_t cellSummaryReadBytes=0;
        mutable std::size_t rootCapacity=0,boundsCapacity=0;
        mutable bool rootsReady=false;
        mutable std::uint64_t rootDispatches=0;
        mutable GLuint gpuAnimation=0,animationTexture=0;
        mutable std::size_t animationCapacity=0;
        mutable std::uint32_t animationFrame=0;
        mutable float animationTime=std::numeric_limits<float>::quiet_NaN();
        mutable std::uint64_t animationTrailRevision=0,animationDispatches=0;
        mutable std::array<GLuint,2> gpuVaos{};
        mutable std::size_t gpuCapacity=0;
        mutable bool computeUsed=false;
        GrassFalloff falloff;
        std::uint64_t policyRevision=0;
        config::FoliageConfig settings;
        glm::vec3 color{1}, landscapeLevels{0};
        glm::vec2 rockRange{0};
        bool landscape=false, water=false;
        double scale=1;
        std::size_t patches=0;
        std::uint64_t allocationBudget=0;
        std::uint64_t stageAdmittedBytes=0;
        glm::dvec3 eye{0};
        std::uint64_t revision=0;
        TerrainGenerationKey generation;
        GpuWorkIdentity workIdentity;
        double distanceMeters=0;
        double density=0;
        int seed=0;
        bool ready=false;
    };
    std::vector<std::unique_ptr<Patch>> patches_;
    struct TrailData {
        GrassTrail history;
        mutable GLuint buffer=0, texture=0;
        mutable std::uint64_t uploadedRevision=0;
        mutable glm::dvec3 origin{0};
        mutable int nodes=0;
    };
    std::map<std::size_t,TrailData> trails_;
    std::map<std::size_t,glm::dvec3> replayPlanEyes_;
    bool adaptiveBudget_=false;
    GrassBudgetController budget_;
    std::uint64_t terrainBudgetBytes_=0,externalBudgetBytes_=0;
    std::map<std::string,GrassFalloff> replayPolicies_;
    void bindTrail(std::size_t index,double scale) const;
    mutable Shader* compute_=nullptr;
    mutable std::array<std::unique_ptr<Shader>,2> computeVariants_;
    mutable std::unique_ptr<Shader> animation_;
    mutable std::unique_ptr<Shader> surface_;
    mutable std::array<std::unique_ptr<Shader>,2> cellVariants_;
    bool cellIdentity_=true;
    std::unique_ptr<GrassMetadataCompute> metadataCompute_;
    std::unique_ptr<GrassAllocationCompute> allocationCompute_;
    void upload(Patch& patch,const GrassPlan& plan);
    void updateDraws(Patch& patch,double scale,double nearDistance,const glm::dvec3& eyeBody);
    void attributes(std::size_t first,int level) const;
    void drawComputed(std::size_t index,const Patch& patch,const GrassPass& pass) const;
    void animateComputed(std::size_t index,const Patch& patch,const GrassPass& pass,int maxGroups) const;
    void refreshSurface(const Patch& patch) const;
    void allocateComputed(const Patch& patch,std::size_t count) const;
    void bindComputedInputs(const Patch& patch) const;
    Shader& placementProgram(const Patch& patch,bool preparing=false) const;
    std::uint64_t dispatchCandidates(const Patch& patch,int maxGroups) const;
    void generateRoots(const Patch& patch) const;
    void prepareCells(const Patch& patch,const GrassCellCache* previous,bool reuse) const;
    bool generateRootsStep(const Patch& patch,const GrassCellCache* previous,
        std::size_t& level,std::size_t& offset,unsigned dispatchLimit) const;
    void finishCells(const Patch& patch,const GrassCellCache* previous) const;
    std::pair<std::shared_ptr<GrassCellCache>,bool> previousCells(const Patch& patch) const;
    static ProceduralGrassStats patchStats(const Patch& patch);
    GrassFalloff cpuPolicy(const config::PlanetConfig& planet,std::size_t triangles) const;
public:
    bool cellIdentity() const {return cellIdentity_;}
    void enableAdaptiveBudget(bool enabled=true) {adaptiveBudget_=enabled;}
    bool adaptiveBudget() const {return adaptiveBudget_;}
    const GrassBudgetSignals& budgetSignals() const {return budget_.signals();}
    void observeBudget(GrassBudgetSignals signals) {budget_.observe(std::move(signals));}
    void inheritBudget(const ProceduralGrass& live) {adaptiveBudget_=live.adaptiveBudget_;budget_=live.budget_;}
    void terrainBudgetBytes(std::uint64_t bytes) {terrainBudgetBytes_=bytes;}
    void admitCpuTerrain(std::uint64_t bytes) const;
    void externalBudgetBytes(std::uint64_t bytes) {externalBudgetBytes_=bytes;}
    std::uint64_t ownedBytes() const;
    std::uint64_t reservedBytes() const {return terrainBudgetBytes_+externalBudgetBytes_+ownedBytes();}
    bool policyChanged(std::size_t index) const;
    // Acknowledge telemetry changes only when effective planner/draw inputs
    // remain identical. Movement and generation replacement are caller checks.
    bool reuseResidentPolicy(std::size_t index,const Mesh& mesh,
        const config::PlanetConfig& planet,double metersPerWorldUnit);
    nlohmann::json policy(std::size_t index) const;
    void restorePolicies(const nlohmann::json& replay,const std::vector<config::PlanetConfig>& planets);
    config::PlanetConfig budgetPlanet(const config::PlanetConfig& planet,std::uint64_t triangles,std::uint64_t blockBytes,
        std::uint64_t reserved,std::uint64_t logical,std::uint64_t terrainBytes) const;
    // Context-thread ownership. Submission never mutates a published patch.
    class Preparation {
        friend class ProceduralGrass;
        std::unique_ptr<Patch> patch_;
        TerrainGenerationKey generation_;
        GLuint vertices_=0,indices_=0;
        GLsync resourcesFence_=nullptr;
        bool summaryConsumed_=false,ready_=false,failed_=false;
        std::shared_ptr<GrassCellCache> previousCells_;
        bool reuseCells_=false;
        std::size_t rootLevel_=0,rootOffset_=0;
    public:
        ~Preparation();
        Preparation();
        Preparation(const Preparation&)=delete;
        Preparation& operator=(const Preparation&)=delete;
        bool ready() const { return ready_; }
        std::uint64_t admittedBytes=0;
        ProceduralGrassStats stats() const;
    };
    static constexpr std::uint64_t defaultStageBytes=512ull*1024*1024;
    static std::uint64_t stageBytes(std::uint64_t triangles,const config::FoliageConfig& settings,std::uint64_t blockBytes);
    std::unique_ptr<Preparation> submitResident(GLuint vertices,GLuint indices,const TerrainBuildStats& terrain,
        const config::PlanetConfig& planet,double metersPerWorldUnit,const glm::dvec3& eyeBody,
        std::uint64_t revision,std::uint64_t otherBytes=0,std::uint64_t byteLimit=defaultStageBytes,
        const TerrainMorphSource& source={});
    bool poll(Preparation& preparation);
    void waitForCapture(Preparation& preparation);
    void reserve(std::size_t index);
    void validateCommit(std::size_t index,const Preparation& preparation,const Mesh& mesh) const;
    void commit(std::size_t index,Preparation& preparation,const Mesh& mesh);
    Shader shader;
    std::optional<glm::dvec3> planningEye(std::size_t index) const;
    std::optional<TerrainGenerationKey> residentGeneration(std::size_t index) const;
    std::optional<std::uint64_t> residentRevision(std::size_t index) const;
    void restorePlanningEye(std::size_t index,const glm::dvec3& eye);
    GrassTrail& trail(std::size_t index) { return trails_[index].history; }
    const GrassTrail* existingTrail(std::size_t index) const {
        const auto found=trails_.find(index);
        return found==trails_.end() ? nullptr : &found->second.history;
    }
    ProceduralGrass();
    ProceduralGrass(const ProceduralGrass&)=delete;
    ProceduralGrass& operator=(const ProceduralGrass&)=delete;
    ~ProceduralGrass();
    void clear();
    // Scene publication exchanges prepared patches/trails while shaders retain
    // their stable addresses. Both owners require the same current GL context.
    void swapState(ProceduralGrass& other) noexcept;
    GrassPreparationStats prepare(std::size_t index,const Mesh& mesh,const config::PlanetConfig& planet,
        double metersPerWorldUnit,const glm::dvec3& eyeBody,std::uint64_t otherTerrainBytes=0,const CpuGrassTrace* trace=nullptr);
    ProceduralGrassStats stats(std::size_t index) const;
    void draw(std::size_t index,const GrassPass* pass=nullptr) const;
    bool usesCompute(std::size_t index) const;
    // Diagnostic readback; never used by the interactive render path.
    std::array<std::size_t,2> computedCounts(std::size_t index) const;
    // Explicit validation only; never read by the interactive render path.
    std::vector<GrassRootRecord> cachedRootsForValidation(std::size_t index) const;
    std::vector<GrassCellRecord> cachedCellsForValidation(std::size_t index) const;
    std::vector<GrassAnimationRecord> cachedAnimationForValidation(std::size_t index) const;
    // Cache-miss counter is enabled only during transform-feedback validation.
    std::uint32_t animationUpdatesForValidation(std::size_t index) const;
};
}

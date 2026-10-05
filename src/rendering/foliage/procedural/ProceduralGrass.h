#pragma once
#include "rendering/diagnostics/timing/GpuWorkProfiler.h"
#include "rendering/Shader.h"
#include "rendering/foliage/GrassStats.h"
#include "rendering/foliage/procedural/GrassPlan.h"
#include "config/FoliageConfig.h"
#include <memory>
#include <map>
#include <optional>
#include "rendering/foliage/trails/GrassTrail.h"
#include "rendering/geometry/terrain/TerrainTopology.h"
class Mesh;
namespace rendering {
class GrassMetadataCompute;
class GrassAllocationCompute;
struct GrassAllocationBuffers;
struct GrassMetadataBuffers;
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
};
class ProceduralGrass {
public:
    class Preparation;
private:
    friend class TerrainPublication;
    void commitPrepared(std::size_t index,Preparation& preparation) noexcept;
    struct Batch { GLuint vao=0; GLsizei count=0; };
    struct Draw { std::size_t first=0, patches=0; int level=0, segments=1; };
    struct Bound { glm::dvec3 center; double radius=0; };
    struct Patch {
        ~Patch();
        std::array<Batch,grassCandidateSlots.size()> batches{};
        std::vector<Bound> bounds;
        std::vector<Draw> draws;
        GLuint buffer=0, vertexTexture=0, indexTexture=0;
        std::unique_ptr<GrassMetadataBuffers> metadata;
        std::unique_ptr<GrassAllocationBuffers> allocation;
        mutable GLuint gpuBlades=0, commands=0;
        mutable std::array<GLuint,2> gpuVaos{};
        mutable std::size_t gpuCapacity=0;
        mutable bool computeUsed=false;
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
    void bindTrail(std::size_t index,double scale) const;
    mutable std::unique_ptr<Shader> compute_;
    std::unique_ptr<GrassMetadataCompute> metadataCompute_;
    std::unique_ptr<GrassAllocationCompute> allocationCompute_;
    void upload(Patch& patch,const GrassPlan& plan);
    void updateDraws(Patch& patch,double scale,double nearDistance,const glm::dvec3& eyeBody);
    void attributes(std::size_t first,int level) const;
    void drawComputed(const Patch& patch,const GrassPass& pass) const;
    void allocateComputed(const Patch& patch,std::size_t count) const;
    static ProceduralGrassStats patchStats(const Patch& patch);
public:
    // Context-thread ownership. Submission never mutates a published patch.
    class Preparation {
        friend class ProceduralGrass;
        std::unique_ptr<Patch> patch_;
        TerrainGenerationKey generation_;
        GLuint vertices_=0,indices_=0;
        GLsync resourcesFence_=nullptr;
        bool summaryConsumed_=false,ready_=false,failed_=false;
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
        std::uint64_t revision,std::uint64_t otherBytes=0,std::uint64_t byteLimit=defaultStageBytes);
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
        double metersPerWorldUnit,const glm::dvec3& eyeBody,std::uint64_t otherTerrainBytes=0);
    ProceduralGrassStats stats(std::size_t index) const;
    void draw(std::size_t index,const GrassPass* pass=nullptr) const;
    bool usesCompute(std::size_t index) const;
    // Diagnostic readback; never used by the interactive render path.
    std::array<std::size_t,2> computedCounts(std::size_t index) const;
};
}

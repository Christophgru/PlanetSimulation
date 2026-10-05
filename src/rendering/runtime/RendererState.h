#pragma once
#include "rendering/Renderer.h"
#include "app/CommandLineOptions.h"
#include "app/Window.h"
#include "app/WindowInput.h"
#include "app/scene/SceneSource.h"
#include "app/scene/PreparedScene.h"
#include "rendering/runtime/ResourceOwners.h"
#include "rendering/runtime/ScenePass.h"
#include "rendering/geometry/compute/TerrainCompute.h"
#include "rendering/geometry/jobs/TerrainBuildScheduler.h"
#include "rendering/geometry/publication/TerrainPublication.h"
#include "rendering/runtime/terrain/reload/LegacySceneReplacement.h"
#include "rendering/runtime/terrain/reload/SceneTerrainReplacement.h"
#include "rendering/diagnostics/PerformanceOverlay.h"
#include "rendering/diagnostics/OrbitOverlay.h"
#include "rendering/diagnostics/GpuUtilization.h"
#include "rendering/diagnostics/AdaptiveQuality.h"
#include "rendering/diagnostics/FrameReuse.h"
#include "rendering/character/AstronautRenderer.h"
#include "rendering/postprocessing/LensFlare.h"
#include <array>
#include <limits>

namespace rendering {
struct PendingSceneReload;
struct ReloadTracking;
struct ReloadCommitState;
struct Renderer::Impl {
    explicit Impl(app::CommandLineOptions arguments);
    ~Impl();
    int capture();
    int interact();
    void reloadScene();
    void requestResidentReload();
    bool pollResidentReload();
    bool sceneReloadPreparing() const;
    void finishSceneReload(ReloadTracking& tracking,app::CommandLineOptions& nextOptions,
        nlohmann::json& nextReplay,const ReloadCommitState& commit);
    void retireSceneReload(bool captureWait);
    glm::dvec3 previewReloadEye(app::PreparedScene& prepared,std::vector<Mesh>& land,
        std::vector<glm::dvec3>& anchors,nlohmann::json& replay,
        app::CommandLineOptions& preparedOptions,double time);
    nlohmann::json sceneReloadState() const;
    void preparePlanetMeshes(const glm::dvec3& eye, bool asyncWalking = false,
        std::optional<double> characterElapsed=std::nullopt);
    void prepareResidentFrame(const glm::dvec3& eye,std::optional<double> characterElapsed);
    void recordResidentPublication(std::size_t index,std::vector<int>& zones,bool terrainChanged);
    PublicationProfiler::Generation publicationGeneration(std::size_t index) const;
    void recordRenderedPublications(bool character);
    bool residentSceneReady() const;
    void publishResidentBuilds(std::vector<std::optional<TerrainCpuBuild>> builds,
        const std::vector<std::optional<TerrainBuildIdentity>>& identities,
        const glm::dvec3& eye,std::optional<double> characterElapsed);
    std::vector<SurfaceContact> characterTerrainContacts() const;
    glm::dvec3 previewAstronautEye(double elapsed,std::vector<SurfaceContact> contacts={});
    nlohmann::json terrainPublicationState() const;
    void installLandMesh(std::size_t index, TerrainGeometry geometry,
                         const glm::dvec3& localEye, int localMask,TerrainComputeBuffers* computed=nullptr);
    void installTerrainBuild(TerrainCpuBuild built,const TerrainBuildIdentity& identity);
    std::vector<std::uint64_t> geometryRevisions() const;
    ClipPlanes planetOrbitClip(const glm::dvec3& eye) const;
    void prepareAstronaut(double elapsed,std::vector<SurfaceContact> plannedContacts={},bool preview=false);
    nlohmann::json astronautState() const;

    app::CommandLineOptions options;
    CpuTrace cpuTrace;
    app::SceneSource source;
    // Reverse member destruction keeps the context alive through every GPU owner,
    // including when a later member or the constructor body throws.
    app::Window context;
    GLFWwindow* window;
    app::PreparedScene scene;
    FrameProfiler profiler;
    SceneMeshes meshes;
    std::unique_ptr<TerrainCompute> terrainCompute;
    std::string terrainFallback;
    std::vector<bool> meshReady;
    std::vector<int> lastLocalMask;
    std::vector<std::vector<int>> lastFaceZones;
    std::vector<glm::dvec3> lastTerrainEyes;
    std::vector<std::array<int, 3>> meshZoneFaces;
    std::vector<int> meshTriangles;
    std::vector<int> meshSteepRefinedFaces;
    TerrainBuildScheduler terrainJobs;
    std::uint64_t terrainSceneEpoch=1,terrainRequestSerial=0,terrainRejectedBuilds=0;
    std::vector<std::uint64_t> installedTerrainSerial;
    std::vector<std::optional<TerrainBuildIdentity>> terrainFailures;
    double simulationTime;
    simulation::SimulationClock simulationClock;
    CameraInput cameraInput;
    app::InputContext inputContext;
    ClipPlanes surfaceClip;
    OwnedShader shader{"shaders/terrain/basic.vert", "shaders/terrain/basic.frag", "shaders/terrain/terrain_shadow.glsl", "shaders/atmosphere/atmosphere.glsl", "shaders/foliage/palette.glsl"};
    OwnedShader waterShader{"shaders/water/water.vert", "shaders/water/water.frag", "shaders/terrain/terrain_shadow.glsl", "shaders/atmosphere/atmosphere.glsl"};
    GrassRenderer grass;
    std::unique_ptr<TerrainPublication> terrainPublication; // Dies before grass/meshes/context.
    std::unique_ptr<SceneTerrainReplacement> retiredResidentScene;
    std::unique_ptr<LegacySceneReplacement> retiredLegacyScene;
    std::unique_ptr<PendingSceneReload> pendingResidentReload;
    std::uint64_t lastReloadAttempt=1,sceneReloadSuperseded=0,sceneReloadFrames=0;
    std::uint64_t sceneReloads=0,sceneReloadFailures=0;
    std::optional<glm::dvec3> reloadCharacterEye;
    bool reloadCharacterPending=false;
    std::vector<SceneTerrainConsumers> terrainConsumers;
    std::uint64_t characterPreviews=0;
    std::optional<glm::dvec3> plannedCharacterEye;
    std::optional<TerrainBuildIdentity> residentStage;
    std::uint64_t residentStageAttempt=0;
    std::vector<int> residentStageZones;
    bool residentStageGrassOnly=false,residentRetirementFailed=false;
    std::uint64_t residentFrames=0,residentFrameFailures=0,residentRetryAfter=0;
    AstronautRenderer astronaut;
    SurfaceContact astronautGround;
    ChasePose astronautView; // World coordinates; motion/contacts stay body-local.
    glm::dvec3 captureTerrainEye{0}; // Selected terrain build anchor, body-local world units.
    std::uint64_t astronautGroundRevision=0;
    bool astronautReplayRestored=false;
    bool astronautBenchmarkBoost=false;
    double characterWindTime=0;
    glm::dvec2 astronautFlightControl{0};
    OwnedShader shadowShader{"shaders/terrain/terrain_shadow.vert", "shaders/terrain/terrain_shadow.frag"};
    TerrainShadowMaps terrainShadows;
    OwnedShader atmosphereShader{"shaders/atmosphere/atmosphere.vert", "shaders/atmosphere/atmosphere.frag", "shaders/atmosphere/atmosphere.glsl"};
    AtmosphereRenderer atmosphere;
    AtmosphereRenderer reflectionAtmosphere;
    AtmosphereTransmittance atmosphereColumns;
    PerformanceOverlay performanceOverlay;
    OrbitOverlay orbitOverlay;
    std::vector<OrbitTrail> orbitTrails;
    std::vector<glm::vec3> orbitColors;
    std::vector<std::uint64_t> orbitColorRevisions;
    double orbitTrailEpoch = std::numeric_limits<double>::quiet_NaN();
    GpuUtilization gpuUtilization;
    FrameRate frameRate;
    FrameReuse frameReuse;
    std::unique_ptr<LensFlare> lensFlare;
    FlareEvidence flareEvidence;
    OwnedShader skyboxShader{"shaders/skybox/skybox.vert", "shaders/skybox/skybox.frag"};
    WaterReflectionTarget waterReflection;
    WaterReflectionTarget qualityTarget;
    OwnedShader qualityPresent{"shaders/diagnostics/quality_present.vert", "shaders/diagnostics/quality_present.frag"};
    VertexArray qualityVao;
    AdaptiveQuality adaptiveQuality;
    std::pair<int, int> lastQualitySize{};
};
}

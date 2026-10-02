#pragma once
#include "rendering/Renderer.h"
#include "app/CommandLineOptions.h"
#include "app/Window.h"
#include "app/WindowInput.h"
#include "app/scene/SceneSource.h"
#include "app/scene/PreparedScene.h"
#include "rendering/runtime/ResourceOwners.h"
#include "rendering/runtime/ScenePass.h"
#include "rendering/diagnostics/PerformanceOverlay.h"
#include "rendering/diagnostics/OrbitOverlay.h"
#include "rendering/diagnostics/GpuUtilization.h"
#include "rendering/diagnostics/AdaptiveQuality.h"
#include "rendering/diagnostics/FrameReuse.h"
#include "rendering/character/AstronautRenderer.h"
#include "rendering/postprocessing/LensFlare.h"
#include <array>
#include <future>
#include <limits>

namespace rendering {
struct Renderer::Impl {
    explicit Impl(app::CommandLineOptions arguments);
    ~Impl();
    int capture();
    int interact();
    void preparePlanetMeshes(const glm::dvec3& eye, bool asyncWalking = false);
    void installLandMesh(std::size_t index, TerrainGeometry geometry,
                         const glm::dvec3& localEye, int localMask);
    std::vector<std::uint64_t> geometryRevisions() const;
    ClipPlanes planetOrbitClip(const glm::dvec3& eye) const;
    void prepareAstronaut(double elapsed);
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
    std::vector<bool> meshReady;
    std::vector<int> lastLocalMask;
    std::vector<std::vector<int>> lastFaceZones;
    std::vector<glm::dvec3> lastTerrainEyes;
    std::vector<std::array<int, 3>> meshZoneFaces;
    std::vector<int> meshTriangles;
    std::vector<int> meshSteepRefinedFaces;
    struct TimedTerrainBuild {
        TerrainGeometry geometry;
        std::optional<TerrainGeometry> water;
        double milliseconds = 0;
    };
    struct PendingTerrainBuild {
        std::future<TimedTerrainBuild> geometry;
        glm::dvec3 eyeLocal{0.0};
        int localMask = 0;
    };
    std::vector<PendingTerrainBuild> pendingTerrain;
    double simulationTime;
    simulation::SimulationClock simulationClock;
    CameraInput cameraInput;
    app::InputContext inputContext;
    ClipPlanes surfaceClip;
    OwnedShader shader{"shaders/terrain/basic.vert", "shaders/terrain/basic.frag", "shaders/terrain/terrain_shadow.glsl", "shaders/atmosphere/atmosphere.glsl", "shaders/foliage/palette.glsl"};
    OwnedShader waterShader{"shaders/water/water.vert", "shaders/water/water.frag", "shaders/terrain/terrain_shadow.glsl", "shaders/atmosphere/atmosphere.glsl"};
    GrassRenderer grass;
    AstronautRenderer astronaut;
    SurfaceContact astronautGround;
    ChasePose astronautView; // World coordinates; motion/contacts stay body-local.
    glm::dvec3 captureTerrainEye{0}; // Selected terrain build anchor, body-local world units.
    std::uint64_t astronautGroundRevision=0;
    bool astronautReplayRestored=false;
    bool astronautBenchmarkBoost=false;
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

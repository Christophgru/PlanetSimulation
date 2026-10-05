#pragma once
#include <array>
#include <cstddef>
#include <vector>
#include "rendering/lighting/CameraExposure.h"
#include "rendering/geometry/SceneTransforms.h"
#include "rendering/geometry/Mesh.h"
#include "rendering/Shader.h"
#include "rendering/WaterReflectionTarget.h"
#include "rendering/foliage/GrassRenderer.h"
#include "rendering/lighting/TerrainShadowMaps.h"
#include "rendering/atmosphere/AtmosphereRenderer.h"
#include "rendering/diagnostics/FrameProfiler.h"

namespace rendering {
class AstronautRenderer;
class TerrainPublication;
struct SceneTerrainConsumers {
    TerrainGenerationKey land,water,grass,contacts;
    std::uint64_t landRevision=0,waterRevision=0,grassRevision=0;
    std::uint64_t mainRevision=0,reflectionRevision=0,shadowRevision=0,waterDrawRevision=0,grassDrawRevision=0;
};
rendering::CameraExposure renderScene(const config::ScenarioConfig& scenario, AtmosphereOpticsCache& opticsCache,
                 const std::vector<simulation::BodyState>& bodies,
                 const glm::mat4& view, float fov,
                 const glm::dvec3& eyeWorld, const Shader& shader,
                 const Shader& waterShader, const Shader& skyboxShader,
                 rendering::WaterReflectionTarget& reflectionTarget,
                 const Shader& shadowShader, rendering::TerrainShadowMaps& shadows,
                 const Shader& atmosphereShader, rendering::AtmosphereRenderer& atmosphere,
                 rendering::AtmosphereRenderer& reflectionAtmosphere,
                 rendering::AtmosphereTransmittance& atmosphereColumns,
                 const Mesh& sunMesh, const Mesh& skyboxMesh,
                 const std::vector<Mesh>& planetMeshes,
                 const std::vector<Mesh>& waterMeshes, int width, int height,
                 rendering::ClipPlanes clip = {},
                 std::optional<std::size_t> meteredPlanet = std::nullopt,
                 bool recordObjects = false, rendering::FrameProfiler* profiler = nullptr,
                 bool forceHdr = false, GLuint outputFramebuffer = 0,
                 rendering::GrassRenderer* grass = nullptr, double sceneTime = 0,
                 AstronautRenderer* astronaut = nullptr,
                 std::vector<std::array<std::size_t,2>>* mainGrassCounts = nullptr,
                 const TerrainPublication* publication = nullptr,
                 std::vector<SceneTerrainConsumers>* consumers = nullptr);
}

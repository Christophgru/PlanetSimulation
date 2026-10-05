#pragma once
#include "config/ScenarioConfig.h"
#include "rendering/atmosphere/AtmosphereOpticsCache.h"
#include "simulation/OrbitalSystem.h"
#include "rendering/geometry/Terrain.h"
#include "rendering/camera/OrbitCamera.h"
#include "rendering/camera/PlanetSurfaceCamera.h"

namespace app {
// Staged CPU scene used both for initial construction and transactional reload.
struct PreparedScene {
    config::ScenarioConfig scenario;
    rendering::AtmosphereOpticsCache atmosphereOptics;
    simulation::OrbitalSystem dynamics;
    std::vector<simulation::BodyState> bodies;
    std::vector<rendering::TerrainSurface> terrainSurfaces;
    OrbitCamera sunCamera;
    std::optional<PlanetSurfaceCamera> surfaceCamera;
    std::optional<OrbitCamera> planetOrbitCamera;
    std::size_t orbitPlanetIndex = 0;
    glm::dvec3 planetOrbitCenter{0.0};
    double planetOrbitOuterRadius = 0.0;
    glm::dvec3 sunPosition{0.0};

    explicit PreparedScene(config::ScenarioConfig parsed);
    void updateSimulation(double seconds);
};
}

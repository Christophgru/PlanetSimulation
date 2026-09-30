#include "app/scene/PreparedScene.h"

namespace app {
PreparedScene::PreparedScene(config::ScenarioConfig parsed)
    : scenario(std::move(parsed)),
      dynamics(scenario), bodies(dynamics.at(0.0)),
      sunCamera(glm::vec3(scenario.camera.target[0], scenario.camera.target[1],
                          scenario.camera.target[2]),
                glm::vec3(scenario.camera.position[0] - scenario.camera.target[0],
                          scenario.camera.position[1] - scenario.camera.target[1],
                          scenario.camera.position[2] - scenario.camera.target[2])),
      sunPosition(bodies[0].position) {
    sunCamera.fov = static_cast<float>(scenario.camera.fov);
    terrainSurfaces.reserve(scenario.planets.size());
    for (std::size_t i = 0; i < scenario.planets.size(); ++i) {
        const auto& planet = scenario.planets[i];
        const glm::dvec3 center = bodies[i + 1].position;
        if (glm::length(glm::dvec3(sunCamera.position) - center) <= 1e-12)
            throw std::invalid_argument("Configured camera cannot start at a planet center");
        terrainSurfaces.emplace_back(planet.surface_noise, planet.terrain_lod,
                                     planet.radius, scenario.metersPerWorldUnit(),
                                     planet.terrain_landscape,
                                     planet.water.enabled ?
                                         std::optional<double>(planet.water.level_m) :
                                         std::nullopt, planet.terrain_material);
    }
    if (scenario.surface_camera.enabled) {
        const auto& settings = scenario.surface_camera;
        const auto& planet = scenario.planets[settings.planet_index];
        coordinates::PlanetLocalFrame planetFrame(
            bodies[settings.planet_index + 1].position, planet.radius,
            bodies[settings.planet_index + 1].orientation);
        surfaceCamera.emplace(
            planetFrame,
            coordinates::LatLonAlt{settings.latitude_deg,
                                   settings.longitude_deg, settings.altitude},
            sunPosition, settings.fov,
            settings.walk_speed_mps / scenario.metersPerWorldUnit());
        surfaceCamera->mountTerrain(terrainSurfaces[settings.planet_index],
                                    settings.altitude,
                                    planet.water.enabled ?
                                        std::optional<double>(planet.water.level_m /
                                            scenario.metersPerWorldUnit()) : std::nullopt);
        if (settings.direction_ned) {
            const auto& saved = *settings.direction_ned;
            std::optional<glm::dvec3> savedUp;
            if (settings.up_ned) {
                const auto& up = *settings.up_ned;
                savedUp = glm::dvec3(up[0], up[1], up[2]);
            }
            surfaceCamera->setDirectionNed({saved[0], saved[1], saved[2]}, savedUp);
        }
    }
    if (!scenario.planets.empty()) {
        orbitPlanetIndex = scenario.surface_camera.enabled ?
            static_cast<std::size_t>(scenario.surface_camera.planet_index) : 0;
        const auto& planet = scenario.planets[orbitPlanetIndex];
        planetOrbitCenter = bodies[orbitPlanetIndex + 1].position;
        double maximumLandHeightMeters =
            planet.terrain_landscape.maximumAbsoluteHeightMeters();
        for (const auto& function : planet.surface_noise)
            maximumLandHeightMeters += function.amplitude_m;
        planetOrbitOuterRadius = planet.radius +
            std::max(maximumLandHeightMeters,
                     planet.water.enabled ? planet.water.level_m : 0.0) /
                scenario.metersPerWorldUnit();
        const float minimumDistance = static_cast<float>(planetOrbitOuterRadius +
            2.0 / scenario.metersPerWorldUnit());
        const float initialDistance = std::max(
            static_cast<float>(2.8 * planet.radius), 1.5f * minimumDistance);
        const float maximumDistance = std::max(
            static_cast<float>(20.0 * planet.radius), 2.0f * initialDistance);
        glm::dvec3 initialRadial = surfaceCamera ?
            surfaceCamera->position() - planetOrbitCenter :
            glm::dvec3(sunCamera.position) - planetOrbitCenter;
        if (glm::length(initialRadial) <= 1e-12)
            initialRadial = glm::dvec3(0.0, 1.0, 0.0);
        const glm::vec3 initialOffset = glm::vec3(
            glm::normalize(initialRadial) * static_cast<double>(initialDistance));
        planetOrbitCamera.emplace(glm::vec3(planetOrbitCenter), initialOffset,
            OrbitCamera::Settings{minimumDistance, maximumDistance, 0.96f});
    }
}

void PreparedScene::updateSimulation(double seconds) {
    bodies = dynamics.at(seconds);
    const glm::dvec3 sunFocusOffset(
        scenario.camera.target[0] - scenario.sun.position[0],
        scenario.camera.target[1] - scenario.sun.position[1],
        scenario.camera.target[2] - scenario.sun.position[2]);
    sunCamera.followTarget(glm::vec3(bodies[0].position + sunFocusOffset));
    sunPosition = bodies[0].position;
    if (planetOrbitCamera) {
        planetOrbitCenter = bodies[orbitPlanetIndex + 1].position;
        planetOrbitCamera->followTarget(glm::vec3(planetOrbitCenter));
    }
    if (surfaceCamera) {
        const auto index = scenario.surface_camera.planet_index;
        surfaceCamera->followPlanet(coordinates::PlanetLocalFrame(
            bodies[index + 1].position, scenario.planets[index].radius,
            bodies[index + 1].orientation), sunPosition);
    }
}
}

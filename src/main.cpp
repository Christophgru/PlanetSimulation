#include <iostream>
#include <algorithm>
#include <cmath>
#include <GL/glew.h>
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <thread>
#include <chrono>
#include <filesystem>
#include <cstdlib>
#include <optional>
#include <array>
#include <future>
#include <stdexcept>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "config/Config.h"
#include "config/ScenarioConfig.h"
#include "simulation/OrbitalSystem.h"
#include "rendering/Mesh.h"
#include "rendering/Terrain.h"
#include "rendering/CameraInput.h"
#include "rendering/OrbitCamera.h"
#include "rendering/PlanetSurfaceCamera.h"
#include "rendering/RenderDiagnostics.h"
#include "rendering/SceneTransforms.h"
#include "rendering/CelestialLighting.h"
#include "rendering/Shader.h"
#include "rendering/SurfaceCameraTelemetry.h"
#include "rendering/WaterReflectionTarget.h"
#include "rendering/TerrainShadowMaps.h"

namespace fs = std::filesystem;

struct InputContext {
    CameraInput* camera = nullptr;
    simulation::SimulationClock* clock = nullptr;
    bool reloadRequested = false;
};

CameraInput* windowCameraInput(GLFWwindow* window) {
    auto* context = static_cast<InputContext*>(glfwGetWindowUserPointer(window));
    return context ? context->camera : nullptr;
}

void onMouseButton(GLFWwindow* window, int button, int action, int) {
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;
    auto* input = windowCameraInput(window);
    if (!input) return;

    if (action == GLFW_PRESS) {
        double x = 0.0;
        double y = 0.0;
        glfwGetCursorPos(window, &x, &y);
        input->beginDrag(x, y);
    } else if (action == GLFW_RELEASE) {
        input->endDrag();
    }
}

void onCursorPosition(GLFWwindow* window, double x, double y) {
    auto* input = windowCameraInput(window);
    if (input) input->moveCursor(x, y);
}

void onScroll(GLFWwindow* window, double, double yOffset) {
    auto* input = windowCameraInput(window);
    if (input) input->scroll(yOffset);
}

void onKey(GLFWwindow* window, int key, int, int action, int) {
    if (action != GLFW_PRESS) return;
    auto* context = static_cast<InputContext*>(glfwGetWindowUserPointer(window));
    if (key == GLFW_KEY_T && context && context->clock) {
        context->clock->togglePause(glfwGetTime());
        std::cout << (context->clock->paused() ? "Simulation paused (T to resume)\n" :
                                               "Simulation resumed (T to pause)\n");
        return;
    }
    if ((key == GLFW_KEY_Y || key == GLFW_KEY_U) && context && context->clock) {
        context->clock->scaleSpeed(key == GLFW_KEY_Y ? 0.5 : 2.0, glfwGetTime());
        std::cout << "Simulation speed: " << context->clock->speed() << "x"
                  << (context->clock->paused() ? " (paused)\n" : "\n");
        return;
    }
    if (key == GLFW_KEY_R && context) {
        context->reloadRequested = true;
        return;
    }
    auto* input = context ? context->camera : nullptr;
    if (!input) return;
    if (key == GLFW_KEY_1) {
        input->selectOrbit();
    } else if (key == GLFW_KEY_2) {
        input->selectSurface();
    } else if (key == GLFW_KEY_3) {
        input->selectPlanetOrbit();
    } else if (key == GLFW_KEY_ESCAPE) {
        input->releaseCursor();
    }
}

struct PreparedScene {
    config::ScenarioConfig scenario;
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

    explicit PreparedScene(config::ScenarioConfig parsed)
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
                                             std::nullopt);
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
};

// The Sun retains its existing sphere mesh; planets use terrain triangles.
Mesh g_mesh;

void renderScene(const config::ScenarioConfig& scenario,
                 const std::vector<simulation::BodyState>& bodies,
                 const glm::mat4& view, float fov,
                 const glm::dvec3& eyeWorld, const Shader& shader,
                 const Shader& waterShader, const Shader& skyboxShader,
                 rendering::WaterReflectionTarget& reflectionTarget,
                 const Shader& shadowShader, rendering::TerrainShadowMaps& shadows,
                 const Mesh& sunMesh, const Mesh& skyboxMesh,
                 const std::vector<Mesh>& planetMeshes,
                 const std::vector<Mesh>& waterMeshes, int width, int height,
                 rendering::ClipPlanes clip = {}) {
    const glm::mat4 projection = rendering::perspectiveProjection(
        fov, static_cast<float>(width) / height, clip);
    const auto& sun = scenario.sun;
    const auto lighting = rendering::calculateLighting(scenario, bodies);
    shadows.ensure(scenario.planets.size(), scenario.lighting.shadows);
    if (scenario.lighting.shadows.enabled) {
        for (std::size_t i = 0; i < scenario.planets.size(); ++i) {
            const auto& planet = scenario.planets[i];
            double heightMeters = planet.terrain_landscape.maximumAbsoluteHeightMeters();
            for (const auto& noise : planet.surface_noise) heightMeters += noise.amplitude_m;
            const double extent = 1.0 + heightMeters / (scenario.metersPerWorldUnit() * planet.radius);
            glm::dvec3 localSun = glm::transpose(bodies[i + 1].orientation) * lighting.planets[i].sunDirection;
            // Coincident centers have no directed sunlight; any map orientation
            // is valid because their diffuse contribution is already zero.
            if (glm::dot(localSun, localSun) == 0.0) localSun = glm::dvec3(0, 0, 1);
            shadows.begin(i, shadowShader, localSun, extent);
            planetMeshes[i].draw();
        }
    }
    const auto setRgb = [](const Shader& target, const char* name, const glm::dvec3& value) {
        target.setFloat3(name, static_cast<float>(value.x), static_cast<float>(value.y),
                        static_cast<float>(value.z));
    };
    const auto setBodyLighting = [&](const Shader& target, std::size_t index, float radiusScale = 1.0f) {
        const auto& light = lighting.planets[index];
        setRgb(target, "uSunDirection", light.sunDirection);
        setRgb(target, "uSunlight", light.sunlight);
        setRgb(target, "uIndirectLight", light.reflectedLight + glm::dvec3(scenario.lighting.ambient_light));
        target.setFloat("uExposure", static_cast<float>(scenario.lighting.exposure));
        shadows.bindForShading(index, target, scenario.lighting.shadows, radiusScale);
    };

    auto drawSkybox = [&](const glm::mat4& passProjection,
                          const glm::mat4& passView) {
        if (!scenario.skybox.enabled) return;
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE);
        skyboxShader.use();
        skyboxShader.setMat4("projection", glm::value_ptr(passProjection));
        skyboxShader.setMat4("view", glm::value_ptr(passView));
        skyboxShader.setInt("uSeed", scenario.skybox.seed);
        skyboxShader.setFloat("uStarDensity",
                              static_cast<float>(scenario.skybox.star_density));
        skyboxShader.setFloat("uStarScale",
                              static_cast<float>(scenario.skybox.star_scale));
        skyboxShader.setFloat("uStarBrightness",
                              static_cast<float>(scenario.skybox.star_brightness));
        skyboxShader.setFloat3("uBackgroundColor",
            static_cast<float>(scenario.skybox.background_color[0]),
            static_cast<float>(scenario.skybox.background_color[1]),
            static_cast<float>(scenario.skybox.background_color[2]));
        skyboxShader.setFloat3("uStarColor",
            static_cast<float>(scenario.skybox.star_color[0]),
            static_cast<float>(scenario.skybox.star_color[1]),
            static_cast<float>(scenario.skybox.star_color[2]));
        skyboxMesh.draw();
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
    };

    auto drawOpaqueScene = [&](const glm::mat4& passProjection,
                               const glm::mat4& passView,
                               const glm::vec3& clipCenter,
                               float clipRadius) {
        shader.use();
        shader.setMat4("projection", glm::value_ptr(passProjection));
        shader.setMat4("view", glm::value_ptr(passView));
        shader.setFloat3("uClipCenter", clipCenter.x, clipCenter.y, clipCenter.z);
        shader.setFloat("uClipRadius", clipRadius);
        setRgb(shader, "uEmission", lighting.sunEmission);
        shader.setFloat("uExposure", static_cast<float>(scenario.lighting.exposure));

        const glm::mat4 sunModel = rendering::sphereModel(
            glm::vec3(bodies[0].position), static_cast<float>(sun.radius));
        shader.setMat4("model", glm::value_ptr(sunModel));
        shader.setFloat3("uColor", static_cast<float>(sun.color[0]),
                         static_cast<float>(sun.color[1]),
                         static_cast<float>(sun.color[2]));
        shader.setFloat("uEmissive", 1.0f);
        sunMesh.draw();

        for (std::size_t i = 0; i < scenario.planets.size(); ++i) {
            const auto& planet = scenario.planets[i];
            const glm::mat4 model = rendering::sphereModel(
                glm::vec3(bodies[i + 1].position), static_cast<float>(planet.radius),
                glm::mat3(bodies[i + 1].orientation));
            shader.setMat4("model", glm::value_ptr(model));
            shader.setFloat3("uColor", static_cast<float>(planet.color[0]),
                             static_cast<float>(planet.color[1]),
                             static_cast<float>(planet.color[2]));
            shader.setFloat("uEmissive", 0.0f);
            setBodyLighting(shader, i);
            planetMeshes[i].draw();
        }
    };

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    glClearColor(static_cast<float>(scenario.skybox.background_color[0]),
                 static_cast<float>(scenario.skybox.background_color[1]),
                 static_cast<float>(scenario.skybox.background_color[2]), 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    drawSkybox(projection, view);
    drawOpaqueScene(projection, view, glm::vec3(0.0f), -1.0f);

    const bool hasWater = std::any_of(
        scenario.planets.begin(), scenario.planets.end(),
        [](const config::PlanetConfig& planet) { return planet.water.enabled; });
    if (!hasWater) return;
    reflectionTarget.ensure(width, height);

    // The opaque main-scene depth buffer masks this translucent sea. Each
    // planet reuses one bounded reflection target before drawing its water.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    for (std::size_t i = 0; i < scenario.planets.size(); ++i) {
        const auto& planet = scenario.planets[i];
        if (!planet.water.enabled) continue;
        const glm::dvec3 centerWorld = bodies[i + 1].position;
        const glm::vec3 center(centerWorld);
        const double radiusWorld =
            planet.radius + planet.water.level_m / scenario.metersPerWorldUnit();
        const glm::mat4 reflectedView = rendering::waterReflectionView(
            view, eyeWorld, centerWorld, radiusWorld);
        const glm::mat4 reflectedProjection = rendering::perspectiveProjection(
            fov, static_cast<float>(reflectionTarget.width()) /
                     reflectionTarget.height(), clip);
        const glm::mat4 reflectionViewProjection = reflectedProjection * reflectedView;

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        reflectionTarget.bind();
        glViewport(0, 0, reflectionTarget.width(), reflectionTarget.height());
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        drawSkybox(reflectedProjection, reflectedView);
        drawOpaqueScene(reflectedProjection, reflectedView, center,
                        static_cast<float>(radiusWorld));

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        waterShader.use();
        setBodyLighting(waterShader, i, static_cast<float>(radiusWorld / planet.radius));
        waterShader.setMat4("projection", glm::value_ptr(projection));
        waterShader.setMat4("view", glm::value_ptr(view));
        waterShader.setMat4("uReflectionViewProjection",
                            glm::value_ptr(reflectionViewProjection));
        waterShader.setFloat3("uCameraPosition", static_cast<float>(eyeWorld.x),
                              static_cast<float>(eyeWorld.y),
                              static_cast<float>(eyeWorld.z));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, reflectionTarget.colorTexture());
        waterShader.setInt("uReflectionTexture", 0);

        const glm::mat4 model = rendering::sphereModel(
            center, static_cast<float>(radiusWorld), glm::mat3(bodies[i + 1].orientation));
        waterShader.setMat4("model", glm::value_ptr(model));
        waterShader.setFloat3("uPlanetCenter", center.x, center.y, center.z);
        waterShader.setFloat3("uWaterColor", static_cast<float>(planet.water.color[0]),
                              static_cast<float>(planet.water.color[1]),
                              static_cast<float>(planet.water.color[2]));
        waterShader.setFloat("uOpacity", static_cast<float>(planet.water.opacity));
        waterShader.setFloat("uReflectionFraction",
                             static_cast<float>(planet.water.reflection_fraction));
        waterMeshes[i].draw();
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

int main(int argc, char** argv) {
    bool renderTestMode = false;
    bool surfaceRenderMode = false;
    bool planetRenderMode = false;
    std::string outputImagePath;
    int renderTestWidth = 800;
    int renderTestHeight = 600;
    double simulationTime = 0.0;
    std::string configPath = "configs/scenarios/solar_system.json";
    
    // Parse command-line arguments
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--render-test" && i + 1 < argc) {
            renderTestMode = true;
            outputImagePath = argv[++i];
        } else if (std::string(argv[i]) == "--surface-render-test" && i + 1 < argc) {
            renderTestMode = true;
            surfaceRenderMode = true;
            outputImagePath = argv[++i];
        } else if (std::string(argv[i]) == "--planet-render-test" && i + 1 < argc) {
            renderTestMode = true;
            planetRenderMode = true;
            outputImagePath = argv[++i];
        } else if (std::string(argv[i]) == "--config" && i + 1 < argc) {
            configPath = argv[++i];
        } else if (std::string(argv[i]) == "--simulation-time" && i + 1 < argc) {
            try {
                simulationTime = std::stod(argv[++i]);
                if (!std::isfinite(simulationTime)) throw std::invalid_argument("time");
            } catch (const std::exception&) {
                std::cerr << "--simulation-time needs a finite number of seconds\n";
                return 1;
            }
        } else if (std::string(argv[i]) == "--render-size" && i + 2 < argc) {
            try {
                renderTestWidth = std::stoi(argv[++i]);
                renderTestHeight = std::stoi(argv[++i]);
            } catch (const std::exception&) {
                std::cerr << "--render-size needs integer width and height\n";
                return 1;
            }
            if (renderTestWidth < 64 || renderTestHeight < 64 ||
                renderTestWidth > 8192 || renderTestHeight > 8192) {
                std::cerr << "--render-size must be between 64 and 8192 pixels\n";
                return 1;
            }
        }
    }

    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return 1;
    }

    // Configure GLFW for modern OpenGL core profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    // Create window (hidden for render-test mode)
    int width = renderTestMode ? renderTestWidth : 1280;
    int height = renderTestMode ? renderTestHeight : 720;
    const char* windowTitle = renderTestMode ? "PlanetSimulation Render Test" : "PlanetSimulation";
    
    auto window = glfwCreateWindow(width, height, windowTitle, nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return 1;
    }

    // Make context current
    glfwMakeContextCurrent(window);

    // Initialize GLEW after context is current
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    // Load scenario config
    if (!fs::exists(configPath)) {
        std::cerr << "Config file not found: " << configPath << "\n";
        std::exit(1);
    }

    try {
        PreparedScene initial(config::ScenarioConfig(config::Config::load(configPath)));
        config::ScenarioConfig scenario = std::move(initial.scenario);
        auto dynamics = std::move(initial.dynamics);
        auto bodies = std::move(initial.bodies);
        auto terrainSurfaces = std::move(initial.terrainSurfaces);
        std::vector<Mesh> planetMeshes(scenario.planets.size());
        std::vector<Mesh> waterMeshes(scenario.planets.size());
        std::vector<bool> meshReady(scenario.planets.size(), false);
        std::vector<bool> waterMeshReady(scenario.planets.size(), false);
        std::vector<int> lastLocalMask(scenario.planets.size(), 0);
        std::vector<std::vector<int>> lastFaceZones(scenario.planets.size());
        std::vector<glm::dvec3> lastEyeRadial(scenario.planets.size(), glm::dvec3(0.0));
        std::vector<std::array<int, 3>> meshZoneFaces(scenario.planets.size());
        std::vector<int> meshTriangles(scenario.planets.size(), 0);
        std::vector<int> meshSteepRefinedFaces(scenario.planets.size(), 0);
        struct PendingTerrainBuild {
            std::future<rendering::TerrainGeometry> geometry;
            glm::dvec3 eyeRadial{0.0};
            int localMask = 0;
        };
        std::vector<PendingTerrainBuild> pendingTerrain(scenario.planets.size());
        auto installLandMesh = [&](std::size_t index, rendering::TerrainGeometry geometry,
                                   const glm::dvec3& radial, int localMask) {
            meshZoneFaces[index] = geometry.zoneFaces;
            meshTriangles[index] = geometry.triangleCount();
            meshSteepRefinedFaces[index] = geometry.steepRefinedFaces;
            lastFaceZones[index] = geometry.faceZones;
            planetMeshes[index].loadTerrain(std::move(geometry));
            meshReady[index] = true;
            lastLocalMask[index] = localMask;
            lastEyeRadial[index] = radial;
        };
        auto preparePlanetMeshes = [&](const glm::dvec3& eye, bool asyncWalking = false) {
            for (std::size_t i = 0; i < scenario.planets.size(); ++i) {
                const auto& planet = scenario.planets[i];
                const glm::dvec3 localEye = bodies[i + 1].toLocalPoint(eye);
                const glm::dvec3 offset = localEye;
                const double distance = glm::length(offset);
                if (!std::isfinite(distance) || distance <= 0.0)
                    throw std::invalid_argument("Camera cannot be at a planet center");
                const glm::dvec3 radial = offset / distance;
                const double seaRadius = planet.radius +
                    planet.water.level_m / scenario.metersPerWorldUnit();
                if (planet.water.enabled && !waterMeshReady[i]) {
                    // Water has no height noise. A fixed shell avoids the ocean
                    // changing tessellation every time the eye moves.
                    const rendering::TerrainSurface seaSurface(
                        {}, planet.terrain_lod, seaRadius, scenario.metersPerWorldUnit());
                    const int budgetSegments = static_cast<int>(std::floor(std::sqrt(
                        planet.terrain_lod.max_triangle_budget / 320.0)));
                    waterMeshes[i].loadTerrain(seaSurface.buildGeometry(
                        std::max(1, std::min({8, planet.terrain_lod.max_edge_segments,
                                              budgetSegments}))));
                    waterMeshReady[i] = true;
                }
                const int localMask = distance < 3.0 * planet.radius ? 1 : 0;
                if (pendingTerrain[i].geometry.valid()) {
                    if (pendingTerrain[i].geometry.wait_for(std::chrono::seconds(0)) !=
                        std::future_status::ready) continue;
                    installLandMesh(i, pendingTerrain[i].geometry.get(),
                                    pendingTerrain[i].eyeRadial,
                                    pendingTerrain[i].localMask);
                }
                const double movedMeters = meshReady[i] ? planet.radius *
                    scenario.metersPerWorldUnit() * std::acos(std::clamp(
                        glm::dot(radial, lastEyeRadial[i]), -1.0, 1.0)) : 0.0;
                if (meshReady[i] && localMask == lastLocalMask[i] &&
                    (localMask == 0 || movedMeters < 10.0)) continue;
                if (asyncWalking && meshReady[i] && localMask == lastLocalMask[i]) {
                    // CPU noise/tessellation can take hundreds of milliseconds.
                    // Keep drawing the current mesh while an immutable copy of
                    // the terrain builds the next geometry off the render loop.
                    auto surface = terrainSurfaces[i];
                    auto zones = lastFaceZones[i];
                    pendingTerrain[i].eyeRadial = radial;
                    pendingTerrain[i].localMask = localMask;
                    pendingTerrain[i].geometry = std::async(std::launch::async,
                        [surface = std::move(surface), zones = std::move(zones),
                         localEye]() mutable {
                            return surface.buildGeometryForEye(localEye, glm::dvec3(0.0), &zones, 20.0);
                        });
                    continue;
                }
                auto geometry = terrainSurfaces[i].buildGeometryForEye(
                    localEye, glm::dvec3(0.0), meshReady[i] ? &lastFaceZones[i] : nullptr, 20.0);
                installLandMesh(i, std::move(geometry), radial, localMask);
            }
        };
        glm::dvec3 sunPosition = initial.sunPosition;
        OrbitCamera camera = std::move(initial.sunCamera);
        auto surfaceCamera = std::move(initial.surfaceCamera);
        auto planetOrbitCamera = std::move(initial.planetOrbitCamera);
        std::size_t orbitPlanetIndex = initial.orbitPlanetIndex;
        glm::dvec3 planetOrbitCenter = initial.planetOrbitCenter;
        double planetOrbitOuterRadius = initial.planetOrbitOuterRadius;
        auto updateSimulation = [&](double seconds) {
            bodies = dynamics.at(seconds);
            const glm::dvec3 sunFocusOffset(
                scenario.camera.target[0] - scenario.sun.position[0],
                scenario.camera.target[1] - scenario.sun.position[1],
                scenario.camera.target[2] - scenario.sun.position[2]);
            camera.followTarget(glm::vec3(bodies[0].position + sunFocusOffset));
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
        };
        updateSimulation(simulationTime);
        simulation::SimulationClock simulationClock(simulationTime, glfwGetTime());
        CameraInput cameraInput(camera, surfaceCamera ? &*surfaceCamera : nullptr,
                                planetOrbitCamera ? &*planetOrbitCamera : nullptr);
        InputContext inputContext{&cameraInput, &simulationClock, false};
        if (surfaceRenderMode && !surfaceCamera) {
            throw std::runtime_error("Surface render test requires surface_camera config");
        }
        if (planetRenderMode && !planetOrbitCamera)
            throw std::runtime_error("Planet orbit render test requires a configured planet");
        rendering::ClipPlanes surfaceClip;
        if (surfaceCamera) {
            surfaceClip = rendering::surfaceClipPlanes(
                surfaceCamera->configuredClearance(),
                glm::length(surfaceCamera->position() - sunPosition),
                scenario.sun.radius);
        }
        auto planetOrbitClip = [&](const glm::dvec3& eye) {
            return rendering::surfaceClipPlanes(
                std::max(0.0, glm::length(eye - planetOrbitCenter) -
                              planetOrbitOuterRadius),
                glm::length(eye - sunPosition), scenario.sun.radius);
        };
        
        // Keep the working Sun sphere geometry.
        g_mesh.generateSphere(32);
        Mesh skyboxMesh;
        skyboxMesh.generateCube();
        
        std::cout << "PlanetSimulation v0.1 initialized\n";
        std::cout << "Scenario: " << scenario.name << "\n";
        std::cout << "Sun radius: " << scenario.sun.radius << "\n";
        std::cout << "Planets: " << scenario.planets.size() << "\n";
        if (surfaceRenderMode) {
            const auto& position = surfaceCamera->position();
            std::cout << "Surface camera position: (" << position.x << ", "
                      << position.y << ", " << position.z << ")\n";
        } else if (planetRenderMode) {
            const auto& position = planetOrbitCamera->position;
            std::cout << "Planet orbit camera position: (" << position.x << ", "
                      << position.y << ", " << position.z << ")\n";
        } else {
            std::cout << "Camera position: (" << camera.position.x << ", "
                      << camera.position.y << ", " << camera.position.z << ")\n";
        }

        if (renderTestMode) {
            std::cout << (surfaceRenderMode ? "Surface render test mode enabled\n" :
                          planetRenderMode ? "Planet orbit render test mode enabled\n" :
                                             "Render test mode enabled\n");
            std::cout << "Output image: " << outputImagePath << "\n";
            
            // Make window hidden for render-test mode
            glfwSetWindowAttrib(window, GLFW_VISIBLE, GLFW_FALSE);
        } else {
            glfwSetWindowUserPointer(window, &inputContext);
            glfwSetMouseButtonCallback(window, onMouseButton);
            glfwSetCursorPosCallback(window, onCursorPosition);
            glfwSetScrollCallback(window, onScroll);
            glfwSetKeyCallback(window, onKey);
            std::cout << "Left-drag to orbit; scroll to zoom gently. Press 1 for Sun orbit";
            if (surfaceCamera) {
                std::cout << ", 2 for the planet surface view (free mouse look and WASD at "
                          << scenario.surface_camera.walk_speed_mps << " m/s)";
            }
            if (planetOrbitCamera) std::cout << ", 3 for planet orbit";
            std::cout << ". Press T to pause/resume orbits and spin. "
                      << "Press Y to halve or U to double simulation speed. "
                      << "Press Esc to release the surface cursor and 2 to capture it again. " << configPath
                      << " reloads on save; press R to reload manually."
                      << " Close the window to exit.\n";
        }

        // Create shader program
        Shader shader("shaders/basic.vert", "shaders/basic.frag", "shaders/terrain_shadow.glsl");
        Shader waterShader("shaders/water.vert", "shaders/water.frag", "shaders/terrain_shadow.glsl");
        Shader shadowShader("shaders/terrain_shadow.vert", "shaders/terrain_shadow.frag");
        rendering::TerrainShadowMaps terrainShadows;
        Shader skyboxShader("shaders/skybox.vert", "shaders/skybox.frag");
        rendering::WaterReflectionTarget waterReflection;
        
        // Enable depth testing
        glEnable(GL_DEPTH_TEST);
        
        // Render exactly one frame for render-test mode
        if (renderTestMode) {
            glfwPollEvents();
            int width = 0;
            int height = 0;
            glfwGetFramebufferSize(window, &width, &height);
            if (width <= 0 || height <= 0) {
                std::cerr << "Render test framebuffer has invalid dimensions\n";
                return 1;
            }
            const glm::mat4 view = surfaceRenderMode ? surfaceCamera->getViewMatrix() :
                                   planetRenderMode ? planetOrbitCamera->getViewMatrix() :
                                                      camera.getViewMatrix();
            const float fov = surfaceRenderMode ? surfaceCamera->fov() :
                              planetRenderMode ? planetOrbitCamera->fov : camera.fov;
            const glm::dvec3 eyeWorld = surfaceRenderMode ? surfaceCamera->position() :
                                         planetRenderMode ? glm::dvec3(planetOrbitCamera->position) :
                                                            glm::dvec3(camera.position);
            const auto meshStart = std::chrono::steady_clock::now();
            preparePlanetMeshes(eyeWorld);
            const auto meshEnd = std::chrono::steady_clock::now();
            renderScene(scenario, bodies, view, fov, eyeWorld, shader, waterShader,
                        skyboxShader, waterReflection, shadowShader, terrainShadows, g_mesh, skyboxMesh,
                        planetMeshes, waterMeshes, width, height,
                        surfaceRenderMode ? surfaceClip :
                        planetRenderMode ? planetOrbitClip(eyeWorld) :
                                           rendering::ClipPlanes{});
            for (std::size_t i = 0; i < scenario.planets.size(); ++i)
                std::cout << "Planet " << i << " terrain: " << meshTriangles[i]
                          << " triangles; far/middle/near faces: " << meshZoneFaces[i][0]
                          << "/" << meshZoneFaces[i][1] << "/" << meshZoneFaces[i][2]
                          << "; steep-refined faces: " << meshSteepRefinedFaces[i]
                          << " (budget " << scenario.planets[i].terrain_lod.max_triangle_budget
                          << ")\n";

            // Call glFinish() before reading framebuffer
            glFinish();
            const auto renderEnd = std::chrono::steady_clock::now();
            std::cout << "Mesh preparation: "
                      << std::chrono::duration<double, std::milli>(meshEnd - meshStart).count()
                      << " ms; GPU-complete render: "
                      << std::chrono::duration<double, std::milli>(renderEnd - meshEnd).count()
                      << " ms\n";
            
            // Configure pixel packing for read
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadBuffer(GL_BACK);
            
            // Read rendered framebuffer (account for vertical flip)
            std::vector<unsigned char> pixels(width * height * 4);
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            std::vector<float> depthPixels(width * height);
            glReadPixels(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, depthPixels.data());
            
            // Check for OpenGL errors after reading
            GLenum err;
            while ((err = glGetError()) != GL_NO_ERROR) {
                std::cerr << "OpenGL error: " << err << "\n";
            }

            // Vertically flip the image data (GL_READ_PIXELS reads bottom-up)
            std::vector<unsigned char> flippedPixels(width * height * 4);
            std::vector<float> flippedDepth(width * height);
            for (int y = 0; y < height; y++) {
                int srcY = height - 1 - y;
                for (int x = 0; x < width; x++) {
                    int idx = (y * width + x) * 4;
                    int srcIdx = (srcY * width + x) * 4;
                    flippedPixels[idx] = pixels[srcIdx];
                    flippedPixels[idx + 1] = pixels[srcIdx + 1];
                    flippedPixels[idx + 2] = pixels[srcIdx + 2];
                    flippedPixels[idx + 3] = pixels[srcIdx + 3];
                    flippedDepth[idx / 4] = depthPixels[srcIdx / 4];
                }
            }

            if (scenario.planets.empty()) {
                std::cerr << "Render test FAILED: No planet is configured\n";
                return 1;
            }

            const std::size_t diagnosticPlanetIndex =
                (surfaceRenderMode || planetRenderMode) ? orbitPlanetIndex : 0;
            const auto& diagnosticPlanet = scenario.planets[diagnosticPlanetIndex];
            const auto frameLighting = rendering::calculateLighting(scenario, bodies);
            const auto sunDisplay = rendering::displayColor(frameLighting.sunEmission, scenario.lighting.exposure);
            const auto diagnosticClip = surfaceRenderMode ? surfaceClip :
                planetRenderMode ? planetOrbitClip(eyeWorld) : rendering::ClipPlanes{};
            const auto sunBounds = rendering::spherePixelBounds(bodies[0].position, scenario.sun.radius,
                glm::dmat4(rendering::perspectiveProjection(fov, static_cast<float>(width) / height,
                                                           diagnosticClip) * view), width, height);
            double diagnosticHeight = diagnosticPlanet.terrain_landscape.maximumAbsoluteHeightMeters();
            for (const auto& noise : diagnosticPlanet.surface_noise) diagnosticHeight += noise.amplitude_m;
            const auto planetBounds = rendering::spherePixelBounds(bodies[diagnosticPlanetIndex + 1].position,
                diagnosticPlanet.radius + diagnosticHeight / scenario.metersPerWorldUnit(),
                glm::dmat4(rendering::perspectiveProjection(fov, static_cast<float>(width) / height,
                                                           diagnosticClip) * view), width, height);
            const rendering::FrameAnalysis analysis = rendering::analyzeFrame(
                flippedPixels, width, height, {sunDisplay.r, sunDisplay.g, sunDisplay.b}, diagnosticPlanet.color,
                diagnosticPlanet.terrain_landscape.enabled || diagnosticPlanet.water.enabled,
                scenario.skybox.background_color,
                scenario.skybox.enabled ? scenario.skybox.star_color : std::vector<double>{},
                sunBounds, flippedDepth, planetBounds);

            for (std::size_t i = 0; i < frameLighting.planets.size(); ++i) {
                const auto& light = frameLighting.planets[i];
                std::cout << scenario.planets[i].name << " lighting: direct RGB ("
                          << light.sunlight.r << ", " << light.sunlight.g << ", " << light.sunlight.b
                          << "), reflected RGB (" << light.reflectedLight.r << ", "
                          << light.reflectedLight.g << ", " << light.reflectedLight.b << ")\n";
            }

            std::cout << "Image size: " << width << "x" << height << "\n";
            std::cout << "Background pixel: (" << static_cast<int>(analysis.background[0])
                      << ", " << static_cast<int>(analysis.background[1]) << ", "
                      << static_cast<int>(analysis.background[2]) << ")\n";
            auto printBounds = [](const char* label, const rendering::PixelBounds& bounds) {
                std::cout << label << ": " << bounds.count;
                if (bounds.count > 0) {
                    std::cout << ", bounding box: (" << bounds.minX << ", "
                              << bounds.minY << ")-(" << bounds.maxX << ", "
                              << bounds.maxY << ")";
                }
                std::cout << "\n";
            };
            printBounds("Non-background pixels", analysis.drawn);
            printBounds("Sun-colored pixels", analysis.sun);
            printBounds("Planet pixels (color/depth)", analysis.planet);
            if (scenario.skybox.enabled)
                printBounds("Star-like pixels", analysis.starLike);
            if (diagnosticPlanet.water.enabled)
                printBounds("Blue water-like pixels", analysis.waterLike);

            // Write PNG using official stb_image_write API with stride parameter
            int result = stbi_write_png(outputImagePath.c_str(), width, height, 4, flippedPixels.data(), width * 4);
            
            if (result == 0) {
                std::cerr << "Failed to write PNG: " << outputImagePath << "\n";
                return 1;
            }

            if (scenario.skybox.enabled && scenario.skybox.star_density > 0.0 &&
                scenario.skybox.star_brightness > 0.0 && analysis.starLike.count == 0) {
                std::cerr << "Render test FAILED: Procedural star sky is not visible\n";
                return 1;
            }

            if (surfaceRenderMode && analysis.sun.count == 0 &&
                analysis.planet.count == 0) {
                std::cerr << "Surface render test FAILED: No configured body is visible\n";
                return 1;
            }
            if (planetRenderMode && analysis.planet.count == 0) {
                std::cerr << "Planet orbit render test FAILED: Planet is not visible\n";
                return 1;
            }
            if (planetRenderMode && meshZoneFaces[orbitPlanetIndex][2] == 0) {
                std::cerr << "Planet orbit render test FAILED: Nearby terrain zone is absent\n";
                return 1;
            }
            if (!surfaceRenderMode && !planetRenderMode && !analysis.bodiesVisible()) {
                std::cerr << "Render test FAILED: Sun or planet is not visible\n";
                return 1;
            }
            if (!surfaceRenderMode && !planetRenderMode && !analysis.bodiesSeparate()) {
                std::cerr << "Render test FAILED: Sun and planet overlap in the image\n";
                return 1;
            }

            std::cout << "Render test completed successfully\n";
            std::cout << "Output image: " << outputImagePath << "\n";
        } else {
            bool cursorCaptured = false;
            SurfaceCameraTelemetry telemetry;
            double previousFrameTime = glfwGetTime();
            std::error_code watchError;
            const auto initialConfigTime = fs::last_write_time(configPath, watchError);
            std::optional<fs::file_time_type> observedConfigTime =
                watchError ? std::nullopt :
                             std::optional<fs::file_time_type>(initialConfigTime);
            bool configChangePending = false;
            double configChangedAt = 0.0;
            double nextConfigCheckAt = previousFrameTime;
            while (!glfwWindowShouldClose(window)) {
                glfwPollEvents();
                const double watchTime = glfwGetTime();
                if (watchTime >= nextConfigCheckAt) {
                    nextConfigCheckAt = watchTime + 0.05;
                    watchError.clear();
                    const auto currentTime = fs::last_write_time(configPath, watchError);
                    const std::optional<fs::file_time_type> currentConfigTime =
                        watchError ? std::nullopt :
                                     std::optional<fs::file_time_type>(currentTime);
                    if (currentConfigTime != observedConfigTime) {
                        observedConfigTime = currentConfigTime;
                        configChangedAt = watchTime;
                        configChangePending = true;
                    } else if (configChangePending && watchTime - configChangedAt >= 0.1) {
                        configChangePending = false;
                        inputContext.reloadRequested = true;
                    }
                }
                if (inputContext.reloadRequested) {
                    inputContext.reloadRequested = false;
                    configChangePending = false;
                    watchError.clear();
                    const auto reloadTime = fs::last_write_time(configPath, watchError);
                    observedConfigTime = watchError ? std::nullopt :
                        std::optional<fs::file_time_type>(reloadTime);
                    try {
                        // Build every CPU-side replacement before touching the live scene.
                        PreparedScene staged(config::ScenarioConfig(
                            config::Config::load(configPath)));
                        const std::size_t count = staged.scenario.planets.size();
                        std::vector<Mesh> nextPlanetMeshes(count);
                        std::vector<Mesh> nextWaterMeshes(count);
                        std::vector<bool> nextMeshReady(count, false);
                        std::vector<bool> nextWaterMeshReady(count, false);
                        std::vector<int> nextLocalMask(count, 0);
                        std::vector<std::vector<int>> nextFaceZones(count);
                        std::vector<glm::dvec3> nextEyeRadial(count, glm::dvec3(0.0));
                        std::vector<std::array<int, 3>> nextZoneFaces(count);
                        std::vector<int> nextTriangles(count, 0);
                        std::vector<int> nextSteepRefinedFaces(count, 0);
                        std::vector<PendingTerrainBuild> nextPendingTerrain(count);

                        for (auto& mesh : planetMeshes) mesh.destroy();
                        for (auto& mesh : waterMeshes) mesh.destroy();
                        scenario = std::move(staged.scenario);
                        dynamics = std::move(staged.dynamics);
                        bodies = std::move(staged.bodies);
                        previousFrameTime = glfwGetTime();
                        simulationClock.reset(0.0, previousFrameTime);
                        terrainSurfaces = std::move(staged.terrainSurfaces);
                        camera = std::move(staged.sunCamera);
                        surfaceCamera = std::move(staged.surfaceCamera);
                        planetOrbitCamera = std::move(staged.planetOrbitCamera);
                        orbitPlanetIndex = staged.orbitPlanetIndex;
                        planetOrbitCenter = staged.planetOrbitCenter;
                        planetOrbitOuterRadius = staged.planetOrbitOuterRadius;
                        sunPosition = staged.sunPosition;
                        planetMeshes.swap(nextPlanetMeshes);
                        waterMeshes.swap(nextWaterMeshes);
                        meshReady.swap(nextMeshReady);
                        waterMeshReady.swap(nextWaterMeshReady);
                        lastLocalMask.swap(nextLocalMask);
                        lastFaceZones.swap(nextFaceZones);
                        lastEyeRadial.swap(nextEyeRadial);
                        meshZoneFaces.swap(nextZoneFaces);
                        meshTriangles.swap(nextTriangles);
                        meshSteepRefinedFaces.swap(nextSteepRefinedFaces);
                        pendingTerrain.swap(nextPendingTerrain);
                        cameraInput.rebind(surfaceCamera ? &*surfaceCamera : nullptr,
                                           planetOrbitCamera ? &*planetOrbitCamera : nullptr);
                        telemetry = SurfaceCameraTelemetry{};
                        std::cout << "Reloaded " << configPath << ": " << scenario.name
                                  << ", " << scenario.planets.size() << " planet(s)\n";
                    } catch (const std::exception& error) {
                        std::cerr << "Config reload failed; current scene retained: "
                                  << error.what() << '\n';
                    }
                }
                const double frameTime = glfwGetTime();
                const double frameElapsed = std::max(0.0, frameTime - previousFrameTime);
                const double elapsedSeconds = std::min(frameElapsed, 0.05);
                previousFrameTime = frameTime;
                // Orbit time uses actual elapsed wall time, independent of the
                // smaller movement step used to keep camera controls smooth.
                updateSimulation(simulationClock.advanceTo(frameTime));
                WalkKeys keys{
                    glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS,
                    glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS,
                    glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS,
                    glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS
                };
                cameraInput.update(keys, elapsedSeconds);
                const bool wantsCursorCapture = cameraInput.mode() == CameraMode::Surface &&
                                                cameraInput.surfacePointerCaptured();
                if (wantsCursorCapture != cursorCaptured) {
                    cursorCaptured = wantsCursorCapture;
                    glfwSetInputMode(window, GLFW_CURSOR,
                        cursorCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
                    if (cameraInput.autoActivated()) {
                        std::cout << "Planet walking controls activated near the surface\n";
                    }
                }
                if (surfaceCamera) {
                    const auto snapshot = telemetry.sample(
                        cameraInput.mode() == CameraMode::Surface,
                        frameTime, *surfaceCamera, scenario.surface_camera,
                        scenario.metersPerWorldUnit(), scenario.distance_unit);
                    if (snapshot) std::cout << snapshot->format() << std::flush;
                }
                int width = 0;
                int height = 0;
                glfwGetFramebufferSize(window, &width, &height);
                if (width > 0 && height > 0) {
                    const bool onSurface = cameraInput.mode() == CameraMode::Surface && surfaceCamera;
                    const bool onPlanetOrbit = cameraInput.mode() == CameraMode::PlanetOrbit &&
                                               planetOrbitCamera;
                    const glm::mat4 view = onSurface ? surfaceCamera->getViewMatrix() :
                                           onPlanetOrbit ? planetOrbitCamera->getViewMatrix() :
                                                           camera.getViewMatrix();
                    const float fov = onSurface ? surfaceCamera->fov() :
                                      onPlanetOrbit ? planetOrbitCamera->fov : camera.fov;
                    const glm::dvec3 eyeWorld = onSurface ? surfaceCamera->position() :
                                                 onPlanetOrbit ? glm::dvec3(planetOrbitCamera->position) :
                                                                 glm::dvec3(camera.position);
                    const rendering::ClipPlanes clip = onSurface
                        ? rendering::surfaceClipPlanes(
                              surfaceCamera->configuredClearance(),
                              glm::length(surfaceCamera->position() - sunPosition),
                              scenario.sun.radius)
                        : onPlanetOrbit ? planetOrbitClip(eyeWorld) : rendering::ClipPlanes{};
                    preparePlanetMeshes(eyeWorld, true);
                    renderScene(scenario, bodies, view, fov, eyeWorld, shader, waterShader,
                                skyboxShader, waterReflection, shadowShader, terrainShadows, g_mesh, skyboxMesh,
                                planetMeshes, waterMeshes, width, height,
                                clip);
                    glfwSwapBuffers(window);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }
        }

        // Cleanup
        g_mesh.destroy();
        skyboxMesh.destroy();
        for (auto& planetMesh : planetMeshes) planetMesh.destroy();
        for (auto& waterMesh : waterMeshes) waterMesh.destroy();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    // Cleanup
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

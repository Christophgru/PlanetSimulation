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
#include <stdexcept>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "config/Config.h"
#include "config/ScenarioConfig.h"
#include "rendering/Mesh.h"
#include "rendering/Terrain.h"
#include "rendering/CameraInput.h"
#include "rendering/OrbitCamera.h"
#include "rendering/PlanetSurfaceCamera.h"
#include "rendering/RenderDiagnostics.h"
#include "rendering/SceneTransforms.h"
#include "rendering/Shader.h"
#include "rendering/SurfaceCameraTelemetry.h"

namespace fs = std::filesystem;

struct InputContext {
    CameraInput* camera = nullptr;
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
        input->selectOrbit();
    }
}

struct PreparedScene {
    config::ScenarioConfig scenario;
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
          sunCamera(glm::vec3(scenario.camera.target[0], scenario.camera.target[1],
                              scenario.camera.target[2]),
                    glm::vec3(scenario.camera.position[0] - scenario.camera.target[0],
                              scenario.camera.position[1] - scenario.camera.target[1],
                              scenario.camera.position[2] - scenario.camera.target[2])),
          sunPosition(scenario.sun.position[0], scenario.sun.position[1],
                      scenario.sun.position[2]) {
        sunCamera.fov = static_cast<float>(scenario.camera.fov);
        terrainSurfaces.reserve(scenario.planets.size());
        for (const auto& planet : scenario.planets) {
            const glm::dvec3 center(planet.position[0], planet.position[1],
                                    planet.position[2]);
            if (glm::length(glm::dvec3(sunCamera.position) - center) <= 1e-12)
                throw std::invalid_argument("Configured camera cannot start at a planet center");
            terrainSurfaces.emplace_back(planet.surface_noise, planet.terrain_lod,
                                         planet.radius, scenario.metersPerWorldUnit(),
                                         planet.terrain_landscape);
        }
        if (scenario.surface_camera.enabled) {
            const auto& settings = scenario.surface_camera;
            const auto& planet = scenario.planets[settings.planet_index];
            coordinates::PlanetLocalFrame planetFrame(
                {planet.position[0], planet.position[1], planet.position[2]},
                planet.radius);
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
            planetOrbitCenter = {planet.position[0], planet.position[1],
                                 planet.position[2]};
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

void renderScene(const config::ScenarioConfig& scenario, const glm::mat4& view, float fov,
                 const glm::dvec3& eyeWorld, const Shader& shader,
                 const Shader& waterShader, const Mesh& sunMesh,
                 const std::vector<Mesh>& planetMeshes,
                 const std::vector<Mesh>& waterMeshes, int width, int height,
                 rendering::ClipPlanes clip = {}) {
    glViewport(0, 0, width, height);
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::mat4 projection = rendering::perspectiveProjection(
        fov, static_cast<float>(width) / height, clip);

    shader.use();
    shader.setMat4("projection", glm::value_ptr(projection));
    shader.setMat4("view", glm::value_ptr(view));

    const auto& sun = scenario.sun;
    const float sunRadius = static_cast<float>(sun.radius);
    glm::mat4 sunModel = rendering::sphereModel(glm::vec3(
        static_cast<float>(sun.position[0]),
        static_cast<float>(sun.position[1]),
        static_cast<float>(sun.position[2])), sunRadius);
    shader.setMat4("model", glm::value_ptr(sunModel));
    shader.setFloat3("uColor", static_cast<float>(sun.color[0]),
                     static_cast<float>(sun.color[1]),
                     static_cast<float>(sun.color[2]));
    sunMesh.draw();

    for (std::size_t i = 0; i < scenario.planets.size(); ++i) {
        const auto& planet = scenario.planets[i];
        const float radius = static_cast<float>(planet.radius);
        glm::mat4 model = rendering::sphereModel(glm::vec3(
            static_cast<float>(planet.position[0]),
            static_cast<float>(planet.position[1]),
            static_cast<float>(planet.position[2])), radius);
        shader.setMat4("model", glm::value_ptr(model));
        shader.setFloat3("uColor", static_cast<float>(planet.color[0]),
                         static_cast<float>(planet.color[1]),
                         static_cast<float>(planet.color[2]));
        planetMeshes[i].draw();
    }

    // The opaque terrain depth buffer masks this translucent, spherical sea.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    waterShader.use();
    waterShader.setMat4("projection", glm::value_ptr(projection));
    waterShader.setMat4("view", glm::value_ptr(view));
    waterShader.setFloat3("uCameraPosition", static_cast<float>(eyeWorld.x),
                          static_cast<float>(eyeWorld.y), static_cast<float>(eyeWorld.z));
    waterShader.setFloat3("uSunPosition", static_cast<float>(sun.position[0]),
                          static_cast<float>(sun.position[1]), static_cast<float>(sun.position[2]));
    waterShader.setFloat3("uSunColor", static_cast<float>(sun.color[0]),
                          static_cast<float>(sun.color[1]), static_cast<float>(sun.color[2]));
    for (std::size_t i = 0; i < scenario.planets.size(); ++i) {
        const auto& planet = scenario.planets[i];
        if (!planet.water.enabled) continue;
        const glm::vec3 center(static_cast<float>(planet.position[0]),
                               static_cast<float>(planet.position[1]),
                               static_cast<float>(planet.position[2]));
        const float radius = static_cast<float>(
            planet.radius + planet.water.level_m / scenario.metersPerWorldUnit());
        const glm::mat4 model = rendering::sphereModel(center, radius);
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
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

int main(int argc, char** argv) {
    bool renderTestMode = false;
    bool surfaceRenderMode = false;
    bool planetRenderMode = false;
    std::string outputImagePath;
    
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
    int width = renderTestMode ? 800 : 1280;
    int height = renderTestMode ? 600 : 720;
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
    const std::string configPath = "configs/scenarios/solar_system.json";
    if (!fs::exists(configPath)) {
        std::cerr << "Config file not found: " << configPath << "\n";
        std::exit(1);
    }

    try {
        PreparedScene initial(config::ScenarioConfig(config::Config::load(configPath)));
        config::ScenarioConfig scenario = std::move(initial.scenario);
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
        auto preparePlanetMeshes = [&](const glm::dvec3& eye) {
            for (std::size_t i = 0; i < scenario.planets.size(); ++i) {
                const auto& planet = scenario.planets[i];
                const glm::dvec3 center(planet.position[0], planet.position[1],
                                        planet.position[2]);
                const glm::dvec3 offset = eye - center;
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
                const double movedMeters = meshReady[i] ? planet.radius *
                    scenario.metersPerWorldUnit() * std::acos(std::clamp(
                        glm::dot(radial, lastEyeRadial[i]), -1.0, 1.0)) : 0.0;
                if (meshReady[i] && localMask == lastLocalMask[i] &&
                    (localMask == 0 || movedMeters < 10.0)) continue;
                auto geometry = terrainSurfaces[i].buildGeometryForEye(
                    eye, center, meshReady[i] ? &lastFaceZones[i] : nullptr, 20.0);
                meshZoneFaces[i] = geometry.zoneFaces;
                meshTriangles[i] = geometry.triangleCount();
                lastFaceZones[i] = geometry.faceZones;
                planetMeshes[i].loadTerrain(std::move(geometry));
                meshReady[i] = true;
                lastLocalMask[i] = localMask;
                lastEyeRadial[i] = radial;
            }
        };
        glm::dvec3 sunPosition = initial.sunPosition;
        OrbitCamera camera = std::move(initial.sunCamera);
        auto surfaceCamera = std::move(initial.surfaceCamera);
        auto planetOrbitCamera = std::move(initial.planetOrbitCamera);
        std::size_t orbitPlanetIndex = initial.orbitPlanetIndex;
        glm::dvec3 planetOrbitCenter = initial.planetOrbitCenter;
        double planetOrbitOuterRadius = initial.planetOrbitOuterRadius;
        CameraInput cameraInput(camera, surfaceCamera ? &*surfaceCamera : nullptr,
                                planetOrbitCamera ? &*planetOrbitCamera : nullptr);
        InputContext inputContext{&cameraInput, false};
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
            std::cout << ". " << configPath
                      << " reloads on save; press R to reload manually."
                      << " Close the window to exit.\n";
        }

        // Create shader program
        Shader shader("shaders/basic.vert", "shaders/basic.frag");
        Shader waterShader("shaders/water.vert", "shaders/water.frag");
        
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
            preparePlanetMeshes(eyeWorld);
            renderScene(scenario, view, fov, eyeWorld, shader, waterShader, g_mesh,
                        planetMeshes, waterMeshes, width, height,
                        surfaceRenderMode ? surfaceClip :
                        planetRenderMode ? planetOrbitClip(eyeWorld) :
                                           rendering::ClipPlanes{});
            for (std::size_t i = 0; i < scenario.planets.size(); ++i)
                std::cout << "Planet " << i << " terrain: " << meshTriangles[i]
                          << " triangles; far/middle/near faces: " << meshZoneFaces[i][0]
                          << "/" << meshZoneFaces[i][1] << "/" << meshZoneFaces[i][2]
                          << " (budget " << scenario.planets[i].terrain_lod.max_triangle_budget
                          << ")\n";

            // Call glFinish() before reading framebuffer
            glFinish();
            
            // Configure pixel packing for read
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadBuffer(GL_BACK);
            
            // Read rendered framebuffer (account for vertical flip)
            std::vector<unsigned char> pixels(width * height * 4);
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            
            // Check for OpenGL errors after reading
            GLenum err;
            while ((err = glGetError()) != GL_NO_ERROR) {
                std::cerr << "OpenGL error: " << err << "\n";
            }

            // Vertically flip the image data (GL_READ_PIXELS reads bottom-up)
            std::vector<unsigned char> flippedPixels(width * height * 4);
            for (int y = 0; y < height; y++) {
                int srcY = height - 1 - y;
                for (int x = 0; x < width; x++) {
                    int idx = (y * width + x) * 4;
                    int srcIdx = (srcY * width + x) * 4;
                    flippedPixels[idx] = pixels[srcIdx];
                    flippedPixels[idx + 1] = pixels[srcIdx + 1];
                    flippedPixels[idx + 2] = pixels[srcIdx + 2];
                    flippedPixels[idx + 3] = pixels[srcIdx + 3];
                }
            }

            if (scenario.planets.empty()) {
                std::cerr << "Render test FAILED: No planet is configured\n";
                return 1;
            }

            const std::size_t diagnosticPlanetIndex =
                (surfaceRenderMode || planetRenderMode) ? orbitPlanetIndex : 0;
            const auto& diagnosticPlanet = scenario.planets[diagnosticPlanetIndex];
            const rendering::FrameAnalysis analysis = rendering::analyzeFrame(
                flippedPixels, width, height, scenario.sun.color, diagnosticPlanet.color,
                diagnosticPlanet.terrain_landscape.enabled || diagnosticPlanet.water.enabled);

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
            printBounds("Planet-colored pixels", analysis.planet);
            if (diagnosticPlanet.water.enabled)
                printBounds("Blue water-like pixels", analysis.waterLike);

            // Write PNG using official stb_image_write API with stride parameter
            int result = stbi_write_png(outputImagePath.c_str(), width, height, 4, flippedPixels.data(), width * 4);
            
            if (result == 0) {
                std::cerr << "Failed to write PNG: " << outputImagePath << "\n";
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
            CameraMode cursorMode = CameraMode::Orbit;
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

                        for (auto& mesh : planetMeshes) mesh.destroy();
                        for (auto& mesh : waterMeshes) mesh.destroy();
                        scenario = std::move(staged.scenario);
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
                const double elapsedSeconds = std::clamp(frameTime - previousFrameTime,
                                                         0.0, 0.05);
                previousFrameTime = frameTime;
                WalkKeys keys{
                    glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS,
                    glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS,
                    glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS,
                    glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS
                };
                cameraInput.update(keys, elapsedSeconds);
                if (cameraInput.mode() != cursorMode) {
                    cursorMode = cameraInput.mode();
                    glfwSetInputMode(window, GLFW_CURSOR,
                        cursorMode == CameraMode::Surface ? GLFW_CURSOR_DISABLED
                                                          : GLFW_CURSOR_NORMAL);
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
                    preparePlanetMeshes(eyeWorld);
                    renderScene(scenario, view, fov, eyeWorld, shader, waterShader,
                                g_mesh, planetMeshes, waterMeshes,
                                width, height,
                                clip);
                    glfwSwapBuffers(window);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }
        }

        // Cleanup
        g_mesh.destroy();
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

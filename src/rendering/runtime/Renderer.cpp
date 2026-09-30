#include "rendering/runtime/RendererState.h"
#include "config/SceneReplay.h"
#include "rendering/diagnostics/VideoMemory.h"
#include <GLFW/glfw3.h>
#include <iostream>

namespace rendering {
Renderer::Renderer(app::CommandLineOptions options)
    : impl_(std::make_unique<Impl>(std::move(options))) {}
Renderer::~Renderer() = default;
int Renderer::run() {
    impl_->context.makeCurrent();
    return impl_->options.renderTestMode ? impl_->capture() : impl_->interact();
}

Renderer::Impl::Impl(app::CommandLineOptions arguments)
    : options(std::move(arguments)), source(options), context(options), window(context.get()),
      scene(config::ScenarioConfig{config::Config{nlohmann::json(source.document)}}),
      profiler(options.performanceTrace), meshes(scene.scenario.planets.size()),
      meshReady(scene.scenario.planets.size(), false),
      lastLocalMask(scene.scenario.planets.size(), 0),
      lastFaceZones(scene.scenario.planets.size()),
      lastEyeRadial(scene.scenario.planets.size(), glm::dvec3(0.0)),
      meshZoneFaces(scene.scenario.planets.size()),
      meshTriangles(scene.scenario.planets.size(), 0),
      meshSteepRefinedFaces(scene.scenario.planets.size(), 0),
      pendingTerrain(scene.scenario.planets.size()),
      simulationTime(config::replayStartTime(scene.scenario, options.commandLineTime)),
      simulationClock(simulationTime, glfwGetTime()),
      cameraInput(scene.sunCamera, scene.surfaceCamera ? &*scene.surfaceCamera : nullptr,
                  scene.planetOrbitCamera ? &*scene.planetOrbitCamera : nullptr),
      inputContext{&cameraInput, &simulationClock, false},
      atmosphere(options.atmosphereFullResolution ? 1 : 4),
      reflectionAtmosphere(options.atmosphereFullResolution ? 1 : 4),
      gpuUtilization(reinterpret_cast<const char*>(glGetString(GL_VENDOR)),
                     reinterpret_cast<const char*>(glGetString(GL_RENDERER))) {
    scene.updateSimulation(simulationTime);
    if (!options.replayPath.empty() && scene.surfaceCamera) cameraInput.selectSurface();
    if (options.surfaceRenderMode && !scene.surfaceCamera) {
        throw std::runtime_error("Surface render test requires surface_camera config");
    }
    if (options.planetRenderMode && !scene.planetOrbitCamera)
        throw std::runtime_error("Planet orbit render test requires a configured planet");
    if (scene.surfaceCamera) {
        surfaceClip = rendering::surfaceClipPlanes(
            scene.surfaceCamera->configuredClearance(),
            glm::length(scene.surfaceCamera->position() - scene.sunPosition),
            scene.scenario.sun.radius);
    }
    // Keep the working Sun sphere geometry.
    meshes.sunMesh.generateSphere(32);
    meshes.skyboxMesh.generateCube();

    std::cout << "PlanetSimulation v" << PLANET_VERSION << " initialized\n";
    std::cout << "Scenario: " << scene.scenario.name << "\n";
    std::cout << "Simulation time: " << nlohmann::json(simulationTime).dump() << " s\n";
    std::cout << "Sun radius: " << scene.scenario.sun.radius << "\n";
    std::cout << "Planets: " << scene.scenario.planets.size() << "\n";
    if (options.surfaceRenderMode) {
        const auto& position = scene.surfaceCamera->position();
        std::cout << "Surface camera position: (" << position.x << ", "
                  << position.y << ", " << position.z << ")\n";
    } else if (options.planetRenderMode) {
        const auto& position = scene.planetOrbitCamera->position;
        std::cout << "Planet orbit camera position: (" << position.x << ", "
                  << position.y << ", " << position.z << ")\n";
    } else {
        std::cout << "Camera position: (" << scene.sunCamera.position.x << ", "
                  << scene.sunCamera.position.y << ", " << scene.sunCamera.position.z << ")\n";
    }

    if (options.renderTestMode) {
        std::cout << (options.surfaceRenderMode ? "Surface render test mode enabled\n" :
                      options.planetRenderMode ? "Planet orbit render test mode enabled\n" :
                                         "Render test mode enabled\n");
        std::cout << "Output image: " << options.outputImagePath << "\n";

        // Make window hidden for render-test mode
        glfwSetWindowAttrib(window, GLFW_VISIBLE, GLFW_FALSE);
    } else {
        app::bindWindowInput(window, inputContext);
        std::cout << "Left-drag to orbit; scroll to zoom gently. Press 1 for Sun orbit";
        if (scene.surfaceCamera) {
            std::cout << ", 2 for the planet surface view (free mouse look and WASD at "
                      << scene.scenario.surface_camera.walk_speed_mps << " m/s)";
        }
        if (scene.planetOrbitCamera) std::cout << ", 3 for planet orbit";
        std::cout << ". Press T to pause/resume orbits and spin. "
                  << "Press O for ten past orbit paths and body labels. "
                  << "Press Y to halve or U to double simulation speed. "
                  << "Press Esc to release the surface cursor and 2 to capture it again. " << source.watchedScenePath
                  << " reloads on save; press R to reload manually."
                  << " Close the window to exit.\n";
    }

    auto availableMemory = availableVideoMemoryBytes();
    if (options.videoMemoryCapBytes && (!availableMemory || *options.videoMemoryCapBytes < *availableMemory))
        availableMemory = options.videoMemoryCapBytes;
    adaptiveQuality = AdaptiveQuality(availableMemory);
    glEnable(GL_DEPTH_TEST);
}

Renderer::Impl::~Impl() {
    context.makeCurrent();
    // Stop callbacks before their borrowed camera/clock pointers are destroyed.
    glfwSetWindowUserPointer(window, nullptr);
    // Workers only own CPU snapshots. Join before releasing scene resources.
    for (auto& task : pendingTerrain)
        if (task.geometry.valid()) task.geometry.wait();
}

ClipPlanes Renderer::Impl::planetOrbitClip(const glm::dvec3& eye) const {
    return rendering::surfaceClipPlanes(
        std::max(0.0, glm::length(eye - scene.planetOrbitCenter) -
                      scene.planetOrbitOuterRadius),
        glm::length(eye - scene.sunPosition), scene.scenario.sun.radius);
}
}

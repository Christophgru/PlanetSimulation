#include "rendering/runtime/RendererState.h"
#include "rendering/camera/CameraTransition.h"
#include "rendering/camera/SurfaceCameraTelemetry.h"
#include "config/SceneReplay.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/type_ptr.hpp>
#include <filesystem>
#include <iostream>
namespace fs = std::filesystem;

namespace rendering {
int Renderer::Impl::interact() {
    bool cursorCaptured = false;
    SurfaceCameraTelemetry telemetry;
    rendering::CameraTransition cameraTransition;
    rendering::CameraPose displayedPose = rendering::CameraPose::fromView(
        glm::dvec3(scene.sunCamera.position), scene.sunCamera.getViewMatrix(), scene.sunCamera.fov);
    CameraMode displayedMode = cameraInput.mode();
    double previousFrameTime = glfwGetTime();
    // Local animation follows wall time, independently of orbital pause/speed.
    // Captures keep their explicit simulation-time phase for exact replay.
    double foliageTime = simulationTime;
    simulationClock.reset(simulationTime, previousFrameTime);
    if (!options.replayPath.empty()) simulationClock.togglePause(previousFrameTime);
    std::error_code watchError;
    const auto initialConfigTime = fs::last_write_time(source.watchedScenePath, watchError);
    std::optional<fs::file_time_type> observedConfigTime =
        watchError ? std::nullopt :
                     std::optional<fs::file_time_type>(initialConfigTime);
    auto observedReloads=sceneReloads;
    bool configChangePending = false;
    double configChangedAt = 0.0;
    double nextConfigCheckAt = previousFrameTime;
    while (!glfwWindowShouldClose(window)) {
        CpuTrace::Scope frameScope("interactive.frame");
        { CpuTrace::Scope scope("glfwPollEvents"); glfwPollEvents(); }
        const bool statsVisible = inputContext.statsVisible ||
            glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS;
        profiler.beginFrame(simulationClock.seconds(), statsVisible);
        const double watchTime = glfwGetTime();
        if (watchTime >= nextConfigCheckAt) {
            nextConfigCheckAt = watchTime + 0.05;
            watchError.clear();
            const auto currentTime = fs::last_write_time(source.watchedScenePath, watchError);
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
        if (!terrainPublication) retireSceneReload(false);
        if (inputContext.reloadRequested) {
            CpuTrace::Scope reloadScope("scene.reload");
            inputContext.reloadRequested = false;
            configChangePending = false;
            watchError.clear();
            const auto reloadTime = fs::last_write_time(source.watchedScenePath, watchError);
            observedConfigTime = watchError ? std::nullopt :
                std::optional<fs::file_time_type>(reloadTime);
            try {
                reloadScene();
            } catch (const std::exception& error) {
                std::cerr << "Config reload failed; current scene retained: "
                          << error.what() << '\n';
            }
        }
        if(terrainPublication) pollResidentReload();
        if(sceneReloads!=observedReloads) {
            observedReloads=sceneReloads;
            previousFrameTime=glfwGetTime();foliageTime=simulationTime;
            cameraTransition.cancel();displayedMode=cameraInput.mode();
            displayedPose=rendering::CameraPose::fromView(
                glm::dvec3(scene.sunCamera.position),scene.sunCamera.getViewMatrix(),scene.sunCamera.fov);
            telemetry=SurfaceCameraTelemetry{};
            std::cout << "Reloaded " << source.watchedScenePath << ": " << scene.scenario.name
                      << ", " << scene.scenario.planets.size() << " planet(s)\n";
        }
        const double frameTime = glfwGetTime();
        const double frameElapsed = std::max(0.0, frameTime - previousFrameTime);
        const double elapsedSeconds = std::min(frameElapsed, 0.05);
        previousFrameTime = frameTime;
        foliageTime += frameElapsed;
        frameRate.sample(frameElapsed);
        // Orbit time uses actual elapsed wall time, independent of the
        // smaller movement step used to keep camera controls smooth.
        { rendering::FrameProfiler::Scope scope(&profiler, rendering::FrameStage::Update, false);
          scene.updateSimulation(simulationClock.advanceTo(frameTime));
          profiler.simulationTime(simulationClock.seconds()); }
        WalkKeys keys{
            glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS,
            glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS,
            glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS,
            glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS,
            glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS
        };
        cameraInput.setThirdPersonAirborne(astronaut.motion.ready() && astronaut.motion.pose().airborne);
        astronautFlightControl={int(keys.forward)-int(keys.backward),int(keys.right)-int(keys.left)};
        { CpuTrace::Scope scope("CameraInput::update");
          cameraInput.update(cameraTransition.active() || (terrainPublication && !residentSceneReady()) ?
              WalkKeys{} : keys, elapsedSeconds); }
        if (cameraInput.mode() == CameraMode::Surface && displayedMode != CameraMode::Surface &&
            displayedMode != CameraMode::ThirdPerson)
            cameraTransition.start(displayedPose, frameTime);
        if (cameraInput.mode()==CameraMode::ThirdPerson && displayedMode!=CameraMode::ThirdPerson) {
            astronaut.motion.reset();
            grass.procedural.trail(scene.scenario.surface_camera.planet_index).breakPath();
        }
        if (cameraInput.mode() != CameraMode::Surface) cameraTransition.cancel();
        if (cameraInput.mode()!=CameraMode::ThirdPerson) inputContext.spacePresses=0;
        const bool wantsCursorCapture = cameraInput.walkingMode() &&
                                        cameraInput.surfacePointerCaptured() &&
                                        !cameraTransition.active();
        if (wantsCursorCapture != cursorCaptured) {
            cursorCaptured = wantsCursorCapture;
            glfwSetInputMode(window, GLFW_CURSOR,
                cursorCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
            if (cameraInput.autoActivated()) {
                std::cout << "Planet walking controls activated near the surface\n";
            }
        }
        if (scene.surfaceCamera) {
            const auto snapshot = telemetry.sample(
                cameraInput.walkingMode() && !cameraTransition.active(),
                frameTime, *scene.surfaceCamera, scene.scenario.surface_camera,
                scene.scenario.metersPerWorldUnit(), scene.scenario.distance_unit, simulationClock.seconds());
            if (snapshot) std::cout << snapshot->format() << std::flush;
        }
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        if (width > 0 && height > 0) {
            const auto presentLoading = [&] {
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glViewport(0, 0, width, height);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                glfwSwapBuffers(window);
                profiler.endFrame();
            };
            const bool onSurface = cameraInput.mode() == CameraMode::Surface && scene.surfaceCamera;
            const bool onThird = cameraInput.mode()==CameraMode::ThirdPerson && scene.surfaceCamera;
            const bool onPlanetOrbit = cameraInput.mode() == CameraMode::PlanetOrbit &&
                                       scene.planetOrbitCamera;
            if (onThird) {
                FrameProfiler::Scope scope(&profiler,FrameStage::Mesh,false);
                // World-space flight must account for the same elapsed wall
                // time as moving celestial bodies. Ground walking stays capped.
                characterWindTime=foliageTime;
                const double characterElapsed=astronaut.motion.ready() && astronaut.motion.pose().airborne ?
                    frameElapsed : elapsedSeconds;
                preparePlanetMeshes(scene.surfaceCamera->position(),true,
                    terrainPublication ? std::optional<double>(characterElapsed) : std::nullopt);
                if (!terrainPublication || residentSceneReady()) prepareAstronaut(characterElapsed);
            }
            // Finish the mesh profiler scope before endFrame clears its stages.
            if (onThird && terrainPublication && !residentSceneReady()) {presentLoading();continue;}
            const glm::mat4 targetView = onThird ? glm::mat4(glm::lookAt(astronautView.eye,astronautView.target,astronautView.up)) :
                                   onSurface ? scene.surfaceCamera->getViewMatrix() :
                                   onPlanetOrbit ? scene.planetOrbitCamera->getViewMatrix() :
                                                   scene.sunCamera.getViewMatrix();
            const float targetFov = (onSurface || onThird) ? scene.surfaceCamera->fov() :
                              onPlanetOrbit ? scene.planetOrbitCamera->fov : scene.sunCamera.fov;
            const glm::dvec3 targetEye = onThird ? astronautView.eye :
                                         onSurface ? scene.surfaceCamera->position() :
                                         onPlanetOrbit ? glm::dvec3(scene.planetOrbitCamera->position) :
                                                         glm::dvec3(scene.sunCamera.position);
            const rendering::CameraPose targetPose = rendering::CameraPose::fromView(
                targetEye, targetView, targetFov);
            rendering::CameraPose renderPose = targetPose;
            if (onSurface && cameraTransition.active()) {
                const std::size_t index = scene.scenario.surface_camera.planet_index;
                const auto& planet = scene.scenario.planets[index];
                const glm::dvec3 center = scene.bodies[index+1].position;
                renderPose = cameraTransition.sample(frameTime, targetPose, center,
                    [&](const glm::dvec3& radial) {
                        const glm::dvec3 local = glm::transpose(scene.bodies[index+1].orientation) * radial;
                        const double terrainHeight = scene.terrainSurfaces[index].heightAt(local);
                        const double waterHeight = planet.water.enabled ?
                            planet.water.level_m / scene.scenario.metersPerWorldUnit() : -std::numeric_limits<double>::infinity();
                        return planet.radius + std::max(terrainHeight, waterHeight) +
                               scene.surfaceCamera->configuredClearance();
                    });
            }
            const glm::mat4 view = renderPose.view();
            const float fov = renderPose.fov;
            const glm::dvec3 eyeWorld = renderPose.position;
            // Match the capture path: altitude above the reference
            // sphere includes the mountain underneath us. Using it
            // here clips away nearby ground and exposes the interior.
            // Keep this small near plane throughout the descent too.
            const rendering::ClipPlanes clip = (onSurface || onThird)
                ? rendering::surfaceClipPlanes(
                      onThird ? .2/scene.scenario.metersPerWorldUnit() : scene.surfaceCamera->configuredClearance(),
                      glm::length(eyeWorld - scene.sunPosition),
                      scene.scenario.sun.radius)
                : onPlanetOrbit ? planetOrbitClip(eyeWorld) : rendering::ClipPlanes{};
            { rendering::FrameProfiler::Scope scope(&profiler, rendering::FrameStage::Mesh, false);
              if (!onThird) preparePlanetMeshes(eyeWorld, true); }
            if (terrainPublication && !residentSceneReady()) {presentLoading();continue;}
            const auto revisions=geometryRevisions();
            adaptiveQuality.observe(frameTime, frameRate.milliseconds,
                !simulationClock.paused());
            const auto [sceneWidth, sceneHeight] = adaptiveQuality.size(width, height);
            if (lastQualitySize != std::pair{sceneWidth, sceneHeight}) {
                std::cout << "Scene render resolution: " << sceneWidth << 'x' << sceneHeight
                          << " (" << std::lround(adaptiveQuality.scale() * 100) << "%)\n" << std::flush;
                lastQualitySize = {sceneWidth, sceneHeight};
            }
            const bool scaledScene = sceneWidth != width || sceneHeight != height;
            if (scaledScene) qualityTarget.ensure(sceneWidth, sceneHeight, false, 8192);
            const GLuint sceneOutput = scaledScene ? qualityTarget.framebuffer() : 0;
            const bool pending=terrainJobs.pending(terrainSceneEpoch) || (terrainPublication && terrainPublication->pending());
            const bool windAnimating=std::any_of(scene.scenario.planets.begin(),scene.scenario.planets.end(),
                [](const auto& planet) {
                    return planet.foliage.enabled && planet.foliage.wind_strength>0 &&
                           planet.foliage.wind_noise.speed_multiplier>0;
                });
            if (rendering::hasAtmosphere(scene.scenario) && frameReuse.matches(view,fov,sceneWidth,sceneHeight,
                    simulationClock.seconds(),revisions,simulationClock.paused(),pending || windAnimating ||
                    onThird,eyeWorld,static_cast<int>(cameraInput.mode()))) {
                rendering::FrameProfiler::Scope scope(&profiler,rendering::FrameStage::CachedPresentation);
                atmosphere.presentCached(atmosphereShader, sceneOutput); profiler.sceneReuse();
            } else {
                renderScene(scene.scenario, scene.atmosphereOptics, scene.bodies, view, fov, eyeWorld, shader, waterShader,
                            skyboxShader, waterReflection, shadowShader, terrainShadows,
                            atmosphereShader, atmosphere, reflectionAtmosphere, atmosphereColumns, meshes.sunMesh, meshes.skyboxMesh,
                            meshes.planetMeshes, meshes.waterMeshes, sceneWidth, sceneHeight,
                            clip, (onSurface || onThird || onPlanetOrbit) ? std::optional<std::size_t>(scene.orbitPlanetIndex) : std::nullopt,
                            false, &profiler, false, sceneOutput, &grass, foliageTime,onThird ? &astronaut : nullptr,
                            nullptr,terrainPublication.get(),&terrainConsumers);
                frameReuse.remember(view,fov,sceneWidth,sceneHeight,simulationClock.seconds(),revisions,eyeWorld,static_cast<int>(cameraInput.mode()));
            }
            const bool showOrbits = inputContext.orbitsVisible && cameraInput.mode() == CameraMode::Orbit;
            if (showOrbits && !scene.scenario.planets.empty()) {
                double shortestPeriod = std::numeric_limits<double>::infinity();
                for (std::size_t body = 1; body < scene.dynamics.size(); ++body)
                    shortestPeriod = std::min(shortestPeriod, scene.dynamics.periodSeconds(body));
                const double now = simulationClock.seconds();
                if (orbitTrails.size() != scene.scenario.planets.size() ||
                    !std::isfinite(orbitTrailEpoch) ||
                    std::abs(now - orbitTrailEpoch) > shortestPeriod * 0.01) {
                    orbitTrails = rendering::pastOrbitTrails(scene.dynamics, now);
                    orbitTrailEpoch = now;
                }
                if (orbitColorRevisions != revisions || orbitColors.size() != scene.scenario.planets.size()) {
                    orbitColors.clear();
                    for (std::size_t i = 0; i < scene.scenario.planets.size(); ++i)
                        orbitColors.push_back(rendering::averageSurfaceColor(
                            scene.scenario.planets[i], meshes.planetMeshes[i].vertices, meshes.planetMeshes[i].hasVertexColors));
                    orbitColorRevisions = revisions;
                }
                glBindFramebuffer(GL_FRAMEBUFFER, sceneOutput);
                glViewport(0, 0, sceneWidth, sceneHeight);
                const glm::mat4 orbitProjection = rendering::perspectiveProjection(
                    fov, static_cast<float>(sceneWidth) / sceneHeight, clip);
                orbitOverlay.paths(orbitTrails, orbitColors, orbitProjection * view);
            }
            if (scaledScene) {
                // Default windows may be multisampled; a texture draw works
                // for both MSAA and single-sample targets.
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glViewport(0, 0, width, height);
                glDisable(GL_DEPTH_TEST);
                glDepthMask(GL_FALSE);
                glDisable(GL_BLEND);
                glDisable(GL_STENCIL_TEST);
                qualityPresent.use();
                qualityPresent.setInt("uScene", 0);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, qualityTarget.colorTexture());
                glBindVertexArray(qualityVao.id);
                glDrawArrays(GL_TRIANGLES, 0, 3);
                glBindVertexArray(0);
                glBindTexture(GL_TEXTURE_2D, 0);
                glDepthMask(GL_TRUE);
                glEnable(GL_DEPTH_TEST);
            }
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, width, height);
            if (showOrbits && !scene.scenario.planets.empty()) {
                const glm::mat4 orbitProjection = rendering::perspectiveProjection(
                    fov, static_cast<float>(width) / height, clip);
                orbitOverlay.labels(scene.scenario, scene.bodies, scene.dynamics, orbitColors,
                                    orbitProjection * view, width, height);
            }
            { rendering::FrameProfiler::Scope scope(&profiler, rendering::FrameStage::Overlay);
              performanceOverlay.draw(statsVisible, width, height,
                  frameRate.fps, frameRate.milliseconds, profiler.gpuMilliseconds, profiler.gpuReady(), gpuUtilization.sample(statsVisible)); }
            { rendering::FrameProfiler::Scope scope(&profiler, rendering::FrameStage::Present, false);
              glfwSwapBuffers(window); }
            displayedPose = renderPose;
            displayedMode = cameraInput.mode();
        }
        if (width <= 0 || height <= 0) glfwWaitEventsTimeout(0.05);
        profiler.endFrame();
    }
    return 0;
}
}

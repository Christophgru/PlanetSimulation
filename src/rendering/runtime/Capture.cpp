#include "rendering/runtime/RendererState.h"
#include "rendering/diagnostics/PngWriter.h"
#include "rendering/diagnostics/RenderDiagnostics.h"
#include "rendering/camera/SurfaceCameraTelemetry.h"
#include "config/SceneReplay.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>

namespace rendering {
int Renderer::Impl::capture() {
    CpuTrace::Scope captureScope("Renderer::capture");
    glfwPollEvents();
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    if (width <= 0 || height <= 0) {
        std::cerr << "Render test framebuffer has invalid dimensions\n";
        return 1;
    }
    const auto meshStart = std::chrono::steady_clock::now();
    auto meshEnd = meshStart;
    rendering::CameraExposure frameExposure;
    const double benchmarkStart = simulationTime;
    double lastBenchmarkFrame = glfwGetTime();
    for (int frame = 0; frame < options.benchmarkFrames; ++frame) {
        CpuTrace::Scope frameScope("capture.frame");
        frameRate.sample(glfwGetTime()-lastBenchmarkFrame); lastBenchmarkFrame=glfwGetTime();
        simulationTime = benchmarkStart + frame * options.benchmarkStep;
        profiler.beginFrame(simulationTime);
        { rendering::FrameProfiler::Scope scope(&profiler, rendering::FrameStage::Update, false);
          if (frame > 0) {
              scene.updateSimulation(simulationTime);
              if (options.benchmarkWalkStep > 0)
                  scene.surfaceCamera->walk(1, 0, options.benchmarkWalkStep /
                      (scene.surfaceCamera->walkSpeed() * scene.scenario.metersPerWorldUnit()));
          } }
        const glm::mat4 view = options.surfaceRenderMode ? scene.surfaceCamera->getViewMatrix() :
                               options.planetRenderMode ? scene.planetOrbitCamera->getViewMatrix() :
                                                  scene.sunCamera.getViewMatrix();
        const float fov = options.surfaceRenderMode ? scene.surfaceCamera->fov() :
                          options.planetRenderMode ? scene.planetOrbitCamera->fov : scene.sunCamera.fov;
        const glm::dvec3 eyeWorld = options.surfaceRenderMode ? scene.surfaceCamera->position() :
                                     options.planetRenderMode ? glm::dvec3(scene.planetOrbitCamera->position) :
                                                        glm::dvec3(scene.sunCamera.position);
        { rendering::FrameProfiler::Scope scope(&profiler, rendering::FrameStage::Mesh, false);
          preparePlanetMeshes(eyeWorld); }
        if (frame == 0) meshEnd = std::chrono::steady_clock::now();
        const auto revisions=geometryRevisions();
        if (rendering::hasAtmosphere(scene.scenario) && frameReuse.matches(view,fov,width,height,simulationTime,revisions,options.benchmarkStep==0,false,eyeWorld,options.surfaceRenderMode ? 1 : options.planetRenderMode ? 2 : 0)) {
            rendering::FrameProfiler::Scope scope(&profiler,rendering::FrameStage::CachedPresentation);
            atmosphere.presentCached(atmosphereShader); profiler.sceneReuse();
        } else {
            frameExposure = renderScene(scene.scenario, scene.bodies, view, fov, eyeWorld, shader, waterShader,
                        skyboxShader, waterReflection, shadowShader, terrainShadows,
                        atmosphereShader, atmosphere, reflectionAtmosphere, atmosphereColumns, meshes.sunMesh, meshes.skyboxMesh,
                        meshes.planetMeshes, meshes.waterMeshes, width, height,
                        options.surfaceRenderMode ? surfaceClip :
                        options.planetRenderMode ? planetOrbitClip(eyeWorld) :
                                           rendering::ClipPlanes{},
                        (options.surfaceRenderMode || options.planetRenderMode) ? std::optional<std::size_t>(scene.orbitPlanetIndex) : std::nullopt, true, &profiler, false, 0, &grass, simulationTime);
            frameReuse.remember(view,fov,width,height,simulationTime,revisions,eyeWorld,options.surfaceRenderMode ? 1 : options.planetRenderMode ? 2 : 0);
        }
        { rendering::FrameProfiler::Scope scope(&profiler, rendering::FrameStage::Overlay);
          performanceOverlay.draw(options.benchmarkOverlay,width,height,frameRate.fps,frameRate.milliseconds,
              profiler.gpuMilliseconds,profiler.gpuReady(),gpuUtilization.sample(options.benchmarkOverlay)); }
        if (frame + 1 < options.benchmarkFrames) {
            rendering::FrameProfiler::Scope scope(&profiler, rendering::FrameStage::Present, false);
            glfwSwapBuffers(window); glfwPollEvents();
        }
        profiler.endFrame();
    }
    for (std::size_t i = 0; i < scene.scenario.planets.size(); ++i) {
        std::cout << "Planet " << i << " terrain: " << meshTriangles[i]
                  << " triangles; far/middle/near faces: " << meshZoneFaces[i][0]
                  << "/" << meshZoneFaces[i][1] << "/" << meshZoneFaces[i][2]
                  << "; steep-refined faces: " << meshSteepRefinedFaces[i]
                  << " (budget " << scene.scenario.planets[i].terrain_lod.max_triangle_budget
                  << "); foliage blades: " << grass.count(i)
                  << "; water triangles: " << meshes.waterMeshes[i].indices.size()/3 << "\n";
        std::cout << "Planet " << i << " terrain LOD faces (coarse to fine): ";
        for (int level = 0; level < 8; ++level) {
            if (level) std::cout << '/';
            std::cout << std::count(lastFaceZones[i].begin(), lastFaceZones[i].end(), level);
        }
        std::cout << "; mesh bytes: " << meshes.planetMeshes[i].vertices.size()*sizeof(float) +
            meshes.planetMeshes[i].indices.size()*sizeof(unsigned int) << '\n';
        const auto stats=grass.drawStats(i);
        std::cout << "Planet " << i << " foliage candidate bounds per pass: " << stats.vertices
                  << " vertices; " << stats.triangles << " triangles; " << stats.batches
                  << " batches; instance bytes: " << stats.instanceBytes << '\n';
        const auto horizon=grass.horizon.stats(i);
        std::cout << "Planet " << i << " horizon foliage candidate bounds per pass: " << horizon.candidates
                  << " candidates; " << horizon.patches << " patches; " << horizon.vertices
                  << " vertices; " << horizon.triangles << " triangles; " << horizon.batches
                  << " batches; patch bytes: " << horizon.patchBytes
                  << "; distance: " << horizon.distanceMeters << " m\n";
        for (const auto* layer:{&grass.near,&grass.horizon}) if (layer->usesCompute(i)) {
            const auto counts=layer->computedCounts(i);
            std::cout << "Planet " << i << (layer==&grass.near ? " near" : " horizon")
                << " GPU grass draws: " << counts[0] << " detailed; " << counts[1]
                << " quads; " << counts[0]*14+counts[1]*4 << " vertices; "
                << counts[0]*12+counts[1]*2 << " triangles; GPU working bytes: "
                << layer->stats(i).gpuBytes << '\n';
        }
    }

    // Call glFinish() before reading framebuffer
    { CpuTrace::Scope scope("capture.gpu_finish"); glFinish(); }
    profiler.collect();
    const auto renderEnd = std::chrono::steady_clock::now();
    std::cout << "Mesh preparation: "
              << std::chrono::duration<double, std::milli>(meshEnd - meshStart).count()
              << " ms; GPU-complete " << (options.benchmarkFrames > 1 ? "benchmark" : "render") << ": "
              << std::chrono::duration<double, std::milli>(renderEnd - meshEnd).count()
              << " ms\n";

    const glm::dvec3 eyeWorld = options.surfaceRenderMode ? scene.surfaceCamera->position() :
        options.planetRenderMode ? glm::dvec3(scene.planetOrbitCamera->position) : glm::dvec3(scene.sunCamera.position);
    const glm::mat4 view = options.surfaceRenderMode ? scene.surfaceCamera->getViewMatrix() :
        options.planetRenderMode ? scene.planetOrbitCamera->getViewMatrix() : scene.sunCamera.getViewMatrix();
    const float fov = options.surfaceRenderMode ? scene.surfaceCamera->fov() :
        options.planetRenderMode ? scene.planetOrbitCamera->fov : scene.sunCamera.fov;
    // Configure pixel packing for read
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);

    // Read rendered framebuffer (account for vertical flip)
    std::vector<unsigned char> pixels(width * height * 4);
    CpuTrace::Scope readbackScope("capture.readback");
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    std::vector<float> depthPixels(width * height);
    glReadPixels(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, depthPixels.data());
    std::vector<unsigned char> objectPixels(width * height);
    glReadPixels(0, 0, width, height, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, objectPixels.data());
    readbackScope.stop();

    // Check for OpenGL errors after reading
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        std::cerr << "OpenGL error: " << err << "\n";
        throw std::runtime_error("OpenGL error during frame capture");
    }

    // Vertically flip the image data (GL_READ_PIXELS reads bottom-up)
    std::vector<unsigned char> flippedPixels(width * height * 4);
    std::vector<float> flippedDepth(width * height);
    std::vector<unsigned char> flippedObjects(width * height);
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
            flippedObjects[idx / 4] = objectPixels[srcIdx / 4];
        }
    }

    if (scene.scenario.planets.empty()) {
        std::cerr << "Render test FAILED: No planet is configured\n";
        return 1;
    }

    const std::size_t diagnosticPlanetIndex =
        (options.surfaceRenderMode || options.planetRenderMode) ? scene.orbitPlanetIndex : 0;
    const auto& diagnosticPlanet = scene.scenario.planets[diagnosticPlanetIndex];
    const auto frameLighting = rendering::calculateLighting(scene.scenario, scene.bodies);
    const auto sunDisplay = rendering::displayColor(frameLighting.sunEmission, frameExposure.exposure);
    const auto diagnosticClip = options.surfaceRenderMode ? surfaceClip :
        options.planetRenderMode ? planetOrbitClip(eyeWorld) : rendering::ClipPlanes{};
    const auto sunBounds = rendering::spherePixelBounds(scene.bodies[0].position, scene.scenario.sun.radius,
        glm::dmat4(rendering::perspectiveProjection(fov, static_cast<float>(width) / height,
                                                   diagnosticClip) * view), width, height);
    double diagnosticHeight = diagnosticPlanet.terrain_landscape.maximumAbsoluteHeightMeters();
    for (const auto& noise : diagnosticPlanet.surface_noise) diagnosticHeight += noise.amplitude_m;
    const auto planetBounds = rendering::spherePixelBounds(scene.bodies[diagnosticPlanetIndex + 1].position,
        diagnosticPlanet.radius + diagnosticHeight / scene.scenario.metersPerWorldUnit(),
        glm::dmat4(rendering::perspectiveProjection(fov, static_cast<float>(width) / height,
                                                   diagnosticClip) * view), width, height);
    std::vector<double> skyBackground = scene.scenario.skybox.background_color;
    for (double& channel : skyBackground) channel *= frameExposure.skySensitivity;
    const rendering::FrameAnalysis analysis = rendering::analyzeFrame(
        flippedPixels, width, height, {sunDisplay.r, sunDisplay.g, sunDisplay.b}, diagnosticPlanet.color,
        diagnosticPlanet.terrain_landscape.enabled || diagnosticPlanet.water.enabled,
        skyBackground,
        scene.scenario.skybox.enabled ? scene.scenario.skybox.star_color : std::vector<double>{},
        sunBounds, flippedDepth, planetBounds, flippedObjects);

    for (std::size_t i = 0; i < frameLighting.planets.size(); ++i) {
        const auto& light = frameLighting.planets[i];
        std::cout << scene.scenario.planets[i].name << " lighting: direct RGB ("
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
    std::cout << "Camera exposure: " << frameExposure.exposure
              << "; sky sensitivity: " << frameExposure.skySensitivity << '\n';
    if (scene.scenario.skybox.enabled)
        printBounds("Star-like pixels", analysis.starLike);
    if (diagnosticPlanet.water.enabled)
        printBounds("Blue water-like pixels", analysis.waterLike);

    { CpuTrace::Scope scope("capture.write_png");
      rendering::writePng(options.outputImagePath, width, height, 4, flippedPixels); }

    if (options.surfaceRenderMode) {
        const auto snapshot = SurfaceCameraTelemetry::capture(*scene.surfaceCamera, scene.scenario.surface_camera,
            scene.scenario.metersPerWorldUnit(), scene.scenario.distance_unit, simulationTime);
        std::cout << snapshot.format();
        const auto metrics = rendering::measureLightingFrame(flippedPixels, flippedDepth, width, height,
                                                             planetBounds, sunBounds, flippedObjects);
        std::cout << "White-clipped pixels (RGB >= 250): " << metrics.whiteClippedPixels
                  << " (" << metrics.whiteClippedFraction * 100.0 << "%)\n";
        GLint samples = 0;
        glGetIntegerv(GL_SAMPLES, &samples);
        const auto& atmosphereConfig = scene.scenario.planets[scene.orbitPlanetIndex].atmosphere;
        const auto optics = simulation::atmosphereOptics(atmosphereConfig,
            scene.scenario.planets[scene.orbitPlanetIndex].radius * scene.scenario.metersPerWorldUnit(),
            simulation::referenceAir(atmosphereConfig));
        const auto drawnNear=grass.near.computedCounts(scene.orbitPlanetIndex);
        const auto drawnFar=grass.horizon.computedCounts(scene.orbitPlanetIndex);
        const nlohmann::json metadata{
            {"schema_version", 1}, {"application_version", PLANET_VERSION}, {"scenario", source.document},
            {"surface_camera", snapshot.startConfig},
            {"render", {{"width", width}, {"height", height}, {"samples", frameExposure.hdrOutput ? 0 : samples},
                {"renderer", reinterpret_cast<const char*>(glGetString(GL_RENDERER))},
                {"exposure", frameExposure.exposure}, {"sky_sensitivity", frameExposure.skySensitivity},
                {"direct_illuminance", frameExposure.directIlluminance},
                {"reflected_illuminance", frameExposure.reflectedIlluminance},
                {"atmospheric_illuminance", frameExposure.atmosphericIlluminance},
                {"metered_illuminance", frameExposure.meteredIlluminance},
                {"atmosphere_refractive_index", optics.refractiveIndex},
                {"atmosphere_refraction_enabled", atmosphereConfig.enabled && atmosphereConfig.refraction_enabled},
                {"atmosphere_downsample", options.atmosphereFullResolution ? 1 : 4},
                {"atmosphere_relative_humidity", optics.relativeHumidity},
                {"atmosphere_liquid_water_g_m3", optics.suspendedLiquidWaterGm3},
                {"white_clipped_pixels", metrics.whiteClippedPixels},
                {"white_clipped_fraction", metrics.whiteClippedFraction},
                {"terrain_pixels", metrics.terrainPixels}, {"sky_pixels", metrics.skyPixels},
                {"foliage_blades", grass.count(scene.orbitPlanetIndex)},
                {"foliage_candidates", grass.count(scene.orbitPlanetIndex)},
                {"foliage_patches", grass.near.stats(scene.orbitPlanetIndex).patches},
                {"foliage_patch_bytes", grass.near.stats(scene.orbitPlanetIndex).patchBytes},
                {"foliage_gpu_compute", grass.near.usesCompute(scene.orbitPlanetIndex)},
                {"horizon_foliage_gpu_compute", grass.horizon.usesCompute(scene.orbitPlanetIndex)},
                {"foliage_gpu_drawn_blades", drawnNear[0]+drawnNear[1]},
                {"foliage_gpu_vertices", drawnNear[0]*14+drawnNear[1]*4},
                {"foliage_gpu_triangles", drawnNear[0]*12+drawnNear[1]*2},
                {"horizon_foliage_gpu_drawn_tufts", drawnFar[0]+drawnFar[1]},
                {"horizon_foliage_gpu_vertices", drawnFar[0]*14+drawnFar[1]*4},
                {"horizon_foliage_gpu_triangles", drawnFar[0]*12+drawnFar[1]*2},
                {"foliage_gpu_working_bytes", grass.near.stats(scene.orbitPlanetIndex).gpuBytes+
                    grass.horizon.stats(scene.orbitPlanetIndex).gpuBytes},
                {"horizon_foliage_candidates", grass.horizon.stats(scene.orbitPlanetIndex).candidates},
                {"horizon_foliage_patches", grass.horizon.stats(scene.orbitPlanetIndex).patches},
                {"horizon_foliage_patch_bytes", grass.horizon.stats(scene.orbitPlanetIndex).patchBytes},
                {"horizon_foliage_distance_m", grass.horizon.stats(scene.orbitPlanetIndex).distanceMeters},
                {"terrain_mean_display_luminance", metrics.terrainMeanLuminance},
                {"terrain_max_display_luminance", metrics.terrainMaxLuminance},
                {"terrain_luminance_stddev", metrics.terrainLuminanceStddev},
                {"sky_mean_display_luminance", metrics.skyMeanLuminance},
                {"sky_interior_pixels", metrics.skyInteriorPixels},
                {"sky_interior_red", metrics.skyInteriorMeanRGB[0]},
                {"sky_interior_green", metrics.skyInteriorMeanRGB[1]},
                {"sky_interior_blue", metrics.skyInteriorMeanRGB[2]},
                {"sky_interior_mean_display_luminance", metrics.skyInteriorMeanLuminance},
                {"non_background_pixels", analysis.drawn.count}}}
        };
        std::ofstream sidecar(options.outputImagePath + ".json");
        sidecar << metadata.dump(2) << '\n';
        if (!sidecar) throw std::runtime_error("Failed to write capture metadata");
        if (options.captureOnly && metrics.terrainPixels == 0)
            throw std::runtime_error("Surface capture has no terrain geometry in view");
    }

    if (!options.captureOnly && scene.scenario.skybox.enabled && scene.scenario.skybox.star_density > 0.0 &&
        scene.scenario.skybox.star_brightness * frameExposure.skySensitivity >= 0.1 && analysis.starLike.count == 0) {
        std::cerr << "Render test FAILED: Procedural star sky is not visible\n";
        return 1;
    }

    if (!options.captureOnly && options.surfaceRenderMode && analysis.sun.count == 0 &&
        analysis.planet.count == 0) {
        std::cerr << "Surface render test FAILED: No configured body is visible\n";
        return 1;
    }
    if (options.planetRenderMode && analysis.planet.count == 0) {
        std::cerr << "Planet orbit render test FAILED: Planet is not visible\n";
        return 1;
    }
    if (options.planetRenderMode && meshZoneFaces[scene.orbitPlanetIndex][2] == 0) {
        std::cerr << "Planet orbit render test FAILED: Nearby terrain zone is absent\n";
        return 1;
    }
    if (!options.surfaceRenderMode && !options.planetRenderMode && !analysis.bodiesVisible()) {
        std::cerr << "Render test FAILED: Sun or planet is not visible\n";
        return 1;
    }
    if (!options.surfaceRenderMode && !options.planetRenderMode && !analysis.bodiesSeparate()) {
        std::cerr << "Render test FAILED: Sun and planet overlap in the image\n";
        return 1;
    }

    std::cout << "Render test completed successfully\n";
    std::cout << "Output image: " << options.outputImagePath << "\n";
    return 0;
}
}

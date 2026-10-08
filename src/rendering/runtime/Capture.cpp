#include "rendering/runtime/RendererState.h"
#include "rendering/geometry/contacts/SparseTerrainContacts.h"
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
    profiler.publications().captureMode(true);
    PublicationProfiler::CaptureFailure failure(profiler.publications(),terrainSceneEpoch);
    glfwPollEvents();
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    if (width <= 0 || height <= 0) {
        std::cerr << "Render test framebuffer has invalid dimensions\n";
        profiler.publications().captureFailed(terrainSceneEpoch);
        return 1;
    }
    const auto meshStart = std::chrono::steady_clock::now();
    auto meshEnd = meshStart;
    rendering::CameraExposure frameExposure;
    std::vector<std::array<std::size_t,2>> mainGrassCounts(scene.scenario.planets.size());
    const double benchmarkStart = simulationTime;
    double lastBenchmarkFrame = glfwGetTime();
    double windStart=benchmarkStart;
    if (options.thirdPersonRenderMode && !options.replayPath.empty()) {
        const auto& replay=source.replayDocument;
        if (replay.contains("astronaut_pose"))
            windStart=replay.at("astronaut_pose").value("wind_time_s",windStart);
    }

    if (!std::isfinite(windStart) || std::abs(windStart)>1e12)
        throw std::invalid_argument("Invalid character wind replay clock");
    for (int frame = 0; frame < options.benchmarkFrames; ++frame) {
        CpuTrace::Scope frameScope("capture.frame");
        frameRate.sample(glfwGetTime()-lastBenchmarkFrame); lastBenchmarkFrame=glfwGetTime();
        simulationTime = benchmarkStart + frame * options.benchmarkStep;
        profiler.beginFrame(simulationTime,grass.procedural.adaptiveBudget());
        observeMemory(MemoryPhase::Capture);
        { rendering::FrameProfiler::Scope scope(&profiler, rendering::FrameStage::Update, false);
          if (frame > 0) {
              scene.updateSimulation(simulationTime);
              if (options.benchmarkWalkStep > 0 && !(options.thirdPersonRenderMode && astronaut.motion.ready() && astronaut.motion.pose().airborne))
                  scene.surfaceCamera->walk(1, 0, options.benchmarkWalkStep /
                      (scene.surfaceCamera->walkSpeed() * scene.scenario.metersPerWorldUnit()));
          } }
        if (options.thirdPersonRenderMode) {
            FrameProfiler::Scope scope(&profiler,FrameStage::Mesh,false);
            if (frame==options.benchmarkJumpFrame || frame==options.benchmarkBoostFrame) ++inputContext.spacePresses;
            astronautBenchmarkBoost=options.benchmarkBoostFrame>=0 && frame>=options.benchmarkBoostFrame;
            astronautFlightControl={options.benchmarkWalkStep>0 ? 1.0 : 0.0,0};
            const double localStep=options.benchmarkCharacterStep>0 ? options.benchmarkCharacterStep : options.benchmarkWalkStep/6.0;
            characterWindTime=windStart+frame*localStep;
            const double characterElapsed=frame>0 ? localStep : 0;
            preparePlanetMeshes(scene.surfaceCamera->position(),false,characterElapsed);
            prepareAstronaut(frame>0 ? (options.benchmarkCharacterStep>0 ? options.benchmarkCharacterStep : options.benchmarkWalkStep/6.0) : 0);
            if(plannedCharacterEye && *plannedCharacterEye!=astronautView.eye)
                throw std::logic_error("Character planning eye differs from committed contact update");
            if(reloadCharacterPending) {
                if(*reloadCharacterEye!=astronautView.eye)
                    throw std::logic_error("Reload character planning eye differs from committed update");
                reloadCharacterPending=false;
            }
            // A destination handoff changes the distance to the Sun. Derive
            // clipping from the current chase eye, including on saved replay.
            surfaceClip = rendering::surfaceClipPlanes(.2/scene.scenario.metersPerWorldUnit(),
                glm::length(astronautView.eye-scene.sunPosition), scene.scenario.sun.radius);
        }
        const glm::mat4 view = options.thirdPersonRenderMode ? glm::mat4(glm::lookAt(astronautView.eye,astronautView.target,astronautView.up)) :
                               options.surfaceRenderMode ? scene.surfaceCamera->getViewMatrix() :
                               options.planetRenderMode ? scene.planetOrbitCamera->getViewMatrix() :
                                                  scene.sunCamera.getViewMatrix();
        const float fov = options.surfaceRenderMode ? scene.surfaceCamera->fov() :
                          options.planetRenderMode ? scene.planetOrbitCamera->fov : scene.sunCamera.fov;
        const glm::dvec3 eyeWorld = options.thirdPersonRenderMode ? astronautView.eye :
                                     options.surfaceRenderMode ? scene.surfaceCamera->position() :
                                     options.planetRenderMode ? glm::dvec3(scene.planetOrbitCamera->position) :
                                                        glm::dvec3(scene.sunCamera.position);
        { rendering::FrameProfiler::Scope scope(&profiler, rendering::FrameStage::Mesh, false);
          if (!options.thirdPersonRenderMode) preparePlanetMeshes(eyeWorld); }
        if (frame == 0) meshEnd = std::chrono::steady_clock::now();
        const auto revisions=geometryRevisions();
        if (!options.offlineQuality && rendering::hasAtmosphere(scene.scenario) && frameReuse.matches(view,fov,width,height,simulationTime,revisions,options.benchmarkStep==0,options.thirdPersonRenderMode,eyeWorld,options.surfaceRenderMode ? 1 : options.planetRenderMode ? 2 : 0)) {
            rendering::FrameProfiler::Scope scope(&profiler,rendering::FrameStage::CachedPresentation);
            atmosphere.presentCached(atmosphereShader); profiler.sceneReuse();
        } else {
            frameExposure = renderScene(scene.scenario, scene.atmosphereOptics, scene.bodies, view, fov, eyeWorld, shader, waterShader,
                        skyboxShader, waterReflection, shadowShader, terrainShadows,
                        atmosphereShader, atmosphere, reflectionAtmosphere, atmosphereColumns, meshes.sunMesh, meshes.skyboxMesh,
                        meshes.planetMeshes, meshes.waterMeshes, width, height,
                        options.surfaceRenderMode ? surfaceClip :
                        options.planetRenderMode ? planetOrbitClip(eyeWorld) :
                                           rendering::ClipPlanes{},
                        (options.surfaceRenderMode || options.planetRenderMode) ? std::optional<std::size_t>(scene.orbitPlanetIndex) : std::nullopt,
                        true, &profiler, false, 0, &grass, options.thirdPersonRenderMode ? characterWindTime : simulationTime,options.thirdPersonRenderMode ? &astronaut : nullptr,&mainGrassCounts,
                        terrainPublication.get(),&terrainConsumers);
            recordRenderedPublications(options.thirdPersonRenderMode);
            frameReuse.remember(view,fov,width,height,simulationTime,revisions,eyeWorld,options.surfaceRenderMode ? 1 : options.planetRenderMode ? 2 : 0);
        }
        if (lensFlare) {
            CpuTrace::Scope trace("capture.lens_flare");
            FrameProfiler::Scope scope(&profiler,FrameStage::LensFlare);
            const auto clip=options.surfaceRenderMode ? surfaceClip : options.planetRenderMode ? planetOrbitClip(eyeWorld) : ClipPlanes{};
            flareEvidence=lensFlare->draw(view,perspectiveProjection(fov,float(width)/height,clip),
                eyeWorld,scene.bodies[0].position,scene.scenario.sun.radius,
                glm::vec3(scene.scenario.sun.color[0],scene.scenario.sun.color[1],scene.scenario.sun.color[2]),width,height);
        }
        { rendering::FrameProfiler::Scope scope(&profiler, rendering::FrameStage::Overlay);
          performanceOverlay.draw(options.benchmarkOverlay,width,height,frameRate.fps,frameRate.milliseconds,
              profiler.gpuMilliseconds,profiler.gpuReady(),gpuUtilization.sample(options.benchmarkOverlay)); }
        if (frame + 1 < options.benchmarkFrames) {
            rendering::FrameProfiler::Scope scope(&profiler, rendering::FrameStage::Present, false);
            glfwSwapBuffers(window); glfwPollEvents();
        }
        profiler.endFrame();
        observeMemory(MemoryPhase::Capture);
    }
    for (std::size_t i = 0; i < scene.scenario.planets.size(); ++i) {
        std::cout << "Planet " << i << " terrain: " << meshTriangles[i]
                  << " triangles; far/middle/near faces: " << meshZoneFaces[i][0]
                  << "/" << meshZoneFaces[i][1] << "/" << meshZoneFaces[i][2]
                  << "; steep-refined faces: " << meshSteepRefinedFaces[i]
                  << " (budget " << scene.scenario.planets[i].terrain_lod.max_triangle_budget
                  << "); foliage blades: " << grass.count(i)
                  << "; water triangles: " << meshes.waterMeshes[i].indexCount/3 << "\n";
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
        const auto& fieldStats=meshes.planetMeshes[i].terrainStats;
        std::cout << "Planet " << i << " terrain contract: " << fieldStats.uniqueSamples
            << " canonical samples; " << fieldStats.topologyInputBytes << " input bytes; planning queries "
            << fieldStats.planningQueries.requests << "/" << fieldStats.planningQueries.evaluations
            << " requested/evaluated; bulk queries " << fieldStats.evaluationQueries.requests
            << "/" << fieldStats.evaluationQueries.evaluations << " requested/evaluated; field/topology "
            << fieldStats.generation.field << "/" << fieldStats.generation.topology << "; backend "
            << (fieldStats.generation.backend==TerrainBackend::Compute?"compute":"cpu") << '\n';
        if(fieldStats.generation.backend==TerrainBackend::Compute)
            std::cout << "Planet " << i << " compute generation: " << fieldStats.gpuInputBytes
                << " uploaded bytes; " << fieldStats.gpuWorkingBytes << " peak bytes; "
                << fieldStats.gpuDispatches << " dispatches; " << fieldStats.gpuMilliseconds << " GPU ms; CPU compatibility mirror retained\n";
        const auto* layer=&grass.procedural;
        if (layer->usesCompute(i)) {
            const auto counts=mainGrassCounts.at(i);
            std::cout << "Planet " << i
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

    const glm::dvec3 eyeWorld = options.thirdPersonRenderMode ? astronautView.eye : options.surfaceRenderMode ? scene.surfaceCamera->position() :
        options.planetRenderMode ? glm::dvec3(scene.planetOrbitCamera->position) : glm::dvec3(scene.sunCamera.position);
    const glm::mat4 view = options.thirdPersonRenderMode ? glm::mat4(glm::lookAt(astronautView.eye,astronautView.target,astronautView.up)) : options.surfaceRenderMode ? scene.surfaceCamera->getViewMatrix() :
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
        const auto& optics = scene.atmosphereOptics.get(scene.orbitPlanetIndex,
            scene.scenario.planets[scene.orbitPlanetIndex], scene.scenario.metersPerWorldUnit());
        const auto drawnGrass=mainGrassCounts.at(scene.orbitPlanetIndex);
        const auto lastPassGrass=grass.procedural.computedCounts(scene.orbitPlanetIndex);
        nlohmann::json metadata{
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
                {"atmosphere_optics_evaluations", scene.atmosphereOptics.evaluations()},
                {"atmosphere_optics_reuses", scene.atmosphereOptics.reuses()},
                {"atmosphere_refraction_enabled", atmosphereConfig.enabled && atmosphereConfig.refraction_enabled},
                {"atmosphere_downsample", options.atmosphereFullResolution ? 1 : 4},
                {"atmosphere_relative_humidity", optics.relativeHumidity},
                {"atmosphere_liquid_water_g_m3", optics.suspendedLiquidWaterGm3},
                {"white_clipped_pixels", metrics.whiteClippedPixels},
                {"white_clipped_fraction", metrics.whiteClippedFraction},
                {"terrain_pixels", metrics.terrainPixels}, {"sky_pixels", metrics.skyPixels},
                {"terrain_contract", {
                    {"backend", options.terrainBackend},
                    {"field_version", meshes.planetMeshes[scene.orbitPlanetIndex].terrainStats.generation.fieldVersion},
                    {"topology_version", meshes.planetMeshes[scene.orbitPlanetIndex].terrainStats.generation.topologyVersion},
                    {"field_fingerprint", std::to_string(meshes.planetMeshes[scene.orbitPlanetIndex].terrainStats.generation.field)},
                    {"topology_fingerprint", std::to_string(meshes.planetMeshes[scene.orbitPlanetIndex].terrainStats.generation.topology)},
                    {"unique_samples", meshes.planetMeshes[scene.orbitPlanetIndex].terrainStats.uniqueSamples},
                    {"input_bytes", meshes.planetMeshes[scene.orbitPlanetIndex].terrainStats.topologyInputBytes},
                    {"planning_requests", meshes.planetMeshes[scene.orbitPlanetIndex].terrainStats.planningQueries.requests},
                    {"planning_evaluations", meshes.planetMeshes[scene.orbitPlanetIndex].terrainStats.planningQueries.evaluations},
                    {"error_refined_triangles", meshes.planetMeshes[scene.orbitPlanetIndex].terrainStats.errorRefinedTriangles},
                    {"remaining_error_ratio", meshes.planetMeshes[scene.orbitPlanetIndex].terrainStats.remainingErrorRatio},
                    {"error_budget_limited", meshes.planetMeshes[scene.orbitPlanetIndex].terrainStats.errorBudgetLimited},
                    {"evaluation_requests", meshes.planetMeshes[scene.orbitPlanetIndex].terrainStats.evaluationQueries.requests},
                    {"evaluation_evaluations", meshes.planetMeshes[scene.orbitPlanetIndex].terrainStats.evaluationQueries.evaluations}}},
                {"foliage_blades", grass.count(scene.orbitPlanetIndex)},
                {"foliage_candidates", grass.count(scene.orbitPlanetIndex)},
                {"foliage_patches", grass.procedural.stats(scene.orbitPlanetIndex).patches},
                {"foliage_patch_bytes", grass.procedural.stats(scene.orbitPlanetIndex).patchBytes},
                {"foliage_gpu_compute", grass.procedural.usesCompute(scene.orbitPlanetIndex)},
                {"foliage_gpu_drawn_blades", drawnGrass[0]+drawnGrass[1]},
                {"foliage_gpu_count_view", "main"},
                {"foliage_gpu_last_pass_drawn_blades", lastPassGrass[0]+lastPassGrass[1]},
                {"foliage_gpu_vertices", drawnGrass[0]*14+drawnGrass[1]*4},
                {"foliage_gpu_triangles", drawnGrass[0]*12+drawnGrass[1]*2},
                {"foliage_gpu_working_bytes", grass.procedural.stats(scene.orbitPlanetIndex).gpuBytes},
                {"foliage_gpu_metadata", {
                    {"version", 1}, {"allocator", options.terrainGrassPlanner},
                    {"resident_bytes", grass.procedural.stats(scene.orbitPlanetIndex).metadataBytes},
                    {"input_bytes", grass.procedural.stats(scene.orbitPlanetIndex).metadataInputBytes},
                    {"dispatches", grass.procedural.stats(scene.orbitPlanetIndex).metadataDispatches},
                    {"diagnostic_read_bytes", grass.procedural.stats(scene.orbitPlanetIndex).metadataReadBytes},
                    {"allocation_bytes", grass.procedural.stats(scene.orbitPlanetIndex).allocationBytes},
                    {"allocation_input_bytes", grass.procedural.stats(scene.orbitPlanetIndex).allocationInputBytes},
                    {"allocation_dispatches", grass.procedural.stats(scene.orbitPlanetIndex).allocationDispatches},
                    {"summary_read_bytes", grass.procedural.stats(scene.orbitPlanetIndex).summaryReadBytes},
                    {"draw_resource_bytes", grass.procedural.stats(scene.orbitPlanetIndex).drawResourceBytes},
                    {"draw_resources_prepared", grass.procedural.stats(scene.orbitPlanetIndex).drawResourcesPrepared},
                    {"stage_admitted_bytes", grass.procedural.stats(scene.orbitPlanetIndex).stageAdmittedBytes},
                    {"effective_candidate_budget", grass.procedural.stats(scene.orbitPlanetIndex).allocationBudget},
                    {"placement_density_per_m2", grass.procedural.stats(scene.orbitPlanetIndex).density}}},
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
        metadata["render"]["offline"]={{"enabled",options.offlineQuality},
            {"foliage_distance_multiplier",options.foliageDistanceMultiplier}, {"lens_flare",options.lensFlare}};
        metadata["render"]["terrain_backend"]=options.terrainBackend;
        metadata["render"]["terrain_grass_planner"]=options.terrainGrassPlanner;
        if(grass.procedural.adaptiveBudget()) {
            auto policies=nlohmann::json::array();
            for(std::size_t i=0;i<scene.scenario.planets.size();++i) {
                auto p=grass.procedural.policy(i);
                if(!p.is_null()) p["body"]=scene.scenario.planets[i].name;
                policies.push_back(std::move(p));
            }
            metadata["render"]["foliage_policy"]=std::move(policies);
        }
        const auto workerStats=terrainJobs.stats();
        metadata["render"]["terrain_cpu_worker"]={{"submitted",workerStats.submitted},
            {"completed",workerStats.completed},{"failed",workerStats.failed},
            {"coalesced",workerStats.coalesced},{"obsolete",workerStats.obsolete},
            {"peak_running",workerStats.peakRunning},{"peak_queued",workerStats.peakQueued},
            {"pending",terrainJobs.pending(terrainSceneEpoch)},{"rejected_completions",terrainRejectedBuilds}};
        metadata["render"]["terrain_publication"]=terrainPublicationState();
        metadata["render"]["scene_reload"]=sceneReloadState();
        metadata["render"]["terrain_fallback"]=terrainFallback;
        const auto& terrain=meshes.planetMeshes[scene.orbitPlanetIndex].terrainStats;
        metadata["render"]["terrain_compute"]={{"input_bytes",terrain.gpuInputBytes},
            {"generation_peak_bytes",terrain.gpuWorkingBytes},{"dispatches",terrain.gpuDispatches},
            {"stage_admitted_bytes",terrain.gpuStageAdmittedBytes},
            {"gpu_ms",terrain.gpuMilliseconds},{"cpu_compatibility_mirror",bool(terrainCompute) && !meshes.planetMeshes[scene.orbitPlanetIndex].residentTerrain},
            {"cpu_render_bytes",meshes.planetMeshes[scene.orbitPlanetIndex].vertices.size()*4+meshes.planetMeshes[scene.orbitPlanetIndex].indices.size()*4},
            {"cpu_water_render_bytes",meshes.waterMeshes[scene.orbitPlanetIndex].vertices.size()*4+meshes.waterMeshes[scene.orbitPlanetIndex].indices.size()*4}};
        const auto& contacts=meshes.planetMeshes[scene.orbitPlanetIndex].contacts;
        metadata["render"]["terrain_contacts"]={{"backend",contacts ? "sparse-oracle" : "cpu-mesh"}};
        if(contacts) {
            const auto stats=contacts->stats();
            metadata["render"]["terrain_contacts"].update({{"schema_version",1},
                {"field_fingerprint",std::to_string(contacts->generation().field)},
                {"topology_fingerprint",std::to_string(contacts->generation().topology)},
                {"queries",stats.queries},{"nodes_visited",stats.nodesVisited},
                {"candidate_triangles",stats.candidateTriangles},{"height_evaluations",stats.heightEvaluations},
                {"resident_positions",stats.residentPositions},{"position_capacity",SparseTerrainContacts::positionCapacity},
                {"topology_bytes",stats.topologyBytes},{"index_bytes",stats.indexBytes},{"build_ms",stats.buildMilliseconds}});
        }
        metadata["render"]["effective_foliage_distance_m"]=scene.scenario.planets[scene.orbitPlanetIndex].foliage.draw_distance_m;
        metadata["render"]["effective_foliage_budget"]=scene.scenario.planets[scene.orbitPlanetIndex].foliage.max_blades;
        metadata["render"]["sun_mesh_triangles"]=meshes.sunMesh.indices.size()/3;
        metadata["render"]["body_mesh_triangles"]=meshTriangles;
        metadata["render"]["lens_flare_visible_sun_pixels"]=flareEvidence.visibleSunPixels;
        metadata["render"]["lens_flare_strength"]=flareEvidence.strength;
        if (options.thirdPersonRenderMode) {
            const auto visible=std::count(flippedObjects.begin(),flippedObjects.end(),4);
            metadata["render"]["camera_mode"]="third_person";
            metadata["render"]["astronaut_pixels"]=visible;
            metadata["astronaut_pose"]=astronautState();
            std::cout << "Astronaut pixels: " << visible << '\n';
            if (!visible) throw std::runtime_error("Astronaut capture has no visible astronaut");
        }
        std::ofstream sidecar(options.outputImagePath + ".json");
        sidecar << metadata.dump(2) << '\n';
        if (!sidecar) throw std::runtime_error("Failed to write capture metadata");
        if (options.captureOnly && !options.thirdPersonRenderMode && metrics.terrainPixels == 0)
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
    if (!options.captureOnly && !options.surfaceRenderMode && !options.planetRenderMode && !analysis.bodiesVisible()) {
        std::cerr << "Render test FAILED: Sun or planet is not visible\n";
        return 1;
    }
    if (!options.captureOnly && !options.surfaceRenderMode && !options.planetRenderMode && !analysis.bodiesSeparate()) {
        std::cerr << "Render test FAILED: Sun and planet overlap in the image\n";
        return 1;
    }

    std::cout << "Render test completed successfully\n";
    std::cout << "Output image: " << options.outputImagePath << "\n";
    return 0;
}
}

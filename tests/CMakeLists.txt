cmake_minimum_required(VERSION 3.14)
project(ConfigTests LANGUAGES CXX)

include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
    GIT_SHALLOW ON
)

FetchContent_MakeAvailable(googletest)

# Unit and legacy rendering fixtures stay fixed while the working scenario is edited.
set(PLANET_FIXTURE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/development")
set(PLANET_FIXTURE_CONFIG "${PLANET_FIXTURE_ROOT}/configs/scenarios/solar_system.json")

add_executable(renderer_lifecycle_tests app/test_renderer_lifecycle.cpp app/test_command_line.cpp app/test_offline_quality.cpp
    app/terrain/test_publication.cpp app/terrain/reload/test_scene_replacement.cpp
    app/terrain/reload/test_renderer_reload.cpp)
target_link_libraries(renderer_lifecycle_tests PRIVATE planet_app GTest::gtest_main)
target_compile_definitions(renderer_lifecycle_tests PRIVATE
    PLANET_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    PLANET_TEST_OUTPUT="${CMAKE_BINARY_DIR}/renderer-lifecycle")
add_test(NAME RendererLifecycleTests COMMAND renderer_lifecycle_tests)
set_tests_properties(RendererLifecycleTests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 180 RUN_SERIAL TRUE)

add_executable(terrain_recovery_tests app/terrain/recovery/test_recovery.cpp)
target_link_libraries(terrain_recovery_tests PRIVATE planet_app GTest::gtest_main)
target_compile_definitions(terrain_recovery_tests PRIVATE
    PLANET_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    PLANET_TEST_OUTPUT="${CMAKE_BINARY_DIR}/terrain-recovery")
add_test(NAME TerrainRecoveryIntegration COMMAND terrain_recovery_tests)
set_tests_properties(TerrainRecoveryIntegration PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 240 RUN_SERIAL TRUE)

add_executable(terrain_frame_tests app/terrain/interactive/test_frame.cpp)
target_link_libraries(terrain_frame_tests PRIVATE planet_app GTest::gtest_main)
target_compile_definitions(terrain_frame_tests PRIVATE
    PLANET_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    PLANET_TEST_OUTPUT="${CMAKE_BINARY_DIR}/terrain-frame")
add_test(NAME TerrainFrameIntegration COMMAND terrain_frame_tests)
set_tests_properties(TerrainFrameIntegration PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 240 RUN_SERIAL TRUE)

add_executable(config_tests config/test_config.cpp)
target_link_libraries(config_tests PRIVATE GTest::gtest GTest::gtest_main nlohmann_json::nlohmann_json)
target_include_directories(config_tests PRIVATE 
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_definitions(config_tests PRIVATE PLANET_SOURCE_DIR="${PLANET_FIXTURE_ROOT}")

add_executable(orbit_camera_tests camera/test_orbit_camera.cpp)
target_link_libraries(orbit_camera_tests PRIVATE GTest::gtest GTest::gtest_main glm::glm)
target_include_directories(orbit_camera_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(camera_input_tests camera/test_camera_input.cpp)
target_link_libraries(camera_input_tests PRIVATE GTest::gtest GTest::gtest_main glm::glm nlohmann_json::nlohmann_json)
target_include_directories(camera_input_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(camera_transition_tests camera/test_camera_transition.cpp)
target_link_libraries(camera_transition_tests PRIVATE GTest::gtest_main glm::glm)
target_include_directories(camera_transition_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(scene_transforms_tests geometry/test_scene_transforms.cpp)
target_link_libraries(scene_transforms_tests PRIVATE GTest::gtest GTest::gtest_main glm::glm)
target_include_directories(scene_transforms_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(orbit_paths_tests geometry/test_orbit_paths.cpp)
target_link_libraries(orbit_paths_tests PRIVATE GTest::gtest_main glm::glm nlohmann_json::nlohmann_json)
target_include_directories(orbit_paths_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(orbital_system_tests simulation/test_orbital_system.cpp)
target_link_libraries(orbital_system_tests PRIVATE GTest::gtest_main glm::glm nlohmann_json::nlohmann_json)
target_include_directories(orbital_system_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(orbital_system_tests PRIVATE PLANET_SOURCE_DIR="${PLANET_FIXTURE_ROOT}")

add_executable(render_diagnostics_tests diagnostics/test_render_diagnostics.cpp)
target_link_libraries(render_diagnostics_tests PRIVATE GTest::gtest GTest::gtest_main)
target_include_directories(render_diagnostics_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(star_field_tests diagnostics/test_star_field.cpp)
target_link_libraries(star_field_tests PRIVATE GTest::gtest GTest::gtest_main)
target_include_directories(star_field_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(mesh_geometry_tests geometry/test_mesh_geometry.cpp)
target_link_libraries(mesh_geometry_tests PRIVATE planet_rendering GTest::gtest GTest::gtest_main OpenGL::GL GLEW::GLEW glm::glm nlohmann_json::nlohmann_json)
target_include_directories(mesh_geometry_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(terrain_tests geometry/test_terrain.cpp geometry/test_terrain_lod.cpp geometry/terrain/test_contracts.cpp)
target_link_libraries(terrain_tests PRIVATE GTest::gtest GTest::gtest_main glm::glm nlohmann_json::nlohmann_json)
target_include_directories(terrain_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(terrain_tests PRIVATE PLANET_SOURCE_DIR="${PLANET_FIXTURE_ROOT}")

add_executable(planet_local_frame_tests camera/test_planet_local_frame.cpp)
target_link_libraries(planet_local_frame_tests PRIVATE GTest::gtest GTest::gtest_main glm::glm)
target_include_directories(planet_local_frame_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(planet_surface_camera_tests camera/test_planet_surface_camera.cpp)
target_link_libraries(planet_surface_camera_tests PRIVATE GTest::gtest GTest::gtest_main glm::glm nlohmann_json::nlohmann_json)
target_include_directories(planet_surface_camera_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(surface_camera_telemetry_tests camera/test_surface_camera_telemetry.cpp)
target_link_libraries(surface_camera_telemetry_tests PRIVATE GTest::gtest GTest::gtest_main glm::glm nlohmann_json::nlohmann_json)
target_include_directories(surface_camera_telemetry_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

enable_testing()
add_test(NAME ConfigTests COMMAND config_tests)
add_test(NAME OrbitCameraTests COMMAND orbit_camera_tests)
add_test(NAME CameraInputTests COMMAND camera_input_tests)
add_executable(astronaut_motion_tests character/test_astronaut_motion.cpp character/test_surface_contact.cpp character/test_jetpack_physics.cpp character/test_flight_navigation.cpp)
target_sources(astronaut_motion_tests PRIVATE character/contacts/test_sparse_contacts.cpp)
target_link_libraries(astronaut_motion_tests PRIVATE planet_geometry planet_character GTest::gtest_main)
add_test(NAME AstronautMotionTests COMMAND astronaut_motion_tests)
set_tests_properties(AstronautMotionTests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_executable(exhaust_physics_tests character/effects/test_exhaust_physics.cpp)
target_link_libraries(exhaust_physics_tests PRIVATE planet_character GTest::gtest_main)
add_test(NAME ExhaustPhysicsTests COMMAND exhaust_physics_tests)
add_executable(exhaust_render_tests character/effects/test_exhaust_render.cpp)
target_link_libraries(exhaust_render_tests PRIVATE planet_rendering GTest::gtest glfw)
add_test(NAME ExhaustRenderIntegration COMMAND exhaust_render_tests)
set_tests_properties(ExhaustRenderIntegration PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 60)

add_test(NAME CameraTransitionTests COMMAND camera_transition_tests)
add_test(NAME SceneTransformsTests COMMAND scene_transforms_tests)
add_test(NAME OrbitPathsTests COMMAND orbit_paths_tests)
add_test(NAME OrbitalSystemTests COMMAND orbital_system_tests)
add_executable(celestial_lighting_tests lighting/test_celestial_lighting.cpp)
target_link_libraries(celestial_lighting_tests PRIVATE GTest::gtest_main glm::glm nlohmann_json::nlohmann_json)
target_include_directories(celestial_lighting_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(celestial_lighting_tests PRIVATE PLANET_SOURCE_DIR="${PLANET_FIXTURE_ROOT}")
add_test(NAME CelestialLightingTests COMMAND celestial_lighting_tests)
add_executable(camera_exposure_tests lighting/test_camera_exposure.cpp)
target_link_libraries(camera_exposure_tests PRIVATE GTest::gtest_main glm::glm nlohmann_json::nlohmann_json)
target_include_directories(camera_exposure_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME CameraExposureTests COMMAND camera_exposure_tests)
add_executable(scene_replay_tests config/test_scene_replay.cpp)
target_link_libraries(scene_replay_tests PRIVATE GTest::gtest_main glm::glm nlohmann_json::nlohmann_json)
target_include_directories(scene_replay_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME SceneReplayTests COMMAND scene_replay_tests)
add_executable(terrain_shadow_projection_tests lighting/test_terrain_shadow_projection.cpp)
target_link_libraries(terrain_shadow_projection_tests PRIVATE GTest::gtest_main glm::glm nlohmann_json::nlohmann_json)
target_include_directories(terrain_shadow_projection_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME TerrainShadowProjectionTests COMMAND terrain_shadow_projection_tests)

add_executable(terrain_shadow_render_tests lighting/test_terrain_shadow_render.cpp
    foliage/test_grass_wind.cpp foliage/test_grass_render.cpp)
target_link_libraries(terrain_shadow_render_tests PRIVATE planet_rendering GTest::gtest OpenGL::GL GLEW::GLEW glfw glm::glm nlohmann_json::nlohmann_json PNG::PNG)
target_include_directories(terrain_shadow_render_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(terrain_shadow_render_tests PRIVATE PLANET_SHADOW_IMAGE="${CMAKE_BINARY_DIR}/terrain-shadow-test.png")
add_test(NAME TerrainShadowRenderIntegration COMMAND terrain_shadow_render_tests)
set_tests_properties(TerrainShadowRenderIntegration PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME RenderDiagnosticsTests COMMAND render_diagnostics_tests)
add_test(NAME StarFieldTests COMMAND star_field_tests)
add_test(NAME MeshGeometryTests COMMAND mesh_geometry_tests)
add_test(NAME TerrainTests COMMAND terrain_tests)
add_executable(terrain_job_tests geometry/jobs/test_terrain_jobs.cpp)
target_link_libraries(terrain_job_tests PRIVATE planet_geometry GTest::gtest_main)
add_test(NAME TerrainJobTests COMMAND terrain_job_tests)
set_tests_properties(TerrainJobTests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 30)
add_executable(terrain_compute_tests geometry/compute/test_terrain_compute.cpp
    geometry/compute/test_staging.cpp
    geometry/publication/test_publication.cpp
    foliage/planning/test_metadata.cpp foliage/planning/test_allocation.cpp)
target_link_libraries(terrain_compute_tests PRIVATE planet_rendering GTest::gtest glfw)
add_test(NAME TerrainComputeIntegration COMMAND terrain_compute_tests)
set_tests_properties(TerrainComputeIntegration PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 120)

add_executable(grass_placement_tests foliage/test_grass_placement.cpp foliage/test_grass_lod.cpp)
add_executable(foliage_benchmark foliage/benchmark_grass.cpp)
add_executable(grass_plan_tests foliage/test_grass_plan.cpp)
target_link_libraries(grass_plan_tests PRIVATE planet_geometry GTest::gtest_main)
add_test(NAME GrassPlanTests COMMAND grass_plan_tests)
add_executable(grass_trail_tests foliage/trails/test_grass_trail.cpp)
target_link_libraries(grass_trail_tests PRIVATE planet_geometry GTest::gtest_main)
add_test(NAME GrassTrailTests COMMAND grass_trail_tests)
target_link_libraries(foliage_benchmark PRIVATE planet_app)
target_link_libraries(grass_placement_tests PRIVATE GTest::gtest_main glm::glm nlohmann_json::nlohmann_json)
target_include_directories(grass_placement_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME GrassPlacementTests COMMAND grass_placement_tests)
add_test(NAME PlanetLocalFrameTests COMMAND planet_local_frame_tests)
add_test(NAME PlanetSurfaceCameraTests COMMAND planet_surface_camera_tests)
add_test(NAME SurfaceCameraTelemetryTests COMMAND surface_camera_telemetry_tests)
add_test(NAME RenderSceneIntegration COMMAND PlanetSimulation --config ${PLANET_FIXTURE_CONFIG} --render-test ${CMAKE_BINARY_DIR}/render-test.png)
set_tests_properties(RenderSceneIntegration PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME SurfaceRenderIntegration COMMAND PlanetSimulation --config ${PLANET_FIXTURE_CONFIG} --surface-render-test ${CMAKE_BINARY_DIR}/surface-render-test.png)
set_tests_properties(SurfaceRenderIntegration PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME PlanetOrbitRenderIntegration COMMAND PlanetSimulation --config ${PLANET_FIXTURE_CONFIG} --planet-render-test ${CMAKE_BINARY_DIR}/planet-render-test.png)
set_tests_properties(PlanetOrbitRenderIntegration PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME MovingSurfaceRenderIntegration COMMAND PlanetSimulation --config ${PLANET_FIXTURE_CONFIG} --surface-render-test ${CMAKE_BINARY_DIR}/moving-surface-render-test.png --simulation-time 37)
set_tests_properties(MovingSurfaceRenderIntegration PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME MovingPlanetRenderIntegration COMMAND PlanetSimulation --config ${PLANET_FIXTURE_CONFIG} --planet-render-test ${CMAKE_BINARY_DIR}/moving-planet-render-test.png --simulation-time 37)
set_tests_properties(MovingPlanetRenderIntegration PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME MoonlightRenderIntegration COMMAND PlanetSimulation
    --config ${CMAKE_CURRENT_SOURCE_DIR}/configs/moonlight.json
    --surface-render-test ${CMAKE_BINARY_DIR}/moonlight-render-test.png)
set_tests_properties(MoonlightRenderIntegration PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME TerrainComputeCaptureIntegration COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/geometry/compute/test_capture.py --binary $<TARGET_FILE:PlanetSimulation>
    --output-dir ${CMAKE_BINARY_DIR}/terrain-compute-captures)
set_tests_properties(TerrainComputeCaptureIntegration PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 240)
add_test(NAME AstronautCaptureIntegration COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/character/test_astronaut_capture.py --binary $<TARGET_FILE:PlanetSimulation>
    --output-dir ${CMAKE_BINARY_DIR}/astronaut-captures)
add_test(NAME ExhaustCaptureIntegration COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/character/effects/test_exhaust_capture.py --binary $<TARGET_FILE:PlanetSimulation>
    --output-dir ${CMAKE_BINARY_DIR}/exhaust-captures)
set_tests_properties(ExhaustCaptureIntegration PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 240)
add_test(NAME SpaceFlightCaptureIntegration COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/character/test_space_flight_capture.py --binary $<TARGET_FILE:PlanetSimulation>
    --output-dir ${CMAKE_BINARY_DIR}/space-flight-captures)
set_tests_properties(SpaceFlightCaptureIntegration PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 180)
add_test(NAME OfflineCaptureIntegration COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/quality/test_offline_capture.py
    --binary $<TARGET_FILE:PlanetSimulation>
    --output-dir ${CMAKE_BINARY_DIR}/offline-captures)
set_tests_properties(OfflineCaptureIntegration PROPERTIES TIMEOUT 180)
# Standing, gait, flight and grass-trail captures each include exact replay.
# Allow their complete HDR/reflection sequence plus software-GL shader startup.
set_tests_properties(AstronautCaptureIntegration PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 300)
add_executable(cpu_trace_tests diagnostics/tracing/test_cpu_trace.cpp)
target_link_libraries(cpu_trace_tests PRIVATE planet_diagnostics GTest::gtest_main nlohmann_json::nlohmann_json)
target_compile_definitions(cpu_trace_tests PRIVATE PLANET_CPU_TRACE_TEST="${CMAKE_BINARY_DIR}/cpu-trace-test.json")
add_test(NAME CpuTraceTests COMMAND cpu_trace_tests)
add_test(NAME CpuTraceReportTests COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/diagnostics/tracing/test_cpu_report.py)
add_test(NAME GrassCaptureIntegration COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/foliage/test_grass_capture.py --binary $<TARGET_FILE:PlanetSimulation>
    --output-dir ${CMAKE_BINARY_DIR}/grass-captures)
# Several real captures, including compute placement, incur shader JIT
# startup on software GL. Keep a bounded allowance for the complete sequence.
set_tests_properties(GrassCaptureIntegration PROPERTIES TIMEOUT 180)
add_test(NAME LightingScenariosRenderIntegration COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/lighting/render_lighting_scenarios.py --binary $<TARGET_FILE:PlanetSimulation>
    --output-dir ${CMAKE_BINARY_DIR}/lighting-scenarios)
set_tests_properties(LightingScenariosRenderIntegration PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 120)
add_test(NAME TwilightScenariosRenderIntegration COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/lighting/render_lighting_scenarios.py --binary $<TARGET_FILE:PlanetSimulation>
    --manifest ${CMAKE_CURRENT_SOURCE_DIR}/scenarios/twilight/manifest.json
    --output-dir ${CMAKE_BINARY_DIR}/twilight-scenarios)
set_tests_properties(TwilightScenariosRenderIntegration PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 120)
add_custom_target(render_lighting_scenarios
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/lighting/render_lighting_scenarios.py
        --binary $<TARGET_FILE:PlanetSimulation> --output-dir ${CMAKE_BINARY_DIR}/lighting-scenarios
    DEPENDS PlanetSimulation WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} USES_TERMINAL)

add_executable(atmosphere_tests atmosphere/test_atmosphere.cpp)
target_link_libraries(atmosphere_tests PRIVATE GTest::gtest_main glm::glm nlohmann_json::nlohmann_json)
target_include_directories(atmosphere_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME AtmosphereTests COMMAND atmosphere_tests)
add_test(NAME AtmosphereScenariosRenderIntegration COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/lighting/render_lighting_scenarios.py --binary $<TARGET_FILE:PlanetSimulation>
    --manifest ${CMAKE_CURRENT_SOURCE_DIR}/scenarios/atmosphere/manifest.json
    --output-dir ${CMAKE_BINARY_DIR}/atmosphere-scenarios)
# Nine scenes each render and replay HDR atmosphere frames. Software GL can
# exceed three minutes; retain a bounded five-minute allowance for the suite.
set_tests_properties(AtmosphereScenariosRenderIntegration PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 300)
add_custom_target(render_atmosphere_scenarios
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/lighting/render_lighting_scenarios.py
        --binary $<TARGET_FILE:PlanetSimulation>
        --manifest ${CMAKE_CURRENT_SOURCE_DIR}/scenarios/atmosphere/manifest.json
        --output-dir ${CMAKE_BINARY_DIR}/atmosphere-scenarios
    DEPENDS PlanetSimulation WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} USES_TERMINAL)

add_executable(atmosphere_render_tests atmosphere/test_atmosphere_render.cpp)
target_link_libraries(atmosphere_render_tests PRIVATE planet_rendering GTest::gtest OpenGL::GL GLEW::GLEW glfw glm::glm nlohmann_json::nlohmann_json)
target_include_directories(atmosphere_render_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(atmosphere_render_tests PRIVATE PLANET_PERFORMANCE_TRACE="${CMAKE_BINARY_DIR}/performance-test.csv")
add_test(NAME AtmosphereRefractionRenderIntegration COMMAND atmosphere_render_tests)
set_tests_properties(AtmosphereRefractionRenderIntegration PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(frame_reuse_tests diagnostics/test_frame_reuse.cpp)
target_link_libraries(frame_reuse_tests PRIVATE GTest::gtest_main OpenGL::GL GLEW::GLEW glm::glm)
target_include_directories(frame_reuse_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME FrameReuseTests COMMAND frame_reuse_tests)
add_test(NAME PerformanceCaptureIntegration COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/diagnostics/test_performance_capture.py --binary $<TARGET_FILE:PlanetSimulation>
    --output-dir ${CMAKE_BINARY_DIR}/performance-capture-tests)
set_tests_properties(PerformanceCaptureIntegration PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 60)

add_executable(gpu_utilization_tests diagnostics/test_gpu_utilization.cpp)
target_include_directories(gpu_utilization_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(gpu_utilization_tests PRIVATE GTest::gtest_main ${CMAKE_DL_LIBS})
add_test(NAME GpuUtilizationTests COMMAND gpu_utilization_tests)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_library(fake_nvml SHARED diagnostics/fake_nvml.cpp)
    set_target_properties(fake_nvml PROPERTIES OUTPUT_NAME nvidia-ml SOVERSION 1
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/fake-nvml)
    add_dependencies(gpu_utilization_tests fake_nvml)
    set_tests_properties(GpuUtilizationTests PROPERTIES
        ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_CURRENT_BINARY_DIR}/fake-nvml")
endif()

find_program(XDOTOOL_EXECUTABLE xdotool)
find_program(IMAGEMAGICK_IMPORT_EXECUTABLE import)
find_program(IMAGEMAGICK_CONVERT_EXECUTABLE convert)
if(UNIX AND XDOTOOL_EXECUTABLE AND IMAGEMAGICK_IMPORT_EXECUTABLE AND IMAGEMAGICK_CONVERT_EXECUTABLE)
    add_test(NAME GrassWindInputIntegration COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/foliage/integration/test_grass_input.py --binary $<TARGET_FILE:PlanetSimulation>
        --output-dir ${CMAKE_BINARY_DIR}/grass-wind-input)
    set_tests_properties(GrassWindInputIntegration PROPERTIES TIMEOUT 90 RUN_SERIAL TRUE)
    add_test(NAME AstronautInputIntegration COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/character/test_astronaut_input.py --binary $<TARGET_FILE:PlanetSimulation>)
    set_tests_properties(AstronautInputIntegration PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
    add_test(NAME PerformanceOverlayInputIntegration COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/diagnostics/test_overlay_input.py --binary $<TARGET_FILE:PlanetSimulation>)
    set_tests_properties(PerformanceOverlayInputIntegration PROPERTIES TIMEOUT 120)
    add_test(NAME OrbitOverlayInputIntegration COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/geometry/test_orbit_input.py --binary $<TARGET_FILE:PlanetSimulation>)
    set_tests_properties(OrbitOverlayInputIntegration PROPERTIES TIMEOUT 120)
    add_test(NAME CameraTransitionInputIntegration COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/camera/test_camera_transition_input.py --binary $<TARGET_FILE:PlanetSimulation>)
    set_tests_properties(CameraTransitionInputIntegration PROPERTIES TIMEOUT 120)
    add_test(NAME CameraClippingInputIntegration COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/camera/test_camera_clipping_input.py
        --binary $<TARGET_FILE:PlanetSimulation>
        --output-dir ${CMAKE_BINARY_DIR}/camera-clipping-test)
    set_tests_properties(CameraClippingInputIntegration PROPERTIES TIMEOUT 90)
endif()

add_test(NAME RepositoryLayout COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/check_layout.py)

add_executable(adaptive_quality_tests diagnostics/test_adaptive_quality.cpp)
target_include_directories(adaptive_quality_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(adaptive_quality_tests PRIVATE GTest::gtest_main)
add_test(NAME AdaptiveQualityTests COMMAND adaptive_quality_tests)
if(UNIX AND XDOTOOL_EXECUTABLE AND IMAGEMAGICK_IMPORT_EXECUTABLE AND IMAGEMAGICK_CONVERT_EXECUTABLE)
    add_test(NAME AdaptiveQualityRenderIntegration COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/diagnostics/test_adaptive_render.py --binary $<TARGET_FILE:PlanetSimulation>)
    set_tests_properties(AdaptiveQualityRenderIntegration PROPERTIES TIMEOUT 60)
endif()

# These tests send real keyboard events to the shared X11 display. Keep
# their windows isolated even when CTest is invoked with --parallel.
if(UNIX AND XDOTOOL_EXECUTABLE AND IMAGEMAGICK_IMPORT_EXECUTABLE AND IMAGEMAGICK_CONVERT_EXECUTABLE)
    set_tests_properties(PerformanceOverlayInputIntegration OrbitOverlayInputIntegration
        CameraTransitionInputIntegration CameraClippingInputIntegration AdaptiveQualityRenderIntegration
        PROPERTIES RUN_SERIAL TRUE)
endif()

# Link only the implementation each CPU test consumes. GPU tests link
# planet_rendering above, which already carries geometry/config dependencies.
foreach(target config_tests orbit_paths_tests celestial_lighting_tests
        camera_exposure_tests scene_replay_tests terrain_shadow_projection_tests
        atmosphere_tests)
    target_link_libraries(${target} PRIVATE planet_config)
endforeach()
foreach(target camera_input_tests orbital_system_tests terrain_tests
        planet_surface_camera_tests surface_camera_telemetry_tests grass_placement_tests)
    target_link_libraries(${target} PRIVATE planet_geometry)
endforeach()

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

add_executable(config_tests test_config.cpp)
target_link_libraries(config_tests PRIVATE GTest::gtest GTest::gtest_main nlohmann_json::nlohmann_json)
target_include_directories(config_tests PRIVATE 
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_definitions(config_tests PRIVATE PLANET_SOURCE_DIR="${PLANET_FIXTURE_ROOT}")

add_executable(orbit_camera_tests test_orbit_camera.cpp)
target_link_libraries(orbit_camera_tests PRIVATE GTest::gtest GTest::gtest_main glm::glm)
target_include_directories(orbit_camera_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(camera_input_tests test_camera_input.cpp)
target_link_libraries(camera_input_tests PRIVATE GTest::gtest GTest::gtest_main glm::glm nlohmann_json::nlohmann_json)
target_include_directories(camera_input_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(scene_transforms_tests test_scene_transforms.cpp)
target_link_libraries(scene_transforms_tests PRIVATE GTest::gtest GTest::gtest_main glm::glm)
target_include_directories(scene_transforms_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(orbital_system_tests test_orbital_system.cpp)
target_link_libraries(orbital_system_tests PRIVATE GTest::gtest_main glm::glm nlohmann_json::nlohmann_json)
target_include_directories(orbital_system_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(orbital_system_tests PRIVATE PLANET_SOURCE_DIR="${PLANET_FIXTURE_ROOT}")

add_executable(render_diagnostics_tests test_render_diagnostics.cpp)
target_link_libraries(render_diagnostics_tests PRIVATE GTest::gtest GTest::gtest_main)
target_include_directories(render_diagnostics_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(star_field_tests test_star_field.cpp)
target_link_libraries(star_field_tests PRIVATE GTest::gtest GTest::gtest_main)
target_include_directories(star_field_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(mesh_geometry_tests test_mesh_geometry.cpp)
target_link_libraries(mesh_geometry_tests PRIVATE GTest::gtest GTest::gtest_main OpenGL::GL GLEW::GLEW glm::glm nlohmann_json::nlohmann_json)
target_include_directories(mesh_geometry_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(terrain_tests test_terrain.cpp)
target_link_libraries(terrain_tests PRIVATE GTest::gtest GTest::gtest_main glm::glm nlohmann_json::nlohmann_json)
target_include_directories(terrain_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(terrain_tests PRIVATE PLANET_SOURCE_DIR="${PLANET_FIXTURE_ROOT}")

add_executable(planet_local_frame_tests test_planet_local_frame.cpp)
target_link_libraries(planet_local_frame_tests PRIVATE GTest::gtest GTest::gtest_main glm::glm)
target_include_directories(planet_local_frame_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(planet_surface_camera_tests test_planet_surface_camera.cpp)
target_link_libraries(planet_surface_camera_tests PRIVATE GTest::gtest GTest::gtest_main glm::glm nlohmann_json::nlohmann_json)
target_include_directories(planet_surface_camera_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(surface_camera_telemetry_tests test_surface_camera_telemetry.cpp)
target_link_libraries(surface_camera_telemetry_tests PRIVATE GTest::gtest GTest::gtest_main glm::glm nlohmann_json::nlohmann_json)
target_include_directories(surface_camera_telemetry_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

enable_testing()
add_test(NAME ConfigTests COMMAND config_tests)
add_test(NAME OrbitCameraTests COMMAND orbit_camera_tests)
add_test(NAME CameraInputTests COMMAND camera_input_tests)
add_test(NAME SceneTransformsTests COMMAND scene_transforms_tests)
add_test(NAME OrbitalSystemTests COMMAND orbital_system_tests)
add_executable(celestial_lighting_tests test_celestial_lighting.cpp)
target_link_libraries(celestial_lighting_tests PRIVATE GTest::gtest_main glm::glm nlohmann_json::nlohmann_json)
target_include_directories(celestial_lighting_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(celestial_lighting_tests PRIVATE PLANET_SOURCE_DIR="${PLANET_FIXTURE_ROOT}")
add_test(NAME CelestialLightingTests COMMAND celestial_lighting_tests)
add_executable(camera_exposure_tests test_camera_exposure.cpp)
target_link_libraries(camera_exposure_tests PRIVATE GTest::gtest_main glm::glm nlohmann_json::nlohmann_json)
target_include_directories(camera_exposure_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME CameraExposureTests COMMAND camera_exposure_tests)
add_executable(scene_replay_tests test_scene_replay.cpp)
target_link_libraries(scene_replay_tests PRIVATE GTest::gtest_main glm::glm nlohmann_json::nlohmann_json)
target_include_directories(scene_replay_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME SceneReplayTests COMMAND scene_replay_tests)
add_executable(terrain_shadow_projection_tests test_terrain_shadow_projection.cpp)
target_link_libraries(terrain_shadow_projection_tests PRIVATE GTest::gtest_main glm::glm nlohmann_json::nlohmann_json)
target_include_directories(terrain_shadow_projection_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME TerrainShadowProjectionTests COMMAND terrain_shadow_projection_tests)

add_executable(terrain_shadow_render_tests test_terrain_shadow_render.cpp)
target_link_libraries(terrain_shadow_render_tests PRIVATE GTest::gtest OpenGL::GL GLEW::GLEW glfw glm::glm nlohmann_json::nlohmann_json)
target_include_directories(terrain_shadow_render_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(terrain_shadow_render_tests PRIVATE PLANET_SHADOW_IMAGE="${CMAKE_BINARY_DIR}/terrain-shadow-test.png")
add_test(NAME TerrainShadowRenderIntegration COMMAND terrain_shadow_render_tests)
set_tests_properties(TerrainShadowRenderIntegration PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME RenderDiagnosticsTests COMMAND render_diagnostics_tests)
add_test(NAME StarFieldTests COMMAND star_field_tests)
add_test(NAME MeshGeometryTests COMMAND mesh_geometry_tests)
add_test(NAME TerrainTests COMMAND terrain_tests)
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
add_test(NAME LightingScenariosRenderIntegration COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/render_lighting_scenarios.py --binary $<TARGET_FILE:PlanetSimulation>
    --output-dir ${CMAKE_BINARY_DIR}/lighting-scenarios)
set_tests_properties(LightingScenariosRenderIntegration PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 120)
add_test(NAME TwilightScenariosRenderIntegration COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/render_lighting_scenarios.py --binary $<TARGET_FILE:PlanetSimulation>
    --manifest ${CMAKE_CURRENT_SOURCE_DIR}/scenarios/twilight/manifest.json
    --output-dir ${CMAKE_BINARY_DIR}/twilight-scenarios)
set_tests_properties(TwilightScenariosRenderIntegration PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 120)
add_custom_target(render_lighting_scenarios
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/render_lighting_scenarios.py
        --binary $<TARGET_FILE:PlanetSimulation> --output-dir ${CMAKE_BINARY_DIR}/lighting-scenarios
    DEPENDS PlanetSimulation WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} USES_TERMINAL)

add_executable(atmosphere_tests test_atmosphere.cpp)
target_link_libraries(atmosphere_tests PRIVATE GTest::gtest_main glm::glm nlohmann_json::nlohmann_json)
target_include_directories(atmosphere_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME AtmosphereTests COMMAND atmosphere_tests)
add_test(NAME AtmosphereScenariosRenderIntegration COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/render_lighting_scenarios.py --binary $<TARGET_FILE:PlanetSimulation>
    --manifest ${CMAKE_CURRENT_SOURCE_DIR}/scenarios/atmosphere/manifest.json
    --output-dir ${CMAKE_BINARY_DIR}/atmosphere-scenarios)
set_tests_properties(AtmosphereScenariosRenderIntegration PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 180)
add_custom_target(render_atmosphere_scenarios
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/render_lighting_scenarios.py
        --binary $<TARGET_FILE:PlanetSimulation>
        --manifest ${CMAKE_CURRENT_SOURCE_DIR}/scenarios/atmosphere/manifest.json
        --output-dir ${CMAKE_BINARY_DIR}/atmosphere-scenarios
    DEPENDS PlanetSimulation WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} USES_TERMINAL)

add_executable(atmosphere_render_tests test_atmosphere_render.cpp)
target_link_libraries(atmosphere_render_tests PRIVATE GTest::gtest OpenGL::GL GLEW::GLEW glfw glm::glm nlohmann_json::nlohmann_json)
target_include_directories(atmosphere_render_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(atmosphere_render_tests PRIVATE PLANET_PERFORMANCE_TRACE="${CMAKE_BINARY_DIR}/performance-test.csv")
add_test(NAME AtmosphereRefractionRenderIntegration COMMAND atmosphere_render_tests)
set_tests_properties(AtmosphereRefractionRenderIntegration PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(frame_reuse_tests test_frame_reuse.cpp)
target_link_libraries(frame_reuse_tests PRIVATE GTest::gtest_main OpenGL::GL GLEW::GLEW glm::glm)
target_include_directories(frame_reuse_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME FrameReuseTests COMMAND frame_reuse_tests)
add_test(NAME PerformanceCaptureIntegration COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/test_performance_capture.py --binary $<TARGET_FILE:PlanetSimulation>
    --output-dir ${CMAKE_BINARY_DIR}/performance-capture-tests)
set_tests_properties(PerformanceCaptureIntegration PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 60)

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

add_executable(config_tests test_config.cpp)
target_link_libraries(config_tests PRIVATE GTest::gtest GTest::gtest_main nlohmann_json::nlohmann_json)
target_include_directories(config_tests PRIVATE 
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_definitions(config_tests PRIVATE PLANET_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

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
target_compile_definitions(orbital_system_tests PRIVATE PLANET_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

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
target_compile_definitions(terrain_tests PRIVATE PLANET_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

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
target_compile_definitions(celestial_lighting_tests PRIVATE PLANET_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_test(NAME CelestialLightingTests COMMAND celestial_lighting_tests)
add_test(NAME RenderDiagnosticsTests COMMAND render_diagnostics_tests)
add_test(NAME StarFieldTests COMMAND star_field_tests)
add_test(NAME MeshGeometryTests COMMAND mesh_geometry_tests)
add_test(NAME TerrainTests COMMAND terrain_tests)
add_test(NAME PlanetLocalFrameTests COMMAND planet_local_frame_tests)
add_test(NAME PlanetSurfaceCameraTests COMMAND planet_surface_camera_tests)
add_test(NAME SurfaceCameraTelemetryTests COMMAND surface_camera_telemetry_tests)
add_test(NAME RenderSceneIntegration COMMAND PlanetSimulation --render-test ${CMAKE_BINARY_DIR}/render-test.png)
set_tests_properties(RenderSceneIntegration PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME SurfaceRenderIntegration COMMAND PlanetSimulation --surface-render-test ${CMAKE_BINARY_DIR}/surface-render-test.png)
set_tests_properties(SurfaceRenderIntegration PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME PlanetOrbitRenderIntegration COMMAND PlanetSimulation --planet-render-test ${CMAKE_BINARY_DIR}/planet-render-test.png)
set_tests_properties(PlanetOrbitRenderIntegration PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME MovingSurfaceRenderIntegration COMMAND PlanetSimulation --surface-render-test ${CMAKE_BINARY_DIR}/moving-surface-render-test.png --simulation-time 37)
set_tests_properties(MovingSurfaceRenderIntegration PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME MovingPlanetRenderIntegration COMMAND PlanetSimulation --planet-render-test ${CMAKE_BINARY_DIR}/moving-planet-render-test.png --simulation-time 37)
set_tests_properties(MovingPlanetRenderIntegration PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME MoonlightRenderIntegration COMMAND PlanetSimulation
    --config ${CMAKE_CURRENT_SOURCE_DIR}/configs/moonlight.json
    --surface-render-test ${CMAKE_BINARY_DIR}/moonlight-render-test.png)
set_tests_properties(MoonlightRenderIntegration PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

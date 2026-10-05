cmake_minimum_required(VERSION 3.24)
project(PlanetSimulation VERSION 0.0.1 LANGUAGES CXX)

# Interactive terrain/foliage preparation needs an optimized default build.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find OpenGL before using its imported targets
find_package(OpenGL REQUIRED)
find_package(PNG REQUIRED)
find_package(Threads REQUIRED)

# Fetch nlohmann_json from git repository (header-only library)
include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW ON
)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    GIT_SHALLOW ON
)
FetchContent_MakeAvailable(glm)

# Only the MIT-licensed animation runtime is needed for astronaut leg IK.
# Pin the release commit; do not build asset importers, samples or library tests.
set(ozz_build_tools OFF CACHE BOOL "" FORCE)
set(ozz_build_samples OFF CACHE BOOL "" FORCE)
set(ozz_build_howtos OFF CACHE BOOL "" FORCE)
set(ozz_build_tests OFF CACHE BOOL "" FORCE)
FetchContent_Declare(ozz_animation
    GIT_REPOSITORY https://github.com/guillaumeblanc/ozz-animation.git
    GIT_TAG ca6328f51d857fa516edf915f3cde92eb6c90ad2)
FetchContent_MakeAvailable(ozz_animation)
# Upstream also defines offline libraries. Build only targets pulled in by IK.
set_property(DIRECTORY "${ozz_animation_SOURCE_DIR}" PROPERTY EXCLUDE_FROM_ALL TRUE)

# Find GLFW and GLEW
find_package(glfw3 REQUIRED)
find_package(GLEW REQUIRED)

# Compile substantial implementations once and share them with the tests.
# Public headers expose value types; JSON parsing stays in planet_config.
add_library(planet_config STATIC
    src/config/parsing/Config.cpp
    src/config/parsing/ScenarioConfig.cpp
    src/config/parsing/AtmosphereConfig.cpp
    src/config/parsing/LightingConfig.cpp
    src/config/parsing/OrbitalConfig.cpp
    src/config/parsing/FoliageConfig.cpp)
target_include_directories(planet_config PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(planet_config PUBLIC nlohmann_json::nlohmann_json)

add_library(planet_diagnostics STATIC src/rendering/diagnostics/tracing/CpuTrace.cpp
    src/rendering/diagnostics/timing/PublicationProfiler.cpp)
target_include_directories(planet_diagnostics PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(planet_diagnostics PRIVATE nlohmann_json::nlohmann_json)

add_library(planet_geometry STATIC
    src/rendering/geometry/Terrain.cpp
    src/rendering/geometry/terrain/PlanetField.cpp
    src/rendering/geometry/terrain/TerrainQueryCache.cpp
    src/rendering/geometry/terrain/TerrainTopology.cpp
    src/rendering/geometry/contacts/SparseTerrainContacts.cpp
    src/rendering/geometry/jobs/TerrainBuild.cpp
    src/rendering/geometry/jobs/TerrainBuildScheduler.cpp
    src/rendering/geometry/TerrainLod.cpp
    src/rendering/geometry/TerrainShoreline.cpp
    src/rendering/foliage/GrassPlacement.cpp
    src/rendering/foliage/GrassLod.cpp)
target_sources(planet_geometry PRIVATE src/rendering/foliage/procedural/GrassPlan.cpp
    src/rendering/foliage/trails/GrassTrail.cpp)
target_link_libraries(planet_geometry PUBLIC planet_config planet_diagnostics glm::glm Threads::Threads)

add_library(planet_character STATIC
    src/rendering/character/AstronautMotion.cpp
    src/rendering/character/SurfaceContact.cpp
    src/rendering/character/flight/JetpackPhysics.cpp
    src/rendering/character/flight/FlightNavigation.cpp
    src/rendering/character/effects/ExhaustParticles.cpp
    src/rendering/foliage/wind/WindField.cpp)
target_include_directories(planet_character PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(planet_character PUBLIC glm::glm planet_geometry PRIVATE ozz_animation)

add_library(planet_rendering STATIC
    src/rendering/Shader.cpp
    src/rendering/geometry/Mesh.cpp
    src/rendering/foliage/GrassRenderer.cpp)
target_sources(planet_rendering PRIVATE src/rendering/foliage/procedural/ProceduralGrass.cpp)
target_sources(planet_rendering PRIVATE src/rendering/foliage/procedural/GrassPreparation.cpp)
target_sources(planet_rendering PRIVATE src/rendering/foliage/procedural/GrassCompute.cpp
    src/rendering/foliage/trails/TrailUpload.cpp)
target_sources(planet_rendering PRIVATE src/rendering/character/AstronautRenderer.cpp
    src/rendering/character/effects/ExhaustRenderer.cpp)
target_sources(planet_rendering PRIVATE src/rendering/postprocessing/LensFlare.cpp)
target_sources(planet_rendering PRIVATE src/rendering/geometry/compute/TerrainCompute.cpp
    src/rendering/geometry/compute/TerrainGpuPreparation.cpp
    src/rendering/geometry/publication/TerrainPublication.cpp
    src/rendering/foliage/planning/GrassMetadata.cpp
    src/rendering/foliage/planning/GrassAllocation.cpp)
target_link_libraries(planet_rendering PUBLIC planet_geometry planet_character OpenGL::GL GLEW::GLEW)

add_library(planet_app STATIC
    src/app/CommandLineOptions.cpp
    src/app/Window.cpp
    src/app/WindowInput.cpp
    src/app/scene/SceneSource.cpp
    src/app/scene/PreparedScene.cpp
    src/rendering/runtime/Renderer.cpp
    src/rendering/runtime/TerrainMeshes.cpp
    src/rendering/runtime/terrain/ResidentTerrain.cpp
    src/rendering/runtime/terrain/InteractiveTerrain.cpp
    src/rendering/runtime/terrain/CharacterPreview.cpp
    src/rendering/runtime/terrain/reload/SceneTerrainReplacement.cpp
    src/rendering/runtime/terrain/reload/ReloadScene.cpp
    src/rendering/runtime/terrain/reload/ReloadCommit.cpp
    src/rendering/runtime/terrain/reload/interactive/InteractiveReload.cpp
    src/rendering/runtime/terrain/reload/ReloadPreview.cpp
    src/rendering/runtime/ScenePass.cpp
    src/rendering/runtime/Capture.cpp
    src/rendering/runtime/Character.cpp
    src/rendering/runtime/Interactive.cpp)
target_compile_definitions(planet_app PRIVATE PLANET_VERSION="${PROJECT_VERSION}")

# Link required libraries
target_link_libraries(planet_app PUBLIC
    planet_rendering
    nlohmann_json::nlohmann_json
    OpenGL::GL
    PNG::PNG
    ${CMAKE_DL_LIBS}
    glfw
    GLEW::GLEW
    glm::glm
)

add_executable(PlanetSimulation src/main.cpp)
target_link_libraries(PlanetSimulation PRIVATE planet_app)

# Include directories for all source files
target_include_directories(PlanetSimulation PRIVATE 
    ${CMAKE_SOURCE_DIR}/src
)

# Copy shaders to build directory (for shader loading)
file(COPY shaders DESTINATION ${CMAKE_BINARY_DIR})

# Tests (optional)
option(BUILD_TESTING "Build tests" OFF)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

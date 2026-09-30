cmake_minimum_required(VERSION 3.16)
project(PlanetSimulation VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find OpenGL before using its imported targets
find_package(OpenGL REQUIRED)
find_package(PNG REQUIRED)

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

add_library(planet_geometry STATIC
    src/rendering/geometry/Terrain.cpp
    src/rendering/geometry/TerrainShoreline.cpp
    src/rendering/foliage/GrassPlacement.cpp
    src/rendering/foliage/GrassLod.cpp)
target_link_libraries(planet_geometry PUBLIC planet_config glm::glm)

add_library(planet_rendering STATIC
    src/rendering/Shader.cpp
    src/rendering/geometry/Mesh.cpp
    src/rendering/foliage/GrassRenderer.cpp)
target_link_libraries(planet_rendering PUBLIC planet_geometry OpenGL::GL GLEW::GLEW)

add_library(planet_app STATIC
    src/app/CommandLineOptions.cpp
    src/app/Window.cpp
    src/app/WindowInput.cpp
    src/app/scene/SceneSource.cpp
    src/app/scene/PreparedScene.cpp
    src/rendering/runtime/Renderer.cpp
    src/rendering/runtime/TerrainMeshes.cpp
    src/rendering/runtime/ScenePass.cpp
    src/rendering/runtime/Capture.cpp
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

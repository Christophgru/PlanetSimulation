cmake_minimum_required(VERSION 3.16)
project(PlanetSimulation VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find OpenGL before using its imported targets
find_package(OpenGL REQUIRED)

# Fetch nlohmann_json from git repository (header-only library)
include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW ON
)
FetchContent_MakeAvailable(nlohmann_json)

# Find GLFW and GLEW
find_package(glfw3 REQUIRED)
find_package(GLEW REQUIRED)

# Main executable - headers are included directly from src/ directories
add_executable(PlanetSimulation src/main.cpp)

# Link required libraries
target_link_libraries(PlanetSimulation PRIVATE 
    nlohmann_json::nlohmann_json
    OpenGL::GL
    glfw
    GLEW::GLEW
)

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

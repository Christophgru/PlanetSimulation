cmake_minimum_required(VERSION 3.16)
project(PlanetSimulation VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)
find_package(nlohmann_json REQUIRED)

add_executable(${PROJECT_NAME}
    src/main.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(${PROJECT_NAME} PRIVATE OpenGL::GL GLEW::GLEW glfw nlohmann_json::nlohmann_json)

# Enable testing
option(BUILD_TESTING "Build tests" OFF)

if(BUILD_TESTING)
    enable_testing()
    
    # Find GoogleTest
    find_package(GTest REQUIRED)
    
    add_executable(${PROJECT_NAME}_tests
        tests/test_main.cpp
    )
    
    target_include_directories(${PROJECT_NAME}_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(${PROJECT_NAME}_tests PRIVATE GTest::GTest GTest::Main OpenGL::GL GLEW::GLEW glfw nlohmann_json::nlohmann_json)
    
    add_test(NAME ${PROJECT_NAME}Tests COMMAND ${PROJECT_NAME}_tests)
endif()

# Install config files
install(DIRECTORY configs/scenarios DESTINATION ${CMAKE_INSTALL_PREFIX}/configs/scenarios)

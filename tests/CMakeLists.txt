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
find_package(nlohmann_json REQUIRED)

add_executable(config_tests test_config.cpp)
target_link_libraries(config_tests PRIVATE GTest::gtest GTest::gtest_main nlohmann_json::nlohmann_json)
target_include_directories(config_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/config)

enable_testing()
add_test(NAME ConfigTests COMMAND config_tests)

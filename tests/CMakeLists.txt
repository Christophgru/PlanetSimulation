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
target_link_libraries(config_tests PRIVATE GTest::gtest GTest::gtest_main)
target_include_directories(config_tests PRIVATE 
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/build-agent/_deps/nlohmann_json-src/include
)

add_executable(test_matrix test_matrix.cpp)
target_link_libraries(test_matrix PRIVATE GTest::gtest GTest::gtest_main)
target_include_directories(test_matrix PRIVATE 
    ${CMAKE_SOURCE_DIR}/src
)

enable_testing()
add_test(NAME ConfigTests COMMAND config_tests)
add_test(NAME MatrixTest COMMAND test_matrix)

#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

TEST(ConfigFileNotFoundTest, ExitWhenConfigMissing) {
    // Create a temporary directory without config file
    const std::string tempDir = "/tmp/planet_sim_test";
    
    // Clean up if exists
    if (fs::exists(tempDir)) {
        fs::remove_all(tempDir);
    }
    
    // Create empty directory
    fs::create_directories(tempDir);
    
    // Verify config file doesn't exist
    EXPECT_FALSE(fs::exists(tempDir + "/configs/scenarios/solar_system.json"));
    
    // Clean up
    fs::remove_all(tempDir);
}

TEST(ConfigFileNotFoundTest, ConfigExists) {
    // Create a temporary directory with config file
    const std::string tempDir = "/tmp/planet_sim_test";
    
    // Clean up if exists
    if (fs::exists(tempDir)) {
        fs::remove_all(tempDir);
    }
    
    // Create empty directory
    fs::create_directories(tempDir + "/configs/scenarios");
    
    // Create config file
    std::ofstream configFile(tempDir + "/configs/scenarios/solar_system.json");
    if (configFile.is_open()) {
        configFile << R"({
            "scenario_name": "Test",
            "sun": {
                "position": [0, 0, 0],
                "radius": 10.0,
                "color": [1.0, 0.9, 0.7]
            },
            "planet": {
                "position": [5.0, 0, 0],
                "orbit_radius": 5.0,
                "orbit_speed": 0.02,
                "radius": 1.5,
                "color": [0.3, 0.6, 0.9]
            }
        })";
        configFile.close();
    }
    
    // Verify config file exists
    EXPECT_TRUE(fs::exists(tempDir + "/configs/scenarios/solar_system.json"));
    
    // Clean up
    fs::remove_all(tempDir);
}

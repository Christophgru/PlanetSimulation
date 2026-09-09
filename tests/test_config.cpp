#include <gtest/gtest.h>
#include "config/Config.h"
#include "config/ScenarioConfig.h"
#include <fstream>
#include <sstream>
#include <filesystem>

TEST(ConfigTest, GetDouble) {
    nlohmann::json json = R"({
        "value": 42.5,
        "pi": 3.14159
    })"_json;
    
    config::Config cfg{std::move(json)};
    
    EXPECT_DOUBLE_EQ(cfg.getDouble("value"), 42.5);
    EXPECT_DOUBLE_EQ(cfg.getDouble("pi", 0.0), 3.14159);
    EXPECT_DOUBLE_EQ(cfg.getDouble("missing", 10.0), 10.0);
}

TEST(ConfigTest, GetInt) {
    nlohmann::json json = R"({
        "count": 100,
        "enabled": true
    })"_json;
    
    config::Config cfg{std::move(json)};
    
    EXPECT_EQ(cfg.getInt("count"), 100);
    EXPECT_EQ(cfg.getInt("missing", -1), -1);
}

TEST(ConfigTest, GetBool) {
    nlohmann::json json = R"({
        "enabled": true,
        "debug": false
    })"_json;
    
    config::Config cfg{std::move(json)};
    
    EXPECT_TRUE(cfg.getBool("enabled"));
    EXPECT_FALSE(cfg.getBool("debug"));
    EXPECT_FALSE(cfg.getBool("missing", true));
}

TEST(ConfigTest, GetArray) {
    nlohmann::json json = R"({
        "positions": [1.0, 2.0, 3.0],
        "colors": [0.5, 0.6, 0.7]
    })"_json;
    
    config::Config cfg{std::move(json)};
    
    auto positions = cfg.getArray("positions", std::vector<double>{});
    EXPECT_EQ(positions.size(), 3u);
    EXPECT_DOUBLE_EQ(positions[0], 1.0);
    EXPECT_DOUBLE_EQ(positions[1], 2.0);
    EXPECT_DOUBLE_EQ(positions[2], 3.0);
    
    auto empty = cfg.getArray("missing", std::vector<double>{1.0, 2.0});
    EXPECT_EQ(empty.size(), 2u);
}

TEST(ConfigTest, GetString) {
    nlohmann::json json = R"({
        "name": "Solar System",
        "version": "1.0"
    })"_json;
    
    config::Config cfg{std::move(json)};
    
    EXPECT_EQ(cfg.get("name"), "Solar System");
    EXPECT_EQ(cfg.get("missing"), "");
}

TEST(ScenarioConfigTest, ParseSun) {
    nlohmann::json json = R"({
        "scenario_name": "Test",
        "sun": {
            "position": [0, 0, 0],
            "radius": 10.0,
            "color": [1.0, 0.9, 0.7]
        }
    })"_json;
    
    config::Config cfg{std::move(json)};
    config::ScenarioConfig scenario(cfg);
    
    EXPECT_EQ(scenario.name, "Test");
    EXPECT_DOUBLE_EQ(scenario.sun.radius, 10.0);
    EXPECT_DOUBLE_EQ(scenario.sun.color[0], 1.0);
}

TEST(ScenarioConfigTest, ParsePlanet) {
    nlohmann::json json = R"({
        "scenario_name": "Test",
        "planet": {
            "position": [5.0, 0, 0],
            "orbit_radius": 5.0,
            "orbit_speed": 0.02,
            "radius": 1.5,
            "color": [0.3, 0.6, 0.9],
            "noise_seed": 42,
            "atmosphere_enabled": true,
            "atmosphere_height": 0.3
        }
    })"_json;
    
    config::Config cfg{std::move(json)};
    config::ScenarioConfig scenario(cfg);
    
    EXPECT_EQ(scenario.name, "Test");
    EXPECT_EQ(scenario.planets.size(), 1u);
    EXPECT_DOUBLE_EQ(scenario.planets[0].orbit_radius, 5.0);
    EXPECT_TRUE(scenario.planets[0].atmosphere_enabled);
}

TEST(ScenarioConfigTest, ParseMultiplePlanets) {
    nlohmann::json json = R"({
        "scenario_name": "Multi Planet",
        "planets": [
            {
                "position": [5.0, 0, 0],
                "orbit_radius": 5.0,
                "radius": 1.5
            },
            {
                "position": [10.0, 0, 0],
                "orbit_radius": 10.0,
                "radius": 2.0
            }
        ]
    })"_json;
    
    config::Config cfg{std::move(json)};
    config::ScenarioConfig scenario(cfg);
    
    EXPECT_EQ(scenario.name, "Multi Planet");
    EXPECT_EQ(scenario.planets.size(), 2u);
}

TEST(ScenarioConfigTest, DefaultValues) {
    nlohmann::json json = R"({
        "scenario_name": "Minimal",
        "sun": {
            "radius": 5.0
        }
    })"_json;
    
    config::Config cfg{std::move(json)};
    config::ScenarioConfig scenario(cfg);
    
    EXPECT_EQ(scenario.name, "Minimal");
    EXPECT_DOUBLE_EQ(scenario.sun.radius, 5.0);
    // Default values should be used for missing fields
    EXPECT_DOUBLE_EQ(scenario.sun.position[0], 0.0);
}

TEST(ScenarioConfigTest, ParseSunAndPlanet) {
    nlohmann::json json = R"({
        "scenario_name": "Complete",
        "sun": {
            "position": [0, 0, 0],
            "radius": 10.0,
            "color": [1.0, 0.9, 0.7]
        },
        "planet": {
            "position": [5.0, 0, 0],
            "orbit_radius": 5.0,
            "radius": 1.5
        }
    })"_json;
    
    config::Config cfg{std::move(json)};
    config::ScenarioConfig scenario(cfg);
    
    EXPECT_EQ(scenario.name, "Complete");
    EXPECT_DOUBLE_EQ(scenario.sun.radius, 10.0);
    EXPECT_EQ(scenario.planets.size(), 1u);
}

TEST(ConfigTest, LoadFile) {
    // Create a temporary test config file
    const std::string temp_path = "/tmp/test_config.json";
    std::ofstream file(temp_path);
    if (!file.is_open()) {
        GTEST_FAIL_() << "Failed to create temp config file";
    }
    
    file << R"({
        "scenario_name": "File Test",
        "sun": {
            "radius": 10.0
        },
        "planet": {
            "orbit_radius": 5.0,
            "radius": 1.5
        }
    })";
    file.close();
    
    config::Config cfg = config::Config::load(temp_path);
    EXPECT_EQ(cfg.get("scenario_name"), "File Test");
    
    // Cleanup
    std::filesystem::remove(temp_path);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

#include <gtest/gtest.h>
#include "config/Config.h"
#include "config/ScenarioConfig.h"
#include <fstream>
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
    EXPECT_TRUE(cfg.getBool("missing", true));
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

TEST(ScenarioConfigTest, DevelopmentSceneUsesPlanetListAndTerrainSettings) {
    const auto path = std::string(PLANET_SOURCE_DIR) +
                      "/configs/scenarios/solar_system.json";
    const auto raw = config::Config::load(path);
    EXPECT_TRUE(raw.data().contains("planets"));
    EXPECT_FALSE(raw.data().contains("planet"));
    config::ScenarioConfig scenario(raw);
    ASSERT_EQ(scenario.planets.size(), 1u);
    EXPECT_EQ(scenario.surface_camera.planet_index, 0);
    EXPECT_DOUBLE_EQ(scenario.planets[0].surface_noise.amplitude_m, 1.0);
    EXPECT_EQ(scenario.planets[0].surface_noise.max_subdivisions, 5);
    EXPECT_DOUBLE_EQ(scenario.surface_camera.altitude *
                     scenario.metersPerWorldUnit(), 2.0);
}

TEST(ScenarioConfigTest, RejectsUnboundedOrPhysicallyInvalidTerrainSettings) {
    auto raw = R"({
        "distance_unit":"km",
        "planets":[{"radius":0.025,"surface_noise":{"amplitude_m":1.0}}]
    })"_json;
    auto parse = [&] {
        config::Config cfg{nlohmann::json(raw)};
        config::ScenarioConfig scenario(cfg);
    };
    raw["planets"][0]["surface_noise"]["max_subdivisions"] = 6;
    EXPECT_THROW(parse(), std::invalid_argument);
    raw["planets"][0]["surface_noise"]["max_subdivisions"] = 5;
    raw["planets"][0]["surface_noise"]["frequency"] = 0.0;
    EXPECT_THROW(parse(), std::invalid_argument);
    raw["planets"][0]["surface_noise"]["frequency"] = 4.0;
    raw["planets"][0]["surface_noise"]["amplitude_m"] = 25.0;
    EXPECT_THROW(parse(), std::invalid_argument);
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
        ASSERT_TRUE(false) << "Failed to create temp config file";
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

TEST(ConfigTest, MissingFileAndMalformedJsonReportErrors) {
    EXPECT_THROW(config::Config::load("configs/scenarios/does-not-exist.json"),
                 std::runtime_error);

    const auto malformed = std::filesystem::temp_directory_path() /
                           "planet-simulation-malformed-config.json";
    {
        std::ofstream file(malformed);
        file << "{ invalid JSON";
    }
    EXPECT_THROW(config::Config::load(malformed.string()), nlohmann::json::parse_error);
    std::filesystem::remove(malformed);
}

TEST(ConfigTest, WrongTypesUseDocumentedFallbacksOrThrow) {
    nlohmann::json json = R"({
        "enabled": 1,
        "position": "not an array",
        "radius": "not a number"
    })"_json;
    config::Config cfg{std::move(json)};

    EXPECT_TRUE(cfg.getBool("enabled", true));
    EXPECT_EQ(cfg.getArray("position", {1.0, 2.0}), (std::vector<double>{1.0, 2.0}));
    EXPECT_EQ(cfg.get("missing", "fallback"), "fallback");
    EXPECT_THROW(cfg.getDouble("radius", 1.0), nlohmann::json::type_error);
}

TEST(ScenarioConfigTest, ShortPositionAndColorArraysRetainSafeDefaults) {
    nlohmann::json json = R"({
        "sun": {"position": [9], "color": [0.1]},
        "planet": {"position": [5], "color": [0.2]}
    })"_json;
    config::Config cfg{std::move(json)};
    config::ScenarioConfig scenario(cfg);

    EXPECT_EQ(scenario.sun.position, (std::vector<double>{0.0, 0.0, 0.0}));
    EXPECT_EQ(scenario.sun.color, (std::vector<double>{1.0, 0.9, 0.7}));
    ASSERT_EQ(scenario.planets.size(), 1u);
    EXPECT_EQ(scenario.planets[0].position, (std::vector<double>{0.0, 0.0, 0.0}));
    EXPECT_EQ(scenario.planets[0].color, (std::vector<double>{0.3, 0.6, 0.9}));
}

TEST(ScenarioConfigTest, PlanetArrayTakesPrecedenceOverSingularPlanet) {
    nlohmann::json json = R"({
        "planet": {"position": [99, 0, 0]},
        "planets": [{"position": [5, 0, 0]}, {"position": [10, 0, 0]}]
    })"_json;
    config::Config cfg{std::move(json)};
    config::ScenarioConfig scenario(cfg);

    ASSERT_EQ(scenario.planets.size(), 2u);
    EXPECT_DOUBLE_EQ(scenario.planets[0].position[0], 5.0);
    EXPECT_DOUBLE_EQ(scenario.planets[1].position[0], 10.0);
}

TEST(ScenarioConfigTest, PlanetMetadataParsesWithoutAddingSimulationMotion) {
    nlohmann::json json = R"({
        "planet": {
            "orbit_radius": 7.0,
            "orbit_speed": 0.125,
            "noise_seed": 123,
            "atmosphere_enabled": true,
            "atmosphere_height": 0.75
        }
    })"_json;
    config::Config cfg{std::move(json)};
    config::ScenarioConfig scenario(cfg);

    ASSERT_EQ(scenario.planets.size(), 1u);
    EXPECT_DOUBLE_EQ(scenario.planets[0].orbit_radius, 7.0);
    EXPECT_DOUBLE_EQ(scenario.planets[0].orbit_speed, 0.125);
    EXPECT_EQ(scenario.planets[0].noise_seed, 123);
    EXPECT_TRUE(scenario.planets[0].atmosphere_enabled);
    EXPECT_DOUBLE_EQ(scenario.planets[0].atmosphere_height, 0.75);
}

TEST(ScenarioConfigTest, EmptyPlanetArrayDoesNotInventABody) {
    nlohmann::json json = R"({"planets": [], "planet": {"radius": 1.0}})"_json;
    config::Config cfg{std::move(json)};
    config::ScenarioConfig scenario(cfg);
    EXPECT_TRUE(scenario.planets.empty());
}

TEST(ScenarioConfigTest, LoadsTheDevelopmentSunAndPlanetFromDisk) {
    const std::string path = std::string(PLANET_SOURCE_DIR) +
                             "/configs/scenarios/solar_system.json";
    config::ScenarioConfig scenario(config::Config::load(path));
    EXPECT_EQ(scenario.name, "Solar System");
    EXPECT_EQ(scenario.distance_unit, "km");
    EXPECT_DOUBLE_EQ(scenario.metersPerWorldUnit(), 1000.0);
    EXPECT_DOUBLE_EQ(scenario.sun.radius * 2.0, 1.0); // 1 km diameter.
    ASSERT_EQ(scenario.planets.size(), 1u);
    EXPECT_DOUBLE_EQ(scenario.planets[0].radius * 2.0 *
                     scenario.metersPerWorldUnit(), 50.0);
    EXPECT_DOUBLE_EQ(scenario.planets[0].orbit_radius, 10.0);
    EXPECT_DOUBLE_EQ(scenario.planets[0].position[0] - scenario.sun.position[0], 10.0);
    EXPECT_DOUBLE_EQ(scenario.surface_camera.altitude *
                     scenario.metersPerWorldUnit(), 2.0);
    EXPECT_GT(scenario.sun.color[0], scenario.sun.color[2]);
    EXPECT_GT(scenario.planets[0].color[2], scenario.planets[0].color[0]);
}

TEST(ScenarioConfigTest, DistanceUnitAcceptsMetersAndRejectsUnknownUnits) {
    config::Config meters{nlohmann::json::parse(R"({"distance_unit":"m"})")};
    EXPECT_DOUBLE_EQ(config::ScenarioConfig(meters).metersPerWorldUnit(), 1.0);
    config::Config unknown{nlohmann::json::parse(R"({"distance_unit":"mile"})")};
    EXPECT_THROW(config::ScenarioConfig{unknown}, std::invalid_argument);
}

TEST(ScenarioConfigTest, ParsesPlanetMountedSurfaceReferenceFrame) {
    config::Config cfg{nlohmann::json::parse(R"({
        "planet": {"position": [5, 0, 0], "radius": 0.5},
        "surface_camera": {
            "reference_frame": "planet_spherical_ned",
            "planet_index": 0,
            "latitude_deg": 12.5,
            "longitude_deg": 180,
            "altitude": 0.2,
            "fov": 55
        }
    })")};
    config::ScenarioConfig scenario(cfg);
    EXPECT_TRUE(scenario.surface_camera.enabled);
    EXPECT_EQ(scenario.surface_camera.reference_frame, "planet_spherical_ned");
    EXPECT_EQ(scenario.surface_camera.planet_index, 0);
    EXPECT_DOUBLE_EQ(scenario.surface_camera.latitude_deg, 12.5);
    EXPECT_DOUBLE_EQ(scenario.surface_camera.longitude_deg, 180.0);
    EXPECT_DOUBLE_EQ(scenario.surface_camera.altitude, 0.2);
    EXPECT_DOUBLE_EQ(scenario.surface_camera.fov, 55.0);
}

TEST(ScenarioConfigTest, SurfaceCameraIsOptionalForExistingScenarios) {
    config::Config cfg{nlohmann::json::parse(R"({"planet": {"radius": 1}})")};
    EXPECT_FALSE(config::ScenarioConfig(cfg).surface_camera.enabled);
}

TEST(ScenarioConfigTest, RejectsUnknownOrUnattachedSurfaceFrames) {
    for (const auto& surface : {
             R"({"reference_frame":"world"})",
             R"({"planet_index":1})",
             R"({"latitude_deg":91})",
             R"({"altitude":-1})",
             R"({"fov":180})"}) {
        nlohmann::json raw = nlohmann::json::parse(R"({"planet":{"radius":0.5}})");
        raw["surface_camera"] = nlohmann::json::parse(surface);
        EXPECT_THROW(config::ScenarioConfig(config::Config{std::move(raw)}),
                     std::invalid_argument) << surface;
    }
}

TEST(ScenarioConfigTest, SurfaceCameraRequiresAPlanetAndObjectConfig) {
    nlohmann::json raw = nlohmann::json::parse(R"({"surface_camera":{}})");
    EXPECT_THROW(config::ScenarioConfig(config::Config{std::move(raw)}),
                 std::invalid_argument);
    raw = nlohmann::json::parse(R"({"planet":{},"surface_camera":7})");
    EXPECT_THROW(config::ScenarioConfig(config::Config{std::move(raw)}),
                 std::invalid_argument);
}

TEST(ScenarioConfigTest, SavedNedDirectionIsOptionalAndParsesPrecisely) {
    nlohmann::json raw = nlohmann::json::parse(R"({
        "planet": {"position": [5, 0, 0], "radius": 0.5},
        "surface_camera": {
            "direction_ned": [0.125, -0.5, -1.0],
            "up_ned": [0, 1, 0]
        }
    })");
    config::ScenarioConfig scenario(config::Config{std::move(raw)});
    ASSERT_TRUE(scenario.surface_camera.direction_ned);
    EXPECT_DOUBLE_EQ((*scenario.surface_camera.direction_ned)[0], 0.125);
    EXPECT_DOUBLE_EQ((*scenario.surface_camera.direction_ned)[1], -0.5);
    EXPECT_DOUBLE_EQ((*scenario.surface_camera.direction_ned)[2], -1.0);
    ASSERT_TRUE(scenario.surface_camera.up_ned);
    EXPECT_DOUBLE_EQ((*scenario.surface_camera.up_ned)[1], 1.0);
    config::Config withoutDirection{nlohmann::json::parse(R"({
        "planet": {}, "surface_camera": {}
    })")};
    EXPECT_FALSE(config::ScenarioConfig(withoutDirection).surface_camera.direction_ned);
    EXPECT_FALSE(config::ScenarioConfig(withoutDirection).surface_camera.up_ned);
}

TEST(ScenarioConfigTest, RejectsMissingOrAlignedSavedUpVectors) {
    for (const auto& start : {
             R"({"up_ned":[1,0,0]})",
             R"({"direction_ned":[1,0,0],"up_ned":[1,0,0]})",
             R"({"direction_ned":[1,0,0],"up_ned":[0,0,0]})",
             R"({"direction_ned":[1,0,0],"up_ned":[0,"up",1]})"}) {
        nlohmann::json raw = nlohmann::json::parse(R"({"planet": {}})");
        raw["surface_camera"] = nlohmann::json::parse(start);
        EXPECT_THROW(config::ScenarioConfig(config::Config{std::move(raw)}),
                     std::invalid_argument) << start;
    }
}

TEST(ScenarioConfigTest, RejectsMalformedSavedNedDirections) {
    for (const auto& invalid : {
             R"([0,0,0])", R"([1,2])", R"([1,2,3,4])",
             R"([1,"east",3])", R"("north")", R"(null)"}) {
        nlohmann::json raw = nlohmann::json::parse(R"({
            "planet": {}, "surface_camera": {}
        })");
        raw["surface_camera"]["direction_ned"] = nlohmann::json::parse(invalid);
        EXPECT_THROW(config::ScenarioConfig(config::Config{std::move(raw)}),
                     std::invalid_argument) << invalid;
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

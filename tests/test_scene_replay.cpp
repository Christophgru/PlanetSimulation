#include <gtest/gtest.h>
#include "config/SceneReplay.h"
#include "rendering/SurfaceCameraTelemetry.h"
#include "simulation/OrbitalSystem.h"

namespace {
nlohmann::json sceneJson() {
    return nlohmann::json::parse(R"({"sun":{"radius":1},"planets":[{
        "name":"earth","radius":1,"orbit":{"parent":"sun","semi_major_axis":10},
        "rotation":{"period_seconds":60,"axial_tilt_deg":20}}]})");
}
nlohmann::json savedCamera() {
    return {{"latitude_deg", -21.409888069265286}, {"longitude_deg", 82.26311950013215},
        {"altitude", 0.03}, {"planet_index", 0}, {"reference_frame", "planet_spherical_ned"},
        {"direction_ned", {-0.12335606307456787, 0.9804278199799195, -0.15344240454374442}},
        {"up_ned", {0.019154890948990685, -0.1522421152799035, -0.9881575929414461}},
        {"fov", 80}, {"walk_speed_mps", 80}, {"simulation_time_seconds", 137.1234567890123}};
}
}

TEST(SceneReplay, SimulationTimestampOverridesDefaultsAndCliOverridesSnapshot) {
    const config::ScenarioConfig old{config::Config{sceneJson()}};
    EXPECT_DOUBLE_EQ(config::replayStartTime(old), 0);
    const config::ScenarioConfig replay{config::Config{config::applyCameraReplay(sceneJson(), savedCamera())}};
    EXPECT_DOUBLE_EQ(config::replayStartTime(replay), savedCamera()["simulation_time_seconds"]);
    EXPECT_DOUBLE_EQ(config::replayStartTime(replay, 0), 0);
    EXPECT_DOUBLE_EQ(config::replayStartTime(replay, -37.5), -37.5);
}

TEST(SceneReplay, SnapshotReconstructsPoseAndLightingTimeAfterSpinAndOrbit) {
    auto document = config::applyCameraReplay(sceneJson(), savedCamera());
    const config::ScenarioConfig scenario{config::Config{nlohmann::json(document)}};
    const double seconds = config::replayStartTime(scenario);
    const auto body = simulation::OrbitalSystem(scenario).at(seconds)[1];
    const auto& settings = scenario.surface_camera;
    coordinates::PlanetLocalFrame frame(body.position, 1, body.orientation);
    PlanetSurfaceCamera original(frame, {settings.latitude_deg, settings.longitude_deg, settings.altitude},
                                  {0, 0, 0}, settings.fov);
    const auto& direction = *settings.direction_ned;
    const auto& up = *settings.up_ned;
    original.setDirectionNed({direction[0], direction[1], direction[2]}, glm::dvec3(up[0], up[1], up[2]));
    const auto snapshot = SurfaceCameraTelemetry::capture(original, settings, 1000, "km", seconds);
    const auto serialized = nlohmann::json::parse(snapshot.startConfig.dump());
    const config::ScenarioConfig restored{config::Config{config::applyCameraReplay(sceneJson(), serialized)}};
    const auto restoredBody = simulation::OrbitalSystem(restored).at(config::replayStartTime(restored))[1];
    const auto& position = restored.surface_camera;
    PlanetSurfaceCamera copy({restoredBody.position, 1, restoredBody.orientation},
        {position.latitude_deg, position.longitude_deg, position.altitude}, {0, 0, 0}, position.fov);
    const auto& d = *position.direction_ned;
    const auto& u = *position.up_ned;
    copy.setDirectionNed({d[0], d[1], d[2]}, glm::dvec3(u[0], u[1], u[2]));
    EXPECT_DOUBLE_EQ(config::replayStartTime(restored), seconds);
    EXPECT_LT(glm::length(original.position() - copy.position()), 1e-12);
    EXPECT_LT(glm::length(original.direction() - copy.direction()), 1e-12);
    EXPECT_LT(glm::length(original.up() - copy.up()), 1e-12);
    EXPECT_NE(snapshot.format().find("Simulation time: " + nlohmann::json(seconds).dump()), std::string::npos);
}

TEST(SceneReplay, ReportsSimulationClockInsteadOfWallTimeUnderPauseAndSpeedChanges) {
    const config::ScenarioConfig scenario{config::Config{config::applyCameraReplay(sceneJson(), savedCamera())}};
    PlanetSurfaceCamera camera({{10, 0, 0}, 1}, {0, 0, 0.03}, {0, 0, 0}, 80);
    SurfaceCameraTelemetry telemetry;
    simulation::SimulationClock clock(100, 1000);
    clock.scaleSpeed(2, 1001); // 101 s
    clock.togglePause(1003); // 105 s
    const auto paused = telemetry.sample(true, 2000, camera, scenario.surface_camera, 1000, "km", clock.advanceTo(2000));
    ASSERT_TRUE(paused);
    EXPECT_DOUBLE_EQ(paused->startConfig["simulation_time_seconds"], 105);
    clock.togglePause(2001);
    const auto resumed = telemetry.sample(true, 2006, camera, scenario.surface_camera, 1000, "km", clock.advanceTo(2006));
    ASSERT_TRUE(resumed);
    EXPECT_DOUBLE_EQ(resumed->startConfig["simulation_time_seconds"], 115);
}

TEST(SceneReplay, ImageSidecarRestoresResolvedSceneInsteadOfEditedWorkingConfig) {
    const auto archived = sceneJson();
    auto edited = sceneJson();
    edited["sun"]["absolute_magnitude"] = -10;
    const auto restored = config::applyCameraReplay(edited, {{"scenario", archived}, {"surface_camera", savedCamera()}});
    EXPECT_EQ(restored["sun"], archived["sun"]);
    EXPECT_EQ(restored["surface_camera"], savedCamera());
}

TEST(SceneReplay, RejectsMalformedSnapshotsAndNonfiniteTime) {
    for (auto time : {nlohmann::json(nullptr), nlohmann::json("12"), nlohmann::json(true),
                     nlohmann::json(std::numeric_limits<double>::infinity()),
                     nlohmann::json(std::numeric_limits<double>::quiet_NaN())}) {
        auto camera = savedCamera();
        camera["simulation_time_seconds"] = time;
        EXPECT_ANY_THROW(config::applyCameraReplay(sceneJson(), camera));
    }
    EXPECT_THROW(config::applyCameraReplay(sceneJson(), nlohmann::json::array()), std::invalid_argument);
    EXPECT_THROW(config::applyCameraReplay(sceneJson(), nlohmann::json::object()), std::invalid_argument);
    auto legacy = savedCamera();
    legacy.erase("simulation_time_seconds");
    EXPECT_NO_THROW(config::applyCameraReplay(sceneJson(), legacy));
}

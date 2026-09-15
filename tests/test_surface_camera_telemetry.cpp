#include <gtest/gtest.h>
#include <limits>
#include <nlohmann/json.hpp>
#include "rendering/SurfaceCameraTelemetry.h"

namespace {
PlanetSurfaceCamera makeCamera() {
    return PlanetSurfaceCamera({{5.0, 0.0, 0.0}, 0.5},
                               {0.0, 180.0, 0.2}, {0.0, 0.0, 0.0}, 60.0);
}

nlohmann::json valueAfter(const std::string& output, const std::string& label) {
    const auto begin = output.find(label);
    if (begin == std::string::npos) return nullptr;
    const auto valueBegin = begin + label.size();
    const auto end = output.find('\n', valueBegin);
    return nlohmann::json::parse(output.substr(valueBegin, end - valueBegin));
}
}

TEST(SurfaceCameraTelemetryTest, PrintsOnEntryThenAtFiveSecondIntervals) {
    auto camera = makeCamera();
    config::SurfaceCameraConfig settings;
    SurfaceCameraTelemetry telemetry;
    EXPECT_FALSE(telemetry.sample(false, 10.0, camera, settings));
    EXPECT_TRUE(telemetry.sample(true, 10.0, camera, settings));
    EXPECT_FALSE(telemetry.sample(true, 14.999, camera, settings));
    EXPECT_TRUE(telemetry.sample(true, 15.0, camera, settings));
    EXPECT_FALSE(telemetry.sample(true, 15.001, camera, settings));
    EXPECT_TRUE(telemetry.sample(true, 20.0, camera, settings));
}

TEST(SurfaceCameraTelemetryTest, LeavingAndReenteringResetsTheClock) {
    auto camera = makeCamera();
    config::SurfaceCameraConfig settings;
    SurfaceCameraTelemetry telemetry;
    ASSERT_TRUE(telemetry.sample(true, 0.0, camera, settings));
    EXPECT_FALSE(telemetry.sample(false, 4.0, camera, settings));
    EXPECT_TRUE(telemetry.sample(true, 4.1, camera, settings));
    EXPECT_FALSE(telemetry.sample(true, 9.0, camera, settings));
    EXPECT_TRUE(telemetry.sample(true, 9.1, camera, settings));
}

TEST(SurfaceCameraTelemetryTest, LongFrameStallProducesOneCurrentSnapshot) {
    auto camera = makeCamera();
    config::SurfaceCameraConfig settings;
    SurfaceCameraTelemetry telemetry;
    ASSERT_TRUE(telemetry.sample(true, 0.0, camera, settings));
    camera.walk(1, 1, 1.0);
    camera.look(120.0, -40.0);
    const auto current = telemetry.sample(true, 23.0, camera, settings);
    ASSERT_TRUE(current);
    EXPECT_EQ(current->worldPosition, camera.position());
    EXPECT_EQ(current->worldDirection, camera.direction());
    EXPECT_FALSE(telemetry.sample(true, 23.01, camera, settings));
    EXPECT_TRUE(telemetry.sample(true, 28.0, camera, settings));
}

TEST(SurfaceCameraTelemetryTest, InvalidClockTimeDoesNotPrintOrHoldOldCadence) {
    auto camera = makeCamera();
    config::SurfaceCameraConfig settings;
    SurfaceCameraTelemetry telemetry;
    ASSERT_TRUE(telemetry.sample(true, 0.0, camera, settings));
    EXPECT_FALSE(telemetry.sample(true, std::numeric_limits<double>::quiet_NaN(),
                                  camera, settings));
    EXPECT_TRUE(telemetry.sample(true, 1.0, camera, settings));
}

TEST(SurfaceCameraTelemetryTest, PrintedPositionAndDirectionAreValidJsonNumbers) {
    auto camera = makeCamera();
    camera.walk(1, 0, 0.75);
    camera.look(80.0, 45.0);
    config::SurfaceCameraConfig settings;
    SurfaceCameraTelemetry telemetry;
    const auto snapshot = telemetry.sample(true, 0.0, camera, settings);
    ASSERT_TRUE(snapshot);
    const std::string output = snapshot->format();
    const auto position = valueAfter(output, "Surface camera position (world, km): ");
    const auto direction = valueAfter(output, "Surface camera direction (world, unit vector): ");
    const auto config = valueAfter(output, "surface_camera start value: ");
    ASSERT_TRUE(position.is_array());
    ASSERT_TRUE(direction.is_array());
    ASSERT_TRUE(config.is_object());
    ASSERT_EQ(position.size(), 3u);
    ASSERT_EQ(direction.size(), 3u);
    EXPECT_DOUBLE_EQ(position[0].get<double>(), camera.position().x);
    EXPECT_DOUBLE_EQ(position[1].get<double>(), camera.position().y);
    EXPECT_DOUBLE_EQ(position[2].get<double>(), camera.position().z);
    EXPECT_DOUBLE_EQ(direction[0].get<double>(), camera.direction().x);
    EXPECT_DOUBLE_EQ(direction[1].get<double>(), camera.direction().y);
    EXPECT_DOUBLE_EQ(direction[2].get<double>(), camera.direction().z);
    EXPECT_EQ(config, snapshot->startConfig);
    EXPECT_TRUE(config.contains("up_ned"));
}

TEST(SurfaceCameraTelemetryTest, LabelsPlanetPositionInDegreesAndMeters) {
    PlanetSurfaceCamera camera({{10.0, 0.0, 0.0}, 0.025},
        {-55.409888069265286, 0.26311950013215024, 0.002},
        {0.0, 0.0, 0.0}, 60.0, 0.002);
    config::SurfaceCameraConfig settings;
    SurfaceCameraTelemetry telemetry;
    const auto snapshot = telemetry.sample(true, 0.0, camera, settings, 1000.0, "km");
    ASSERT_TRUE(snapshot);
    const std::string output = snapshot->format();
    EXPECT_NE(output.find("Surface camera position (world, km): "), std::string::npos);
    EXPECT_NE(output.find("latitude=-55.409888069265286 deg"), std::string::npos);
    EXPECT_NE(output.find("longitude=0.26311950013215024 deg"), std::string::npos);
    EXPECT_NE(output.find("altitude=2.0 m"), std::string::npos);
    EXPECT_DOUBLE_EQ(snapshot->startConfig.at("altitude").get<double>(), 0.002);
}

TEST(SurfaceCameraTelemetryTest, ConfigSnippetRestoresWalkedAndTurnedCamera) {
    coordinates::PlanetLocalFrame frame({1000000.0, -2000000.0, 3000000.0}, 6000.0);
    PlanetSurfaceCamera original(frame, {42.5, -121.25, 12.0},
                                 {0.0, 0.0, 0.0}, 55.0);
    original.walk(1, 1, 1.25);
    original.look(170.0, 80.0);
    config::SurfaceCameraConfig settings;
    settings.reference_frame = "planet_spherical_ned";
    settings.planet_index = 0;
    SurfaceCameraTelemetry telemetry;
    const auto snapshot = telemetry.sample(true, 0.0, original, settings);
    ASSERT_TRUE(snapshot);
    const auto copyReady = valueAfter(snapshot->format(), "surface_camera start value: ");
    nlohmann::json raw{
        {"sun", {{"position", {0.0, 0.0, 0.0}}}},
        {"planet", {{"position", {1000000.0, -2000000.0, 3000000.0}},
                    {"radius", 6000.0}}},
        {"surface_camera", copyReady}
    };
    config::ScenarioConfig restoredConfig(config::Config{std::move(raw)});
    ASSERT_TRUE(restoredConfig.surface_camera.direction_ned);
    ASSERT_TRUE(restoredConfig.surface_camera.up_ned);
    const auto& start = restoredConfig.surface_camera;
    PlanetSurfaceCamera restored(frame,
        {start.latitude_deg, start.longitude_deg, start.altitude},
        {0.0, 0.0, 0.0}, start.fov);
    const auto& saved = *start.direction_ned;
    const auto& savedUp = *start.up_ned;
    restored.setDirectionNed({saved[0], saved[1], saved[2]},
                             glm::dvec3(savedUp[0], savedUp[1], savedUp[2]));
    EXPECT_NEAR(glm::length(restored.position() - original.position()), 0.0, 1e-7);
    EXPECT_NEAR(glm::length(restored.direction() - original.direction()), 0.0, 1e-10);
    EXPECT_NEAR(glm::length(restored.up() - original.up()), 0.0, 1e-10);
    EXPECT_FLOAT_EQ(restored.fov(), original.fov());
}

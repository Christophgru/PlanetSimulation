#include <gtest/gtest.h>
#include <limits>
#include <glm/gtc/matrix_transform.hpp>
#include "rendering/PlanetSurfaceCamera.h"
#include "rendering/SceneTransforms.h"

TEST(PlanetSurfaceCameraTest, ElevatedGroundUsesTwoMeterClearanceForClipping) {
    config::PlanetConfig::TerrainLandscape landscape;
    landscape.enabled = true;
    landscape.elevation_offset_m = 12.0;
    config::PlanetConfig::TerrainLod lod;
    const rendering::TerrainSurface terrain({}, lod, 0.1, 1000.0, landscape);
    PlanetSurfaceCamera camera({{10.0, 0.0, 0.0}, 0.1},
                               {0.0, 180.0, 0.002}, {0.0, 0.0, 0.0}, 60.0);
    camera.mountTerrain(terrain, 0.002);
    ASSERT_NEAR(camera.location().altitude, 0.014, 1e-10);
    EXPECT_NEAR(camera.groundClearance(), 0.002, 1e-10);
    const auto clip = rendering::surfaceClipPlanes(
        camera.configuredClearance(), glm::length(camera.position()), 0.5);
    EXPECT_FLOAT_EQ(clip.nearPlane, 0.0002f); // 0.2 m, not 1.4 m from sphere altitude.
}

TEST(PlanetSurfaceCameraTest, MountedPositionLooksAtTheSunWithNorthUp) {
    coordinates::PlanetLocalFrame frame({5.0, 0.0, 0.0}, 0.5);
    PlanetSurfaceCamera camera(frame, {0.0, 180.0, 0.2}, {0.0, 0.0, 0.0}, 60.0);
    EXPECT_NEAR(camera.position().x, 4.3, 1e-10);
    EXPECT_NEAR(camera.position().y, 0.0, 1e-10);
    EXPECT_NEAR(camera.up().z, 1.0, 1e-10);

    const glm::vec4 eyeInView = camera.getViewMatrix() * glm::vec4(camera.position(), 1.0);
    const glm::vec4 sunInView = camera.getViewMatrix() * glm::vec4(camera.target(), 1.0);
    EXPECT_NEAR(eyeInView.x, 0.0, 1e-5);
    EXPECT_NEAR(eyeInView.y, 0.0, 1e-5);
    EXPECT_NEAR(eyeInView.z, 0.0, 1e-5);
    EXPECT_NEAR(sunInView.x, 0.0, 1e-5);
    EXPECT_NEAR(sunInView.y, 0.0, 1e-5);
    EXPECT_LT(sunInView.z, -4.0);

    const glm::mat4 clip = rendering::perspectiveProjection(camera.fov(), 4.0f / 3.0f) *
                           camera.getViewMatrix();
    const glm::vec4 sunClip = clip * glm::vec4(camera.target(), 1.0);
    EXPECT_NEAR(sunClip.x / sunClip.w, 0.0, 1e-5);
    EXPECT_NEAR(sunClip.y / sunClip.w, 0.0, 1e-5);
}

TEST(PlanetSurfaceCameraTest, MovingThePlanetMovesTheMountedCamera) {
    PlanetSurfaceCamera original({{5.0, 0.0, 0.0}, 0.5},
                                 {0.0, 180.0, 0.2}, {0.0, 0.0, 0.0}, 60.0);
    PlanetSurfaceCamera moved({{7.0, 1.0, 0.0}, 0.5},
                              {0.0, 180.0, 0.2}, {0.0, 0.0, 0.0}, 60.0);
    EXPECT_NEAR(moved.position().x - original.position().x, 2.0, 1e-10);
    EXPECT_NEAR(moved.position().y - original.position().y, 1.0, 1e-10);
}

TEST(PlanetSurfaceCameraTest, LookingAlongNorthKeepsRadialUpAndFiniteView) {
    coordinates::PlanetLocalFrame frame({0.0, 0.0, 0.0}, 1.0);
    PlanetSurfaceCamera camera(frame, {0.0, 0.0, 0.1}, {1.1, 0.0, 10.0}, 60.0);
    EXPECT_NEAR(camera.up().x, 1.0, 1e-10);
    EXPECT_NEAR(camera.up().y, 0.0, 1e-10);
    const glm::mat4 view = camera.getViewMatrix();
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            EXPECT_TRUE(std::isfinite(view[column][row]));
        }
    }
}

TEST(PlanetSurfaceCameraTest, SunAndCameraCannotCoincide) {
    coordinates::PlanetLocalFrame frame({5.0, 0.0, 0.0}, 0.5);
    EXPECT_THROW(PlanetSurfaceCamera(frame, {0.0, 180.0, 0.2},
                                     {4.3, 0.0, 0.0}, 60.0), std::invalid_argument);
    EXPECT_THROW(PlanetSurfaceCamera(frame, {0.0, 180.0, 0.2},
                                     {0.0, 0.0, 0.0}, 180.0), std::invalid_argument);
}

TEST(PlanetSurfaceCameraTest, MouseLookUsesIndependentHeadingAndPitch) {
    PlanetSurfaceCamera camera({{5.0, 0.0, 0.0}, 0.5},
                               {0.0, 180.0, 0.2}, {0.0, 0.0, 0.0}, 60.0);
    camera.setDirectionNed({1.0, 1.0, 0.0});
    const double initialHeading = std::atan2(camera.directionNed().y,
                                             camera.directionNed().x);
    camera.look(0.0, -500.0);
    const glm::dvec3 afterVertical = camera.directionNed();
    EXPECT_NEAR(std::atan2(afterVertical.y, afterVertical.x), initialHeading, 1e-10);

    camera.look(100.0, 0.0);
    const glm::dvec3 afterHorizontal = camera.directionNed();
    EXPECT_NEAR(std::remainder(std::atan2(afterHorizontal.y, afterHorizontal.x) -
                               initialHeading, 2.0 * glm::pi<double>()),
                0.5, 1e-10);
    EXPECT_NEAR(glm::length(camera.direction()), 1.0, 1e-10);
    EXPECT_NEAR(glm::dot(camera.direction(), camera.up()), 0.0, 1e-10);
}

TEST(PlanetSurfaceCameraTest, VerticalLookStopsAtNinetyDegreesWithoutSpinning) {
    PlanetSurfaceCamera camera({{5.0, 0.0, 0.0}, 0.5},
                               {20.0, 110.0, 0.2}, {0.0, 0.0, 0.0}, 60.0);
    camera.setDirectionNed({1.0, 0.5, 0.0});
    const double heading = std::atan2(camera.directionNed().y,
                                      camera.directionNed().x);
    for (int step = 0; step < 20; ++step) camera.look(0.0, -500.0);
    const glm::dvec3 atLimit = camera.directionNed();
    const glm::dvec3 upAtLimit = camera.up();
    EXPECT_NEAR(glm::degrees(std::asin(-atLimit.z)), 89.9, 1e-8);
    EXPECT_NEAR(std::atan2(atLimit.y, atLimit.x), heading, 1e-10);

    for (int step = 0; step < 20; ++step) camera.look(0.0, -500.0);
    EXPECT_NEAR(glm::length(camera.directionNed() - atLimit), 0.0, 1e-12);
    EXPECT_NEAR(glm::length(camera.up() - upAtLimit), 0.0, 1e-12);

    for (int step = 0; step < 40; ++step) camera.look(0.0, 500.0);
    const glm::dvec3 atLowerLimit = camera.directionNed();
    const glm::dvec3 upAtLowerLimit = camera.up();
    EXPECT_NEAR(glm::degrees(std::asin(-atLowerLimit.z)), -89.9, 1e-8);
    EXPECT_NEAR(std::atan2(atLowerLimit.y, atLowerLimit.x), heading, 1e-10);
    for (int step = 0; step < 20; ++step) camera.look(0.0, 500.0);
    EXPECT_NEAR(glm::length(camera.directionNed() - atLowerLimit), 0.0, 1e-12);
    EXPECT_NEAR(glm::length(camera.up() - upAtLowerLimit), 0.0, 1e-12);

    const glm::mat4 view = camera.getViewMatrix();
    for (int column = 0; column < 4; ++column)
        for (int row = 0; row < 4; ++row)
            EXPECT_TRUE(std::isfinite(view[column][row]));
}

TEST(PlanetSurfaceCameraTest, LookingDownEventuallyFacesThePlanetWithoutEnteringIt) {
    PlanetSurfaceCamera camera({{5.0, 0.0, 0.0}, 0.5},
                               {0.0, 180.0, 0.2}, {0.0, 0.0, 0.0}, 60.0);
    for (int step = 0; step < 4; ++step) camera.look(0.0, 200.0);
    EXPECT_GT(glm::dot(camera.direction(), camera.down()), 0.1);
    EXPECT_GT(glm::length(camera.position() - camera.frame().center()),
              camera.frame().radius());
    const glm::mat4 view = camera.getViewMatrix();
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            EXPECT_TRUE(std::isfinite(view[column][row]));
        }
    }
}

TEST(PlanetSurfaceCameraTest, WalkingFollowsTheSphereAndKeepsAltitudeAndLocalDown) {
    PlanetSurfaceCamera camera({{5.0, 0.0, 0.0}, 0.5},
                               {0.0, 180.0, 0.2}, {0.0, 0.0, 0.0}, 60.0);
    camera.walk(1, 0, 1.0);
    EXPECT_GT(camera.location().latitudeDeg, 0.0);
    EXPECT_NEAR(camera.location().altitude, 0.2, 1e-10);
    EXPECT_NEAR(glm::length(camera.position() - camera.frame().center()),
                0.7, 1e-10);
    EXPECT_NEAR(glm::dot(camera.down(),
                         glm::normalize(camera.frame().center() - camera.position())),
                1.0, 1e-10);
    camera.walk(-1, 0, 1.0);
    EXPECT_NEAR(camera.position().x, 4.3, 1e-10);
    EXPECT_NEAR(camera.position().z, 0.0, 1e-10);
}

TEST(PlanetSurfaceCameraTest, WalkingRightAndDiagonalUseTangentDirections) {
    PlanetSurfaceCamera camera({{5.0, 0.0, 0.0}, 0.5},
                               {0.0, 180.0, 0.2}, {0.0, 0.0, 0.0}, 60.0);
    const glm::dvec3 initialRadial = glm::normalize(camera.position() -
                                                    camera.frame().center());
    camera.walk(0, 1, 1.0);
    EXPECT_LT(camera.position().y, 0.0); // East at longitude 180 degrees.
    camera.walk(0, -1, 1.0);
    EXPECT_NEAR(camera.position().y, 0.0, 1e-10);
    camera.walk(1, 1, 1.0);
    const glm::dvec3 movedRadial = glm::normalize(camera.position() -
                                                  camera.frame().center());
    EXPECT_NEAR(std::acos(glm::dot(initialRadial, movedRadial)),
                0.4 / 0.7, 1e-10); // Diagonal speed is normalized.
}

TEST(PlanetSurfaceCameraTest, AutomaticEntryClampsAnInsidePositionAboveTheSphere) {
    PlanetSurfaceCamera camera({{5.0, 0.0, 0.0}, 0.5},
                               {0.0, 180.0, 0.2}, {0.0, 0.0, 0.0}, 60.0);
    camera.enterFromWorld({5.1, 0.0, 0.0}, {0.0, 0.0, 0.0});
    EXPECT_NEAR(glm::length(camera.position() - camera.frame().center()),
                0.51, 1e-10);
    EXPECT_NEAR(camera.location().altitude, 0.01, 1e-10);
    EXPECT_GT(glm::dot(camera.direction(), camera.down()), 0.0); // Sun is inward here.
}

TEST(PlanetSurfaceCameraTest, SavedNedDirectionRestoresAViewAwayFromTheSun) {
    PlanetSurfaceCamera camera({{5.0, 0.0, 0.0}, 0.5},
                               {20.0, 110.0, 0.2}, {0.0, 0.0, 0.0}, 60.0);
    const glm::dvec3 sunDirection = camera.direction();
    const glm::dvec3 saved{1.0, 2.0, -0.5};
    camera.setDirectionNed(saved);
    EXPECT_GT(glm::length(camera.direction() - sunDirection), 0.1);
    EXPECT_NEAR(glm::length(camera.directionNed() - glm::normalize(saved)),
                0.0, 1e-10);
    camera.walk(1, 0, 0.3);
    EXPECT_NEAR(glm::length(camera.directionNed() - glm::normalize(saved)),
                0.0, 1e-10);
}

TEST(PlanetSurfaceCameraTest, RejectsInvalidSavedViewDirection) {
    PlanetSurfaceCamera camera({{5.0, 0.0, 0.0}, 0.5},
                               {0.0, 180.0, 0.2}, {0.0, 0.0, 0.0}, 60.0);
    EXPECT_THROW(camera.setDirectionNed({0.0, 0.0, 0.0}), std::invalid_argument);
    EXPECT_THROW(camera.setDirectionNed(
        {std::numeric_limits<double>::quiet_NaN(), 0.0, 1.0}),
        std::invalid_argument);
    const glm::dvec3 before = camera.direction();
    EXPECT_THROW(camera.setDirectionNed({1.0, 0.0, 0.0},
                                        glm::dvec3(1.0, 0.0, 0.0)),
                 std::invalid_argument);
    EXPECT_EQ(camera.direction(), before);
    camera.setDirectionNed({1e308, 0.0, 0.0});
    EXPECT_NEAR(glm::length(camera.directionNed() - glm::dvec3(1.0, 0.0, 0.0)),
                0.0, 1e-10);
}

TEST(PlanetSurfaceCameraTest, SavedUpRestoresRollAtAVerticalView) {
    coordinates::PlanetLocalFrame frame({5.0, 0.0, 0.0}, 0.5);
    PlanetSurfaceCamera original(frame, {0.0, 180.0, 0.2},
                                 {0.0, 0.0, 0.0}, 60.0);
    original.setDirectionNed({0.0, 0.0, -1.0},
                             glm::dvec3(0.0, 1.0, 0.0));
    PlanetSurfaceCamera restored(frame, {0.0, 180.0, 0.2},
                                 {0.0, 0.0, 0.0}, 60.0);
    restored.setDirectionNed(original.directionNed(), original.upNed());
    EXPECT_NEAR(glm::length(restored.direction() - original.direction()),
                0.0, 1e-10);
    EXPECT_NEAR(glm::length(restored.up() - original.up()), 0.0, 1e-10);
    const glm::mat4 originalView = original.getViewMatrix();
    const glm::mat4 restoredView = restored.getViewMatrix();
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            EXPECT_NEAR(restoredView[column][row], originalView[column][row], 1e-5);
        }
    }
}

TEST(PlanetSurfaceCameraTest, TwoMeterPerSecondWalkOnFiftyMeterPlanet) {
    PlanetSurfaceCamera camera({{10.0, 0.0, 0.0}, 0.025},
                               {0.0, 180.0, 0.002}, {0.0, 0.0, 0.0},
                               60.0, 0.002);
    const glm::dvec3 radialBefore = glm::normalize(
        camera.position() - camera.frame().center());
    camera.walk(1, 0, 1.0);
    const glm::dvec3 radialAfter = glm::normalize(
        camera.position() - camera.frame().center());
    EXPECT_NEAR(std::acos(glm::dot(radialBefore, radialAfter)),
                0.002 / 0.027, 1e-10);
    EXPECT_NEAR(camera.location().altitude * 1000.0, 2.0, 1e-10);
    EXPECT_DOUBLE_EQ(camera.walkSpeed() * 1000.0, 2.0);
}

TEST(PlanetSurfaceCameraTest, TerrainMountAndWalkingKeepTwoMetersAboveSampledGround) {
    config::PlanetConfig::SurfaceNoiseFunction hills;
    hills.amplitude_m = 0.8;
    config::PlanetConfig::SurfaceNoiseFunction ridges;
    ridges.type = "ridged_fbm";
    ridges.amplitude_m = 0.35;
    ridges.frequency = 18.0;
    ridges.seed = 771;
    rendering::TerrainSurface terrain({hills, ridges},
                                      config::PlanetConfig::TerrainLod{},
                                      0.025, 1000.0);
    PlanetSurfaceCamera camera({{10.0, 0.0, 0.0}, 0.025},
                               {-22.4, 67.26, 0.002}, {0.0, 0.0, 0.0},
                               60.0, 0.002);
    camera.mountTerrain(terrain, 0.002);
    const double firstAltitude = camera.location().altitude;
    EXPECT_NEAR(camera.groundClearance() * 1000.0, 2.0, 1e-9);
    EXPECT_NEAR(glm::length(camera.position() - camera.frame().center()),
                0.025 + firstAltitude, 1e-10);
    camera.walk(1, 0, 1.0);
    EXPECT_NE(camera.location().altitude, firstAltitude);
    EXPECT_NEAR(camera.groundClearance() * 1000.0, 2.0, 1e-9);
    EXPECT_NEAR(camera.location().altitude,
                terrain.heightAt(-camera.down()) + 0.002, 1e-10);
}

TEST(PlanetSurfaceCameraTest, EnteringSurfaceModeSnapsToTerrainClearance) {
    config::PlanetConfig::SurfaceNoiseFunction noise;
    noise.amplitude_m = 1.0;
    rendering::TerrainSurface terrain({noise}, config::PlanetConfig::TerrainLod{},
                                      0.025, 1000.0);
    PlanetSurfaceCamera camera({{10.0, 0.0, 0.0}, 0.025},
                               {0.0, 180.0, 0.002}, {0.0, 0.0, 0.0},
                               60.0, 0.002);
    camera.mountTerrain(terrain, 0.002);
    camera.enterFromWorld({10.0, 0.0, 0.01}, {0.0, 0.0, 0.0});
    EXPECT_NEAR(camera.location().latitudeDeg, 90.0, 1e-10);
    EXPECT_NEAR(camera.groundClearance() * 1000.0, 2.0, 1e-9);
    EXPECT_GT(glm::length(camera.position() - camera.frame().center()), 0.025);
}

TEST(PlanetSurfaceCameraTest, WaterSupportsEyeWhenTerrainIsSubmerged) {
    config::PlanetConfig::SurfaceNoiseFunction noise;
    noise.amplitude_m = 1.0;
    const rendering::TerrainSurface terrain({noise}, config::PlanetConfig::TerrainLod{},
                                             0.025, 1000.0);
    PlanetSurfaceCamera camera({{10.0, 0.0, 0.0}, 0.025},
                               {0.0, 180.0, 0.002}, {0.0, 0.0, 0.0},
                               60.0, 0.002);
    camera.mountTerrain(terrain, 0.002, 0.01);
    EXPECT_NEAR(camera.location().altitude, 0.012, 1e-12);
    EXPECT_NEAR(camera.groundClearance(), 0.002, 1e-12);
    camera.walk(1, 0, 1.0);
    EXPECT_NEAR(camera.location().altitude, 0.012, 1e-12);
    EXPECT_NEAR(camera.groundClearance(), 0.002, 1e-12);
}

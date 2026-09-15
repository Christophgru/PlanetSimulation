#include <gtest/gtest.h>
#include "rendering/CameraInput.h"

TEST(CameraInputTest, DragEventsApplyTheInvertedDirections) {
    OrbitCamera camera(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 10.0f));
    PlanetSurfaceCamera surface({{5.0, 0.0, 0.0}, 0.5},
                                {0.0, 180.0, 0.2}, {0.0, 0.0, 0.0}, 60.0);
    CameraInput input(camera, &surface);
    input.moveCursor(100.0, 100.0);
    EXPECT_FLOAT_EQ(camera.position.x, 0.0f);
    input.beginDrag(100.0, 100.0);
    input.moveCursor(120.0, 80.0);
    EXPECT_LT(camera.position.x, 0.0f);
    EXPECT_LT(camera.position.y, 0.0f);
    const glm::vec3 stoppedPosition = camera.position;
    input.endDrag();
    input.moveCursor(500.0, 500.0);
    EXPECT_EQ(camera.position, stoppedPosition);
}

TEST(CameraInputTest, SwitchingViewsCancelsDragAndSuspendsOrbitInput) {
    OrbitCamera camera(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 10.0f));
    PlanetSurfaceCamera surface({{5.0, 0.0, 0.0}, 0.5},
                                {0.0, 180.0, 0.2}, {0.0, 0.0, 0.0}, 60.0);
    CameraInput input(camera, &surface);
    input.beginDrag(10.0, 10.0);
    EXPECT_TRUE(input.dragging());
    input.selectSurface();
    EXPECT_EQ(input.mode(), CameraMode::Surface);
    EXPECT_FALSE(input.dragging());
    const glm::vec3 position = camera.position;
    input.beginDrag(10.0, 10.0);
    input.moveCursor(200.0, 200.0);
    input.scroll(100.0);
    EXPECT_EQ(camera.position, position);
    input.selectOrbit();
    EXPECT_EQ(input.mode(), CameraMode::Orbit);
    input.moveCursor(300.0, 300.0);
    EXPECT_EQ(camera.position, position);
    input.beginDrag(300.0, 300.0);
    input.moveCursor(320.0, 300.0);
    EXPECT_NE(camera.position, position);
}

TEST(CameraInputTest, UnconfiguredSurfaceModeCannotBeSelected) {
    OrbitCamera camera(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 10.0f));
    CameraInput input(camera, nullptr);
    input.selectSurface();
    EXPECT_EQ(input.mode(), CameraMode::Orbit);
}

TEST(CameraInputTest, ScrollControlsZoomOnlyInOrbitMode) {
    OrbitCamera camera(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 10.0f));
    PlanetSurfaceCamera surface({{5.0, 0.0, 0.0}, 0.5},
                                {0.0, 180.0, 0.2}, {0.0, 0.0, 0.0}, 60.0);
    CameraInput input(camera, &surface);
    input.scroll(1.0);
    EXPECT_LT(glm::length(camera.position), 10.0f);
    input.selectSurface();
    const float distance = glm::length(camera.position);
    input.scroll(1.0);
    EXPECT_FLOAT_EQ(glm::length(camera.position), distance);
}

TEST(CameraInputTest, SurfaceMouseLookNeedsNoDragButton) {
    OrbitCamera camera(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 10.0f));
    PlanetSurfaceCamera surface({{5.0, 0.0, 0.0}, 0.5},
                                {0.0, 180.0, 0.2}, {0.0, 0.0, 0.0}, 60.0);
    CameraInput input(camera, &surface);
    input.selectSurface();
    const glm::dvec3 initialDirection = surface.direction();
    input.moveCursor(100.0, 100.0); // Establish baseline on cursor capture.
    EXPECT_EQ(surface.direction(), initialDirection);
    input.moveCursor(120.0, 100.0);
    EXPECT_GT(glm::length(surface.direction() - initialDirection), 0.05);
    input.selectOrbit();
    const glm::dvec3 stoppedDirection = surface.direction();
    input.moveCursor(300.0, 300.0);
    EXPECT_EQ(surface.direction(), stoppedDirection);
}

TEST(CameraInputTest, WalkingKeysAreActiveOnlyInSurfaceMode) {
    OrbitCamera camera(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 10.0f));
    PlanetSurfaceCamera surface({{5.0, 0.0, 0.0}, 0.5},
                                {0.0, 180.0, 0.2}, {0.0, 0.0, 0.0}, 60.0);
    CameraInput input(camera, &surface);
    const glm::dvec3 initialPosition = surface.position();
    input.update(WalkKeys{.forward = true}, 1.0);
    EXPECT_EQ(surface.position(), initialPosition);
    input.selectSurface();
    input.update(WalkKeys{.forward = true}, 1.0);
    EXPECT_GT(surface.position().z, initialPosition.z);
    EXPECT_NEAR(surface.location().altitude, 0.2, 1e-10);
}

TEST(CameraInputTest, ProximityEnablesSurfaceModeAtOnePointOneDiameters) {
    PlanetSurfaceCamera surface({{5.0, 0.0, 0.0}, 0.5},
                                {0.0, 180.0, 0.2}, {0.0, 0.0, 0.0}, 60.0);
    OrbitCamera orbit(glm::vec3(0.0f), glm::vec3(6.2f, 0.0f, 0.0f));
    CameraInput input(orbit, &surface);
    input.update({}, 0.0);
    EXPECT_EQ(input.mode(), CameraMode::Orbit);
    orbit.position = glm::vec3(6.0f, 0.0f, 0.0f);
    input.update({}, 0.0);
    EXPECT_EQ(input.mode(), CameraMode::Surface);
    EXPECT_TRUE(input.autoActivated());
    EXPECT_NEAR(surface.position().x, 6.0, 1e-10);
    EXPECT_NEAR(glm::length(surface.position() - surface.frame().center()),
                1.0, 1e-10);
    EXPECT_LT(glm::dot(surface.down(), glm::dvec3(orbit.position) -
                                      glm::dvec3(5.0, 0.0, 0.0)),
              0.0);
}

TEST(CameraInputTest, ExplicitOrbitSelectionCanLeaveTheProximityZone) {
    PlanetSurfaceCamera surface({{5.0, 0.0, 0.0}, 0.5},
                                {0.0, 180.0, 0.2}, {0.0, 0.0, 0.0}, 60.0);
    OrbitCamera orbit(glm::vec3(0.0f), glm::vec3(6.0f, 0.0f, 0.0f));
    CameraInput input(orbit, &surface);
    input.update({}, 0.0);
    ASSERT_EQ(input.mode(), CameraMode::Surface);
    input.selectOrbit();
    input.update({}, 0.0);
    EXPECT_EQ(input.mode(), CameraMode::Orbit);
    orbit.position = glm::vec3(7.0f, 0.0f, 0.0f);
    input.update({}, 0.0);
    orbit.position = glm::vec3(6.0f, 0.0f, 0.0f);
    input.update({}, 0.0);
    EXPECT_EQ(input.mode(), CameraMode::Surface);
}

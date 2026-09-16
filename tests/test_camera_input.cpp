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

TEST(CameraInputTest, SceneReloadRebindsSurfaceAndKeepsTheSelectedMode) {
    OrbitCamera sun(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 10.0f));
    PlanetSurfaceCamera oldSurface({{5.0, 0.0, 0.0}, 0.5},
                                   {0.0, 180.0, 0.2}, {0.0, 0.0, 0.0}, 60.0);
    PlanetSurfaceCamera newSurface({{6.0, 0.0, 0.0}, 0.5},
                                   {0.0, 180.0, 0.2}, {0.0, 0.0, 0.0}, 60.0);
    CameraInput input(sun, &oldSurface);
    input.selectSurface();
    const glm::dvec3 oldDirection = oldSurface.direction();
    const glm::dvec3 newDirection = newSurface.direction();
    input.rebind(&newSurface, nullptr);
    EXPECT_EQ(input.mode(), CameraMode::Surface);
    input.moveCursor(100.0, 100.0); // Reload resets the captured pointer baseline.
    input.moveCursor(120.0, 100.0);
    EXPECT_EQ(oldSurface.direction(), oldDirection);
    EXPECT_NE(newSurface.direction(), newDirection);
    input.rebind(nullptr, nullptr);
    EXPECT_EQ(input.mode(), CameraMode::Orbit);
}

TEST(CameraInputTest, SceneReloadDropsUnavailablePlanetOrbitMode) {
    OrbitCamera sun(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 10.0f));
    OrbitCamera planet(glm::vec3(5.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 2.0f));
    CameraInput input(sun, nullptr, &planet);
    input.selectPlanetOrbit();
    input.rebind(nullptr, nullptr);
    EXPECT_EQ(input.mode(), CameraMode::Orbit);
    const glm::vec3 before = sun.position;
    input.beginDrag(10.0, 10.0);
    input.moveCursor(30.0, 10.0);
    EXPECT_NE(sun.position, before);
}

TEST(CameraInputTest, ScrollControlsZoomOnlyInOrbitMode) {
    OrbitCamera camera(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 10.0f));
    PlanetSurfaceCamera surface({{5.0, 0.0, 0.0}, 0.5},
                                {0.0, 180.0, 0.2}, {0.0, 0.0, 0.0}, 60.0);
    CameraInput input(camera, &surface);
    input.scroll(1.0);
    input.update({}, 1.0 / 60.0);
    EXPECT_LT(glm::length(camera.position), 10.0f);
    input.selectSurface();
    const float distance = glm::length(camera.position);
    input.scroll(1.0);
    EXPECT_FLOAT_EQ(glm::length(camera.position), distance);
}

TEST(CameraInputTest, PlanetOrbitUsesItsOwnDragAndSoftZoom) {
    OrbitCamera sun(glm::vec3(0.0f), glm::vec3(0, 0, 10));
    OrbitCamera planet(glm::vec3(5, 0, 0), glm::vec3(0, 0, 2),
                       OrbitCamera::Settings{0.6f, 10.0f, 0.96f});
    PlanetSurfaceCamera surface({{5.0, 0.0, 0.0}, 0.5},
                                {0.0, 180.0, 0.2}, {0.0, 0.0, 0.0}, 60.0);
    CameraInput input(sun, &surface, &planet);
    input.selectPlanetOrbit();
    EXPECT_EQ(input.mode(), CameraMode::PlanetOrbit);
    const glm::vec3 sunPosition = sun.position;
    const glm::vec3 planetPosition = planet.position;
    input.beginDrag(100.0, 100.0);
    input.moveCursor(120.0, 80.0);
    EXPECT_NE(planet.position, planetPosition);
    EXPECT_EQ(sun.position, sunPosition);
    EXPECT_NEAR(glm::length(planet.position - planet.target), 2.0f, 1e-5f);
    input.endDrag();
    input.scroll(1.0);
    EXPECT_NEAR(planet.requestedDistance(), 1.92f, 1e-5f);
    EXPECT_NEAR(sun.requestedDistance(), 10.0f, 1e-5f);
    input.update({}, 1.0 / 60.0);
    EXPECT_LT(glm::length(planet.position - planet.target), 2.0f);
    input.selectOrbit();
    EXPECT_EQ(input.mode(), CameraMode::Orbit);
    input.scroll(1.0);
    EXPECT_NEAR(sun.requestedDistance(), 9.6f, 1e-5f);
}

TEST(CameraInputTest, PlanetOrbitAutoEntersSurfaceNearPlanet) {
    OrbitCamera sun(glm::vec3(0.0f), glm::vec3(0, 0, 10));
    OrbitCamera planet(glm::vec3(5, 0, 0), glm::vec3(0, 0, 2),
                       OrbitCamera::Settings{0.6f, 10.0f, 0.96f});
    PlanetSurfaceCamera surface({{5.0, 0.0, 0.0}, 0.5},
                                {0.0, 180.0, 0.2}, {0.0, 0.0, 0.0}, 60.0);
    CameraInput input(sun, &surface, &planet);
    input.selectPlanetOrbit();
    planet.zoom(1000.0f);
    input.update({}, 1.0);
    EXPECT_EQ(input.mode(), CameraMode::Surface);
    EXPECT_TRUE(input.autoActivated());
    EXPECT_NEAR(glm::length(surface.position() - surface.frame().center()),
                glm::length(glm::dvec3(planet.position) - surface.frame().center()), 1e-6);
    EXPECT_GT(glm::dot(surface.direction(), surface.target() - surface.position()), 0.0);
    input.selectPlanetOrbit();
    input.update({}, 0.1);
    EXPECT_EQ(input.mode(), CameraMode::PlanetOrbit); // Explicit mode choice is honored.
}

TEST(CameraInputTest, PlanetOrbitNeedsAConfiguredPlanetCamera) {
    OrbitCamera sun(glm::vec3(0.0f), glm::vec3(0, 0, 10));
    CameraInput input(sun, nullptr);
    input.selectPlanetOrbit();
    EXPECT_EQ(input.mode(), CameraMode::Orbit);
}

TEST(CameraInputTest, ReturningFromSurfaceOrbitsCurrentPlanetLocation) {
    OrbitCamera sun(glm::vec3(0.0f), glm::vec3(0, 0, 10));
    OrbitCamera planet(glm::vec3(5, 0, 0), glm::vec3(0, 0, 2),
                       OrbitCamera::Settings{0.6f, 10.0f, 0.96f});
    PlanetSurfaceCamera surface({{5.0, 0.0, 0.0}, 0.5},
                                {0.0, 180.0, 0.2}, {0.0, 0.0, 0.0}, 60.0);
    CameraInput input(sun, &surface, &planet);
    input.selectSurface();
    surface.walk(1, 0, 0.25);
    const glm::dvec3 surfaceRadial = glm::normalize(
        surface.position() - surface.frame().center());
    input.selectPlanetOrbit();
    EXPECT_EQ(input.mode(), CameraMode::PlanetOrbit);
    EXPECT_GT(glm::dot(glm::normalize(glm::dvec3(planet.position - planet.target)),
                       surfaceRadial), 0.999);
    EXPECT_NEAR(glm::length(planet.position - planet.target), 2.0f, 1e-5f);
}

TEST(CameraInputTest, SurfaceMouseLookNeedsNoDragButton) {
    OrbitCamera camera(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 10.0f));
    PlanetSurfaceCamera surface({{5.0, 0.0, 0.0}, 0.5},
                                {0.0, 180.0, 0.2}, {0.0, 0.0, 0.0}, 60.0);
    surface.setDirectionNed({1.0, 0.0, 0.0});
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

TEST(CameraInputTest, EscapeReleasesSurfaceCursorWithoutChangingViewOrWalking) {
    OrbitCamera camera(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 10.0f));
    PlanetSurfaceCamera surface({{5.0, 0.0, 0.0}, 0.5},
                                {0.0, 180.0, 0.2}, {0.0, 0.0, 0.0}, 60.0);
    CameraInput input(camera, &surface);
    input.selectSurface();
    ASSERT_TRUE(input.surfacePointerCaptured());
    input.moveCursor(100.0, 100.0);
    input.moveCursor(120.0, 100.0);
    const glm::dvec3 direction = surface.direction();
    const glm::dvec3 position = surface.position();

    input.releaseCursor();
    EXPECT_EQ(input.mode(), CameraMode::Surface);
    EXPECT_FALSE(input.surfacePointerCaptured());
    input.moveCursor(500.0, 300.0);
    EXPECT_EQ(surface.direction(), direction);
    input.update(WalkKeys{.forward = true}, 0.25);
    EXPECT_NE(surface.position(), position);
    const glm::dvec3 directionAfterWalk = surface.direction();

    input.selectSurface();
    EXPECT_TRUE(input.surfacePointerCaptured());
    input.moveCursor(500.0, 300.0); // Capture establishes a fresh pointer baseline.
    EXPECT_EQ(surface.direction(), directionAfterWalk);
    input.moveCursor(520.0, 300.0);
    EXPECT_NE(surface.direction(), directionAfterWalk);
}

TEST(CameraInputTest, SurfaceCursorReleaseSurvivesReloadAndEscapeKeepsPlanetOrbit) {
    OrbitCamera sun(glm::vec3(0.0f), glm::vec3(0, 0, 10));
    OrbitCamera planet(glm::vec3(5, 0, 0), glm::vec3(0, 0, 2));
    PlanetSurfaceCamera oldSurface({{5.0, 0.0, 0.0}, 0.5},
                                   {0.0, 180.0, 0.2}, {0.0, 0.0, 0.0}, 60.0);
    PlanetSurfaceCamera newSurface({{6.0, 0.0, 0.0}, 0.5},
                                   {0.0, 180.0, 0.2}, {0.0, 0.0, 0.0}, 60.0);
    CameraInput input(sun, &oldSurface, &planet);
    input.selectSurface();
    input.releaseCursor();
    input.rebind(&newSurface, &planet);
    EXPECT_EQ(input.mode(), CameraMode::Surface);
    EXPECT_FALSE(input.surfacePointerCaptured());
    input.selectPlanetOrbit();
    input.releaseCursor();
    EXPECT_EQ(input.mode(), CameraMode::PlanetOrbit);
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

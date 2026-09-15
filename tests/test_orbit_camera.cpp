#include <gtest/gtest.h>
#include <cmath>
#include <glm/gtc/type_ptr.hpp>
#include "rendering/OrbitCamera.h"

TEST(OrbitCameraTest, StartsAtTheExistingView) {
    OrbitCamera camera(glm::vec3(0.0f), glm::vec3(12.0f, 0.0f, 0.5f));
    EXPECT_FLOAT_EQ(camera.position.x, 12.0f);
    EXPECT_FLOAT_EQ(camera.position.y, 0.0f);
    EXPECT_FLOAT_EQ(camera.position.z, 0.5f);

    const glm::vec4 eyeInView = camera.getViewMatrix() * glm::vec4(camera.position, 1.0f);
    EXPECT_NEAR(eyeInView.x, 0.0f, 1e-5f);
    EXPECT_NEAR(eyeInView.y, 0.0f, 1e-5f);
    EXPECT_NEAR(eyeInView.z, 0.0f, 1e-5f);
}

TEST(OrbitCameraTest, DragOrbitsWithoutChangingDistanceOrReachingAPole) {
    OrbitCamera camera(glm::vec3(0.0f), glm::vec3(15.0f, 2.0f, 8.0f));
    const glm::vec3 initialPosition = camera.position;
    const float initialDistance = glm::length(camera.position - camera.target);

    camera.orbit(120.0f, -40.0f);
    EXPECT_GT(glm::length(camera.position - initialPosition), 0.1f);
    EXPECT_NEAR(glm::length(camera.position - camera.target), initialDistance, 1e-4f);

    camera.orbit(0.0f, -100000.0f);
    const glm::mat4 view = camera.getViewMatrix();
    for (int i = 0; i < 16; ++i) {
        EXPECT_TRUE(std::isfinite(glm::value_ptr(view)[i]));
    }
}

TEST(OrbitCameraTest, DragDirectionsAreInverted) {
    OrbitCamera horizontal(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 10.0f));
    horizontal.orbit(20.0f, 0.0f);
    EXPECT_LT(horizontal.position.x, 0.0f);

    OrbitCamera vertical(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 10.0f));
    vertical.orbit(0.0f, -20.0f);
    EXPECT_LT(vertical.position.y, 0.0f);
}

TEST(OrbitCameraTest, ScrollZoomStaysOutsideTheSunAndWithinViewRange) {
    OrbitCamera camera(glm::vec3(0.0f), glm::vec3(15.0f, 2.0f, 8.0f));

    camera.zoom(1000.0f);
    EXPECT_NEAR(glm::length(camera.position - camera.target), 2.0f, 1e-4f);

    camera.zoom(-1000.0f);
    EXPECT_NEAR(glm::length(camera.position - camera.target), 200.0f, 1e-3f);
}

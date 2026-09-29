#include <gtest/gtest.h>
#include <glm/gtc/matrix_transform.hpp>
#include "rendering/camera/CameraTransition.h"

namespace {
rendering::CameraPose pose(const glm::dvec3& eye, const glm::dvec3& look, float fov = 60.0f) {
    return rendering::CameraPose::fromView(
        eye, glm::mat4(glm::lookAt(eye, look, glm::dvec3(0,1,0))), fov);
}
}

TEST(CameraTransition, StartsAtRenderedOrbitPoseAndFinishesExactlyAfterOneWallSecond) {
    rendering::CameraTransition transition;
    const auto from = pose({0, 0, 10}, {0, 0, 0}, 70);
    const auto target = pose({2, 0, 0}, {1, 1, 0}, 50);
    EXPECT_NEAR(glm::length(from.forward() - glm::dvec3(0,0,-1)), 0, 1e-12);
    EXPECT_NEAR(glm::length(target.forward() - glm::normalize(glm::dvec3(-1,1,0))), 0, 1e-7);
    transition.start(from, 8.0);
    const auto ground = [](const glm::dvec3&) { return 2.0; };
    const auto first = transition.sample(8.0, target, {0,0,0}, ground);
    EXPECT_EQ(first.position, from.position);
    EXPECT_FLOAT_EQ(first.fov, 70);
    const auto middle = transition.sample(8.5, target, {0,0,0}, ground);
    EXPECT_GT(glm::length(middle.position - from.position), 0.1);
    EXPECT_GT(glm::length(middle.position - target.position), 0.1);
    EXPECT_NEAR(glm::length(middle.position), std::sqrt(20.0), 1e-9);
    EXPECT_NEAR(middle.fov, 60, 1e-5);
    EXPECT_GT(glm::dot(middle.forward(), from.forward()), 0.0);
    EXPECT_GT(glm::dot(middle.forward(), target.forward()), 0.0);
    EXPECT_NEAR(glm::length(middle.forward()), 1.0, 1e-9);
    EXPECT_NEAR(glm::dot(middle.forward(), middle.up()), 0.0, 1e-9);
    const auto last = transition.sample(9.0, target, {0,0,0}, ground);
    EXPECT_EQ(last.position, target.position);
    EXPECT_EQ(last.fov, target.fov);
    EXPECT_FALSE(transition.active());
}

TEST(CameraTransition, StaysAboveTerrainOnAntipodalDescent) {
    rendering::CameraTransition transition;
    const auto from = pose({0,0,10}, {0,0,0});
    const auto target = pose({0,0,-2}, {0,1,-1});
    transition.start(from, 0);
    for (int step = 1; step < 10; ++step) {
        const auto frame = transition.sample(step/10.0, target, {0,0,0},
            [](const glm::dvec3&) { return 3.0; });
        EXPECT_GE(glm::length(frame.position), 3.0 - 1e-12);
        EXPECT_TRUE(std::isfinite(frame.position.x));
        EXPECT_TRUE(std::isfinite(frame.position.y));
        EXPECT_TRUE(std::isfinite(frame.position.z));
    }
    EXPECT_EQ(transition.sample(1.0, target, {0,0,0},
        [](const glm::dvec3&) { return 3.0; }).position, target.position);
}

TEST(CameraTransition, FollowsMovingSurfaceTargetAndCanBeCancelled) {
    rendering::CameraTransition transition;
    transition.start(pose({0,0,10}, {0,0,0}), 3.0);
    const auto original = pose({2,0,0}, {0,0,0});
    const auto shifted = pose({2,1,0}, {0,1,0});
    const auto ground = [](const glm::dvec3&) { return 1.0; };
    const auto a = transition.sample(3.5, original, {0,0,0}, ground);
    const auto b = transition.sample(3.5, shifted, {0,0,0}, ground);
    EXPECT_GT(glm::length(a.position-b.position), 0.1);
    transition.cancel();
    EXPECT_EQ(transition.sample(3.6, shifted, {0,0,0}, ground).position, shifted.position);
}

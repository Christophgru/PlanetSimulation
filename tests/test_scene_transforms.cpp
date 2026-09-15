#include <gtest/gtest.h>
#include <glm/glm.hpp>
#include "rendering/OrbitCamera.h"
#include "rendering/SceneTransforms.h"

TEST(SceneTransformsTest, SphereCenterStaysAtItsConfiguredPosition) {
    const glm::mat4 model = rendering::sphereModel(glm::vec3(5.0f, 1.0f, -2.0f), 0.5f);
    const glm::vec4 center = model * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    EXPECT_FLOAT_EQ(center.x, 5.0f);
    EXPECT_FLOAT_EQ(center.y, 1.0f);
    EXPECT_FLOAT_EQ(center.z, -2.0f);
    EXPECT_FLOAT_EQ(center.w, 1.0f);
}

TEST(SceneTransformsTest, SphereRadiusScalesAroundItsOwnCenter) {
    const glm::mat4 model = rendering::sphereModel(glm::vec3(5.0f, 0.0f, 0.0f), 0.5f);
    const glm::vec4 edge = model * glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
    EXPECT_FLOAT_EQ(edge.x, 5.5f);
    EXPECT_FLOAT_EQ(edge.y, 0.0f);
    EXPECT_FLOAT_EQ(edge.z, 0.0f);
}

TEST(SceneTransformsTest, ProjectionMapsNearAndFarPlanesToOpenGLDepthRange) {
    const glm::mat4 projection = rendering::perspectiveProjection(60.0f, 4.0f / 3.0f);
    const glm::vec4 nearClip = projection * glm::vec4(0.0f, 0.0f, -0.1f, 1.0f);
    const glm::vec4 farClip = projection * glm::vec4(0.0f, 0.0f, -1000.0f, 1.0f);
    EXPECT_NEAR(nearClip.z / nearClip.w, -1.0f, 1e-4f);
    EXPECT_NEAR(farClip.z / farClip.w, 1.0f, 1e-4f);
    EXPECT_GT(nearClip.w, 0.0f);
    EXPECT_GT(farClip.w, 0.0f);
}

TEST(SceneTransformsTest, CurrentSunAndPlanetCentersAreVisibleAndSeparate) {
    OrbitCamera camera(glm::vec3(0.0f), glm::vec3(12.0f, 0.0f, 0.5f));
    const glm::mat4 viewProjection =
        rendering::perspectiveProjection(camera.fov, 4.0f / 3.0f) * camera.getViewMatrix();
    const glm::vec4 sunClip = viewProjection * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    const glm::vec4 planetClip = viewProjection * glm::vec4(10.0f, 0.0f, 0.0f, 1.0f);
    const glm::vec3 sunNdc = glm::vec3(sunClip) / sunClip.w;
    const glm::vec3 planetNdc = glm::vec3(planetClip) / planetClip.w;

    EXPECT_GT(sunClip.w, 0.0f);
    EXPECT_GT(planetClip.w, 0.0f);
    EXPECT_LT(glm::abs(sunNdc.x), 1.0f);
    EXPECT_LT(glm::abs(sunNdc.y), 1.0f);
    EXPECT_LT(glm::abs(planetNdc.x), 1.0f);
    EXPECT_LT(glm::abs(planetNdc.y), 1.0f);
    EXPECT_GT(glm::abs(planetNdc.x - sunNdc.x), 0.1f);
}

TEST(SceneTransformsTest, SurfaceProjectionKeepsOneMeterNearAndSunFar) {
    const rendering::ClipPlanes clip = rendering::surfaceClipPlanes(0.002, 10.0, 0.5);
    EXPECT_FLOAT_EQ(clip.nearPlane, 0.001f);
    EXPECT_FLOAT_EQ(clip.farPlane, 21.0f);
    const glm::mat4 projection = rendering::perspectiveProjection(60.0f, 4.0f / 3.0f, clip);
    const glm::vec4 nearClip = projection * glm::vec4(0.0f, 0.0f, -0.001f, 1.0f);
    const glm::vec4 farClip = projection * glm::vec4(0.0f, 0.0f, -21.0f, 1.0f);
    EXPECT_NEAR(nearClip.z / nearClip.w, -1.0f, 1e-3f);
    EXPECT_NEAR(farClip.z / farClip.w, 1.0f, 1e-3f);
    EXPECT_THROW(rendering::surfaceClipPlanes(-0.1, 10.0, 0.5), std::invalid_argument);
}

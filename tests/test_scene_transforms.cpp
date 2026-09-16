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

TEST(SceneTransformsTest, SurfaceProjectionUsesLocalClearanceForNearAndKeepsSunFar) {
    const rendering::ClipPlanes clip = rendering::surfaceClipPlanes(0.002, 10.0, 0.5);
    EXPECT_FLOAT_EQ(clip.nearPlane, 0.0002f);
    EXPECT_FLOAT_EQ(clip.farPlane, 21.0f);
    const glm::mat4 projection = rendering::perspectiveProjection(60.0f, 4.0f / 3.0f, clip);
    const glm::vec4 nearClip = projection * glm::vec4(0.0f, 0.0f, -clip.nearPlane, 1.0f);
    const glm::vec4 farClip = projection * glm::vec4(0.0f, 0.0f, -21.0f, 1.0f);
    EXPECT_NEAR(nearClip.z / nearClip.w, -1.0f, 1e-3f);
    EXPECT_NEAR(farClip.z / farClip.w, 1.0f, 1e-3f);
    EXPECT_THROW(rendering::surfaceClipPlanes(-0.1, 10.0, 0.5), std::invalid_argument);
}

TEST(SceneTransformsTest, PlaneReflectionMirrorsPointsAndVectors) {
    const glm::dvec3 normal(0.0, 0.0, 1.0);
    const glm::dvec3 planePoint(0.0, 0.0, 1.0);
    EXPECT_NEAR(glm::length(rendering::reflectPointAcrossPlane(
                    {2.0, -3.0, 4.0}, planePoint, normal) -
                glm::dvec3(2.0, -3.0, -2.0)), 0.0, 1e-12);
    EXPECT_NEAR(glm::length(rendering::reflectVectorAcrossPlane(
                    {0.0, 2.0, -3.0}, normal) -
                glm::dvec3(0.0, 2.0, 3.0)), 0.0, 1e-12);
}

TEST(SceneTransformsTest, WaterReflectionViewMirrorsEyeAndDirectionAtTangentPlane) {
    const glm::dvec3 eye(0.0, 0.0, 2.0);
    const glm::mat4 view = glm::lookAt(glm::vec3(eye), glm::vec3(0.0f, 1.0f, 1.0f),
                                      glm::vec3(0.0f, 0.0f, 1.0f));
    const glm::mat4 reflected = rendering::waterReflectionView(
        view, eye, glm::dvec3(0.0), 1.0);
    const glm::mat4 inverse = glm::inverse(reflected);
    const glm::dvec3 reflectedEye(inverse[3]);
    const glm::dvec3 reflectedForward = -glm::normalize(glm::dvec3(inverse[2]));
    EXPECT_NEAR(glm::length(reflectedEye - glm::dvec3(0.0)), 0.0, 1e-6);
    EXPECT_GT(reflectedForward.y, 0.7);
    EXPECT_GT(reflectedForward.z, 0.7);
    EXPECT_THROW(rendering::waterReflectionView(view, eye, glm::dvec3(0.0), 0.0),
                 std::invalid_argument);
}

#include <gtest/gtest.h>
#include "math/Matrix4.h"
#include "math/Vector3.h"

TEST(Matrix4Test, PerspectiveMatrix) {
    // Test perspective matrix with standard OpenGL parameters
    Matrix4 proj = Matrix4::perspective(60.0, 1280.0 / 720.0, 0.1, 1000.0);
    
    // Check aspect ratio scaling in X (data[0])
    float fovyRad = 60.0 * M_PI / 180.0;
    float tanHalfFov = std::tan(fovyRad / 2.0f);
    float expectedXScale = (1280.0 / 720.0) / tanHalfFov;
    
    EXPECT_NEAR(proj.data[0], expectedXScale, 1e-5f);
    
    // Check Y scaling (data[5])
    float expectedYScale = 1.0f / tanHalfFov;
    EXPECT_NEAR(proj.data[5], expectedYScale, 1e-5f);
    
    // Check depth terms (data[10], data[11], data[14])
    float expectedZ0 = -(1000.0 + 0.1) / (1000.0 - 0.1);
    EXPECT_NEAR(proj.data[10], expectedZ0, 1e-5f);
    
    float expectedZ1 = -2.0f * 1000.0 * 0.1 / (1000.0 - 0.1);
    EXPECT_NEAR(proj.data[11], expectedZ1, 1e-5f);
    
    // Check bottom-right is 0 (not inherited from identity)
    EXPECT_FLOAT_EQ(proj.data[15], 0.0f);
}

TEST(Matrix4Test, LookAtMatrix) {
    // Test lookAt with camera at (15, 2, 8) looking at origin
    Vector3 eye(15.0, 2.0, 8.0);
    Vector3 target(0.0, 0.0, 0.0);
    Vector3 up(0.0f, 1.0f, 0.0f);
    
    Matrix4 view = Matrix4::lookAt(eye, target, up);
    
    // Forward vector should point from eye to target (normalized)
    double len = std::sqrt(15.0*15.0 + 2.0*2.0 + 8.0*8.0); // sqrt(293) ≈ 17.117
    Vector3 expectedForward(-15.0/len, -2.0/len, -8.0/len);
    EXPECT_NEAR(view.data[8], expectedForward.x, 1e-4f);
    EXPECT_NEAR(view.data[9], expectedForward.y, 1e-4f);
    EXPECT_NEAR(view.data[10], expectedForward.z, 1e-4f);
    
    // Right vector (cross(up, forward))
    double rightX = up.y * expectedForward.z - up.z * expectedForward.y;
    double rightY = up.z * expectedForward.x - up.x * expectedForward.z;
    double rightZ = up.x * expectedForward.y - up.y * expectedForward.x;
    double rightLen = std::sqrt(rightX*rightX + rightY*rightY + rightZ*rightZ);
    Vector3 expectedRight(rightX/rightLen, rightY/rightLen, rightZ/rightLen);
    EXPECT_NEAR(view.data[0], expectedRight.x, 1e-4f);
    EXPECT_NEAR(view.data[4], expectedRight.y, 1e-4f);
    EXPECT_NEAR(view.data[8], expectedRight.z, 1e-4f);
    
    // Corrected up vector (cross(right, forward))
    double upX = rightY * expectedForward.z - rightZ * expectedForward.y;
    double upY = rightZ * expectedForward.x - rightX * expectedForward.z;
    double upZ = rightX * expectedForward.y - rightY * expectedForward.x;
    double upLen = std::sqrt(upX*upX + upY*upY + upZ*upZ);
    Vector3 expectedUp(upX/upLen, upY/upLen, upZ/upLen);
    EXPECT_NEAR(view.data[1], expectedUp.x, 1e-4f);
    EXPECT_NEAR(view.data[5], expectedUp.y, 1e-4f);
    EXPECT_NEAR(view.data[9], expectedUp.z, 1e-4f);
    
    // Translation: -dot(right, eye), -dot(correctedUp, eye), dot(forward, eye)
    float expectedTransX = -(expectedRight.x * eye.x + expectedRight.y * eye.y + expectedRight.z * eye.z);
    EXPECT_NEAR(view.data[12], expectedTransX, 1e-4f);
    
    float expectedTransY = -(expectedUp.x * eye.x + expectedUp.y * eye.y + expectedUp.z * eye.z);
    EXPECT_NEAR(view.data[13], expectedTransY, 1e-4f);
    
    float expectedTransZ = (expectedForward.x * eye.x + expectedForward.y * eye.y + expectedForward.z * eye.z);
    EXPECT_NEAR(view.data[14], expectedTransZ, 1e-4f);
}

TEST(Matrix4Test, ViewSpaceOrigin) {
    // Test that origin ends up in front of camera with negative view-space Z
    Vector3 eye(15.0, 2.0, 8.0);
    Vector3 target(0.0, 0.0, 0.0);
    Vector3 up(0.0f, 1.0f, 0.0f);
    
    Matrix4 view = Matrix4::lookAt(eye, target, up);
    
    // Transform origin (0,0,0) into view space
    float worldPos[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float viewPos[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    
    // gl_Position = projection * view * world
    // For origin: viewPos = view * (0,0,0,1)
    viewPos[0] = view.data[0]*0 + view.data[4]*0 + view.data[8]*0 + view.data[12]*1;
    viewPos[1] = view.data[1]*0 + view.data[5]*0 + view.data[9]*0 + view.data[13]*1;
    viewPos[2] = view.data[2]*0 + view.data[6]*0 + view.data[10]*0 + view.data[14]*1;
    
    // View-space Z should be negative (in front of camera)
    EXPECT_LT(viewPos[2], 0.0f);
    
    // Check that origin is within near/far clip bounds after projection
    Matrix4 proj = Matrix4::perspective(60.0, 1.0, 0.1, 1000.0);
    float clipPos[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    
    // proj * viewPos
    clipPos[0] = proj.data[0]*viewPos[0] + proj.data[1]*viewPos[1] + proj.data[2]*viewPos[2] + proj.data[3]*viewPos[3];
    clipPos[1] = proj.data[4]*viewPos[0] + proj.data[5]*viewPos[1] + proj.data[6]*viewPos[2] + proj.data[7]*viewPos[3];
    clipPos[2] = proj.data[8]*viewPos[0] + proj.data[9]*viewPos[1] + proj.data[10]*viewPos[2] + proj.data[11]*viewPos[3];
    clipPos[3] = proj.data[12]*viewPos[0] + proj.data[13]*viewPos[1] + proj.data[14]*viewPos[2] + proj.data[15]*viewPos[3];
    
    // Divide by W for NDC
    float ndcX = clipPos[0] / clipPos[3];
    float ndcY = clipPos[1] / clipPos[3];
    float ndcZ = clipPos[2] / clipPos[3];
    
    // Should be within [-1, 1] range
    EXPECT_NEAR(ndcX, 0.0f, 1e-5f);
    EXPECT_NEAR(ndcY, 0.0f, 1e-5f);
    EXPECT_NEAR(ndcZ, -1.0f, 1e-3f); // Near plane is at Z=-1 in NDC
}

TEST(Matrix4Test, IdentityTransform) {
    Matrix4 identity;
    
    // Check all diagonal elements are 1
    EXPECT_FLOAT_EQ(identity.data[0], 1.0f);
    EXPECT_FLOAT_EQ(identity.data[5], 1.0f);
    EXPECT_FLOAT_EQ(identity.data[10], 1.0f);
    EXPECT_FLOAT_EQ(identity.data[15], 1.0f);
    
    // Check all off-diagonal elements are 0
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (i != j) {
                EXPECT_FLOAT_EQ(identity.data[i + j*4], 0.0f);
            }
        }
    }
}

TEST(Matrix4Test, MatrixMultiplication) {
    Matrix4 a = Matrix4::translation(1.0, 0.0, 0.0);
    Matrix4 b = Matrix4::rotationZ(M_PI / 4.0f);
    
    Matrix4 result = Matrix4::multiply(a, b);
    
    // Translation * rotation should give rotated translation
    // Check that multiplication is associative-ish (not exactly due to column-major)
    EXPECT_NEAR(result.data[12], -0.70710678f, 1e-5f);
    EXPECT_NEAR(result.data[13], 0.70710678f, 1e-5f);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

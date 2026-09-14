#include <gtest/gtest.h>
#include "math/Matrix4.h"
#include "math/Vector3.h"

// Test-only helper: multiply Matrix4 by a vec4 (w=1 for points)
struct Vec4 {
    double x, y, z, w;
    
    Vec4(double x = 0, double y = 0, double z = 0, double w = 1) : x(x), y(y), z(z), w(w) {}
    
    Vec4 operator*(const Matrix4& m) const {
        return Vec4(
            m.data[0] * x + m.data[1] * y + m.data[2] * z + m.data[3] * w,
            m.data[4] * x + m.data[5] * y + m.data[6] * z + m.data[7] * w,
            m.data[8] * x + m.data[9] * y + m.data[10] * z + m.data[11] * w,
            m.data[12] * x + m.data[13] * y + m.data[14] * z + m.data[15] * w
        );
    }
};

TEST(MatrixTest, IdentityTransformsVec) {
    Matrix4 identity;
    
    Vec4 input(1.0, 2.0, 3.0, 1.0);
    Vec4 result = input * identity;
    
    EXPECT_DOUBLE_EQ(result.x, 1.0);
    EXPECT_DOUBLE_EQ(result.y, 2.0);
    EXPECT_DOUBLE_EQ(result.z, 3.0);
    EXPECT_DOUBLE_EQ(result.w, 1.0);
}

TEST(MatrixTest, TranslationTransformsVec) {
    Matrix4 m = Matrix4::translation(1.0, 2.0, 3.0);
    
    Vec4 origin(0.0, 0.0, 0.0, 1.0);
    Vec4 result = origin * m;
    
    EXPECT_DOUBLE_EQ(result.x, 1.0);
    EXPECT_DOUBLE_EQ(result.y, 2.0);
    EXPECT_DOUBLE_EQ(result.z, 3.0);
    EXPECT_DOUBLE_EQ(result.w, 1.0);
}

TEST(MatrixTest, ScaleTransformsVec) {
    Matrix4 m = Matrix4::scale(2.0f, 3.0f, 4.0f);
    
    Vec4 input(1.0, 1.0, 1.0, 1.0);
    Vec4 result = input * m;
    
    EXPECT_DOUBLE_EQ(result.x, 2.0);
    EXPECT_DOUBLE_EQ(result.y, 3.0);
    EXPECT_DOUBLE_EQ(result.z, 4.0);
    EXPECT_DOUBLE_EQ(result.w, 1.0);
}

TEST(MatrixTest, RotationXTransformsVec) {
    Matrix4 m = Matrix4::rotationX(M_PI / 2.0f);
    
    // +Y (0,1,0,1) should map to +Z (0,0,1,1) after 90° rotation around X
    Vec4 input(0.0, 1.0, 0.0, 1.0);
    Vec4 result = input * m;
    
    EXPECT_NEAR(result.x, 0.0, 1e-6);
    EXPECT_NEAR(result.y, 0.0, 1e-6);
    EXPECT_NEAR(result.z, 1.0, 1e-6);
    EXPECT_DOUBLE_EQ(result.w, 1.0);
}

TEST(MatrixTest, RotationYTransformsVec) {
    Matrix4 m = Matrix4::rotationY(M_PI / 2.0f);
    
    // +Z (0,0,1,1) should map to +X (1,0,0,1) after 90° rotation around Y
    Vec4 input(0.0, 0.0, 1.0, 1.0);
    Vec4 result = input * m;
    
    EXPECT_NEAR(result.x, 1.0, 1e-6);
    EXPECT_DOUBLE_EQ(result.y, 0.0);
    EXPECT_NEAR(result.z, 0.0, 1e-6);
    EXPECT_DOUBLE_EQ(result.w, 1.0);
}

TEST(MatrixTest, RotationZTransformsVec) {
    Matrix4 m = Matrix4::rotationZ(M_PI / 2.0f);
    
    // +X (1,0,0,1) should map to +Y (0,1,0,1) after 90° rotation around Z
    Vec4 input(1.0, 0.0, 0.0, 1.0);
    Vec4 result = input * m;
    
    EXPECT_NEAR(result.x, 0.0, 1e-6);
    EXPECT_NEAR(result.y, 1.0, 1e-6);
    EXPECT_DOUBLE_EQ(result.z, 0.0);
    EXPECT_DOUBLE_EQ(result.w, 1.0);
}

TEST(MatrixTest, CompositionTranslationScale) {
    Matrix4 t = Matrix4::translation(5.0, 0.0, 0.0);
    Matrix4 s = Matrix4::scale(2.0f, 1.0f, 1.0f);
    
    // Apply scale then translation: (1,0,0,1) -> (2,0,0,1) -> (7,0,0,1)
    Vec4 input(1.0, 0.0, 0.0, 1.0);
    Matrix4 combined = Matrix4::multiply(t, s); // t * s means apply s first, then t
    Vec4 result = input * combined;
    
    EXPECT_NEAR(result.x, 7.0, 1e-6);
    EXPECT_DOUBLE_EQ(result.y, 0.0);
    EXPECT_DOUBLE_EQ(result.z, 0.0);
    EXPECT_DOUBLE_EQ(result.w, 1.0);
}

TEST(MatrixTest, LookAtTransformsEyeTarget) {
    Vector3 eye(0.0, 0.0, 5.0);
    Vector3 target(0.0, 0.0, 0.0);
    Vector3 up(0.0, 1.0, 0.0);
    
    Matrix4 m = Matrix4::lookAt(eye, target, up);
    
    // Eye should map to origin in view space
    Vec4 eyeVec(0.0, 0.0, 5.0, 1.0);
    Vec4 result = eyeVec * m;
    
    EXPECT_NEAR(result.x, 0.0, 1e-6);
    EXPECT_NEAR(result.y, 0.0, 1e-6);
    EXPECT_NEAR(result.z, 0.0, 1e-6);
    EXPECT_DOUBLE_EQ(result.w, 1.0);
    
    // Target should map to negative view-space Z (behind camera)
    Vec4 targetVec(0.0, 0.0, 0.0, 1.0);
    Vec4 targetResult = targetVec * m;
    
    EXPECT_NEAR(targetResult.x, 0.0, 1e-6);
    EXPECT_NEAR(targetResult.y, 0.0, 1e-6);
    EXPECT_LT(targetResult.z, 0.0);
    EXPECT_DOUBLE_EQ(targetResult.w, 1.0);
}

TEST(MatrixTest, PerspectiveTransformsPoint) {
    Matrix4 m = Matrix4::perspective(90.0f, 1.0f, 1.0f, 10.0f);
    
    // Point at (0,0,-5,1) should map to positive clip W after perspective divide
    Vec4 input(0.0, 0.0, -5.0, 1.0);
    Vec4 result = input * m;
    
    // After perspective divide: x_ndc = x_clip / w_clip, y_ndc = y_clip / w_clip
    EXPECT_GT(result.w, 0.0);
    
    double ndcX = result.x / result.w;
    double ndcY = result.y / result.w;
    double ndcZ = result.z / result.w;
    
    // NDC x/y should be near zero (point is on center line)
    EXPECT_NEAR(ndcX, 0.0, 1e-6);
    EXPECT_NEAR(ndcY, 0.0, 1e-6);
    
    // NDC z should be inside [-1, 1] for visible point
    EXPECT_GE(ndcZ, -1.0);
    EXPECT_LE(ndcZ, 1.0);
}

TEST(MatrixTest, FullProjectionViewTransform) {
    Vector3 eye(15.0, 2.0, 8.0);
    Vector3 target(0.0, 0.0, 0.0);
    Vector3 up(0.0, 1.0, 0.0);
    
    Matrix4 view = Matrix4::lookAt(eye, target, up);
    Matrix4 proj = Matrix4::perspective(60.0f, 16.0f / 9.0f, 0.1f, 100.0f);
    
    Matrix4 combined = Matrix4::multiply(proj, view);
    
    // Origin in world space should be inside clip volume after transform
    Vec4 origin(0.0, 0.0, 0.0, 1.0);
    Vec4 result = origin * combined;
    
    double ndcX = result.x / result.w;
    double ndcY = result.y / result.w;
    double ndcZ = result.z / result.w;
    
    // Should be inside clip volume [-1, 1] for all axes
    EXPECT_GE(ndcX, -1.0);
    EXPECT_LE(ndcX, 1.0);
    EXPECT_GE(ndcY, -1.0);
    EXPECT_LE(ndcY, 1.0);
    EXPECT_GE(ndcZ, -1.0);
    EXPECT_LE(ndcZ, 1.0);
}

// Additional behavioral tests for edge cases and completeness

TEST(MatrixTest, IdentityMatrixValues) {
    Matrix4 identity;
    
    // Check all diagonal elements are 1.0, off-diagonal are 0.0
    EXPECT_DOUBLE_EQ(identity.data[0], 1.0f);
    EXPECT_DOUBLE_EQ(identity.data[5], 1.0f);
    EXPECT_DOUBLE_EQ(identity.data[10], 1.0f);
    EXPECT_DOUBLE_EQ(identity.data[15], 1.0f);
    
    for (int i = 0; i < 16; i++) {
        if (i % 4 == 0) continue; // Skip diagonal
        EXPECT_DOUBLE_EQ(identity.data[i], 0.0f);
    }
}

TEST(MatrixTest, TranslationNegativeValues) {
    Matrix4 m = Matrix4::translation(-1.0, -2.0, -3.0);
    
    Vec4 origin(0.0, 0.0, 0.0, 1.0);
    Vec4 result = origin * m;
    
    EXPECT_DOUBLE_EQ(result.x, -1.0);
    EXPECT_DOUBLE_EQ(result.y, -2.0);
    EXPECT_DOUBLE_EQ(result.z, -3.0);
    EXPECT_DOUBLE_EQ(result.w, 1.0);
}

TEST(MatrixTest, ScaleNegativeValues) {
    Matrix4 m = Matrix4::scale(-2.0f, -3.0f, -4.0f);
    
    Vec4 input(1.0, 1.0, 1.0, 1.0);
    Vec4 result = input * m;
    
    EXPECT_DOUBLE_EQ(result.x, -2.0);
    EXPECT_DOUBLE_EQ(result.y, -3.0);
    EXPECT_DOUBLE_EQ(result.z, -4.0);
    EXPECT_DOUBLE_EQ(result.w, 1.0);
}

TEST(MatrixTest, RotationXNegativeAngle) {
    Matrix4 m = Matrix4::rotationX(-M_PI / 2.0f);
    
    // +Y (0,1,0,1) should map to -Z (0,0,-1,1) after -90° rotation around X
    Vec4 input(0.0, 1.0, 0.0, 1.0);
    Vec4 result = input * m;
    
    EXPECT_NEAR(result.x, 0.0, 1e-6);
    EXPECT_NEAR(result.y, 0.0, 1e-6);
    EXPECT_NEAR(result.z, -1.0, 1e-6);
    EXPECT_DOUBLE_EQ(result.w, 1.0);
}

TEST(MatrixTest, RotationYNegativeAngle) {
    Matrix4 m = Matrix4::rotationY(-M_PI / 2.0f);
    
    // +Z (0,0,1,1) should map to -X (-1,0,0,1) after -90° rotation around Y
    Vec4 input(0.0, 0.0, 1.0, 1.0);
    Vec4 result = input * m;
    
    EXPECT_NEAR(result.x, -1.0, 1e-6);
    EXPECT_DOUBLE_EQ(result.y, 0.0);
    EXPECT_NEAR(result.z, 0.0, 1e-6);
    EXPECT_DOUBLE_EQ(result.w, 1.0);
}

TEST(MatrixTest, RotationZNegativeAngle) {
    Matrix4 m = Matrix4::rotationZ(-M_PI / 2.0f);
    
    // +X (1,0,0,1) should map to -Y (0,-1,0,1) after -90° rotation around Z
    Vec4 input(1.0, 0.0, 0.0, 1.0);
    Vec4 result = input * m;
    
    EXPECT_NEAR(result.x, 0.0, 1e-6);
    EXPECT_NEAR(result.y, -1.0, 1e-6);
    EXPECT_DOUBLE_EQ(result.z, 0.0);
    EXPECT_DOUBLE_EQ(result.w, 1.0);
}

TEST(MatrixTest, CompositionScaleThenTranslate) {
    Matrix4 t = Matrix4::translation(5.0, 0.0, 0.0);
    Matrix4 s = Matrix4::scale(2.0f, 1.0f, 1.0f);
    
    // Apply scale then translation: (1,0,0,1) -> (2,0,0,1) -> (7,0,0,1)
    Vec4 input(1.0, 0.0, 0.0, 1.0);
    Matrix4 combined = Matrix4::multiply(t, s); // t * s means apply s first, then t
    Vec4 result = input * combined;
    
    EXPECT_NEAR(result.x, 7.0, 1e-6);
    EXPECT_DOUBLE_EQ(result.y, 0.0);
    EXPECT_DOUBLE_EQ(result.z, 0.0);
    EXPECT_DOUBLE_EQ(result.w, 1.0);
}

TEST(MatrixTest, CompositionTranslateThenScale) {
    Matrix4 t = Matrix4::translation(5.0, 0.0, 0.0);
    Matrix4 s = Matrix4::scale(2.0f, 1.0f, 1.0f);
    
    // Apply translation then scale: (1,0,0,1) -> (6,0,0,1) -> (12,0,0,1)
    Vec4 input(1.0, 0.0, 0.0, 1.0);
    Matrix4 combined = Matrix4::multiply(s, t); // s * t means apply t first, then s
    Vec4 result = input * combined;
    
    EXPECT_NEAR(result.x, 12.0, 1e-6);
    EXPECT_DOUBLE_EQ(result.y, 0.0);
    EXPECT_DOUBLE_EQ(result.z, 0.0);
    EXPECT_DOUBLE_EQ(result.w, 1.0);
}

TEST(MatrixTest, LookAtEyeMapsToOrigin) {
    Vector3 eye(10.0, 5.0, 20.0);
    Vector3 target(0.0, 0.0, 0.0);
    Vector3 up(0.0, 1.0, 0.0);
    
    Matrix4 m = Matrix4::lookAt(eye, target, up);
    
    // Eye should map to origin in view space
    Vec4 eyeVec(10.0, 5.0, 20.0, 1.0);
    Vec4 result = eyeVec * m;
    
    EXPECT_NEAR(result.x, 0.0, 1e-6);
    EXPECT_NEAR(result.y, 0.0, 1e-6);
    EXPECT_NEAR(result.z, 0.0, 1e-6);
    EXPECT_DOUBLE_EQ(result.w, 1.0);
}

TEST(MatrixTest, LookAtTargetMapsToNegativeZ) {
    Vector3 eye(0.0, 0.0, 5.0);
    Vector3 target(0.0, 0.0, 0.0);
    Vector3 up(0.0, 1.0, 0.0);
    
    Matrix4 m = Matrix4::lookAt(eye, target, up);
    
    // Target should map to negative view-space Z (behind camera)
    Vec4 targetVec(0.0, 0.0, 0.0, 1.0);
    Vec4 targetResult = targetVec * m;
    
    EXPECT_NEAR(targetResult.x, 0.0, 1e-6);
    EXPECT_NEAR(targetResult.y, 0.0, 1e-6);
    EXPECT_LT(targetResult.z, 0.0);
    EXPECT_DOUBLE_EQ(targetResult.w, 1.0);
}

TEST(MatrixTest, PerspectiveNDCMapping) {
    Matrix4 m = Matrix4::perspective(90.0f, 1.0f, 1.0f, 10.0f);
    
    // Point at (2,0,-5,1) should map to positive NDC X after perspective divide
    Vec4 input(2.0, 0.0, -5.0, 1.0);
    Vec4 result = input * m;
    
    EXPECT_GT(result.w, 0.0);
    
    double ndcX = result.x / result.w;
    double ndcY = result.y / result.w;
    double ndcZ = result.z / result.w;
    
    // NDC x/y should be within [-1, 1] for visible point
    EXPECT_GE(ndcX, -1.0);
    EXPECT_LE(ndcX, 1.0);
    EXPECT_GE(ndcY, -1.0);
    EXPECT_LE(ndcY, 1.0);
    
    // NDC z should be inside [-1, 1] for visible point
    EXPECT_GE(ndcZ, -1.0);
    EXPECT_LE(ndcZ, 1.0);
}

TEST(MatrixTest, CameraAt15_2_8LookingAtOrigin) {
    Vector3 eye(15.0, 2.0, 8.0);
    Vector3 target(0.0, 0.0, 0.0);
    Vector3 up(0.0, 1.0, 0.0);
    
    Matrix4 view = Matrix4::lookAt(eye, target, up);
    Matrix4 proj = Matrix4::perspective(60.0f, 16.0f / 9.0f, 0.1f, 100.0f);
    
    Matrix4 combined = Matrix4::multiply(proj, view);
    
    // Origin in world space should be inside clip volume after transform
    Vec4 origin(0.0, 0.0, 0.0, 1.0);
    Vec4 result = origin * combined;
    
    double ndcX = result.x / result.w;
    double ndcY = result.y / result.w;
    double ndcZ = result.z / result.w;
    
    // Should be inside clip volume [-1, 1] for all axes
    EXPECT_GE(ndcX, -1.0);
    EXPECT_LE(ndcX, 1.0);
    EXPECT_GE(ndcY, -1.0);
    EXPECT_LE(ndcY, 1.0);
    EXPECT_GE(ndcZ, -1.0);
    EXPECT_LE(ndcZ, 1.0);
}

TEST(MatrixTest, CameraAt15_2_8LookingAtOriginEyeMapsToOrigin) {
    Vector3 eye(15.0, 2.0, 8.0);
    Vector3 target(0.0, 0.0, 0.0);
    Vector3 up(0.0, 1.0, 0.0);
    
    Matrix4 view = Matrix4::lookAt(eye, target, up);
    Matrix4 proj = Matrix4::perspective(60.0f, 16.0f / 9.0f, 0.1f, 100.0f);
    
    Matrix4 combined = Matrix4::multiply(proj, view);
    
    // Eye position in world space should map to origin in clip space
    Vec4 eyeVec(15.0, 2.0, 8.0, 1.0);
    Vec4 result = eyeVec * combined;
    
    double ndcX = result.x / result.w;
    double ndcY = result.y / result.w;
    double ndcZ = result.z / result.w;
    
    // Eye should be at origin in view space (0,0,0) after perspective divide
    EXPECT_NEAR(ndcX, 0.0, 1e-6);
    EXPECT_NEAR(ndcY, 0.0, 1e-6);
    EXPECT_NEAR(ndcZ, 0.0, 1e-6);
}

TEST(MatrixTest, PerspectiveNearFarPlane) {
    Matrix4 m = Matrix4::perspective(90.0f, 1.0f, 1.0f, 10.0f);
    
    // Point at (0,0,-2,1) should be inside near plane
    Vec4 inputNear(0.0, 0.0, -2.0, 1.0);
    Vec4 resultNear = inputNear * m;
    
    EXPECT_GT(resultNear.w, 0.0);
    
    double ndcZNear = resultNear.z / resultNear.w;
    EXPECT_GE(ndcZNear, -1.0);
    EXPECT_LE(ndcZNear, 1.0);
    
    // Point at (0,0,-8,1) should be inside far plane
    Vec4 inputFar(0.0, 0.0, -8.0, 1.0);
    Vec4 resultFar = inputFar * m;
    
    EXPECT_GT(resultFar.w, 0.0);
    
    double ndcZFar = resultFar.z / resultFar.w;
    EXPECT_GE(ndcZFar, -1.0);
    EXPECT_LE(ndcZFar, 1.0);
}

TEST(MatrixTest, PerspectivePointAtInfinity) {
    Matrix4 m = Matrix4::perspective(90.0f, 1.0f, 1.0f, 10.0f);
    
    // Point at (0,0,-1000,1) should still be valid
    Vec4 input(0.0, 0.0, -1000.0, 1.0);
    Vec4 result = input * m;
    
    EXPECT_GT(result.w, 0.0);
    
    double ndcZ = result.z / result.w;
    // Should be inside clip volume
    EXPECT_GE(ndcZ, -1.0);
    EXPECT_LE(ndcZ, 1.0);
}

TEST(MatrixTest, ScaleUniform) {
    Matrix4 m = Matrix4::scale(2.0f, 2.0f, 2.0f);
    
    Vec4 input(1.0, 1.0, 1.0, 1.0);
    Vec4 result = input * m;
    
    EXPECT_DOUBLE_EQ(result.x, 2.0);
    EXPECT_DOUBLE_EQ(result.y, 2.0);
    EXPECT_DOUBLE_EQ(result.z, 2.0);
    EXPECT_DOUBLE_EQ(result.w, 1.0);
}

TEST(MatrixTest, ScaleNonUniform) {
    Matrix4 m = Matrix4::scale(2.0f, 3.0f, 4.0f);
    
    Vec4 input(1.0, 1.0, 1.0, 1.0);
    Vec4 result = input * m;
    
    EXPECT_DOUBLE_EQ(result.x, 2.0);
    EXPECT_DOUBLE_EQ(result.y, 3.0);
    EXPECT_DOUBLE_EQ(result.z, 4.0);
    EXPECT_DOUBLE_EQ(result.w, 1.0);
}

TEST(MatrixTest, RotationXPreservesYZ) {
    Matrix4 m = Matrix4::rotationX(M_PI / 2.0f);
    
    // Point on X axis should remain unchanged
    Vec4 input(5.0, 0.0, 0.0, 1.0);
    Vec4 result = input * m;
    
    EXPECT_NEAR(result.x, 5.0, 1e-6);
    EXPECT_DOUBLE_EQ(result.y, 0.0);
    EXPECT_DOUBLE_EQ(result.z, 0.0);
}

TEST(MatrixTest, RotationYPreservesXZ) {
    Matrix4 m = Matrix4::rotationY(M_PI / 2.0f);
    
    // Point on Y axis should remain unchanged
    Vec4 input(0.0, 5.0, 0.0, 1.0);
    Vec4 result = input * m;
    
    EXPECT_DOUBLE_EQ(result.x, 0.0);
    EXPECT_NEAR(result.y, 5.0, 1e-6);
    EXPECT_DOUBLE_EQ(result.z, 0.0);
}

TEST(MatrixTest, RotationZPreservesXY) {
    Matrix4 m = Matrix4::rotationZ(M_PI / 2.0f);
    
    // Point on Z axis should remain unchanged
    Vec4 input(0.0, 0.0, 5.0, 1.0);
    Vec4 result = input * m;
    
    EXPECT_DOUBLE_EQ(result.x, 0.0);
    EXPECT_DOUBLE_EQ(result.y, 0.0);
    EXPECT_NEAR(result.z, 5.0, 1e-6);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

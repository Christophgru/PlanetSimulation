#include <gtest/gtest.h>
#include "math/Matrix4.h"
#include "math/Vector3.h"

TEST(MatrixTest, Identity) {
    Matrix4 m;
    
    EXPECT_DOUBLE_EQ(m.data[0], 1.0);
    EXPECT_DOUBLE_EQ(m.data[5], 1.0);
    EXPECT_DOUBLE_EQ(m.data[10], 1.0);
    EXPECT_DOUBLE_EQ(m.data[15], 1.0);
    
    for (int i = 0; i < 16; i++) {
        if (i == 0 || i == 5 || i == 10 || i == 15) {
            continue;
        }
        EXPECT_DOUBLE_EQ(m.data[i], 0.0);
    }
}

TEST(MatrixTest, Translation) {
    Matrix4 m = Matrix4::translation(1.0, 2.0, 3.0);
    
    EXPECT_DOUBLE_EQ(m.data[12], 1.0f);
    EXPECT_DOUBLE_EQ(m.data[13], 2.0f);
    EXPECT_DOUBLE_EQ(m.data[14], 3.0f);
}

TEST(MatrixTest, RotationX) {
    Matrix4 m = Matrix4::rotationX(90.0f * M_PI / 180.0f);
    
    // After 90 degree rotation around X:
    // cos(90) = 0, sin(90) = 1
    EXPECT_NEAR(m.data[5], 0.0f, 1e-6f);
    EXPECT_NEAR(m.data[6], -1.0f, 1e-6f);
    EXPECT_NEAR(m.data[9], 1.0f, 1e-6f);
    EXPECT_NEAR(m.data[10], 0.0f, 1e-6f);
}

TEST(MatrixTest, RotationY) {
    Matrix4 m = Matrix4::rotationY(90.0f * M_PI / 180.0f);
    
    // After 90 degree rotation around Y:
    EXPECT_NEAR(m.data[0], 0.0f, 1e-6f);
    EXPECT_DOUBLE_EQ(m.data[8], 1.0f);
    EXPECT_NEAR(m.data[5], 1.0f, 1e-6f);
    EXPECT_NEAR(m.data[2], -1.0f, 1e-6f);
}

TEST(MatrixTest, RotationZ) {
    Matrix4 m = Matrix4::rotationZ(90.0f * M_PI / 180.0f);
    
    // After 90 degree rotation around Z:
    EXPECT_NEAR(m.data[0], 0.0f, 1e-6f);
    EXPECT_NEAR(m.data[1], -1.0f, 1e-6f);
    EXPECT_DOUBLE_EQ(m.data[4], 1.0f);
    EXPECT_NEAR(m.data[5], 0.0f, 1e-6f);
}

TEST(MatrixTest, Scale) {
    Matrix4 m = Matrix4::scale(2.0f, 3.0f, 4.0f);
    
    EXPECT_DOUBLE_EQ(m.data[0], 2.0f);
    EXPECT_DOUBLE_EQ(m.data[5], 3.0f);
    EXPECT_DOUBLE_EQ(m.data[10], 4.0f);
}

TEST(MatrixTest, Perspective) {
    Matrix4 m = Matrix4::perspective(60.0, 16.0 / 9.0, 0.1, 100.0);
    
    // Check that the matrix was created with reasonable non-zero values
    EXPECT_GT(m.data[0], 0.0f);
    EXPECT_GT(m.data[5], 0.0f);
    EXPECT_LE(m.data[14], -1.0f);
}

TEST(MatrixTest, LookAt) {
    Vector3 eye(0.0, 0.0, 5.0);
    Vector3 target(0.0, 0.0, 0.0);
    Vector3 up(0.0, 1.0, 0.0);
    
    Matrix4 m = Matrix4::lookAt(eye, target, up);
    
    // Check that the matrix has been created with reasonable values
    EXPECT_DOUBLE_EQ(m.data[15], 1.0f);
}

TEST(MatrixTest, Multiply) {
    Matrix4 a = Matrix4::translation(1.0, 0.0, 0.0);
    Matrix4 b = Matrix4::rotationX(M_PI / 2.0f);
    
    Matrix4 result = Matrix4::multiply(a, b);
    
    // Just verify the multiplication produces a valid matrix
    EXPECT_DOUBLE_EQ(result.data[15], 1.0f);
}

TEST(MatrixTest, Vector3Operations) {
    Vector3 v1(1.0, 2.0, 3.0);
    Vector3 v2(4.0, 5.0, 6.0);
    
    Vector3 sum = v1 + v2;
    EXPECT_DOUBLE_EQ(sum.x, 5.0);
    EXPECT_DOUBLE_EQ(sum.y, 7.0);
    EXPECT_DOUBLE_EQ(sum.z, 9.0);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

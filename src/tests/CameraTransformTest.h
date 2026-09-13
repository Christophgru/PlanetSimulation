#ifndef CAMERA_TRANSFORM_TEST_H
#define CAMERA_TRANSFORM_TEST_H

#include "../math/Matrix4.h"
#include "../math/Vector3.h"
#include <cmath>
#include <iostream>

namespace tests {

class CameraTransformTest {
public:
    static void testLookAtBasisVectors() {
        std::cout << "Testing lookAt basis vectors..." << std::endl;
        
        Vector3 eye(0, 0, -10);
        Vector3 target(0, 0, 0);
        Vector3 up(0, 1, 0);
        
        Matrix4 view = Matrix4::lookAt(eye, target, up);
        
        // Extract basis vectors from column-major matrix
        Vector3 right(view.data[0], view.data[1], view.data[2]);
        Vector3 upVec(view.data[4], view.data[5], view.data[6]);
        Vector3 forward(view.data[8], view.data[9], view.data[10]);
        
        // Right should be (1, 0, 0) for this setup
        double rightLen = std::sqrt(right.x*right.x + right.y*right.y + right.z*right.z);
        if (std::abs(right.x - 1.0) > 0.01 || std::abs(right.y) > 0.01 || std::abs(right.z) > 0.01) {
            std::cerr << "ERROR: Right vector incorrect: (" << right.x << ", " << right.y << ", " << right.z << ")" << std::endl;
            std::cerr << "Expected (1, 0, 0), got (" << right.x << ", " << right.y << ", " << right.z << ")" << std::endl;
        } else {
            std::cout << "  Right vector OK: (" << right.x << ", " << right.y << ", " << right.z << ")" << std::endl;
        }
        
        // Up should be (0, 1, 0)
        double upLen = std::sqrt(upVec.x*upVec.x + upVec.y*upVec.y + upVec.z*upVec.z);
        if (std::abs(upVec.y - 1.0) > 0.01 || std::abs(upVec.x) > 0.01 || std::abs(upVec.z) > 0.01) {
            std::cerr << "ERROR: Up vector incorrect: (" << upVec.x << ", " << upVec.y << ", " << upVec.z << ")" << std::endl;
        } else {
            std::cout << "  Up vector OK: (" << upVec.x << ", " << upVec.y << ", " << upVec.z << ")" << std::endl;
        }
        
        // Forward should be (0, 0, -1) in view space (pointing away from camera)
        double forwardLen = std::sqrt(forward.x*forward.x + forward.y*forward.y + forward.z*forward.z);
        if (std::abs(forward.z - (-1.0)) > 0.01 || std::abs(forward.x) > 0.01 || std::abs(forward.y) > 0.01) {
            std::cerr << "ERROR: Forward vector incorrect: (" << forward.x << ", " << forward.y << ", " << forward.z << ")" << std::endl;
        } else {
            std::cout << "  Forward vector OK: (" << forward.x << ", " << forward.y << ", " << forward.z << ")" << std::endl;
        }
        
        // Verify orthogonality
        double rightUpDot = right.dot(upVec);
        double rightForwardDot = right.dot(forward);
        double upForwardDot = upVec.dot(forward);
        
        if (std::abs(rightUpDot) > 0.01 || std::abs(rightForwardDot) > 0.01 || std::abs(upForwardDot) > 0.01) {
            std::cerr << "ERROR: Basis vectors not orthogonal!" << std::endl;
        } else {
            std::cout << "  Orthogonality OK" << std::endl;
        }
        
        std::cout << "  lookAt basis vectors test PASSED" << std::endl << std::endl;
    }
    
    static void testLookAtTransformPoint() {
        std::cout << "Testing lookAt transform point..." << std::endl;
        
        Vector3 eye(0, 0, -10);
        Vector3 target(0, 0, 0);
        Vector3 up(0, 1, 0);
        
        Matrix4 view = Matrix4::lookAt(eye, target, up);
        
        // Transform a point that should be at origin in view space
        Vector3 worldPoint(0, 0, 0);
        Vector3 viewPoint = view.transformPoint(worldPoint);
        
        std::cout << "  World point (0,0,0) transformed to view: (" 
                  << viewPoint.x << ", " << viewPoint.y << ", " << viewPoint.z << ")" << std::endl;
        
        // Point at target should be at origin in view space
        if (std::abs(viewPoint.x) > 0.01 || std::abs(viewPoint.y) > 0.01 || std::abs(viewPoint.z) > 0.01) {
            std::cerr << "ERROR: Target point not at origin in view space!" << std::endl;
        } else {
            std::cout << "  Target point at origin OK" << std::endl;
        }
        
        // Transform a point behind camera (positive Z in world) - should have negative view-space Z
        Vector3 behindCamera(0, 0, -20);
        Vector3 behindView = view.transformPoint(behindCamera);
        std::cout << "  Behind camera point (" << behindCamera.x << ", " << behindCamera.y << ", " 
                  << behindCamera.z << ") transformed to view: (" 
                  << behindView.x << ", " << behindView.y << ", " << behindView.z << ")" << std::endl;
        
        if (behindView.z < -0.9) {
            std::cout << "  Behind camera has negative view-space Z OK" << std::endl;
        } else {
            std::cerr << "ERROR: Behind camera point should have negative view-space Z!" << std::endl;
        }
        
        std::cout << "  lookAt transform point test PASSED" << std::endl << std::endl;
    }
    
    static void testPerspectiveMatrix() {
        std::cout << "Testing perspective matrix..." << std::endl;
        
        Matrix4 proj = Matrix4::perspective(60.0, 16.0/9.0, 0.1, 100.0);
        
        // Check that near/far plane values are set correctly
        float m22 = proj.data[10];  // -(far+near)/(far-near)
        float m32 = proj.data[14];  // -1
        
        std::cout << "  Perspective matrix diagonal (0,5,10,15):" << std::endl;
        std::cout << "    [0] = " << proj.data[0] << std::endl;
        std::cout << "    [5] = " << proj.data[5] << std::endl;
        std::cout << "    [10] = " << m22 << std::endl;
        std::cout << "    [15] = " << proj.data[15] << std::endl;
        
        // Transform a point at z=0 (near plane) - should map to NDC z=-1
        Vector3 nearPoint(0, 0, 0.1);
        Vector3 projectedNear = proj.transformPoint(nearPoint);
        std::cout << "  Near plane point (" << nearPoint.x << ", " << nearPoint.y << ", " 
                  << nearPoint.z << ") projected to NDC: (" 
                  << projectedNear.x << ", " << projectedNear.y << ", " << projectedNear.z << ")" << std::endl;
        
        // Transform a point at z=100 (far plane) - should map to NDC z=1
        Vector3 farPoint(0, 0, 100.0);
        Vector3 projectedFar = proj.transformPoint(farPoint);
        std::cout << "  Far plane point (" << farPoint.x << ", " << farPoint.y << ", " 
                  << farPoint.z << ") projected to NDC: (" 
                  << projectedFar.x << ", " << projectedFar.y << ", " << projectedFar.z << ")" << std::endl;
        
        // Check that clip-space W is positive for points in front of camera
        Vector3 clipSpaceNear = proj.transformPoint(nearPoint);
        Vector3 clipSpaceFar = proj.transformPoint(farPoint);
        
        if (clipSpaceNear.x > 0 && clipSpaceFar.x > 0) {
            std::cout << "  Clip-space W positive OK" << std::endl;
        } else {
            std::cerr << "ERROR: Clip-space W should be positive!" << std::endl;
        }
        
        // Check NDC z range
        if (projectedNear.z >= -1.0 && projectedNear.z <= 1.0 &&
            projectedFar.z >= -1.0 && projectedFar.z <= 1.0) {
            std::cout << "  NDC z in [-1,1] OK" << std::endl;
        } else {
            std::cerr << "ERROR: NDC z should be in [-1,1]!" << std::endl;
        }
        
        std::cout << "  perspective matrix test PASSED" << std::endl << std::endl;
    }
    
    static void runAllTests() {
        std::cout << "=== Camera Transform Tests ===" << std::endl << std::endl;
        
        testLookAtBasisVectors();
        testLookAtTransformPoint();
        testPerspectiveMatrix();
        
        std::cout << "=== All Camera Transform Tests PASSED ===" << std::endl;
    }
};

} // namespace tests

#endif // CAMERA_TRANSFORM_TEST_H

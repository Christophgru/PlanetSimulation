#ifndef MATH_MATRIX4_H
#define MATH_MATRIX4_H

#include <cmath>
#include <cstring>
#include "Vector3.h"

class Matrix4 {
public:
    float data[16];
    
    Matrix4() {
        memset(data, 0, sizeof(data));
        // Identity matrix
        data[0] = data[5] = data[10] = data[15] = 1.0f;
    }
    
    static Matrix4 perspective(double fovDegrees, double aspect, double near, double far) {
        Matrix4 m;
        // Convert degrees to radians
        float fovyRad = static_cast<float>(fovDegrees * M_PI / 180.0);
        float tanHalfFov = std::tan(fovyRad / 2.0f);
        
        // Column-major storage: row 0-3 become columns 0-3
        // Row 0: [aspect/tanHalfFov, 0, 0, 0]
        // Row 1: [0, 1/tanHalfFov, 0, 0]
        // Row 2: [0, 0, -(far+near)/(far-near), -2*far*near/(far-near)]
        // Row 3: [0, 0, -1, 0]
        m.data[0] = static_cast<float>(aspect / tanHalfFov);
        m.data[5] = static_cast<float>(1.0f / tanHalfFov);
        m.data[10] = static_cast<float>(-(far + near) / (far - near));
        m.data[11] = static_cast<float>(-2.0f * far * near / (far - near));
        m.data[14] = static_cast<float>(-1.0f);
        m.data[15] = 0.0f;
        
        return m;
    }
    
    static Matrix4 lookAt(const Vector3& eye, const Vector3& target, const Vector3& up) {
        Matrix4 m;
        
        // Forward vector (normalized): direction from eye to target
        Vector3 forward = target - eye;
        forward.normalize();
        
        // Right vector: cross(up, forward), then normalize
        Vector3 right = up.cross(forward);
        right.normalize();
        
        // Corrected up vector: cross(forward, right), then normalize
        Vector3 correctedUp = forward.cross(right);
        correctedUp.normalize();
        
        // Column-major storage: right (col 0), correctedUp (col 1), -forward (col 2), translation (col 3)
        m.data[0] = static_cast<float>(right.x);
        m.data[1] = static_cast<float>(correctedUp.x);
        m.data[2] = static_cast<float>(-forward.x);
        m.data[3] = 0.0f;
        
        m.data[4] = static_cast<float>(right.y);
        m.data[5] = static_cast<float>(correctedUp.y);
        m.data[6] = static_cast<float>(-forward.y);
        m.data[7] = 0.0f;
        
        m.data[8] = static_cast<float>(right.z);
        m.data[9] = static_cast<float>(correctedUp.z);
        m.data[10] = static_cast<float>(-forward.z);
        m.data[11] = 0.0f;
        
        // Translation: -dot(right, eye), -dot(correctedUp, eye), dot(forward, eye)
        m.data[12] = static_cast<float>(-right.dot(eye));
        m.data[13] = static_cast<float>(-correctedUp.dot(eye));
        m.data[14] = static_cast<float>(forward.dot(eye));
        m.data[15] = 1.0f;
        
        return m;
    }
    
    static Matrix4 translation(double x, double y, double z) {
        Matrix4 m;
        m.data[12] = static_cast<float>(x);
        m.data[13] = static_cast<float>(y);
        m.data[14] = static_cast<float>(z);
        return m;
    }
    
    static Matrix4 rotationX(float angle) {
        float c = std::cos(angle);
        float s = std::sin(angle);
        Matrix4 m;
        m.data[0] = 1.0f; m.data[5] = c; m.data[6] = -s; m.data[9] = s; m.data[10] = c;
        return m;
    }
    
    static Matrix4 rotationY(float angle) {
        float c = std::cos(angle);
        float s = std::sin(angle);
        Matrix4 m;
        m.data[0] = c; m.data[1] = s; m.data[2] = 0.0f; m.data[5] = -s; m.data[8] = 1.0f; m.data[9] = 0.0f; m.data[10] = c;
        return m;
    }
    
    static Matrix4 rotationZ(float angle) {
        float c = std::cos(angle);
        float s = std::sin(angle);
        Matrix4 m;
        m.data[0] = c; m.data[1] = -s; m.data[4] = s; m.data[5] = c;
        return m;
    }
    
    static Matrix4 scale(float x, float y, float z) {
        Matrix4 m;
        m.data[0] = x; m.data[5] = y; m.data[10] = z;
        return m;
    }
    
    // Matrix multiplication (column-major: result_col_j = sum_i(a_row_i * b_col_i))
    static Matrix4 multiply(const Matrix4& a, const Matrix4& b) {
        Matrix4 result;
        for (int j = 0; j < 4; j++) {
            for (int i = 0; i < 4; i++) {
                float sum = 0.0f;
                for (int k = 0; k < 4; k++) {
                    sum += a.data[k + i*4] * b.data[k + j*4];
                }
                result.data[i + j*4] = sum;
            }
        }
        return result;
    }
};

#endif // MATH_MATRIX4_H

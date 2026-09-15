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
    
    static Matrix4 perspective(double fovDegrees, double aspect,
                           double nearPlane, double farPlane) {
    Matrix4 m;
    std::memset(m.data, 0, sizeof(m.data));

    const double radians = fovDegrees * M_PI / 180.0;
    const float f = static_cast<float>(1.0 / std::tan(radians / 2.0));

    m.data[0]  = f / static_cast<float>(aspect);
    m.data[5]  = f;
    m.data[10] = static_cast<float>(
        (farPlane + nearPlane) / (nearPlane - farPlane));
    m.data[11] = -1.0f;
    m.data[14] = static_cast<float>(
        (2.0 * farPlane * nearPlane) / (nearPlane - farPlane));
    m.data[15] = 0.0f;

    return m;
}
    
    static Matrix4 lookAt(const Vector3& eye,
                      const Vector3& target,
                      const Vector3& up) {
    Vector3 forward = target - eye;
    forward.normalize();

    Vector3 right = forward.cross(up);
    right.normalize();

    Vector3 correctedUp = right.cross(forward);

    Matrix4 m;

    m.data[0] = static_cast<float>(right.x);
    m.data[1] = static_cast<float>(correctedUp.x);
    m.data[2] = static_cast<float>(-forward.x);
    m.data[3] = 0.0f;

    m.data[4] = static_cast<float>(right.y);
    m.data[5] = static_cast<float>(correctedUp.y);
    m.data[6] = static_cast<float>(-forward.y);
    m.data[7] = 0.0f;

    m.data[8]  = static_cast<float>(right.z);
    m.data[9]  = static_cast<float>(correctedUp.z);
    m.data[10] = static_cast<float>(-forward.z);
    m.data[11] = 0.0f;

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
    const float c = std::cos(angle);
    const float s = std::sin(angle);

    Matrix4 m;
    m.data[5] = c;
    m.data[6] = s;
    m.data[9] = -s;
    m.data[10] = c;
    return m;
}

static Matrix4 rotationY(float angle) {
    const float c = std::cos(angle);
    const float s = std::sin(angle);

    Matrix4 m;
    m.data[0] = c;
    m.data[2] = -s;
    m.data[8] = s;
    m.data[10] = c;
    return m;
}

static Matrix4 rotationZ(float angle) {
    const float c = std::cos(angle);
    const float s = std::sin(angle);

    Matrix4 m;
    m.data[0] = c;
    m.data[1] = s;
    m.data[4] = -s;
    m.data[5] = c;
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
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                float sum = 0.0f;
                for (int k = 0; k < 4; k++) {
                    sum += a.data[i + k*4] * b.data[k + j*4];
                }
                result.data[i + j*4] = sum;
            }
        }
        return result;
    }
    
    // Transform a point by this matrix (for testing)
    Vector3 transformPoint(const Vector3& p) const {
    const float x = static_cast<float>(p.x);
    const float y = static_cast<float>(p.y);
    const float z = static_cast<float>(p.z);

    const float tx = data[0] * x + data[4] * y + data[8]  * z + data[12];
    const float ty = data[1] * x + data[5] * y + data[9]  * z + data[13];
    const float tz = data[2] * x + data[6] * y + data[10] * z + data[14];
    const float tw = data[3] * x + data[7] * y + data[11] * z + data[15];

    if (std::abs(tw) < 1e-7f)
        return Vector3(tx, ty, tz);

    return Vector3(tx / tw, ty / tw, tz / tw);
}

Vector3 transformVector(const Vector3& v) const {
    const float x = static_cast<float>(v.x);
    const float y = static_cast<float>(v.y);
    const float z = static_cast<float>(v.z);

    return Vector3(
        data[0] * x + data[4] * y + data[8]  * z,
        data[1] * x + data[5] * y + data[9]  * z,
        data[2] * x + data[6] * y + data[10] * z
    );
}

};
#endif // MATH_MATRIX4_H

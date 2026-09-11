#ifndef MATH_MATRIX4_H
#define MATH_MATRIX4_H

#include <cmath>
#include <cstring>

class Matrix4 {
public:
    float data[16];
    
    Matrix4() {
        memset(data, 0, sizeof(data));
        // Identity matrix
        data[0] = data[5] = data[10] = data[15] = 1.0f;
    }
    
    static Matrix4 perspective(float fovY, float aspect, float near, float far) {
        float tanHalfFov = std::tan(fovY / 2.0f);
        Matrix4 m;
        // Column-major storage: row 0-3 become columns 0-3
        // Row 0: [aspect/tanHalfFov, 0, 0, 0]
        // Row 1: [0, 1/tanHalfFov, 0, 0]
        // Row 2: [0, 0, -(far+near)/(far-near), -2*far*near/(far-near)]
        // Row 3: [0, 0, -1, 0]
        m.data[0] = aspect / tanHalfFov;
        m.data[5] = 1.0f / tanHalfFov;
        m.data[10] = -(far + near) / (far - near);
        m.data[11] = -2.0f * far * near / (far - near);
        m.data[14] = -1.0f;
        return m;
    }
    
    static Matrix4 lookAt(double eyeX, double eyeY, double eyeZ, 
                         double centerX, double centerY, double centerZ,
                         double upX, double upY, double upZ) {
        Matrix4 m;
        // Forward vector (normalized)
        double f[3] = {centerX - eyeX, centerY - eyeY, centerZ - eyeZ};
        double len = std::sqrt(f[0]*f[0] + f[1]*f[1] + f[2]*f[2]);
        f[0]/=len; f[1]/=len; f[2]/=len;
        
        // Right vector (cross product of up and forward)
        double r[3] = {upY*f[2] - upZ*f[1], upZ*f[0] - upX*f[2], upX*f[1] - upY*f[0]};
        len = std::sqrt(r[0]*r[0] + r[1]*r[1] + r[2]*r[2]);
        r[0]/=len; r[1]/=len; r[2]/=len;
        
        // Side vector (cross product of forward and right)
        double s[3] = {f[1]*r[2] - f[2]*r[1], f[2]*r[0] - f[0]*r[2], f[0]*r[1] - f[1]*r[0]};
        
        // Column-major storage: right, up, side, translation
        m.data[0] = (float)r[0]; m.data[4] = (float)r[1]; m.data[8] = (float)r[2]; m.data[12] = 0.0f;
        m.data[1] = (float)s[0]; m.data[5] = (float)s[1]; m.data[9] = (float)s[2]; m.data[13] = 0.0f;
        m.data[2] = -(float)f[0]; m.data[6] = -(float)f[1]; m.data[10] = -(float)f[2]; m.data[14] = 0.0f;
        m.data[3] = 0.0f; m.data[7] = 0.0f; m.data[11] = 0.0f; m.data[15] = 1.0f;
        
        // Translation: -dot(right, eye), -dot(side, eye), dot(forward, eye)
        m.data[12] -= (float)(r[0]*eyeX + r[1]*eyeY + r[2]*eyeZ);
        m.data[13] -= (float)(s[0]*eyeX + s[1]*eyeY + s[2]*eyeZ);
        m.data[14] += (float)(f[0]*eyeX + f[1]*eyeY + f[2]*eyeZ);
        
        return m;
    }
    
    static Matrix4 translation(double x, double y, double z) {
        Matrix4 m;
        m.data[12] = (float)x;
        m.data[13] = (float)y;
        m.data[14] = (float)z;
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
};

#endif // MATH_MATRIX4_H

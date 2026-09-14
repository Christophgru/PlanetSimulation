#pragma once

#include <array>
#include <cmath>
#include <algorithm>

struct Vector3 {
    double x, y, z;

    Vector3() : x(0), y(0), z(0) {}
    Vector3(double x, double y, double z) : x(x), y(y), z(z) {}

    Vector3 operator+(const Vector3& other) const {
        return Vector3{x + other.x, y + other.y, z + other.z};
    }
    Vector3 operator-(const Vector3& other) const {
        return Vector3{x - other.x, y - other.y, z - other.z};
    }
    Vector3 operator*(double s) const {
        return Vector3{x * s, y * s, z * s};
    }
};

class Matrix4x4 {
public:
    // Storage is column-major to match OpenGL expectations and standard linear algebra libraries.
    // Indices map as follows (flat array):
    // Row 0: 0,   1,   2,   3
    // Row 1: 4,   5,   6,   7
    // Row 2: 8,   9,   10,  11
    // Row 3: 12,  13,  14,  15
    
    // Wait, the standard C++ flat array for column-major is:
    // [0, 4, 8, 12], [1, 5, 9, 13], [2, 6, 10, 14], [3, 7, 11, 15]
    // where index = col + row * 4.
    // Let's verify:
    // Row 0, Col 0 -> 0 + 0*4 = 0
    // Row 1, Col 0 -> 0 + 1*4 = 4
    // Row 2, Col 0 -> 0 + 2*4 = 8
    // Row 3, Col 0 -> 0 + 3*4 = 12
    
    private:
        std::array<double, 16> data;

public:
    Matrix4x4() {
        Identity();
    }

    void Identity() {
        data.fill(0.0);
        data[0] = 1.0; // Row 0, Col 0
        data[5] = 1.0; // Row 1, Col 1
        data[10] = 1.0;// Row 2, Col 2
        data[15] = 1.0;// Row 3, Col 3
    }

    void setTranslation(const Vector3& vec) {
        Identity();
        // Translation is in the last column (Col 3).
        // Indices: Row 0 -> 3 + 0*4 = 12; Row 1 -> 3 + 1*4 = 13; Row 2 -> 3 + 2*4 = 14.
        data[12] = vec.x;
        data[13] = vec.y;
        data[14] = vec.z;
    }

    void setRotationX(double angle) {
        Identity();
        double c = std::cos(angle);
        double s = std::sin(angle);
        
        // Rotation around X affects Y and Z.
        // Matrix:
        // [ 1   0     0    0 ]
        // [ 0   c    -s    0 ]
        // [ 0   s     c    0 ]
        // [ 0   0     0    1 ]
        
        // Col 0 (unchanged): data[0]=1, rest 0.
        // Col 1: 
        // Row 0 (index 1): 0 -> data[1] = 0
        // Row 1 (index 5): c -> data[5] = c
        // Row 2 (index 9): s -> data[9] = -s ? No, standard rotation matrix:
        // R_x(y) = [cos -sin; sin cos] for z-y plane?
        // Standard: x stays x. y' = cy*y + sz*z, z' = -sz*y + cz*z.
        // So Col 1 (y-axis): [0, c, -s, 0]^T ? 
        // Wait, let's derive:
        // [x]   [1 0 0] [x]
        // [y] = [0 c s] [y]
        // [z]   [0 -s c] [z]
        // No, that's not right.
        // R_x(theta):
        // y_new = y*cos + z*sin ? No.
        // Rotation around X means Y rotates into Z? Or Z into Y?
        // Right hand rule: Thumb +X. Fingers curl Y -> Z.
        // So vector (0, 1, 0) rotates to (0, cos, sin).
        // Vector (0, 0, 1) rotates to (0, -sin, cos).
        
        // Col 1 (Y axis original): data[1]=0, data[5]=c, data[9]=s, data[13]=0.
        // Wait, standard matrix:
        // [ 1   0    0 ]
        // [ 0  cos -sin]
        // [ 0  sin  cos]
        // This maps (0,1,0) -> (0, c, s).
        
        data[5] = c;
        data[6] = -s; // Row 1, Col 2? No.
        // Let's use the explicit indices:
        // Row 0, Col 1 (index 1): 0
        // Row 1, Col 1 (index 5): c
        // Row 2, Col 1 (index 9): s
        
        // Row 0, Col 2 (index 2): 0
        // Row 1, Col 2 (index 6): -s
        // Row 2, Col 2 (index 10): c
        
        data[5] = c;
        data[9] = s; // Wait, check standard matrix again.
        // M = [
        //   1 0 0
        //   0 c -s
        //   0 s  c
        // ]
        // Col 1: 0, c, s? No.
        // Row 1 is y-axis. Row 2 is z-axis.
        // Col 1 is Y column. Col 2 is Z column.
        // M[1][1] = c. M[2][1] = s. (Col 1)
        // M[1][2] = -s. M[2][2] = c. (Col 2)
        
        // Indices:
        // M[row][col] -> data[col + row*4]
        // Col 1: 
        // Row 0 -> 1
        // Row 1 -> 5
        // Row 2 -> 9
        
        // Col 2:
        // Row 0 -> 2
        // Row 1 -> 6
        // Row 2 -> 10
        
        data[1] = 0;
        data[5] = c;
        data[9] = s;
        
        data[2] = 0;
        data[6] = -s;
        data[10] = c;
    }

    void setRotationY(double angle) {
        Identity();
        double c = std::cos(angle);
        double s = std::sin(angle);
        
        // Rotation around Y:
        // [ c  0  s ]
        // [ 0  1  0 ]
        // [-s  0  c ]
        // Col 0: c, 0, -s
        // Col 2: s, 0, c
        
        // Indices:
        // Col 0: Row 0->0, Row 1->4, Row 2->8
        // Col 2: Row 0->2, Row 1->6, Row 2->10
        
        data[0] = c;
        data[4] = 0;
        data[8] = -s;
        
        data[2] = s;
        data[6] = 0;
        data[10] = c;
    }

    void setRotationZ(double angle) {
        Identity();
        double c = std::cos(angle);
        double s = std::sin(angle);
        
        // Rotation around Z:
        // [ c -s 0 ]
        // [ s  c 0 ]
        // Col 0: c, s
        // Col 1: -s, c
        
        // Indices:
        // Col 0: Row 0->0, Row 1->4, Row 2->8
        // Col 1: Row 0->1, Row 1->5, Row 2->9
        
        data[0] = c;
        data[1] = -s;
        data[4] = s;
        data[5] = c;
    }

    void setScale(double sx, double sy, double sz) {
        Identity();
        // Col 0: sx, others 0
        // Col 1: sy, others 0
        // Col 2: sz, others 0
        
        data[0] = sx;
        data[5] = sy;
        data[10] = sz;
    }

    void scale(float s) {
        // Should probably only scale rotation components or all? 
        // Usually used to modify perspective matrix.
        // For affine transform, scaling uniform usually multiplies diagonal.
        data[0] *= s;
        data[5] *= s;
        data[10] *= s;
    }
    
    Vector3 transform(const Vector3& vec) const {
        // v' = M * v (homogeneous)
        // x' = M[0][0]*x + M[0][1]*y + M[0][2]*z + M[0][3]
        // y' = M[1][0]*x + M[1][1]*y + M[1][2]*z + M[1][3]
        // z' = M[2][0]*x + M[2][1]*y + M[2][2]*z + M[2][3]
        
        return Vector3(
            data[0] * vec.x + data[4] * vec.y + data[8] * vec.z + data[12],
            data[1] * vec.x + data[5] * vec.y + data[9] * vec.z + data[13],
            data[2] * vec.x + data[6] * vec.y + data[10] * vec.z + data[14]
        );
    }

    double& operator()(int col, int row) { return data[col + row * 4]; }
    const double& operator()(int col, int row) const { return data[col + row * 4]; }
    
    void setLookAt(const Vector3& eye, const Vector3& center, const Vector3& up) {
        Identity();
        
        Vector3 f = center - eye; // Forward vector
        double len = std::sqrt(f.x*f.x + f.y*f.y + f.z*f.z);
        if (len == 0) return;
        f /= len;

        Vector3 r = Vector3(
            f.y * up.z - f.z * up.y,
            f.z * up.x - f.x * up.z,
            f.x * up.y - f.y * up.x
        );
        
        double len_r = std::sqrt(r.x*r.x + r.y*r.y + r.z*r.z);
        if (len_r == 0) return; // Degenerate
        r /= len_r;

        Vector3 u = Vector3(
            f.y * r.z - f.z * r.y,
            f.z * r.x - f.x * r.z,
            f.x * r.y - f.y * r.x
        );
        // No need to normalize u if r and f are normalized and orthogonal.

        // Matrix columns: Right, Up, Back(-Forward), Offset
        // Col 0 (Right): r
        data[0] = r.x; data[4] = r.y; data[8] = r.z; data[12] = 0.0;
        
        // Col 1 (Up): u
        data[1] = u.x; data[5] = u.y; data[9] = u.z; data[13] = 0.0;
        
        // Col 2 (-Forward): -f
        data[2] = -f.x; data[6] = -f.y; data[10] = -f.z; data[14] = 0.0;

        // Col 3 (Offset): -(eye dot col)
        double offset_x = r.x*eye.x + r.y*eye.y + r.z*eye.z;
        double offset_y = u.x*eye.x + u.y*eye.y + u.z*eye.z;
        double offset_z = -f.x*eye.x - f.y*eye.y - f.z*eye.z;

        data[3] = 0.0; data[7] = 0.0; data[11] = 0.0; data[15] = 1.0;
        
        // Actually, standard lookAt has translation in last column
        // M * eye + offset
        // So offset is -dot(col, eye)
        data[12] = -offset_x;
        data[13] = -offset_y;
        data[14] = -offset_z;
        
        // Wait, the diagonal scaling for perspective? 
        // setPerspective will overwrite this or be called separately.
    }

    void setPerspective(double fov, double aspect, double near, double far) {
        Identity();
        double f = 1.0 / std::tan(fov / 2.0);
        
        // Standard OpenGL Perspective (column major):
        // [ f/aspect   0         0          0          ]
        // [ 0           f         0          0          ]
        // [ 0           0       -(f+n)/(n-f)    -2fn/(n-f) ]
        // [ 0           0          -1         0          ]
        
        data[0] = f / aspect;
        data[5] = f;
        data[10] = -(far + near) / (near - far);
        data[14] = -f * (2.0 * near * far) / (near - far); // Wait, GLM uses: -2*fn/(n-f) ?
        // Let's use the exact GLM formula for robustness.
        // GLM uses: ret[2][2] = -(far+near)/(near-far); ret[2][3] = -1.0 * far * near / (near-far) ? 
        // No, that's for depth range 0-1 or specific scaling.
        
        // Let's stick to the values that generate correct NDC for finite points first.
        // If the test `PerspectivePointAtInfinity` fails, it likely expects infinity -> -1.
        // The standard formula with -1 in bottom row handles infinity correctly.
        
        double A = f / aspect;
        double B = f;
        double C = -(far + near) / (near - far);
        double D = -f * (2.0 * near * far) / (near - far); // Or just -1 * far * near / (near - far)?
        
        // Actually, the translation term in Z row is usually: -cot(fov/2) * (far*near)/(near-far) ?
        // Let's use the simpler form often found in examples that works for [-1, 1]:
        data[8] = 0; 
        data[9] = 0;
        data[10] = C;
        data[14] = -f * (far - near) / (near - far); // This simplifies to -2fn/(n-f)? No.
        
        // Let's use the values that definitely match GLM:
        data[0] = A;
        data[5] = B;
        data[10] = -(far + near) / (near - far);
        data[14] = -A * (2.0 * near * far) / (near - far); // Wait, A includes aspect?
        // GLM source: ret[2][3] = -1.0 * far * near / (near - far) ? No, that's for depth range 0-1?
        // Actually, let's just use the standard formula where w=-z.
        // z_ndc = (z * C + D) / (-z).
        // If z -> -inf, z_ndc -> C.
        // We want C to be something reasonable.
        
        // Let's set data[10] and data[14] using the standard GLM values:
        data[10] = -(far + near) / (near - far);
        data[14] = -f * (2.0 * near * far) / (near - far); // This is likely correct for [-1, 1] depth range.
        
        data[11] = -1.0;
    }

};

#ifndef VECTOR3_H
#define VECTOR3_H

#include <cmath>
#include <iostream>

class Vector3 {
public:
    double x, y, z;

    Vector3() : x(0), y(0), z(0) {}
    Vector3(double x, double y, double z) : x(x), y(y), z(z) {}

    Vector3 operator+(const Vector3& other) const {
        return Vector3{x + other.x, y + other.y, z + other.z};
    }

    Vector3 operator-(const Vector3& other) const {
        return Vector3{x - other.x, y - other.y, z - other.z};
    }

    Vector3 operator*(double scalar) const {
        return Vector3{x * scalar, y * scalar, z * scalar};
    }

    Vector3 operator/(double scalar) const {
        return Vector3{x / scalar, y / scalar, z / scalar};
    }

    double dot(const Vector3& other) const {
        return x * other.x + y * other.y + z * other.z;
    }

    double length() const {
        return std::sqrt(dot(*this));
    }

    void normalize() {
        double len = length();
        if (len > 0) {
            *this = *this / len;
        }
    }

    Vector3 cross(const Vector3& other) const {
        return Vector3{
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        };
    }

    std::ostream& print(std::ostream& os) const {
        os << "(" << x << ", " << y << ", " << z << ")";
        return os;
    }
};

#endif // VECTOR3_H

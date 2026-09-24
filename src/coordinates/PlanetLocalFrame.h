#pragma once

#include <cmath>
#include <stdexcept>

#include <glm/glm.hpp>

namespace coordinates {

// Spherical planet coordinates: +Z is north, +X is longitude 0,
// and +Y is longitude 90 degrees east. Altitude is measured from the sphere.
struct LatLonAlt {
    double latitudeDeg = 0.0;
    double longitudeDeg = 0.0;
    double altitude = 0.0;
};

// A right-handed local tangent frame: North x East = Down.
struct NedFrame {
    glm::dvec3 north;
    glm::dvec3 east;
    glm::dvec3 down;

    glm::dvec3 toWorld(const glm::dvec3& ned) const {
        return ned.x * north + ned.y * east + ned.z * down;
    }

    glm::dvec3 fromWorld(const glm::dvec3& worldVector) const {
        return {glm::dot(worldVector, north), glm::dot(worldVector, east),
                glm::dot(worldVector, down)};
    }
};

class PlanetLocalFrame {
public:
    PlanetLocalFrame(const glm::dvec3& planetCenter, double planetRadius,
                     const glm::dmat3& orientation = glm::dmat3(1.0))
        : center_(planetCenter), radius_(planetRadius), orientation_(orientation) {
        if (!std::isfinite(radius_) || radius_ <= 0.0 ||
            !std::isfinite(center_.x) || !std::isfinite(center_.y) ||
            !std::isfinite(center_.z)) {
            throw std::invalid_argument("Planet frame requires a finite center and positive radius");
        }
        const glm::dmat3 product = glm::transpose(orientation_) * orientation_;
        for (int col = 0; col < 3; ++col)
            for (int row = 0; row < 3; ++row)
                if (!std::isfinite(product[col][row]) ||
                    std::abs(product[col][row] - (col == row ? 1.0 : 0.0)) > 1e-10)
                    throw std::invalid_argument("Planet orientation must be a rotation");
        if (glm::determinant(orientation_) < 0.0)
            throw std::invalid_argument("Planet orientation must be right handed");
    }

    glm::dvec3 toWorld(const LatLonAlt& location) const {
        validate(location);
        const double lat = glm::radians(location.latitudeDeg);
        const double lon = glm::radians(location.longitudeDeg);
        const double distance = radius_ + location.altitude;
        return center_ + toWorldVector(distance * glm::dvec3(std::cos(lat) * std::cos(lon),
                                                std::cos(lat) * std::sin(lon),
                                                std::sin(lat)));
    }

    LatLonAlt fromWorld(const glm::dvec3& worldPosition) const {
        const glm::dvec3 local = toLocalVector(worldPosition - center_);
        const double distance = glm::length(local);
        if (!std::isfinite(distance) || distance <= 0.0) {
            throw std::invalid_argument("Position must be away from the planet center");
        }
        return {glm::degrees(std::asin(glm::clamp(local.z / distance, -1.0, 1.0))),
                glm::degrees(std::atan2(local.y, local.x)), distance - radius_};
    }

    NedFrame nedAt(const LatLonAlt& location) const {
        validate(location);
        const double lat = glm::radians(location.latitudeDeg);
        const double lon = glm::radians(location.longitudeDeg);
        return {
            toWorldVector(glm::dvec3(-std::sin(lat) * std::cos(lon),
                       -std::sin(lat) * std::sin(lon), std::cos(lat))),
            toWorldVector(glm::dvec3(-std::sin(lon), std::cos(lon), 0.0)),
            toWorldVector(glm::dvec3(-std::cos(lat) * std::cos(lon),
                       -std::cos(lat) * std::sin(lon), -std::sin(lat)))
        };
    }

    const glm::dvec3& center() const { return center_; }
    double radius() const { return radius_; }
    const glm::dmat3& orientation() const { return orientation_; }
    glm::dvec3 toWorldVector(const glm::dvec3& local) const { return orientation_ * local; }
    glm::dvec3 toLocalVector(const glm::dvec3& world) const { return glm::transpose(orientation_) * world; }

private:
    void validate(const LatLonAlt& location) const {
        if (!std::isfinite(location.latitudeDeg) ||
            !std::isfinite(location.longitudeDeg) ||
            !std::isfinite(location.altitude) ||
            location.latitudeDeg < -90.0 || location.latitudeDeg > 90.0 ||
            radius_ + location.altitude <= 0.0) {
            throw std::invalid_argument("Invalid planet latitude, longitude, or altitude");
        }
    }

    glm::dvec3 center_;
    double radius_;
    glm::dmat3 orientation_;
};

} // namespace coordinates

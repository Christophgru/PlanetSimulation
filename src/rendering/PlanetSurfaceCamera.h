#pragma once

#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "coordinates/PlanetLocalFrame.h"

class PlanetSurfaceCamera {
public:
    PlanetSurfaceCamera(const coordinates::PlanetLocalFrame& planetFrame,
                        const coordinates::LatLonAlt& location,
                        const glm::dvec3& sunPosition, double fieldOfViewDeg)
        : frame_(planetFrame), location_(location), position_(frame_.toWorld(location)),
          sunPosition_(sunPosition), fov_(static_cast<float>(fieldOfViewDeg)) {
        if (!finite(sunPosition_) || !std::isfinite(fieldOfViewDeg) ||
            fieldOfViewDeg <= 0.0 || fieldOfViewDeg >= 180.0) {
            throw std::invalid_argument("Surface camera requires a valid Sun target and field of view");
        }
        aimAt(sunPosition_);
    }

    glm::mat4 getViewMatrix() const {
        return glm::mat4(glm::lookAt(position_, position_ + direction_, up_));
    }

    // A saved view direction is expressed in the planet's current NED frame.
    void setDirectionNed(const glm::dvec3& nedDirection,
                         const std::optional<glm::dvec3>& upNed = std::nullopt) {
        const double magnitude = std::hypot(nedDirection.x, nedDirection.y,
                                            nedDirection.z);
        if (!finite(nedDirection) || !std::isfinite(magnitude) ||
            magnitude <= 1e-12) {
            throw std::invalid_argument("Surface view direction must be a nonzero NED vector");
        }
        const auto ned = frame_.nedAt(location_);
        const glm::dvec3 newDirection =
            glm::normalize(ned.toWorld(nedDirection / magnitude));
        std::optional<glm::dvec3> newUp;
        if (upNed) {
            const double upLength = std::hypot(upNed->x, upNed->y, upNed->z);
            if (!finite(*upNed) || !std::isfinite(upLength) || upLength <= 1e-12) {
                throw std::invalid_argument("Surface view up must be a nonzero NED vector");
            }
            const glm::dvec3 worldUp = ned.toWorld(*upNed / upLength);
            const glm::dvec3 projected = worldUp -
                glm::dot(worldUp, newDirection) * newDirection;
            if (glm::length(projected) <= 1e-8) {
                throw std::invalid_argument("Surface view up must not align with its direction");
            }
            newUp = glm::normalize(projected);
        }
        direction_ = newDirection;
        updateAnglesFromDirection();
        if (newUp) up_ = *newUp;
        else updateUp(ned.north);
    }

    glm::dvec3 directionNed() const {
        return frame_.nedAt(location_).fromWorld(direction_);
    }

    glm::dvec3 upNed() const {
        return frame_.nedAt(location_).fromWorld(up_);
    }

    // Mouse movement controls the view without holding a button.
    void look(double deltaX, double deltaY) {
        if (!std::isfinite(deltaX) || !std::isfinite(deltaY)) return;
        deltaX = std::clamp(deltaX, -500.0, 500.0);
        deltaY = std::clamp(deltaY, -500.0, 500.0);
        const glm::dvec3 right = glm::normalize(glm::cross(direction_, up_));
        const glm::dvec3 candidate = direction_ +
            kMouseRadiansPerPixel * (deltaX * right - deltaY * up_);
        if (glm::length(candidate) <= 0.0) return;
        direction_ = glm::normalize(candidate);
        updateAnglesFromDirection();
        const double pitchLimit = glm::radians(89.9);
        if (std::abs(pitch_) > pitchLimit) {
            pitch_ = std::clamp(pitch_, -pitchLimit, pitchLimit);
            updateDirectionFromAngles();
        }
        updateUp(up_);
    }

    // WASD moves along a spherical arc and keeps the current altitude.
    void walk(int forwardAxis, int rightAxis, double elapsedSeconds) {
        if (!std::isfinite(elapsedSeconds) || elapsedSeconds <= 0.0 ||
            (forwardAxis == 0 && rightAxis == 0)) return;
        const auto ned = frame_.nedAt(location_);
        const glm::dvec3 tangentForward =
            std::cos(heading_) * ned.north + std::sin(heading_) * ned.east;
        const glm::dvec3 tangentRight =
            -std::sin(heading_) * ned.north + std::cos(heading_) * ned.east;
        const glm::dvec3 tangent = glm::normalize(
            static_cast<double>(forwardAxis) * tangentForward +
            static_cast<double>(rightAxis) * tangentRight);
        const double distance = frame_.radius() + location_.altitude;
        const double angle = kWalkSpeed * elapsedSeconds / distance;
        const glm::dvec3 radial = glm::normalize(position_ - frame_.center());
        const glm::dvec3 nextRadial =
            glm::normalize(std::cos(angle) * radial + std::sin(angle) * tangent);
        const double altitude = location_.altitude;
        location_ = frame_.fromWorld(frame_.center() + distance * nextRadial);
        location_.altitude = altitude;
        position_ = frame_.toWorld(location_);
        updateDirectionFromAngles();
        updateUp(up_);
    }

    // Automatic entry preserves an outside position and its aim. Positions
    // inside the sphere are lifted just above the surface.
    void enterFromWorld(const glm::dvec3& worldPosition,
                        const glm::dvec3& lookTarget) {
        if (!finite(worldPosition) || !finite(lookTarget)) {
            throw std::invalid_argument("Surface entry requires finite coordinates");
        }
        const glm::dvec3 offset = worldPosition - frame_.center();
        if (glm::length(offset) > 1e-12) {
            location_ = frame_.fromWorld(worldPosition);
            location_.altitude = std::max(location_.altitude,
                                          frame_.radius() * kMinimumEyeHeightFraction);
            position_ = frame_.toWorld(location_);
        }
        aimAt(lookTarget);
    }

    const coordinates::PlanetLocalFrame& frame() const { return frame_; }
    const coordinates::LatLonAlt& location() const { return location_; }
    const glm::dvec3& position() const { return position_; }
    const glm::dvec3& target() const { return sunPosition_; }
    const glm::dvec3& direction() const { return direction_; }
    const glm::dvec3& up() const { return up_; }
    glm::dvec3 down() const { return frame_.nedAt(location_).down; }
    float fov() const { return fov_; }

private:
    static constexpr double kMouseRadiansPerPixel = 0.005;
    static constexpr double kWalkSpeed = 0.4;
    static constexpr double kMinimumEyeHeightFraction = 0.02;

    static bool finite(const glm::dvec3& vector) {
        return std::isfinite(vector.x) && std::isfinite(vector.y) &&
               std::isfinite(vector.z);
    }

    void aimAt(const glm::dvec3& target) {
        const glm::dvec3 offset = target - position_;
        if (!finite(offset) || glm::length(offset) <= 1e-12) {
            throw std::invalid_argument("Surface camera must be away from its look target");
        }
        direction_ = glm::normalize(offset);
        updateAnglesFromDirection();
        updateUp(frame_.nedAt(location_).north);
    }

    void updateAnglesFromDirection() {
        const auto ned = frame_.nedAt(location_);
        const double north = glm::dot(direction_, ned.north);
        const double east = glm::dot(direction_, ned.east);
        if (std::hypot(north, east) > 1e-8) {
            heading_ = std::atan2(east, north);
        }
        pitch_ = std::asin(std::clamp(glm::dot(direction_, -ned.down), -1.0, 1.0));
    }

    void updateDirectionFromAngles() {
        const auto ned = frame_.nedAt(location_);
        const glm::dvec3 tangent =
            std::cos(heading_) * ned.north + std::sin(heading_) * ned.east;
        direction_ = glm::normalize(std::cos(pitch_) * tangent -
                                    std::sin(pitch_) * ned.down);
    }

    void updateUp(const glm::dvec3& preferredUp) {
        const auto ned = frame_.nedAt(location_);
        auto project = [this](const glm::dvec3& vector) {
            return vector - glm::dot(vector, direction_) * direction_;
        };
        glm::dvec3 preferred = project(preferredUp);
        if (glm::length(preferred) < 1e-8) preferred = project(ned.north);
        if (glm::length(preferred) < 1e-8) preferred = project(ned.east);
        preferred = glm::normalize(preferred);

        glm::dvec3 gravityUp = project(-ned.down);
        const double vertical = std::abs(glm::dot(direction_, -ned.down));
        if (glm::length(gravityUp) < 1e-8 || vertical >= 0.98) {
            up_ = preferred;
            return;
        }
        gravityUp = glm::normalize(gravityUp);
        const double gravityWeight = std::clamp((0.98 - vertical) / 0.08, 0.0, 1.0);
        const glm::dvec3 blended =
            (1.0 - gravityWeight) * preferred + gravityWeight * gravityUp;
        up_ = glm::length(blended) < 1e-8 ? gravityUp : glm::normalize(blended);
    }

    coordinates::PlanetLocalFrame frame_;
    coordinates::LatLonAlt location_;
    glm::dvec3 position_;
    glm::dvec3 sunPosition_;
    glm::dvec3 direction_;
    glm::dvec3 up_;
    double heading_ = 0.0;
    double pitch_ = 0.0;
    float fov_;
};

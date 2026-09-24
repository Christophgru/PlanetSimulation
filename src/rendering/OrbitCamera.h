#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class OrbitCamera {
public:
    struct Settings {
        float minDistance = 2.0f;
        float maxDistance = 200.0f;
        float zoomFactor = 0.96f;
    };

    glm::vec3 position;
    glm::vec3 target;
    float fov = 60.0f;

    OrbitCamera(const glm::vec3& focus, const glm::vec3& initialOffset)
        : OrbitCamera(focus, initialOffset, Settings{}) {}

    OrbitCamera(const glm::vec3& focus, const glm::vec3& initialOffset,
                const Settings& settings)
        : position(focus + initialOffset), target(focus), settings_(settings) {
        if (!std::isfinite(settings_.minDistance) || settings_.minDistance <= 0.0f ||
            !std::isfinite(settings_.maxDistance) ||
            settings_.maxDistance <= settings_.minDistance ||
            !std::isfinite(settings_.zoomFactor) ||
            settings_.zoomFactor <= 0.0f || settings_.zoomFactor >= 1.0f ||
            !std::isfinite(glm::length(focus)) ||
            !std::isfinite(glm::length(initialOffset)))
            throw std::invalid_argument("Invalid orbit camera settings");
        distance = glm::length(initialOffset);
        if (distance < settings_.minDistance) {
            distance = settings_.minDistance;
            position = target + glm::vec3(0.0f, 0.0f, distance);
        } else if (distance > settings_.maxDistance) {
            distance = settings_.maxDistance;
            position = target + glm::normalize(initialOffset) * distance;
        }
        desiredDistance_ = distance;
        const glm::vec3 offset = position - target;
        yaw = std::atan2(offset.x, offset.z);
        pitch = std::asin(std::clamp(offset.y / distance, -1.0f, 1.0f));
    }

    void orbit(float deltaX, float deltaY) {
        yaw = std::remainder(yaw - deltaX * kRadiansPerPixel,
                             2.0f * std::numbers::pi_v<float>);
        const float pitchLimit = glm::radians(89.0f);
        pitch = std::clamp(pitch + deltaY * kRadiansPerPixel,
                           -pitchLimit, pitchLimit);
        updatePosition();
    }

    void alignRadial(const glm::vec3& radial) {
        const float length = glm::length(radial);
        if (!std::isfinite(length) || length <= 0.0f)
            throw std::invalid_argument("Orbit alignment needs a finite direction");
        const glm::vec3 direction = radial / length;
        yaw = std::atan2(direction.x, direction.z);
        pitch = std::asin(std::clamp(direction.y, -1.0f, 1.0f));
        pitch = std::clamp(pitch, -glm::radians(89.0f), glm::radians(89.0f));
        updatePosition();
    }

    void zoom(float scrollY) {
        if (!std::isfinite(scrollY)) return;
        desiredDistance_ = std::clamp(
            desiredDistance_ * std::pow(settings_.zoomFactor, scrollY),
            settings_.minDistance, settings_.maxDistance);
    }

    // Ease toward the wheel's requested distance while keeping drag immediate.
    void advance(double elapsedSeconds) {
        if (!std::isfinite(elapsedSeconds) || elapsedSeconds <= 0.0) return;
        const float blend = static_cast<float>(
            1.0 - std::exp(-std::min(elapsedSeconds, 1.0) * 12.0));
        distance += (desiredDistance_ - distance) * blend;
        if (std::abs(desiredDistance_ - distance) < 1e-5f)
            distance = desiredDistance_;
        updatePosition();
    }

    float requestedDistance() const { return desiredDistance_; }

    void followTarget(const glm::vec3& focus) {
        if (!std::isfinite(focus.x) || !std::isfinite(focus.y) || !std::isfinite(focus.z))
            throw std::invalid_argument("Orbit target must be finite");
        target = focus;
        updatePosition();
    }

    glm::mat4 getViewMatrix() const {
        return glm::lookAt(position, target, glm::vec3(0.0f, 1.0f, 0.0f));
    }

private:
    static constexpr float kRadiansPerPixel = 0.005f;
    Settings settings_;
    float distance = 0.0f;
    float desiredDistance_ = 0.0f;
    float yaw = 0.0f;
    float pitch = 0.0f;

    void updatePosition() {
        const float horizontalDistance = distance * std::cos(pitch);
        position = target + glm::vec3(
            horizontalDistance * std::sin(yaw),
            distance * std::sin(pitch),
            horizontalDistance * std::cos(yaw));
    }
};

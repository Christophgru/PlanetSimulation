#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class OrbitCamera {
public:
    glm::vec3 position;
    glm::vec3 target;
    float fov = 60.0f;

    OrbitCamera(const glm::vec3& focus, const glm::vec3& initialOffset)
        : position(focus + initialOffset), target(focus) {
        distance = glm::length(initialOffset);
        if (distance < kMinDistance) {
            distance = kMinDistance;
            position = target + glm::vec3(0.0f, 0.0f, distance);
        }
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

    void zoom(float scrollY) {
        distance = std::clamp(distance * std::pow(kZoomFactor, scrollY),
                               kMinDistance, kMaxDistance);
        updatePosition();
    }

    glm::mat4 getViewMatrix() const {
        return glm::lookAt(position, target, glm::vec3(0.0f, 1.0f, 0.0f));
    }

private:
    static constexpr float kRadiansPerPixel = 0.005f;
    static constexpr float kZoomFactor = 0.9f;
    static constexpr float kMinDistance = 2.0f;
    static constexpr float kMaxDistance = 200.0f;

    float distance = 0.0f;
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

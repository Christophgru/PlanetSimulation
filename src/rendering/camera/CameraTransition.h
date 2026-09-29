#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <numbers>
#include <stdexcept>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace rendering {

struct CameraPose {
    glm::dvec3 position{0.0};
    glm::dquat orientation{1.0, 0.0, 0.0, 0.0};
    float fov = 60.0f;

    static CameraPose fromView(const glm::dvec3& eye, const glm::mat4& view, float fov) {
        return {eye, glm::normalize(glm::quat_cast(glm::transpose(glm::dmat3(view)))), fov};
    }
    glm::dvec3 forward() const { return orientation * glm::dvec3(0.0, 0.0, -1.0); }
    glm::dvec3 up() const { return orientation * glm::dvec3(0.0, 1.0, 0.0); }
    glm::mat4 view() const {
        return glm::mat4(glm::lookAt(position, position + forward(), up()));
    }
};

// Wall-clock one-second descent. The target is resampled each frame so a
// rotating/translated planet does not leave the camera at a stale location.
class CameraTransition {
public:
    static constexpr double durationSeconds = 1.0;

    void start(const CameraPose& from, double wallSeconds) {
        if (!std::isfinite(wallSeconds)) throw std::invalid_argument("Transition time must be finite");
        from_ = from;
        startedAt_ = wallSeconds;
        active_ = true;
    }
    void cancel() { active_ = false; }
    bool active() const { return active_; }

    CameraPose sample(double wallSeconds, const CameraPose& target,
                      const glm::dvec3& planetCenter,
                      const std::function<double(const glm::dvec3&)>& groundRadius) {
        if (!active_) return target;
        if (!std::isfinite(wallSeconds) || wallSeconds < startedAt_)
            throw std::invalid_argument("Transition time must be monotonic");
        const double fraction = std::clamp((wallSeconds-startedAt_)/durationSeconds, 0.0, 1.0);
        if (fraction >= 1.0) { active_ = false; return target; }
        if (fraction <= 0.0) return from_;
        const double eased = fraction*fraction*(3.0-2.0*fraction);
        const glm::dvec3 startOffset = from_.position - planetCenter;
        const glm::dvec3 endOffset = target.position - planetCenter;
        const double startRadius = glm::length(startOffset);
        const double endRadius = glm::length(endOffset);
        if (startRadius <= 0.0 || endRadius <= 0.0)
            throw std::invalid_argument("Transition cameras must lie outside the planet center");
        const glm::dvec3 radial = sphericalDirection(startOffset/startRadius,
                                                       endOffset/endRadius, eased);
        const double distance = std::exp(glm::mix(std::log(startRadius), std::log(endRadius), eased));
        const double floor = groundRadius(radial);
        if (!std::isfinite(floor) || floor <= 0.0)
            throw std::invalid_argument("Transition terrain radius must be positive");
        return {planetCenter + radial*std::max(distance, floor),
                glm::normalize(glm::slerp(from_.orientation, target.orientation, eased)),
                static_cast<float>(glm::mix(static_cast<double>(from_.fov),
                                            static_cast<double>(target.fov), eased))};
    }

private:
    CameraPose from_;
    double startedAt_ = 0.0;
    bool active_ = false;

    static glm::dvec3 sphericalDirection(const glm::dvec3& a, const glm::dvec3& b, double t) {
        const double dot = std::clamp(glm::dot(a,b), -1.0, 1.0);
        if (dot > 0.999999) return glm::normalize(glm::mix(a,b,t));
        if (dot < -0.999999) {
            const glm::dvec3 reference = std::abs(a.y) < 0.9 ? glm::dvec3(0,1,0) : glm::dvec3(1,0,0);
            const glm::dvec3 axis = glm::normalize(glm::cross(a,reference));
            return glm::angleAxis(std::numbers::pi*t, axis) * a;
        }
        const double angle = std::acos(dot);
        return (std::sin((1.0-t)*angle)*a + std::sin(t*angle)*b) / std::sin(angle);
    }
};

} // namespace rendering

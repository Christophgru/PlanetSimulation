#pragma once

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace rendering {

struct ClipPlanes {
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
};

inline ClipPlanes surfaceClipPlanes(double surfaceClearance, double sunDistance,
                                    double sunRadius) {
    if (!std::isfinite(surfaceClearance) || surfaceClearance < 0.0 ||
        !std::isfinite(sunDistance) || sunDistance <= 0.0 ||
        !std::isfinite(sunRadius) || sunRadius < 0.0) {
        throw std::invalid_argument("Surface clip distances must be finite and positive");
    }
    // Keep the first-person near plane well inside the eye's local clearance.
    // Reference-sphere altitude can be much larger on cliffs and must not be
    // used here or close terrain will be clipped away.
    return {static_cast<float>(std::clamp(surfaceClearance * 0.1, 0.00001, 0.1)),
            static_cast<float>(std::max(20.0, 2.0 * (sunDistance + sunRadius)))};
}

inline glm::mat4 sphereModel(const glm::vec3& position, float radius) {
    glm::mat4 model(1.0f);
    model = glm::translate(model, position);
    return glm::scale(model, glm::vec3(radius));
}

inline glm::mat4 perspectiveProjection(float fovDegrees, float aspect,
                                       ClipPlanes clip = {}) {
    return glm::perspective(glm::radians(fovDegrees), aspect,
                            clip.nearPlane, clip.farPlane);
}

inline glm::dvec3 reflectPointAcrossPlane(const glm::dvec3& point,
                                          const glm::dvec3& planePoint,
                                          const glm::dvec3& unitNormal) {
    return point - 2.0 * glm::dot(point - planePoint, unitNormal) * unitNormal;
}

inline glm::dvec3 reflectVectorAcrossPlane(const glm::dvec3& vector,
                                           const glm::dvec3& unitNormal) {
    return vector - 2.0 * glm::dot(vector, unitNormal) * unitNormal;
}

// Approximate the spherical ocean by its tangent plane beneath the eye. The
// reflection pass separately clips the terrain against the spherical sea.
inline glm::mat4 waterReflectionView(const glm::mat4& view,
                                     const glm::dvec3& eye,
                                     const glm::dvec3& planetCenter,
                                     double waterRadius) {
    const glm::dvec3 offset = eye - planetCenter;
    const double distance = glm::length(offset);
    if (!std::isfinite(distance) || distance <= 1e-12 ||
        !std::isfinite(waterRadius) || waterRadius <= 0.0) {
        throw std::invalid_argument("Water reflection requires a valid eye and radius");
    }
    const glm::dvec3 normal = offset / distance;
    const glm::dvec3 planePoint = planetCenter + waterRadius * normal;
    const glm::mat4 inverseView = glm::inverse(view);
    const glm::dvec3 forward = -glm::normalize(glm::dvec3(inverseView[2]));
    const glm::dvec3 up = glm::normalize(glm::dvec3(inverseView[1]));
    const glm::dvec3 reflectedEye = reflectPointAcrossPlane(eye, planePoint, normal);
    const glm::dvec3 reflectedForward = reflectVectorAcrossPlane(forward, normal);
    const glm::dvec3 reflectedUp = reflectVectorAcrossPlane(up, normal);
    return glm::lookAt(glm::vec3(reflectedEye),
                       glm::vec3(reflectedEye + reflectedForward),
                       glm::vec3(reflectedUp));
}

inline double directionalBrightness(const glm::dvec3& normal,
                                    const glm::dvec3& worldPosition,
                                    const glm::dvec3& sunPosition,
                                    double ambient = 0.12) {
    if (!std::isfinite(ambient) || ambient < 0.0 || ambient > 1.0 ||
        glm::length(normal) <= 1e-12 ||
        glm::length(sunPosition - worldPosition) <= 1e-12) {
        throw std::invalid_argument("Directional brightness requires valid vectors");
    }
    const double diffuse = std::max(0.0, glm::dot(
        glm::normalize(normal), glm::normalize(sunPosition - worldPosition)));
    return ambient + (1.0 - ambient) * diffuse;
}

} // namespace rendering

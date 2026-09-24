#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
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

inline glm::mat4 sphereModel(const glm::vec3& position, float radius,
                             const glm::mat3& orientation = glm::mat3(1.0f)) {
    glm::mat4 model(1.0f);
    model = glm::translate(model, position);
    model *= glm::mat4(orientation);
    return glm::scale(model, glm::vec3(radius));
}

inline glm::mat4 perspectiveProjection(float fovDegrees, float aspect,
                                       ClipPlanes clip = {}) {
    return glm::perspective(glm::radians(fovDegrees), aspect,
                            clip.nearPlane, clip.farPlane);
}

// Conservative image bounds of a sphere, from its enclosing cube. These let
// render diagnostics distinguish a white Sun from white background stars.
inline std::array<int, 4> spherePixelBounds(const glm::dvec3& center, double radius,
                                           const glm::dmat4& viewProjection,
                                           int width, int height) {
    if (!std::isfinite(radius) || radius <= 0.0 || width <= 0 || height <= 0)
        throw std::invalid_argument("Sphere image bounds require a positive radius and image size");
    glm::dvec2 minimum(std::numeric_limits<double>::infinity());
    glm::dvec2 maximum(-std::numeric_limits<double>::infinity());
    int inFront = 0;
    for (int x : {-1, 1}) for (int y : {-1, 1}) for (int z : {-1, 1}) {
        const glm::dvec4 clip = viewProjection * glm::dvec4(center + radius * glm::dvec3(x, y, z), 1);
        if (!std::isfinite(clip.x) || !std::isfinite(clip.y) || !std::isfinite(clip.w))
            throw std::invalid_argument("Sphere image projection must be finite");
        if (clip.w <= 0.0) continue;
        ++inFront;
        const glm::dvec2 ndc = glm::dvec2(clip) / clip.w;
        minimum = glm::min(minimum, ndc);
        maximum = glm::max(maximum, ndc);
    }
    if (inFront == 0) return {0, 0, -1, -1};
    if (inFront != 8) return {0, 0, width - 1, height - 1};
    if (maximum.x < -1 || minimum.x > 1 || maximum.y < -1 || minimum.y > 1)
        return {0, 0, -1, -1};
    minimum = glm::clamp(minimum, glm::dvec2(-1), glm::dvec2(1));
    maximum = glm::clamp(maximum, glm::dvec2(-1), glm::dvec2(1));
    return {std::max(0, static_cast<int>(std::floor((minimum.x + 1) * width / 2))),
            std::max(0, static_cast<int>(std::floor((1 - maximum.y) * height / 2))),
            std::min(width - 1, static_cast<int>(std::ceil((maximum.x + 1) * width / 2))),
            std::min(height - 1, static_cast<int>(std::ceil((1 - minimum.y) * height / 2)))};
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

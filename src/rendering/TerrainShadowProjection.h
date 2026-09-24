#pragma once

#include <cmath>
#include <stdexcept>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace rendering {

// Body-local coordinates keep the shadow depth independent of orbital distance.
// extent encloses the terrain in units of the planet's reference radius.
inline glm::dmat4 terrainShadowProjection(const glm::dvec3& towardSun, double extent) {
    const double length = glm::length(towardSun);
    if (!std::isfinite(length) || length <= 0.0 || !std::isfinite(extent) || extent <= 0.0)
        throw std::invalid_argument("Shadow projection needs a finite direction and positive extent");
    const glm::dvec3 direction = towardSun / length;
    const glm::dvec3 up = std::abs(direction.z) < 0.9 ? glm::dvec3(0, 0, 1) : glm::dvec3(0, 1, 0);
    const double radius = 1.01 * extent;
    return glm::ortho(-radius, radius, -radius, radius, radius, 3.0 * radius) *
           glm::lookAt(2.0 * radius * direction, glm::dvec3(0), up);
}

} // namespace rendering

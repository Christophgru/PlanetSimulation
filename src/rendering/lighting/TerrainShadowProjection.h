#pragma once

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace rendering {

// Invalidate against the last rendered direction, so small changes accumulate.
// The angular threshold bounds motion at the reference radius to one map texel.
class TerrainShadowCache {
public:
    bool updateNeeded(const glm::dvec3& sun,double extent,int resolution,std::uint64_t revision) {
        const auto direction=glm::normalize(sun);
        const bool changed=!valid_ || extent!=extent_ || resolution!=resolution_ || revision!=revision_ ||
            glm::length(direction-direction_) >= 2.0/resolution;
        if (changed) {
            valid_=true; direction_=direction; extent_=extent; resolution_=resolution; revision_=revision;
        }
        return changed;
    }
private:
    bool valid_=false;
    glm::dvec3 direction_{0};
    double extent_=0;
    int resolution_=0;
    std::uint64_t revision_=0;
};

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

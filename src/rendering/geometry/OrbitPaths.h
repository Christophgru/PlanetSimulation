#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>
#include <glm/glm.hpp>
#include "config/ScenarioConfig.h"
#include "simulation/OrbitalSystem.h"

namespace rendering {

struct OrbitTrail {
    std::vector<glm::dvec3> positions;
    std::vector<float> opacity;
    double periodSeconds = 0.0;
};

// Oldest samples are first and the current body position is last. The oldest
// 20% fades in, making a long trail end without a visible hard cut.
inline std::vector<OrbitTrail> pastOrbitTrails(
    const simulation::OrbitalSystem& system, double epoch,
    int revolutions = 10, int segmentsPerRevolution = 32) {
    if (!std::isfinite(epoch) || revolutions < 1 || segmentsPerRevolution < 3)
        throw std::invalid_argument("Invalid orbit trail sampling request");
    std::vector<OrbitTrail> trails(system.size() - 1);
    for (std::size_t body = 1; body < system.size(); ++body) {
        auto& trail = trails[body - 1];
        trail.periodSeconds = system.periodSeconds(body);
        const int segments = revolutions * segmentsPerRevolution;
        trail.positions.reserve(static_cast<std::size_t>(segments) + 1);
        trail.opacity.reserve(static_cast<std::size_t>(segments) + 1);
        for (int step = 0; step <= segments; ++step) {
            const double fraction = static_cast<double>(step) / segments;
            const double time = epoch - trail.periodSeconds *
                (static_cast<double>(segments - step) / segmentsPerRevolution);
            trail.positions.push_back(system.at(time)[body].position);
            const double fade = std::clamp(fraction / 0.2, 0.0, 1.0);
            trail.opacity.push_back(static_cast<float>(fade * fade * (3.0 - 2.0 * fade)));
        }
    }
    return trails;
}

// Terrain vertex RGB is the same multiplier used by the terrain shader.
// The configured base RGB supplies the remaining factor.
inline glm::vec3 averageSurfaceColor(const config::PlanetConfig& planet,
                                     const std::vector<float>& meshVertices,
                                     bool hasVertexColors) {
    glm::dvec3 factor(1.0);
    if (hasVertexColors && meshVertices.size() >= 9 && meshVertices.size() % 9 == 0) {
        factor = glm::dvec3(0.0);
        const std::size_t count = meshVertices.size() / 9;
        for (std::size_t i = 0; i < count; ++i)
            factor += glm::dvec3(meshVertices[9*i+6], meshVertices[9*i+7], meshVertices[9*i+8]);
        factor /= static_cast<double>(count);
    }
    return glm::vec3(glm::clamp(factor * glm::dvec3(
        planet.color[0], planet.color[1], planet.color[2]),
        glm::dvec3(0.0), glm::dvec3(1.0)));
}

} // namespace rendering

#pragma once
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>
namespace config { struct PlanetConfig; struct FoliageConfig; }

namespace rendering {
// Each instance is in the same body-local radius units as the terrain mesh.
// Sampling rendered triangles keeps roots on the visible ground at every LOD.
struct GrassBlade {
    glm::vec3 root;
    glm::vec3 up;
    glm::vec4 variation; // azimuth, lean, height multiplier, shade
};
std::uint32_t grassHash(std::uint32_t value);
double grassRandom(std::uint32_t& state);
double grassRebuildDistance(const config::FoliageConfig& settings);

std::vector<GrassBlade> placeGrass(const std::vector<float>& vertices,
        const std::vector<unsigned>& indices, const config::PlanetConfig& planet,
        double metersPerWorldUnit, const glm::dvec3& eyeBody);
} // namespace rendering

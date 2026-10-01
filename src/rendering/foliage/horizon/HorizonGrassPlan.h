#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>
namespace config { struct PlanetConfig; }

namespace rendering {
// One coarse triangle descriptor replaces many individually uploaded roots.
// The shader generates candidates from this seed; no CPU random root loop.
struct HorizonGrassPatch {
    glm::vec3 a, b, c, normal, color;
    float expectedCandidates = 0;
    std::uint32_t seed = 0;
};
static_assert(sizeof(HorizonGrassPatch) == 68);
inline constexpr std::array<int, 8> horizonGrassSlots{1,2,4,8,16,32,64,128};
struct HorizonGrassBatch { std::size_t first=0, count=0; };
struct HorizonGrassPlan {
    std::vector<HorizonGrassPatch> patches;
    std::array<HorizonGrassBatch, 8> batches{};
    std::size_t candidates=0;
    double distanceMeters=0;
};
double horizonGrassDistance(const config::PlanetConfig& planet, double metersPerWorldUnit,
                            const glm::dvec3& eyeBody);
HorizonGrassPlan planHorizonGrass(const std::vector<float>& vertices,
    const std::vector<unsigned>& indices, const config::PlanetConfig& planet,
    double metersPerWorldUnit, const glm::dvec3& eyeBody);
} // namespace rendering

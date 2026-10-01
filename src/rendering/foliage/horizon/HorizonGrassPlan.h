#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>
namespace config { struct PlanetConfig; }

namespace rendering {
// CPU-only patch geometry supports budgets, bounds and test oracles. The GPU
// receives only triangle IDs and reads geometry from the existing terrain.
struct HorizonGrassPatch {
    glm::vec3 a, b, c, normal, color;
    float expectedCandidates = 0;
    std::uint32_t triangle = 0;
};
static_assert(sizeof(HorizonGrassPatch) == 68);
inline constexpr std::array<int, 17> horizonGrassSlots{1,2,4,8,16,32,64,128,256,512,1024,2048,4096,8192,16384,32768,65536};
struct HorizonGrassBatch { std::size_t first=0, count=0; };
struct HorizonGrassPlan {
    std::vector<HorizonGrassPatch> patches;
    std::array<HorizonGrassBatch, horizonGrassSlots.size()> batches{};
    std::size_t candidates=0;
    double distanceMeters=0;
    double density=0;
};
double horizonGrassDistance(const config::PlanetConfig& planet, double metersPerWorldUnit,
                            const glm::dvec3& eyeBody);
HorizonGrassPlan planHorizonGrass(const std::vector<float>& vertices,
    const std::vector<unsigned>& indices, const config::PlanetConfig& planet,
    double metersPerWorldUnit, const glm::dvec3& eyeBody, bool detailed=false);
} // namespace rendering

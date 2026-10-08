#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>
#include "rendering/foliage/planning/GrassBudget.h"
namespace config { struct PlanetConfig; }

namespace rendering {
// CPU-only patch geometry supports budgets, bounds and test oracles. The GPU
// receives only triangle IDs and reads geometry from the existing terrain.
struct ProceduralGrassPatch {
    glm::vec3 a, b, c, normal, color;
    float expectedCandidates = 0;
    std::uint32_t triangle = 0;
};
static_assert(sizeof(ProceduralGrassPatch) == 68);
inline constexpr std::array<int, 17> grassCandidateSlots{1,2,4,8,16,32,64,128,256,512,1024,2048,4096,8192,16384,32768,65536};
struct ProceduralGrassBatch { std::size_t first=0, count=0; };
struct GrassPlan {
    std::vector<ProceduralGrassPatch> patches;
    std::array<ProceduralGrassBatch, grassCandidateSlots.size()> batches{};
    std::size_t candidates=0;
    double distanceMeters=0;
    double density=0;
    GrassFalloff falloff;
};
// CPU geometry collected with the same conservative reach/movement bounds as
// resident metadata. Allocation consumes raw area in protected mode.
struct GrassPlanTriangle {
    ProceduralGrassPatch patch;
    double area=0,distance=0;
    int level=0;
    double minimumDistance=0;
};
GrassPlan allocateProtectedGrass(std::vector<GrassPlanTriangle> triangles,
    const config::FoliageConfig& settings,GrassFalloff policy);
GrassPlan planGrass(const std::vector<float>& vertices,
    const std::vector<unsigned>& indices, const config::PlanetConfig& planet,
    double metersPerWorldUnit, const glm::dvec3& eyeBody,const GrassFalloff& policy={});
} // namespace rendering

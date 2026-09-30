#pragma once
#include "rendering/foliage/GrassPlacement.h"
#include <array>
#include <cstddef>

namespace rendering {
inline constexpr std::array<int, 8> grassLodSegments{6,5,4,3,2,1,1,1};
// A strip ends in one shared tip, not two coincident tip vertices.
inline constexpr int grassLodVertices(int level) { return 2*grassLodSegments[level]+1; }
double grassLodNearDistance(double drawDistance);
double grassLodFadeEnd(float variation, double drawDistance);
int grassLodLevel(double distance, double drawDistance);
struct GrassLodBatch { std::size_t first=0, count=0; };
struct GrassLodPlan {
    std::vector<GrassBlade> blades;
    std::array<GrassLodBatch, 8> batches{};
};
// Guards cover camera movement until the next patch rebuild. CPU rejection
// removes only blades whose shader sinking transition is already complete.
GrassLodPlan batchGrass(const std::vector<GrassBlade>& blades, const glm::dvec3& eye,
                       double metersPerRadius, double drawDistance, double movementMargin);
}

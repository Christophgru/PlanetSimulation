#pragma once
#include "rendering/foliage/GrassPlacement.h"
#include <array>
#include <cstddef>

namespace rendering {
inline constexpr std::array<int, 8> grassLodSegments{6,6,6,6,6,1,1,1};
// Four vertices form the low quad; a narrow top keeps both triangles useful.
inline constexpr int grassLodVertices(int level) { return 2*grassLodSegments[level]+2; }
double grassLodNearDistance(double drawDistance);
double grassLodFadeEnd(float variation, double drawDistance);
int grassLodLevel(double distance, double drawDistance);
struct GrassLodBatch { std::size_t first=0, count=0; };
struct GrassLodPlan {
    std::vector<GrassBlade> blades;
    std::array<GrassLodBatch, 8> batches{};
};
// Guards cover camera movement until the next patch rebuild. CPU rejection
// removes only blades whose coverage fade is already complete. This CPU
// reference remains for comparison; production roots are generated in GLSL.
GrassLodPlan batchGrass(const std::vector<GrassBlade>& blades, const glm::dvec3& eye,
                       double metersPerRadius, double drawDistance, double movementMargin);
}

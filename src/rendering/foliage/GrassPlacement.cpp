#include "rendering/foliage/GrassPlacement.h"
#include "rendering/diagnostics/tracing/CpuTrace.h"
#include "config/ScenarioConfig.h"
#include "rendering/geometry/Terrain.h"
#include <algorithm>
#include <cmath>

namespace rendering {
std::uint32_t grassHash(std::uint32_t value) {
    value ^= value >> 16; value *= 0x7feb352du;
    value ^= value >> 15; value *= 0x846ca68bu;
    return value ^ (value >> 16);
}

double grassRandom(std::uint32_t& state) {
    state = grassHash(state + 0x9e3779b9u);
    return double(state) / 4294967296.0;
}

double grassRebuildDistance(const config::FoliageConfig& settings) {
    return settings.draw_distance_m * 0.15;
}

std::vector<GrassBlade> placeGrass(const std::vector<float>& vertices,
        const std::vector<unsigned>& indices, const config::PlanetConfig& planet,
        double metersPerWorldUnit, const glm::dvec3& eyeBody) {
    CpuTrace::Scope scope("placeGrass");
    std::vector<GrassBlade> result;
    const auto& settings = planet.foliage;
    if (!settings.enabled || vertices.empty()) return result;
    const double metersPerRadius = planet.radius * metersPerWorldUnit;
    const double radius = settings.draw_distance_m + grassRebuildDistance(settings);
    double highestGround=planet.terrain_landscape.maximumAbsoluteHeightMeters();
    for (const auto& noise:planet.surface_noise) highestGround+=noise.amplitude_m;
    if ((glm::length(eyeBody)-1)*metersPerRadius-highestGround>radius) return result;
    // A three-sigma patch concentrates instances around the walker instead
    // of spending the same density on barely visible distant grass. Integrate
    // the truncated Gaussian to scale its peak to the instance budget.
    const double sigma = settings.draw_distance_m / 3.0;
    const double variance2 = 2.0 * sigma * sigma;
    const double weightedArea = std::acos(-1.0) * variance2 *
        (1.0 - std::exp(-radius * radius / variance2));
    const double density = std::min(settings.density_per_m2,
        0.8 * settings.max_blades / weightedArea);
    result.reserve(settings.max_blades);
    const auto rockRange = planet.terrain_material.slopeMetricRange();
    std::uint32_t accepted = 0, reservoir = grassHash(settings.seed);
    for (std::size_t triangle = 0; triangle + 2 < indices.size(); triangle += 3) {
        glm::dvec3 p[3], normal[3], color[3];
        for (int c = 0; c < 3; ++c) {
            const auto offset = std::size_t(indices[triangle+c]) * 9;
            for (int axis = 0; axis < 3; ++axis) {
                p[c][axis] = vertices.at(offset+axis);
                normal[c][axis] = vertices.at(offset+3+axis);
                color[c][axis] = vertices.at(offset+6+axis) * planet.color[axis];
            }
        }
        const glm::dvec3 center = (p[0]+p[1]+p[2])/3.0;
        const double bound = std::max({glm::length(p[0]-center),glm::length(p[1]-center),glm::length(p[2]-center)});
        const double minimumDistance = std::max(0.0,
            (glm::length(center-eyeBody)-bound) * metersPerRadius);
        if (minimumDistance > radius) continue;
        // The bounding sphere contains the entire triangle, so this is an
        // upper bound on every candidate's Gaussian acceptance probability.
        // Reject against it before doing barycentric geometry or exp per root.
        const double maximumAcceptance = std::exp(-minimumDistance*minimumDistance / variance2);
        const double area = 0.5 * glm::length(glm::cross(p[1]-p[0],p[2]-p[0])) * metersPerRadius * metersPerRadius;
        std::uint32_t random = grassHash(std::uint32_t(triangle/3) ^ std::uint32_t(settings.seed));
        // Bound work for coarse orbital triangles intersecting the patch.
        const int count = int(std::min(8192.0,std::floor(area*density + grassRandom(random))));
        const auto triangleSeed=random;
        for (int blade = 0; blade < count; ++blade) {
            // Rejection by the camera must not move the following candidates.
            random=grassHash(triangleSeed+std::uint32_t(blade));
            // Keep the original random stream and candidate indices intact:
            // accepted roots, attributes and reservoir order remain identical.
            const double areaSample = grassRandom(random), b = grassRandom(random);
            const double acceptanceSample = grassRandom(random);
            if (acceptanceSample >= maximumAcceptance) continue;
            const double a = std::sqrt(areaSample);
            const glm::dvec3 weights(1-a,a*(1-b),a*b);
            const glm::dvec3 root = p[0]*weights.x+p[1]*weights.y+p[2]*weights.z;
            const double distanceMeters = glm::length(root-eyeBody)*metersPerRadius;
            if (distanceMeters > radius || glm::length(root) < 1e-9) continue;
            if (acceptanceSample >= std::exp(-distanceMeters*distanceMeters / variance2)) continue;
            const glm::dvec3 radial = glm::normalize(root);
            const auto n = glm::normalize(normal[0]*weights.x+normal[1]*weights.y+normal[2]*weights.z);
            auto tint = color[0]*weights.x+color[1]*weights.y+color[2]*weights.z;
            if (planet.terrain_landscape.enabled) {
                const double cosine=std::clamp(glm::dot(n,radial),1e-6,1.0);
                const double riseOverRun=std::sqrt(1.0-cosine*cosine)/cosine;
                tint=TerrainSurface::landscapeColorFactors((glm::length(root)-1)*metersPerRadius,
                    riseOverRun,planet.water.enabled ? planet.water.level_m : 0.0,0.1,highestGround,
                    planet.terrain_material)*glm::dvec3(planet.color[0],planet.color[1],planet.color[2]);
            }
            // Only the green biome grows grass; beaches, snow and airless gray
            // bodies are excluded. Thin coverage smoothly through the rock blend.
            if (tint.y <= 1.15 * std::max(tint.x,tint.z)) continue;
            const double slope = 1.0 - glm::dot(n,radial);
            const double t = std::clamp((slope-rockRange[0])/(rockRange[1]-rockRange[0]),0.0,1.0);
            if (grassRandom(random) < t*t*(3-2*t)) continue;
            if (planet.water.enabled && (glm::length(root)-1)*metersPerRadius <= planet.water.level_m+0.15) continue;
            GrassBlade instance{glm::vec3(root + n*(0.005/metersPerRadius)),glm::vec3(radial),
                glm::vec4(grassRandom(random)*6.28318530718,0.1+0.3*grassRandom(random),
                          0.75+0.75*grassRandom(random),grassRandom(random))};
            // Reservoir sampling keeps the hard instance cap unbiased by mesh
            // traversal order when unusually folded terrain exceeds the estimate.
            ++accepted;
            if (result.size() < std::size_t(settings.max_blades)) result.push_back(instance);
            else {
                const auto slot = std::uint32_t(grassRandom(reservoir)*accepted);
                if (slot < result.size()) result[slot] = instance;
            }
        }
    }
    return result;
}
} // namespace rendering

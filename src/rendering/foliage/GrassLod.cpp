#include "rendering/foliage/GrassLod.h"
#include "rendering/diagnostics/tracing/CpuTrace.h"
#include <algorithm>
#include <cmath>

namespace rendering {
double grassLodNearDistance(double drawDistance) { return std::min(15.0, drawDistance*.25); }
double grassLodFadeEnd(float variation, double drawDistance) {
    // Match grass.vert's integer hash. This decorrelates retention from tint
    // and stays stable across patch movement and front-to-back sorting.
    const auto group=grassHash(static_cast<std::uint32_t>(variation*65536.0f)) & 7u;
    const double near=grassLodNearDistance(drawDistance);
    return near+(group+1)*(drawDistance-near)/8.0;
}
int grassLodLevel(double distance, double drawDistance) {
    const double near=grassLodNearDistance(drawDistance);
    const double detailed=near*.5;
    if (distance<detailed) return 0;
    if (distance<near) return 1+static_cast<int>((distance-detailed)*4/(near-detailed));
    return static_cast<int>(std::clamp(5.0+std::floor((distance-near)*3/(drawDistance-near)),5.0,7.0));
}
GrassLodPlan batchGrass(const std::vector<GrassBlade>& blades, const glm::dvec3& eye,
                       double metersPerRadius, double drawDistance, double movementMargin) {
    CpuTrace::Scope scope("batchGrass");
    struct Key { std::size_t index; double distance; };
    std::vector<Key> keys;
    keys.reserve(blades.size());
    for (std::size_t i=0; i<blades.size(); ++i) {
        const double distance=glm::length(glm::dvec3(blades[i].root)-eye)*metersPerRadius;
        const double closest=std::max(0.0,distance-movementMargin);
        // Small float guard also covers CPU/GPU rounding at the endpoint.
        if (closest >= grassLodFadeEnd(blades[i].variation.w,drawDistance)+.001) continue;
        keys.push_back({i,distance});
    }
    std::sort(keys.begin(),keys.end(),[](const auto& a,const auto& b) {
        return a.distance<b.distance;
    });
    GrassLodPlan result;
    result.blades.reserve(keys.size());
    for (const auto& key:keys) {
        const int level=grassLodLevel(std::max(0.0,key.distance-movementMargin),drawDistance);
        auto& batch=result.batches[level];
        if (!batch.count) batch.first=result.blades.size();
        ++batch.count;
        result.blades.push_back(blades[key.index]);
    }
    return result;
}
} // namespace rendering

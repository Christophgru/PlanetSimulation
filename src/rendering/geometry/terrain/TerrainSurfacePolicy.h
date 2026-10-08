#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>

namespace rendering {
// A generation-wide std430 pack: no per-vertex morph data or CPU height uploads.
// Zero radius selects the historical, unfiltered field.
struct alignas(32) TerrainSurfacePolicy {
    std::array<double,4> eyeEdge{}; // local eye radial (zero in orbit), base edge metres
    std::array<double,4> distances{}; // near, middle, radius metres, maximum sink
    std::array<double,8> spacing{}; // parent sample spacing, coarse to fine
    bool enabled() const { return distances[2]>0; }
    glm::dvec3 profile(const glm::dvec3& radial) const {
        if(!enabled()) return {};
        if(eyeEdge[0]==0 && eyeEdge[1]==0 && eyeEdge[2]==0) return {spacing[0],spacing[0],0};
        const double distance=distances[2]*std::acos(std::clamp(
            glm::dot(radial,glm::dvec3(eyeEdge[0],eyeEdge[1],eyeEdge[2])),-1.0,1.0));
        const double level=7*(1-std::clamp((distance-distances[0])/(distances[1]-distances[0]),0.0,1.0));
        const int parent=static_cast<int>(std::floor(level)),fine=std::min(7,parent+1);
        double blend=1-(level-parent);blend=blend*blend*(3-2*blend);
        return {spacing[fine],spacing[parent],blend};
    }
    static double weight(double frequency,double radiusMeters,const glm::dvec3& profile) {
        const auto resolved=[&](double spacing) {
            double t=std::clamp((frequency*spacing/radiusMeters-.25)/.25,0.0,1.0);
            return 1-t*t*(3-2*t);
        };
        return std::lerp(resolved(profile.x),resolved(profile.y),profile.z);
    }
    bool operator==(const TerrainSurfacePolicy&) const = default;
};
static_assert(sizeof(TerrainSurfacePolicy)==128);
}

#include "rendering/foliage/procedural/GrassPlan.h"
#include "rendering/foliage/GrassPlacement.h"
#include "rendering/diagnostics/tracing/CpuTrace.h"
#include "config/ScenarioConfig.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>

namespace rendering {
namespace {
double maximumGround(const config::PlanetConfig& planet) {
    double height=planet.terrain_landscape.maximumAbsoluteHeightMeters();
    for (const auto& noise:planet.surface_noise) height+=noise.amplitude_m;
    return height;
}
int bucket(double expected,int maxSlots) {
    int level=0;
    while (level+1<int(grassCandidateSlots.size()) && grassCandidateSlots[level]<maxSlots && grassCandidateSlots[level]<expected) ++level;
    return level;
}
}

GrassPlan planGrass(const std::vector<float>& vertices,
    const std::vector<unsigned>& indices, const config::PlanetConfig& planet,
    double metersPerWorldUnit, const glm::dvec3& eyeBody) {
    CpuTrace::Scope scope("planGrass");
    GrassPlan result;
    const auto& settings=planet.foliage;
    if (!settings.enabled || vertices.empty()) return result;
    const double scale=planet.radius*metersPerWorldUnit;
    if (!std::isfinite(scale) || scale<=0 || !std::isfinite(glm::length(eyeBody)) || glm::length(eyeBody)<=0)
        throw std::invalid_argument("Invalid grass scale or eye");
    result.distanceMeters=settings.draw_distance_m;
    const double margin=grassRebuildDistance(settings);
    const double variance2=2*std::pow(settings.draw_distance_m*settings.gaussian_sigma_fraction,2);
    const double weightedArea=std::acos(-1.0)*variance2*(1-std::exp(-std::pow(result.distanceMeters+margin,2)/variance2));
    const double requestedDensity=std::min(settings.density_per_m2,
        settings.budget_fraction*settings.max_blades/weightedArea);
    const int slotCap=int(std::bit_floor(unsigned(settings.max_candidates_per_triangle)));
    const double maximumHeight=maximumGround(planet);
    const auto rockRange=planet.terrain_material.slopeMetricRange();
    const double water=planet.water.enabled ? planet.water.level_m : 0;
    const double relief=std::max(1.0,maximumHeight-water);
    const double snowStart=std::max(water+1.1,water+.25*relief);
    const double snowEnd=std::max(snowStart+1,water+.4*relief);
    if ((glm::length(eyeBody)-1)*scale-maximumHeight>result.distanceMeters+margin) return result;
    struct Candidate { ProceduralGrassPatch patch; double area, distance; int level=0; };
    std::vector<Candidate> candidates;
    double totalArea=0;
    for (std::size_t t=0;t+2<indices.size();t+=3) {
        glm::dvec3 p[3], n[3], color[3];
        for (int i=0;i<3;++i) {
            const std::size_t v=std::size_t(indices[t+i])*9;
            for (int c=0;c<3;++c) {
                p[i][c]=vertices.at(v+c);
            }
        }
        const auto center=(p[0]+p[1]+p[2])/3.0;
        const double reach=std::max({glm::length(p[0]-center),glm::length(p[1]-center),glm::length(p[2]-center)});
        const double distance=glm::length(center-eyeBody)*scale;
        if (distance-reach*scale>result.distanceMeters+margin) continue;
        const double area=glm::length(glm::cross(p[1]-p[0],p[2]-p[0]))*.5*scale*scale;
        if (!std::isfinite(area) || area<=1e-10 || glm::length(center)<1e-9) continue;
        for (int i=0;i<3;++i) {
            const std::size_t v=std::size_t(indices[t+i])*9;
            for (int c=0;c<3;++c) {
                n[i][c]=vertices.at(v+3+c); color[i][c]=vertices.at(v+6+c)*planet.color[c];
            }
        }
        if (glm::length(n[0]+n[1]+n[2])<1e-9) continue;
        const auto tint=(color[0]+color[1]+color[2])/3.0;
        const auto normal=glm::normalize(n[0]+n[1]+n[2]);
        const auto radial=glm::normalize(center);
        const double radialReach=2*reach/std::max(1e-9,glm::length(center)-reach);
        // Conservative patch bounds avoid paying one descriptor per tiny
        // water/snow/cliff triangle. Remaining mixed patches reject per root.
        if (1-glm::dot(normal,radial)-radialReach>=rockRange[1]) continue;
        const double highest=(std::max({glm::length(p[0]),glm::length(p[1]),glm::length(p[2])})-1)*scale;
        if (planet.water.enabled && highest<=planet.water.level_m+settings.water_clearance_m) continue;
        if (planet.terrain_landscape.enabled) {
            const auto faceNormal=glm::normalize(glm::cross(p[1]-p[0],p[2]-p[0]));
            const double lowest=(std::abs(glm::dot(faceNormal,p[0]))-1)*scale;
            if (lowest>=snowEnd) continue;
        }
        // Landscape biome/altitude rejection is per GPU root. For bodies
        // without a landscape, their triangle tint is the coarse classifier.
        if (!planet.terrain_landscape.enabled && tint.y<=settings.green_ratio*std::max(tint.x,tint.z)) continue;
        ProceduralGrassPatch patch{glm::vec3(p[0]),glm::vec3(p[1]),glm::vec3(p[2]),
            glm::vec3(normal),glm::vec3(tint),0,
            std::uint32_t(t/3)};
        // Reserve a conservative Gaussian bound for camera movement. The GPU
        // evaluates actual density against stable candidate ranks.
        const double minimumDistance=std::max(0.0,distance-reach*scale-margin);
        const double weighted=area*std::exp(-minimumDistance*minimumDistance/variance2);
        if (weighted*requestedDensity<1e-6) continue;
        candidates.push_back({patch,weighted,distance}); totalArea+=weighted;
    }
    const std::size_t budget=std::size_t(settings.max_blades);
    if (candidates.size()>budget) {
        // A very small explicit budget cannot represent every triangle. Keep
        // a seed-stable sample instead of cutting coverage at a near distance.
        std::sort(candidates.begin(),candidates.end(),[](const auto& a,const auto& b) { return grassHash(a.patch.triangle)<grassHash(b.patch.triangle); });
        candidates.resize(budget);
        totalArea=0; for (const auto& candidate:candidates) totalArea+=candidate.area;
    }
    if (candidates.empty()) return result;
    double low=0, high=requestedDensity;
    // Sorted areas turn each budget probe into binary searches over slot thresholds instead
    // of another full candidate scan. Keep the multiplication/comparison used
    // by bucket(), including exact power-of-two boundaries.
    std::vector<double> areas;
    areas.reserve(candidates.size());
    for (const auto& candidate:candidates) areas.push_back(candidate.area);
    std::sort(areas.begin(),areas.end());
    const auto submitted=[&](double density) {
        std::size_t count=areas.size();
        for (int slots:grassCandidateSlots) {
            if (slots>=slotCap || slots==grassCandidateSlots.back()) break;
            const auto first=std::upper_bound(areas.begin(),areas.end(),double(slots),
                [density](double limit,double area) { return limit<area*density; });
            count+=std::size_t(areas.end()-first)*slots;
        }
        return count;
    };
    if (submitted(high)>budget) {
        // Account for rounded-up power-of-two slots, including one slot on
        // tiny triangles. This is a hard submitted-work cap, not an average.
        high=std::min(high,double(budget)/totalArea);
        for (int i=0;i<32;++i) {
            const double middle=(low+high)*.5;
            if (submitted(middle)<=budget) low=middle; else high=middle;
        }
        high=low;
    }
    std::stable_sort(candidates.begin(),candidates.end(),[](const auto& a,const auto& b) { return a.distance<b.distance; });
    for (auto& candidate:candidates) {
        candidate.level=bucket(candidate.area*high,slotCap);
        ++result.batches[candidate.level].count;
    }
    std::array<std::size_t,grassCandidateSlots.size()> next{};
    std::size_t patchCount=0;
    for (std::size_t level=0;level<grassCandidateSlots.size();++level) {
        auto& batch=result.batches[level]; batch.first=patchCount;
        next[level]=patchCount; patchCount+=batch.count;
        result.candidates+=batch.count*grassCandidateSlots[level];
    }
    result.patches.resize(patchCount);
    for (auto& candidate:candidates) {
        candidate.patch.expectedCandidates=float(std::min(candidate.area*high,double(grassCandidateSlots[candidate.level])));
        result.patches[next[candidate.level]++]=candidate.patch;
    }
    result.density=high;
    if (result.candidates>budget) throw std::logic_error("Grass candidate budget exceeded");
    return result;
}
} // namespace rendering

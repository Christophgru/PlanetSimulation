#include "rendering/foliage/procedural/GrassPlan.h"
#include "rendering/foliage/GrassPlacement.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>

namespace rendering {
GrassPlan allocateProtectedGrass(std::vector<GrassPlanTriangle> triangles,
    const config::FoliageConfig& settings,GrassFalloff policy) {
    settings.validate();
    if(!policy.enabled || !policy.capacity || policy.capacity>unsigned(settings.max_blades) ||
       !policy.budget || policy.budget>policy.capacity)
        throw std::invalid_argument("Invalid protected grass allocation budget");
    const unsigned cap=std::bit_floor(unsigned(settings.max_candidates_per_triangle));
    // GPU metadata transports the protected radius as a float.
    const double protectedMeters=float(policy.protectedMeters);
    const auto near=[&](const auto& t) {return t.minimumDistance<=protectedMeters;};
    std::erase_if(triangles,[](const auto& t) {return t.area<1e-12;});
    const auto nearCount=std::count_if(triangles.begin(),triangles.end(),near);
    if(std::size_t(nearCount)>policy.capacity) {
        std::erase_if(triangles,[&](const auto& t) {return !near(t);});
        std::sort(triangles.begin(),triangles.end(),[](const auto& a,const auto& b) {
            return grassHash(a.patch.triangle)<grassHash(b.patch.triangle);
        });
        triangles.resize(policy.capacity);
    }
    const auto profile=[&](const auto& t,double sigma) {
        const double distance=std::max(0.,t.minimumDistance-protectedMeters);
        return distance==0 ? 1. : sigma<=0 ? 0. : std::exp(-distance*distance/(2*sigma*sigma));
    };
    const auto level=[&](double expected) {
        unsigned l=0;
        while(l<16 && (1u<<l)<cap && double(1u<<l)<expected) ++l;
        return l;
    };
    const auto work=[&](double density,double sigma) {
        std::uint64_t count=0;
        for(const auto& t:triangles) {
            const double expected=t.area*density*profile(t,sigma);
            if(expected>=1e-6) count+=1u<<level(expected);
        }
        return count;
    };
    double density=policy.locked?policy.density:settings.density_per_m2;
    double sigma=policy.sigmaMeters;
    const auto nearWork=work(density,0);
    policy.nearInfeasible=(policy.locked && policy.nearInfeasible) ||
        std::size_t(nearCount)>policy.capacity || nearWork>policy.capacity ||
        std::any_of(triangles.begin(),triangles.end(),[&](const auto& t) {
            return near(t) && t.area*density>cap;
        });
    if(!policy.locked) {
        if(nearWork>policy.capacity) {
            sigma=0;
            double low=0,high=density;
            for(int i=0;i<32;++i) {
                const double middle=(low+high)*.5;
                if(work(middle,0)<=policy.capacity) low=middle;else high=middle;
            }
            density=low;policy.budget=policy.capacity;
        } else {
            policy.budget=std::min<std::uint64_t>(policy.capacity,std::max<std::uint64_t>(policy.budget,nearWork));
            // Same 32 probes as resident allocation: first test the full tail,
            // then bisect. Configured near density outranks the soft quota.
            double low=0,high=sigma,probe=sigma;
            for(int i=0;i<32;++i) {
                if(work(density,probe)<=policy.budget) low=probe;else high=probe;
                probe=(low+high)*.5;
            }
            sigma=low;
        }
    }
    policy.density=density;policy.sigmaMeters=sigma;
    std::erase_if(triangles,[&](const auto& t) {return t.area*density*profile(t,sigma)<1e-6;});
    std::sort(triangles.begin(),triangles.end(),[](const auto& a,const auto& b) {
        return a.distance<b.distance || (a.distance==b.distance && a.patch.triangle<b.patch.triangle);
    });
    GrassPlan result;result.density=density;result.distanceMeters=settings.draw_distance_m;result.falloff=policy;
    for(auto& t:triangles) {
        t.level=level(t.area*density*profile(t,sigma));
        ++result.batches[t.level].count;
    }
    std::array<std::size_t,17> next{};
    std::size_t patches=0;
    for(unsigned l=0;l<17;++l) {
        result.batches[l].first=next[l]=patches;
        patches+=result.batches[l].count;
        result.candidates+=result.batches[l].count*(1u<<l);
    }
    if(result.candidates>policy.budget) throw std::runtime_error("Protected foliage replay/allocation exceeds budget");
    result.patches.resize(patches);
    for(auto& t:triangles) {
        t.patch.expectedCandidates=float(std::min(t.area*density*profile(t,sigma),double(1u<<t.level)));
        result.patches[next[t.level]++]=t.patch;
    }
    return result;
}
} // namespace rendering

#include "rendering/foliage/procedural/ProceduralGrass.h"
#include "config/ScenarioConfig.h"
#include <algorithm>
#include <stdexcept>

namespace rendering {
void ProceduralGrass::admitCpuTerrain(std::uint64_t bytes) const {
    if(!adaptiveBudget_) return;
    constexpr std::uint64_t totalLimit=1024ull*1024*1024;
    const auto reserved=reservedBytes();
    const auto allowance=budget_.available(reserved,
        std::min(defaultStageBytes,totalLimit-std::min(totalLimit,reserved)));
    if(bytes>allowance) throw std::runtime_error("CPU terrain exceeds available staging memory");
}
std::uint64_t ProceduralGrass::ownedBytes() const {
    std::uint64_t bytes=0;
    for(const auto& patch:patches_) if(patch) {
        // CPU planning reserves worst-case output queues even on GL 3.3, where
        // direct instancing does not allocate them. This also bounds a later
        // compute-placement draw without another admission decision.
        bytes+=patch->stageAdmittedBytes ? patch->stageAdmittedBytes :
            patch->gpuCapacity*128+patch->patches*4+(patch->commands?32:0);
    }
    return bytes;
}
GrassFalloff ProceduralGrass::cpuPolicy(const config::PlanetConfig& planet,std::size_t triangles) const {
    if(!adaptiveBudget_ || !planet.foliage.enabled) return {};
    constexpr std::uint64_t totalLimit=1024ull*1024*1024;
    const auto reserved=reservedBytes();
    const auto available=budget_.available(reserved,
        std::min(defaultStageBytes,totalLimit-std::min(totalLimit,reserved)));
    const auto fixed=triangles*4+32;
    if(available<fixed+128) throw std::runtime_error("Insufficient CPU foliage staging budget");
    std::uint64_t blockBytes=defaultStageBytes;
    if(GLEW_VERSION_4_3 && planet.foliage.compute_placement) {
        GLint64 block=0;glGetInteger64v(GL_MAX_SHADER_STORAGE_BLOCK_SIZE,&block);
        if(block<=0) throw std::runtime_error("Invalid foliage storage limit");
        blockBytes=block;
    }
    const auto capacity=std::min<std::uint64_t>({std::uint64_t(planet.foliage.max_blades),blockBytes/128,(available-fixed)/128});
    if(const auto saved=replayPolicies_.find(planet.name);saved!=replayPolicies_.end()) {
        if(saved->second.capacity>capacity) throw std::runtime_error("Locked foliage replay exceeds available staging memory");
        return saved->second;
    }
    GrassFalloff p;p.enabled=true;p.capacity=capacity;
    p.budget=budget_.softBudget(p.capacity,planet.foliage);
    p.protectedMeters=planet.foliage.quadDistanceMeters();
    p.sigmaMeters=planet.foliage.draw_distance_m*planet.foliage.gaussian_sigma_fraction;
    p.density=planet.foliage.density_per_m2;
    return p;
}
}

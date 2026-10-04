#pragma once
#include "rendering/geometry/compute/TerrainCompute.h"
#include "rendering/geometry/jobs/TerrainBuild.h"

namespace rendering {
// One context-owned land/water submission. Grass uses the same ordered context;
// its worst-case resources are reserved by the logical admission ledger.
struct TerrainGpuPreparation {
    TerrainBuildIdentity identity;
    TerrainCpuBuild cpu;
    std::unique_ptr<TerrainComputeBuffers> land,water;
    std::uint64_t admittedBytes=0;
    bool ready=false;
    static std::uint64_t requiredBytes(const TerrainCpuBuild& build,const TerrainBuildIdentity& request,
        const config::PlanetConfig& planet,const TerrainComputeLimits& limits);
    TerrainGpuPreparation(TerrainCpuBuild build,TerrainBuildIdentity request,
        const config::PlanetConfig& planet,TerrainCompute& compute,std::uint64_t byteLimit=512ull*1024*1024);
    bool poll();
    void waitForCapture();
};
}

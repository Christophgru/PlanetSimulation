#include "rendering/geometry/compute/TerrainGpuPreparation.h"
#include "rendering/diagnostics/timing/GpuWorkProfiler.h"
#include "rendering/geometry/contacts/SparseTerrainContacts.h"
#include "rendering/foliage/procedural/ProceduralGrass.h"
#include <stdexcept>

namespace rendering {
namespace {
std::uint64_t terrainBytes(const TerrainTopology& t) {
    t.validate();
    return 704+t.samples.size()*68ull+t.indices.size()*44ull+8;
}
}
std::uint64_t TerrainGpuPreparation::requiredBytes(const TerrainCpuBuild& cpu,const TerrainBuildIdentity& identity,
    const config::PlanetConfig& planet,const TerrainComputeLimits& limits) {
    if(!identity.epoch || !identity.serial || identity.fieldVersion!=PlanetField::version || identity.topologyVersion!=1 ||
       identity.backend!=TerrainBackend::Compute || identity.bodyName!=planet.name || !cpu.field || !cpu.topology ||
       !cpu.contacts || cpu.field->fingerprint()!=identity.field || cpu.geometry.generation!=cpu.topology->generation ||
       bool(cpu.water)!=planet.water.enabled ||
       (cpu.water && (!cpu.waterField || !cpu.waterTopology || cpu.water->generation!=cpu.waterTopology->generation)))
        throw std::invalid_argument("Missing matching staged terrain consumers");
    auto key=cpu.topology->generation;key.backend=TerrainBackend::Compute;
    if(cpu.contacts->generation()!=key) throw std::invalid_argument("Stale staged terrain contacts");
    auto bytes=terrainBytes(*cpu.topology);
    if(cpu.water) bytes+=terrainBytes(*cpu.waterTopology);
    if(identity.resident) bytes+=ProceduralGrass::stageBytes(cpu.topology->triangleCount(),planet.foliage,limits.blockBytes);
    return bytes;
}
TerrainGpuPreparation::TerrainGpuPreparation(TerrainCpuBuild build,TerrainBuildIdentity request,
    const config::PlanetConfig& planet,TerrainCompute& compute,std::uint64_t byteLimit)
    :identity(std::move(request)),cpu(std::move(build)) {
    admittedBytes=requiredBytes(cpu,identity,planet,compute.limits());
    if(admittedBytes>byteLimit) throw std::runtime_error("Terrain consumers exceed logical staging byte limit");
    GpuWorkProfiler::Request timingIdentity(identity.epoch,identity.serial,identity.bodyIndex);
    land=compute.generate(*cpu.field,*cpu.topology);
    if(cpu.water) water=compute.generate(*cpu.waterField,*cpu.waterTopology);
    land->contacts=std::move(cpu.contacts);land->stats.gpuStageAdmittedBytes=admittedBytes;
}
bool TerrainGpuPreparation::poll() {
    if(ready) return true;
    const bool landReady=land->poll(),waterReady=!water || water->poll();
    ready=landReady && waterReady;return ready;
}
void TerrainGpuPreparation::waitForCapture() {
    land->waitForCapture();if(water) water->waitForCapture();
    if(!poll()) throw std::logic_error("Capture terrain consumers are incomplete");
}
}

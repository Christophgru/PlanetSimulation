#pragma once
#include "rendering/geometry/compute/TerrainCompute.h"
#include "rendering/foliage/planning/GrassMetadata.h"
#include "config/FoliageConfig.h"
namespace rendering {
// Fixed-size capture summary. No triangle/vertex data returns to the CPU.
struct alignas(32) GrassAllocationSummary {
    std::array<double,4> densitySearch{};
    std::array<std::uint32_t,4> control{}; // effective budget, slot cap, hash low/high
    std::array<std::uint32_t,4> status{}; // cutoff, hash search, density search, iteration
    std::array<std::uint32_t,4> totals{}; // patches, candidates, error, reserved
    std::array<std::uint32_t,17> counts{},first{};
    std::array<std::uint32_t,2> padding{};
};
static_assert(sizeof(GrassAllocationSummary)==224);
static_assert(offsetof(GrassAllocationSummary,counts)==80);
static_assert(offsetof(GrassAllocationSummary,first)==148);
struct GrassAllocationBuffers {
    GLuint references=0,state=0,groups=0,ranks=0,offsets=0;
    GLsync fence=nullptr;
    TerrainGenerationKey generation;
    glm::dvec3 planningEye{0};
    std::uint64_t triangles=0,inputBytes=0,workingBytes=0,dispatches=0;
    mutable std::uint64_t summaryReadBytes=0,diagnosticReadBytes=0;
    bool complete=false;
    ~GrassAllocationBuffers();
    GrassAllocationBuffers()=default;
    GrassAllocationBuffers(const GrassAllocationBuffers&)=delete;
    GrassAllocationBuffers& operator=(const GrassAllocationBuffers&)=delete;
    bool poll();
    void waitForCapture();
    GrassAllocationSummary readSummary() const;
    std::vector<std::uint32_t> readReferencesForValidation(std::size_t count) const;
};
class GrassAllocationCompute {
    TerrainComputeLimits limits_;
    std::unique_ptr<Shader> shader_;
public:
    explicit GrassAllocationCompute(TerrainComputeLimits limits=TerrainComputeLimits::query()) : limits_(std::move(limits)) {}
    ~GrassAllocationCompute();
    GrassAllocationCompute(const GrassAllocationCompute&)=delete;
    GrassAllocationCompute& operator=(const GrassAllocationCompute&)=delete;
    std::unique_ptr<GrassAllocationBuffers> generate(const GrassMetadataBuffers& metadata,
        const config::FoliageConfig& settings);
};
}

#pragma once
#include <GL/glew.h>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "rendering/geometry/terrain/TerrainTopology.h"
#include "rendering/Shader.h"
#include "rendering/foliage/planning/GrassBudget.h"
namespace config { struct PlanetConfig; }
namespace rendering {
// std430 descriptors retain original terrain triangle IDs. Allocation and
// density adjustment consume these on GPU in the next migration phase.
struct alignas(32) GrassTriangleMetadata {
    std::array<double,4> centerReach{};
    std::array<double,2> areaDistance{}; // Gaussian-weighted m² and center distance m
    std::array<std::uint32_t,4> identity{}; // triangle, eligible, reserved, reserved
};
static_assert(sizeof(GrassTriangleMetadata)==64);
static_assert(offsetof(GrassTriangleMetadata,areaDistance)==32);
static_assert(offsetof(GrassTriangleMetadata,identity)==48);
struct alignas(32) GrassMetadataParameters {
    std::array<double,4> eyeScale{};
    std::array<double,4> ranges{}; // distance, rebuild margin, 2 sigma², requested density
    std::array<double,4> biome{}; // water level, clearance, rock end, snow end
    std::array<double,4> colorGreen{};
    std::array<std::uint32_t,4> flags{}; // enabled/within ground envelope, water, landscape, triangles
    std::array<std::uint32_t,4> padding{};
};
static_assert(sizeof(GrassMetadataParameters)==160);
static_assert(offsetof(GrassMetadataParameters,flags)==128);
GrassMetadataParameters grassMetadataParameters(const config::PlanetConfig& planet,
    double metersPerWorldUnit,const glm::dvec3& eye,std::uint32_t triangles,const GrassFalloff& policy={});
struct GrassMetadataLimits {
    std::uint64_t blockBytes=0;
    std::uint32_t groups=0;
    std::string unavailable;
    static GrassMetadataLimits query();
};
struct GrassMetadataBuffers {
    GLuint descriptors=0,parameters=0;
    GLsync fence=nullptr;
    TerrainGenerationKey generation;
    glm::dvec3 planningEye{0};
    std::uint64_t triangles=0,inputBytes=0,workingBytes=0,dispatches=0;
    mutable std::uint64_t diagnosticReadBytes=0;
    bool complete=false,adaptive=false;
    ~GrassMetadataBuffers();
    GrassMetadataBuffers()=default;
    GrassMetadataBuffers(const GrassMetadataBuffers&)=delete;
    GrassMetadataBuffers& operator=(const GrassMetadataBuffers&)=delete;
    bool poll();
    void waitForCapture();
    // Explicit test-only readback; the renderer never calls this.
    std::vector<GrassTriangleMetadata> readForValidation() const;
};
class GrassMetadataCompute {
    GrassMetadataLimits limits_;
    std::unique_ptr<Shader> shader_;
public:
    explicit GrassMetadataCompute(GrassMetadataLimits limits=GrassMetadataLimits::query()) : limits_(std::move(limits)) {}
    ~GrassMetadataCompute();
    GrassMetadataCompute(const GrassMetadataCompute&)=delete;
    GrassMetadataCompute& operator=(const GrassMetadataCompute&)=delete;
    // Source VBO/EBO already contain float-rounded/sunk legacy terrain corners.
    // No shaped geometry or triangle descriptors cross the CPU/GPU boundary.
    std::unique_ptr<GrassMetadataBuffers> generate(GLuint vertices,GLuint indices,
        const TerrainBuildStats& terrain,const config::PlanetConfig& planet,
        double metersPerWorldUnit,const glm::dvec3& eye,const GrassFalloff& policy={});
    GLuint program() const { return shader_ ? shader_->id : 0; }
};
}

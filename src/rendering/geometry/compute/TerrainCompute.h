#pragma once
#include <GL/glew.h>
#include <array>
#include <map>
#include <memory>
#include <string>
#include "rendering/Shader.h"
#include "rendering/geometry/Terrain.h"
#include "rendering/geometry/terrain/detail/TerrainMorph.h"
class Mesh;
namespace rendering {
class SparseTerrainContacts;
// Borrowed immutable source from retained targets and the canonical corner map.
// The owning generation must outlive submitted consumers.
struct TerrainMorphSource {
    GLuint targets=0,corners=0;
    std::uint64_t samples=0;
    TerrainGenerationKey generation;
    explicit operator bool() const {return targets || corners || samples || generation.field || generation.topology;}
    bool operator==(const TerrainMorphSource&) const = default;
};
struct TerrainComputeLimits {
    std::uint64_t blockBytes=0;
    std::uint32_t groups=0;
    std::string unavailable;
    static TerrainComputeLimits query();
};
// One bounded spare generation. Context thread only; completion/readbacks are
// explicit capture/test operations, never a normal walking wait.
struct TerrainComputeBuffers {
    GLuint vao=0,vbo=0,ebo=0;
    std::array<GLuint,5> scratch{}; // radial, unique values, corner map, diagnostic heights, surface policy
    std::array<GLuint,2> timers{};
    GLsync fence=nullptr;
    TerrainBuildStats stats;
    std::shared_ptr<SparseTerrainContacts> contacts;
    bool complete=false,diagnostics=false;
    bool retainedMorph=false;
    TerrainMorphSource morphSource() const {
        return retainedMorph && (scratch[1] || scratch[2])?
            TerrainMorphSource{scratch[1],scratch[2],stats.uniqueSamples,stats.generation}:TerrainMorphSource{};
    }
    ~TerrainComputeBuffers();
    TerrainComputeBuffers()=default;
    TerrainComputeBuffers(const TerrainComputeBuffers&)=delete;
    TerrainComputeBuffers& operator=(const TerrainComputeBuffers&)=delete;
    bool poll();
    void waitForCapture();
    std::vector<float> readVertices() const;
    std::vector<double> readHeights() const;
    std::vector<std::uint32_t> readIndices() const;
};
class TerrainCompute {
public:
    explicit TerrainCompute(TerrainComputeLimits limits=TerrainComputeLimits::query()) : limits_(std::move(limits)) {}
    ~TerrainCompute();
    TerrainCompute(const TerrainCompute&)=delete;
    TerrainCompute& operator=(const TerrainCompute&)=delete;
    const TerrainComputeLimits& limits() const { return limits_; }
    std::unique_ptr<TerrainComputeBuffers> generate(const PlanetField& field,
        const TerrainTopology& topology,bool diagnostics=false,TerrainMorphStorage morph=TerrainMorphStorage::Transient);
    std::unique_ptr<TerrainComputeBuffers> generate(const PlanetField& field,
        const TerrainTopologySnapshot& topology,bool diagnostics=false,TerrainMorphStorage morph=TerrainMorphStorage::Transient);
    // Standalone deformation primitive. Runtime consumers must share a phase
    // before this may be used on a mesh with bound sparse contacts.
    void deform(Mesh& mesh,const TerrainMorphFootprint& footprint);
    GLuint program() const { return shader_ ? shader_->id : 0; }
private:
    std::unique_ptr<TerrainComputeBuffers> generateValidated(const PlanetField& field,
        const TerrainTopology& topology,bool diagnostics,TerrainMorphStorage morph);
    TerrainComputeLimits limits_;
    Shader* shader_=nullptr;
    std::array<std::unique_ptr<Shader>,3> variants_;
    std::unique_ptr<Shader> morphShader_;
    std::map<std::uint64_t,GLuint> fields_;
};
}

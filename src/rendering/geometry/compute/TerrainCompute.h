#pragma once
#include <GL/glew.h>
#include <array>
#include <map>
#include <memory>
#include <string>
#include "rendering/Shader.h"
#include "rendering/geometry/Terrain.h"
namespace rendering {
class SparseTerrainContacts;
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
        const TerrainTopology& topology,bool diagnostics=false);
    GLuint program() const { return shader_ ? shader_->id : 0; }
private:
    TerrainComputeLimits limits_;
    std::unique_ptr<Shader> shader_;
    std::map<std::uint64_t,GLuint> fields_;
};
}

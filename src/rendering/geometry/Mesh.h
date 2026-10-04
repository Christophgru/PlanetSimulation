#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <GL/glew.h>
#include "rendering/geometry/terrain/TerrainTopology.h"
namespace rendering { struct TerrainGeometry; }
namespace rendering { struct TerrainComputeBuffers; }
namespace rendering { class SparseTerrainContacts; }

class Mesh {
public:
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    bool hasVertexColors = false;
    bool residentTerrain = false;
    std::size_t indexCount = 0;
    std::uint64_t revision = 0;
    rendering::TerrainBuildStats terrainStats{};
    std::shared_ptr<rendering::SparseTerrainContacts> contacts;
    
    Mesh() = default;
    
    void addVertex(float x, float y, float z, float nx, float ny, float nz);
    
    void addTriangle(unsigned int idx0, unsigned int idx1, unsigned int idx2);
    
    void buildSphereGeometry(int segments);

    void generateSphere(int segments);

    void buildCubeGeometry();

    void generateCube();

    void loadTerrain(rendering::TerrainGeometry geometry);
    void loadComputedTerrain(rendering::TerrainGeometry geometry,
                             rendering::TerrainComputeBuffers& buffers, bool keepCpuMirror=true);

    void upload();
    
    void draw() const;

    void destroy();
    // Transfer a complete prevalidated generation without allocating or GL work.
    void swap(Mesh& other) noexcept;
};

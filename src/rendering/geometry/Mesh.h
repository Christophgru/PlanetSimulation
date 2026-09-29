#pragma once

#include <cstdint>
#include <vector>
#include <GL/glew.h>
namespace rendering { struct TerrainGeometry; }

class Mesh {
public:
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    bool hasVertexColors = false;
    std::uint64_t revision = 0;
    
    Mesh() = default;
    
    void addVertex(float x, float y, float z, float nx, float ny, float nz);
    
    void addTriangle(unsigned int idx0, unsigned int idx1, unsigned int idx2);
    
    void buildSphereGeometry(int segments);

    void generateSphere(int segments);

    void buildCubeGeometry();

    void generateCube();

    void loadTerrain(rendering::TerrainGeometry geometry);

    void upload();
    
    void draw() const;

    void destroy();
};

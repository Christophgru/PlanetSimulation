#pragma once
#include "rendering/Shader.h"
#include "rendering/geometry/Mesh.h"

namespace rendering {
inline std::uint64_t terrainStorageBytes(const std::vector<Mesh>& meshes) {
    std::uint64_t bytes=0;
    for(const auto& mesh:meshes) bytes+=mesh.terrainStats.gpuWorkingBytes ? mesh.terrainStats.gpuWorkingBytes :
        mesh.vertices.size()*sizeof(float)+mesh.indices.size()*sizeof(unsigned);
    return bytes;
}
class OwnedShader : public Shader {
public:
    using Shader::Shader;
    ~OwnedShader() { glDeleteProgram(id); }
    OwnedShader(const OwnedShader&) = delete;
    OwnedShader& operator=(const OwnedShader&) = delete;
};
class VertexArray {
public:
    VertexArray() { glGenVertexArrays(1, &id); }
    ~VertexArray() { glDeleteVertexArrays(1, &id); }
    VertexArray(const VertexArray&) = delete;
    VertexArray& operator=(const VertexArray&) = delete;
    GLuint id = 0;
};
struct SceneMeshes {
    explicit SceneMeshes(std::size_t count) : planetMeshes(count), waterMeshes(count) {}
    ~SceneMeshes() {
        sunMesh.destroy();
        skyboxMesh.destroy();
        for (auto& mesh : planetMeshes) mesh.destroy();
        for (auto& mesh : waterMeshes) mesh.destroy();
    }
    SceneMeshes(const SceneMeshes&) = delete;
    SceneMeshes& operator=(const SceneMeshes&) = delete;
    std::uint64_t terrainBytes() const {return terrainStorageBytes(planetMeshes)+terrainStorageBytes(waterMeshes);}
    Mesh sunMesh, skyboxMesh;
    std::vector<Mesh> planetMeshes, waterMeshes;
};
}

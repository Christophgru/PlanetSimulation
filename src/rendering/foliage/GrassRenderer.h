#pragma once
#include <cstddef>
#include <GL/glew.h>
#include "rendering/Shader.h"
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>
class Mesh;
namespace config { struct PlanetConfig; }


namespace rendering {
struct GrassBlade;
class GrassRenderer {
    struct Batch { GLuint vao=0, buffer=0; GLsizei count=0; };
    struct Patch {
        Batch near, far;
        glm::dvec3 eye{0};
        std::uint64_t revision=0;
        bool ready=false;
    };
    std::vector<Patch> patches_;
    static void upload(Batch& batch, const std::vector<GrassBlade>& blades);
public:
    Shader shader{"shaders/foliage/grass.vert","shaders/foliage/grass.frag",
                  "shaders/terrain/terrain_shadow.glsl","shaders/atmosphere/atmosphere.glsl"};
    GrassRenderer() = default;
    GrassRenderer(const GrassRenderer&) = delete;
    GrassRenderer& operator=(const GrassRenderer&) = delete;
    ~GrassRenderer();
    void clear();
    void prepare(std::size_t index,const Mesh& mesh,const config::PlanetConfig& planet,
                 double metersPerWorldUnit,const glm::dvec3& eyeBody);
    std::size_t count(std::size_t index) const;
    void draw(std::size_t index) const;
};
} // namespace rendering

#pragma once
#include <cstddef>
#include <GL/glew.h>
#include "rendering/Shader.h"
#include <array>
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>
class Mesh;
namespace config { struct PlanetConfig; }


namespace rendering {
struct GrassBlade;
struct GrassLodPlan;
struct GrassPreparationStats {
    double placementMs = 0, sortMs = 0, uploadMs = 0;
    unsigned rebuilds = 0;
};
struct GrassDrawStats {
    std::size_t blades=0, vertices=0, triangles=0, batches=0, instanceBytes=0;
};
class GrassRenderer {
    struct Batch { GLuint vao=0; GLsizei count=0; };
    struct Patch {
        std::array<Batch, 8> batches{};
        GLuint buffer=0;
        glm::dvec3 eye{0};
        std::uint64_t revision=0;
        bool ready=false;
    };
    std::vector<Patch> patches_;
    static void upload(Patch& patch, const GrassLodPlan& plan);
public:
    Shader shader{"shaders/foliage/grass.vert","shaders/foliage/grass.frag",
                  "shaders/terrain/terrain_shadow.glsl","shaders/atmosphere/atmosphere.glsl"};
    GrassRenderer() = default;
    GrassRenderer(const GrassRenderer&) = delete;
    GrassRenderer& operator=(const GrassRenderer&) = delete;
    ~GrassRenderer();
    void clear();
    GrassPreparationStats prepare(std::size_t index,const Mesh& mesh,const config::PlanetConfig& planet,
                 double metersPerWorldUnit,const glm::dvec3& eyeBody);
    std::size_t count(std::size_t index) const;
    GrassDrawStats drawStats(std::size_t index) const;
    void draw(std::size_t index) const;
};
} // namespace rendering

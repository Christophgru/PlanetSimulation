#pragma once
#include <cstddef>
#include "rendering/foliage/procedural/ProceduralGrass.h"
class Mesh;
namespace config { struct PlanetConfig; }

namespace rendering {
struct GrassDrawStats {
    std::size_t blades=0, vertices=0, triangles=0, batches=0, instanceBytes=0;
};
class GrassRenderer {
public:
    // Upload triangle IDs; placement and wind stay on the GPU.
    ProceduralGrass procedural;
    Shader& shader=procedural.shader;
    GrassRenderer() = default;
    GrassRenderer(const GrassRenderer&) = delete;
    GrassRenderer& operator=(const GrassRenderer&) = delete;
    ~GrassRenderer()=default;
    void clear();
    GrassPreparationStats prepare(std::size_t index,const Mesh& mesh,const config::PlanetConfig& planet,
                 double metersPerWorldUnit,const glm::dvec3& eyeBody);
    std::size_t count(std::size_t index) const;
    GrassDrawStats drawStats(std::size_t index) const;
    void draw(std::size_t index,const GrassPass* pass=nullptr) const;
};
} // namespace rendering

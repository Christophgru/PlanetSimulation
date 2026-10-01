#pragma once
#include <cstddef>
#include <GL/glew.h>
#include "rendering/Shader.h"
#include "rendering/foliage/GrassStats.h"
#include "rendering/foliage/horizon/HorizonGrass.h"
#include <array>
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>
class Mesh;
namespace config { struct PlanetConfig; }


namespace rendering {
struct GrassBlade;
struct GrassLodPlan;
struct GrassDrawStats {
    std::size_t blades=0, vertices=0, triangles=0, batches=0, instanceBytes=0;
};
class GrassRenderer {
public:
    // Both layers upload terrain descriptors; all random roots live on GPU.
    HorizonGrass near{true};
    HorizonGrass horizon;
    Shader& shader=near.shader;
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

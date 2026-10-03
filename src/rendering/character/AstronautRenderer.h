#pragma once
#include "rendering/character/AstronautMotion.h"
#include "rendering/Shader.h"
#include "rendering/geometry/Mesh.h"
#include <cstddef>
#include "rendering/character/effects/ExhaustRenderer.h"

namespace rendering {
// Original procedural model: grey/blue suit, oversized helmet, dark visor,
// articulated limbs, German arm flags and a life-support/jetpack backpack.
class AstronautRenderer {
public:
    AstronautRenderer();
    ~AstronautRenderer();
    AstronautRenderer(const AstronautRenderer&)=delete;
    AstronautRenderer& operator=(const AstronautRenderer&)=delete;
    void draw(const glm::dvec3& planetCenter,const glm::dmat3& orientation,
              double radiusWorld,double metersPerWorldUnit,
              const glm::mat4& view,const glm::mat4& projection);
    AstronautMotion motion;
    std::size_t planetIndex=0;
    Shader shader;
    ExhaustParticles exhaust;
    std::optional<ExhaustEmitter> lastEmitter;
    std::optional<double> exhaustTime;
    ExhaustRenderer exhaustRenderer;
private:
    Mesh sphere_;
    Mesh flag_;
};
}

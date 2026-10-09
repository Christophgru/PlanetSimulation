#pragma once
#include "rendering/character/AstronautMotion.h"
#include "rendering/Shader.h"
#include "rendering/geometry/Mesh.h"
#include <cstddef>
#include "rendering/character/assets/SkinnedSuit.h"
#include "rendering/character/effects/ExhaustRenderer.h"

namespace rendering {
// NASA ACES mesh, skinned by the existing locomotion/foot IK rig.
// German arm flags and the functional jetpack are attached accessories.
class AstronautRenderer {
public:
    AstronautRenderer();
    ~AstronautRenderer();
    AstronautRenderer(const AstronautRenderer&)=delete;
    AstronautRenderer& operator=(const AstronautRenderer&)=delete;
    void draw(const glm::dvec3& planetCenter,const glm::dmat3& orientation,
              double radiusWorld,double metersPerWorldUnit,
              const glm::mat4& view,const glm::mat4& projection);
    std::size_t triangleCount() const { return suit_.triangleCount(); }
    AstronautMotion motion;
    std::size_t planetIndex=0;
    Shader shader;
    ExhaustParticles exhaust;
    std::optional<ExhaustEmitter> lastEmitter;
    std::optional<double> exhaustTime;
    ExhaustRenderer exhaustRenderer;
private:
    SkinnedSuit suit_;
    Mesh sphere_;
    Mesh flag_;
};
}

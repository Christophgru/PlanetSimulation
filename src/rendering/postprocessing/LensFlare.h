#pragma once
#include "rendering/runtime/ResourceOwners.h"
#include <glm/glm.hpp>
#include <cstddef>

namespace rendering {
struct FlareEvidence { std::size_t visibleSunPixels=0; double strength=0; };
class LensFlare {
public:
    // Capture-only display-space approximation of aperture ghosts. Depth-tested
    // Sun IDs gate the effect; scene color attenuates it through atmosphere.
    FlareEvidence draw(const glm::mat4& view, const glm::mat4& projection,
                       const glm::dvec3& eye, const glm::dvec3& sun, double radius,
                       const glm::vec3& tint, int width, int height);
private:
    OwnedShader shader_{"shaders/postprocessing/flare.vert", "shaders/postprocessing/flare.frag"};
    VertexArray vao_;
};
}

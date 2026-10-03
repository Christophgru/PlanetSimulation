#pragma once
#include "config/FoliageConfig.h"
#include <glm/glm.hpp>
namespace rendering {
// Float arithmetic, hashing and advection match both production grass paths.
struct WindField {
    static float perlin(glm::vec3 point,int seed);
    static glm::vec3 samples(const config::FoliageConfig&,glm::dvec3 localMeters,double seconds);
    // Grass specifies a bend angle, not air speed. 6 m/s at full gust/strength
    // defines the aerodynamic interpretation, in the same tangent direction.
    static glm::dvec3 velocity(const config::FoliageConfig&,glm::dvec3 localMeters,double seconds);
};
}

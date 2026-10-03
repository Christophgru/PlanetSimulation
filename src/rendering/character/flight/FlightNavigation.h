#pragma once
#include "rendering/character/SurfaceContact.h"
#include "rendering/character/flight/JetpackPhysics.h"
#include <optional>
#include <vector>

namespace rendering {
// All positions and velocities are inertial world-space SI values. Index zero
// is the Sun; the remaining indices agree with the scene's body array.
struct FlightBody {
    glm::dvec3 position{0}, velocity{0}, angularVelocity{0};
    glm::dmat3 orientation{1};
    FlightEnvironment environment;
    bool orientable=true;
    GroundQuery ground;
    std::function<glm::dvec3(const glm::dvec3&,double)> wind;
    double windTime=0;
    glm::dvec3 localPoint(const glm::dvec3& p) const { return glm::transpose(orientation)*(p-position); }
    glm::dvec3 airVelocity(const glm::dvec3& p) const {
        return surfaceVelocity(p)+(wind ? orientation*wind(localPoint(p),windTime) : glm::dvec3(0));
    }
    glm::dvec3 surfaceVelocity(const glm::dvec3& p) const {
        return velocity+glm::cross(angularVelocity,p-position);
    }
};
struct FlightState {
    glm::dvec3 position{0}, velocity{0}, up{0,1,0};
    std::size_t referenceBody=1;
    bool outerSpace=false;
    glm::dvec3 suitUp{0,1,0};
};
struct FlightStep {
    glm::dvec3 suitUp{0,1,0};
    double thrust=0;
    std::optional<std::size_t> landedBody;
};
class FlightNavigation {
public:
    static constexpr double orientationRadiusFactor=2.4; // 1.2 diameters from centre.
    static std::vector<std::size_t> nearest(const std::vector<FlightBody>&,const glm::dvec3&);
    static glm::dvec3 gravity(const std::vector<FlightBody>&,const glm::dvec3&);
    static void orient(FlightState&,const std::vector<FlightBody>&,double elapsed);
    static FlightStep advance(FlightState&,const std::vector<FlightBody>&,
        const glm::dvec3& look,const glm::dvec3& right,const glm::dvec2& directional,
        bool upward,const glm::dvec3& suitUp,double maximumThrust,double elapsed);
    static void validate(const FlightState&,std::size_t bodyCount);
    static glm::dmat3 transport(const glm::dvec3& from,const glm::dvec3& to,double blend=1);
};
}

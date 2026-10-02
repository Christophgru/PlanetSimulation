#pragma once
#include "rendering/character/SurfaceContact.h"
#include "rendering/character/flight/JetpackPhysics.h"
#include <array>
#include <glm/glm.hpp>

namespace rendering {
struct AstronautFoot {
    GroundContact contact;
    glm::dvec3 forward{0,0,-1};
    glm::dvec3 start{0}, target{0};
    double progress = 1;
    double duration = .24;
    bool planted() const { return progress>=1; }
};
struct AstronautPose {
    glm::dvec3 root{0}, up{0,1,0}, forward{0,0,-1}, right{1,0,0};
    std::array<glm::dvec3,2> hips{}, knees{}, ankles{}; // Body-local metres.
    std::array<bool,2> legReached{};
    std::array<AstronautFoot,2> feet{};
    double armSwing = 0;
    double walkedMeters = 0;
    double flightHeight = 0, verticalVelocity = 0, effectSeconds = 0, boostPulse = 0;
    bool airborne = false, jetpackArmed = false, boosting = false;
    glm::dvec3 velocity{0}, suitUp{0,1,0}; // Rotating-body-frame SI velocity and exhaust axis.
    double thrustN=0;
    glm::dvec3 bodyOffset{0}; // Suit-local pelvis adjustment; ground root and boots stay fixed.
    glm::dmat3 suitBasis() const {
        auto facing=forward-suitUp*glm::dot(forward,suitUp);
        if (glm::length(facing)<1e-8) facing=glm::cross(right,suitUp);
        facing=glm::normalize(facing);
        return {glm::normalize(glm::cross(facing,suitUp)),suitUp,-facing};
    }
};
struct ChasePose { glm::dvec3 eye{0}, target{0}, up{0,1,0}; };

class AstronautMotion {
public:
    void reset() { ready_=false; spacePresses_=0; boostHeld_=false; }
    bool ready() const { return ready_; }
    bool animating() const;
    void update(const GroundContact& root,const glm::dvec3& lookForward,
                double elapsed,const GroundQuery& ground);
    // Mesh revisions may change contact height, but never its surface direction.
    void refreshContacts(const GroundQuery& ground);
    ChasePose chase(const glm::dvec3& lookDirection,const GroundQuery& ground) const;
    const AstronautPose& pose() const { return pose_; }
    void restore(const AstronautPose& pose);
    void pressSpace() { ++spacePresses_; }
    void holdBoost(bool held) { boostHeld_=held; }
    void setGravity(double metersPerSecondSquared);
    void setFlightEnvironment(const FlightEnvironment& environment,double maximumThrust);
    void setFlightControl(const glm::dvec2& forwardRight) {
        if (!std::isfinite(glm::length(forwardRight))) throw std::invalid_argument("Flight input must be finite");
        flightControl_=forwardRight;
    }
    double maximumThrust() const { return maximumThrust_; }
    double airPressure() const { return JetpackPhysics::pressure(flightEnvironment_,pose_.root); }
    double airDensity() const { return JetpackPhysics::density(flightEnvironment_,pose_.root); }
    static double surfaceGravity(double massKg,double radiusMeters,double spinPeriod,double latitudeRadians);
private:
    void solveLegs(bool standing,double elapsed,bool newlyPlaced=false);
    AstronautPose pose_;
    bool ready_=false;
    int nextFoot_=0;
    double walked_=0;
    unsigned spacePresses_=0;
    bool boostHeld_=false;
    FlightEnvironment flightEnvironment_;
    double maximumThrust_=6000;
    glm::dvec2 flightControl_{0};
};
}

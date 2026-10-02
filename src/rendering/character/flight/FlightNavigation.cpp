#include "rendering/character/flight/FlightNavigation.h"
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <numeric>
#include <stdexcept>

namespace rendering {
glm::dmat3 FlightNavigation::transport(const glm::dvec3& from,const glm::dvec3& to,double blend) {
    const double dot=std::clamp(glm::dot(from,to),-1.0,1.0);
    glm::dquat turn;
    if (dot<-.999999) {
        const auto axis=glm::normalize(glm::cross(from,std::abs(from.x)<.9 ? glm::dvec3(1,0,0) : glm::dvec3(0,1,0)));
        turn=glm::angleAxis(glm::pi<double>(),axis);
    } else turn=glm::normalize(glm::dquat(1+dot,glm::cross(from,to)));
    return glm::mat3_cast(glm::slerp(glm::dquat(1,0,0,0),turn,blend));
}
std::vector<std::size_t> FlightNavigation::nearest(const std::vector<FlightBody>& bodies,const glm::dvec3& p) {
    std::vector<std::size_t> indices(bodies.size());
    std::iota(indices.begin(),indices.end(),0);
    std::stable_sort(indices.begin(),indices.end(),[&](auto a,auto b) {
        return glm::length(bodies[a].position-p)<glm::length(bodies[b].position-p);
    });
    indices.resize(std::min<std::size_t>(3,indices.size()));
    return indices;
}
glm::dvec3 FlightNavigation::gravity(const std::vector<FlightBody>& bodies,const glm::dvec3& p) {
    glm::dvec3 result(0);
    for (auto i:nearest(bodies,p)) {
        const auto delta=bodies[i].position-p;
        const double distance=glm::length(delta);
        // Uniform sphere inside the body prevents a singularity during a
        // collision step; outside, this is precisely inverse-square gravity.
        const double radius=std::max(distance,bodies[i].environment.radiusMeters);
        result+=delta*(6.67430e-11*bodies[i].environment.planetMassKg/(radius*radius*radius));
    }
    return result;
}
void FlightNavigation::validate(const FlightState& s,std::size_t count) {
    if (!std::isfinite(glm::length(s.position)) || !std::isfinite(glm::length(s.velocity)) ||
        !std::isfinite(glm::length(s.up)) || std::abs(glm::length(s.up)-1)>1e-6 ||
        !std::isfinite(glm::length(s.suitUp)) || std::abs(glm::length(s.suitUp)-1)>1e-6 ||
        s.referenceBody==0 || s.referenceBody>=count)
        throw std::invalid_argument("Invalid world-space astronaut flight state");
}
void FlightNavigation::orient(FlightState& s,const std::vector<FlightBody>& bodies,double elapsed) {
    validate(s,bodies.size());
    if (!std::isfinite(elapsed) || elapsed<0) throw std::invalid_argument("Invalid flight orientation time");
    const auto distance=[&](std::size_t i) { return glm::length(s.position-bodies[i].position); };
    const auto inside=[&](std::size_t i) { return distance(i)<=orientationRadiusFactor*bodies[i].environment.radiusMeters; };
    std::optional<std::size_t> chosen;
    // Retain the current body in overlapping regions unless the other body's
    // centre proximity is at least 10% better. Exit still occurs at 2.4 R.
    if (bodies[s.referenceBody].orientable && inside(s.referenceBody))
        chosen=s.referenceBody;
    for (std::size_t i=0;i<bodies.size();++i)
        if (bodies[i].orientable && inside(i) &&
            (!chosen || distance(i)<distance(*chosen)*.9)) chosen=i;
    s.outerSpace=!chosen;
    if (!chosen) return; // Preserve the inertial up axis in space.
    s.referenceBody=*chosen;
    const auto offset=s.position-bodies[*chosen].position;
    if (glm::length(offset)<1e-9) return;
    const auto target=glm::normalize(offset);
    s.up=glm::normalize(transport(s.up,target,1-std::exp(-elapsed/.35))*s.up);
}
FlightStep FlightNavigation::advance(FlightState& s,const std::vector<FlightBody>& bodies,
    const glm::dvec3& look,const glm::dvec3& right,const glm::dvec2& directional,
    bool upward,const glm::dvec3& suitUp,double maximum,double dt) {
    if (!std::isfinite(dt) || dt<0 || dt>.010001 || !std::isfinite(maximum) || maximum<=0)
        throw std::invalid_argument("Invalid flight integration step");
    orient(s,bodies,0);
    FlightStep result; result.suitUp=suitUp;
    if (!s.outerSpace)
        result.suitUp=glm::normalize(transport(suitUp,s.up,1-std::exp(-dt/.35))*suitUp);
    const auto& body=bodies[s.referenceBody];
    const auto airVelocity=body.surfaceVelocity(s.position);
    const auto relative=s.velocity-airVelocity;
    struct Air { double drag; glm::dvec3 velocity; };
    std::vector<Air> air;
    glm::dvec3 dragAcceleration(0);
    for (auto i:nearest(bodies,s.position)) {
        const auto& b=bodies[i];
        const double drag=JetpackPhysics::dragCoefficient(b.environment,b.localPoint(s.position));
        if (drag<=0) continue;
        const auto velocity=b.surfaceVelocity(s.position), relative=s.velocity-velocity;
        air.push_back({drag,velocity});
        dragAcceleration+=drag*glm::length(relative)*relative;
    }
    const auto g=gravity(bodies,s.position);
    auto direction=look*directional.x+right*directional.y+(upward ? s.up : glm::dvec3(0));
    if (glm::length(direction)>1e-9) {
        direction=glm::normalize(direction);
        // Near a body, steer the complete relative velocity toward the 3D
        // command. Space flight commands acceleration without a speed cap.
        auto acceleration=s.outerSpace ? direction*30.0 : (direction*JetpackPhysics::speedTarget-relative)*.3;
        if (glm::length(acceleration)>30) acceleration=glm::normalize(acceleration)*30.0;
        auto force=JetpackPhysics::massKg*acceleration;
        force+=JetpackPhysics::massKg*(dragAcceleration-(s.outerSpace ? glm::dvec3(0) : g));
        const double magnitude=glm::length(force);
        if (magnitude>1e-9) {
            result.suitUp=glm::normalize(transport(suitUp,force/magnitude,1-std::exp(-dt/.12))*suitUp);
            result.thrust=std::min(maximum,magnitude);
        }
    }
    const auto before=s.velocity;
    const auto kicked=s.velocity+(g+result.suitUp*(result.thrust/JetpackPhysics::massKg))*dt;
    s.velocity=kicked;
    // Aerodynamics depends on gas fields, never on the orientation mode. Each
    // quadratic drag split is dissipative relative to that body's moving air.
    for (const auto& a:air) {
        const auto relative=s.velocity-a.velocity;
        s.velocity=a.velocity+relative/(1+a.drag*glm::length(relative)*dt);
    }
    s.position+=(before+s.velocity)*(.5*dt);
    // Sample each body's own terrain/water. A destination collision cannot
    // accidentally clamp to the departure planet's surface.
    for (std::size_t i=0;i<bodies.size();++i) {
        const auto& b=bodies[i];
        if (!b.orientable) continue;
        const auto p=b.localPoint(s.position);
        if (glm::length(p)<1e-9) continue;
        const auto floor=b.ground ? b.ground(p) : GroundContact{glm::normalize(p)*b.environment.radiusMeters,glm::normalize(p)};
        if (glm::length(p)<=glm::length(floor.position)) {
            const auto normal=b.orientation*glm::normalize(floor.position);
            s.position=b.position+b.orientation*floor.position;
            if (glm::dot(s.velocity-b.surfaceVelocity(s.position),normal)<=0) {
                s.velocity=b.surfaceVelocity(s.position); s.referenceBody=i; s.outerSpace=false; s.up=normal;
                result.landedBody=i; result.thrust=0;
                break;
            }
        }
    }
    orient(s,bodies,dt);
    s.suitUp=result.suitUp;
    return result;
}
}

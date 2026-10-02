#include "rendering/character/flight/JetpackPhysics.h"
#include <algorithm>
#include <cmath>

namespace rendering {
namespace {
double molarMass(const config::AtmosphereConfig& a) {
    const double gas=100-a.red_dust;
    if (gas<=1e-9) return .0280134;
    return (a.nitrogen*.0280134+a.oxygen*.031998+a.water*.01801528+
        a.carbon_dioxide*.0440095+(a.argon+a.balancePercent())*.039948)/gas;
}
}
glm::dvec3 JetpackPhysics::gravity(const FlightEnvironment& e,const glm::dvec3& p,const glm::dvec3& v) {
    const double radius=glm::length(p);
    const auto omega=glm::dvec3(0,0,e.spinRadiansPerSecond);
    const double g=e.planetMassKg>0 ? 6.67430e-11*e.planetMassKg/(radius*radius) : e.constantGravity;
    return -glm::normalize(p)*g-2.0*glm::cross(omega,v)-glm::cross(omega,glm::cross(omega,p));
}
double JetpackPhysics::pressure(const FlightEnvironment& e,const glm::dvec3& p) {
    const auto& a=e.atmosphere;
    if (!a.enabled || a.surface_pressure_pa==0) return 0;
    const double sea=e.radiusMeters+e.seaLevelMeters;
    const auto seaPoint=glm::normalize(p)*sea;
    const double g=std::max(0.0,-glm::dot(gravity(e,seaPoint,glm::dvec3(0)),glm::normalize(p)));
    // Isothermal hydrostatic pressure with local sea-level effective gravity.
    // Optical scale heights in this miniature renderer are artistic; they
    // are deliberately not used to calculate astronaut aerodynamic forces.
    const double height=std::max(0.0,glm::length(p)-sea);
    return a.surface_pressure_pa*std::exp(-g*height*molarMass(a)/(8.314462618*a.temperature_k));
}
double JetpackPhysics::density(const FlightEnvironment& e,const glm::dvec3& p) {
    return pressure(e,p)*molarMass(e.atmosphere)/(8.314462618*e.atmosphere.temperature_k);
}
double JetpackPhysics::maximumThrust(const FlightEnvironment& e) {
    const glm::dvec3 p(e.radiusMeters+e.seaLevelMeters,0,0);
    // Budget for the largest support load, including headings which cancel
    // planetary rotation. Do not size the engine from equatorial centrifugal
    // relief alone. The actual force model still uses local rotating gravity.
    const double weight=massKg*(e.planetMassKg>0 ?
        6.67430e-11*e.planetMassKg/glm::dot(p,p) : e.constantGravity);
    const double drag=.5*density(e,p)*dragArea*speedTarget*speedTarget;
    return std::max(3000.0,1.1*std::hypot(weight+massKg*20,drag));
}
glm::dvec3 JetpackPhysics::requestedThrust(const FlightEnvironment& e,const glm::dvec3& p,
                                          const glm::dvec3& v,const glm::dvec3& input,double maximum) {
    const auto up=glm::normalize(p);
    const auto horizontal=v-up*glm::dot(v,up);
    // Cartesian motion already creates v_t^2/r radial acceleration. Subtract
    // it when requesting a radial climb acceleration on a curved planet.
    glm::dvec3 force=up*massKg*(std::max(0.0,-glm::dot(gravity(e,p,v),up)-
        glm::dot(horizontal,horizontal)/glm::length(p))+20);
    if (glm::length(input)>1e-9) {
        // A 100 m/s commanded speed governor changes force, never clips the
        // velocity. Airless coasting has no invented terminal-speed clamp.
        auto acceleration=(glm::normalize(input)*speedTarget-horizontal)*.3;
        if (glm::length(acceleration)>30) acceleration=glm::normalize(acceleration)*30.0;
        force+=massKg*(acceleration+dragCoefficient(e,p)*glm::length(v)*horizontal);
    }
    if (glm::length(force)>maximum) force*=maximum/glm::length(force);
    return force;
}
}

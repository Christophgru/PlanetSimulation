#pragma once
#include "config/AtmosphereConfig.h"
#include <glm/glm.hpp>

namespace rendering {
struct FlightEnvironment {
    double radiusMeters=1000, planetMassKg=0, spinRadiansPerSecond=0;
    double seaLevelMeters=0, constantGravity=9.81;
    config::AtmosphereConfig atmosphere;
};
struct JetpackPhysics {
    static constexpr double massKg=100, dragArea=.7, speedTarget=100;
    static constexpr double exhaustSpeed=1000, efficiency=.6;
    static glm::dvec3 gravity(const FlightEnvironment&,const glm::dvec3& position,
                             const glm::dvec3& velocity);
    static double density(const FlightEnvironment&,const glm::dvec3& position);
    static double pressure(const FlightEnvironment&,const glm::dvec3& position);
    static double maximumThrust(const FlightEnvironment& mainPlanet);
    static double exhaustPower(double thrust) { return thrust*exhaustSpeed/(2*efficiency); }
    static double dragCoefficient(const FlightEnvironment& e,const glm::dvec3& p) {
        return .5*density(e,p)*dragArea/massKg;
    }
    // A controller requests one thrust vector; the nozzles never provide
    // sideways force independently of the tilted suit's exhaust axis.
    static glm::dvec3 requestedThrust(const FlightEnvironment&,const glm::dvec3& position,
                                    const glm::dvec3& velocity,const glm::dvec3& input,
                                    double maximum);
};
}

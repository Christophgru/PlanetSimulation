#include "rendering/character/AstronautMotion.h"
#include "rendering/camera/CameraInput.h"
#include <gtest/gtest.h>
#include <limits>

namespace {
using rendering::JetpackPhysics;
using rendering::FlightEnvironment;
rendering::GroundContact ground(const glm::dvec3& p) {
    const auto up=glm::normalize(p); return {up*1000.0,up};
}
}
TEST(JetpackPhysics, PressureAndDensityFollowGasTemperatureAltitudeAndVacuum) {
    FlightEnvironment e; e.atmosphere.enabled=true;
    const glm::dvec3 sea(1000,0,0);
    EXPECT_NEAR(JetpackPhysics::pressure(e,sea),101325,1e-9);
    EXPECT_NEAR(JetpackPhysics::density(e,sea),1.204,0.005);
    EXPECT_LT(JetpackPhysics::pressure(e,{2000,0,0}),101325);
    const auto density=JetpackPhysics::density(e,sea);
    e.atmosphere.temperature_k*=1.1;
    EXPECT_NEAR(JetpackPhysics::density(e,sea),density/1.1,1e-12);
    e.atmosphere.nitrogen=e.atmosphere.oxygen=e.atmosphere.carbon_dioxide=0;
    e.atmosphere.argon=100;
    EXPECT_GT(JetpackPhysics::density(e,sea),density);
    e.atmosphere.enabled=false;
    EXPECT_EQ(JetpackPhysics::density(e,sea),0);
    e.atmosphere.enabled=true; e.atmosphere.surface_pressure_pa=0;
    EXPECT_EQ(JetpackPhysics::pressure(e,sea),0);
}
TEST(JetpackPhysics, GravityIncludesAltitudeCentrifugalAndCoriolisTerms) {
    FlightEnvironment e; e.planetMassKg=1e18;
    const glm::dvec3 p(1000,0,0);
    const auto gravity=JetpackPhysics::gravity(e,p,glm::dvec3(0));
    EXPECT_NEAR(gravity.x,-66.743,1e-9);
    EXPECT_NEAR(JetpackPhysics::gravity(e,p*2.0,glm::dvec3(0)).x,gravity.x/4,1e-9);
    e.spinRadiansPerSecond=.1;
    EXPECT_NEAR(JetpackPhysics::gravity(e,p,glm::dvec3(0)).x,gravity.x+10,1e-9);
    EXPECT_NEAR(JetpackPhysics::gravity(e,p,{0,100,0}).x,gravity.x+30,1e-9);
}
TEST(JetpackPhysics, CommandedSpeedBalancesDragAndThrustHasASharedHardwareBound) {
    FlightEnvironment e; e.atmosphere.enabled=true;
    const glm::dvec3 p(0,1000,0),input(1,0,0),v(100,0,0);
    const double maximum=JetpackPhysics::maximumThrust(e);
    const auto starting=JetpackPhysics::requestedThrust(e,p,glm::dvec3(0),input,maximum);
    EXPECT_NEAR(starting.x,3000,1e-9);
    const auto cruise=JetpackPhysics::requestedThrust(e,p,v,input,maximum);
    EXPECT_NEAR(cruise.x,JetpackPhysics::dragCoefficient(e,p)*100*100*JetpackPhysics::massKg,1e-9);
    EXPECT_LE(glm::length(cruise),maximum);
    auto heavier=e; heavier.constantGravity=100;
    const auto limited=JetpackPhysics::requestedThrust(heavier,p,glm::dvec3(0),input,maximum);
    EXPECT_NEAR(glm::length(limited),maximum,1e-9); // Same engine on another planet.
    e.atmosphere.enabled=false;
    EXPECT_NEAR(JetpackPhysics::requestedThrust(e,p,v,input,maximum).x,0,1e-9);
    EXPECT_GT(JetpackPhysics::requestedThrust(e,p,{90,0,0},input,maximum).x,0);
    EXPECT_LT(JetpackPhysics::requestedThrust(e,p,{110,0,0},input,maximum).x,0);
}
TEST(JetpackPhysics, FallingConvergesToDragTerminalSpeedAndVacuumHasNoClamp) {
    FlightEnvironment e; e.atmosphere.enabled=true;
    const glm::dvec3 p(0,1000,0);
    const double k=JetpackPhysics::dragCoefficient(e,p),dt=.001;
    double v=0;
    for (int i=0;i<40000;++i) { const double kicked=v-e.constantGravity*dt; v=kicked/(1+k*std::abs(kicked)*dt); }
    EXPECT_NEAR(v,-std::sqrt(e.constantGravity/k),.02);
    e.atmosphere.enabled=false;
    rendering::AstronautMotion falling;
    falling.setFlightEnvironment(e,3000);
    falling.update(ground(p),{1,0,0},0,ground);
    auto pose=falling.pose(); pose.root={0,20000,0};
    pose.airborne=true; pose.flightHeight=19000;
    falling.restore(pose);
    falling.update(ground(p),{1,0,0},10,ground);
    EXPECT_NEAR(falling.pose().velocity.y,-98.1,1e-8);
    EXPECT_LT(falling.pose().verticalVelocity,-50);
}
TEST(AstronautFlight, TiltedAccelerationLocksArmsAndReplayContinuesDeterministically) {
    rendering::AstronautMotion motion;
    FlightEnvironment e; e.atmosphere.enabled=true;
    const double thrust=JetpackPhysics::maximumThrust(e);
    motion.setFlightEnvironment(e,thrust); motion.setFlightControl({1,0});
    motion.update(ground({0,1000,0}),{1,0,0},0,ground);
    motion.pressSpace(); motion.update(ground(motion.pose().root),{1,0,0},.1,ground);
    motion.pressSpace(); motion.holdBoost(true);
    for (int i=0;i<50;++i) motion.update(ground(motion.pose().root),{1,0,0},.01,ground);
    const auto saved=motion.pose();
    EXPECT_GT(saved.velocity.x,2); EXPECT_GT(saved.root.x,1);
    EXPECT_GT(saved.suitUp.x,.1); EXPECT_EQ(saved.armSwing,0);
    EXPECT_GT(saved.thrustN,0); EXPECT_LE(saved.thrustN,thrust);
    EXPECT_NEAR(glm::determinant(saved.suitBasis()),1,1e-9);
    rendering::AstronautMotion replay;
    replay.setFlightEnvironment(e,thrust); replay.setFlightControl({1,0}); replay.holdBoost(true); replay.restore(saved);
    motion.update(ground(saved.root),{1,0,0},.5,ground);
    for (int i=0;i<50;++i) replay.update(ground(replay.pose().root),{1,0,0},.01,ground);
    EXPECT_LT(glm::length(motion.pose().root-replay.pose().root),1e-9);
    EXPECT_LT(glm::length(motion.pose().velocity-replay.pose().velocity),1e-9);
    const auto velocity=motion.pose().velocity;
    motion.holdBoost(false); motion.setFlightControl({0,0});
    // Consume the short second-press pulse before measuring powered-off coasting.
    motion.update(ground(motion.pose().root),{1,0,0},.2,ground);
    EXPECT_FALSE(motion.pose().boosting); EXPECT_EQ(motion.pose().thrustN,0);
    EXPECT_GT(motion.pose().velocity.x,0); EXPECT_LT(motion.pose().velocity.x,velocity.x);
    auto bad=saved; bad.velocity.x=std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(replay.restore(bad),std::invalid_argument);
    EXPECT_THROW(motion.setFlightControl({std::numeric_limits<double>::infinity(),0}),std::invalid_argument);
}
TEST(AstronautInput, AirborneControlsDoNotWalkTheGroundCamera) {
    OrbitCamera orbit({0,0,0},{0,0,3000});
    PlanetSurfaceCamera surface({{0,0,0},1000},{0,0,2},{2000,0,0},60,80);
    CameraInput input(orbit,&surface); input.selectThirdPerson(); input.setThirdPersonAirborne(true);
    const auto start=surface.position(); input.update({true,false,false,false,true},.1);
    EXPECT_EQ(start,surface.position());
    surface.followSurfaceDirection({10,1000,0});
    EXPECT_NEAR(surface.location().altitude,2,1e-10);
    EXPECT_NEAR(glm::length(surface.direction()),1,1e-10);
    const auto followed=surface.position();
    input.selectSurface(); input.update({true,false,false,false,true},.1);
    EXPECT_NE(followed,surface.position()); // Camera 2 keeps its independent controls.
}

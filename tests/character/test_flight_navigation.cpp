#include "rendering/character/flight/FlightNavigation.h"
#include "rendering/character/AstronautMotion.h"
#include "rendering/camera/PlanetSurfaceCamera.h"
#include <gtest/gtest.h>
#include <limits>

namespace {
using namespace rendering;
std::vector<FlightBody> system() {
    std::vector<FlightBody> bodies(4);
    bodies[0].position={-10000,0,0}; bodies[0].orientable=false;
    bodies[1].position={0,0,0}; bodies[1].environment.radiusMeters=1000;
    bodies[2].position={6000,0,0}; bodies[2].environment.radiusMeters=500;
    bodies[3].position={30000,0,0};
    for (auto& b:bodies) { b.environment.planetMassKg=1e12; b.environment.atmosphere.enabled=false; }
    return bodies;
}
GroundContact ground(const glm::dvec3& p) { const auto u=glm::normalize(p); return {u*1000.0,u}; }
}
TEST(FlightNavigation, ThreeNearestBodiesUseVectorGravityAndChangeWithPosition) {
    auto bodies=system(); const glm::dvec3 p(3000,500,0);
    EXPECT_EQ(FlightNavigation::nearest(bodies,p),(std::vector<std::size_t>{1,2,0}));
    glm::dvec3 expected(0);
    for (auto i:{1,2,0}) { auto d=bodies[i].position-p; expected+=d*(6.67430e-11*1e12/std::pow(glm::length(d),3)); }
    EXPECT_LT(glm::length(FlightNavigation::gravity(bodies,p)-expected),1e-15);
    EXPECT_EQ(FlightNavigation::nearest(bodies,{20000,0,0}),(std::vector<std::size_t>{3,2,1}));
    bodies.resize(2); EXPECT_EQ(FlightNavigation::nearest(bodies,p).size(),2);
    EXPECT_TRUE(std::isfinite(glm::length(FlightNavigation::gravity(bodies,bodies[1].position))));
}
TEST(FlightNavigation, ExitAtTwoPointFourRadiiKeepsInertialOrientationAndVelocity) {
    const auto bodies=system(); FlightState s{{0,2400,0},{5,6,7},{0,1,0},1,false};
    FlightNavigation::orient(s,bodies,.01); EXPECT_FALSE(s.outerSpace);
    s.position.y+=.001; const auto v=s.velocity,up=s.up,p=s.position;
    FlightNavigation::orient(s,bodies,1); EXPECT_TRUE(s.outerSpace);
    EXPECT_EQ(s.up,up); EXPECT_EQ(s.velocity,v); EXPECT_EQ(s.position,p);
    s.position={3000,4000,0}; FlightNavigation::orient(s,bodies,1); EXPECT_EQ(s.up,up);
}
TEST(FlightNavigation, MoonApproachReorientsSmoothlyWithoutTeleportingOrChangingVelocity) {
    const auto bodies=system(); FlightState s{{6000,1100,0},{40,-10,2},{1,0,0},1,true};
    const auto p=s.position,v=s.velocity;
    FlightNavigation::orient(s,bodies,.01);
    EXPECT_FALSE(s.outerSpace); EXPECT_EQ(s.referenceBody,2u);
    EXPECT_LT(glm::length(s.up-glm::dvec3(1,0,0)),.05);
    for (int i=0;i<500;++i) FlightNavigation::orient(s,bodies,.01);
    EXPECT_GT(s.up.y,.999999); EXPECT_EQ(s.position,p); EXPECT_EQ(s.velocity,v);
}
TEST(FlightNavigation, OverlappingRegionsRetainCurrentBodyUntilAnotherIsMeaningfullyCloser) {
    auto bodies=system(); bodies[2].position={2000,0,0}; bodies[2].environment.radiusMeters=1000;
    FlightState s{{1000,1000,0},{0,0,0},{0,1,0},1,false};
    FlightNavigation::orient(s,bodies,.01); EXPECT_EQ(s.referenceBody,1u);
    s.position.x=1010; FlightNavigation::orient(s,bodies,.01); EXPECT_EQ(s.referenceBody,1u);
    s.position.x=1400; FlightNavigation::orient(s,bodies,.01); EXPECT_EQ(s.referenceBody,2u);
}
TEST(FlightNavigation, DirectionalOnlyThrustDescendsAndReleaseCoastsInVacuum) {
    auto bodies=system(); for (auto& b:bodies) b.environment.planetMassKg=0;
    FlightState s{{0,1800,0},{0,0,0},{0,1,0},1,false}; glm::dvec3 axis(0,1,0);
    for (int i=0;i<200;++i) {
        auto step=FlightNavigation::advance(s,bodies,{0,-1,0},{1,0,0},{1,0},false,axis,6000,.01);
        axis=step.suitUp; EXPECT_GT(step.thrust,0); EXPECT_LE(step.thrust,6000);
    }
    EXPECT_LT(s.velocity.y,-20); EXPECT_LT(s.position.y,1800);
    const auto v=s.velocity;
    auto step=FlightNavigation::advance(s,bodies,{0,-1,0},{1,0,0},{0,0},false,axis,6000,.01);
    EXPECT_EQ(step.thrust,0); EXPECT_EQ(s.velocity,v);
}
TEST(FlightNavigation, UpwardThrustAndSpaceAccelerationAreNotVelocityClamped) {
    auto bodies=system(); for (auto& b:bodies) b.environment.planetMassKg=0;
    FlightState s{{0,4000,0},{0,150,0},{0,1,0},1,true};
    auto step=FlightNavigation::advance(s,bodies,{1,0,0},{0,0,1},{0,0},true,{0,1,0},6000,.01);
    EXPECT_TRUE(s.outerSpace); EXPECT_GT(s.velocity.y,150); EXPECT_GT(step.thrust,0);
}
TEST(FlightNavigation, DestinationLandingUsesItsOwnSurfaceAndMovingAirFrame) {
    auto bodies=system(); bodies[2].velocity={10,0,0};
    bodies[2].ground=[](const glm::dvec3& p) { auto u=glm::normalize(p); return GroundContact{u*520.0,u}; };
    FlightState s{{6000,520.001,0},{10,-2,0},{0,1,0},1,true};
    auto step=FlightNavigation::advance(s,bodies,{0,-1,0},{1,0,0},{0,0},false,{0,1,0},6000,.01);
    ASSERT_TRUE(step.landedBody); EXPECT_EQ(*step.landedBody,2u);
    EXPECT_NEAR(glm::length(s.position-bodies[2].position),520,1e-10);
    EXPECT_EQ(s.velocity,bodies[2].velocity); EXPECT_FALSE(s.outerSpace);
}
TEST(FlightNavigation, FlightCameraDoesNotFollowDeparturePlanetAndCanPitchThroughVertical) {
    PlanetSurfaceCamera camera({{0,0,0},1000},{0,0,2},{2000,0,0},60);
    camera.followFlight({0,3000,0},{0,1,0},true);
    const auto p=camera.position(),view=camera.direction();
    camera.followPlanet({{1000,0,0},1000}, {5000,0,0});
    EXPECT_EQ(camera.position(),p); EXPECT_EQ(camera.direction(),view);
    camera.look(0,-400); // Two radians: past the old near-vertical pitch clamp.
    EXPECT_NEAR(glm::length(camera.direction()),1,1e-12);
    EXPECT_NEAR(glm::dot(camera.direction(),camera.up()),0,1e-12);
    EXPECT_NE(camera.direction(),view);
}
TEST(FlightNavigation, MotionWorldReplayAndBodyReframingPreserveWorldPose) {
    auto bodies=system(); AstronautMotion motion;
    motion.setFlightWorld(bodies,1); motion.update(ground({0,1000,0}),{1,0,0},0,ground);
    motion.pressSpace(); motion.setFlightControl({1,0});
    motion.update(ground(motion.pose().root),{1,0,0},.5,ground);
    const auto saved=motion.pose(); ASSERT_TRUE(saved.navigation); EXPECT_TRUE(saved.boosting);
    AstronautMotion replay; replay.setFlightWorld(bodies,1); replay.restore(saved); replay.setFlightControl({1,0});
    motion.update(ground(saved.root),{1,0,0},.2,ground);
    replay.update(ground(saved.root),{1,0,0},.2,ground);
    EXPECT_EQ(motion.pose().root,replay.pose().root); EXPECT_EQ(motion.pose().velocity,replay.pose().velocity);
    const auto n=*motion.pose().navigation;
    motion.reframeFlight(2);
    EXPECT_LT(glm::length(bodies[2].position+bodies[2].orientation*motion.pose().root-n.position),1e-9);
    EXPECT_EQ(motion.pose().navigation->velocity,n.velocity);
    auto bad=saved; bad.navigation->velocity.x=std::numeric_limits<double>::infinity();
    EXPECT_THROW(replay.restore(bad),std::invalid_argument);
    bad=saved; bad.navigation->referenceBody=100;
    EXPECT_THROW(replay.restore(bad),std::invalid_argument);
}
TEST(FlightNavigation, PoweredJourneyLeavesDepartureFrameAndEntersMoonFrame) {
    const auto bodies=system();
    FlightState s{{0,1700,0},{0,0,0},{0,1,0},1,false};
    glm::dvec3 axis(0,1,0);
    bool sawSpace=false,arrived=false;
    for (int i=0;i<20000;++i) {
        const auto look=s.position.x<3000 ? glm::dvec3(1,0,0) : glm::normalize(glm::dvec3(6000,700,0)-s.position);
        const auto step=FlightNavigation::advance(s,bodies,look,{0,0,1},{1,0},false,axis,6000,.01);
        axis=step.suitUp; sawSpace|=s.outerSpace;
        EXPECT_FALSE(step.landedBody);
        if (s.referenceBody==2 && !s.outerSpace) { arrived=true; break; }
    }
    EXPECT_TRUE(sawSpace); EXPECT_TRUE(arrived);
    EXPECT_GT(glm::length(s.velocity),50);
}
TEST(FlightNavigation, OrientationBoundaryDoesNotSwitchAtmosphericDragOff) {
    auto bodies=system(); bodies[1].environment.atmosphere.enabled=true;
    bodies[1].environment.planetMassKg=9.81*1000*1000/6.67430e-11;
    FlightState near{{0,2399.999,0},{100,0,0},{0,1,0},1,false};
    FlightState far=near; far.position.y=2400.001;
    FlightNavigation::advance(near,bodies,{1,0,0},{0,0,1},{0,0},false,{0,1,0},6000,.01);
    FlightNavigation::advance(far,bodies,{1,0,0},{0,0,1},{0,0},false,{0,1,0},6000,.01);
    EXPECT_TRUE(far.outerSpace); EXPECT_LT(far.velocity.x,100);
    EXPECT_NEAR(far.velocity.x,near.velocity.x,1e-6);
}
TEST(FlightNavigation, MovingBodyIsSampledThroughoutSlowFramesWithoutFalseLanding) {
    auto bodies=system();
    for (auto& b:bodies) { b.environment.planetMassKg=0; b.velocity={0,2000,0}; }
    AstronautMotion motion; motion.setFlightWorld(bodies,1);
    motion.update(ground({0,1000,0}),{1,0,0},0,ground);
    auto p=motion.pose(); p.airborne=true; p.root={0,1500,0}; p.flightHeight=500;
    p.navigation=FlightState{{0,1500,0},{0,2000,0},{0,1,0},1,false};
    motion.restore(p);
    for (auto& b:bodies) b.position.y+=1000; // One half-second of prescribed body motion.
    motion.setFlightWorld(bodies,1);
    motion.update(ground(p.root),{1,0,0},.5,ground);
    ASSERT_TRUE(motion.pose().airborne); ASSERT_TRUE(motion.pose().navigation);
    EXPECT_NEAR(motion.pose().navigation->position.y,2500,1e-9);
    EXPECT_NEAR(motion.pose().root.y,1500,1e-9);
    EXPECT_NEAR(motion.pose().velocity.y,0,1e-9);
}
TEST(FlightNavigation, CoastingSuitAxisStaysInertialWhileItsReferencePlanetSpins) {
    auto bodies=system(); for (auto& b:bodies) b.environment.planetMassKg=0;
    AstronautMotion motion; motion.setFlightWorld(bodies,1);
    motion.update(ground({0,1000,0}),{1,0,0},0,ground);
    auto p=motion.pose(); p.airborne=true; p.root={0,1700,0}; p.flightHeight=700;
    p.navigation=FlightState{{0,1700,0},{0,0,0},{0,1,0},1,false};
    motion.restore(p);
    bodies[1].angularVelocity={0,0,glm::pi<double>()};
    bodies[1].orientation=glm::dmat3(glm::rotate(glm::dmat4(1),glm::pi<double>()/2,glm::dvec3(0,0,1)));
    motion.setFlightWorld(bodies,1);
    motion.update(ground(p.root),glm::transpose(bodies[1].orientation)*glm::dvec3(1,0,0),.5,ground);
    ASSERT_TRUE(motion.pose().navigation);
    EXPECT_EQ(motion.pose().navigation->suitUp,(glm::dvec3(0,1,0)));
    EXPECT_LT(glm::length(bodies[1].orientation*motion.pose().suitUp-glm::dvec3(0,1,0)),1e-12);
}
TEST(FlightNavigation, NearBodyAirborneChaseDoesNotGoUndergroundWhenLookingUp) {
    const auto bodies=system(); AstronautMotion motion; motion.setFlightWorld(bodies,1);
    motion.update(ground({0,1000,0}),{1,0,0},0,ground);
    auto p=motion.pose(); p.airborne=true; p.root={0,1001,0}; p.flightHeight=1;
    p.navigation=FlightState{{0,1001,0},{0,0,0},{0,1,0},1,false};
    motion.restore(p);
    const auto camera=motion.chase({0,1,0},ground,glm::dvec3(0,0,1));
    EXPECT_GE(glm::length(camera.eye),1000.45);
    EXPECT_GT(glm::length(camera.target-camera.eye),.2);
    EXPECT_NEAR(glm::length(camera.up),1,1e-12);
    EXPECT_THROW(motion.reframeFlight(0),std::invalid_argument);
}
TEST(FlightNavigation, CoastingAstronautAlsoAlignsItsSuitOnMoonApproach) {
    const auto bodies=system(); FlightState s{{6000,600,0},{0,0,0},{1,0,0},1,true};
    s.suitUp={1,0,0}; glm::dvec3 axis(1,0,0);
    for (int i=0;i<500;++i) {
        const auto step=FlightNavigation::advance(s,bodies,{0,0,-1},{1,0,0},{0,0},false,axis,6000,.01);
        axis=step.suitUp; EXPECT_EQ(step.thrust,0);
    }
    EXPECT_EQ(s.referenceBody,2u); EXPECT_GT(s.up.y,.9999); EXPECT_GT(axis.y,.9999);
}
TEST(FlightNavigation, MoonBoundarySelectsTheMoonEvenInsideItsLargerParentRegion) {
    auto bodies=system(); bodies[2].position={2500,0,0}; bodies[2].environment.radiusMeters=270;
    FlightState s{{1900,0,0},{0,0,0},{1,0,0},1,false};
    FlightNavigation::orient(s,bodies,.01);
    EXPECT_FALSE(s.outerSpace); EXPECT_EQ(s.referenceBody,2u);
}
TEST(FlightNavigation, CapturedWorldViewRestoresWithoutRenormalizingItsComponents) {
    PlanetSurfaceCamera source({{0,0,0},1000},{30,40,2},{2000,0,0},60);
    source.followFlight({1000,2000,3000},{0,1,0},true); source.look(81,123);
    const auto direction=source.direction(),up=source.up();
    PlanetSurfaceCamera replay({{6000,0,0},500},{-10,15,2},{2000,0,0},60);
    replay.setWorldView(direction,up); replay.followFlight({1000,2000,3000},{0,1,0},true);
    EXPECT_EQ(replay.direction(),direction); EXPECT_EQ(replay.up(),up);
    EXPECT_THROW(replay.setWorldView({0,0,0},up),std::invalid_argument);
}
TEST(FlightNavigation, LaunchIncludesBothOrbitalAndSpinVelocity) {
    auto bodies=system(); bodies[1].velocity={30,50,0}; bodies[1].angularVelocity={0,0,.05};
    AstronautMotion motion; motion.setFlightWorld(bodies,1);
    motion.update(ground({0,1000,0}),{1,0,0},0,ground);
    motion.pressSpace(); motion.update(ground(motion.pose().root),{1,0,0},0,ground);
    ASSERT_TRUE(motion.pose().navigation);
    EXPECT_EQ(motion.pose().navigation->velocity,(glm::dvec3(-20,60,0)));
}
TEST(FlightNavigation, NearBodySteeringControlsTheFullVelocityVector) {
    auto bodies=system(); for (auto& b:bodies) b.environment.planetMassKg=0;
    FlightState s{{0,1800,0},{0,80,0},{0,1,0},1,false};
    const auto axis=glm::normalize(glm::dvec3(100,-80,0));
    const auto step=FlightNavigation::advance(s,bodies,{1,0,0},{0,0,1},{1,0},false,axis,6000,.01);
    EXPECT_GT(step.thrust,0); EXPECT_GT(s.velocity.x,0); EXPECT_LT(s.velocity.y,80);
}

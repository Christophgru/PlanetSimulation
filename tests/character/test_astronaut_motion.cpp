#include "rendering/character/AstronautMotion.h"
#include "rendering/camera/CameraInput.h"
#include <gtest/gtest.h>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>

namespace {
rendering::GroundContact sphere(const glm::dvec3& v) {
    const auto radial=glm::normalize(v);
    return {1000.0*radial,radial};
}
glm::dvec3 point(double distance) { return {0,1000*std::cos(distance/1000),-1000*std::sin(distance/1000)}; }
}
TEST(AstronautMotion, RestingFeetRemainFixedAndIKReachesBothSoles) {
    rendering::AstronautMotion motion;
    motion.update(sphere(point(0)),{0,0,-1},0,sphere);
    const auto feet=motion.pose().feet;
    for (int i=0;i<10;++i) motion.update(sphere(point(0)),{0,0,-1},.05,sphere);
    EXPECT_FALSE(motion.animating());
    for (int leg=0;leg<2;++leg) {
        const auto& p=motion.pose();
        EXPECT_EQ(p.feet[leg].contact.position,feet[leg].contact.position);
        EXPECT_TRUE(p.legReached[leg]);
        EXPECT_LT(glm::length(p.ankles[leg]-(p.feet[leg].contact.position+p.feet[leg].contact.normal*.09)),1e-5);
        EXPECT_NEAR(glm::length(p.knees[leg]-p.hips[leg]),std::hypot(.47,.10),1e-6);
        EXPECT_NEAR(glm::length(p.ankles[leg]-p.knees[leg]),std::hypot(.47,.10),1e-6);
    }
}
TEST(AstronautMotion, WalkingAlternatesLiftedFeetWithoutSlidingTheStanceContact) {
    rendering::AstronautMotion motion;
    int swings[2]={0,0}, locked=0;
    bool sawLift=false;
    for (int frame=0;frame<150;++frame) {
        const bool ready=motion.ready();
        const auto previous=motion.pose();
        motion.update(sphere(point(frame*.04)),{0,0,-1},.02,sphere);
        const auto& p=motion.pose();
        EXPECT_FALSE(!p.feet[0].planted() && !p.feet[1].planted());
        for (int leg=0;leg<2;++leg) {
            if (ready && previous.feet[leg].planted() && !p.feet[leg].planted()) ++swings[leg];
            if (ready && previous.feet[leg].planted() && p.feet[leg].planted()) {
                EXPECT_EQ(previous.feet[leg].contact.position,p.feet[leg].contact.position);
                ++locked;
            }
            sawLift|=glm::length(p.feet[leg].contact.position)>1000.01;
            EXPECT_TRUE(p.legReached[leg]) << "frame " << frame << " leg " << leg;
            EXPECT_LT(glm::length(p.ankles[leg]-(p.feet[leg].contact.position+p.feet[leg].contact.normal*.09)),1e-5);
        }
    }
    EXPECT_GT(swings[0],3); EXPECT_GT(swings[1],3); EXPECT_GT(locked,100); EXPECT_TRUE(sawLift);
    const auto stopped=motion.pose().root;
    for (int i=0;i<20;++i) motion.update(sphere(stopped),{0,0,-1},.02,sphere);
    EXPECT_FALSE(motion.animating());
}
TEST(AstronautMotion, MovementAndPoseStayFiniteAcrossBothPolesAndDirectionChanges) {
    for (double sign:{-1.,1.}) {
        rendering::AstronautMotion motion;
        for (int frame=0;frame<80;++frame) {
            const double offset=(frame-40)*.04;
            const auto root=glm::dvec3(1000*std::sin(offset/1000),0,sign*1000*std::cos(offset/1000));
            motion.update(sphere(root),{1,0,0},.02,sphere);
            const auto& p=motion.pose();
            EXPECT_NEAR(glm::length(p.forward),1,1e-10);
            EXPECT_NEAR(glm::dot(p.forward,p.up),0,1e-10);
            EXPECT_NEAR(glm::determinant(glm::dmat3(p.right,p.up,-p.forward)),1,1e-10);
        }
        // Backtracking is a new step; existing stance feet are still locked.
        const auto feet=motion.pose().feet;
        motion.update(sphere(motion.pose().root-glm::dvec3(.04,0,0)),{-1,0,0},.02,sphere);
        for (int leg=0;leg<2;++leg) if (feet[leg].planted() && motion.pose().feet[leg].planted())
            EXPECT_EQ(feet[leg].contact.position,motion.pose().feet[leg].contact.position);
    }
}
TEST(AstronautMotion, GroundRevisionAdjustsHeightWhilePreservingTheSurfaceLocation) {
    rendering::AstronautMotion motion;
    motion.update(sphere(point(0)),{0,0,-1},0,sphere);
    const auto before=motion.pose().feet;
    motion.refreshContacts([](const glm::dvec3& radial) {
        const auto n=glm::normalize(radial); return rendering::GroundContact{n*1000.1,n};
    });
    for (int leg=0;leg<2;++leg) {
        const auto after=motion.pose().feet[leg].contact.position;
        EXPECT_NEAR(glm::length(after),1000.1,1e-10);
        EXPECT_LT(glm::length(glm::normalize(after)-glm::normalize(before[leg].contact.position)),1e-12);
    }
}
TEST(AstronautMotion, ChaseClearsTheGroundAndShortensAtInterveningMountains) {
    rendering::AstronautMotion motion;
    motion.update(sphere(point(0)),{0,0,-1},0,sphere);
    const auto open=motion.chase({0,0,-1},sphere);
    EXPECT_GT(glm::length(open.eye),1000.45);
    const rendering::GroundQuery ridge=[](const glm::dvec3& v) {
        const auto n=glm::normalize(v);
        const double height=n.z>.0007 && n.z<.0025 ? 2.0 : 0;
        return rendering::GroundContact{n*(1000+height),n};
    };
    const auto blocked=motion.chase({0,0,-1},ridge);
    EXPECT_LT(glm::length(blocked.eye-blocked.target),glm::length(open.eye-open.target));
    for (int i=0;i<=40;++i) {
        const auto p=glm::mix(blocked.target,blocked.eye,i/40.0);
        EXPECT_GE(glm::length(p),glm::length(ridge(p).position));
    }
}
TEST(AstronautMotion, ReplayValidatesAndRestoresTheCompleteWalkingPose) {
    rendering::AstronautMotion motion;
    for (int i=0;i<10;++i) motion.update(sphere(point(i*.05)),{0,0,-1},.025,sphere);
    const auto p=motion.pose();
    rendering::AstronautMotion replay;
    replay.restore(p);
    EXPECT_EQ(replay.pose().root,p.root);
    EXPECT_EQ(replay.pose().knees,p.knees);
    EXPECT_EQ(replay.pose().feet[0].contact.position,p.feet[0].contact.position);
    auto bad=p; bad.feet[0].progress=std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(replay.restore(bad),std::invalid_argument);
}
TEST(AstronautMotion, WalkingAndSprintingAtSixAndTwelveMetersKeepStanceFeetLocked) {
    for (double speed:{6.,12.}) {
        rendering::AstronautMotion motion;
        for (int frame=0;frame<120;++frame) {
            motion.update(sphere(point(frame*speed*.02)),{0,0,-1},.02,sphere);
            const auto& p=motion.pose();
            for (int leg=0;leg<2;++leg) {
                EXPECT_TRUE(p.legReached[leg]) << speed << " m/s frame " << frame << " leg " << leg;
                if (p.feet[leg].planted()) EXPECT_NEAR(glm::length(p.feet[leg].contact.position),1000,1e-9);
            }
        }
    }
}
TEST(AstronautFlight, JumpUsesGravityAndHoldingTheFirstPressDoesNotEngageTheJetpack) {
    double maxima[2]={0,0};
    for (int run=0;run<2;++run) {
        rendering::AstronautMotion motion;
        motion.setGravity(run==0 ? 9.81 : 19.62);
        motion.update(sphere(point(0)),{0,0,-1},0,sphere);
        motion.pressSpace(); motion.holdBoost(true);
        for (int i=0;i<300;++i) {
            motion.update(sphere(point(0)),{0,0,-1},.01,sphere);
            maxima[run]=std::max(maxima[run],motion.pose().flightHeight);
            EXPECT_FALSE(motion.pose().boosting);
            EXPECT_GE(motion.pose().flightHeight,0);
        }
        EXPECT_FALSE(motion.pose().airborne);
        EXPECT_NEAR(motion.pose().flightHeight,0,1e-10);
        for (const auto& foot:motion.pose().feet) EXPECT_TRUE(foot.planted());
    }
    EXPECT_NEAR(maxima[0],100/(2*9.81),.002);
    EXPECT_NEAR(maxima[1],100/(2*19.62),.002);
}
TEST(AstronautFlight, SecondSpaceEnablesBoostAndReleaseFallsBackToALanding) {
    rendering::AstronautMotion motion;
    motion.setGravity(9.81);
    motion.update(sphere(point(0)),{0,0,-1},0,sphere);
    motion.pressSpace(); motion.update(sphere(point(0)),{0,0,-1},.1,sphere);
    const double jumped=motion.pose().flightHeight;
    motion.pressSpace(); motion.holdBoost(true);
    for (int frame=0;frame<60;++frame)
        motion.update(sphere(point(frame*.06)),{0,0,-1},.01,sphere);
    EXPECT_TRUE(motion.pose().airborne); EXPECT_TRUE(motion.pose().boosting);
    EXPECT_GT(motion.pose().flightHeight,jumped+8);
    for (const auto& foot:motion.pose().feet) EXPECT_FALSE(foot.planted());
    const auto airborne=motion.pose();
    rendering::AstronautMotion replay; replay.restore(airborne);
    EXPECT_EQ(replay.pose().flightHeight,airborne.flightHeight); EXPECT_TRUE(replay.pose().boosting);
    motion.holdBoost(false);
    const auto root=sphere(motion.pose().root);
    for (int i=0;i<1500;++i) motion.update(root,{0,0,-1},.01,sphere);
    EXPECT_FALSE(motion.pose().airborne); EXPECT_FALSE(motion.pose().boosting);
    EXPECT_NEAR(glm::length(motion.pose().root),1000,1e-8);
}
TEST(AstronautFlight, SpinReducesRadialGravityAndRaisedTerrainStopsTheDescent) {
    EXPECT_LT(rendering::AstronautMotion::surfaceGravity(1e18,1000,60,0),
              rendering::AstronautMotion::surfaceGravity(1e18,1000,0,0));
    EXPECT_NEAR(rendering::AstronautMotion::surfaceGravity(1e18,1000,60,glm::pi<double>()/2),
                rendering::AstronautMotion::surfaceGravity(1e18,1000,0,0),1e-10);
    rendering::AstronautMotion motion;
    motion.update(sphere(point(0)),{0,0,-1},0,sphere);
    motion.pressSpace(); motion.update(sphere(point(0)),{0,0,-1},.1,sphere);
    const rendering::GroundQuery plateau=[](const glm::dvec3& v) {
        const auto up=glm::normalize(v); return rendering::GroundContact{up*1002.0,up};
    };
    motion.update(plateau(point(0)),{0,0,-1},.05,plateau);
    EXPECT_GE(glm::length(motion.pose().root),1002);
}
TEST(AstronautInput, ShiftDoublesOnlyCameraFourWalkingSpeed) {
    OrbitCamera orbit({0,0,0},{0,0,3000});
    PlanetSurfaceCamera surface({{0,0,0},1000},{0,0,2},{2000,0,0},60,80);
    surface.setDirectionNed({1,0,0});
    CameraInput input(orbit,&surface); input.setThirdPersonWalkSpeed(6); input.selectThirdPerson();
    const auto start=surface.position(); input.update({true,false,false,false,false},.1);
    EXPECT_NEAR(glm::length(surface.position()-start),.6,1e-5);
    const auto walk=surface.position(); input.update({true,false,false,false,true},.1);
    EXPECT_NEAR(glm::length(surface.position()-walk),1.2,1e-5);
    input.selectSurface(); const auto firstPerson=surface.position();
    input.update({true,false,false,false,true},.1);
    EXPECT_NEAR(glm::length(surface.position()-firstPerson),8,1e-4);
}
TEST(AstronautInput, CameraFourSharesPositionButWalksAtItsOwnSpeedAndReloadsSafely) {
    OrbitCamera orbit({0,0,0},{0,0,3000});
    PlanetSurfaceCamera surface({{0,0,0},1000},{0,0,2},{2000,0,0},60,80);
    surface.setDirectionNed({1,0,0});
    CameraInput input(orbit,&surface);
    input.setThirdPersonWalkSpeed(2);
    const auto start=surface.position();
    input.selectThirdPerson();
    EXPECT_EQ(input.mode(),CameraMode::ThirdPerson); EXPECT_EQ(surface.position(),start);
    EXPECT_TRUE(input.surfacePointerCaptured());
    input.update({true,false,false,false},.1);
    EXPECT_NEAR(glm::length(surface.position()-start),.2,1e-6);
    input.releaseCursor(); EXPECT_FALSE(input.surfacePointerCaptured());
    input.selectThirdPerson();
    auto replacement=surface;
    input.rebind(&replacement,nullptr); EXPECT_EQ(input.mode(),CameraMode::ThirdPerson);
    const auto dir=replacement.direction();
    input.moveCursor(10,10); input.moveCursor(20,15); EXPECT_NE(replacement.direction(),dir);
    input.selectSurface(); EXPECT_EQ(surface.position(),replacement.position());
    input.selectThirdPerson(); input.rebind(nullptr,nullptr); EXPECT_EQ(input.mode(),CameraMode::Orbit);
}

#include "rendering/character/AstronautMotion.h"
#include <ozz/animation/runtime/ik_two_bone_job.h>
#include <ozz/base/maths/simd_quaternion.h>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace rendering {
namespace {
glm::dvec3 tangent(const glm::dvec3& v,const glm::dvec3& up,const glm::dvec3& fallback) {
    const auto t=v-up*glm::dot(v,up);
    return glm::length(t)>1e-8 ? glm::normalize(t) : fallback;
}
ozz::math::SimdFloat4 simd(const glm::dvec3& v) {
    return ozz::math::simd_float4::Load(v.x,v.y,v.z,0);
}
glm::dquat quaternion(const ozz::math::SimdQuaternion& q) {
    float v[4]; ozz::math::StorePtrU(q.xyzw,v);
    return {v[3],v[0],v[1],v[2]};
}
}
bool AstronautMotion::animating() const {
    return ready_ && (!pose_.feet[0].planted() || !pose_.feet[1].planted());
}
double AstronautMotion::surfaceGravity(double massKg,double radiusMeters,double spinPeriod,double latitudeRadians) {
    const double omega=spinPeriod==0 ? 0 : 2*glm::pi<double>()/spinPeriod;
    const double latitude=std::cos(latitudeRadians);
    return std::max(.25,6.67430e-11*massKg/(radiusMeters*radiusMeters)-
                         omega*omega*radiusMeters*latitude*latitude);
}
void AstronautMotion::setGravity(double value) {
    if (!std::isfinite(value) || value<0 || value>1e8)
        throw std::invalid_argument("Astronaut gravity must be finite and nonnegative");
    gravity_=value;
}
void AstronautMotion::restore(const AstronautPose& pose) {
    const auto valid=[](const glm::dvec3& v) { return std::isfinite(glm::length(v)); };
    if (!valid(pose.root) || glm::length(pose.root)<1e-6 ||
        !valid(pose.up) || !valid(pose.forward) || !valid(pose.right) ||
        std::abs(glm::length(pose.up)-1)>1e-6 || std::abs(glm::length(pose.forward)-1)>1e-6 ||
        std::abs(glm::length(pose.right)-1)>1e-6 || std::abs(glm::dot(pose.up,pose.forward))>1e-6 ||
        std::abs(glm::dot(pose.up,pose.right))>1e-6 || std::abs(glm::dot(pose.forward,pose.right))>1e-6 ||
        !std::isfinite(pose.armSwing) || !std::isfinite(pose.walkedMeters) || pose.walkedMeters<0 ||
        !std::isfinite(pose.flightHeight) || pose.flightHeight<0 || !std::isfinite(pose.verticalVelocity) ||
        !std::isfinite(pose.effectSeconds) || !std::isfinite(pose.boostPulse) || pose.boostPulse<0)
        throw std::invalid_argument("Invalid astronaut replay basis");
    for (int leg=0;leg<2;++leg) {
        const auto& f=pose.feet[leg];
        if (!valid(f.contact.position) || !valid(f.contact.normal) || !valid(f.forward) ||
            !valid(f.start) || !valid(f.target) || !valid(pose.hips[leg]) ||
            !valid(pose.knees[leg]) || !valid(pose.ankles[leg]) ||
            std::abs(glm::length(f.contact.normal)-1)>1e-6 ||
            !std::isfinite(f.progress) || f.progress<0 || f.progress>1 ||
            !std::isfinite(f.duration) || f.duration<.01 || f.duration>1)
            throw std::invalid_argument("Invalid astronaut replay contacts");
    }
    pose_=pose; ready_=true;
    walked_=pose.walkedMeters;
    if (!pose.feet[0].planted()) nextFoot_=1;
    else if (!pose.feet[1].planted()) nextFoot_=0;
    else nextFoot_=glm::dot(pose.root-pose.feet[0].contact.position,pose.forward)>
                       glm::dot(pose.root-pose.feet[1].contact.position,pose.forward) ? 0 : 1;
}
void AstronautMotion::refreshContacts(const GroundQuery& ground) {
    if (!ready_ || pose_.airborne) return;
    for (auto& foot:pose_.feet) {
        if (foot.planted()) foot.contact=ground(foot.contact.position);
        else foot.target=ground(foot.target).position;
    }
}
void AstronautMotion::update(const GroundContact& root,const glm::dvec3& lookForward,
                             double elapsed,const GroundQuery& ground) {
    if (!std::isfinite(elapsed) || elapsed<0 || !std::isfinite(glm::length(root.position)) ||
        glm::length(root.position)<1e-6 || !std::isfinite(glm::length(lookForward)))
        throw std::invalid_argument("Astronaut requires finite root, view and elapsed time");
    // Substeps keep fast walking, jump collision and stance reach stable even
    // when presentation is slow. Ground/flight use local time, never orbit time.
    const double horizontalMove=ready_ ? glm::length(root.position-glm::normalize(pose_.root)*glm::length(root.position)) : 0;
    if (ready_ && (elapsed>.010001 || horizontalMove>.060001) && horizontalMove<1.5) {
        const int steps=std::min(500,int(std::ceil(std::max(elapsed/.01,horizontalMove/.06))));
        const auto start=ground(pose_.root).position;
        for (int i=1;i<=steps;++i)
            update(ground(glm::mix(start,root.position,double(i)/steps)),lookForward,elapsed/steps,ground);
        return;
    }
    const auto up=glm::normalize(root.position);
    const auto previous=pose_.root;
    const auto delta=root.position-previous;
    const double distance=ready_ ? glm::length(delta-up*glm::dot(delta,up)) : 0;
    // Explicit camera relocation/scene reload is a new placement, not a step.
    if (ready_ && distance>1.5) ready_=false;
    if (!ready_) {
        const auto fallback=glm::normalize(glm::cross(up,std::abs(up.z)<.9 ? glm::dvec3(0,0,1) : glm::dvec3(0,1,0)));
        pose_.forward=tangent(lookForward,up,fallback);
        pose_.flightHeight=pose_.verticalVelocity=pose_.effectSeconds=pose_.boostPulse=0;
        pose_.airborne=pose_.jetpackArmed=pose_.boosting=false;
    } else pose_.forward=tangent(distance>1e-6 ? delta : pose_.forward,up,pose_.forward);
    pose_.root=root.position; pose_.up=up;
    pose_.right=glm::normalize(glm::cross(pose_.forward,up));
    if (!ready_) {
        for (int leg=0;leg<2;++leg) {
            auto& foot=pose_.feet[leg];
            foot.contact=ground(root.position+pose_.right*(leg==0 ? -.17 : .17));
            foot.forward=pose_.forward; foot.progress=1;
        }
        ready_=true; nextFoot_=0; walked_=0;
    }
    const bool moving=distance>1e-6;
    walked_+=distance;
    pose_.walkedMeters=walked_;
    pose_.armSwing=moving ? .16*std::sin(walked_*9) : 0;
    const bool wasAirborne=pose_.airborne;
    if (wasAirborne) pose_.flightHeight+=glm::length(previous)-pose_.flightHeight-glm::length(root.position);
    while (spacePresses_) {
        --spacePresses_;
        if (!pose_.airborne) {
            pose_.airborne=true; pose_.verticalVelocity=10;
        } else {
            pose_.jetpackArmed=true; pose_.boostPulse=.18;
            pose_.verticalVelocity=std::min(35.0,pose_.verticalVelocity+8);
        }
    }
    pose_.boosting=pose_.airborne && pose_.jetpackArmed && (boostHeld_ || pose_.boostPulse>0);
    pose_.boostPulse=std::max(0.0,pose_.boostPulse-elapsed);
    pose_.effectSeconds+=elapsed;
    if (pose_.airborne) {
        const double acceleration=pose_.boosting ? 20.0 : -gravity_;
        pose_.flightHeight+=pose_.verticalVelocity*elapsed+.5*acceleration*elapsed*elapsed;
        pose_.verticalVelocity=std::clamp(pose_.verticalVelocity+acceleration*elapsed,-50.0,35.0);
        if (pose_.flightHeight<=0) {
            pose_.flightHeight=0;
            if (pose_.verticalVelocity<=0) {
                pose_.verticalVelocity=0;
                pose_.airborne=pose_.jetpackArmed=pose_.boosting=false;
            }
        }
    }
    pose_.root=root.position+up*pose_.flightHeight;
    if (pose_.airborne) {
        for (int leg=0;leg<2;++leg) {
            auto& foot=pose_.feet[leg];
            foot.contact={pose_.root+pose_.right*(leg==0 ? -.17 : .17)-pose_.forward*.06,up};
            foot.forward=pose_.forward; foot.progress=.5;
        }
        solveLegs();
        return;
    }
    if (wasAirborne) for (int leg=0;leg<2;++leg) {
        auto& foot=pose_.feet[leg];
        foot.contact=ground(root.position+pose_.right*(leg==0 ? -.17 : .17));
        foot.forward=pose_.forward; foot.progress=1;
    }
    // Only the swinging foot moves. Its opposite contact remains bit-identical
    // throughout stance; neither spin nor orbital time enters this state.
    for (auto& foot:pose_.feet) if (!foot.planted()) {
        foot.progress=std::min(1.0,foot.progress+elapsed/foot.duration);
        if (foot.progress>1-1e-12) foot.progress=1;
        const double t=foot.progress, smooth=t*t*(3-2*t);
        foot.contact=ground(glm::mix(foot.start,foot.target,smooth));
        foot.contact.position+=glm::normalize(foot.contact.position)*(.15*std::sin(glm::pi<double>()*t));
        if (foot.planted()) foot.contact=ground(foot.target);
    }
    if (moving && !animating()) {
        const int leg=nextFoot_;
        const auto nominal=root.position+pose_.right*(leg==0 ? -.17 : .17);
        const auto separation=nominal-pose_.feet[leg].contact.position;
        if (glm::length(separation-up*glm::dot(separation,up))>.14) {
            auto& foot=pose_.feet[leg];
            foot.start=foot.contact.position;
            foot.target=ground(nominal+pose_.forward*.32).position;
            foot.forward=pose_.forward; foot.progress=0; nextFoot_=1-leg;
            foot.duration=std::clamp(.40/std::max(2.0,elapsed>0 ? distance/elapsed : 2.0),.02,.24);
        }
    }
    solveLegs();
}
void AstronautMotion::solveLegs() {
    const glm::dmat3 basis(pose_.right,pose_.up,-pose_.forward);
    const auto inverse=glm::transpose(basis);
    // The rest chain is slightly bent, with knees pointing toward local -Z.
    // Work near the actor origin before converting to the library's floats.
    for (int leg=0;leg<2;++leg) {
        const glm::dvec3 hip(leg==0 ? -.17 : .17,.75,0);
        const glm::dvec3 thigh(0,-.47,-.10), shin(0,-.47,.10);
        const auto knee=hip+thigh, ankle=knee+shin;
        const auto start=ozz::math::Float4x4::Translation(simd(hip));
        const auto middle=ozz::math::Float4x4::Translation(simd(knee));
        const auto end=ozz::math::Float4x4::Translation(simd(ankle));
        const auto target=pose_.feet[leg].contact.position+
            pose_.feet[leg].contact.normal*.09;
        ozz::math::SimdQuaternion q0,q1;
        ozz::animation::IKTwoBoneJob job;
        job.start_joint=&start; job.mid_joint=&middle; job.end_joint=&end;
        job.target=simd(inverse*(target-pose_.root));
        job.pole_vector=simd({0,0,-1}); job.mid_axis=ozz::math::simd_float4::x_axis();
        job.start_joint_correction=&q0; job.mid_joint_correction=&q1;
        bool reached=false; job.reached=&reached;
        if (!job.Run()) throw std::runtime_error("Astronaut leg IK failed validation");
        const auto rotation=quaternion(q0);
        const auto kneeSolved=hip+rotation*thigh;
        const auto ankleSolved=kneeSolved+(rotation*quaternion(q1))*shin;
        pose_.hips[leg]=pose_.root+basis*hip;
        pose_.knees[leg]=pose_.root+basis*kneeSolved;
        pose_.ankles[leg]=pose_.root+basis*ankleSolved;
        pose_.legReached[leg]=reached;
    }
}
ChasePose AstronautMotion::chase(const glm::dvec3& lookDirection,const GroundQuery& ground) const {
    if (!ready_) throw std::logic_error("Chase camera requires a placed astronaut");
    const auto radial=pose_.up;
    const auto forward=tangent(lookDirection,radial,pose_.forward);
    const double pitch=std::clamp(glm::dot(glm::normalize(lookDirection),radial),-.65,.65);
    const auto target=pose_.root+radial*.95;
    auto eye=pose_.root-forward*4.0+radial*(2.6-pitch*3.0);
    const auto floor=ground(eye);
    if (glm::length(eye)<glm::length(floor.position)+.45)
        eye=glm::normalize(eye)*(glm::length(floor.position)+.45);
    // Shorten the arm at intervening terrain, with a conservative clearance.
    for (int i=1;i<=24;++i) {
        const double t=double(i)/24;
        const auto p=glm::mix(target,eye,t);
        const auto floorAt=ground(p);
        if (glm::length(p)<glm::length(floorAt.position)+.20) {
            eye=glm::mix(target,eye,double(i-1)/24); break;
        }
    }
    if (glm::length(eye-target)<.2) eye=target+radial*.5;
    return {eye,target,radial};
}
}

#include "rendering/character/effects/ExhaustParticles.h"
#include "rendering/character/flight/FlightNavigation.h"
#include "rendering/foliage/wind/WindField.h"
#include <gtest/gtest.h>
#include <limits>
using namespace rendering;
namespace {
ExhaustEmitter emitter() { ExhaustEmitter e; e.nozzles={glm::dvec3(-.12,1,.23),glm::dvec3(.12,1,.23)}; e.firing=true; return e; }
ExhaustAir vacuum(const glm::dvec3&,double) { return {}; }
}
TEST(Exhaust, StartsIncrementallyAndRetainsFadingReleaseTail) {
    ExhaustParticles p; auto e=emitter(); p.update(.024,e,e,vacuum); EXPECT_TRUE(p.state().particles.empty());
    p.update(.001,e,e,vacuum); ASSERT_EQ(p.state().particles.size(),1u); EXPECT_EQ(p.state().particles[0].age,0);
    p.update(.1,e,e,vacuum); EXPECT_EQ(p.state().particles.size(),5u);
    const auto ids=p.state().nextId; e.firing=false; p.update(.1,e,e,vacuum);
    EXPECT_EQ(p.state().nextId,ids); EXPECT_EQ(p.state().particles.size(),5u);
    p.update(1.5,e,e,vacuum); EXPECT_TRUE(p.state().particles.empty());
}
TEST(Exhaust, LifetimeSmoothlyFadesInAndOutWithoutOpaqueShells) {
    ExhaustParticle p; EXPECT_EQ(p.opacity(),0);
    p.age=.03; EXPECT_GT(p.opacity(),0); EXPECT_LT(p.opacity(),.28);
    p.age=.6; EXPECT_DOUBLE_EQ(p.opacity(),.28);
    double previous=p.opacity();
    for (int i=84;i<=140;++i) { p.age=i*.01; EXPECT_LE(p.opacity(),previous+1e-12); previous=p.opacity(); }
    EXPECT_EQ(p.opacity(),0); EXPECT_NEAR(p.radius(),.135,1e-12);
}
TEST(Exhaust, WorldParticlesInheritVelocityAndDoNotFollowEmitterRelocation) {
    ExhaustParticles p; auto e=emitter(); e.velocity={100,0,0}; p.update(.1,e,e,vacuum);
    const auto before=p.state().particles[0]; e.firing=false; e.nozzles[0].x+=10000; e.nozzles[1].x+=10000;
    p.update(.2,e,e,vacuum); const auto after=p.state().particles[0];
    EXPECT_NEAR(after.position.x-before.position.x,20,.06); EXPECT_NEAR(after.velocity.x,100,.25);
    EXPECT_LT(after.position.x,100);
}
TEST(Exhaust, DenseWindAdvectsParticlesWhileVacuumRemainsBallistic) {
    auto e=emitter(); ExhaustParticles calm,windy;
    calm.update(.05,e,e,vacuum); windy.restore(calm.state()); e.firing=false;
    calm.update(.8,e,e,vacuum);
    windy.update(.8,e,e,[](const auto&,double) { return ExhaustAir{{8,0,0},{0,-9.81,0},1.225}; });
    EXPECT_GT(windy.state().particles[0].position.x-calm.state().particles[0].position.x,4);
    EXPECT_GT(windy.state().particles[0].velocity.x,7);
}
TEST(Exhaust, CapacityAndUploadsStayBoundedDuringLongBurn) {
    ExhaustParticles p; auto e=emitter(); p.update(20,e,e,vacuum);
    EXPECT_LE(p.state().particles.size(),ExhaustParticles::capacity); EXPECT_GE(p.state().particles.size(),54u);
    EXPECT_LE(p.state().particles.size()*32,2048u); EXPECT_GT(p.state().nextId,700u);
}
TEST(Exhaust, SavedPoolAndEmissionClockContinueExactly) {
    ExhaustParticles original,replay; auto e=emitter(); original.update(.137,e,e,vacuum);
    replay.restore(original.state()); original.update(.371,e,e,vacuum); replay.update(.371,e,e,vacuum);
    EXPECT_EQ(original.state().nextId,replay.state().nextId); EXPECT_EQ(original.state().emissionPhase,replay.state().emissionPhase);
    ASSERT_EQ(original.state().particles.size(),replay.state().particles.size());
    for (std::size_t i=0;i<original.state().particles.size();++i) {
        EXPECT_EQ(original.state().particles[i].position,replay.state().particles[i].position);
        EXPECT_EQ(original.state().particles[i].age,replay.state().particles[i].age);
    }
}
TEST(Exhaust, RejectsOversizedDuplicateAndNonfiniteReplay) {
    ExhaustParticles p; ExhaustState s; s.nextId=2; s.particles.push_back({{0,0,0},{0,0,0},.1,1.4,0});
    p.restore(s); s.particles.push_back(s.particles[0]); EXPECT_THROW(p.restore(s),std::invalid_argument);
    s.particles.resize(1); s.particles[0].velocity.x=std::numeric_limits<double>::infinity(); EXPECT_THROW(p.restore(s),std::invalid_argument);
    s.particles.clear(); s.emissionPhase=.025; EXPECT_THROW(p.restore(s),std::invalid_argument);
    s.emissionPhase=0; s.particles.resize(65); EXPECT_THROW(p.restore(s),std::invalid_argument);
}
TEST(WindCoupling, UsesSharedClockSeedStrengthAndTangentDirection) {
    config::FoliageConfig f; glm::dvec3 p(32,15,1000);
    const auto a=WindField::samples(f,p,42),b=WindField::samples(f,p,42+8192);
    EXPECT_LT(glm::length(a-b),1e-6); const auto v=WindField::velocity(f,p,42);
    EXPECT_NEAR(glm::dot(v,glm::normalize(p)),0,1e-6); EXPECT_GT(glm::length(v),.1);
    f.wind_noise.seed=4; EXPECT_GT(glm::length(a-WindField::samples(f,p,42)),.01);
    f.wind_noise.speed_multiplier=0; EXPECT_EQ(WindField::samples(f,p,0),WindField::samples(f,p,100));
    f.wind_strength=0; EXPECT_EQ(WindField::velocity(f,p,42),glm::dvec3(0));
}
TEST(WindCoupling, AstronautDragRespondsToWindOnlyWhereGasExists) {
    std::vector<FlightBody> b(2); b[0].orientable=false; b[0].position={-10000,0,0};
    b[0].environment.atmosphere.enabled=false; b[1].environment.radiusMeters=1000; b[1].environment.atmosphere.enabled=true;
    b[1].wind=[](const auto&,double) { return glm::dvec3(12,0,0); };
    FlightState s{{0,1020,0},{0,0,0},{0,1,0},1,false};
    auto dry=s; auto dryBodies=b; dryBodies[1].environment.atmosphere.enabled=false;
    for (int i=0;i<200;++i) {
        FlightNavigation::advance(s,b,{1,0,0},{0,0,1},{0,0},false,{0,1,0},6000,.01);
        FlightNavigation::advance(dry,dryBodies,{1,0,0},{0,0,1},{0,0},false,{0,1,0},6000,.01);
    }
    EXPECT_GT(s.velocity.x,.5); EXPECT_EQ(dry.velocity.x,0); EXPECT_GT(s.position.x,dry.position.x);
}

TEST(Exhaust, AntipodalNozzleTurnsKeepFiniteParticleMotion) {
    ExhaustParticles p; auto before=emitter(),after=before; after.up=-before.up;
    p.update(.05,before,after,vacuum); ASSERT_EQ(p.state().particles.size(),2u);
    for (const auto& q:p.state().particles) EXPECT_TRUE(std::isfinite(glm::length(q.velocity)));
    after.up={0,0,0}; EXPECT_THROW(p.update(.05,before,after,vacuum),std::invalid_argument);
}

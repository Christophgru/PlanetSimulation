#include <gtest/gtest.h>
#include <limits>
#include "simulation/OrbitalSystem.h"
#include "rendering/PlanetSurfaceCamera.h"
#include "rendering/OrbitCamera.h"
#include "rendering/SceneTransforms.h"

namespace {

void expectVector(const glm::dvec3& actual, const glm::dvec3& expected, double tolerance = 1e-10) {
    for (int i = 0; i < 3; ++i) EXPECT_NEAR(actual[i], expected[i], tolerance) << "axis " << i;
}

config::ScenarioConfig parse(nlohmann::json raw) {
    return config::ScenarioConfig(config::Config{std::move(raw)});
}

nlohmann::json systemJson() {
    return nlohmann::json::parse(R"({
        "sun":{"name":"sun","mass_kg":1e19,"position":[3,4,5]},
        "planets":[
            {"name":"earth","mass_kg":6e17,
             "orbit":{"parent":"sun","semi_major_axis":12.5,"semi_minor_axis":10}},
            {"name":"moon","mass_kg":1e16,
             "orbit":{"parent":"earth","semi_major_axis":2.5,"semi_minor_axis":2,
                      "inclination_deg":20}}
        ]
    })");
}

config::ScenarioConfig development() {
    return config::ScenarioConfig(config::Config::load(
        std::string(PLANET_SOURCE_DIR) + "/configs/scenarios/solar_system.json"));
}

config::OrbitConfig ellipse(double a = 10.0, double b = 8.0) {
    config::OrbitConfig orbit;
    orbit.semi_major_axis = a;
    orbit.semi_minor_axis = b;
    return orbit;
}

} // namespace

TEST(SimulationClock, AdvancesInRealSecondsRegardlessOfFrameCadence) {
    simulation::SimulationClock manyFrames(37.0, 1000.0);
    simulation::SimulationClock oneFrame(37.0, 1000.0);
    for (int frame = 1; frame <= 600; ++frame) manyFrames.advanceTo(1000.0 + frame / 60.0);
    EXPECT_DOUBLE_EQ(manyFrames.seconds(), oneFrame.advanceTo(1010.0));
    EXPECT_DOUBLE_EQ(manyFrames.seconds(), 47.0);
    EXPECT_FALSE(manyFrames.paused());
}

TEST(SimulationClock, PauseFreezesEveryBodyAndSpinAndResumeHasNoCatchUp) {
    const simulation::OrbitalSystem system(development());
    simulation::SimulationClock clock(0.0, 0.0);
    clock.advanceTo(5.0);
    clock.togglePause(17.0); // Include running time up to the actual key press.
    EXPECT_TRUE(clock.paused());
    EXPECT_DOUBLE_EQ(clock.seconds(), 17.0);
    const auto frozen = system.at(clock.seconds());
    for (double wallTime : {18.0, 30.0, 600.0, 3600.0}) {
        const auto states = system.at(clock.advanceTo(wallTime));
        for (std::size_t i = 0; i < states.size(); ++i) {
            expectVector(states[i].position, frozen[i].position, 0);
            for (int axis = 0; axis < 3; ++axis)
                expectVector(states[i].orientation[axis], frozen[i].orientation[axis], 0);
        }
    }
    clock.togglePause(7200.0);
    EXPECT_FALSE(clock.paused());
    EXPECT_DOUBLE_EQ(clock.seconds(), 17.0);
    EXPECT_DOUBLE_EQ(clock.advanceTo(7201.0), 18.0);
    EXPECT_GT(glm::length(system.at(clock.seconds())[1].position - frozen[1].position), 0.0);
}

TEST(SimulationClock, RepeatedTogglesAndReloadPreservePauseState) {
    simulation::SimulationClock clock(0.0, 0.0);
    for (int cycle = 0; cycle < 10; ++cycle) {
        clock.togglePause(2.0 * cycle + 1.0);
        clock.togglePause(2.0 * cycle + 2.0);
    }
    EXPECT_DOUBLE_EQ(clock.seconds(), 10.0);
    clock.togglePause(20.0);
    clock.reset(0.0, 21.0);
    EXPECT_TRUE(clock.paused());
    EXPECT_DOUBLE_EQ(clock.advanceTo(100.0), 0.0);
    clock.togglePause(100.0);
    EXPECT_DOUBLE_EQ(clock.advanceTo(101.0), 1.0);
    clock.reset(37.0, 101.0);
    EXPECT_FALSE(clock.paused());
    EXPECT_DOUBLE_EQ(clock.advanceTo(102.0), 38.0);
}

TEST(SimulationClock, RejectsInvalidAndReversedWallTimeWithoutChangingState) {
    simulation::SimulationClock clock(10.0, 20.0);
    EXPECT_THROW(clock.advanceTo(19.0), std::invalid_argument);
    for (double bad : {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        EXPECT_THROW(clock.advanceTo(bad), std::invalid_argument);
        EXPECT_THROW(clock.togglePause(bad), std::invalid_argument);
        EXPECT_THROW(clock.reset(bad, 30.0), std::invalid_argument);
        EXPECT_THROW(clock.reset(30.0, bad), std::invalid_argument);
    }
    EXPECT_FALSE(clock.paused());
    EXPECT_DOUBLE_EQ(clock.advanceTo(21.0), 11.0);
}

TEST(SimulationClock, SpeedChangesApplyAfterTheKeyPressWithoutAnOrbitalPhaseJump) {
    simulation::SimulationClock clock(0.0, 100.0);
    EXPECT_DOUBLE_EQ(clock.speed(), 1.0);
    clock.scaleSpeed(2.0, 110.0);
    EXPECT_DOUBLE_EQ(clock.seconds(), 10.0);
    EXPECT_DOUBLE_EQ(clock.speed(), 2.0);
    EXPECT_DOUBLE_EQ(clock.advanceTo(115.0), 20.0);
    clock.scaleSpeed(0.5, 120.0);
    EXPECT_DOUBLE_EQ(clock.seconds(), 30.0);
    EXPECT_DOUBLE_EQ(clock.speed(), 1.0);
    clock.scaleSpeed(0.5, 124.0);
    EXPECT_DOUBLE_EQ(clock.seconds(), 34.0);
    EXPECT_DOUBLE_EQ(clock.speed(), 0.5);
    EXPECT_DOUBLE_EQ(clock.advanceTo(130.0), 37.0);

    const simulation::OrbitalSystem system(development());
    const auto expected = system.at(37.0);
    const auto actual = system.at(clock.seconds());
    for (std::size_t i = 0; i < system.size(); ++i) {
        expectVector(actual[i].position, expected[i].position, 0);
        for (int axis = 0; axis < 3; ++axis)
            expectVector(actual[i].orientation[axis], expected[i].orientation[axis], 0);
    }
}

TEST(SimulationClock, RepeatedSpeedStepsAndPauseRetainTheSelectedSpeedAcrossReload) {
    simulation::SimulationClock clock(0.0, 0.0);
    clock.scaleSpeed(2.0, 0.0);
    clock.scaleSpeed(2.0, 0.0);
    EXPECT_DOUBLE_EQ(clock.speed(), 4.0);
    clock.togglePause(2.0);
    EXPECT_DOUBLE_EQ(clock.seconds(), 8.0);
    clock.scaleSpeed(0.5, 10.0);
    EXPECT_TRUE(clock.paused());
    EXPECT_DOUBLE_EQ(clock.seconds(), 8.0);
    EXPECT_DOUBLE_EQ(clock.speed(), 2.0);
    clock.reset(0.0, 20.0);
    EXPECT_DOUBLE_EQ(clock.speed(), 2.0);
    EXPECT_TRUE(clock.paused());
    clock.togglePause(100.0);
    EXPECT_DOUBLE_EQ(clock.seconds(), 0.0);
    EXPECT_DOUBLE_EQ(clock.advanceTo(103.0), 6.0);
    clock.scaleSpeed(0.5, 103.0);
    clock.scaleSpeed(0.5, 103.0);
    EXPECT_DOUBLE_EQ(clock.speed(), 0.5);
    EXPECT_DOUBLE_EQ(clock.advanceTo(107.0), 8.0);
}

TEST(SimulationClock, RepeatedSpeedKeysStayBoundedAndCanReverseAwayFromTheLimits) {
    simulation::SimulationClock clock(0.0, 0.0);
    for (int press = 0; press < 2000; ++press) clock.scaleSpeed(2.0, 0.0);
    EXPECT_DOUBLE_EQ(clock.speed(), simulation::SimulationClock::maximumSpeed);
    EXPECT_DOUBLE_EQ(clock.advanceTo(1.0), 1024.0);
    clock.scaleSpeed(0.5, 1.0);
    EXPECT_DOUBLE_EQ(clock.speed(), 512.0);
    for (int press = 0; press < 2000; ++press) clock.scaleSpeed(0.5, 1.0);
    EXPECT_DOUBLE_EQ(clock.speed(), simulation::SimulationClock::minimumSpeed);
    EXPECT_DOUBLE_EQ(clock.advanceTo(1025.0), 1025.0);
    clock.scaleSpeed(2.0, 1025.0);
    EXPECT_DOUBLE_EQ(clock.speed(), 1.0 / 512.0);
}

TEST(SimulationClock, InvalidSpeedChangesLeaveTimeAndSpeedIntact) {
    simulation::SimulationClock clock(5.0, 10.0);
    for (double factor : {0.0, -1.0, std::numeric_limits<double>::infinity(),
                          std::numeric_limits<double>::quiet_NaN()}) {
        EXPECT_THROW(clock.scaleSpeed(factor, 20.0), std::invalid_argument);
        EXPECT_DOUBLE_EQ(clock.seconds(), 5.0);
        EXPECT_DOUBLE_EQ(clock.speed(), 1.0);
    }
    EXPECT_THROW(clock.scaleSpeed(2.0, 9.0), std::invalid_argument);
    EXPECT_DOUBLE_EQ(clock.speed(), 1.0);
    EXPECT_DOUBLE_EQ(clock.advanceTo(11.0), 6.0);
}

TEST(KeplerOrbit, CircularQuarterTurnsHaveConstantSpeed) {
    const double mass = 1e19;
    const double satelliteMass = 1e17;
    const double mu = 6.67430e-11 * (mass + satelliteMass) / 1e9;
    const double expectedPeriod = 2.0 * std::numbers::pi * std::sqrt(1000.0 / mu);
    simulation::KeplerOrbit orbit(ellipse(10, 10), mass, satelliteMass, 1000);
    EXPECT_NEAR(orbit.periodSeconds(), expectedPeriod, 1e-10);
    for (int quarter = 0; quarter <= 4; ++quarter) {
        const double angle = quarter * std::numbers::pi / 2.0;
        const auto state = orbit.at(expectedPeriod * quarter / 4.0);
        expectVector(state.position, {10 * std::cos(angle), 10 * std::sin(angle), 0});
        EXPECT_NEAR(glm::length(state.velocity), std::sqrt(mu / 10.0), 1e-12);
        EXPECT_NEAR(glm::dot(state.position, state.velocity), 0.0, 1e-12);
    }
}

TEST(KeplerOrbit, EllipseUsesFocusAndCorrectPeriapsisAndApoapsisSpeeds) {
    simulation::KeplerOrbit orbit(ellipse(), 1e19, 1e17, 1000);
    const auto near = orbit.at(0);
    const auto far = orbit.at(orbit.periodSeconds() / 2);
    expectVector(near.position, {4, 0, 0});
    expectVector(far.position, {-16, 0, 0});
    EXPECT_NEAR(glm::length(near.velocity) / glm::length(far.velocity), 4.0, 1e-12);
    EXPECT_NEAR(glm::length(near.velocity),
        std::sqrt(orbit.gravitationalParameter() * (2.0 / 4.0 - 1.0 / 10.0)), 1e-12);
    // A quarter of the period is not a quarter of the ellipse parameter angle.
    EXPECT_LT(orbit.at(orbit.periodSeconds() / 4).position.x, -6.0);
}

TEST(KeplerOrbit, MassAndMajorAxisDeterminePeriodWhileMinorAxisDeterminesShape) {
    simulation::KeplerOrbit base(ellipse(), 1e19, 1e17, 1000);
    simulation::KeplerOrbit heavier(ellipse(), 4e19, 4e17, 1000);
    simulation::KeplerOrbit larger(ellipse(40, 32), 1e19, 1e17, 1000);
    simulation::KeplerOrbit narrower(ellipse(10, 2), 1e19, 1e17, 1000);
    EXPECT_NEAR(heavier.periodSeconds(), base.periodSeconds() / 2, 1e-10);
    EXPECT_NEAR(larger.periodSeconds(), base.periodSeconds() * 8, 1e-10);
    EXPECT_DOUBLE_EQ(narrower.periodSeconds(), base.periodSeconds());
    EXPECT_GT(glm::length(narrower.at(0).velocity), glm::length(base.at(0).velocity));
    simulation::KeplerOrbit heavySatellite(ellipse(), 1e19, 1e19, 1000);
    EXPECT_LT(heavySatellite.periodSeconds(), base.periodSeconds());
}

TEST(KeplerOrbit, MetersAndKilometersRepresentTheSamePhysicalOrbit) {
    simulation::KeplerOrbit km(ellipse(), 1e19, 1e17, 1000);
    simulation::KeplerOrbit meters(ellipse(10000, 8000), 1e19, 1e17, 1);
    EXPECT_NEAR(km.periodSeconds(), meters.periodSeconds(), 1e-10);
    for (double time : {0.0, 1.0, 10.0, 53.0, 128.0, 10000.0}) {
        expectVector(meters.at(time).position, 1000.0 * km.at(time).position, 1e-8);
        expectVector(meters.at(time).velocity, 1000.0 * km.at(time).velocity, 1e-8);
    }
}

class EllipticalInvariants : public testing::TestWithParam<double> {};

TEST_P(EllipticalInvariants, ShapeVisVivaEnergyAngularMomentumAndKeplersEquation) {
    const double requestedEccentricity = GetParam();
    const double a = 10;
    const double b = a * std::sqrt((1 - requestedEccentricity) * (1 + requestedEccentricity));
    // The JSON supplies rounded axes, not eccentricity. Use a higher-precision
    // reference for the actual ellipse; near a circle, converting e -> b -> e
    // amplifies rounding and cannot reproduce the original requested e exactly.
    const long double ratio = static_cast<long double>(b) / a;
    const double e = static_cast<double>(std::sqrt(1.0L - ratio * ratio));
    simulation::KeplerOrbit orbit(ellipse(a, b), 1e19, 1e17, 1000);
    const double mu = orbit.gravitationalParameter();
    const double energy = -mu / (2 * a);
    const double angularMomentum = std::sqrt(mu * a * (1 - e * e));
    for (int step = 0; step < 1000; ++step) {
        SCOPED_TRACE(step);
        const double time = orbit.periodSeconds() * step / 1000.0;
        const auto state = orbit.at(time);
        const double radius = glm::length(state.position);
        const double speedSquared = glm::dot(state.velocity, state.velocity);
        const double x = (state.position.x + a * e) / a;
        const double y = state.position.y / b;
        EXPECT_NEAR(x * x + y * y, 1.0, 2e-12);
        EXPECT_NEAR(speedSquared, mu * (2.0 / radius - 1.0 / a),
                    std::max(1.0, speedSquared) * 2e-9);
        EXPECT_NEAR(speedSquared / 2.0 - mu / radius, energy, 2e-8);
        expectVector(glm::cross(state.position, state.velocity), {0, 0, angularMomentum}, 2e-9);
        const double anomaly = std::atan2(y, x);
        EXPECT_NEAR(std::remainder(anomaly - e * std::sin(anomaly) -
            simulation::tau * time / orbit.periodSeconds(), simulation::tau), 0.0, 2e-12);
    }
}

INSTANTIATE_TEST_SUITE_P(CircularThroughHighlyEccentric, EllipticalInvariants,
                        testing::Values(0.0, 0.000001, 0.2, 0.6, 0.9, 0.99, 0.999999));

TEST(KeplerOrbit, VelocityAndAccelerationAgreeWithIndependentFiniteDifferences) {
    simulation::KeplerOrbit orbit(ellipse(), 1e19, 1e17, 1000);
    for (double time : {0.0, 7.0, 53.0, 100.0, 147.0}) {
        constexpr double dt = 1e-4;
        const auto before = orbit.at(time - dt);
        const auto after = orbit.at(time + dt);
        const auto state = orbit.at(time);
        expectVector((after.position - before.position) / (2 * dt), state.velocity, 1e-9);
        const double radius = glm::length(state.position);
        expectVector((after.velocity - before.velocity) / (2 * dt),
                     -orbit.gravitationalParameter() * state.position / (radius * radius * radius), 1e-10);
    }
}

TEST(KeplerOrbit, OrientationAnglesAndInitialPhaseHaveIndependentEffects) {
    auto settings = ellipse(10, 10);
    settings.mean_anomaly_deg = 90;
    settings.inclination_deg = 20;
    simulation::KeplerOrbit tilted(settings, 1e19, 1e17, 1000);
    expectVector(tilted.at(0).position, {0, 10 * std::cos(glm::radians(20.0)),
                                          10 * std::sin(glm::radians(20.0))});
    settings.mean_anomaly_deg = 0;
    settings.periapsis_deg = 90;
    simulation::KeplerOrbit periapsis(settings, 1e19, 1e17, 1000);
    expectVector(periapsis.at(0).position, tilted.at(0).position);
    settings.ascending_node_deg = 90;
    simulation::KeplerOrbit node(settings, 1e19, 1e17, 1000);
    expectVector(node.at(0).position, {-10 * std::cos(glm::radians(20.0)), 0,
                                      10 * std::sin(glm::radians(20.0))});
}

TEST(KeplerOrbit, NegativeTimeAndManyPeriodsRemainStableAndIndependentOfEvaluationOrder) {
    simulation::KeplerOrbit orbit(ellipse(), 1e19, 1e17, 1000);
    const auto initial = orbit.at(0);
    for (double cycles : {-1e8, -100.0, -1.0, 1.0, 100.0, 1e8}) {
        expectVector(orbit.at(cycles * orbit.periodSeconds()).position, initial.position, 1e-5);
        expectVector(orbit.at(cycles * orbit.periodSeconds()).velocity, initial.velocity, 1e-7);
    }
    auto forward = orbit.at(17);
    auto backward = orbit.at(-17);
    expectVector(backward.position, {forward.position.x, -forward.position.y, 0});
    for (int i = 0; i < 1000; ++i) orbit.at(i * 0.037);
    expectVector(orbit.at(17).position, forward.position, 0);
}

TEST(OrbitalSystem, UsesWholeDescendantMassInOuterSpeedAndNoDoubleCountingInInnerSpeed) {
    auto raw = systemJson();
    raw["planets"].push_back({{"name", "submoon"}, {"mass_kg", 5e15},
        {"orbit", {{"parent", "moon"}, {"semi_major_axis", 0.5}}}});
    const auto config = parse(raw);
    simulation::OrbitalSystem system(config);
    EXPECT_DOUBLE_EQ(system.collectiveMass(1), 6.15e17);
    EXPECT_DOUBLE_EQ(system.collectiveMass(2), 1.5e16);
    EXPECT_DOUBLE_EQ(system.collectiveMass(3), 5e15);
    simulation::KeplerOrbit earth(config.planets[0].orbit, 1e19, 6.15e17, 1000);
    simulation::KeplerOrbit moon(config.planets[1].orbit, 6e17, 1.5e16, 1000);
    EXPECT_DOUBLE_EQ(system.periodSeconds(1), earth.periodSeconds());
    EXPECT_DOUBLE_EQ(system.periodSeconds(2), moon.periodSeconds());
    raw["planets"].erase(2);
    const simulation::OrbitalSystem withoutSubmoon(parse(raw));
    EXPECT_LT(system.periodSeconds(1), withoutSubmoon.periodSeconds(1));
    EXPECT_LT(system.periodSeconds(2), withoutSubmoon.periodSeconds(2));
}

TEST(OrbitalSystem, EarthWobblesOppositeMoonByTheMassRatioAtEveryPhase) {
    const auto config = parse(systemJson());
    const simulation::OrbitalSystem system(config);
    for (int step = 0; step < 1000; ++step) {
        const auto states = system.at(step * 0.37);
        const auto earthOffset = states[1].position - states[1].collectivePosition;
        const auto moonOffset = states[2].position - states[1].collectivePosition;
        expectVector(earthOffset * config.planets[0].mass_kg / config.planets[1].mass_kg,
                     -moonOffset, 2e-12);
        EXPECT_LT(glm::dot(earthOffset, moonOffset), 0);
        expectVector((config.planets[0].mass_kg * states[1].position +
                      config.planets[1].mass_kg * states[2].position) / system.collectiveMass(1),
                     states[1].collectivePosition);
    }
}

TEST(OrbitalSystem, EqualMassBinarySharesItsOrbitEqually) {
    auto raw = systemJson();
    raw["planets"][1]["mass_kg"] = 6e17;
    const simulation::OrbitalSystem system(parse(raw));
    for (double time : {0.0, 17.0, 64.0, 150.0}) {
        const auto states = system.at(time);
        expectVector(states[1].position - states[1].collectivePosition,
                     states[1].collectivePosition - states[2].position);
    }
}

TEST(OrbitalSystem, SunAlsoRecoilsAndTotalCenterOfMassAndMomentumAreConserved) {
    auto raw = systemJson();
    raw["planets"].push_back({{"name", "another_moon"}, {"mass_kg", 2e16},
        {"orbit", {{"parent", "earth"}, {"semi_major_axis", 4.0}}}});
    raw["planets"].push_back({{"name", "another_planet"}, {"mass_kg", 3e17},
        {"orbit", {{"parent", "sun"}, {"semi_major_axis", 24.0}}}});
    const auto config = parse(raw);
    const simulation::OrbitalSystem system(config);
    const auto initial = system.at(0);
    expectVector(initial[0].position, {3, 4, 5});
    EXPECT_GT(glm::length(system.at(17)[0].position - initial[0].position), 0.01);
    for (int step = 0; step < 1000; ++step) {
        const auto states = system.at(step * 3.19);
        glm::dvec3 center = states[0].position * (config.sun.mass_kg / system.collectiveMass(0));
        glm::dvec3 velocity = states[0].velocity * (config.sun.mass_kg / system.collectiveMass(0));
        for (std::size_t i = 1; i < system.size(); ++i) {
            const double fraction = config.planets[i - 1].mass_kg / system.collectiveMass(0);
            center += states[i].position * fraction;
            velocity += states[i].velocity * fraction;
        }
        expectVector(center, initial[0].collectivePosition, 2e-12);
        expectVector(velocity, glm::dvec3(0), 1e-15);
    }
}

TEST(OrbitalSystem, PlanetOrderDoesNotAffectPhysics) {
    auto raw = systemJson();
    const simulation::OrbitalSystem first(parse(raw));
    std::swap(raw["planets"][0], raw["planets"][1]);
    const simulation::OrbitalSystem second(parse(raw));
    for (double time : {0.0, 17.0, 1234.0}) {
        const auto a = first.at(time);
        const auto b = second.at(time);
        expectVector(a[0].position, b[0].position);
        expectVector(a[1].position, b[2].position);
        expectVector(a[2].velocity, b[1].velocity);
    }
}

TEST(OrbitalSystem, IsTranslationInvariantAndSupportsAnEmptyPlanetList) {
    auto raw = systemJson();
    const simulation::OrbitalSystem original(parse(raw));
    const glm::dvec3 translation(1e9, -2e9, 3e9);
    raw["sun"]["position"] = {3 + translation.x, 4 + translation.y, 5 + translation.z};
    const simulation::OrbitalSystem translated(parse(raw));
    for (double time : {0.0, 9.0, 30.0}) {
        const auto a = original.at(time);
        const auto b = translated.at(time);
        for (std::size_t i = 0; i < a.size(); ++i) {
            expectVector(b[i].position - translation, a[i].position, 1e-6);
            expectVector(b[i].velocity, a[i].velocity, 0);
        }
    }
    raw["planets"] = nlohmann::json::array();
    const simulation::OrbitalSystem sunOnly(parse(raw));
    ASSERT_EQ(sunOnly.size(), 1u);
    expectVector(sunOnly.at(1000)[0].velocity, glm::dvec3(0));
    EXPECT_THROW(sunOnly.periodSeconds(0), std::invalid_argument);
}

TEST(DevelopmentOrbits, RequestedPeriodsAndTwentyDegreeEquatorialMoonOrbit) {
    const auto config = development();
    const simulation::OrbitalSystem system(config);
    ASSERT_EQ(system.size(), 3u);
    EXPECT_NEAR(system.periodSeconds(1), 300.0, 1e-10);
    EXPECT_NEAR(system.periodSeconds(2), 120.0, 1e-10);
    EXPECT_DOUBLE_EQ(config.planets[0].rotation.period_seconds, 60.0);
    EXPECT_DOUBLE_EQ(config.planets[1].rotation.period_seconds, 60.0);
    for (int time = 0; time <= 600; ++time) {
        const auto states = system.at(time);
        const auto moonRelative = states[2].position - states[1].position;
        const auto moonVelocity = states[2].velocity - states[1].velocity;
        const auto moonNormal = glm::normalize(glm::cross(moonRelative, moonVelocity));
        const auto outerNormal = glm::normalize(glm::cross(
            states[1].collectivePosition - states[0].position,
            states[1].collectiveVelocity - states[0].velocity));
        EXPECT_NEAR(glm::degrees(std::acos(glm::dot(moonNormal, outerNormal))), 20.0, 1e-10);
        expectVector(states[1].orientation[2], moonNormal);
        EXPECT_NEAR(glm::dot(moonRelative, states[1].orientation[2]), 0.0, 1e-12);
        // Configured bodies cannot intersect over a full joint orbital cycle.
        EXPECT_GT(glm::length(moonRelative), config.planets[0].radius + config.planets[1].radius);
        EXPECT_GT(glm::length(states[1].position - states[0].position),
                  config.sun.radius + config.planets[0].radius);
    }
    const auto zero = system.at(0);
    const auto afterMoonPeriod = system.at(120);
    const auto afterEarthPeriod = system.at(300);
    expectVector(afterMoonPeriod[2].position - afterMoonPeriod[1].position,
                 zero[2].position - zero[1].position);
    expectVector(afterEarthPeriod[1].collectivePosition - afterEarthPeriod[0].position,
                 zero[1].collectivePosition - zero[0].position);
}

TEST(Rotation, QuarterTurnTiltPhaseRetrogradeAndDisabledSpin) {
    config::RotationConfig settings;
    expectVector(simulation::bodyOrientation(settings, 15) * glm::dvec3(1, 0, 0), {0, 1, 0});
    expectVector(simulation::bodyOrientation(settings, 60) * glm::dvec3(1, 0, 0), {1, 0, 0});
    settings.period_seconds = -60;
    expectVector(simulation::bodyOrientation(settings, 15) * glm::dvec3(1, 0, 0), {0, -1, 0});
    settings.period_seconds = 0;
    settings.axial_tilt_deg = 20;
    settings.phase_deg = 90;
    expectVector(simulation::bodyOrientation(settings, 1e10) * glm::dvec3(1, 0, 0),
                 {0, std::cos(glm::radians(20.0)), std::sin(glm::radians(20.0))});
}

TEST(MovingPlanetFrame, LatLonNedAndModelTransformAgreeUnderSpinAndTilt) {
    const auto config = development();
    const simulation::OrbitalSystem system(config);
    for (double time : {0.0, 15.0, 37.0, 60.0, 120.0, 300.0}) {
        const auto state = system.at(time)[1];
        const coordinates::PlanetLocalFrame frame(state.position, 1, state.orientation);
        for (double lat : {-89.0, -21.0, 0.0, 51.0, 89.0}) {
            const coordinates::LatLonAlt location{lat, 73, 0.03};
            const auto point = frame.toWorld(location);
            const auto recovered = frame.fromWorld(point);
            EXPECT_NEAR(recovered.latitudeDeg, lat, 1e-10);
            EXPECT_NEAR(recovered.longitudeDeg, 73, 1e-10);
            EXPECT_NEAR(recovered.altitude, 0.03, 1e-12);
            const auto ned = frame.nedAt(location);
            expectVector(glm::cross(ned.north, ned.east), ned.down);
            expectVector(ned.fromWorld(ned.toWorld({1, 2, 3})), {1, 2, 3});
            const auto local = state.toLocalPoint(point);
            const auto model = rendering::sphereModel(glm::vec3(state.position), 1,
                                                       glm::mat3(state.orientation));
            expectVector(glm::dvec3(model * glm::vec4(glm::vec3(local), 1)), point, 2e-6);
        }
    }
}

TEST(MovingPlanetFrame, RejectsScalingReflectionAndNonfiniteOrientations) {
    EXPECT_THROW(coordinates::PlanetLocalFrame(glm::dvec3(0), 1, glm::dmat3(2)), std::invalid_argument);
    glm::dmat3 orientation(1);
    orientation[0][0] = -1;
    EXPECT_THROW(coordinates::PlanetLocalFrame(glm::dvec3(0), 1, orientation), std::invalid_argument);
    orientation[0][0] = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(coordinates::PlanetLocalFrame(glm::dvec3(0), 1, orientation), std::invalid_argument);
}

TEST(MovingSurfaceCamera, TerrainClearanceLocationAndSavedViewRemainAttachedAcrossOrbits) {
    const auto config = development();
    const simulation::OrbitalSystem system(config);
    const auto initial = system.at(0);
    const auto& earth = config.planets[0];
    rendering::TerrainSurface terrain(earth.surface_noise, earth.terrain_lod,
        earth.radius, config.metersPerWorldUnit(), earth.terrain_landscape);
    PlanetSurfaceCamera camera({initial[1].position, earth.radius, initial[1].orientation},
        {17, 73, 0.03}, initial[0].position, 60);
    camera.mountTerrain(terrain, 0.03, 0.0);
    camera.setDirectionNed({1, 2, -0.1}, glm::dvec3(0, 0, -1));
    const auto initialLocation = camera.location();
    const auto initialDirection = camera.directionNed();
    const auto initialUp = camera.upNed();
    const auto initialLocalEye = initial[1].toLocalPoint(camera.position());
    for (int time = 1; time <= 600; ++time) {
        const auto states = system.at(time);
        camera.followPlanet({states[1].position, earth.radius, states[1].orientation}, states[0].position);
        EXPECT_DOUBLE_EQ(camera.location().latitudeDeg, initialLocation.latitudeDeg);
        EXPECT_DOUBLE_EQ(camera.location().longitudeDeg, initialLocation.longitudeDeg);
        EXPECT_DOUBLE_EQ(camera.location().altitude, initialLocation.altitude);
        EXPECT_NEAR(camera.groundClearance(), 0.03, 1e-11);
        expectVector(camera.directionNed(), initialDirection);
        expectVector(camera.upNed(), initialUp);
        expectVector(states[1].toLocalPoint(camera.position()), initialLocalEye);
        expectVector(camera.target(), states[0].position);
    }
    camera.walk(1, 1, 0.5);
    EXPECT_NE(camera.location().latitudeDeg, initialLocation.latitudeDeg);
    EXPECT_NEAR(camera.groundClearance(), 0.03, 1e-11);
}

TEST(MovingOrbitCamera, FollowPreservesDistanceAndUserOrbitAndZoom) {
    OrbitCamera camera({0, 0, 0}, {10, 3, 4});
    camera.orbit(20, -12);
    camera.zoom(1);
    camera.advance(0.1);
    const auto offset = camera.position - camera.target;
    const auto requestedDistance = camera.requestedDistance();
    camera.followTarget({20, 30, 40});
    expectVector(camera.position - camera.target, offset, 3e-6);
    EXPECT_FLOAT_EQ(camera.requestedDistance(), requestedDistance);
    camera.advance(0.1);
    EXPECT_LT(glm::length(camera.position - camera.target), glm::length(offset));
}

TEST(OrbitalConfig, LegacyDefaultsStillAttachEveryBodyToSunAndDeriveCircularAxes) {
    const auto config = parse(nlohmann::json::parse(R"({
        "planets":[{"orbit_radius":7,"orbit_speed":99999},{}]
    })"));
    EXPECT_EQ(config.planets[0].name, "planet_0");
    EXPECT_EQ(config.planets[1].name, "planet_1");
    EXPECT_EQ(config.planets[0].orbit.parent, "sun");
    EXPECT_DOUBLE_EQ(config.planets[0].orbit.semi_major_axis, 7);
    EXPECT_DOUBLE_EQ(config.planets[0].orbit.semi_minor_axis, 7);
    EXPECT_NO_THROW(simulation::OrbitalSystem{config});
}

TEST(OrbitalConfig, RejectsMissingDuplicateSelfReferencingAndCyclicParents) {
    for (const auto& patch : {
             nlohmann::json{{"name", "sun"}}, nlohmann::json{{"name", "moon"}},
             nlohmann::json{{"name", ""}},
             nlohmann::json{{"orbit", {{"parent", "missing"}}}},
             nlohmann::json{{"orbit", {{"parent", "earth"}}}},
             nlohmann::json{{"orbit", {{"parent", "moon"}}}},
             nlohmann::json{{"orbit", {{"parent", ""}}}}}) {
        auto raw = systemJson();
        raw["planets"][0].merge_patch(patch);
        EXPECT_THROW(parse(raw), std::invalid_argument) << patch;
    }
    auto raw = systemJson();
    raw["sun"]["orbit"] = {{"parent", "earth"}};
    EXPECT_THROW(parse(raw), std::invalid_argument);
    raw = systemJson();
    raw["planets"].push_back({{"name", "third"}, {"orbit", {{"parent", "moon"}}}});
    raw["planets"][0]["orbit"]["parent"] = "third";
    EXPECT_THROW(parse(raw), std::invalid_argument);
}

class InvalidPositiveValue : public testing::TestWithParam<double> {};

TEST_P(InvalidPositiveValue, RejectsInvalidMassAndAxes) {
    const auto bad = GetParam();
    auto raw = systemJson();
    raw["sun"]["mass_kg"] = bad;
    EXPECT_THROW(parse(raw), std::invalid_argument);
    raw = systemJson();
    raw["planets"][0]["mass_kg"] = bad;
    EXPECT_THROW(parse(raw), std::invalid_argument);
    for (const char* axis : {"semi_major_axis", "semi_minor_axis"}) {
        raw = systemJson();
        raw["planets"][0]["orbit"][axis] = bad;
        EXPECT_THROW(parse(raw), std::invalid_argument);
    }
}

INSTANTIATE_TEST_SUITE_P(InvalidNumbers, InvalidPositiveValue, testing::Values(
    0.0, -1.0, std::numeric_limits<double>::infinity(),
    -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()));

TEST(OrbitalConfig, RejectsSwappedDegenerateAndNumericallyUnrepresentableEllipses) {
    auto orbit = ellipse(1, 2);
    EXPECT_THROW(orbit.validate(), std::invalid_argument);
    orbit = ellipse(1, 1e-100);
    EXPECT_THROW(orbit.validate(), std::invalid_argument);
    orbit = ellipse(1e-300, 1e-300);
    EXPECT_THROW(simulation::KeplerOrbit(orbit, 1e19, 1e17, 1000), std::invalid_argument);
    orbit = ellipse(1e300, 1e300);
    EXPECT_THROW(simulation::KeplerOrbit(orbit, 1e19, 1e17, 1000), std::invalid_argument);
    EXPECT_THROW(simulation::KeplerOrbit(ellipse(), 1e308, 1e308, 1000), std::invalid_argument);
}

TEST(OrbitalConfig, RejectsMalformedObjectsAndWrongTypes) {
    for (const char* field : {"orbit", "rotation"}) {
        for (const auto& value : {nlohmann::json(nullptr), nlohmann::json("bad"),
                                  nlohmann::json(7), nlohmann::json::array()}) {
            auto raw = systemJson();
            raw["planets"][0][field] = value;
            EXPECT_THROW(parse(raw), std::invalid_argument);
        }
    }
    auto raw = systemJson();
    raw["planets"][0]["mass_kg"] = "heavy";
    EXPECT_THROW(parse(raw), nlohmann::json::type_error);
    raw = systemJson();
    raw["planets"][0]["orbit"]["parent"] = 0;
    EXPECT_THROW(parse(raw), nlohmann::json::type_error);
}

TEST(OrbitalConfig, RejectsNonfiniteAnglesSpinTimeAndUnitScale) {
    for (double bad : {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        for (const char* field : {"inclination_deg", "mean_anomaly_deg", "ascending_node_deg", "periapsis_deg"}) {
            auto raw = systemJson();
            raw["planets"][0]["orbit"][field] = bad;
            EXPECT_THROW(parse(raw), std::invalid_argument);
        }
        for (const char* field : {"period_seconds", "axial_tilt_deg", "phase_deg"}) {
            auto raw = systemJson();
            raw["planets"][0]["rotation"][field] = bad;
            EXPECT_THROW(parse(raw), std::invalid_argument);
        }
        EXPECT_THROW(simulation::KeplerOrbit(ellipse(), 1e19, 1e17, bad), std::invalid_argument);
        const simulation::OrbitalSystem system(development());
        EXPECT_THROW(system.at(bad), std::invalid_argument);
        EXPECT_THROW(simulation::bodyOrientation(config::RotationConfig{}, bad), std::invalid_argument);
    }
}

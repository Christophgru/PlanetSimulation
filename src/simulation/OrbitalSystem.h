#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "config/ScenarioConfig.h"

namespace simulation {

inline constexpr double gravitationalConstant = 6.67430e-11; // m^3 kg^-1 s^-2
inline constexpr double tau = 2.0 * std::numbers::pi;

// Keep camera wall time independent of the pauseable orbital/spin timeline.
// Pause and speed transitions first account for time up to the key press,
// so changing controls never jumps ahead or catches up on paused time.
class SimulationClock {
public:
    static constexpr double minimumSpeed = 1.0 / 1024.0;
    static constexpr double maximumSpeed = 1024.0;

    SimulationClock(double seconds, double wallSeconds) { reset(seconds, wallSeconds); }

    void reset(double seconds, double wallSeconds) {
        if (!std::isfinite(seconds) || !std::isfinite(wallSeconds))
            throw std::invalid_argument("Simulation clock times must be finite");
        seconds_ = seconds;
        wallSeconds_ = wallSeconds;
    }

    double advanceTo(double wallSeconds) {
        if (!std::isfinite(wallSeconds) || wallSeconds < wallSeconds_)
            throw std::invalid_argument("Simulation wall clock must be finite and monotonic");
        if (!paused_) seconds_ += (wallSeconds - wallSeconds_) * speed_;
        wallSeconds_ = wallSeconds;
        return seconds_;
    }

    void togglePause(double wallSeconds) {
        advanceTo(wallSeconds);
        paused_ = !paused_;
    }

    void scaleSpeed(double factor, double wallSeconds) {
        if (!std::isfinite(factor) || factor <= 0.0)
            throw std::invalid_argument("Simulation speed factor must be finite and positive");
        advanceTo(wallSeconds);
        speed_ = std::clamp(speed_ * factor, minimumSpeed, maximumSpeed);
    }

    bool paused() const { return paused_; }
    double seconds() const { return seconds_; }
    double speed() const { return speed_; }

private:
    double seconds_ = 0.0;
    double wallSeconds_ = 0.0;
    double speed_ = 1.0;
    bool paused_ = false;
};

inline double radians(double degrees) {
    return std::remainder(degrees, 360.0) * (std::numbers::pi / 180.0);
}

inline glm::dmat3 rotationX(double angle) {
    return glm::dmat3(glm::rotate(glm::dmat4(1.0), angle, glm::dvec3(1, 0, 0)));
}

inline glm::dmat3 rotationZ(double angle) {
    return glm::dmat3(glm::rotate(glm::dmat4(1.0), angle, glm::dvec3(0, 0, 1)));
}

inline glm::dmat3 bodyOrientation(const config::RotationConfig& rotation, double seconds) {
    rotation.validate();
    if (!std::isfinite(seconds)) throw std::invalid_argument("Simulation time must be finite");
    const double spin = rotation.period_seconds == 0.0 ? 0.0 :
        tau * (std::remainder(seconds, std::abs(rotation.period_seconds)) / rotation.period_seconds);
    return rotationX(radians(rotation.axial_tilt_deg)) *
           rotationZ(radians(rotation.phase_deg) + spin);
}

struct OrbitalState {
    glm::dvec3 position{0.0};
    glm::dvec3 velocity{0.0};
};

// Two-body relative orbit. Its origin is a focus, not the ellipse's center.
// Absolute-time evaluation avoids integration drift and frame-rate dependence.
class KeplerOrbit {
public:
    KeplerOrbit(const config::OrbitConfig& orbit, double parentMassKg,
                double collectiveMassKg, double metersPerWorldUnit)
        : orbit_(orbit), eccentricity_(orbit.eccentricity()) {
        orbit.validate();
        const double axisRatio = orbit_.semi_minor_axis / orbit_.semi_major_axis;
        oneMinusEccentricity_ = axisRatio * axisRatio / (1.0 + eccentricity_);
        config::validateMass(parentMassKg);
        config::validateMass(collectiveMassKg);
        if (!std::isfinite(metersPerWorldUnit) || metersPerWorldUnit <= 0.0)
            throw std::invalid_argument("Orbit distance scale must be finite and positive");
        mu_ = gravitationalConstant * (parentMassKg + collectiveMassKg) /
              metersPerWorldUnit / metersPerWorldUnit / metersPerWorldUnit;
        meanMotion_ = std::sqrt(mu_ / orbit_.semi_major_axis) / orbit_.semi_major_axis;
        period_ = tau / meanMotion_;
        if (!std::isfinite(mu_) || mu_ <= 0.0 || !std::isfinite(period_) || period_ <= 0.0 ||
            !std::isfinite(meanMotion_) || meanMotion_ <= 0.0)
            throw std::invalid_argument("Orbit masses and axes exceed numerical range");
        orientation_ = rotationZ(radians(orbit.ascending_node_deg)) *
                       rotationX(radians(orbit.inclination_deg)) *
                       rotationZ(radians(orbit.periapsis_deg));
        // Catch finite config values whose periapsis speed overflows.
        const auto initial = at(0.0);
        const double periapsisSpeed = orbit_.semi_minor_axis * meanMotion_ / oneMinusEccentricity_;
        if (!std::isfinite(periapsisSpeed) || !std::isfinite(glm::length(initial.position)))
            throw std::invalid_argument("Orbit position or velocity exceeds numerical range");
    }

    double periodSeconds() const { return period_; }
    double gravitationalParameter() const { return mu_; }

    OrbitalState at(double seconds) const {
        if (!std::isfinite(seconds)) throw std::invalid_argument("Simulation time must be finite");
        const double meanAnomaly = std::remainder(radians(orbit_.mean_anomaly_deg) +
            tau * (std::remainder(seconds, period_) / period_), tau);
        // Monotonic bracketing remains reliable even for very eccentric orbits,
        // unlike an unguarded Newton iteration near periapsis.
        double low = -std::numbers::pi;
        double high = std::numbers::pi;
        double anomaly = meanAnomaly;
        for (int iteration = 0; iteration < 64; ++iteration) {
            const double residual = anomaly - eccentricity_ * std::sin(anomaly) - meanAnomaly;
            if (residual == 0.0) break;
            if (residual < 0.0) low = anomaly;
            else high = anomaly;
            anomaly = 0.5 * (low + high);
        }
        const double sine = std::sin(anomaly);
        const double cosine = std::cos(anomaly);
        const double halfSine = std::sin(anomaly / 2.0);
        // Rationalize 1-e and use 1-cos(E)=2sin²(E/2) to avoid catastrophic
        // cancellation of radius and speed near a very eccentric periapsis.
        const double oneMinusCosine = 2.0 * halfSine * halfSine;
        const double anomalyRate = meanMotion_ /
            (oneMinusEccentricity_ + eccentricity_ * oneMinusCosine);
        return {
            orientation_ * glm::dvec3(orbit_.semi_major_axis * (oneMinusEccentricity_ - oneMinusCosine),
                                      orbit_.semi_minor_axis * sine, 0.0),
            orientation_ * glm::dvec3(-orbit_.semi_major_axis * sine * anomalyRate,
                                      orbit_.semi_minor_axis * cosine * anomalyRate, 0.0)
        };
    }

private:
    config::OrbitConfig orbit_;
    double eccentricity_;
    double oneMinusEccentricity_;
    double mu_;
    double meanMotion_;
    double period_;
    glm::dmat3 orientation_{1.0};
};

struct BodyState : OrbitalState {
    glm::dmat3 orientation{1.0};
    glm::dvec3 collectivePosition{0.0};
    glm::dvec3 collectiveVelocity{0.0};

    glm::dvec3 toLocalPoint(const glm::dvec3& worldPoint) const {
        return glm::transpose(orientation) * (worldPoint - position);
    }
};

// A hierarchy of Kepler orbits with mass-weighted recoil at every level.
// Subtrees act as point masses in their outer orbit. Inter-branch perturbations
// and tidal effects are deliberately outside this prescribed-ellipse model.
class OrbitalSystem {
public:
    explicit OrbitalSystem(const config::ScenarioConfig& scenario)
        : parents_(scenario.orbitalParents()), children_(parents_.size()),
          masses_(parents_.size()), collectiveMasses_(parents_.size()),
          rotations_(parents_.size()), orbits_(parents_.size()) {
        masses_[0] = scenario.sun.mass_kg;
        anchor_ = {scenario.sun.position[0], scenario.sun.position[1], scenario.sun.position[2]};
        for (std::size_t i = 1; i < parents_.size(); ++i) {
            masses_[i] = scenario.planets[i - 1].mass_kg;
            rotations_[i] = scenario.planets[i - 1].rotation;
            children_[parents_[i]].push_back(i);
        }
        order_.push_back(0);
        for (std::size_t i = 0; i < order_.size(); ++i)
            for (auto child : children_[order_[i]]) order_.push_back(child);
        collectiveMasses_ = masses_;
        for (auto it = order_.rbegin(); it != order_.rend(); ++it) {
            if (*it != 0) collectiveMasses_[parents_[*it]] += collectiveMasses_[*it];
        }
        for (double mass : collectiveMasses_) config::validateMass(mass);
        for (std::size_t i = 1; i < parents_.size(); ++i)
            orbits_[i].emplace(scenario.planets[i - 1].orbit, masses_[parents_[i]],
                              collectiveMasses_[i], scenario.metersPerWorldUnit());

        // Keep the configured Sun position at the initial epoch. The fixed
        // system barycenter then includes the initial planetary displacement.
        const auto initial = at(0.0);
        anchor_ += anchor_ - initial[0].position;
    }

    std::size_t size() const { return parents_.size(); }
    double collectiveMass(std::size_t body) const { return collectiveMasses_.at(body); }
    double periodSeconds(std::size_t body) const {
        if (body == 0) throw std::invalid_argument("Sun has no parent orbit");
        return orbits_.at(body)->periodSeconds();
    }

    std::vector<BodyState> at(double seconds) const {
        if (!std::isfinite(seconds)) throw std::invalid_argument("Simulation time must be finite");
        std::vector<OrbitalState> relative(size());
        std::vector<BodyState> states(size());
        for (std::size_t i = 1; i < size(); ++i) {
            relative[i] = orbits_[i]->at(seconds);
            states[i].orientation = bodyOrientation(rotations_[i], seconds);
        }
        states[0].collectivePosition = anchor_;
        for (auto i : order_) {
            auto& state = states[i];
            state.position = state.collectivePosition;
            state.velocity = state.collectiveVelocity;
            for (auto child : children_[i]) {
                const double fraction = collectiveMasses_[child] / collectiveMasses_[i];
                state.position -= fraction * relative[child].position;
                state.velocity -= fraction * relative[child].velocity;
            }
            for (auto child : children_[i]) {
                states[child].collectivePosition = state.position + relative[child].position;
                states[child].collectiveVelocity = state.velocity + relative[child].velocity;
            }
        }
        return states;
    }

private:
    std::vector<std::size_t> parents_;
    std::vector<std::vector<std::size_t>> children_;
    std::vector<std::size_t> order_;
    std::vector<double> masses_;
    std::vector<double> collectiveMasses_;
    std::vector<config::RotationConfig> rotations_;
    std::vector<std::optional<KeplerOrbit>> orbits_;
    glm::dvec3 anchor_{0.0};
};

} // namespace simulation

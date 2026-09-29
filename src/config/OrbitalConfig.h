#pragma once

#include <cmath>
#include <stdexcept>
#include <string>

namespace config {
class Config;

inline void validateMass(double mass) {
    if (!std::isfinite(mass) || mass <= 0.0)
        throw std::invalid_argument("Body mass_kg must be finite and positive");
}

struct OrbitConfig {
    std::string parent = "sun";
    // Relative ellipse from the parent body to this body's subtree barycenter,
    // in scenario distance units. Angles use a fixed world XY reference plane.
    double semi_major_axis = 5.0;
    double semi_minor_axis = 5.0;
    double mean_anomaly_deg = 0.0;
    double inclination_deg = 0.0;
    double ascending_node_deg = 0.0;
    double periapsis_deg = 0.0;

    OrbitConfig() = default;
    explicit OrbitConfig(const Config& cfg);

    double eccentricity() const {
        const double ratio = semi_minor_axis / semi_major_axis;
        // Subtract the supplied axes before division to retain their small
        // difference when the orbit is nearly circular.
        return std::sqrt(((semi_major_axis - semi_minor_axis) / semi_major_axis) * (1.0 + ratio));
    }

    void validate() const {
        if (parent.empty() || !std::isfinite(semi_major_axis) || semi_major_axis <= 0.0 ||
            !std::isfinite(semi_minor_axis) || semi_minor_axis <= 0.0 ||
            semi_minor_axis > semi_major_axis || eccentricity() >= 1.0 ||
            !std::isfinite(mean_anomaly_deg) || !std::isfinite(inclination_deg) ||
            !std::isfinite(ascending_node_deg) || !std::isfinite(periapsis_deg))
            throw std::invalid_argument("Invalid orbit: require a parent and finite axes 0 < b <= a");
    }
};

struct RotationConfig {
    // Zero disables spin; negative periods spin in the opposite direction.
    double period_seconds = 60.0;
    double axial_tilt_deg = 0.0;
    double phase_deg = 0.0;

    RotationConfig() = default;
    explicit RotationConfig(const Config& cfg);

    void validate() const {
        if (!std::isfinite(period_seconds) || !std::isfinite(axial_tilt_deg) ||
            !std::isfinite(phase_deg))
            throw std::invalid_argument("Rotation parameters must be finite");
    }
};

} // namespace config

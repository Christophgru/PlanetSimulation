#include "config/OrbitalConfig.h"
#include "config/Config.h"

namespace config {

OrbitConfig::OrbitConfig(const Config& cfg) {
    if (!cfg.data().is_object()) throw std::invalid_argument("orbit must be an object");
    parent = cfg.get("parent", parent);
    semi_major_axis = cfg.getDouble("semi_major_axis", semi_major_axis);
    semi_minor_axis = cfg.getDouble("semi_minor_axis", semi_major_axis);
    mean_anomaly_deg = cfg.getDouble("mean_anomaly_deg", mean_anomaly_deg);
    inclination_deg = cfg.getDouble("inclination_deg", inclination_deg);
    ascending_node_deg = cfg.getDouble("ascending_node_deg", ascending_node_deg);
    periapsis_deg = cfg.getDouble("periapsis_deg", periapsis_deg);
    validate();
}

RotationConfig::RotationConfig(const Config& cfg) {
    if (!cfg.data().is_object()) throw std::invalid_argument("rotation must be an object");
    period_seconds = cfg.getDouble("period_seconds", period_seconds);
    axial_tilt_deg = cfg.getDouble("axial_tilt_deg", axial_tilt_deg);
    phase_deg = cfg.getDouble("phase_deg", phase_deg);
    validate();
}
} // namespace config

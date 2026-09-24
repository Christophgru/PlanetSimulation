#pragma once

#include <cmath>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>
#include "config/ScenarioConfig.h"
#include "rendering/PlanetSurfaceCamera.h"

struct SurfaceCameraSnapshot {
    glm::dvec3 worldPosition;
    glm::dvec3 worldDirection;
    coordinates::LatLonAlt planetPosition;
    std::string distanceUnit;
    double metersPerWorldUnit;
    nlohmann::json startConfig;

    std::string format() const {
        const auto position = nlohmann::json::array(
            {worldPosition.x, worldPosition.y, worldPosition.z});
        const auto direction = nlohmann::json::array(
            {worldDirection.x, worldDirection.y, worldDirection.z});
        return "Simulation time: " + startConfig.at("simulation_time_seconds").dump() + " s\n" +
               "Surface camera position (world, " + distanceUnit + "): " +
               position.dump() + "\n" +
               "Surface camera direction (world, unit vector): " +
               direction.dump() + "\n" +
               "Surface camera position (planet LLA): latitude=" +
               nlohmann::json(planetPosition.latitudeDeg).dump() + " deg, longitude=" +
               nlohmann::json(planetPosition.longitudeDeg).dump() +
               " deg, altitude=" +
               nlohmann::json(planetPosition.altitude * metersPerWorldUnit).dump() +
               " m above reference sphere\n" +
               "Surface camera ground clearance: " +
               nlohmann::json(startConfig.at("altitude").get<double>() *
                              metersPerWorldUnit).dump() + " m\n" +
               "surface_camera start value: " + startConfig.dump() + "\n";
    }
};

class SurfaceCameraTelemetry {
public:
    static constexpr double kIntervalSeconds = 5.0;

    std::optional<SurfaceCameraSnapshot> sample(
        bool inSurfaceMode, double currentTime,
        const PlanetSurfaceCamera& camera,
        const config::SurfaceCameraConfig& settings,
        double metersPerWorldUnit = 1000.0,
        const std::string& distanceUnit = "km", double simulationTime = 0.0) {
        if (!inSurfaceMode || !std::isfinite(currentTime) || !std::isfinite(simulationTime)) {
            active_ = false;
            return std::nullopt;
        }
        if (!active_ || currentTime >= nextReportTime_) {
            active_ = true;
            nextReportTime_ = currentTime + kIntervalSeconds;
            return capture(camera, settings, metersPerWorldUnit, distanceUnit, simulationTime);
        }
        return std::nullopt;
    }

    static SurfaceCameraSnapshot capture(
        const PlanetSurfaceCamera& camera, const config::SurfaceCameraConfig& settings,
        double metersPerWorldUnit, const std::string& distanceUnit, double simulationTime) {
        if (!std::isfinite(simulationTime)) throw std::invalid_argument("Snapshot time must be finite");
        const auto location = camera.location();
        const glm::dvec3 nedDirection = camera.directionNed();
        const glm::dvec3 nedUp = camera.upNed();
        return SurfaceCameraSnapshot{
            camera.position(), camera.direction(), location,
            distanceUnit, metersPerWorldUnit,
            nlohmann::json{
                {"simulation_time_seconds", simulationTime},
                {"reference_frame", settings.reference_frame},
                {"planet_index", settings.planet_index},
                {"latitude_deg", location.latitudeDeg},
                {"longitude_deg", location.longitudeDeg},
                {"altitude", camera.configuredClearance()},
                {"walk_speed_mps", settings.walk_speed_mps},
                {"direction_ned", nlohmann::json::array(
                    {nedDirection.x, nedDirection.y, nedDirection.z})},
                {"up_ned", nlohmann::json::array(
                    {nedUp.x, nedUp.y, nedUp.z})},
                {"fov", static_cast<double>(camera.fov())}
            }
        };
    }

private:
    bool active_ = false;
    double nextReportTime_ = 0.0;
};

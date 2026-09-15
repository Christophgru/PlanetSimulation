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
        return "Surface camera position (world, " + distanceUnit + "): " +
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
        const std::string& distanceUnit = "km") {
        if (!inSurfaceMode || !std::isfinite(currentTime)) {
            active_ = false;
            return std::nullopt;
        }
        if (!active_ || currentTime >= nextReportTime_) {
            active_ = true;
            nextReportTime_ = currentTime + kIntervalSeconds;
            const auto location = camera.location();
            const glm::dvec3 nedDirection = camera.directionNed();
            const glm::dvec3 nedUp = camera.upNed();
            return SurfaceCameraSnapshot{
                camera.position(), camera.direction(), location,
                distanceUnit, metersPerWorldUnit,
                nlohmann::json{
                    {"reference_frame", settings.reference_frame},
                    {"planet_index", settings.planet_index},
                    {"latitude_deg", location.latitudeDeg},
                    {"longitude_deg", location.longitudeDeg},
                    {"altitude", camera.hasTerrain() ? camera.groundClearance()
                                                      : location.altitude},
                    {"direction_ned", nlohmann::json::array(
                        {nedDirection.x, nedDirection.y, nedDirection.z})},
                    {"up_ned", nlohmann::json::array(
                        {nedUp.x, nedUp.y, nedUp.z})},
                    {"fov", static_cast<double>(camera.fov())}
                }
            };
        }
        return std::nullopt;
    }

private:
    bool active_ = false;
    double nextReportTime_ = 0.0;
};

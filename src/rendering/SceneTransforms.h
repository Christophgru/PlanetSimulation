#pragma once

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace rendering {

struct ClipPlanes {
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
};

inline ClipPlanes surfaceClipPlanes(double altitude, double sunDistance,
                                    double sunRadius) {
    if (!std::isfinite(altitude) || altitude < 0.0 ||
        !std::isfinite(sunDistance) || sunDistance <= 0.0 ||
        !std::isfinite(sunRadius) || sunRadius < 0.0) {
        throw std::invalid_argument("Surface clip distances must be finite and positive");
    }
    return {static_cast<float>(std::clamp(altitude * 0.5, 0.00001, 0.1)),
            static_cast<float>(std::max(20.0, 2.0 * (sunDistance + sunRadius)))};
}

inline glm::mat4 sphereModel(const glm::vec3& position, float radius) {
    glm::mat4 model(1.0f);
    model = glm::translate(model, position);
    return glm::scale(model, glm::vec3(radius));
}

inline glm::mat4 perspectiveProjection(float fovDegrees, float aspect,
                                       ClipPlanes clip = {}) {
    return glm::perspective(glm::radians(fovDegrees), aspect,
                            clip.nearPlane, clip.farPlane);
}

} // namespace rendering

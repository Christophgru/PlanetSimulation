#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace rendering {

inline glm::mat4 sphereModel(const glm::vec3& position, float radius) {
    glm::mat4 model(1.0f);
    model = glm::translate(model, position);
    return glm::scale(model, glm::vec3(radius));
}

inline glm::mat4 perspectiveProjection(float fovDegrees, float aspect) {
    return glm::perspective(glm::radians(fovDegrees), aspect, 0.1f, 1000.0f);
}

} // namespace rendering

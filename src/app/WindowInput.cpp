#include "app/WindowInput.h"
#include <GLFW/glfw3.h>
#include <iostream>

namespace app {
namespace {
CameraInput* windowCameraInput(GLFWwindow* window) {
    auto* context = static_cast<InputContext*>(glfwGetWindowUserPointer(window));
    return context ? context->camera : nullptr;
}

void onMouseButton(GLFWwindow* window, int button, int action, int) {
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;
    auto* input = windowCameraInput(window);
    if (!input) return;

    if (action == GLFW_PRESS) {
        double x = 0.0;
        double y = 0.0;
        glfwGetCursorPos(window, &x, &y);
        input->beginDrag(x, y);
    } else if (action == GLFW_RELEASE) {
        input->endDrag();
    }
}

void onCursorPosition(GLFWwindow* window, double x, double y) {
    auto* input = windowCameraInput(window);
    if (input) input->moveCursor(x, y);
}

void onScroll(GLFWwindow* window, double, double yOffset) {
    auto* input = windowCameraInput(window);
    if (input) input->scroll(yOffset);
}

void onKey(GLFWwindow* window, int key, int, int action, int) {
    if (action != GLFW_PRESS) return;
    auto* context = static_cast<InputContext*>(glfwGetWindowUserPointer(window));
    if (key == GLFW_KEY_I && context) {
        // GLFW_REPEAT is filtered above so holding I toggles only once.
        context->statsVisible = !context->statsVisible;
        return;
    }
    if (key == GLFW_KEY_O && context) {
        context->orbitsVisible = !context->orbitsVisible;
        std::cout << (context->orbitsVisible ? "Orbit paths on\n" : "Orbit paths off\n");
        return;
    }
    if (key == GLFW_KEY_T && context && context->clock) {
        context->clock->togglePause(glfwGetTime());
        std::cout << (context->clock->paused() ? "Simulation paused (T to resume)\n" :
                                               "Simulation resumed (T to pause)\n");
        return;
    }
    if ((key == GLFW_KEY_Y || key == GLFW_KEY_U) && context && context->clock) {
        context->clock->scaleSpeed(key == GLFW_KEY_Y ? 0.5 : 2.0, glfwGetTime());
        std::cout << "Simulation speed: " << context->clock->speed() << "x"
                  << (context->clock->paused() ? " (paused)\n" : "\n");
        return;
    }
    if (key == GLFW_KEY_R && context) {
        context->reloadRequested = true;
        return;
    }
    auto* input = context ? context->camera : nullptr;
    if (!input) return;
    if (key == GLFW_KEY_1) {
        input->selectOrbit();
    } else if (key == GLFW_KEY_2) {
        input->selectSurface();
    } else if (key == GLFW_KEY_3) {
        input->selectPlanetOrbit();
    } else if (key == GLFW_KEY_ESCAPE) {
        input->releaseCursor();
    }
}

}
void bindWindowInput(GLFWwindow* window, InputContext& input) {
    glfwSetWindowUserPointer(window, &input);
    glfwSetMouseButtonCallback(window, onMouseButton);
    glfwSetCursorPosCallback(window, onCursorPosition);
    glfwSetScrollCallback(window, onScroll);
    glfwSetKeyCallback(window, onKey);
}
}

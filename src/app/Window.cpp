#include "app/Window.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stdexcept>

namespace app {
Window::Window(const CommandLineOptions& options) {
    if (!glfwInit()) throw std::runtime_error("Failed to initialize GLFW");
    // NVIDIA honors the requested version exactly; ask for compute support
    // before falling back to the minimum rendering context on older drivers.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_STENCIL_BITS, 8);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);
    const auto create = [&] { return glfwCreateWindow(options.renderTestMode ? options.renderTestWidth : 1280,
                              options.renderTestMode ? options.renderTestHeight : 720,
                              options.renderTestMode ? "PlanetSimulation Render Test" : "PlanetSimulation",
                              nullptr, nullptr); };
    window_ = create();
    if (!window_) {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        window_ = create();
    }
    if (!window_) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }
    makeCurrent();
    glfwSwapInterval(options.renderTestMode ? 0 : 1);
    if (glewInit() != GLEW_OK) {
        glfwDestroyWindow(window_);
        glfwTerminate();
        throw std::runtime_error("Failed to initialize GLEW");
    }
}
Window::~Window() {
    glfwDestroyWindow(window_);
    glfwTerminate();
}
void Window::makeCurrent() const { glfwMakeContextCurrent(window_); }
}

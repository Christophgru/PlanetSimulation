#include <iostream>
#include <GL/glew.h>
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <thread>
#include <chrono>

#include "math/Vector3.h"
#include "coordinate_systems/CoordinateSystem.h"

int main() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return 1;
    }

    // Configure GLFW for modern OpenGL core profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    // Create window
    auto window = glfwCreateWindow(1280, 720, "PlanetSimulation", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return 1;
    }

    // Make context current
    glfwMakeContextCurrent(window);

    // Initialize GLEW after context is current
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    std::cout << "PlanetSimulation v0.1 initialized\n";
    std::cout << "Coordinate system abstractions ready\n";
    std::cout << "OpenGL version: " << glGetString(GL_VERSION) << "\n";
    
    // Keep window open for viewing
    std::cout << "Press Escape to exit...\n";

    while (!glfwWindowShouldClose(window)) {
        // Process events
        glfwPollEvents();

        // Clear framebuffer with non-black color (light blue)
        glClearColor(0.5f, 0.7f, 0.9f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Swap buffers
        glfwSwapBuffers(window);

        // Sleep to control frame rate (optional)
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    // Cleanup
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

#include <iostream>
#include <GL/glew.h>
#include <GL/gl.h>
#include <thread>
#include <chrono>

#include "math/Vector3.h"
#include "coordinate_systems/CoordinateSystem.h"

int main() {
    // Initialize OpenGL context (minimal)
    glewExperimental = GL_TRUE;
    glewInit();

    std::cout << "PlanetSimulation v0.1 initialized\n";
    std::cout << "Coordinate system abstractions ready\n";
    
    // Keep window open for viewing
    std::cout << "Press Ctrl+C to exit...\n";
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return 0;
}

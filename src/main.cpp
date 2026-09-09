#include <iostream>
#include <GL/glew.h>
#include <GL/gl.h>

int main() {
    // Initialize OpenGL context (minimal)
    glewExperimental = GL_TRUE;
    glewInit();

    std::cout << "PlanetSimulation v0.1 initialized\n";
    std::cout << "Coordinate system abstractions ready\n";
    
    return 0;
}

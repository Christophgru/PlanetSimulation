#include <iostream>
#include <GL/glew.h>
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <thread>
#include <chrono>
#include <cmath>
#include <filesystem>

#include "math/Vector3.h"
#include "config/Config.h"
#include "config/ScenarioConfig.h"

namespace fs = std::filesystem;

// Simple 4x4 matrix class for transformations
class Matrix4 {
public:
    double data[16];
    
    Matrix4() {
        memset(data, 0, sizeof(data));
        // Identity matrix
        data[0] = data[5] = data[10] = data[15] = 1.0;
    }
    
    static Matrix4 translate(double x, double y, double z) {
        Matrix4 m;
        m.data[0] = m.data[5] = m.data[10] = m.data[15] = 1.0;
        m.data[12] = x;
        m.data[13] = y;
        m.data[14] = z;
        return m;
    }
    
    static Matrix4 perspective(double fov, double aspect, double near, double far) {
        Matrix4 m;
        double f = 1.0 / tan(fov * M_PI / 360.0);
        m.data[0] = f / aspect;
        m.data[5] = f;
        m.data[10] = -(far + near) / (far - near);
        m.data[11] = -1.0;
        m.data[14] = 0.0;
        return m;
    }
};

// Simple camera class for view matrix calculation
class Camera {
public:
    Vector3 position;
    Vector3 target;
    double fov;
    
    Camera() : position({0, 0, 0}), target({0, 0, 0}), fov(60.0) {}
    
    void update(const config::ScenarioConfig& scenario) {
        // Use default camera position for now
        position = Vector3{15.0, 2.0, 8.0};
        target = Vector3{0.0, 0.0, 0.0};
        fov = 60.0;
    }
    
    // Simple view matrix (looking at target from position)
    Matrix4 getViewMatrix() const {
        return Matrix4::translate(-position.x, -position.y, -position.z);
    }
};

// Simple sphere mesh generator
class Sphere {
public:
    int segments = 32;
    
    void generateMesh() {
        // Generate vertices for a sphere
        float vCount = (segments + 1) * (segments + 1);
        std::vector<float> vertices(vCount * 3);
        
        for (int lat = 0; lat <= segments; lat++) {
            for (int lon = 0; lon <= segments; lon++) {
                float theta = M_PI * 2.0f * lon / segments;
                float phi = M_PI * lat / segments;
                
                float x = cos(theta) * sin(phi);
                float y = cos(phi);
                float z = sin(theta) * sin(phi);
                
                int idx = (lat * (segments + 1) + lon) * 3;
                vertices[idx] = x;
                vertices[idx + 1] = y;
                vertices[idx + 2] = z;
            }
        }
        
        // Upload to GPU
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
        
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
        glEnableVertexAttribArray(0);
        
        glBindVertexArray(0);
    }
    
    GLuint getVao() const { return vao; }
    int getVertexCount() const { return segments * (segments + 1) * 2; }
    GLuint getVbo() const { return vbo; }
    
private:
    GLuint vao = 0;
    GLuint vbo = 0;
};

// Global sphere mesh
Sphere g_sphere;

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

    // Load scenario config
    const std::string configPath = "configs/scenarios/solar_system.json";
    if (!fs::exists(configPath)) {
        std::cerr << "Config file not found: " << configPath << "\n";
        std::cout << "Press Escape to exit...\n";
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            glClearColor(0.5f, 0.7f, 0.9f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glfwSwapBuffers(window);
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
        return 0;
    }

    try {
        config::Config cfg = config::Config::load(configPath);
        config::ScenarioConfig scenario(cfg);
        
        // Setup camera - position behind a planet to see both sun and planets
        Camera camera;
        camera.update(scenario);
        
        // Generate sphere mesh
        g_sphere.generateMesh();
        
        std::cout << "PlanetSimulation v0.1 initialized\n";
        std::cout << "Scenario: " << scenario.name << "\n";
        std::cout << "Sun radius: " << scenario.sun.radius << "\n";
        std::cout << "Planets: " << scenario.planets.size() << "\n";
        std::cout << "Camera position: (" << camera.position.x << ", " 
                  << camera.position.y << ", " << camera.position.z << ")\n";
        
        // Keep window open for viewing
        std::cout << "Press Escape to exit...\n";

        while (!glfwWindowShouldClose(window)) {
            // Process events
            glfwPollEvents();

            // Setup projection matrix
            int width = 0, height = 0;
            glfwGetFramebufferSize(window, &width, &height);
            Matrix4 proj = Matrix4::perspective(camera.fov, 
                                                (double)width / (double)height,
                                                0.1f, 1000.0f);
            
            // Setup view matrix
            Matrix4 view = camera.getViewMatrix();
            
            // Clear framebuffer with dark background
            glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // Render sun
            {
                glBindVertexArray(g_sphere.getVao());
                glDrawArrays(GL_TRIANGLES, 0, g_sphere.getVertexCount());
                glBindVertexArray(0);
            }

            // Render planets
            for (const auto& planet : scenario.planets) {
                // Calculate planet position based on orbit
                double time = glfwGetTime();
                double angle = time * planet.orbit_speed;
                
                Vector3 sunPos = Vector3{scenario.sun.position[0], 
                                         scenario.sun.position[1], 
                                         scenario.sun.position[2]};
                Vector3 planetPos = Vector3{
                    sunPos.x + cos(angle) * planet.orbit_radius,
                    sunPos.y + sin(angle) * planet.orbit_radius,
                    sunPos.z
                };
                
                // Draw planet at calculated position (no transformation needed for now)
                glBindVertexArray(g_sphere.getVao());
                glDrawArrays(GL_TRIANGLES, 0, g_sphere.getVertexCount());
                glBindVertexArray(0);
            }

            // Swap buffers
            glfwSwapBuffers(window);

            // Sleep to control frame rate (optional)
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }

        // Cleanup
        glDeleteVertexArrays(1, &g_sphere.getVao());
        glDeleteBuffers(1, &g_sphere.getVbo());
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }

    // Cleanup
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

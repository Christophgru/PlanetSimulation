#include <iostream>
#include <GL/glew.h>
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <thread>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <cstdlib>
#include <vector>

#include "math/Vector3.h"
#include "math/Matrix4.h"
#include "config/Config.h"
#include "config/ScenarioConfig.h"
#include "rendering/Mesh.h"
#include "rendering/Shader.h"
#include "stb_image_write.h"

namespace fs = std::filesystem;

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
    
    // View matrix (looking at target from position)
    Matrix4 getViewMatrix() const {
        return Matrix4::lookAt(position, target, Vector3{0.0f, 1.0f, 0.0f});
    }
};

// Global mesh for sun/planets
Mesh g_mesh;

int main(int argc, char** argv) {
    bool renderTestMode = false;
    std::string outputImagePath;
    
    // Parse command-line arguments
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--render-test" && i + 1 < argc) {
            renderTestMode = true;
            outputImagePath = argv[++i];
        }
    }

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

    // Create window (hidden for render-test mode)
    int width = renderTestMode ? 800 : 1280;
    int height = renderTestMode ? 600 : 720;
    const char* windowTitle = renderTestMode ? "PlanetSimulation Render Test" : "PlanetSimulation";
    
    auto window = glfwCreateWindow(width, height, windowTitle, nullptr, nullptr);
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
        std::exit(1);
    }

    try {
        config::Config cfg = config::Config::load(configPath);
        config::ScenarioConfig scenario(cfg);
        
        // Setup camera - position behind a planet to see both sun and planets
        Camera camera;
        camera.update(scenario);
        
        // Generate sphere mesh (will be used for sun)
        g_mesh.generateSphere(32);
        
        // Upload vertex data to VBO
        glGenBuffers(1, &g_mesh.vbo);
        glBindBuffer(GL_ARRAY_BUFFER, g_mesh.vbo);
        glBufferData(GL_ARRAY_BUFFER, g_mesh.vertices.size() * sizeof(float), 
                     g_mesh.vertices.data(), GL_STATIC_DRAW);
        
        // Upload index data to EBO
        glGenBuffers(1, &g_mesh.ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_mesh.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, g_mesh.indices.size() * sizeof(unsigned int), 
                     g_mesh.indices.data(), GL_STATIC_DRAW);
        
        std::cout << "PlanetSimulation v0.1 initialized\n";
        std::cout << "Scenario: " << scenario.name << "\n";
        std::cout << "Sun radius: " << scenario.sun.radius << "\n";
        std::cout << "Planets: " << scenario.planets.size() << "\n";
        std::cout << "Camera position: (" << camera.position.x << ", " 
                  << camera.position.y << ", " << camera.position.z << ")\n";

        if (renderTestMode) {
            std::cout << "Render test mode enabled\n";
            std::cout << "Output image: " << outputImagePath << "\n";
            
            // Make window hidden for render-test mode
            glfwSetWindowAttrib(window, GLFW_VISIBLE, GLFW_FALSE);
        } else {
            // Keep window open for viewing
            std::cout << "Press Escape to exit...\n";
        }

        // Create shader program
        Shader shader("shaders/basic.vert", "shaders/basic.frag");
        
        // Enable depth testing
        glEnable(GL_DEPTH_TEST);
        
        // Set viewport
        int width = renderTestMode ? 800 : 0;
        int height = renderTestMode ? 600 : 0;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);

        // Render exactly one frame for render-test mode
        if (renderTestMode) {
            // Process events
            glfwPollEvents();

            // Setup projection matrix
            Matrix4 proj = Matrix4::perspective(camera.fov, 
                                                (double)width / (double)height,
                                                0.1f, 1000.0f);
            
            // Setup view matrix
            Matrix4 view = camera.getViewMatrix();
            
            // Clear framebuffer with dark background
            glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // Use shader program
            shader.use();
            
            // Set uniforms
            const float* projData = reinterpret_cast<const float*>(&proj);
            const float* viewData = reinterpret_cast<const float*>(&view);
            
            shader.setMat4("projection", projData);
            shader.setMat4("view", viewData);
            
            // Sun color (convert double to float)
            shader.setFloat3("uSunColor", 
                static_cast<float>(scenario.sun.color[0]),
                static_cast<float>(scenario.sun.color[1]),
                static_cast<float>(scenario.sun.color[2]));
            
            // Model matrix for sun (at origin, no scaling for now)
            Matrix4 model = Matrix4();
            const float* modelData = reinterpret_cast<const float*>(&model);
            shader.setMat4("model", modelData);
            
            // Render sun
            {
                g_mesh.draw();
            }

            // Call glFinish() before reading framebuffer
            glFinish();
            
            // Check for OpenGL errors after rendering
            GLenum err;
            while ((err = glGetError()) != GL_NO_ERROR) {
                std::cerr << "OpenGL error: " << err << "\n";
            }

            // Read rendered framebuffer (account for vertical flip)
            std::vector<unsigned char> pixels(width * height * 4);
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            
            // Check for OpenGL errors after reading
            while ((err = glGetError()) != GL_NO_ERROR) {
                std::cerr << "OpenGL error: " << err << "\n";
            }

            // Write PNG using stb_image_write.h (no need to flip manually, stb handles it)
            FILE* f = fopen(outputImagePath.c_str(), "wb");
            if (!f) {
                std::cerr << "Failed to open file for writing: " << outputImagePath << "\n";
                std::exit(1);
            }
            
            // Use stb_image_write.h to write PNG
            stbi__write_png(f, pixels.data(), width, height, 4);
            
            fclose(f);
            
            std::cout << "Render test completed successfully\n";
            std::cout << "Output image: " << outputImagePath << "\n";
        } else {
            // Swap buffers for interactive mode
            glfwSwapBuffers(window);

            // Sleep to control frame rate (optional)
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }

        // Keep window open for interactive mode
        if (!renderTestMode) {
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

                // Use shader program
                shader.use();
                
                // Set uniforms
                const float* projData = reinterpret_cast<const float*>(&proj);
                const float* viewData = reinterpret_cast<const float*>(&view);
                
                shader.setMat4("projection", projData);
                shader.setMat4("view", viewData);
                
                // Sun color (convert double to float)
                shader.setFloat3("uSunColor", 
                    static_cast<float>(scenario.sun.color[0]),
                    static_cast<float>(scenario.sun.color[1]),
                    static_cast<float>(scenario.sun.color[2]));
                
                // Model matrix for sun (at origin, no scaling for now)
                Matrix4 model = Matrix4();
                const float* modelData = reinterpret_cast<const float*>(&model);
                shader.setMat4("model", modelData);
                
                // Render sun
                {
                    g_mesh.draw();
                }

                // Swap buffers for interactive mode
                glfwSwapBuffers(window);

                // Sleep to control frame rate (optional)
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }
        }

        // Cleanup
        if (g_mesh.vao != 0) glDeleteVertexArrays(1, &g_mesh.vao);
        if (g_mesh.vbo != 0) glDeleteBuffers(1, &g_mesh.vbo);
        if (g_mesh.ebo != 0) glDeleteBuffers(1, &g_mesh.ebo);
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }

    // Cleanup
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

#include <iostream>
#include <GL/glew.h>
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <thread>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <array>
#include <filesystem>
#include <cstdlib>
#include <limits>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "config/Config.h"
#include "config/ScenarioConfig.h"
#include "rendering/Mesh.h"
#include "rendering/Shader.h"

namespace fs = std::filesystem;

// Simple camera class for view matrix calculation
class Camera {
public:
    glm::vec3 position;
    glm::vec3 target;
    float fov;
    
    Camera() : position(0.0f), target(0.0f), fov(60.0f) {}
    
    void update(const config::ScenarioConfig& scenario) {
        // Use default camera position for now
        position = glm::vec3(15.0f, 2.0f, 8.0f);
        target = glm::vec3(0.0f);
        fov = 60.0f;
    }
    
    // View matrix (looking at target from position)
    glm::mat4 getViewMatrix() const {
        return glm::lookAt(position, target, glm::vec3(0.0f, 1.0f, 0.0f));
    }
};

// Global mesh for sun/planets
Mesh g_mesh;

void renderScene(const config::ScenarioConfig& scenario, const Camera& camera,
                 const Shader& shader, const Mesh& mesh, int width, int height) {
    glViewport(0, 0, width, height);
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::mat4 projection = glm::perspective(
        glm::radians(camera.fov), static_cast<float>(width) / height,
        0.1f, 1000.0f);
    glm::mat4 view = camera.getViewMatrix();

    shader.use();
    shader.setMat4("projection", glm::value_ptr(projection));
    shader.setMat4("view", glm::value_ptr(view));

    const auto& sun = scenario.sun;
    glm::mat4 sunModel(1.0f);
    sunModel = glm::translate(sunModel, glm::vec3(
        static_cast<float>(sun.position[0]),
        static_cast<float>(sun.position[1]),
        static_cast<float>(sun.position[2])));
    const float sunRadius = static_cast<float>(sun.radius);
    sunModel = glm::scale(sunModel, glm::vec3(sunRadius));
    shader.setMat4("model", glm::value_ptr(sunModel));
    shader.setFloat3("uColor", static_cast<float>(sun.color[0]),
                     static_cast<float>(sun.color[1]),
                     static_cast<float>(sun.color[2]));
    mesh.draw();

    for (const auto& planet : scenario.planets) {
        const float radius = static_cast<float>(planet.radius);
        glm::mat4 model(1.0f);
        model = glm::translate(model, glm::vec3(
            static_cast<float>(planet.position[0]),
            static_cast<float>(planet.position[1]),
            static_cast<float>(planet.position[2])));
        model = glm::scale(model, glm::vec3(radius));
        shader.setMat4("model", glm::value_ptr(model));
        shader.setFloat3("uColor", static_cast<float>(planet.color[0]),
                         static_cast<float>(planet.color[1]),
                         static_cast<float>(planet.color[2]));
        mesh.draw();
    }
}

struct PixelBounds {
    int count = 0;
    int minX = std::numeric_limits<int>::max();
    int minY = std::numeric_limits<int>::max();
    int maxX = -1;
    int maxY = -1;

    void include(int x, int y) {
        ++count;
        minX = std::min(minX, x);
        minY = std::min(minY, y);
        maxX = std::max(maxX, x);
        maxY = std::max(maxY, y);
    }
};

bool matchesColor(const std::vector<unsigned char>& pixels, int index,
                  const std::vector<double>& color) {
    for (int channel = 0; channel < 3; ++channel) {
        const int expected = static_cast<int>(std::lround(color[channel] * 255.0));
        if (std::abs(static_cast<int>(pixels[index + channel]) - expected) > 8) {
            return false;
        }
    }
    return true;
}

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
        
        // Generate sphere mesh (will be used for sun and all planets)
        g_mesh.generateSphere(32);
        
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
            std::cout << "Close the window to exit...\n";
        }

        // Create shader program
        Shader shader("shaders/basic.vert", "shaders/basic.frag");
        
        // Enable depth testing
        glEnable(GL_DEPTH_TEST);
        
        // Render exactly one frame for render-test mode
        if (renderTestMode) {
            glfwPollEvents();
            int width = 0;
            int height = 0;
            glfwGetFramebufferSize(window, &width, &height);
            if (width <= 0 || height <= 0) {
                std::cerr << "Render test framebuffer has invalid dimensions\n";
                return 1;
            }
            renderScene(scenario, camera, shader, g_mesh, width, height);

            // Call glFinish() before reading framebuffer
            glFinish();
            
            // Configure pixel packing for read
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadBuffer(GL_BACK);
            
            // Read rendered framebuffer (account for vertical flip)
            std::vector<unsigned char> pixels(width * height * 4);
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            
            // Check for OpenGL errors after reading
            GLenum err;
            while ((err = glGetError()) != GL_NO_ERROR) {
                std::cerr << "OpenGL error: " << err << "\n";
            }

            // Vertically flip the image data (GL_READ_PIXELS reads bottom-up)
            std::vector<unsigned char> flippedPixels(width * height * 4);
            for (int y = 0; y < height; y++) {
                int srcY = height - 1 - y;
                for (int x = 0; x < width; x++) {
                    int idx = (y * width + x) * 4;
                    int srcIdx = (srcY * width + x) * 4;
                    flippedPixels[idx] = pixels[srcIdx];
                    flippedPixels[idx + 1] = pixels[srcIdx + 1];
                    flippedPixels[idx + 2] = pixels[srcIdx + 2];
                    flippedPixels[idx + 3] = pixels[srcIdx + 3];
                }
            }

            if (scenario.planets.empty()) {
                std::cerr << "Render test FAILED: No planet is configured\n";
                return 1;
            }

            // Use the rendered corner pixel as the framebuffer's clear color.
            const std::array<unsigned char, 3> background = {
                flippedPixels[0], flippedPixels[1], flippedPixels[2]
            };
            PixelBounds drawnBounds;
            PixelBounds sunBounds;
            PixelBounds planetBounds;
            for (int y = 0; y < height; y++) {
                for (int x = 0; x < width; x++) {
                    int idx = (y * width + x) * 4;
                    if (flippedPixels[idx] != background[0] ||
                        flippedPixels[idx + 1] != background[1] ||
                        flippedPixels[idx + 2] != background[2]) {
                        drawnBounds.include(x, y);
                        if (matchesColor(flippedPixels, idx, scenario.sun.color)) {
                            sunBounds.include(x, y);
                        }
                        if (matchesColor(flippedPixels, idx, scenario.planets[0].color)) {
                            planetBounds.include(x, y);
                        }
                    }
                }
            }

            std::cout << "Image size: " << width << "x" << height << "\n";
            std::cout << "Background pixel: (" << static_cast<int>(background[0])
                      << ", " << static_cast<int>(background[1]) << ", "
                      << static_cast<int>(background[2]) << ")\n";
            auto printBounds = [](const char* label, const PixelBounds& bounds) {
                std::cout << label << ": " << bounds.count;
                if (bounds.count > 0) {
                    std::cout << ", bounding box: (" << bounds.minX << ", "
                              << bounds.minY << ")-(" << bounds.maxX << ", "
                              << bounds.maxY << ")";
                }
                std::cout << "\n";
            };
            printBounds("Non-background pixels", drawnBounds);
            printBounds("Sun-colored pixels", sunBounds);
            printBounds("Planet-colored pixels", planetBounds);

            // Write PNG using official stb_image_write API with stride parameter
            int result = stbi_write_png(outputImagePath.c_str(), width, height, 4, flippedPixels.data(), width * 4);
            
            if (result == 0) {
                std::cerr << "Failed to write PNG: " << outputImagePath << "\n";
                return 1;
            }

            if (drawnBounds.count == 0 || sunBounds.count == 0 || planetBounds.count == 0) {
                std::cerr << "Render test FAILED: Sun or planet is not visible\n";
                return 1;
            }
            const bool separate = sunBounds.maxX < planetBounds.minX ||
                                  planetBounds.maxX < sunBounds.minX ||
                                  sunBounds.maxY < planetBounds.minY ||
                                  planetBounds.maxY < sunBounds.minY;
            if (!separate) {
                std::cerr << "Render test FAILED: Sun and planet overlap in the image\n";
                return 1;
            }

            std::cout << "Render test completed successfully\n";
            std::cout << "Output image: " << outputImagePath << "\n";
        } else {
            while (!glfwWindowShouldClose(window)) {
                glfwPollEvents();
                int width = 0;
                int height = 0;
                glfwGetFramebufferSize(window, &width, &height);
                if (width > 0 && height > 0) {
                    renderScene(scenario, camera, shader, g_mesh, width, height);
                    glfwSwapBuffers(window);
                }
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

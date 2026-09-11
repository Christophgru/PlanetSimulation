#include <iostream>
#include <GL/glew.h>
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <thread>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <cstdlib>

#include "math/Vector3.h"
#include "config/Config.h"
#include "config/ScenarioConfig.h"
#include "rendering/Mesh.h"
#include "rendering/Shader.h"

namespace fs = std::filesystem;

// Simple 4x4 matrix class for transformations (column-major order for OpenGL)
class Matrix4 {
public:
    float data[16];
    
    Matrix4() {
        memset(data, 0, sizeof(data));
        // Identity matrix
        data[0] = data[5] = data[10] = data[15] = 1.0f;
    }
    
    static Matrix4 translate(float x, float y, float z) {
        Matrix4 m;
        m.data[0] = m.data[5] = m.data[10] = m.data[15] = 1.0f;
        m.data[12] = x;
        m.data[13] = y;
        m.data[14] = z;
        return m;
    }
    
    static Matrix4 perspective(double fov, double aspect, double near, double far) {
        Matrix4 m;
        double f = 1.0 / tan(fov * M_PI / 360.0); // Fixed: half-angle for perspective
        m.data[0] = static_cast<float>(f);
        m.data[5] = static_cast<float>(f);
        m.data[10] = static_cast<float>(-(far + near) / (far - near));
        m.data[11] = static_cast<float>(-1.0);
        m.data[14] = 0.0f;
        return m;
    }
    
    static Matrix4 lookAt(const Vector3& eye, const Vector3& target, const Vector3& up) {
        Matrix4 m;
        
        // Forward vector
        Vector3 forward = target - eye;
        forward.normalize();
        
        // Right vector (orthogonal to forward and up)
        Vector3 right = forward.cross(up);
        right.normalize();
        
        // Up vector (orthogonal to forward and right)
        Vector3 upVec = right.cross(forward);
        upVec.normalize();
        
        // Build view matrix (rotation + translation)
        m.data[0] = static_cast<float>(right.x);
        m.data[1] = static_cast<float>(right.y);
        m.data[2] = static_cast<float>(right.z);
        m.data[3] = 0.0f;
        
        m.data[4] = static_cast<float>(upVec.x);
        m.data[5] = static_cast<float>(upVec.y);
        m.data[6] = static_cast<float>(upVec.z);
        m.data[7] = 0.0f;
        
        m.data[8] = static_cast<float>(forward.x);
        m.data[9] = static_cast<float>(forward.y);
        m.data[10] = static_cast<float>(forward.z);
        m.data[11] = 0.0f;
        
        m.data[12] = static_cast<float>(-right.dot(eye));
        m.data[13] = static_cast<float>(-upVec.dot(eye));
        m.data[14] = static_cast<float>(-forward.dot(eye));
        m.data[15] = 1.0f;
        
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

    // Create window (or hidden framebuffer for render test)
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
        } else {
            // Keep window open for viewing
            std::cout << "Press Escape to exit...\n";
        }

        // Create shader program
        Shader shader("shaders/basic.vert", "shaders/basic.frag");
        
        // Enable depth testing
        glEnable(GL_DEPTH_TEST);
        
        // Set viewport
        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);

        while (!glfwWindowShouldClose(window)) {
            // Process events
            glfwPollEvents();

            // Setup projection matrix
            int width = renderTestMode ? 800 : 0;
            int height = renderTestMode ? 600 : 0;
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

            if (renderTestMode) {
                // Call glFinish() before reading framebuffer
                glFinish();
                
                // Read rendered framebuffer (account for vertical flip)
                std::vector<unsigned char> pixels(width * height * 4);
                glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                
                // Flip vertically for PNG output
                std::vector<unsigned char> flippedPixels(width * height * 4);
                for (int y = 0; y < height; y++) {
                    for (int x = 0; x < width; x++) {
                        int srcIdx = (y * width + x) * 4;
                        int dstIdx = ((height - 1 - y) * width + x) * 4;
                        flippedPixels[dstIdx] = pixels[srcIdx];
                        flippedPixels[dstIdx + 1] = pixels[srcIdx + 1];
                        flippedPixels[dstIdx + 2] = pixels[srcIdx + 2];
                        flippedPixels[dstIdx + 3] = pixels[srcIdx + 3];
                    }
                }
                
                // Smoke test: check for obvious failures
                bool smokeTestPassed = true;
                std::string smokeTestMsg;
                
                // Check if entire image is uniform clear color
                bool allSame = true;
                unsigned char firstPixelR = flippedPixels[0];
                unsigned char firstPixelG = flippedPixels[1];
                unsigned char firstPixelB = flippedPixels[2];
                for (size_t i = 4; i < pixels.size(); i += 4) {
                    if (pixels[i] != firstPixelR || 
                        pixels[i+1] != firstPixelG || 
                        pixels[i+2] != firstPixelB) {
                        allSame = false;
                        break;
                    }
                }
                
                // Check if entirely black
                bool allBlack = true;
                for (size_t i = 0; i < pixels.size(); i += 4) {
                    if (pixels[i] > 0 || pixels[i+1] > 0 || pixels[i+2] > 0) {
                        allBlack = false;
                        break;
                    }
                }
                
                // Check if no pixels differ from background
                bool noDiffFromBg = true;
                unsigned char bgR = static_cast<unsigned char>(0.1f * 255);
                unsigned char bgG = static_cast<unsigned char>(0.1f * 255);
                unsigned char bgB = static_cast<unsigned char>(0.15f * 255);
                for (size_t i = 0; i < pixels.size(); i += 4) {
                    if (pixels[i] != bgR || pixels[i+1] != bgG || pixels[i+2] != bgB) {
                        noDiffFromBg = false;
                        break;
                    }
                }
                
                if (allSame && allBlack) {
                    smokeTestPassed = false;
                    smokeTestMsg = "Smoke test FAILED: Entire image is uniform clear color";
                } else if (noDiffFromBg) {
                    smokeTestPassed = false;
                    smokeTestMsg = "Smoke test FAILED: No pixels differ from background color";
                }
                
                if (!smokeTestPassed) {
                    std::cerr << "Smoke test FAILED: " << smokeTestMsg << "\n";
                    std::exit(1);
                } else {
                    std::cout << "Smoke test PASSED\n";
                }
                
                // Write PNG using stb_image_write
                #include "stb_image_write.h"
                if (stbi__write_png(outputImagePath.c_str(), flippedPixels.data(), width, height, 4) == 0) {
                    std::cerr << "Failed to write PNG: " << outputImagePath << "\n";
                    std::exit(1);
                }
                
                std::cout << "Render test completed successfully\n";
                std::cout << "Output image: " << outputImagePath << "\n";
            } else {
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

#pragma once
#include "app/CommandLineOptions.h"
struct GLFWwindow;

namespace app {
// Owns the process GLFW lifetime. Construct before, destroy after GPU owners.
class Window {
public:
    explicit Window(const CommandLineOptions& options);
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    GLFWwindow* get() const { return window_; }
    void makeCurrent() const;
private:
    GLFWwindow* window_ = nullptr;
};
}

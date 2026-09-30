#pragma once
#include "rendering/camera/CameraInput.h"
#include "simulation/OrbitalSystem.h"
struct GLFWwindow;

namespace app {
struct InputContext {
    CameraInput* camera = nullptr;
    simulation::SimulationClock* clock = nullptr;
    bool reloadRequested = false;
    bool statsVisible = false;
    bool orbitsVisible = false;
};

void bindWindowInput(GLFWwindow* window, InputContext& input);
}

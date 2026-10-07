// Private blocking permutation controls reuse the frozen wind/native audit.
// No queue inspection or CPU reordering is linked into the shipping executable.
#define glfwSwapBuffers orderAuditedSwapBuffers
#include "../Probe.cpp"
#undef glfwSwapBuffers
#include "Inspection.h"

extern "C" void glfwSwapBuffers(GLFWwindow* window) {
    if(observing) try {
        order::install();
        order::finish(window);
    } catch(const std::exception& e) {
        error=e.what();glfwSetWindowShouldClose(window,GLFW_TRUE);
    }
    orderAuditedSwapBuffers(window);
}

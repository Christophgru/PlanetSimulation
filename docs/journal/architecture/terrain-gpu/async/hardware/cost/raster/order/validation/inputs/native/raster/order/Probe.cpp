// Private blocking permutation controls reuse the frozen wind/native audit.
// No queue inspection or CPU reordering is linked into the shipping executable.
#include "rendering/runtime/RendererState.h"
#include <GLFW/glfw3.h>
#include "../../BenchmarkObservation.h"
template<class State> nlohmann::json orderBenchmarkObservation(State&,GLFWwindow*);
#define glfwSwapBuffers orderAuditedSwapBuffers
#define benchmarkObservation orderBenchmarkObservation
#include "../../NativeProbe.cpp"
#undef benchmarkObservation
#undef glfwSwapBuffers
#include "../Hooks.h"
#include "Inspection.h"

template<class State> nlohmann::json orderBenchmarkObservation(State& r,GLFWwindow* window) {
    auto result=rasterBenchmarkObservation(r,window);
    result["grass_raster"]["full_draw_timing_eligible"]=false;
    result["blade_order"]={{"scope","All frames of this private blocking probe are excluded"},
        {"timing_acceptance",false},{"migration_acceptance",false},{"inspection",order::lastInspection}};
    order::lastInspection=nullptr;
    return result;
}

extern "C" void glfwSwapBuffers(GLFWwindow* window) {
    if(observing) try {
        order::install();
        order::finish(window);
    } catch(const std::exception& e) {
        error=e.what();glfwSetWindowShouldClose(window,GLFW_TRUE);
    }
    orderAuditedSwapBuffers(window);
}

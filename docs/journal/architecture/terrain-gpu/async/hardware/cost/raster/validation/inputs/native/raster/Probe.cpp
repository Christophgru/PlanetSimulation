// Observe the existing native loop. Only this executable links the wind/draw
// wrappers; production and the ordinary native/coverage probes stay unchanged.
#include "rendering/runtime/RendererState.h"
#include <GLFW/glfw3.h>
#include "../BenchmarkObservation.h"
template<class State> nlohmann::json rasterBenchmarkObservation(State&, GLFWwindow*);
#define benchmarkObservation rasterBenchmarkObservation
#include "../NativeProbe.cpp"
#undef benchmarkObservation
#include "Hooks.h"

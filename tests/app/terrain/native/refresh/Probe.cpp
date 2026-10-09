// Hold only the CPU terrain builder. The production frame loop, GPU fences,
// grass planner and input remain live, exposing grass starvation deterministically.
#define glfwPollEvents auditedPollEvents
#include "../coverage/Probe.cpp"
#undef glfwPollEvents
#include <atomic>

namespace {
std::atomic<bool> holdTerrain{false};
std::mutex holdMutex;
std::condition_variable holdChanged;
}
extern "C" rendering::TerrainCpuBuild realBuild(const rendering::TerrainBuildRequest&)
    asm("__real__ZN9rendering15buildTerrainCpuERKNS_19TerrainBuildRequestE");
extern "C" rendering::TerrainCpuBuild wrappedBuild(const rendering::TerrainBuildRequest&)
    asm("__wrap__ZN9rendering15buildTerrainCpuERKNS_19TerrainBuildRequestE");
extern "C" rendering::TerrainCpuBuild wrappedBuild(const rendering::TerrainBuildRequest& request) {
    if(holdTerrain.load()) {
        std::cout << "Terrain refresh test: worker held\n";
        std::unique_lock lock(holdMutex);
        if(!holdChanged.wait_for(lock,std::chrono::seconds(30),[]{return !holdTerrain.load();}))
            throw std::runtime_error("Terrain refresh test did not release its worker");
    }
    return realBuild(request);
}
extern "C" void glfwPollEvents() {
    if(observing) {
        loadControls();
        {
            std::lock_guard lock(holdMutex);
            holdTerrain.store(controls.value("hold_terrain",false));
        }
        if(!holdTerrain.load()) holdChanged.notify_all();
    }
    auditedPollEvents();
}

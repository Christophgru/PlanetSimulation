#include "rendering/runtime/RendererState.h"
#include <GLFW/glfw3.h>
#include <dlfcn.h>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace rendering {
struct RendererRecoveryProbe {static auto& state(Renderer& r) {return *r.impl_;}};
}
namespace {
using nlohmann::json;
rendering::Renderer* renderer=nullptr;
std::ofstream trace;
std::uint64_t frames=0,polls=0,blocking=0,waits=0,reads=0,finishes=0;
std::uint64_t fenceFaults=0,pollFaults=0,lastFenceToken=0,lastPollToken=0;
bool delayed=false,failFence=false,failPoll=false,observing=false;
json controls=json::object();
std::string error,contextRenderer,contextVersion;
PFNGLCLIENTWAITSYNCPROC clientWait;
PFNGLWAITSYNCPROC serverWait;
PFNGLGETBUFFERSUBDATAPROC bufferRead;
PFNGLFENCESYNCPROC createFence;
json counters() {return {{"polls",polls},{"blocking_polls",blocking},{"server_waits",waits},
    {"bulk_reads",reads},{"finishes",finishes},{"injected_fence_failures",fenceFaults},{"injected_poll_failures",pollFaults}};}
GLenum GLAPIENTRY poll(GLsync sync,GLbitfield flags,GLuint64 timeout) {
    ++polls;if(flags || timeout) ++blocking;
    if(failPoll) {failPoll=false;++pollFaults;return GL_WAIT_FAILED;}
    if(delayed || (controls.value("delay_retirement",false) && renderer &&
        rendering::RendererRecoveryProbe::state(*renderer).retiredResidentScene)) return GL_TIMEOUT_EXPIRED;
    return clientWait(sync,flags,timeout);
}
void GLAPIENTRY wait(GLsync sync,GLbitfield flags,GLuint64 timeout) {++waits;serverWait(sync,flags,timeout);}
void GLAPIENTRY read(GLenum target,GLintptr offset,GLsizeiptr size,void* data) {
    if(size>224) ++reads;bufferRead(target,offset,size,data);
}
GLsync GLAPIENTRY fence(GLenum condition,GLbitfield flags) {
    if(failFence && renderer && rendering::RendererRecoveryProbe::state(*renderer).sceneReloadPreparing()) {
        failFence=false;++fenceFaults;return nullptr;
    }
    return createFence(condition,flags);
}
void loadControls() {
    const auto* path=std::getenv("PLANET_NATIVE_CONTROL");if(!path) return;
    std::ifstream input(path);if(!input) return;
    auto next=json::parse(input,nullptr,false);if(next.is_discarded()) return;
    controls=std::move(next);delayed=controls.value("delay",false);
    const auto f=controls.value("fail_fence",0ull),p=controls.value("fail_poll",0ull);
    if(f && f!=lastFenceToken) {lastFenceToken=f;failFence=true;}
    if(p && p!=lastPollToken) {lastPollToken=p;failPoll=true;}
}
json vector(const glm::dvec3& v) {return {v.x,v.y,v.z};}
void sample(GLFWwindow* window) {
    auto& r=rendering::RendererRecoveryProbe::state(*renderer);loadControls();
    const auto stats=r.terrainJobs.stats();
    if(stats.running>1 || stats.queued>1 || stats.ready>1) throw std::runtime_error("Unbounded native worker ownership");
    json j={{"frame",++frames},{"mode",int(r.cameraInput.mode())},{"backend",r.options.terrainBackend},
        {"planner",r.options.terrainGrassPlanner},{"scenario",r.scene.scenario.name},{"renderer",contextRenderer},
        {"opengl_version",contextVersion},
        {"publication",r.terrainPublicationState()},{"reload",r.sceneReloadState()},
        {"paused",r.simulationClock.paused()},{"controls",controls},{"gl",counters()},
        {"worker",{{"running",stats.running},{"queued",stats.queued},{"ready",stats.ready}}}};
    j["keys"]={{"w",glfwGetKey(window,GLFW_KEY_W)==GLFW_PRESS},
        {"shift",glfwGetKey(window,GLFW_KEY_LEFT_SHIFT)==GLFW_PRESS},
        {"space",glfwGetKey(window,GLFW_KEY_SPACE)==GLFW_PRESS}};
    if(r.scene.surfaceCamera) {
        j["selected"]=r.scene.scenario.surface_camera.planet_index;
        j["camera_position"]=vector(r.scene.surfaceCamera->position());
        j["camera_local"]=vector(r.scene.bodies[r.scene.scenario.surface_camera.planet_index+1].toLocalPoint(r.scene.surfaceCamera->position()));
    }
    if(r.astronaut.motion.ready() && r.scene.surfaceCamera) {
        const auto& p=r.astronaut.motion.pose();const auto* trail=r.grass.procedural.existingTrail(r.astronaut.planetIndex);
        j["pose"]={{"root",vector(p.root)},{"walked_m",p.walkedMeters},{"effect_s",p.effectSeconds},
            {"airborne",p.airborne},{"boosting",p.boosting},{"thrust_n",p.thrustN},
            {"trail_segments",trail ? trail->segments().size() : 0},{"exhaust_particles",r.astronaut.exhaust.state().particles.size()}};
        if(p.navigation) j["pose"]["navigation"]={{"position_m",vector(p.navigation->position)},
            {"velocity_mps",vector(p.navigation->velocity)},{"up",vector(p.navigation->up)},
            {"outer_space",p.navigation->outerSpace},{"reference_body",p.navigation->referenceBody},
            {"gravity_indices",r.astronaut.motion.gravitySourceIndices()}};
    }
    const auto glError=glGetError();if(glError!=GL_NO_ERROR) throw std::runtime_error("Native GL error "+std::to_string(glError));
    trace << j.dump() << '\n' << std::flush;
    if(controls.value("close",false)) glfwSetWindowShouldClose(window,GLFW_TRUE);
}
struct Audit {
    Audit() {
        clientWait=__glewClientWaitSync;serverWait=__glewWaitSync;bufferRead=__glewGetBufferSubData;createFence=__glewFenceSync;
        __glewClientWaitSync=poll;__glewWaitSync=wait;__glewGetBufferSubData=read;__glewFenceSync=fence;observing=true;
    }
    ~Audit() {
        observing=false;__glewClientWaitSync=clientWait;__glewWaitSync=serverWait;
        __glewGetBufferSubData=bufferRead;__glewFenceSync=createFence;
    }
};
}
// Test executable interposition observes the production loop's presentation
// boundary. Product code has no observer, injected input or test controls.
extern "C" void glfwSwapBuffers(GLFWwindow* window) {
    static auto swap=reinterpret_cast<void(*)(GLFWwindow*)>(dlsym(RTLD_NEXT,"glfwSwapBuffers"));
    if(observing) try {sample(window);} catch(const std::exception& e) {error=e.what();glfwSetWindowShouldClose(window,GLFW_TRUE);}
    swap(window);
}
extern "C" void GLAPIENTRY glFinish() {
    static auto finish=reinterpret_cast<void(*)()>(dlsym(RTLD_NEXT,"glFinish"));
    if(observing) ++finishes;finish();
}
int main(int argc,char** argv) {
    std::cout << std::unitbuf;
    try {
        auto options=app::CommandLineOptions::parse(argc,argv);
        rendering::Renderer instance(options);renderer=&instance;
        contextRenderer=reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        contextVersion=reinterpret_cast<const char*>(glGetString(GL_VERSION));
        if(options.renderTestMode) return instance.run();
        const auto* path=std::getenv("PLANET_NATIVE_TRACE");if(!path) throw std::runtime_error("Missing native trace path");
        trace.open(path);if(!trace) throw std::runtime_error("Cannot open native trace");
        glfwSetWindowSize(glfwGetCurrentContext(),320,180);
        int result;
        {Audit audit;result=instance.run();}
        std::cout << "Native audit: " << counters().dump() << '\n';
        if(!error.empty()) throw std::runtime_error(error);
        if(blocking || waits || reads || finishes) throw std::runtime_error("Native frame wait/readback audit failed");
        return result;
    } catch(const std::exception& e) {std::cerr << "Native probe failed: " << e.what() << '\n';return 1;}
}

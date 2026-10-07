// Reuse the production-loop audit without adding product hooks or changing the
// ordinary timed probe. Rename its presentation interposer in this translation
// unit; the wrapper installs the inspection-only indirect-draw interposer.
#include "rendering/runtime/RendererState.h"
#include <GLFW/glfw3.h>
#define glfwSwapBuffers auditedSwapBuffers
#include "../NativeProbe.cpp"
#undef glfwSwapBuffers
#include "Inspection.h"

namespace {
coverage::Snapshot inspection;
PFNGLDRAWARRAYSINDIRECTPROC originalDraw=nullptr;
void GLAPIENTRY inspectDraw(GLenum mode,const void* offset) {
    if(!observing) {originalDraw(mode,offset);return;}
    try {inspection.draw(rendering::RendererRecoveryProbe::state(*renderer),mode,offset,controls,originalDraw);}
    catch(const std::exception& e) {error=e.what();glfwSetWindowShouldClose(glfwGetCurrentContext(),GLFW_TRUE);}
}
}
extern "C" void glfwSwapBuffers(GLFWwindow* window) {
    if(observing) try {
        if(!originalDraw && std::getenv("PLANET_NATIVE_COVERAGE")) {
            originalDraw=__glewDrawArraysIndirect;inspection.readBuffer=bufferRead;__glewDrawArraysIndirect=inspectDraw;
        }
        inspection.finish(rendering::RendererRecoveryProbe::state(*renderer));
        if(inspection.completed && controls.value("coverage_close",false)) glfwSetWindowShouldClose(window,GLFW_TRUE);
    } catch(const std::exception& e) {error=e.what();glfwSetWindowShouldClose(window,GLFW_TRUE);}
    auditedSwapBuffers(window);
}

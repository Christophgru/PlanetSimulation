// Reuse the production-loop audit without adding product hooks or changing the
// ordinary timed probe. Rename its presentation interposer in this translation
// unit; the wrapper installs the inspection-only indirect-draw interposer.
#include "rendering/runtime/RendererState.h"
#include <GLFW/glfw3.h>
#define glfwSwapBuffers auditedSwapBuffers
#include "../NativeProbe.cpp"
#undef glfwSwapBuffers
#include "Inspection.h"
#include "matched/Render.h"

extern "C" rendering::GrassPreparationStats realPrepare(rendering::GrassRenderer*,std::size_t,
    const Mesh&,const config::PlanetConfig&,double,const glm::dvec3&,std::uint64_t)
    asm("__real__ZN9rendering13GrassRenderer7prepareEmRK4MeshRKN6config12PlanetConfigEdRKN3glm3vecILi3EdLNS8_9qualifierE0EEEm");
extern "C" rendering::GrassPreparationStats wrappedPrepare(rendering::GrassRenderer*,std::size_t,
    const Mesh&,const config::PlanetConfig&,double,const glm::dvec3&,std::uint64_t)
    asm("__wrap__ZN9rendering13GrassRenderer7prepareEmRK4MeshRKN6config12PlanetConfigEdRKN3glm3vecILi3EdLNS8_9qualifierE0EEEm");
extern "C" rendering::GrassPreparationStats wrappedPrepare(rendering::GrassRenderer* self,std::size_t i,
    const Mesh& mesh,const config::PlanetConfig& planet,double units,const glm::dvec3& eye,std::uint64_t bytes) {
    if(coverage::retainPlan) {++coverage::skippedPreparations;return {};}
    return realPrepare(self,i,mesh,planet,units,eye,bytes);
}

namespace {
coverage::Snapshot inspection;
PFNGLDRAWARRAYSINDIRECTPROC originalDraw=nullptr;
void GLAPIENTRY inspectDraw(GLenum mode,const void* offset) {
    if(!observing) {originalDraw(mode,offset);return;}
    try {
        auto& r=rendering::RendererRecoveryProbe::state(*renderer);
        if(coverage::controlledPass==2) return;
        if(coverage::controlledPass==1) {
            GLint program=0;glGetIntegerv(GL_CURRENT_PROGRAM,&program);
            const bool main=GLuint(program)==r.grass.shader.id &&
                coverage::Snapshot::uniform(program,"uClipRadius",1)[0]<0;
            GLint func=0,ref=0,mask=0;
            if(main) {
                glGetIntegerv(GL_STENCIL_FUNC,&func);glGetIntegerv(GL_STENCIL_REF,&ref);glGetIntegerv(GL_STENCIL_VALUE_MASK,&mask);
                glStencilFunc(GL_ALWAYS,5,0xff);
            }
            coverage::controlledSnapshot.draw(r,mode,offset,coverage::controlledControls,originalDraw);
            if(main) glStencilFunc(func,ref,mask);
        } else inspection.draw(r,mode,offset,controls,originalDraw);
    }
    catch(const std::exception& e) {error=e.what();glfwSetWindowShouldClose(glfwGetCurrentContext(),GLFW_TRUE);}
}
}
extern "C" void glfwSwapBuffers(GLFWwindow* window) {
    if(observing) try {
        if(!originalDraw && std::getenv("PLANET_NATIVE_COVERAGE")) {
            originalDraw=__glewDrawArraysIndirect;inspection.readBuffer=bufferRead;__glewDrawArraysIndirect=inspectDraw;
        }
        auto& r=rendering::RendererRecoveryProbe::state(*renderer);
        const auto previous=inspection.completed;
        inspection.finish(r);
        if(inspection.completed!=previous && controls.value("coverage_matched",false)) {
            const auto character=r.astronautState();
            coverage::renderMatched(r,inspection,controls,bufferRead);
            if(character!=r.astronautState()) throw std::runtime_error("Matched inspection failed to restore native character/trail state");
        }
        if(inspection.completed && controls.value("coverage_close",false)) glfwSetWindowShouldClose(window,GLFW_TRUE);
    } catch(const std::exception& e) {error=e.what();glfwSetWindowShouldClose(window,GLFW_TRUE);}
    auditedSwapBuffers(window);
}

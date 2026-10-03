#include "rendering/character/effects/ExhaustRenderer.h"
#include <algorithm>
#include <stdexcept>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
namespace rendering {
ExhaustRenderer::ExhaustRenderer():shader("shaders/character/exhaust.vert","shaders/character/exhaust.frag",
    "shaders/terrain/terrain_shadow.glsl","shaders/atmosphere/atmosphere.glsl") {
    glGenVertexArrays(1,&vao_); glGenBuffers(1,&buffer_);
    glBindVertexArray(vao_); glBindBuffer(GL_ARRAY_BUFFER,buffer_);
    glBufferData(GL_ARRAY_BUFFER,ExhaustParticles::capacity*8*sizeof(float),nullptr,GL_STREAM_DRAW);
    for (int i=0;i<2;++i) {
        glEnableVertexAttribArray(i); glVertexAttribPointer(i,4,GL_FLOAT,GL_FALSE,8*sizeof(float),reinterpret_cast<void*>(i*4*sizeof(float)));
        glVertexAttribDivisor(i,1);
    }
    glBindVertexArray(0);
}
ExhaustRenderer::~ExhaustRenderer() { glDeleteBuffers(1,&buffer_); glDeleteVertexArrays(1,&vao_); glDeleteProgram(shader.id); }
void ExhaustRenderer::draw(const ExhaustState& state,const glm::dvec3& center,const glm::dmat3& orientation,
    double radius,double units,const glm::mat4& view,const glm::mat4& projection) {
    if (state.particles.size()>ExhaustParticles::capacity)
        throw std::invalid_argument("Exhaust draw exceeds pool capacity");
    if (state.particles.empty()) return;
    const auto inverse=glm::inverse(view); const auto eye=glm::dvec3(inverse[3]);
    const auto forward=-glm::dvec3(inverse[2]);
    auto sorted=state.particles;
    std::stable_sort(sorted.begin(),sorted.end(),[&](const auto& a,const auto& b) {
        return glm::dot(a.position/units-eye,forward)>glm::dot(b.position/units-eye,forward);
    });
    std::vector<float> instances; instances.reserve(sorted.size()*8);
    for (const auto& p:sorted) {
        const auto relative=glm::vec3(p.position/units-eye);
        const auto body=glm::vec3(glm::transpose(orientation)*(p.position/units-center)/radius);
        instances.insert(instances.end(),{relative.x,relative.y,relative.z,float(p.radius()/units),body.x,body.y,body.z,float(p.opacity())});
    }
    const bool blend=glIsEnabled(GL_BLEND),stencil=glIsEnabled(GL_STENCIL_TEST),cull=glIsEnabled(GL_CULL_FACE);
    GLboolean writes; glGetBooleanv(GL_DEPTH_WRITEMASK,&writes);
    GLint sr,dr,sa,da; glGetIntegerv(GL_BLEND_SRC_RGB,&sr); glGetIntegerv(GL_BLEND_DST_RGB,&dr);
    glGetIntegerv(GL_BLEND_SRC_ALPHA,&sa); glGetIntegerv(GL_BLEND_DST_ALPHA,&da);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA); glDepthMask(GL_FALSE);
    glDisable(GL_STENCIL_TEST); glDisable(GL_CULL_FACE);
    shader.use();
    const glm::mat4 transform=projection*glm::mat4(glm::mat3(view));
    shader.setMat4("uCameraProjection",glm::value_ptr(transform));
    shader.setFloat3("uEyeWorld",eye.x,eye.y,eye.z);
    const auto right=glm::normalize(glm::vec3(inverse[0])),up=glm::normalize(glm::vec3(inverse[1]));
    shader.setFloat3("uCameraRight",right.x,right.y,right.z); shader.setFloat3("uCameraUp",up.x,up.y,up.z);
    const glm::mat4 localRotation=glm::dmat4(glm::transpose(orientation));
    shader.setMat4("uBodyRotation",glm::value_ptr(localRotation)); shader.setFloat("uReferenceRadius",radius);
    glBindVertexArray(vao_); glBindBuffer(GL_ARRAY_BUFFER,buffer_);
    glBufferSubData(GL_ARRAY_BUFFER,0,instances.size()*sizeof(float),instances.data());
    glDrawArraysInstanced(GL_TRIANGLE_STRIP,0,4,sorted.size());
    glBindVertexArray(0); glDepthMask(writes); glBlendFuncSeparate(sr,dr,sa,da);
    if (!blend) glDisable(GL_BLEND); if (stencil) glEnable(GL_STENCIL_TEST); if (cull) glEnable(GL_CULL_FACE);
}
}

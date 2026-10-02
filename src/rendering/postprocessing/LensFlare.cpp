#include "rendering/postprocessing/LensFlare.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace rendering {
FlareEvidence LensFlare::draw(const glm::mat4& view, const glm::mat4& projection,
    const glm::dvec3& eye, const glm::dvec3& sun, double radius,
    const glm::vec3& tint, int width, int height) {
    FlareEvidence evidence;
    const glm::dvec4 clip=glm::dmat4(projection)*glm::dmat4(view)*glm::dvec4(sun,1);
    if (clip.w<=0 || std::abs(clip.z)>clip.w || width<=0 || height<=0) return evidence;
    const glm::dvec2 center=glm::dvec2(clip)/clip.w*.5+.5;
    const double distance=glm::length(sun-eye);
    if (distance<=radius) return evidence;
    const double angularRadius=std::asin(std::clamp(radius/distance,0.0,1.0));
    const double radiusPixels=.5*height*projection[1][1]*std::tan(angularRadius);
    const double expectedPixels=std::acos(-1.0)*radiusPixels*radiusPixels;
    if (expectedPixels<.1) return evidence;

    // A synchronous readback is deliberate: this mode has no frame-time target.
    // Occluders replace the Sun's stencil ID, including opaque grass and actors.
    GLint pack=4; glGetIntegerv(GL_PACK_ALIGNMENT,&pack); glPixelStorei(GL_PACK_ALIGNMENT,1);
    std::vector<unsigned char> objects(std::size_t(width)*height), rgba(objects.size()*4);
    glReadBuffer(GL_BACK);
    glReadPixels(0,0,width,height,GL_STENCIL_INDEX,GL_UNSIGNED_BYTE,objects.data());
    glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
    glPixelStorei(GL_PACK_ALIGNMENT,pack);
    double brightness=0;
    for (std::size_t i=0;i<objects.size();++i) if (objects[i]==1) {
        ++evidence.visibleSunPixels;
        brightness+=std::max({rgba[4*i],rgba[4*i+1],rgba[4*i+2]})/255.0;
    }
    if (!evidence.visibleSunPixels) return evidence;
    evidence.strength=std::clamp(evidence.visibleSunPixels/expectedPixels,0.0,1.0)*
        brightness/evidence.visibleSunPixels;
    if (evidence.strength<=0) return evidence;

    const GLboolean depth=glIsEnabled(GL_DEPTH_TEST), stencil=glIsEnabled(GL_STENCIL_TEST), blend=glIsEnabled(GL_BLEND);
    GLboolean depthWrite; glGetBooleanv(GL_DEPTH_WRITEMASK,&depthWrite);
    GLint srcRgb,dstRgb,srcAlpha,dstAlpha,rgbEquation,alphaEquation,previousVao,program;
    glGetIntegerv(GL_BLEND_SRC_RGB,&srcRgb); glGetIntegerv(GL_BLEND_DST_RGB,&dstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA,&srcAlpha); glGetIntegerv(GL_BLEND_DST_ALPHA,&dstAlpha);
    glGetIntegerv(GL_BLEND_EQUATION_RGB,&rgbEquation); glGetIntegerv(GL_BLEND_EQUATION_ALPHA,&alphaEquation);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&previousVao); glGetIntegerv(GL_CURRENT_PROGRAM,&program);
    glDisable(GL_DEPTH_TEST); glDisable(GL_STENCIL_TEST); glDepthMask(GL_FALSE);
    glEnable(GL_BLEND); glBlendEquation(GL_FUNC_ADD); glBlendFuncSeparate(GL_ONE,GL_ONE,GL_ZERO,GL_ONE);
    shader_.use();
    shader_.setFloat2("uViewport",width,height);
    shader_.setFloat2("uSun",center.x,center.y);
    shader_.setFloat3("uTint",tint.r,tint.g,tint.b);
    shader_.setFloat("uStrength",evidence.strength);
    shader_.setFloat("uRadius",radiusPixels/height);
    glBindVertexArray(vao_.id); glDrawArrays(GL_TRIANGLES,0,3);
    glBindVertexArray(previousVao); glUseProgram(program);
    glBlendFuncSeparate(srcRgb,dstRgb,srcAlpha,dstAlpha);
    glBlendEquationSeparate(rgbEquation,alphaEquation);
    glDepthMask(depthWrite);
    if (depth) glEnable(GL_DEPTH_TEST);
    if (stencil) glEnable(GL_STENCIL_TEST);
    if (!blend) glDisable(GL_BLEND);
    return evidence;
}
}

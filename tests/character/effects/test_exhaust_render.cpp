#include "rendering/character/effects/ExhaustRenderer.h"
#include "rendering/foliage/wind/WindField.h"
#include <gtest/gtest.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <iterator>
#include <array>
using namespace rendering;
TEST(ExhaustRender, TransparentGlossPreservesBackgroundDepthAndGlState) {
    GLuint fbo,color,depth; glGenFramebuffers(1,&fbo); glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    glGenRenderbuffers(1,&color); glBindRenderbuffer(GL_RENDERBUFFER,color); glRenderbufferStorage(GL_RENDERBUFFER,GL_RGBA16F,128,128);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,color);
    glGenRenderbuffers(1,&depth); glBindRenderbuffer(GL_RENDERBUFFER,depth); glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,128,128);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
    ASSERT_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER),GLenum(GL_FRAMEBUFFER_COMPLETE));
    glViewport(0,0,128,128); glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE); glDisable(GL_BLEND); glBlendFunc(GL_ONE,GL_ZERO);
    ExhaustRenderer renderer; ExhaustState state; state.nextId=1; state.particles.push_back({{0,0,-2},{0,0,0},.5,1.4,0});
    renderer.shader.use(); renderer.shader.setInt("uShadowsEnabled",0); renderer.shader.setInt("uAtmEnabled",0);
    renderer.shader.setInt("uLinearOutput",1); renderer.shader.setFloat("uExposure",1);
    renderer.shader.setFloat("uClipRadius",-1); renderer.shader.setFloat3("uIndirectLight",.2,.2,.2);
    renderer.shader.setFloat3("uSunDirection",0,0,1);
    const auto draw=[&](float sunlight) {
        glClearColor(.8,.1,.05,1); glClearDepth(1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        renderer.shader.use(); renderer.shader.setFloat3("uSunlight",sunlight,sunlight,sunlight);
        renderer.draw(state,{0,0,-4},glm::dmat3(1),1,1,glm::mat4(1),glm::ortho(-.2f,.2f,-.2f,.2f,.1f,10.f));
        std::array<unsigned char,4> pixel{}; glReadPixels(64,64,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel.data()); return pixel;
    };
    const auto shaded=draw(0),glossy=draw(3);
    EXPECT_GT(shaded[0],150); // Red background remains visible through the shell.
    EXPECT_GT(glossy[2],shaded[2]+20); // The real Sun/specular term creates a reflection highlight.
    GLfloat z=0; glReadPixels(64,64,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&z); EXPECT_FLOAT_EQ(z,1);
    GLboolean writes=false; glGetBooleanv(GL_DEPTH_WRITEMASK,&writes); EXPECT_TRUE(writes); EXPECT_FALSE(glIsEnabled(GL_BLEND));
    GLint factor=0; glGetIntegerv(GL_BLEND_SRC_RGB,&factor); EXPECT_EQ(factor,GL_ONE);
    EXPECT_EQ(state.particles[0].age,.5); EXPECT_EQ(state.particles[0].position,(glm::dvec3(0,0,-2)));
    EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
    glBindFramebuffer(GL_FRAMEBUFFER,0); glDeleteRenderbuffers(1,&depth); glDeleteRenderbuffers(1,&color); glDeleteFramebuffers(1,&fbo);
}
TEST(WindRender, CpuPerlinMatchesProductionGrassShaderAtNegativeAndWrappedCoordinates) {
    std::ifstream file("shaders/foliage/grass.vert"); std::string production{std::istreambuf_iterator<char>(file),{}};
    const auto start=production.find("float grassGradient"),end=production.find("mat3 axisRotation");
    ASSERT_NE(start,std::string::npos); ASSERT_NE(end,std::string::npos);
    const std::string source="#version 330 core\nuniform int uWindSeed; uniform vec3 uPoint; out float sampled;\n"+
        production.substr(start,end-start)+"\nvoid main(){sampled=grassPerlin(uPoint);gl_Position=vec4(0,0,0,1);}";
    GLuint shader=glCreateShader(GL_VERTEX_SHADER); const char* code=source.c_str(); glShaderSource(shader,1,&code,nullptr); glCompileShader(shader);
    GLint ok=0; glGetShaderiv(shader,GL_COMPILE_STATUS,&ok); ASSERT_TRUE(ok);
    GLuint program=glCreateProgram(); glAttachShader(program,shader); const char* output="sampled";
    glTransformFeedbackVaryings(program,1,&output,GL_INTERLEAVED_ATTRIBS); glLinkProgram(program); glGetProgramiv(program,GL_LINK_STATUS,&ok); ASSERT_TRUE(ok);
    GLuint vao,buffer; glGenVertexArrays(1,&vao); glBindVertexArray(vao); glGenBuffers(1,&buffer);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,buffer); glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,sizeof(float),nullptr,GL_STREAM_READ);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,buffer); glUseProgram(program); glEnable(GL_RASTERIZER_DISCARD);
    for (int seed:{0,17,-21}) for (int i=0;i<40;++i) {
        const glm::vec3 point(-513.13f+i*1.71f,256.3f-i*3.7f,i*.3f);
        glUniform1i(glGetUniformLocation(program,"uWindSeed"),seed); glUniform3fv(glGetUniformLocation(program,"uPoint"),1,glm::value_ptr(point));
        glBeginTransformFeedback(GL_POINTS); glDrawArrays(GL_POINTS,0,1); glEndTransformFeedback();
        float actual=0; glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER,0,sizeof(float),&actual);
        EXPECT_NEAR(actual,WindField::perlin(point,seed),2e-5) << i << ' ' << seed;
    }
    glDisable(GL_RASTERIZER_DISCARD); glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,0); glBindVertexArray(0); glUseProgram(0);
    glDeleteBuffers(1,&buffer); glDeleteVertexArrays(1,&vao); glDeleteProgram(program); glDeleteShader(shader);
    EXPECT_EQ(glGetError(),GLenum(GL_NO_ERROR));
}
int main(int argc,char** argv) {
    ::testing::InitGoogleTest(&argc,argv); if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE); glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
    auto* window=glfwCreateWindow(128,128,"Exhaust regression",nullptr,nullptr); if (!window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window); glewExperimental=GL_TRUE; if (glewInit()!=GLEW_OK) return 1;
    while (glGetError()!=GL_NO_ERROR) {} const int result=RUN_ALL_TESTS(); glfwDestroyWindow(window); glfwTerminate(); return result;
}

#include "rendering/foliage/GrassWind.h"
#include "rendering/foliage/GrassLod.h"
#include <gtest/gtest.h>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {
// Capture the production vertex shader's positions and noise, without pixel
// quantization hiding discontinuities. The existing GL test main owns a context.
class WindProbe {
    GLuint program_ = 0, vao_ = 0, buffer_ = 0;
public:
    WindProbe() {
        std::ifstream file("shaders/foliage/grass.vert");
        std::string source{std::istreambuf_iterator<char>(file), {}};
        const auto main = source.find("void main()");
        if (main == std::string::npos) throw std::runtime_error("Missing grass shader main");
        source.replace(main, 11, "void grassMain()");
        source += "\nuniform vec3 uNoisePoint; out float windNoise, lodEnd;\n"
                  "void main() { grassMain(); windNoise=grassPerlin(uNoisePoint); lodEnd=grassLodFadeEnd(aVariation.w); }\n";
        const GLuint shader = glCreateShader(GL_VERTEX_SHADER);
        const char* code = source.c_str();
        glShaderSource(shader, 1, &code, nullptr);
        glCompileShader(shader);
        GLint ok = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            std::array<char, 2048> log{};
            glGetShaderInfoLog(shader, log.size(), nullptr, log.data());
            glDeleteShader(shader);
            throw std::runtime_error(log.data());
        }
        program_ = glCreateProgram();
        glAttachShader(program_, shader);
        const char* outputs[] = {"vBodyPosition", "vWorldPosition", "windNoise", "lodEnd"};
        glTransformFeedbackVaryings(program_, 4, outputs, GL_INTERLEAVED_ATTRIBS);
        glLinkProgram(program_);
        glDeleteShader(shader);
        glGetProgramiv(program_, GL_LINK_STATUS, &ok);
        if (!ok) {
            glDeleteProgram(program_);
            throw std::runtime_error("Cannot link grass transform feedback probe");
        }
        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &buffer_);
        glBindVertexArray(vao_);
        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buffer_);
        glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 8*sizeof(float), nullptr, GL_STREAM_READ);
        glUseProgram(program_);
        glUniform1i(glGetUniformLocation(program_,"uTerrainVertices"),8);
        glUniform1i(glGetUniformLocation(program_,"uTerrainIndices"),9);
        uniform("uMetersPerRadius", 100);
        uniform("uGrassHeight", 1.5f);
        uniform("uGrassWidth", .1f);
        uniform("uDrawDistance", 40);
        glUniform1i(glGetUniformLocation(program_, "uSegments"), 6);
        for (const char* name : {"model", "view", "projection", "uShadowMatrix"})
            glUniformMatrix4fv(glGetUniformLocation(program_, name), 1, GL_FALSE, glm::value_ptr(glm::mat4(1)));
        glVertexAttrib4f(2, .3f, .2f, 1, .5f);
    }
    ~WindProbe() {
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);
        glBindVertexArray(0);
        glUseProgram(0);
        glDeleteBuffers(1, &buffer_);
        glDeleteVertexArrays(1, &vao_);
        glDeleteProgram(program_);
    }
    void uniform(const char* name, float value) {
        glUniform1f(glGetUniformLocation(program_, name), value);
    }
    void seed(int value) { glUniform1i(glGetUniformLocation(program_,"uWindSeed"),value); }
    void frequencies(float gust,float direction,float flutter) {
        glUniform3f(glGetUniformLocation(program_,"uWindFrequencies"),gust,direction,flutter);
    }
    void segments(int value) { glUniform1i(glGetUniformLocation(program_, "uSegments"),value); }
    void variation(float value) { glVertexAttrib4f(2,.3f,.2f,1,value); }
    std::array<float, 8> sample(glm::vec3 root, double seconds, float strength=1,
                                int vertex=12, glm::mat4 model=glm::mat4(1), float distance=0) {
        const auto up=glm::normalize(root);
        glVertexAttrib3fv(0, glm::value_ptr(root));
        glVertexAttrib3fv(1, glm::value_ptr(up));
        glUniform3fv(glGetUniformLocation(program_, "uNoisePoint"), 1, glm::value_ptr(root));
        const auto eye=root+up*(distance/100);
        glUniform3fv(glGetUniformLocation(program_, "uGrassEyeBody"), 1, glm::value_ptr(eye));
        glUniformMatrix4fv(glGetUniformLocation(program_, "model"), 1, GL_FALSE, glm::value_ptr(model));
        uniform("uTime", rendering::grassWindTime(seconds));
        uniform("uWindStrength", strength);
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buffer_);
        glEnable(GL_RASTERIZER_DISCARD);
        glBeginTransformFeedback(GL_POINTS);
        glDrawArrays(GL_POINTS, vertex, 1);
        glEndTransformFeedback();
        glDisable(GL_RASTERIZER_DISCARD);
        std::array<float, 8> result{};
        glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof(result), result.data());
        EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
        for (float value : result) EXPECT_TRUE(std::isfinite(value));
        return result;
    }
};

glm::vec3 body(const std::array<float, 8>& sample) { return {sample[0], sample[1], sample[2]}; }
}

TEST(GrassWind, CoverageRetirementNeverSinksAndLowQuadsKeepBothTopVertices) {
    WindProbe probe;
    const glm::vec3 root(0,0,1);
    for (float distance : {5.f,60.f,400.f}) {
        probe.uniform("uDrawDistance",distance);
        for (int seed=0; seed<64; ++seed) {
            const float variation=seed/64.f;
            probe.variation(variation);
            const float end=rendering::grassLodFadeEnd(variation,distance);
            const auto sample=[&](float d,int vertex) { return probe.sample(root,3,1,vertex,glm::mat4(1),d); };
            EXPECT_NEAR(sample(0,12)[7],end,.0001f);
            const auto before=body(sample(end-.001f,12)), after=body(sample(end+.001f,12));
            EXPECT_LT(glm::length(before-after)*100,.003f);
            EXPECT_GT(glm::length(after-root)*100,1.4f);
            const auto left=body(sample(end+1,0)), right=body(sample(end+1,1));
            EXPECT_LT(glm::length((left+right)*.5f-root),.000001f);
        }
    }
    probe.uniform("uDrawDistance",60);
    probe.variation(.5f);
    const auto sample=[&](int vertex) { return body(probe.sample(root,3,1,vertex,glm::mat4(1),16)); };
    probe.segments(6);
    const auto left=sample(0), tipLeft=sample(12), tipRight=sample(13), middle=sample(6);
    EXPECT_GT(glm::length(tipRight-tipLeft),.00005f);
    EXPECT_LT(glm::length(middle-glm::mix(left,tipLeft,.5f)),.000001f);
    probe.segments(1);
    EXPECT_EQ(sample(0),left); EXPECT_EQ(sample(2),tipLeft); EXPECT_EQ(sample(3),tipRight);
}

TEST(GrassWind, ConfiguredSeedAndFrequenciesChangeNoiseAndKeepRootsFixed) {
    WindProbe probe;
    const glm::vec3 root(.013f,.027f,1);
    const auto original=probe.sample(root,3);
    probe.seed(123);
    EXPECT_NE(original,probe.sample(root,3));
    probe.frequencies(.2f,.05f,1.4f);
    EXPECT_NE(original,probe.sample(root,3));
    const auto left=body(probe.sample(root,3,1,0)), right=body(probe.sample(root,3,1,1));
    EXPECT_LT(glm::length((left+right)*.5f-root),.000001f);
}

TEST(GrassWind, GradientFieldIsContinuousAtLatticeAndPeriodBoundaries) {
    WindProbe probe;
    float minimum=1, maximum=-1;
    for (int i=0; i<32; ++i) {
        const auto p=glm::vec3(.17f+i*.31f, .43f-i*.13f, .71f+i*.21f);
        const float n=probe.sample(p, 0)[6];
        minimum=std::min(minimum,n); maximum=std::max(maximum,n);
        EXPECT_NEAR(n, probe.sample(p+glm::vec3(256,-256,256),0)[6], .0001f);
    }
    EXPECT_LT(minimum, -.2f); EXPECT_GT(maximum, .2f);
    for (int axis=0; axis<3; ++axis) for (float boundary : {-256.f,-1.f,0.f,1.f,256.f}) {
        glm::vec3 p(.37f,.61f,.23f), step(0);
        p[axis]=boundary; step[axis]=.001f;
        const float left=probe.sample(p-step,0)[6], center=probe.sample(p,0)[6], right=probe.sample(p+step,0)[6];
        EXPECT_NEAR(left,right,.006f);
        EXPECT_NEAR((center-left)/.001f,(right-center)/.001f,.08f);
    }
    // Gradient noise vanishes at lattice corners; value noise generally does not.
    EXPECT_FLOAT_EQ(probe.sample({-1,2,3},0)[6],0);
}

TEST(GrassWind, BladesStayRootedAndMoveSmoothlyThroughClockWrapAtBothPoles) {
    WindProbe probe;
    for (float pole : {-1.f,1.f}) {
        const glm::vec3 root(.013f,.027f,pole);
        EXPECT_NE(body(probe.sample(root,0)),body(probe.sample(root,2)));
        EXPECT_EQ(probe.sample(root,2),probe.sample(root,2));
        EXPECT_EQ(probe.sample(root,0,0),probe.sample(root,2,0));
        EXPECT_EQ(body(probe.sample(root,0,1,0)),body(probe.sample(root,2,1,0)));
        const double wrap=rendering::grassWindPeriodSeconds/2;
        for (double time : {-wrap,0.0,wrap}) {
            const auto a=body(probe.sample(root,time-.001));
            const auto b=body(probe.sample(root,time+.001));
            EXPECT_LT(glm::length(a-b)*100,.003f); // No millisecond-scale jump.
        }
        EXPECT_EQ(probe.sample(root,3),probe.sample(root,3+rendering::grassWindPeriodSeconds*1000000));
        // Sampling in the body frame makes planetary transforms independent
        // of local deformation. Both render passes use this same phase/field.
        const auto transform=glm::rotate(glm::translate(glm::mat4(1),{12,-8,3}),.7f,glm::vec3(0,1,0));
        const auto local=probe.sample(root,3), rotated=probe.sample(root,3,1,12,transform);
        EXPECT_EQ(body(local),body(rotated));
        const auto expected=transform*glm::vec4(body(local),1);
        for (int axis=0; axis<3; ++axis) EXPECT_NEAR(rotated[3+axis],expected[axis],.00001f);
    }
}

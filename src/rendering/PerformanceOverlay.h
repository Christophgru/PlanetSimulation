#pragma once
#include <array>
#include <cstdio>
#include <string>
#include <vector>
#include <GL/glew.h>
#include "rendering/Shader.h"

namespace rendering {
// Tiny batched vector font: no font dependency, texture atlas, or per-letter draw calls.
class PerformanceOverlay {
public:
    PerformanceOverlay() : shader_("shaders/performance_overlay.vert", "shaders/performance_overlay.frag") {
        glGenVertexArrays(1, &vao_); glGenBuffers(1, &buffer_);
        glBindVertexArray(vao_); glBindBuffer(GL_ARRAY_BUFFER, buffer_);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,5*sizeof(float),nullptr);
        glEnableVertexAttribArray(1); glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,5*sizeof(float),reinterpret_cast<void*>(2*sizeof(float)));
        glBindVertexArray(0);
    }
    ~PerformanceOverlay() {
        glDeleteBuffers(1,&buffer_); glDeleteVertexArrays(1,&vao_); glDeleteProgram(shader_.id);
    }
    PerformanceOverlay(const PerformanceOverlay&) = delete;
    PerformanceOverlay& operator=(const PerformanceOverlay&) = delete;
    void draw(bool visible, int width, int height, double fps, double frameMs, double gpuMs, bool gpuReady) {
        if (!visible || width <= 0 || height <= 0) return;
        char lines[160], gpu[40];
        if (gpuReady) std::snprintf(gpu,sizeof(gpu),"GPU %.1f MS",gpuMs);
        else std::snprintf(gpu,sizeof(gpu),"GPU -- MS");
        if (fps > 0) std::snprintf(lines,sizeof(lines),"FPS %.1f\nFRAME %.1f MS\n%s",fps,frameMs,gpu);
        else std::snprintf(lines,sizeof(lines),"FPS --\nFRAME -- MS\n%s",gpu);
        if (text_ != lines) {
            text_ = lines; vertices_.clear();
            rectangle(12,12,270,74,0.025f);
            float x=24,y=22;
            for (char c : text_) {
                if (c == '\n') { x=24; y+=20; continue; }
                const auto glyph = rows(c);
                for (int row=0;row<7;++row) for (int col=0;col<5;++col)
                    if (glyph[row] & (1 << (4-col))) rectangle(x+col*2,y+row*2,2,2,0.95f);
                x += 12;
            }
            glBindBuffer(GL_ARRAY_BUFFER,buffer_);
            glBufferData(GL_ARRAY_BUFFER,vertices_.size()*sizeof(float),vertices_.data(),GL_DYNAMIC_DRAW);
        }
        glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE);
        glDisable(GL_BLEND); glDisable(GL_STENCIL_TEST);
        shader_.use(); shader_.setFloat2("uViewport",width,height);
        glBindVertexArray(vao_); glDrawArrays(GL_TRIANGLES,0,vertices_.size()/5); glBindVertexArray(0);
        glDepthMask(GL_TRUE); glEnable(GL_DEPTH_TEST);
    }
private:
    Shader shader_;
    GLuint vao_=0,buffer_=0;
    std::vector<float> vertices_;
    std::string text_;
    void rectangle(float x,float y,float width,float height,float color) {
        for (const auto point : {std::array<float,2>{x,y}, {x+width,y}, {x,y+height},
                                 {x,y+height}, {x+width,y}, {x+width,y+height}})
            vertices_.insert(vertices_.end(),{point[0],point[1],color,color,color});
    }
    static std::array<unsigned char,7> rows(char c) {
        switch(c) {
        case '0': return {14,17,19,21,25,17,14}; case '1': return {4,12,4,4,4,4,14};
        case '2': return {14,17,1,2,4,8,31}; case '3': return {30,1,1,14,1,1,30};
        case '4': return {2,6,10,18,31,2,2}; case '5': return {31,16,16,30,1,1,30};
        case '6': return {14,16,16,30,17,17,14}; case '7': return {31,1,2,4,8,8,8};
        case '8': return {14,17,17,14,17,17,14}; case '9': return {14,17,17,15,1,1,14};
        case 'F': return {31,16,16,30,16,16,16}; case 'P': return {30,17,17,30,16,16,16};
        case 'S': return {15,16,16,14,1,1,30}; case 'R': return {30,17,17,30,20,18,17};
        case 'A': return {14,17,17,31,17,17,17}; case 'M': return {17,27,21,21,17,17,17};
        case 'E': return {31,16,16,30,16,16,31}; case 'G': return {14,17,16,23,17,17,14};
        case 'U': return {17,17,17,17,17,17,14}; case '.': return {0,0,0,0,0,12,12};
        case '-': return {0,0,0,31,0,0,0}; default: return {};
        }
    }
};
} // namespace rendering

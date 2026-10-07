#pragma once
#include <GL/glew.h>
#include <stdexcept>

namespace coverage {
// Excluded inspection resource: two float maps plus depth, independent of the
// final sampling resolution. No full-resolution supersampled GPU allocation.
struct AreaTarget {
    GLuint fbo=0,textures[2]{},depth=0;
    explicit AreaTarget(int edge) {
        glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
        glGenTextures(2,textures);
        for(int i=0;i<2;++i) {
            glBindTexture(GL_TEXTURE_2D,textures[i]);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,edge,edge,0,GL_RGBA,GL_FLOAT,nullptr);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
            glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0+i,GL_TEXTURE_2D,textures[i],0);
        }
        const GLenum buffers[]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1};glDrawBuffers(2,buffers);
        glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);
        glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,edge,edge);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)
            throw std::runtime_error("Incomplete tiled area target");
    }
    ~AreaTarget() {glDeleteRenderbuffers(1,&depth);glDeleteTextures(2,textures);glDeleteFramebuffers(1,&fbo);}
    AreaTarget(const AreaTarget&)=delete;
    AreaTarget& operator=(const AreaTarget&)=delete;
};
}

#pragma once
#include <GL/glew.h>
#include <stdexcept>
namespace coverage {
struct LiveTarget {
    GLuint fbo=0,textures[4]{},depth=0;
    explicit LiveTarget(int edge) {
        glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glGenTextures(4,textures);
        for(int i=0;i<4;++i) {
            glBindTexture(GL_TEXTURE_2D,textures[i]);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,edge,edge,0,GL_RGBA,GL_FLOAT,nullptr);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
            glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0+i,GL_TEXTURE_2D,textures[i],0);
        }
        const GLenum buffers[]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1,GL_COLOR_ATTACHMENT2,GL_COLOR_ATTACHMENT3};glDrawBuffers(4,buffers);
        glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);
        glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH24_STENCIL8,edge,edge);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER,depth);
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE) throw std::runtime_error("Incomplete live area target");
    }
    ~LiveTarget() {glDeleteRenderbuffers(1,&depth);glDeleteTextures(4,textures);glDeleteFramebuffers(1,&fbo);}
    LiveTarget(const LiveTarget&)=delete;
    LiveTarget& operator=(const LiveTarget&)=delete;
};
}

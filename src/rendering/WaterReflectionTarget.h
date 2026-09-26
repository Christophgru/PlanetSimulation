#pragma once

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <GL/glew.h>

namespace rendering {

class WaterReflectionTarget {
public:
    WaterReflectionTarget() = default;
    WaterReflectionTarget(const WaterReflectionTarget&) = delete;
    WaterReflectionTarget& operator=(const WaterReflectionTarget&) = delete;
    ~WaterReflectionTarget() { destroy(); }

    void ensure(int viewportWidth, int viewportHeight, bool linearColor = false) {
        if (viewportWidth <= 0 || viewportHeight <= 0)
            throw std::invalid_argument("Water reflection viewport must be positive");
        const double scale = std::min(1.0, 1024.0 / viewportWidth);
        const int requestedWidth = std::max(1, static_cast<int>(std::lround(
            viewportWidth * scale)));
        const int requestedHeight = std::max(1, static_cast<int>(std::lround(
            viewportHeight * scale)));
        if (requestedWidth == width_ && requestedHeight == height_ && linearColor == linearColor_ && framebuffer_ != 0)
            return;

        destroy();
        linearColor_ = linearColor;
        width_ = requestedWidth;
        height_ = requestedHeight;
        glGenFramebuffers(1, &framebuffer_);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);

        glGenTextures(1, &colorTexture_);
        glBindTexture(GL_TEXTURE_2D, colorTexture_);
        glTexImage2D(GL_TEXTURE_2D, 0, linearColor ? GL_RGBA16F : GL_RGBA8, width_, height_, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, colorTexture_, 0);

        glGenRenderbuffers(1, &depthBuffer_);
        glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer_);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width_, height_);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                  GL_RENDERBUFFER, depthBuffer_);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            destroy();
            throw std::runtime_error("Water reflection framebuffer is incomplete");
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void bind() const { glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_); }
    GLuint framebuffer() const { return framebuffer_; }
    GLuint colorTexture() const { return colorTexture_; }
    int width() const { return width_; }
    int height() const { return height_; }

    void destroy() {
        if (depthBuffer_ != 0) glDeleteRenderbuffers(1, &depthBuffer_);
        if (colorTexture_ != 0) glDeleteTextures(1, &colorTexture_);
        if (framebuffer_ != 0) glDeleteFramebuffers(1, &framebuffer_);
        framebuffer_ = colorTexture_ = depthBuffer_ = 0;
        width_ = height_ = 0;
    }

private:
    GLuint framebuffer_ = 0;
    GLuint colorTexture_ = 0;
    GLuint depthBuffer_ = 0;
    bool linearColor_ = false;
    int width_ = 0;
    int height_ = 0;
};

} // namespace rendering

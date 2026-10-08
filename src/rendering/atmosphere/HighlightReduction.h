#pragma once

#include <cmath>
#include <array>
#include <bit>
#include <cstdint>
#include <GL/glew.h>
#include "rendering/Shader.h"

namespace rendering {
// Exact weighted selection for RGBA16F scene peaks. Storage is fixed at 128 KiB
// plus one double regardless of viewport; tile generation remains shared with
// the GL 3.3 reference path. This owner is created only on GL 4.3 contexts.
class HighlightReduction {
public:
    HighlightReduction() : shader_("shaders/atmosphere/highlight.comp") {
        glGenBuffers(1, &histogram_);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, histogram_);
        glBufferData(GL_SHADER_STORAGE_BUFFER, 32769 * sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        glGenTextures(1, &result_);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, result_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32UI, 1, 1, 0, GL_RG_INTEGER, GL_UNSIGNED_INT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glGenFramebuffers(1, &framebuffer_);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, result_, 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            throw std::runtime_error("Highlight reduction framebuffer is incomplete");
    }
    ~HighlightReduction() {
        glDeleteBuffers(1, &histogram_); glDeleteTextures(1, &result_);
        glDeleteFramebuffers(1, &framebuffer_); glDeleteProgram(shader_.id);
    }
    HighlightReduction(const HighlightReduction&) = delete;
    HighlightReduction& operator=(const HighlightReduction&) = delete;

    double reduce(GLuint tiles, int width, int height, double requested) {
        shader_.use(); shader_.setInt("uTiles", 0); glUniform1d(glGetUniformLocation(shader_.id, "uRequested"), requested);
        glUniform2i(glGetUniformLocation(shader_.id, "uSize"), width, height);
        const double linearWhite = std::pow((248.0 / 255.0 + 0.055) / 1.055, 2.4);
        glUniform1d(glGetUniformLocation(shader_.id, "uWhiteLimit"), -std::log1p(-linearWhite));
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, tiles);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, histogram_);
        glBindImageTexture(0, result_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG32UI);
        shader_.setInt("uMode", 0); glDispatchCompute(129, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        shader_.setInt("uMode", 1);
        const auto count = static_cast<GLuint>((width + 7) / 8) * ((height + 7) / 8);
        glDispatchCompute((count + 255) / 256, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        shader_.setInt("uMode", 2); glDispatchCompute(1, 1, 1);
        glMemoryBarrier(GL_FRAMEBUFFER_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
        // Keep the existing CPU exposure receipt for capture/analysis callers.
        // Only this bounded scalar crosses the bus; no CPU tile loop or sort.
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
        std::array<GLuint, 2> words{};
        glReadPixels(0, 0, 1, 1, GL_RG_INTEGER, GL_UNSIGNED_INT, words.data());
        glBindImageTexture(0, 0, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG32UI);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        return std::bit_cast<double>(std::uint64_t(words[0]) | (std::uint64_t(words[1]) << 32));
    }
private:
    Shader shader_;
    GLuint histogram_ = 0, result_ = 0, framebuffer_ = 0;
};
} // namespace rendering

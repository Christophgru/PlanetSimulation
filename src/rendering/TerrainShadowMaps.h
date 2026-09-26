#pragma once

#include <vector>
#include <stdexcept>
#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>
#include "config/LightingConfig.h"
#include "rendering/Shader.h"
#include "rendering/TerrainShadowProjection.h"

namespace rendering {

// One depth texture per planet, reused by all scene and water reflection passes.
class TerrainShadowMaps {
public:
    TerrainShadowMaps() = default;
    TerrainShadowMaps(const TerrainShadowMaps&) = delete;
    TerrainShadowMaps& operator=(const TerrainShadowMaps&) = delete;
    ~TerrainShadowMaps() { destroy(); }

    void ensure(std::size_t count, const config::TerrainShadowConfig& settings) {
        settings.validate();
        if (!settings.enabled) { destroy(); return; }
        if (textures_.size() == count && resolution_ == settings.resolution) return;
        destroy();
        GLint maximum = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximum);
        if (settings.resolution > maximum)
            throw std::runtime_error("Shadow resolution exceeds GL_MAX_TEXTURE_SIZE");
        resolution_ = settings.resolution;
        matrices_.resize(count);
        cache_.resize(count);
        textures_.resize(count);
        glGenFramebuffers(1, &framebuffer_);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        glGenTextures(static_cast<GLsizei>(count), textures_.data());
        glActiveTexture(GL_TEXTURE1);
        for (GLuint texture : textures_) {
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, resolution_, resolution_,
                         0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
            const float border[] = {1, 1, 1, 1};
            glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, texture, 0);
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glActiveTexture(GL_TEXTURE0);
                destroy();
                throw std::runtime_error("Terrain shadow framebuffer is incomplete");
            }
        }
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    bool beginIfNeeded(std::size_t index,const Shader& shader,const glm::dvec3& sun,
                       double extent,std::uint64_t revision) {
        if (!cache_.at(index).updateNeeded(sun,extent,resolution_,revision)) return false;
        begin(index,shader,sun,extent);
        return true;
    }

    // Caller draws the full terrain mesh after this, including off-camera ridges.
    void begin(std::size_t index, const Shader& depthShader,
               const glm::dvec3& localSunDirection, double extent) {
        matrices_.at(index) = glm::mat4(terrainShadowProjection(localSunDirection, extent));
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, textures_.at(index), 0);
        glViewport(0, 0, resolution_, resolution_);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glClearDepth(1.0);
        glClear(GL_DEPTH_BUFFER_BIT);
        depthShader.use();
        depthShader.setMat4("uShadowMatrix", glm::value_ptr(matrices_[index]));
    }

    void bindForShading(std::size_t index, const Shader& shader,
                        const config::TerrainShadowConfig& settings,
                        float radiusScale = 1.0f) const {
        shader.setInt("uShadowsEnabled", settings.enabled ? 1 : 0);
        shader.setInt("uShadowMap", 1);
        if (!settings.enabled) return;
        const glm::mat4 matrix = glm::scale(matrices_.at(index), glm::vec3(radiusScale));
        shader.setMat4("uShadowMatrix", glm::value_ptr(matrix));
        // Projection's depth range equals its width, so one texel is 1/resolution.
        shader.setFloat("uShadowBias", static_cast<float>(settings.bias_texels / resolution_));
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, textures_.at(index));
        glActiveTexture(GL_TEXTURE0);
    }

    void destroy() {
        if (!textures_.empty()) glDeleteTextures(static_cast<GLsizei>(textures_.size()), textures_.data());
        if (framebuffer_) glDeleteFramebuffers(1, &framebuffer_);
        textures_.clear();
        matrices_.clear();
        cache_.clear();
        framebuffer_ = 0;
        resolution_ = 0;
    }

private:
    GLuint framebuffer_ = 0;
    int resolution_ = 0;
    std::vector<GLuint> textures_;
    std::vector<glm::mat4> matrices_;
    std::vector<TerrainShadowCache> cache_;
};

} // namespace rendering

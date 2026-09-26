#pragma once

#include <algorithm>
#include <numeric>
#include <vector>
#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>
#include "simulation/Atmosphere.h"
#include "rendering/CelestialLighting.h"
#include "rendering/Shader.h"
#include "rendering/AtmosphereTransmittance.h"

namespace rendering {
inline bool hasAtmosphere(const config::ScenarioConfig& scene) {
    return std::any_of(scene.planets.begin(), scene.planets.end(), [](const auto& p) {
        return p.atmosphere.enabled && p.atmosphere.surface_pressure_pa > 0;
    });
}
inline void bindAtmosphere(const Shader& shader, const config::PlanetConfig& planet,
                           double metersPerUnit, const glm::dvec3& localSun) {
    shader.setInt("uAtmEnabled", planet.atmosphere.enabled && planet.atmosphere.surface_pressure_pa > 0);
    shader.setFloat("uAtmRefractivity", 0.0);
    if (!planet.atmosphere.enabled) return;
    const auto optics = simulation::atmosphereOptics(planet.atmosphere, planet.radius * metersPerUnit,
                                                   simulation::referenceAir(planet.atmosphere));
    auto rgb = [&](const char* name, const glm::dvec3& v) { shader.setFloat3(name, v.x, v.y, v.z); };
    shader.setFloat("uAtmOuter", planet.atmosphere.radius_multiplier);
    shader.setFloat("uAtmRefractivity", planet.atmosphere.refraction_enabled ? optics.refractiveIndex - 1 : 0);
    shader.setFloat2("uAtmHeights", optics.molecularScaleHeight, optics.aerosolScaleHeight);
    rgb("uAtmRayleigh", optics.rayleigh); rgb("uAtmScatter", optics.aerosolScattering);
    rgb("uAtmAbsorb", optics.aerosolAbsorption); rgb("uAtmSunDirection", localSun);
}

// Linear HDR targets are used only for scenes opting into the atmosphere block.
// Geometry depth/stencil survive composition for capture diagnostics and replay.
class AtmosphereRenderer {
public:
    explicit AtmosphereRenderer(int downsample = 4) : downsample_(downsample) {
        if (downsample < 1 || downsample > 4) throw std::invalid_argument("Invalid atmosphere downsample");
    }
    AtmosphereRenderer(const AtmosphereRenderer&) = delete;
    AtmosphereRenderer& operator=(const AtmosphereRenderer&) = delete;
    ~AtmosphereRenderer() { destroy(); }
    GLuint framebuffer() const { return framebuffers_[0]; }
    void presentCached(const Shader& shader) const {
        glBindFramebuffer(GL_FRAMEBUFFER,0); glViewport(0,0,width_,height_);
        glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE); glDisable(GL_BLEND); glDisable(GL_STENCIL_TEST);
        shader.use(); shader.setInt("uToneMap",1); shader.setInt("uSceneColor",0);
        shader.setFloat("uExposure",lastExposure_);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,colors_[lastSource_]);
        glBindVertexArray(vao_); glDrawArrays(GL_TRIANGLES,0,3); glBindVertexArray(0);
        glBindFramebuffer(GL_READ_FRAMEBUFFER,framebuffers_[0]); glBindFramebuffer(GL_DRAW_FRAMEBUFFER,0);
        glBlitFramebuffer(0,0,width_,height_,0,0,width_,height_,GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT,GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER,0); glBindTexture(GL_TEXTURE_2D,0);
        glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE);
    }
    void begin(int width, int height) {
        if (width <= 0 || height <= 0) throw std::invalid_argument("Invalid atmosphere viewport");
        if (width != width_ || height != height_) {
            destroy(); width_ = width; height_ = height;
            glGenVertexArrays(1, &vao_);
            glGenFramebuffers(3, framebuffers_);
            glGenTextures(3, colors_);
            glGenTextures(1, &depth_);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, depth_);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, width, height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
            textureSettings();
            for (int i = 0; i < 3; ++i) {
                glBindFramebuffer(GL_FRAMEBUFFER, framebuffers_[i]);
                glBindTexture(GL_TEXTURE_2D, colors_[i]);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
                textureSettings();
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colors_[i], 0);
                if (i == 0) glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depth_, 0);
                glDrawBuffer(GL_COLOR_ATTACHMENT0); glReadBuffer(GL_COLOR_ATTACHMENT0);
                if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                    throw std::runtime_error("Atmosphere framebuffer is incomplete");
            }
            if (downsample_>1) {
                lowWidth_=(width+downsample_-1)/downsample_; lowHeight_=(height+downsample_-1)/downsample_;
                glGenFramebuffers(1,&lowFramebuffer_); glBindFramebuffer(GL_FRAMEBUFFER,lowFramebuffer_);
                glGenTextures(2,lowColors_);
                for (int i=0;i<2;++i) {
                    glBindTexture(GL_TEXTURE_2D,lowColors_[i]);
                    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA16F,lowWidth_,lowHeight_,0,GL_RGBA,GL_FLOAT,nullptr);
                    textureSettings();
                    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0+i,GL_TEXTURE_2D,lowColors_[i],0);
                }
                const GLenum attachments[]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1};
                glDrawBuffers(2,attachments);
                if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)
                    throw std::runtime_error("Atmosphere integration framebuffer is incomplete");
            }
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffers_[0]);
    }

    void finish(const Shader& shader, const config::ScenarioConfig& scene,
                const std::vector<simulation::BodyState>& bodies, const FrameLighting& lighting,
                double exposure, const glm::mat4& view, const glm::mat4& projection,
                const glm::dvec3& eye, GLuint output = 0, bool toneMap = true,
                const AtmosphereTransmittance* columns = nullptr) {
        glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE);
        glDisable(GL_BLEND); glDisable(GL_STENCIL_TEST);
        glViewport(0, 0, width_, height_);
        shader.use(); shader.setInt("uToneMap", 0);
        shader.setInt("uSceneColor", 0); shader.setInt("uSceneDepth", 2);
        shader.setInt("uAtmScatteringBuffer",4); shader.setInt("uAtmTransferBuffer",5);
        shader.setInt("uAtmIntegrateOnly",0); shader.setInt("uAtmResolve",0);
        shader.setFloat("uExposure", exposure);
        const auto inverseProjection = glm::inverse(projection);
        shader.setMat4("uInverseProjection", glm::value_ptr(inverseProjection));
        shader.setMat4("uProjection", glm::value_ptr(projection));
        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, depth_);
        glBindVertexArray(vao_);
        std::vector<std::size_t> order(scene.planets.size());
        std::iota(order.begin(), order.end(), 0);
        std::stable_sort(order.begin(), order.end(), [&](auto a, auto b) {
            return glm::length(bodies[a + 1].position - eye) > glm::length(bodies[b + 1].position - eye);
        });
        int source = 0;
        for (auto i : order) {
            const auto& planet = scene.planets[i];
            if (!planet.atmosphere.enabled || planet.atmosphere.surface_pressure_pa == 0) continue;
            const auto worldToBody = glm::transpose(bodies[i + 1].orientation);
            shader.setInt("uAtmUseColumns", 0);
            if (columns) columns->bind(i, shader);
            bindAtmosphere(shader, planet, scene.metersPerWorldUnit(), worldToBody * lighting.planets[i].sunDirection);
            shader.setFloat("uAtmSunAngularRadius", std::asin(std::clamp(scene.sun.radius /
                std::max(scene.sun.radius, glm::length(bodies[0].position - bodies[i + 1].position)), 0.0, 1.0)));
            const glm::dvec3 eyeBody = worldToBody * (eye - bodies[i + 1].position) / planet.radius;
            shader.setFloat3("uEyeBody", eyeBody.x, eyeBody.y, eyeBody.z);
            const glm::mat4 cameraToBody(glm::mat3(worldToBody) * glm::transpose(glm::mat3(view)));
            shader.setMat4("uCameraToBody", glm::value_ptr(cameraToBody));
            shader.setFloat("uRadius", planet.radius);
            const auto sun = lighting.planets[i].sunlight;
            const auto indirect = lighting.planets[i].reflectedLight + glm::dvec3(scene.lighting.ambient_light);
            shader.setFloat3("uAtmSunlight", sun.x, sun.y, sun.z);
            shader.setFloat3("uAtmIndirect", indirect.x, indirect.y, indirect.z);
            const int destination = source == 1 ? 2 : 1;
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, colors_[source]);
            if (downsample_>1) {
                glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D,0);
                glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D,0);
                shader.setInt("uAtmIntegrateOnly",1); shader.setInt("uAtmResolve",0);
                glBindFramebuffer(GL_FRAMEBUFFER,lowFramebuffer_); glViewport(0,0,lowWidth_,lowHeight_);
                glDrawArrays(GL_TRIANGLES,0,3);
                shader.setInt("uAtmIntegrateOnly",0); shader.setInt("uAtmResolve",1);
                glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D,lowColors_[0]);
                glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D,lowColors_[1]);
            }
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffers_[destination]);
            glViewport(0,0,width_,height_);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            source = destination;
        }
        if (toneMap) {
            lastSource_=source; lastExposure_=exposure;
            glBindFramebuffer(GL_FRAMEBUFFER, output);
            shader.setInt("uToneMap", 1);
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, colors_[source]);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffers_[0]);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, output);
            glBlitFramebuffer(0, 0, width_, height_, 0, 0, width_, height_, GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, GL_NEAREST);
        } else {
            glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffers_[source]);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, output);
            glBlitFramebuffer(0, 0, width_, height_, 0, 0, width_, height_, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        }
        glBindVertexArray(0);
        glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D,0);
        glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D,0);
        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, output);
        glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE);
    }
private:
    GLuint framebuffers_[3]{}, colors_[3]{}, depth_ = 0, vao_ = 0;
    int width_ = 0, height_ = 0;
    int downsample_=4,lowWidth_=0,lowHeight_=0;
    int lastSource_=0;
    double lastExposure_=1;
    GLuint lowFramebuffer_=0,lowColors_[2]{};
    static void textureSettings() {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    void destroy() {
        glDeleteFramebuffers(3, framebuffers_); glDeleteTextures(3, colors_);
        glDeleteFramebuffers(1,&lowFramebuffer_); glDeleteTextures(2,lowColors_);
        lowFramebuffer_=lowColors_[0]=lowColors_[1]=0;
        if (depth_) glDeleteTextures(1, &depth_);
        if (vao_) glDeleteVertexArrays(1, &vao_);
        for (int i = 0; i < 3; ++i) framebuffers_[i] = colors_[i] = 0;
        depth_ = vao_ = 0; width_ = height_ = 0;
    }
};
} // namespace rendering

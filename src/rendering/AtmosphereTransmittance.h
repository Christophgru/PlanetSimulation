#pragma once
#include <vector>
#include <GL/glew.h>
#include "config/ScenarioConfig.h"
#include "rendering/Shader.h"

namespace rendering {
// Density-column table in planet radii, independent of camera, composition,
// pressure, exposure and Sun direction. Regenerate only when the shell changes.
class AtmosphereTransmittance {
public:
    AtmosphereTransmittance() : shader_("shaders/atmosphere.vert","shaders/atmosphere_columns.frag","shaders/atmosphere.glsl") {
        glGenFramebuffers(1,&framebuffer_); glGenVertexArrays(1,&vao_);
    }
    ~AtmosphereTransmittance() {
        clear(); glDeleteFramebuffers(1,&framebuffer_); glDeleteVertexArrays(1,&vao_); glDeleteProgram(shader_.id);
    }
    AtmosphereTransmittance(const AtmosphereTransmittance&) = delete;
    AtmosphereTransmittance& operator=(const AtmosphereTransmittance&) = delete;
    void ensure(const config::ScenarioConfig& scene) {
        if (entries_.size()!=scene.planets.size()) { clear(); entries_.resize(scene.planets.size()); }
        for (std::size_t i=0;i<entries_.size();++i) {
            const auto& config=scene.planets[i].atmosphere;
            auto& entry=entries_[i];
            if (!config.enabled || config.surface_pressure_pa==0) continue;
            if (entry.texture && entry.outer==config.radius_multiplier) continue;
            if (!entry.texture) glGenTextures(1,&entry.texture);
            entry.outer=config.radius_multiplier;
            glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D,entry.texture);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RG32F,256,128,0,GL_RG,GL_FLOAT,nullptr);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
            glBindTexture(GL_TEXTURE_2D,0); glActiveTexture(GL_TEXTURE0);
            glBindFramebuffer(GL_FRAMEBUFFER,framebuffer_);
            glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,entry.texture,0);
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)
                throw std::runtime_error("Atmosphere lookup framebuffer is incomplete");
            glViewport(0,0,256,128); glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND); glDisable(GL_STENCIL_TEST);
            shader_.use(); shader_.setFloat("uAtmOuter",entry.outer);
            shader_.setFloat2("uAtmHeights",0.25*(entry.outer-1),0.2*(entry.outer-1));
            glBindVertexArray(vao_); glDrawArrays(GL_TRIANGLES,0,3); glBindVertexArray(0);
        }
        glEnable(GL_DEPTH_TEST); glBindFramebuffer(GL_FRAMEBUFFER,0);
    }
    void bind(std::size_t index,const Shader& shader) const {
        shader.setInt("uAtmUseColumns",entries_.at(index).texture!=0);
        shader.setInt("uAtmColumns",3);
        glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D,entries_[index].texture); glActiveTexture(GL_TEXTURE0);
    }
private:
    struct Entry { GLuint texture=0; double outer=0; };
    std::vector<Entry> entries_;
    Shader shader_;
    GLuint framebuffer_=0,vao_=0;
    void clear() { for (auto& entry:entries_) if(entry.texture) glDeleteTextures(1,&entry.texture); entries_.clear(); }
};
} // namespace rendering

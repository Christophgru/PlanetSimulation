#pragma once
#include <cstddef>
#include <GL/glew.h>
#include "rendering/Shader.h"
#include "rendering/geometry/Mesh.h"
#include "rendering/foliage/GrassPlacement.h"

namespace rendering {
class GrassRenderer {
    struct Batch { GLuint vao=0, buffer=0; GLsizei count=0; };
    struct Patch {
        Batch near, far;
        glm::dvec3 eye{0};
        std::uint64_t revision=0;
        bool ready=false;
    };
    std::vector<Patch> patches_;
    static void upload(Batch& batch, const std::vector<GrassBlade>& blades) {
        if (!batch.vao) glGenVertexArrays(1,&batch.vao);
        if (!batch.buffer) glGenBuffers(1,&batch.buffer);
        glBindVertexArray(batch.vao);
        glBindBuffer(GL_ARRAY_BUFFER,batch.buffer);
        glBufferData(GL_ARRAY_BUFFER,blades.size()*sizeof(GrassBlade),blades.data(),GL_STATIC_DRAW);
        for (int location=0;location<3;++location) {
            const std::size_t offset = location==0 ? offsetof(GrassBlade,root) :
                location==1 ? offsetof(GrassBlade,up) : offsetof(GrassBlade,variation);
            glEnableVertexAttribArray(location);
            glVertexAttribPointer(location,location==2 ? 4 : 3,GL_FLOAT,GL_FALSE,sizeof(GrassBlade),reinterpret_cast<void*>(offset));
            glVertexAttribDivisor(location,1);
        }
        batch.count=static_cast<GLsizei>(blades.size());
        glBindVertexArray(0);
    }
public:
    Shader shader{"shaders/foliage/grass.vert","shaders/foliage/grass.frag",
                  "shaders/terrain/terrain_shadow.glsl","shaders/atmosphere/atmosphere.glsl"};
    GrassRenderer() = default;
    GrassRenderer(const GrassRenderer&) = delete;
    GrassRenderer& operator=(const GrassRenderer&) = delete;
    ~GrassRenderer() { clear(); glDeleteProgram(shader.id); }
    void clear() {
        for (auto& patch : patches_) for (auto* batch : {&patch.near,&patch.far}) {
            glDeleteVertexArrays(1,&batch->vao); glDeleteBuffers(1,&batch->buffer);
        }
        patches_.clear();
    }
    void prepare(std::size_t index,const Mesh& mesh,const config::PlanetConfig& planet,
                 double metersPerWorldUnit,const glm::dvec3& eyeBody) {
        if (patches_.size()<=index) patches_.resize(index+1);
        auto& patch=patches_[index];
        if (!planet.foliage.enabled || !mesh.hasVertexColors) {
            patch.near.count=patch.far.count=0; patch.ready=false; return;
        }
        const double scale=planet.radius*metersPerWorldUnit;
        const double margin=grassRebuildDistance(planet.foliage);
        if (patch.ready && patch.revision==mesh.revision && glm::length(eyeBody-patch.eye)*scale<margin) return;
        auto blades=placeGrass(mesh.vertices,mesh.indices,planet,metersPerWorldUnit,eyeBody);
        std::vector<GrassBlade> near,far;
        for (const auto& blade:blades) {
            // The margin guarantees that a low-detail blade cannot approach
            // within the detailed range before the next patch update.
            const double distance=glm::length(glm::dvec3(blade.root)-eyeBody)*scale;
            (distance<=15.0+margin ? near : far).push_back(blade);
        }
        // Front-to-back blades let depth testing reject the dense layers behind
        // them before running atmospheric/material shading.
        const auto nearer=[&](const GrassBlade& a,const GrassBlade& b) {
            const auto da=glm::dvec3(a.root)-eyeBody,db=glm::dvec3(b.root)-eyeBody;
            return glm::dot(da,da)<glm::dot(db,db);
        };
        std::sort(near.begin(),near.end(),nearer);
        std::sort(far.begin(),far.end(),nearer);
        upload(patch.near,near); upload(patch.far,far);
        patch.eye=eyeBody; patch.revision=mesh.revision; patch.ready=true;
    }
    std::size_t count(std::size_t index) const {
        return index<patches_.size() ? patches_[index].near.count+patches_[index].far.count : 0;
    }
    void draw(std::size_t index) const {
        if (index>=patches_.size()) return;
        const bool culled=glIsEnabled(GL_CULL_FACE);
        glDisable(GL_CULL_FACE); // Two-sided blades, without duplicate geometry.
        const auto& patch=patches_[index];
        for (auto [batch,segments] : {std::pair{&patch.near,6},std::pair{&patch.far,1}}) {
            if (!batch->count) continue;
            shader.setInt("uSegments",segments);
            glBindVertexArray(batch->vao);
            glDrawArraysInstanced(GL_TRIANGLE_STRIP,0,2*(segments+1),batch->count);
        }
        glBindVertexArray(0);
        if (culled) glEnable(GL_CULL_FACE);
    }
};
} // namespace rendering

#include "rendering/foliage/procedural/ProceduralGrass.h"

namespace rendering {
void ProceduralGrass::bindTrail(std::size_t index,double scale) const {
    shader.setInt("uTrailTree",10); shader.setInt("uTrailNodes",0);
    const auto found=trails_.find(index);
    if (found==trails_.end() || found->second.history.segments().empty()) return;
    const auto& data=found->second;
    if (data.uploadedRevision!=data.history.revision()) {
        // Relative metre coordinates keep the hierarchy accurate on large
        // planets. The existing terrain/root float precision still applies.
        data.origin=data.history.segments().back().end;
        const auto nodes=data.history.hierarchy(data.origin);
        if (!data.buffer) glGenBuffers(1,&data.buffer);
        if (!data.texture) glGenTextures(1,&data.texture);
        glBindBuffer(GL_TEXTURE_BUFFER,data.buffer);
        glBufferData(GL_TEXTURE_BUFFER,nodes.size()*sizeof(glm::vec4),nodes.data(),GL_DYNAMIC_DRAW);
        glActiveTexture(GL_TEXTURE10); glBindTexture(GL_TEXTURE_BUFFER,data.texture);
        glTexBuffer(GL_TEXTURE_BUFFER,GL_RGBA32F,data.buffer);
        data.nodes=nodes.size()/4; data.uploadedRevision=data.history.revision();
    }
    glActiveTexture(GL_TEXTURE10); glBindTexture(GL_TEXTURE_BUFFER,data.texture);
    glActiveTexture(GL_TEXTURE0);
    shader.setInt("uTrailTree",10); shader.setInt("uTrailNodes",data.nodes);
    const auto origin=data.origin/scale;
    shader.setFloat3("uTrailOrigin",origin.x,origin.y,origin.z);
}
}

#pragma once
#include "rendering/character/assets/AcesSuit.h"
#include <GL/glew.h>
namespace rendering {
class SkinnedSuit {
public:
    SkinnedSuit();
    ~SkinnedSuit();
    SkinnedSuit(const SkinnedSuit&)=delete;
    SkinnedSuit& operator=(const SkinnedSuit&)=delete;
    void draw() const;
    std::array<glm::mat4,AcesSuit::BoneCount> palette(const AstronautPose& pose) const { return bind_.palette(pose); }
    std::size_t triangleCount() const { return count_/3; }
private:
    AcesSuit bind_;
    GLuint vao_=0,vbo_=0,ebo_=0;
    GLsizei count_=0;
};
}

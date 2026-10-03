#pragma once
#include "rendering/character/effects/ExhaustParticles.h"
#include "rendering/Shader.h"
namespace rendering {
class ExhaustRenderer {
public:
    ExhaustRenderer();
    ~ExhaustRenderer();
    ExhaustRenderer(const ExhaustRenderer&)=delete;
    ExhaustRenderer& operator=(const ExhaustRenderer&)=delete;
    void draw(const ExhaustState&,const glm::dvec3& bodyCenter,const glm::dmat3& orientation,
        double radiusWorld,double metersPerUnit,const glm::mat4& view,const glm::mat4& projection);
    Shader shader;
private:
    GLuint vao_=0,buffer_=0;
};
}

#include "rendering/character/AstronautRenderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace rendering {
AstronautRenderer::AstronautRenderer()
    : shader("shaders/character/astronaut.vert","shaders/character/astronaut.frag",
             "shaders/terrain/terrain_shadow.glsl","shaders/atmosphere/atmosphere.glsl") {
    sphere_.generateSphere(12);
    for (auto p:{glm::vec3(0,-1,-1),glm::vec3(0,1,-1),glm::vec3(0,1,1),glm::vec3(0,-1,1)})
        flag_.addVertex(p.x,p.y,p.z,1,0,0);
    flag_.addTriangle(0,1,2); flag_.addTriangle(0,2,3); flag_.upload();
}
AstronautRenderer::~AstronautRenderer() { sphere_.destroy(); flag_.destroy(); glDeleteProgram(shader.id); }
void AstronautRenderer::draw(const glm::dvec3& planetCenter,const glm::dmat3& orientation,
                             double radiusWorld,double metersPerWorldUnit,
                             const glm::mat4& view,const glm::mat4& projection) {
    if (!motion.ready()) return;
    const auto& pose=motion.pose();
    const auto basis=pose.suitBasis();
    const glm::dmat4 actorWorld=glm::translate(glm::dmat4(1),planetCenter+orientation*pose.root/metersPerWorldUnit)*
        glm::dmat4(orientation*basis)*glm::scale(glm::dmat4(1),glm::dvec3(1/metersPerWorldUnit));
    const glm::dmat4 actorBody=glm::translate(glm::dmat4(1),pose.root/(radiusWorld*metersPerWorldUnit))*
        glm::dmat4(basis)*glm::scale(glm::dmat4(1),glm::dvec3(1/(radiusWorld*metersPerWorldUnit)));
    shader.use();
    shader.setMat4("projection",glm::value_ptr(projection));
    const auto palette=suit_.palette(pose);
    shader.setInt("uSkinned",1);
    glUniformMatrix4fv(glGetUniformLocation(shader.id,"uBones[0]"),palette.size(),GL_FALSE,glm::value_ptr(palette[0]));
    const glm::mat4 suitWorld=actorWorld,suitBody=actorBody,suitView=glm::dmat4(view)*actorWorld;
    shader.setMat4("uModelView",glm::value_ptr(suitView));
    shader.setMat4("model",glm::value_ptr(suitWorld));
    shader.setMat4("uBodyModel",glm::value_ptr(suitBody));
    shader.setFloat("uGlow",0);
    suit_.draw();
    shader.setInt("uSkinned",0);
    const glm::vec3 blue(.045,.23,.53), dark(.035,.06,.085);
    const auto part=[&](const glm::dvec3& center,const glm::dvec3& scale,const glm::vec3& color,
                        const glm::dmat3& rotation=glm::dmat3(1),float shine=0,float glow=0) {
        const auto local=glm::translate(glm::dmat4(1),center)*glm::dmat4(rotation)*
            glm::scale(glm::dmat4(1),scale);
        const glm::mat4 world=actorWorld*local,body=actorBody*local,modelView=glm::dmat4(view)*actorWorld*local;
        shader.setMat4("uModelView",glm::value_ptr(modelView));
        shader.setMat4("model",glm::value_ptr(world));
        shader.setMat4("uBodyModel",glm::value_ptr(body));
        shader.setFloat3("uColor",color.r,color.g,color.b);
        shader.setFloat("uShininess",shine);
        shader.setFloat("uGlow",glow);
        sphere_.draw();
    };
    const auto upperPart=[&](const glm::dvec3& center,const glm::dvec3& scale,const glm::vec3& color,
                             const glm::dmat3& rotation=glm::dmat3(1),float shine=0,float glow=0) {
        part(center+pose.bodyOffset,scale,color,rotation,shine,glow);
    };
    upperPart({0,1.12,.22},{.22,.26,.12},dark); // Backpack behind the actor.
    upperPart({-.23,1.1,.23},{.055,.23,.065},blue);
    upperPart({.23,1.1,.23},{.055,.23,.065},blue);
    // German upper-arm patches follow the same skin joint as the sleeve.
    for (int arm=0;arm<2;++arm) {
        const double sign=arm==0 ? -1 : 1;
        const glm::dmat4 attachment=palette[7+3*arm];
        const auto armY=glm::normalize(glm::dvec3(-sign*.2,1,0));
        const glm::dmat3 patchBasis(glm::normalize(glm::dvec3(sign,.2,0)),armY,glm::dvec3(0,0,sign));
        const glm::dvec3 patch(sign*.449,1.46,arm==0 ? .10 : .17);
        const glm::vec3 stripes[]={{.015,.015,.015},{.85,.025,.025},{.95,.66,.035}};
        for (int stripe=0;stripe<3;++stripe) {
            const auto local=attachment*glm::translate(glm::dmat4(1),patch+armY*(.03-.03*stripe))*
                glm::dmat4(patchBasis)*glm::scale(glm::dmat4(1),glm::dvec3(1,.015,.045));
            const glm::mat4 world=actorWorld*local,body=actorBody*local,modelView=glm::dmat4(view)*actorWorld*local;
            shader.setMat4("uModelView",glm::value_ptr(modelView));
            shader.setMat4("model",glm::value_ptr(world)); shader.setMat4("uBodyModel",glm::value_ptr(body));
            shader.setFloat3("uColor",stripes[stripe].r,stripes[stripe].g,stripes[stripe].b);
            shader.setFloat("uShininess",0); shader.setFloat("uGlow",0); flag_.draw();
        }
    }
    // Both nozzle positions also feed the persistent world-space emitter.
    upperPart({-.12,.82,.23},{.065,.07,.065},blue);
    upperPart({.12,.82,.23},{.065,.07,.065},blue);
}
}

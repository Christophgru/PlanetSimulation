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
    const auto inverse=glm::transpose(basis);
    const glm::dmat4 actorWorld=glm::translate(glm::dmat4(1),planetCenter+orientation*pose.root/metersPerWorldUnit)*
        glm::dmat4(orientation*basis)*glm::scale(glm::dmat4(1),glm::dvec3(1/metersPerWorldUnit));
    const glm::dmat4 actorBody=glm::translate(glm::dmat4(1),pose.root/(radiusWorld*metersPerWorldUnit))*
        glm::dmat4(basis)*glm::scale(glm::dmat4(1),glm::dvec3(1/(radiusWorld*metersPerWorldUnit)));
    shader.use();
    shader.setMat4("view",glm::value_ptr(view));
    shader.setMat4("projection",glm::value_ptr(projection));
    const glm::vec3 grey(.72,.77,.80), blue(.045,.23,.53), dark(.035,.06,.085), visor(.07,.19,.32);
    const auto part=[&](const glm::dvec3& center,const glm::dvec3& scale,const glm::vec3& color,
                        const glm::dmat3& rotation=glm::dmat3(1),float shine=0,float glow=0) {
        const auto local=glm::translate(glm::dmat4(1),center)*glm::dmat4(rotation)*
            glm::scale(glm::dmat4(1),scale);
        const glm::mat4 world=actorWorld*local,body=actorBody*local;
        shader.setMat4("model",glm::value_ptr(world));
        shader.setMat4("uBodyModel",glm::value_ptr(body));
        shader.setFloat3("uColor",color.r,color.g,color.b);
        shader.setFloat("uShininess",shine);
        shader.setFloat("uGlow",glow);
        sphere_.draw();
    };
    const auto limb=[&](const glm::dvec3& a,const glm::dvec3& b,double width,const glm::vec3& color) {
        const auto span=b-a;
        const auto y=glm::normalize(span);
        const auto x=glm::normalize(glm::cross(y,std::abs(y.y)<.9 ? glm::dvec3(0,1,0) : glm::dvec3(1,0,0)));
        const glm::dmat3 rotation(x,y,glm::cross(x,y));
        part((a+b)*.5,{width,glm::length(span)*.5+width*.35,width},color,rotation);
    };
    part({0,1.01,0},{.28,.30,.19},grey); // Suit torso.
    part({0,.76,0},{.25,.15,.18},blue);
    part({0,1.12,.22},{.22,.26,.12},dark); // Backpack behind the actor.
    part({-.23,1.1,.23},{.055,.23,.065},blue);
    part({.23,1.1,.23},{.055,.23,.065},blue);
    part({0,1.02,-.18},{.14,.13,.055},blue); // Chest control panel.
    part({-.045,1.06,-.225},{.035,.035,.012},dark);
    part({.045,1.06,-.225},{.022,.022,.012},grey);
    part({0,1.30,0},{.22,.06,.20},dark); // Neck seal.
    part({0,1.48,0},{.34,.32,.32},grey);
    part({0,1.49,-.23},{.285,.215,.145},dark); // Visor seal.
    part({0,1.49,-.263},{.255,.183,.13},visor,glm::dmat3(1),1);
    part({-.095,1.565,-.370},{.08,.025,.009},grey); // Comic glint.
    part({-.33,1.46,0},{.04,.095,.095},blue);
    part({.33,1.46,0},{.04,.095,.095},blue);
    for (int leg=0;leg<2;++leg) {
        const auto hip=inverse*(pose.hips[leg]-pose.root);
        const auto knee=inverse*(pose.knees[leg]-pose.root);
        const auto ankle=inverse*(pose.ankles[leg]-pose.root);
        limb(hip,knee,.105,grey); limb(knee,ankle,.085,grey);
        part(knee,{.115,.085,.105},blue);
        const auto& foot=pose.feet[leg];
        const auto up=foot.contact.normal;
        auto forward=foot.forward-up*glm::dot(foot.forward,up);
        if (glm::length(forward)<1e-8) forward=pose.forward;
        forward=glm::normalize(forward);
        const glm::dmat3 footBasis(glm::normalize(glm::cross(forward,up)),up,-forward);
        const auto bootCenter=inverse*(foot.contact.position+up*.09-pose.root);
        part(bootCenter,{.12,.09,.21},dark,inverse*footBasis);
        const double sign=leg==0 ? -1 : 1;
        const glm::dvec3 shoulder(sign*.28,1.16,0);
        const glm::dvec3 elbow(sign*.36,.93,sign*pose.armSwing);
        const glm::dvec3 hand(sign*.34,.72,-.04+sign*pose.armSwing);
        part(shoulder,{.13,.13,.13},blue);
        limb(shoulder,elbow,.095,grey); limb(elbow,hand,.08,grey);
        part(hand,{.095,.11,.095},blue);
        // Flat embroidered upper-arm patch, black/red/gold from top to bottom.
        const auto armY=glm::normalize(shoulder-elbow);
        const auto armZ=glm::normalize(glm::cross(glm::dvec3(sign,0,0),armY));
        const glm::dmat3 armBasis(glm::cross(armY,armZ),armY,armZ);
        const auto patch=(shoulder+elbow)*.5+armBasis[0]*.098;
        const glm::vec3 stripes[]={{.015,.015,.015},{.85,.025,.025},{.95,.66,.035}};
        for (int stripe=0;stripe<3;++stripe) {
            const auto local=glm::translate(glm::dmat4(1),patch+armY*(.03-.03*stripe))*
                glm::dmat4(armBasis)*glm::scale(glm::dmat4(1),glm::dvec3(1,.015,.075));
            const glm::mat4 world=actorWorld*local,body=actorBody*local;
            shader.setMat4("model",glm::value_ptr(world)); shader.setMat4("uBodyModel",glm::value_ptr(body));
            shader.setFloat3("uColor",stripes[stripe].r,stripes[stripe].g,stripes[stripe].b);
            shader.setFloat("uShininess",0); shader.setFloat("uGlow",0); flag_.draw();
        }
    }
    // Two nozzles beneath the backpack. Bubbles are procedural, bounded and
    // use the same local animation phase in main and reflected views.
    part({-.12,.82,.23},{.065,.07,.065},blue);
    part({.12,.82,.23},{.065,.07,.065},blue);
    if (pose.boosting) {
        const bool blended=glIsEnabled(GL_BLEND);
        GLboolean writes; glGetBooleanv(GL_DEPTH_WRITEMASK,&writes);
        GLint srcRgb,dstRgb,srcAlpha,dstAlpha;
        glGetIntegerv(GL_BLEND_SRC_RGB,&srcRgb); glGetIntegerv(GL_BLEND_DST_RGB,&dstRgb);
        glGetIntegerv(GL_BLEND_SRC_ALPHA,&srcAlpha); glGetIntegerv(GL_BLEND_DST_ALPHA,&dstAlpha);
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA); glDepthMask(GL_FALSE);
        for (int bubble=0;bubble<32;++bubble) {
            const double age=std::fmod(pose.effectSeconds*1.6+bubble*.61803398875,1.0);
            const double side=bubble%2 ? -.12 : .12;
            const double size=.04+.09*age;
            const glm::dvec3 p(side+std::sin(bubble*2.4+age*4)*age*.13,
                               .76-age*2.1,.23+std::cos(bubble*1.8)*age*.12);
            part(p,glm::dvec3(size),glm::vec3(.14,.65,1),glm::dmat3(1),.5,2);
        }
        glDepthMask(writes); glBlendFuncSeparate(srcRgb,dstRgb,srcAlpha,dstAlpha);
        if (!blended) glDisable(GL_BLEND);
    }
}
}

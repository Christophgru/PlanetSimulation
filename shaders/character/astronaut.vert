#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec4 aSuitColor;
layout(location=3) in uvec2 aJoints;
layout(location=4) in vec2 aWeights;
uniform bool uSkinned;
uniform mat4 uBones[13];
uniform vec3 uColor;
uniform float uShininess;
out vec4 vSuitColor;
uniform mat4 model, uModelView, projection, uBodyModel, uShadowMatrix;
out vec3 vWorldPosition, vBodyPosition, vWorldNormal;
out vec4 vShadowPosition;
void main() {
    mat4 skin=mat4(1);
    if (uSkinned) skin=uBones[aJoints.x]*aWeights.x+uBones[aJoints.y]*aWeights.y;
    vec4 position=skin*vec4(aPos,1);
    vec4 world=model*position;
    vSuitColor=uSkinned ? aSuitColor : vec4(uColor,uShininess);
    // Precompose view/model in CPU double precision: preserve millimetre suit detail.
    gl_Position=projection*uModelView*position;
    vWorldPosition=world.xyz;
    vBodyPosition=(uBodyModel*position).xyz;
    vWorldNormal=normalize(mat3(transpose(inverse(model)))*mat3(skin)*aNormal);
    vShadowPosition=uShadowMatrix*vec4(vBodyPosition,1);
}

#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
uniform mat4 model, view, projection, uBodyModel, uShadowMatrix;
out vec3 vWorldPosition, vBodyPosition, vWorldNormal;
out vec4 vShadowPosition;
void main() {
    vec4 world=model*vec4(aPos,1);
    gl_Position=projection*view*world;
    vWorldPosition=world.xyz;
    vBodyPosition=(uBodyModel*vec4(aPos,1)).xyz;
    vWorldNormal=normalize(mat3(transpose(inverse(model)))*aNormal);
    vShadowPosition=uShadowMatrix*vec4(vBodyPosition,1);
}

#version 330 core
layout(location=0) in vec4 aCenterRadius;
layout(location=1) in vec4 aBodyOpacity;
uniform mat4 uCameraProjection,uBodyRotation,uShadowMatrix;
uniform vec3 uCameraRight,uCameraUp,uEyeWorld;
uniform float uReferenceRadius;
out vec2 vDisc;
out vec3 vWorldPosition,vBodyPosition;
out vec4 vShadowPosition;
out float vOpacity;
void main() {
    vDisc=vec2((gl_VertexID&1)==0 ? -1 : 1,gl_VertexID<2 ? -1 : 1);
    vec3 offset=(uCameraRight*vDisc.x+uCameraUp*vDisc.y)*aCenterRadius.w;
    vec3 relative=aCenterRadius.xyz+offset;
    gl_Position=uCameraProjection*vec4(relative,1);
    vWorldPosition=uEyeWorld+relative;
    vBodyPosition=aBodyOpacity.xyz+(uBodyRotation*vec4(offset,0)).xyz/uReferenceRadius;
    vShadowPosition=uShadowMatrix*vec4(vBodyPosition,1);
    vOpacity=aBodyOpacity.w;
}

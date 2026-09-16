#version 330 core

layout(location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 uReflectionViewProjection;

out vec3 vWorldPosition;
out vec4 vReflectionClip;

void main() {
    vec4 world = model * vec4(aPos, 1.0);
    vWorldPosition = world.xyz;
    vReflectionClip = uReflectionViewProjection * world;
    gl_Position = projection * view * world;
}

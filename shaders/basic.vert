#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 vPos;
out vec3 vNormal;
out vec3 vColor;

void main() {
    vec4 worldPos = model * vec4(aPos, 1.0);
    vPos = worldPos.xyz;
    vNormal = aNormal;
    vColor = vec3(1.0); // Default color, can be set via uniform if needed
    gl_Position = projection * view * worldPos;
}

#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec3 uColor;
uniform mat4 uShadowMatrix;

out vec3 vColor;
out vec3 vWorldPosition;
out vec3 vWorldNormal;
out vec4 vShadowPosition;

void main() {
    vec4 worldPos = model * vec4(aPos, 1.0);
    gl_Position = projection * view * worldPos;
    vWorldPosition = worldPos.xyz;
    vWorldNormal = normalize(mat3(transpose(inverse(model))) * aNormal);
    vColor = uColor * aColor;
    vShadowPosition = uShadowMatrix * vec4(aPos, 1.0);
}

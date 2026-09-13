#version 330 core

in vec3 vPos;
in vec3 vNormal;
in vec3 vColor;

out vec4 fColor;

uniform vec3 uSunColor;

void main() {
    // Use vertex color or sun color
    fColor = vec4(vColor, 1.0);
}

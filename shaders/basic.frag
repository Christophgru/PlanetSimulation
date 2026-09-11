#version 330 core

out vec4 fColor;

uniform vec3 uSunColor;

void main() {
    fColor = vec4(uSunColor, 1.0);
}

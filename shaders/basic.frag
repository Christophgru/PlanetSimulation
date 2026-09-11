#version 330 core

layout(location = 0) in vec3 aPos;

out vec4 fColor;

uniform vec3 uSunColor;

void main() {
    fColor = vec4(uSunColor, 1.0);
}

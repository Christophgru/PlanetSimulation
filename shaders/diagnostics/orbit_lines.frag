#version 330 core
uniform vec3 uColor;
in float vOpacity;
out vec4 fColor;
void main() { fColor = vec4(uColor, vOpacity); }

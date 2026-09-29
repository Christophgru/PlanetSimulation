#version 330 core
in vec2 vUv;
out vec4 color;
uniform sampler2D uScene;
void main() { color = texture(uScene, vUv); }

#version 330 core

in vec3 aPos;
in vec3 aNormal;
in vec2 aTexCoord;

out vec4 FragColor;

uniform vec3 color;

void main() {
    FragColor = vec4(color, 1.0);
}
#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

out vec3 FragColor;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec3 color;

void main() {
    // Simple solid color rendering
    FragColor = color;
}

#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in float aOpacity;
uniform mat4 uViewProjection;
out float vOpacity;
void main() {
    vOpacity = aOpacity;
    gl_Position = uViewProjection * vec4(aPosition, 1.0);
}

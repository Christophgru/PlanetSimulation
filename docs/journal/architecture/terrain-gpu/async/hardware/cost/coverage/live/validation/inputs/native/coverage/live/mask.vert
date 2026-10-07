#version 330 core
layout(location=0) in vec3 position;
uniform mat4 model,view,projection;
void main() {
    vec4 world=model*vec4(position,1);
    gl_Position=projection*view*world;
}

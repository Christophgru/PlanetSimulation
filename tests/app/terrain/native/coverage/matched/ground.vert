#version 330 core
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec3 color;
uniform mat4 model,view,projection;
uniform vec3 rootBody;
uniform float scale;
out vec3 offsetMeters,bodyNormal,bodyColor;
void main() {
    gl_Position=projection*view*model*vec4(position,1);
    // Subtract before interpolation to avoid cancellation in pixel derivatives.
    offsetMeters=(position-rootBody)*scale;
    bodyNormal=normal; bodyColor=color;
}

#version 330 core
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec3 color;
uniform mat4 model,view,projection;
uniform vec3 rootBody;
uniform float scale;
out vec3 offsetMeters,bodyNormal,bodyColor;
void main() {
    // Match basic.vert's operation order for the same depth/silhouette grid.
    vec4 world=model*vec4(position,1);
    gl_Position=projection*view*world;
    offsetMeters=(position-rootBody)*scale;
    bodyNormal=normal;bodyColor=color;
}

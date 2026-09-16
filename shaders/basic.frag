#version 330 core

in vec3 vColor;
in vec3 vWorldPosition;

uniform vec3 uClipCenter;
uniform float uClipRadius;

out vec4 fColor;

void main() {
    if (uClipRadius > 0.0 && distance(vWorldPosition, uClipCenter) < uClipRadius)
        discard;
    fColor = vec4(vColor, 1.0);
}

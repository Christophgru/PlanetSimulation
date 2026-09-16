#version 330 core

in vec3 vColor;
in vec3 vWorldPosition;
in vec3 vWorldNormal;

uniform vec3 uClipCenter;
uniform float uClipRadius;
uniform vec3 uSunPosition;
uniform float uEmissive;

out vec4 fColor;

void main() {
    if (uClipRadius > 0.0 && distance(vWorldPosition, uClipCenter) < uClipRadius)
        discard;
    vec3 toSun = normalize(uSunPosition - vWorldPosition);
    float diffuse = max(dot(normalize(vWorldNormal), toSun), 0.0);
    float brightness = mix(0.35 + 0.65 * diffuse, 1.0,
                           clamp(uEmissive, 0.0, 1.0));
    fColor = vec4(vColor * brightness, 1.0);
}

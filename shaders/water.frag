#version 330 core

in vec3 vWorldPosition;
in vec4 vReflectionClip;
out vec4 fColor;

uniform vec3 uPlanetCenter;
uniform vec3 uCameraPosition;
uniform vec3 uWaterColor;
uniform float uOpacity;
uniform float uReflectionFraction;
uniform sampler2D uReflectionTexture;

void main() {
    vec3 normal = normalize(vWorldPosition - uPlanetCenter);
    vec3 towardEye = normalize(uCameraPosition - vWorldPosition);
    // Draw only the near surface of the shell, even when its mesh has no culling.
    if (dot(normal, towardEye) <= 0.0) discard;

    vec2 reflectionUv = 0.5 * (vReflectionClip.xy / vReflectionClip.w) + 0.5;
    vec3 reflection = texture(uReflectionTexture,
                              clamp(reflectionUv, vec2(0.0), vec2(1.0))).rgb;
    vec3 surface = mix(uWaterColor, reflection, uReflectionFraction);
    fColor = vec4(surface, uOpacity);
}

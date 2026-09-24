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
uniform vec3 uSunDirection;
uniform vec3 uSunlight;
uniform vec3 uIndirectLight;
uniform float uExposure;

vec3 displayColor(vec3 radiance) {
    vec3 mapped = vec3(1.0) - exp(-max(radiance, vec3(0.0)) * uExposure);
    return mix(1.055 * pow(mapped, vec3(1.0 / 2.4)) - 0.055,
               12.92 * mapped, lessThanEqual(mapped, vec3(0.0031308)));
}

void main() {
    vec3 normal = normalize(vWorldPosition - uPlanetCenter);
    vec3 towardEye = normalize(uCameraPosition - vWorldPosition);
    // Draw only the near surface of the shell, even when its mesh has no culling.
    if (dot(normal, towardEye) <= 0.0) discard;

    vec2 reflectionUv = 0.5 * (vReflectionClip.xy / vReflectionClip.w) + 0.5;
    vec3 reflection = texture(uReflectionTexture,
                              clamp(reflectionUv, vec2(0.0), vec2(1.0))).rgb;
    float diffuse = max(dot(normal, uSunDirection), 0.0);
    vec3 litWater = displayColor(uWaterColor * (uIndirectLight + uSunlight * diffuse));
    // The reflection pass is already in display space; do not expose it twice.
    vec3 surface = mix(litWater, reflection, uReflectionFraction);
    fColor = vec4(surface, uOpacity);
}

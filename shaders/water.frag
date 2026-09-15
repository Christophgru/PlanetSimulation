#version 330 core

in vec3 vWorldPosition;
out vec4 fColor;

uniform vec3 uPlanetCenter;
uniform vec3 uCameraPosition;
uniform vec3 uSunPosition;
uniform vec3 uSunColor;
uniform vec3 uWaterColor;
uniform float uOpacity;
uniform float uReflectionFraction;

void main() {
    vec3 normal = normalize(vWorldPosition - uPlanetCenter);
    vec3 towardEye = normalize(uCameraPosition - vWorldPosition);
    // Draw only the near surface of the shell, even when its mesh has no culling.
    if (dot(normal, towardEye) <= 0.0) discard;

    vec3 reflectedRay = reflect(-towardEye, normal);
    vec3 sky = mix(vec3(0.10, 0.13, 0.20), vec3(0.30, 0.46, 0.64),
                   clamp(0.5 + 0.5 * reflectedRay.z, 0.0, 1.0));
    vec3 sunDirection = normalize(uSunPosition - vWorldPosition);
    float sunGlint = pow(max(dot(reflectedRay, sunDirection), 0.0), 256.0);
    vec3 reflection = clamp(sky + uSunColor * sunGlint, 0.0, 1.0);
    vec3 surface = mix(uWaterColor, reflection, uReflectionFraction);
    fColor = vec4(surface, uOpacity);
}

#version 330 core

in vec3 vColor;
in vec3 vWorldPosition;
in vec3 vWorldNormal;

uniform vec3 uClipCenter;
uniform float uClipRadius;
uniform vec3 uSunDirection;
uniform vec3 uSunlight;
uniform vec3 uIndirectLight;
uniform vec3 uEmission;
uniform float uExposure;
uniform float uEmissive;

out vec4 fColor;

vec3 displayColor(vec3 radiance) {
    vec3 mapped = vec3(1.0) - exp(-max(radiance, vec3(0.0)) * uExposure);
    return mix(1.055 * pow(mapped, vec3(1.0 / 2.4)) - 0.055,
               12.92 * mapped, lessThanEqual(mapped, vec3(0.0031308)));
}

void main() {
    if (uClipRadius > 0.0 && distance(vWorldPosition, uClipCenter) < uClipRadius)
        discard;
    vec3 radiance = uEmission;
    if (uEmissive < 0.5) {
        float diffuse = max(dot(normalize(vWorldNormal), uSunDirection), 0.0);
        radiance = vColor * (uIndirectLight + uSunlight * diffuse * sunlightVisibility(diffuse));
    }
    fColor = vec4(displayColor(radiance), 1.0);
}

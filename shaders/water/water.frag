#version 330 core

in vec3 vWorldPosition;
in vec3 vBodyPosition;
uniform bool uLinearOutput;
uniform float uAtmosphereRadiusScale;
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
    float diffuse = max(dot(normal, uSunDirection), 0.0);
    float visibility = diffuse > 0.0 ? sunlightVisibility(diffuse) : 0.0;
    bool validReflection = vReflectionClip.w > 0.0 &&
        all(greaterThanEqual(reflectionUv, vec2(0.0))) &&
        all(lessThanEqual(reflectionUv, vec2(1.0)));
    vec3 reflection = validReflection ? texture(uReflectionTexture, reflectionUv).rgb : vec3(0.0);
    vec3 sunlight = uSunlight, indirect = uIndirectLight;
#ifdef PLANET_ATMOSPHERE
    vec3 bodyPosition = vBodyPosition * uAtmosphereRadiusScale;
    sunlight *= atmosphereSunTransmittance(bodyPosition);
    indirect *= atmosphereLightTransmittance(bodyPosition, normalize(bodyPosition));
#endif
    vec3 radiance = uWaterColor * (indirect + sunlight * diffuse * visibility);
    vec3 litWater = uLinearOutput ? radiance : displayColor(radiance);
    // Reflection and water share linear HDR or legacy display space for this frame.
    // A reflected day scene can otherwise paint the planet's night ocean.
    // Keep the indirect water term even when the Sun is behind the horizon.
    float daySide = smoothstep(0.0, 0.03, diffuse) * visibility;
    vec3 surface = mix(litWater, reflection,
                       validReflection ? uReflectionFraction * daySide : 0.0);
    fColor = vec4(surface, uOpacity);
}

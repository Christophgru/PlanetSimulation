#version 330 core

in vec3 vColor;
in vec3 vWorldPosition;
in vec3 vBodyPosition;
uniform bool uLinearOutput;
in vec3 vWorldNormal;
in vec3 vBodyNormal;

uniform vec3 uClipCenter;
uniform float uClipRadius;
uniform vec3 uSunDirection;
uniform vec3 uSunlight;
uniform vec3 uIndirectLight;
uniform vec3 uEmission;
uniform float uExposure;
uniform float uEmissive;
uniform float uTerrainMetersPerRadius;
uniform vec3 uTerrainEyeBody;
uniform mat4 model;

out vec4 fColor;

// A planet-fixed value texture avoids seams and extra terrain vertices. Fade
// its shortest wavelengths before they become smaller than a screen pixel.
float terrainHash(vec3 p) {
    p = fract(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}
float terrainNoise(vec3 p) {
    vec3 cell = floor(p), t = fract(p);
    t = t * t * (3.0 - 2.0 * t);
    return mix(
        mix(mix(terrainHash(cell), terrainHash(cell + vec3(1,0,0)), t.x),
            mix(terrainHash(cell + vec3(0,1,0)), terrainHash(cell + vec3(1,1,0)), t.x), t.y),
        mix(mix(terrainHash(cell + vec3(0,0,1)), terrainHash(cell + vec3(1,0,1)), t.x),
            mix(terrainHash(cell + vec3(0,1,1)), terrainHash(cell + vec3(1,1,1)), t.x), t.y), t.z);
}

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
        vec3 normal = normalize(vWorldNormal);
        vec3 albedo = vColor;
        float roughness = 0.0;
        if (uTerrainMetersPerRadius > 0.0) {
            vec3 p = vBodyPosition * uTerrainMetersPerRadius;
            vec3 bodyNormal = normalize(vBodyNormal);
            float rock = smoothstep(0.025, 0.16,
                1.0 - dot(bodyNormal, normalize(vBodyPosition)));
            float footprint = max(max(fwidth(p.x), fwidth(p.y)), fwidth(p.z));
            float coarseVisibility = 1.0 - smoothstep(0.35, 1.0, footprint * 0.45);
            float coarse = mix(0.5, terrainNoise(p * 0.45), coarseVisibility);
            float fineVisibility = 1.0 - smoothstep(0.35, 1.0,
                footprint * 1.8);
            float fine = mix(0.5, terrainNoise(p * 1.8), fineVisibility);
            float reliefMeters = (coarse - 0.5) * mix(0.025, 0.09, rock) +
                                 (fine - 0.5) * 0.012;
            // Screen derivatives turn sub-polygon relief into a normal map.
            // The geometry and depth stay stable while light reveals grit.
            // Use body-local metres and perturb the smooth sampled normal;
            // rebuilding a normal from screen triangles exposes mesh seams.
            vec3 dx = dFdx(p), dy = dFdy(p);
            vec3 rx = cross(dy, bodyNormal), ry = cross(bodyNormal, dx);
            float determinant = dot(dx, rx);
            if (abs(determinant) > 1e-12) {
                vec3 gradient = (dFdx(reliefMeters) * rx + dFdy(reliefMeters) * ry) / determinant;
                normal = normalize(mat3(model) * normalize(bodyNormal - gradient));
            }
            float grain = (coarse - 0.5) * 0.30 + (fine - 0.5) * 0.15;
            albedo *= 1.0 + grain;
            float luminance = dot(albedo, vec3(0.2126, 0.7152, 0.0722));
            albedo = mix(albedo, vec3(luminance * 0.84), rock);
            roughness = mix(0.55, 0.9, rock) + (fine - 0.5) * 0.1;
        }
        float diffuse = max(dot(normal, uSunDirection), 0.0);
        float visibility = sunlightVisibility(max(dot(normalize(vWorldNormal), uSunDirection), 0.0));
        if (uTerrainMetersPerRadius > 0.0) {
            // Rough diffuse reflection broadens the response of matte grit.
            vec3 viewDirection = normalize(mat3(model) * (uTerrainEyeBody - vBodyPosition));
            float nv = max(dot(normal, viewDirection), 0.0);
            float variance = roughness * roughness;
            float a = 1.0 - 0.5 * variance / (variance + 0.33);
            float b = 0.45 * variance / (variance + 0.09);
            float correlation = max(0.0, dot(uSunDirection, viewDirection) - diffuse * nv);
            diffuse *= a + b * correlation / max(max(diffuse, nv), 0.001);
            // Small bumps must not illuminate the macro surface's night side.
            diffuse *= smoothstep(0.0, 0.05, dot(normalize(vWorldNormal), uSunDirection));
        }
        vec3 sunlight = uSunlight, indirect = uIndirectLight;
#ifdef PLANET_ATMOSPHERE
        sunlight *= atmosphereSunTransmittance(vBodyPosition);
        indirect *= atmosphereLightTransmittance(vBodyPosition, normalize(vBodyPosition));
#endif
        radiance = albedo * (indirect + sunlight * diffuse * visibility);
    }
    fColor = vec4(uLinearOutput ? radiance : displayColor(radiance), 1.0);
}

#version 330 core

in vec3 vDirection;

uniform int uSeed;
uniform float uStarDensity;
uniform float uStarScale;
uniform float uStarBrightness;
uniform vec3 uBackgroundColor;
uniform vec3 uStarColor;
uniform float uSkySensitivity;

out vec4 fColor;

uint starHash(ivec3 cell, uint seed) {
    uvec3 value = uvec3(cell);
    uint hash = seed ^ 0x9e3779b9u;
    hash ^= value.x * 0x85ebca6bu;
    hash ^= value.y * 0xc2b2ae35u;
    hash ^= value.z * 0x27d4eb2fu;
    hash ^= hash >> 16u;
    hash *= 0x7feb352du;
    hash ^= hash >> 15u;
    hash *= 0x846ca68bu;
    hash ^= hash >> 16u;
    return hash;
}

float hashUnit(uint hash) {
    return float(hash >> 8u) / 16777215.0;
}

void main() {
    ivec3 cell = ivec3(floor(normalize(vDirection) * uStarScale));
    uint primary = starHash(cell, uint(uSeed));
    float visible = step(1.0 - uStarDensity, hashUnit(primary));
    float magnitude = mix(0.35, 1.0,
        hashUnit(starHash(cell, uint(uSeed) ^ 0xa511e9b3u)));
    vec3 color = uBackgroundColor +
                 visible * magnitude * uStarBrightness * uStarColor;
    fColor = vec4(color * uSkySensitivity, 1.0);
}

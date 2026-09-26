#version 330 core
in vec2 vUv;
layout(location = 0) out vec4 fColor;
layout(location = 1) out vec4 fTransfer;
uniform bool uAtmIntegrateOnly;
uniform bool uAtmResolve;
uniform sampler2D uAtmScatteringBuffer;
uniform sampler2D uAtmTransferBuffer;
uniform sampler2D uSceneColor;
uniform sampler2D uSceneDepth;
uniform mat4 uInverseProjection;
uniform mat4 uProjection;
uniform mat4 uCameraToBody;
uniform vec3 uEyeBody;
uniform vec3 uAtmSunlight;
uniform vec3 uAtmIndirect;
uniform float uRadius;
uniform float uExposure;
uniform bool uToneMap;

vec3 displayColor(vec3 radiance) {
    vec3 mapped = vec3(1.0) - exp(-max(radiance, vec3(0.0)) * uExposure);
    return mix(1.055 * pow(mapped, vec3(1.0 / 2.4)) - 0.055,
               12.92 * mapped, lessThanEqual(mapped, vec3(0.0031308)));
}
float sceneDistance(vec2 uv) {
    float depth = texture(uSceneDepth, uv).r;
    if (depth >= 1.0) return 1e20;
    vec4 point = uInverseProjection * vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    return length(point.xyz / point.w) / uRadius;
}
// Depth-aware upsampling keeps terrain and Sun edges at full resolution.
// If none of the coarse samples describes this surface, integrate that pixel.
bool resolveAtmosphere(vec3 original,vec3 direction) {
    ivec2 size = textureSize(uAtmScatteringBuffer,0);
    vec2 grid = vUv*vec2(size)-0.5, fraction = fract(grid);
    ivec2 base = ivec2(floor(grid));
    float targetDistance = sceneDistance(vUv), sum = 0.0;
    vec4 scatter = vec4(0.0), transfer = vec4(0.0);
    for (int y=0;y<2;++y) for (int x=0;x<2;++x) {
        ivec2 cell = clamp(base+ivec2(x,y),ivec2(0),size-1);
        vec2 uv = (vec2(cell)+0.5)/vec2(size);
        float distance = sceneDistance(uv);
        float agreement = exp(-abs(distance-targetDistance)/max(0.02*targetDistance,0.0001));
        float weight = (x==0 ? 1.0-fraction.x : fraction.x)*(y==0 ? 1.0-fraction.y : fraction.y)*agreement;
        scatter += texelFetch(uAtmScatteringBuffer,cell,0)*weight;
        transfer += texelFetch(uAtmTransferBuffer,cell,0)*weight;
        sum += weight;
    }
    if (sum < 0.05) return false;
    scatter /= sum; transfer /= sum;
    vec2 uv = vUv+vec2(scatter.a,transfer.a);
    float exitDistance = atmosphereSphere(uEyeBody,direction,uAtmOuter).y;
    if (targetDistance>exitDistance+0.01 && all(greaterThanEqual(uv,vec2(0.0))) && all(lessThanEqual(uv,vec2(1.0))) &&
        sceneDistance(uv)>exitDistance+0.01)
        original=texture(uSceneColor,uv).rgb;
    fColor = vec4(original*transfer.rgb+scatter.rgb,1.0);
    return true;
}
void main() {
    vec3 original = texture(uSceneColor, vUv).rgb;
    if (uToneMap) { fColor = vec4(displayColor(original), 1.0); return; }
    vec4 eyePoint = uInverseProjection * vec4(vUv * 2.0 - 1.0, 1.0, 1.0);
    vec3 direction = normalize(mat3(uCameraToBody) * (eyePoint.xyz / eyePoint.w));
    if (uAtmResolve && resolveAtmosphere(original,direction)) return;
    vec2 segment = atmosphereSphere(uEyeBody, direction, uAtmOuter);
    segment.x = max(0.0, segment.x);
    float distance = sceneDistance(vUv);
    bool distant = distance > segment.y + 0.01;
    bool bending = uAtmRefractivity > 0.0;
    segment.y = min(segment.y, distance);
    vec2 ground = atmosphereSphere(uEyeBody, direction, 1.0);
    if (ground.x > 0.0) segment.y = min(segment.y, ground.x);
    if (segment.y <= segment.x) {
        fColor = uAtmIntegrateOnly ? vec4(0.0) : vec4(original,1.0);
        fTransfer = vec4(1.0,1.0,1.0,0.0); return;
    }
    int steps = bending ? 48 : 24;
    // A curved ray can travel farther than the straight chord. Bound the work;
    // rays still inside at the limit are treated as trapped, not leaked to sky.
    float pathScale = bending && distant ? 1.05 + min(uAtmRefractivity / uAtmHeights.x, 1.0) : 1.0;
    float stepLength = (segment.y - segment.x) * pathScale / float(steps);
    vec3 point = uEyeBody + direction * segment.x;
    vec3 transmission = vec3(1.0), scattered = vec3(0.0);
    bool blocked = false;
    for (int i = 0; i < 48; ++i) {
        if (i >= steps) break;
        float ds = stepLength;
        if (bending) {
            vec2 exitSegment = atmosphereSphere(point, direction, uAtmOuter);
            if (exitSegment.y <= 0.000001) break;
            ds = min(ds, exitSegment.y);
            vec2 hit = atmosphereSphere(point, direction, 1.0);
            if (hit.x >= 0.0 && hit.x < ds) { ds = hit.x; blocked = true; }
        }
        vec3 middle = point + 0.5*ds*direction;
        vec3 middleDirection = normalize(direction + 0.5*ds*atmosphereCurvature(point,direction));
        float mu = dot(middleDirection, uAtmSunDirection);
        float rayleighPhase = 0.0596831 * (1.0 + mu * mu);
        const float g = 0.65;
        float aerosolPhase = (1.0 - g * g) / (12.566371 * pow(1.0 + g * g - 2.0 * g * mu, 1.5));
        vec2 density = atmosphereDensity(middle);
        vec3 extinction = atmosphereExtinction(density);
        vec3 stepTransmission = exp(-min(extinction * ds, vec3(80.0)));
        vec3 source = uAtmSunlight * atmosphereSunTransmittance(middle) *
            (uAtmRayleigh * density.x * rayleighPhase + uAtmScatter * density.y * aerosolPhase);
        source += uAtmIndirect * atmosphereLightTransmittance(middle, normalize(middle)) *
            (uAtmRayleigh * density.x + uAtmScatter * density.y) * 0.07957747;
        // Analytic integration within each sample stays bounded in thick dust.
        vec3 weight = (vec3(1.0) - stepTransmission) / max(extinction, vec3(1e-8));
        scattered += transmission * source * weight;
        transmission *= stepTransmission;
        direction = normalize(direction + ds*atmosphereCurvature(middle,middleDirection));
        point += ds*middleDirection;
        if (blocked) break;
    }
    vec2 deflection = vec2(0.0);
    if (bending && distant) {
        if (blocked || length(point) < uAtmOuter - 0.00001) {
            original = vec3(0.0);
            transmission = vec3(0.0);
        } else {
            // Reproject the escaped direction into the existing distant image.
            // Near terrain retains raster visibility; unavailable off-screen or
            // occluded samples fall back to the original pixel to avoid smearing.
            vec3 cameraDirection = transpose(mat3(uCameraToBody)) * direction;
            vec4 projected = uProjection * vec4(cameraDirection, 0.0);
            vec2 uv = 0.5 + 0.5 * projected.xy / projected.w;
            if (projected.w > 0.0 && all(greaterThanEqual(uv, vec2(0.0))) && all(lessThanEqual(uv, vec2(1.0))) &&
                sceneDistance(uv) > segment.y + 0.01) {
                original = texture(uSceneColor, uv).rgb;
                deflection = uv-vUv;
            }
        }
    }
    fColor = uAtmIntegrateOnly ? vec4(scattered,deflection.x) : vec4(original*transmission+scattered,1.0);
    fTransfer = vec4(transmission,deflection.y);
}

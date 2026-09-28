#define PLANET_ATMOSPHERE
uniform bool uAtmEnabled;
uniform bool uAtmUseColumns;
uniform sampler2D uAtmColumns;
uniform float uAtmOuter;
uniform float uAtmRefractivity; // Surface n - 1; zero disables bending.
uniform vec2 uAtmHeights;
uniform vec3 uAtmRayleigh;
uniform vec3 uAtmScatter;
uniform vec3 uAtmAbsorb;
uniform vec3 uAtmSunDirection;
uniform float uAtmSunAngularRadius;

vec2 atmosphereSphere(vec3 origin, vec3 direction, float radius) {
    float b = dot(origin, direction);
    float discriminant = b * b - dot(origin, origin) + radius * radius;
    if (discriminant < 0.0) return vec2(-1.0);
    float root = sqrt(max(discriminant, 0.0));
    return vec2(-b - root, -b + root);
}
vec2 atmosphereDensity(vec3 point) {
    float h = max(length(point) - 1.0, 0.0);
    float top = uAtmOuter - 1.0;
    return exp(-h / uAtmHeights) * (1.0 - smoothstep(0.85 * top, top, h));
}
vec3 atmosphereExtinction(vec2 density) {
    return uAtmRayleigh * density.x + (uAtmScatter + uAtmAbsorb) * density.y;
}
vec3 atmosphereLightTransmittance(vec3 point, vec3 direction) {
    if (!uAtmEnabled) return vec3(1.0);
    // Move an exact surface point off the reference sphere to avoid self-hits.
    point += normalize(point) * 0.00001;
    float r = max(length(point), 1.000001);
    float mu = dot(point / r, direction);
    float horizon = -sqrt(max(0.0, 1.0 - 1.0 / (r * r)));
    float sunWidth = max(sin(uAtmSunAngularRadius), 0.001);
    float visibility = smoothstep(horizon - sunWidth, horizon + sunWidth, mu);
    if (visibility <= 0.0) return vec3(0.0);
    // The finite solar disk softens the planet shadow at twilight. For the
    // visible sliver use its grazing path, rather than integrating underground.
    if (mu < horizon + 0.0001) {
        vec3 up = normalize(point);
        vec3 tangent = direction - up * dot(direction, up);
        if (dot(tangent, tangent) < 1e-10) return vec3(0.0);
        float grazing = horizon + 0.0001;
        direction = normalize(normalize(tangent) * sqrt(max(0.0, 1.0 - grazing * grazing)) + up * grazing);
    }
    vec2 outer = atmosphereSphere(point, direction, uAtmOuter);
    if (outer.y <= 0.0) return vec3(1.0);
    vec2 ground = atmosphereSphere(point, direction, 1.0);
    if (ground.x > 0.00001 && ground.x < outer.y) return vec3(0.0);
    if (length(point) < 1.0 && dot(point, direction) < 0.0) return vec3(0.0);
    float start = max(outer.x, 0.0);
    if (uAtmUseColumns) {
        vec3 entry = point + direction*start;
        float inside = 0.0;
        if (length(entry)<1.0) {
            // Reflected cameras can lie below the reference sea sphere.
            inside = max(0.0,atmosphereSphere(entry,direction,1.0).y);
            entry += direction*inside;
        }
        float radius = clamp(length(entry),1.0,uAtmOuter);
        float H = sqrt(uAtmOuter*uAtmOuter-1.0);
        float rho = sqrt(max(0.0,radius*radius-1.0));
        float distance = max(0.0,outer.y-start-inside);
        float shortest = uAtmOuter-radius, longest = rho+H;
        vec2 unit = vec2((distance-shortest)/max(longest-shortest,1e-6),rho/H);
        vec2 uv = (clamp(unit,0.0,1.0)*vec2(255.0,127.0)+0.5)/vec2(256.0,128.0);
        vec2 column = texture(uAtmColumns,uv).rg+vec2(inside);
        vec3 opticalDepth = uAtmRayleigh*column.x+(uAtmScatter+uAtmAbsorb)*column.y;
        return visibility*exp(-min(opticalDepth,vec3(80.0)));
    }
    float stepLength = max(0.0, outer.y - start) / 8.0;
    vec3 opticalDepth = vec3(0.0);
    for (int i = 0; i < 8; ++i)
        opticalDepth += atmosphereExtinction(atmosphereDensity(point + direction * (start + (float(i) + 0.5) * stepLength))) * stepLength;
    return visibility * exp(-min(opticalDepth, vec3(80.0)));
}
vec3 atmosphereSunTransmittance(vec3 point) {
    return atmosphereLightTransmittance(point, uAtmSunDirection);
}

// Same tapered radial profile and midpoint eikonal step as Atmosphere.h.
vec3 atmosphereCurvature(vec3 point, vec3 direction) {
    if (uAtmRefractivity == 0.0) return vec3(0.0);
    float h = max(length(point) - 1.0, 0.0), top = uAtmOuter - 1.0;
    float t = clamp((h - 0.85 * top) / (0.15 * top), 0.0, 1.0);
    float taper = 1.0 - t*t*(3.0 - 2.0*t);
    float derivative = -6.0*t*(1.0-t) / (0.15 * top);
    float base = uAtmRefractivity * exp(-h / uAtmHeights.x);
    vec3 gradient = normalize(point) * base * (derivative - taper / uAtmHeights.x);
    return (gradient - direction * dot(direction, gradient)) / (1.0 + base * taper);
}
void atmosphereAdvance(inout vec3 point, inout vec3 direction, float stepLength) {
    vec3 middleDirection = normalize(direction + 0.5 * stepLength * atmosphereCurvature(point, direction));
    vec3 middlePoint = point + 0.5 * stepLength * direction;
    direction = normalize(direction + stepLength * atmosphereCurvature(middlePoint, middleDirection));
    point += stepLength * middleDirection;
}

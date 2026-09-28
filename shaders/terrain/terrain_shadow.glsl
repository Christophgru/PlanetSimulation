// Shared by terrain and water. The map contains this body's opaque terrain.
in vec4 vShadowPosition;
uniform bool uShadowsEnabled;
uniform sampler2DShadow uShadowMap;
uniform float uShadowBias;

float sunlightVisibility(float incidence) {
    if (!uShadowsEnabled) return 1.0;
    vec3 position = vShadowPosition.xyz / vShadowPosition.w * 0.5 + 0.5;
    // Correct each filter tap for the receiver plane's depth. Without this,
    // the filter mistakes steep, unoccluded slopes for a blocking mountain.
    vec3 dx = dFdx(position), dy = dFdy(position);
    float determinant = dx.x * dy.y - dx.y * dy.x;
    vec2 gradient = vec2(0.0);
    if (abs(determinant) > 1e-12)
        gradient = vec2(dy.y * dx.z - dx.y * dy.z, dx.x * dy.z - dy.x * dx.z) / determinant;
    // At grazing incidence the projected triangle collapses toward a line.
    // Extrapolating its plane across neighboring texels can then put a tap's
    // comparison depth in front of the mountain that should block it. Bound
    // both the tap correction and the bilinear bias to the same finite slope.
    const float maxReceiverSlope = 10.0;
    float slope = abs(gradient.x) + abs(gradient.y);
    gradient /= max(1.0, slope / maxReceiverSlope);
    slope = min(slope, maxReceiverSlope);
    if (incidence <= 0.0) return 1.0;
    if (any(lessThan(position, vec3(0.0))) || any(greaterThan(position, vec3(1.0))))
        return 1.0;
    vec2 texel = 1.0 / vec2(textureSize(uShadowMap, 0));
    // Bias covers the hardware bilinear footprint; the outer 3x3 taps use
    // the bounded receiver-plane gradient instead of increasing that bias.
    float bias = uShadowBias * (1.0 + 2.0 * slope);
    float visible = 0.0;
    for (int y = -1; y <= 1; ++y)
        for (int x = -1; x <= 1; ++x)
            visible += texture(uShadowMap, vec3(position.xy + vec2(x, y) * texel,
                               position.z + dot(gradient, vec2(x, y) * texel) - bias));
    return visible / 9.0;
}

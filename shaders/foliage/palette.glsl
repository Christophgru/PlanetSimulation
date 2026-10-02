// Shared linear albedo: terrain uses the midpoint blade variation.
vec3 grassTipColor(float variation) {
    return mix(vec3(.65,.8,.25),vec3(.8,.9,.4),variation)*mix(.75,1.0,variation);
}
vec3 grassBladeColor(float height,float variation) {
    vec3 base=mix(vec3(.02,.075,.01),vec3(.025,.1,.01),variation)*mix(.75,1.0,variation);
    return mix(base,grassTipColor(variation),pow(height,4.0));
}

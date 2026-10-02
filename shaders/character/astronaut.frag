#version 330 core
in vec3 vWorldPosition, vBodyPosition, vWorldNormal;
uniform vec3 uColor, uSunDirection, uSunlight, uIndirectLight, uEyeWorld;
uniform vec3 uClipCenter;
uniform float uClipRadius, uExposure, uShininess, uGlow;
uniform bool uLinearOutput;
out vec4 fColor;
vec3 displayColor(vec3 value) {
    vec3 mapped=1.0-exp(-max(value,vec3(0))*uExposure);
    return mix(1.055*pow(mapped,vec3(1.0/2.4))-.055,
               12.92*mapped,lessThanEqual(mapped,vec3(.0031308)));
}
void main() {
    if (uClipRadius>0 && distance(vWorldPosition,uClipCenter)<uClipRadius) discard;
    vec3 n=normalize(vWorldNormal);
    float incidence=max(dot(n,uSunDirection),0);
    float diffuse=incidence*(.75+.25*smoothstep(.3,.6,incidence));
    float visibility=sunlightVisibility(incidence);
    vec3 sun=uSunlight, indirect=uIndirectLight;
#ifdef PLANET_ATMOSPHERE
    sun*=atmosphereSunTransmittance(vBodyPosition);
    indirect*=atmosphereLightTransmittance(vBodyPosition,normalize(vBodyPosition));
#endif
    vec3 eye=normalize(uEyeWorld-vWorldPosition);
    vec3 halfway=uSunDirection+eye;
    float specular=length(halfway)>1e-5 ? pow(max(dot(n,normalize(halfway)),0),48) : 0;
    vec3 radiance=uColor*(indirect+sun*diffuse*visibility)+sun*specular*visibility*uShininess*.35;
    if (uGlow>0) radiance=uColor*uGlow*(.3+.7*pow(1.0-max(dot(n,eye),0),2));
    fColor=vec4(uLinearOutput ? radiance : displayColor(radiance),uGlow>0 ? .65 : 1);
}

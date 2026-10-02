#version 330 core
// Adapted from SimonDev Quick_Grass; see external/quick-grass/LICENSE.
in vec3 vBodyPosition,vWorldPosition,vNormal,vUp,vColor,vBlade;
flat in float vFade,vVariation;
uniform vec3 uSunDirection,uSunlight,uIndirectLight,uViewEyeWorld,uClipCenter;
uniform float uClipRadius,uExposure;
uniform bool uLinearOutput;
out vec4 fColor;
vec3 displayColor(vec3 radiance) {
    vec3 mapped=vec3(1)-exp(-max(radiance,vec3(0))*uExposure);
    return mix(1.055*pow(mapped,vec3(1.0/2.4))-.055,12.92*mapped,lessThanEqual(mapped,vec3(.0031308)));
}
float grassShadow() {
    if (!uShadowsEnabled) return 1.0;
    vec3 p=vShadowPosition.xyz/vShadowPosition.w*.5+.5;
    if (any(lessThan(p,vec3(0))) || any(greaterThan(p,vec3(1)))) return 1.0;
    // Thin blades use one hardware-bilinear shadow lookup rather than the
    // terrain's nine receiver-plane taps, which are costly under overdraw.
    return texture(uShadowMap,vec3(p.xy,p.z-2.0*uShadowBias));
}
void main() {
    // Screen-space coverage fade retires whole pixels while keeping the
    // blade's root/height fixed. No transparency sorting or depth blending.
    float threshold=fract(52.9829189*fract(dot(floor(gl_FragCoord.xy),vec2(.06711056,.00583715))));
    if (vFade<=threshold) discard;
    if (uClipRadius>0 && distance(vWorldPosition,uClipCenter)<uClipRadius) discard;
    vec3 normal=normalize(vNormal),up=normalize(vUp);
    vec3 eye=normalize(uViewEyeWorld-vWorldPosition);
    float incidence=max(dot(up,uSunDirection),0.0);
    float visibility=grassShadow()*smoothstep(0.0,.05,incidence);
    float wrap=clamp((dot(normal,uSunDirection)+.5)/1.5,0.0,1.0);
    float scatter=.5*clamp((dot(eye,-uSunDirection)+.5)/1.5,0.0,1.0)*vBlade.z;
    vec3 direct=uSunlight,indirect=uIndirectLight;
#ifdef PLANET_ATMOSPHERE
    direct*=atmosphereSunTransmittance(vBodyPosition);
    indirect*=atmosphereLightTransmittance(vBodyPosition,normalize(vBodyPosition));
#endif
    float ao=mix(.25,1.0,vBlade.x*vBlade.x);
    float widthRatio=mix(1.0-.9*vBlade.x*vBlade.x,1.0-.9*vBlade.x,1.0-vBlade.z);
    float side=.5+vBlade.y/max(widthRatio,.1);
    float middle=1.0-smoothstep(0.0,.5,abs(side-.5));
    vec3 color=grassBladeColor(vBlade.x,vVariation);
    vec3 albedo=color*ao*mix(.85,1.0,middle);
    vec3 radiance=albedo*(indirect+direct*(wrap+scatter)*visibility);
    fColor=vec4(uLinearOutput ? radiance : displayColor(radiance),1);
}

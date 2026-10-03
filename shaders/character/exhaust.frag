#version 330 core
in vec2 vDisc;
in vec3 vWorldPosition,vBodyPosition;
in float vOpacity;
uniform vec3 uCameraRight,uCameraUp,uEyeWorld,uSunDirection,uSunlight,uIndirectLight,uClipCenter;
uniform float uClipRadius,uExposure;
uniform bool uLinearOutput;
out vec4 fColor;
void main() {
    float r2=dot(vDisc,vDisc);
    if (r2>=1 || (uClipRadius>0 && distance(vWorldPosition,uClipCenter)<uClipRadius)) discard;
    vec3 eye=normalize(uEyeWorld-vWorldPosition);
    vec3 n=normalize(uCameraRight*vDisc.x+uCameraUp*vDisc.y+eye*sqrt(1-r2));
    float incidence=max(dot(n,uSunDirection),0);
    float visibility=sunlightVisibility(incidence);
    vec3 sun=uSunlight,ambient=uIndirectLight;
#ifdef PLANET_ATMOSPHERE
    sun*=atmosphereSunTransmittance(vBodyPosition);
    ambient*=atmosphereLightTransmittance(vBodyPosition,normalize(vBodyPosition));
#endif
    float fresnel=.04+.96*pow(1-max(dot(n,eye),0),5);
    vec3 halfway=uSunDirection+eye;
    float specular=length(halfway)>1e-5 ? pow(max(dot(n,normalize(halfway)),0),64) : 0;
    vec3 radiance=vec3(.12,.55,.9)*(1.6+ambient*.35)+ambient*fresnel+
        sun*visibility*(specular*.65+incidence*fresnel*.12);
    float alpha=vOpacity*(.35+.55*fresnel+.10*specular)*(1-smoothstep(.97,1,r2));
    if (!uLinearOutput) {
        vec3 mapped=1-exp(-radiance*uExposure);
        radiance=mix(1.055*pow(mapped,vec3(1.0/2.4))-.055,12.92*mapped,lessThanEqual(mapped,vec3(.0031308)));
    }
    fColor=vec4(radiance,alpha);
}

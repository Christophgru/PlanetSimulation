#version 330 core
// Distant tufts use seeded GPU placement on coarse rendered triangles.
// Palette and wind follow Quick_Grass adaptations; see external/quick-grass.
layout(location=0) in vec3 aPatchA;
layout(location=1) in vec3 aPatchB;
layout(location=2) in vec3 aPatchC;
layout(location=3) in vec3 aPatchNormal;
layout(location=4) in vec3 aPatchColor;
layout(location=5) in float aExpectedCandidates;
layout(location=6) in uint aPatchSeed;
uniform mat4 model,view,projection,uShadowMatrix;
uniform vec3 uGrassEyeBody,uFacingEyeBody,uPlanetColor,uLandscapeLevels;
uniform vec2 uTerrainRockRange;
uniform float uMetersPerRadius,uTime,uWindStrength,uGrassHeight,uGrassWidth,uDrawDistance,uFarDistance;
uniform int uSlotsPerPatch;
uniform bool uLandscapeEnabled,uWaterEnabled;
uniform vec2 uHeightMultiplierRange=vec2(.75,1.5);
uniform vec3 uFarShape=vec3(1.0,6.0,.25); // height scale, width scale, minimum width (m)
uniform vec3 uFarFadeFractions=vec3(.5,.75,.85);
uniform float uGreenRatio=1.15,uWaterClearance=.15;
out vec3 vBodyPosition,vWorldPosition,vNormal,vUp,vColor,vBlade;
out vec4 vShadowPosition;
// Additional outputs make production root generation directly testable.
out vec3 vRoot;
out float vVisible;
// Improved Perlin gradient noise: corner dot products and a quintic fade.
// Algorithm: https://cs.nyu.edu/~perlin/noise/ . Integer hashing replaces
// the reference permutation table; no textures or extra instance data.
float grassGradient(ivec3 cell, vec3 offset) {
    uvec3 c=uvec3(cell & ivec3(255));
    uint h=c.x*1597334677u ^ c.y*3812015801u ^ c.z*2798796415u;
    h^=h>>16; h*=0x7feb352du; h^=h>>15;
    const vec3 gradients[12]=vec3[12](
        vec3(1,1,0),vec3(-1,1,0),vec3(1,-1,0),vec3(-1,-1,0),
        vec3(1,0,1),vec3(-1,0,1),vec3(1,0,-1),vec3(-1,0,-1),
        vec3(0,1,1),vec3(0,-1,1),vec3(0,1,-1),vec3(0,-1,-1));
    return dot(gradients[h%12u],offset);
}
float grassPerlin(vec3 p) {
    // Bound integer conversion as well as the gradient lookup. The field
    // repeats every 256 cells, including at negative coordinates.
    p=mod(p,256.0);
    ivec3 cell=ivec3(floor(p));
    vec3 f=fract(p);
    vec3 blend=f*f*f*(f*(f*6.0-15.0)+10.0);
    return mix(
        mix(mix(grassGradient(cell,f),grassGradient(cell+ivec3(1,0,0),f-vec3(1,0,0)),blend.x),
            mix(grassGradient(cell+ivec3(0,1,0),f-vec3(0,1,0)),grassGradient(cell+ivec3(1,1,0),f-vec3(1,1,0)),blend.x),blend.y),
        mix(mix(grassGradient(cell+ivec3(0,0,1),f-vec3(0,0,1)),grassGradient(cell+ivec3(1,0,1),f-vec3(1,0,1)),blend.x),
            mix(grassGradient(cell+ivec3(0,1,1),f-vec3(0,1,1)),grassGradient(cell+ivec3(1,1,1),f-vec3(1,1,1)),blend.x),blend.y),blend.z);
}


uint horizonHash(uint h) {
    h^=h>>16; h*=0x7feb352du; h^=h>>15; h*=0x846ca68bu; return h^(h>>16);
}
float horizonRandom(uint h) { return float(horizonHash(h)>>8)* (1.0/16777216.0); }
float biomeVisibility(vec3 root,vec3 up,uint seed) {
    float height=(length(root)-1.0)*uMetersPerRadius;
    if (uWaterEnabled && height<=uLandscapeLevels.x+uWaterClearance) return 0.0;
    vec3 tint=aPatchColor;
    if (uLandscapeEnabled) {
        float water=uLandscapeLevels.x, beachTop=water+uLandscapeLevels.y;
        float beachFade=max(.15,.15*uLandscapeLevels.y);
        float relief=max(1.0,uLandscapeLevels.z-water);
        float snowStart=max(beachTop+1.0,water+.25*relief);
        float snowEnd=max(snowStart+1.0,water+.40*relief);
        if (height>=snowEnd) return 0.0;
        float beachWeight=(1.0-smoothstep(beachTop,beachTop+beachFade,height));
        tint=mix(mix(vec3(1.10,1.30,.18),vec3(4.20,1.90,.18),beachWeight),
                 vec3(4.60,2.30,.92),smoothstep(snowStart,snowEnd,height))*uPlanetColor;
    }
    if (tint.y<=uGreenRatio*max(tint.x,tint.z)) return 0.0;
    float slope=1.0-dot(normalize(aPatchNormal),up);
    return horizonRandom(seed+17u)<smoothstep(uTerrainRockRange.x,uTerrainRockRange.y,slope) ? 0.0 : 1.0;
}
void main() {
    uint candidate=uint(gl_InstanceID % uSlotsPerPatch);
    uint seed=horizonHash(aPatchSeed ^ (candidate*0x9e3779b9u));
    float a=sqrt(horizonRandom(seed+1u)), b=horizonRandom(seed+2u);
    vec3 root=aPatchA*(1.0-a)+aPatchB*(a*(1.0-b))+aPatchC*(a*b);
    vRoot=root; vVisible=0.0;
    vBodyPosition=root; vWorldPosition=(model*vec4(root,1)).xyz;
    vNormal=vec3(0,0,1); vUp=vNormal; vColor=vec3(0); vBlade=vec3(0);
    vShadowPosition=vec4(0,0,0,1); gl_Position=vec4(2,2,2,1);
    if (horizonRandom(seed+3u)>=aExpectedCandidates/float(uSlotsPerPatch) || length(root)<1e-9) return;
    float distanceToEye=length(root-uGrassEyeBody)*uMetersPerRadius;
    // Fade geometry in as detailed Gaussian grass thins, and sink out at
    // the conservative horizon. Rejection precedes the expensive wind field.
    float fade=smoothstep(uDrawDistance*uFarFadeFractions.x,uDrawDistance*uFarFadeFractions.y,distanceToEye)*
        (1.0-smoothstep(uFarDistance*uFarFadeFractions.z,uFarDistance,distanceToEye));
    vec3 up=normalize(root);
    if (fade<=0.0 || biomeVisibility(root,up,seed)==0.0) return;
    vVisible=fade;
    float t=gl_VertexID==2 ? 1.0 : 0.0;
    float side=gl_VertexID==1 ? 1.0 : (gl_VertexID==2 ? .5 : 0.0);
    vec3 viewDirection=uFacingEyeBody-root;
    vec3 right=cross(up,viewDirection);
    vec3 bodyRight=normalize(cross(abs(up.z)<.9 ? vec3(0,0,1) : vec3(0,1,0),up));
    right=length(right)>1e-7 ? normalize(right) : bodyRight;
    float variation=horizonRandom(seed+4u);
    float height=uGrassHeight*uFarShape.x*mix(uHeightMultiplierRange.x,uHeightMultiplierRange.y,horizonRandom(seed+5u));
    float width=max(uFarShape.z,uGrassWidth*uFarShape.y);
    vec3 p=root*uMetersPerRadius;
    // One coherent gust field, rather than all three detailed wind fields.
    float gust=clamp(.5+.5*grassPerlin(p*.08+uTime*vec3(.25,.125,0)),0.0,1.0);
    vec3 tipOffset=up*height+bodyRight*(height*.25*uWindStrength*(gust-.5));
    vec3 local=right*((side-.5)*width*(1.0-t))+tipOffset*t;
    vec3 body=root+(local*fade-up*(height+width+.01)*(1.0-fade))/uMetersPerRadius;
    vBodyPosition=body;
    vec4 world=model*vec4(body,1); vWorldPosition=world.xyz;
    vUp=normalize(mat3(model)*up); vNormal=vUp;
    vColor=mix(vec3(.02,.075,.01),vec3(.65,.8,.25),t)*mix(.85,1.05,variation);
    vBlade=vec3(t,side,0); // Cheap distant lighting has no blade backscatter.
    vShadowPosition=uShadowMatrix*vec4(body,1);
    gl_Position=projection*view*world;
}

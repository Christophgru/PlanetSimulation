#version 330 core
// Blade shape, palette and lighting adapted from SimonDev Quick_Grass (MIT).
// See external/quick-grass/LICENSE and README.md for the pinned source.
layout(location=0) in vec3 aRoot;
layout(location=1) in vec3 aUp;
layout(location=2) in vec4 aVariation;
uniform mat4 model,view,projection,uShadowMatrix;
uniform vec3 uGrassEyeBody;
uniform float uMetersPerRadius,uTime,uWindStrength,uGrassHeight,uGrassWidth,uDrawDistance;
uniform float uQuadDistance=15.0;
uniform int uSegments;
// Descriptor attributes advance once per uSlotsPerPatch generated roots.
layout(location=3) in uint aTriangle;
uniform bool uProcedural=false,uGpuInstances=false;
layout(location=4) in vec3 aWindSamples;
layout(location=5) in float aCoverage;
layout(location=6) in float aLow;
uniform int uSlotsPerPatch,uWindSeed=0;
uniform vec3 uPlanetColor,uLandscapeLevels;
uniform vec2 uTerrainRockRange,uHeightMultiplierRange=vec2(.75,1.5),uLeanRange=vec2(.1,.4);
uniform bool uLandscapeEnabled,uWaterEnabled;
uniform float uGaussianSigma=13.333333,uGreenRatio=1.15,uWaterClearance=.15,uRootOffset=.005;
uniform vec3 uWindFrequencies=vec3(.08,.02,.7);
flat out float vFade,vVariation;
out vec3 vRoot;
out float vVisible;
out vec3 vBodyPosition,vWorldPosition,vNormal,vUp,vColor;
out vec4 vShadowPosition;
out vec3 vBlade; // height fraction, normalized lateral offset, detailed shading weight

uniform samplerBuffer uTerrainVertices;
uniform usamplerBuffer uTerrainIndices;
uniform int uGrassSeed=7321;
uniform float uPlacementDensity;
vec3 aPatchA,aPatchB,aPatchC,aPatchNormal,aPatchColor;
vec3 patchNormalA,patchNormalB,patchNormalC,patchColorA,patchColorB,patchColorC;
float aExpectedCandidates;
uint aPatchSeed;
vec3 terrainVector(int offset) {
    // R32F buffer views work on core GL 3.3 without RGB32 extensions or
    // repacking the terrain's nine-float interleaved vertex records.
    return vec3(texelFetch(uTerrainVertices,offset).r,
        texelFetch(uTerrainVertices,offset+1).r,texelFetch(uTerrainVertices,offset+2).r);
}
uniform bool uFrustumCull=false;
uniform float uCullExtent=0;
bool patchVisible() {
    vec3 center=(aPatchA+aPatchB+aPatchC)/3.0;
    float radius=max(max(length(aPatchA-center),length(aPatchB-center)),length(aPatchC-center))+uCullExtent;
    mat4 clip=projection*view*model;
    vec4 w=vec4(clip[0][3],clip[1][3],clip[2][3],clip[3][3]);
    for (int axis=0;axis<3;++axis) {
        vec4 row=vec4(clip[0][axis],clip[1][axis],clip[2][axis],clip[3][axis]);
        for (int sign=-1;sign<=1;sign+=2) {
            vec4 plane=w+float(sign)*row;
            if (dot(plane,vec4(center,1)) < -(radius+1e-5)*length(plane.xyz)) return false;
        }
    }
    return true;
}
bool loadTerrainPatch() {
    ivec3 indices=ivec3(texelFetch(uTerrainIndices,int(aTriangle)*3).r,
        texelFetch(uTerrainIndices,int(aTriangle)*3+1).r,
        texelFetch(uTerrainIndices,int(aTriangle)*3+2).r)*9;
    aPatchA=terrainVector(indices.x);
    aPatchB=terrainVector(indices.y);
    aPatchC=terrainVector(indices.z);
    if (uFrustumCull && !patchVisible()) return false;
    patchNormalA=terrainVector(indices.x+3);
    patchNormalB=terrainVector(indices.y+3);
    patchNormalC=terrainVector(indices.z+3);
    patchColorA=terrainVector(indices.x+6)*uPlanetColor;
    patchColorB=terrainVector(indices.y+6)*uPlanetColor;
    patchColorC=terrainVector(indices.z+6)*uPlanetColor;
    aPatchNormal=normalize(patchNormalA+patchNormalB+patchNormalC);
    aPatchColor=(patchColorA+patchColorB+patchColorC)/3.0;
    aExpectedCandidates=.5*length(cross(aPatchB-aPatchA,aPatchC-aPatchA))*
        uMetersPerRadius*uMetersPerRadius*uPlacementDensity;
    aPatchSeed=aTriangle ^ uint(uGrassSeed);
    return true;
}

// Improved Perlin gradient noise: corner dot products and a quintic fade.
// Algorithm: https://cs.nyu.edu/~perlin/noise/ . Integer hashing replaces
// the reference permutation table; no textures or extra instance data.
float grassGradient(ivec3 cell, vec3 offset) {
    uvec3 c=uvec3(cell & ivec3(255));
    uint h=c.x*1597334677u ^ c.y*3812015801u ^ c.z*2798796415u ^ uint(uWindSeed);
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

mat3 axisRotation(vec3 a,float angle) {
    float s=sin(angle),c=cos(angle),k=1.0-c;
    return mat3(c+a.x*a.x*k,a.y*a.x*k+a.z*s,a.z*a.x*k-a.y*s,
                a.x*a.y*k-a.z*s,c+a.y*a.y*k,a.z*a.y*k+a.x*s,
                a.x*a.z*k+a.y*s,a.y*a.z*k-a.x*s,c+a.z*a.z*k);
}
float grassLodFadeEnd(float variation) {
    // Shared with GrassLod.cpp: a stable, tint-independent retention tier.
    uint h=uint(variation*65536.0);
    h^=h>>16; h*=0x7feb352du; h^=h>>15; h*=0x846ca68bu; h^=h>>16;
    float nearDistance=min(15.0,uDrawDistance*.25);
    return nearDistance+float((h&7u)+1u)*(uDrawDistance-nearDistance)/8.0;
}
uint grassHash(uint h) {
    h^=h>>16; h*=0x7feb352du; h^=h>>15; h*=0x846ca68bu; return h^(h>>16);
}
float grassRandom(uint h) { return float(grassHash(h)>>8)* (1.0/16777216.0); }
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
    return grassRandom(seed+17u)<smoothstep(uTerrainRockRange.x,uTerrainRockRange.y,slope) ? 0.0 : 1.0;
}
void main() {
    vec3 root=aRoot, rootUp=aUp;
    vec4 variation=aVariation;
    float densityFade=1.0;
    vFade=1.0; vVariation=variation.w; vRoot=root; vVisible=1.0;
    if (uProcedural) {
        if (!loadTerrainPatch()) {
            vVisible=0.0; vFade=0.0; vBodyPosition=vec3(0); vWorldPosition=vec3(0);
            vNormal=vec3(0,0,1); vUp=vNormal; vColor=vec3(0); vBlade=vec3(0);
            vShadowPosition=vec4(0,0,0,1); gl_Position=vec4(2,2,2,1); return;
        }
        uint seed=grassHash(aPatchSeed ^ (uint(gl_InstanceID % uSlotsPerPatch)*0x9e3779b9u));
        float a=sqrt(grassRandom(seed+1u)), b=grassRandom(seed+2u);
        root=aPatchA*(1.0-a)+aPatchB*(a*(1.0-b))+aPatchC*(a*b);
        rootUp=normalize(root);
        aPatchNormal=normalize(patchNormalA*(1.0-a)+patchNormalB*(a*(1.0-b))+patchNormalC*(a*b));
        aPatchColor=patchColorA*(1.0-a)+patchColorB*(a*(1.0-b))+patchColorC*(a*b);
        vRoot=root; vVisible=0.0; vFade=0.0;
        vBodyPosition=root; vWorldPosition=(model*vec4(root,1)).xyz;
        vNormal=vec3(0,0,1); vUp=vNormal; vColor=vec3(0); vBlade=vec3(0);
        vShadowPosition=vec4(0,0,0,1); gl_Position=vec4(2,2,2,1);
        // A slot keeps its own density rank when candidate batches grow or
        // shrink. Dividing acceptance by batch size used to reshuffle roots.
        float d=length(root-uGrassEyeBody)*uMetersPerRadius;
        float expected=aExpectedCandidates*exp(-d*d/(2.0*uGaussianSigma*uGaussianSigma));
        float rank=float(gl_InstanceID % uSlotsPerPatch)+max(grassRandom(seed+3u),1e-5);
        densityFade=smoothstep(rank,rank*1.2,expected);
        if (length(root)<1e-9 || d>=uDrawDistance || densityFade<=0.0 ||
            biomeVisibility(root,rootUp,seed)==0.0) return;
        variation=vec4(grassRandom(seed+6u)*6.28318530718,
            mix(uLeanRange.x,uLeanRange.y,grassRandom(seed+7u)),
            mix(uHeightMultiplierRange.x,uHeightMultiplierRange.y,grassRandom(seed+5u)),grassRandom(seed+4u));
        root+=normalize(aPatchNormal)*(uRootOffset/uMetersPerRadius);
        vVisible=1.0; vVariation=variation.w;
    }
    float t=float(gl_VertexID/2)/float(uSegments);
    // A narrow but nonzero top edge makes both triangles of the low quad
    // useful, and shares endpoints with the six-segment strip.
    float side=float(gl_VertexID%2);
    float distanceToEye=length(root-uGrassEyeBody)*uMetersPerRadius;
    float nearDistance=min(15.0,uDrawDistance*.25);
    float low=uGpuInstances ? aLow : smoothstep(uQuadDistance*.5,uQuadDistance,distanceToEye);
    float fadeEnd=grassLodFadeEnd(variation.w);
    float tierWidth=(uDrawDistance-nearDistance)/8.0;
    vFade=min(1.0-smoothstep(fadeEnd-tierWidth,fadeEnd,distanceToEye),
              1.0-smoothstep(uDrawDistance*.75,uDrawDistance,distanceToEye));
    vFade*=densityFade;
    if (uGpuInstances) vFade=aCoverage;
    vVisible=vFade;
    vec3 up=normalize(rootUp);
    vec3 right=normalize(cross(abs(up.z)<0.9 ? vec3(0,0,1) : vec3(0,1,0),up));
    vec3 forward=cross(right,up);
    mat3 frame=mat3(right,up,forward);
    vec3 p=root*uMetersPerRadius;
    // Advected 3D fields stay attached to the body without a UV/pole seam.
    // All advection rates traverse whole 256-cell periods in 8192 seconds;
    // GrassWind.h wraps the double clock by that exact common period.
    float gust=uGpuInstances ? aWindSamples.x : clamp(0.5+0.5*grassPerlin(p*uWindFrequencies.x+uTime*vec3(.25,.125,0)),0.0,1.0);
    // Distant blades become straight before the CPU can lower their segment
    // count. Collinear intermediate vertices then vanish without a shape pop.
    float windAngle=pow(mix(.25,1.0,gust),2.0)*1.25*uWindStrength*mix(t,1.0,low);
    float windDirection=uGpuInstances ? aWindSamples.y : grassPerlin(p*uWindFrequencies.y+vec3(19.3,7.1,43.7)+uTime*vec3(.03125,0,0));
    mat3 wind=axisRotation(vec3(cos(windDirection),0,sin(windDirection)),windAngle);
    mat3 turn=axisRotation(vec3(0,1,0),variation.x);
    float flutter=uGpuInstances ? aWindSamples.z : grassPerlin(p*uWindFrequencies.z+vec3(5.2,31.8,11.4)+uTime*vec3(0,.5,0));
    float lean=variation.y+.1*uWindStrength*flutter;
    float curve=-lean*mix(t*t,1.0,low);
    float height=uGrassHeight*variation.z;
    float width=uGrassWidth*mix(1.0-.9*t*t,1.0-.9*t,low);
    vec3 local=axisRotation(vec3(1,0,0),curve)*vec3((side-.5)*width,t*height,0);
    mat3 bend=frame*wind*turn;
    // Fade coverage in the fragment shader; roots and height never sink.
    vec3 body=root+bend*local/uMetersPerRadius;
    float derivative=-2.0*lean*t*(1.0-low);
    vec3 tangent=normalize(vec3(0,cos(curve)-t*sin(curve)*derivative,sin(curve)+t*cos(curve)*derivative));
    vec3 bladeNormal=vec3(0,-tangent.z,tangent.y);
    bladeNormal=axisRotation(vec3(0,1,0),mix(.3,-.3,side)*3.14159265)*bladeNormal;
    vNormal=normalize(mat3(model)*normalize(mix(up,bend*bladeNormal,.25*(1.0-low))));
    vUp=normalize(mat3(model)*up);
    vColor=vec3(1); // Shared fragment palette avoids interpolation-dependent LOD tint.
    // Interpolate physical lateral offset, then recover side in the fragment.
    // Interpolating side directly shades a tapered quad differently from strips.
    vBlade=vec3(t,(side-.5)*mix(1.0-.9*t*t,1.0-.9*t,low),1.0-low);
    vBodyPosition=body;
    vec4 world=model*vec4(body,1);
    vWorldPosition=world.xyz;
    vShadowPosition=uShadowMatrix*vec4(body,1);
    gl_Position=projection*view*world;
}

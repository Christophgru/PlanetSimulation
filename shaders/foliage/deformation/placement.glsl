// Static roots are generated once; each view compacts and animates them.
layout(local_size_x=128) in;
layout(std430,binding=0) readonly buffer TriangleRules { uint triangleIDs[]; };
struct Command { uint vertices, instances, first, base; };
layout(std430,binding=1) buffer DrawCommands { Command commands[2];uvec4 animationDiagnostics; };
struct Blade { vec4 rootFade, upLow, variation;vec3 wind;uint animationIndex; };
layout(std430,binding=2) buffer GeneratedBlades { Blade blades[]; };
struct Root { vec4 rootExpected,upRank,normalSlot,variation; };
layout(std430,binding=3) buffer CachedRoots { Root roots[]; };
layout(std430,binding=4) buffer PatchBounds { vec4 bounds[]; };
struct Cell {uvec4 key,geometry,span;vec4 metric;};
layout(std430,binding=6) readonly buffer Cells {Cell cells[];};
layout(std430,binding=7) readonly buffer PreviousRoots {Root previousRoots[];};
uniform bool uCellMode=false;
uniform int uCellFirst=0;
uniform bool uBuildRoots=false;
uniform int uFirstRoot=0;
uniform bool uReset;
uniform float uQuadDistance=15.0;
uniform int uCapacity,uFirstPatch,uCandidateCount,uSlotsPerPatch,uWindSeed=0;
uniform int uCandidateOffset=0;
uniform mat4 model,view,projection;
uniform vec3 uGrassEyeBody,uPlanetColor,uLandscapeLevels;
uniform vec2 uTerrainRockRange,uHeightMultiplierRange,uLeanRange;
uniform bool uLandscapeEnabled,uWaterEnabled;
uniform float uMetersPerRadius,uTime,uWindStrength,uGrassHeight,uGrassWidth,uDrawDistance;
uniform float uGaussianSigma,uGreenRatio,uWaterClearance,uRootOffset;
uniform vec3 uWindFrequencies;
uint aTriangle;
uniform samplerBuffer uTerrainVertices;
uniform usamplerBuffer uTerrainIndices;
uniform int uGrassSeed=7321;
#ifdef GRASS_MORPH_SOURCE
#include "source.glsl"
#endif
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
bool patchVisible(vec3 center,float reach) {
    float radius=reach+uCullExtent;
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
    uint identity=aTriangle;
    if(uCellFirst>0) {
        uvec4 key;
        for(int i=0;i<4;++i) key[i]=texelFetch(uTerrainIndices,uCellFirst+int(aTriangle)*4+i).r;
        indices=ivec3(indices[int(key.w&3u)],indices[int((key.w>>2u)&3u)],indices[int((key.w>>4u)&3u)]);
        identity=key.x^(key.y*0x9e3779b9u)^(key.z*0x85ebca6bu);
    }
    #ifdef GRASS_MORPH_SOURCE
    aPatchA=vec3(terrainMorphFinePosition(uint(indices.x/9)));
#else
    aPatchA=terrainVector(indices.x);
#endif
    #ifdef GRASS_MORPH_SOURCE
    aPatchB=vec3(terrainMorphFinePosition(uint(indices.y/9)));
#else
    aPatchB=terrainVector(indices.y);
#endif
    #ifdef GRASS_MORPH_SOURCE
    aPatchC=vec3(terrainMorphFinePosition(uint(indices.z/9)));
#else
    aPatchC=terrainVector(indices.z);
#endif
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
    aPatchSeed=identity ^ uint(uGrassSeed);
    return true;
}

uniform bool uAdaptiveFalloff=false;
float grassLodFadeEnd(float variation) {
    // Shared with GrassLod.cpp: a stable, tint-independent retention tier.
    uint h=uint(variation*65536.0);
    h^=h>>16; h*=0x7feb352du; h^=h>>15; h*=0x846ca68bu; h^=h>>16;
    float nearDistance=uAdaptiveFalloff?uQuadDistance:min(15.0,uDrawDistance*.25);
    return nearDistance+float((h&7u)+1u)*(uDrawDistance-nearDistance)/8.0;
}
float grassDensityWeight(float distance) {
    float d=uAdaptiveFalloff?max(0.0,distance-uQuadDistance):distance;
    return d==0.0 ? 1.0 : uGaussianSigma<=0.0 ? 0.0 : exp(-d*d/(2.0*uGaussianSigma*uGaussianSigma));
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
#ifdef GRASS_MORPH_SOURCE
bool surfaceBiomeVisible(vec3 root) {
    float height=(length(root)-1.0)*uMetersPerRadius;
    if(uWaterEnabled && height<=uLandscapeLevels.x+uWaterClearance) return false;
    if(!uLandscapeEnabled) return true; // Fixed tint/slope were filtered during generation.
    float water=uLandscapeLevels.x,beachTop=water+uLandscapeLevels.y;
    float beachFade=max(.15,.15*uLandscapeLevels.y),relief=max(1.0,uLandscapeLevels.z-water);
    float snowStart=max(beachTop+1.0,water+.25*relief),snowEnd=max(snowStart+1.0,water+.40*relief);
    if(height>=snowEnd) return false;
    float beachWeight=1.0-smoothstep(beachTop,beachTop+beachFade,height);
    vec3 tint=mix(mix(vec3(1.10,1.30,.18),vec3(4.20,1.90,.18),beachWeight),
        vec3(4.60,2.30,.92),smoothstep(snowStart,snowEnd,height))*uPlanetColor;
    return tint.y>uGreenRatio*max(tint.x,tint.z);
}
#endif
void main() {
    uint id=gl_GlobalInvocationID.x+uint(uCandidateOffset);
    if (uReset) {
        if(gl_GlobalInvocationID.x==0u) animationDiagnostics=uvec4(0);
        if (gl_GlobalInvocationID.x<2u) commands[gl_GlobalInvocationID.x]=Command(gl_GlobalInvocationID.x==0u ? 14u : 4u,0u,0u,0u);
        return;
    }
    if (id>=uint(uCandidateCount)) return;
    uint patchIndex=uint(uFirstPatch)+id/uint(uSlotsPerPatch);
    uint index=uint(uFirstRoot)+id;
    if(uBuildRoots) {
        if(uCellMode) {
            Cell cell=cells[patchIndex];uint slot=id%uint(uSlotsPerPatch);
            if(cell.span.z!=0xffffffffu && slot<cell.span.w) {
                Root cached=previousRoots[cell.span.z+slot];
                cached.rootExpected.w=cell.metric.x;roots[index]=cached;return;
            }
        }
        roots[index]=Root(vec4(0),vec4(0),vec4(0),vec4(0));
        aTriangle=triangleIDs[patchIndex];
        if (!loadTerrainPatch()) return;
        vec3 center=(aPatchA+aPatchB+aPatchC)/3.0;
        float reach=max(max(length(aPatchA-center),length(aPatchB-center)),length(aPatchC-center));
        if(!uCellMode && id%uint(uSlotsPerPatch)==0u) bounds[patchIndex]=vec4(center,reach);
        uint seed=grassHash(aPatchSeed ^ ((id%uint(uSlotsPerPatch))*0x9e3779b9u));
        float a=sqrt(grassRandom(seed+1u)), b=grassRandom(seed+2u);
        vec3 root=aPatchA*(1.0-a)+aPatchB*(a*(1.0-b))+aPatchC*(a*b);
        if (length(root)<1e-9) return;
        vec3 up=normalize(root);
        aPatchNormal=normalize(patchNormalA*(1.0-a)+patchNormalB*(a*(1.0-b))+patchNormalC*(a*b));
        aPatchColor=patchColorA*(1.0-a)+patchColorB*(a*(1.0-b))+patchColorC*(a*b);
#ifdef GRASS_MORPH_SOURCE
        // Height eligibility changes with the surface. Keep potential roots;
        // fixed tint and field-normal slope still use their stable source data.
        if(!uLandscapeEnabled && aPatchColor.y<=uGreenRatio*max(aPatchColor.x,aPatchColor.z)) return;
        float slope=1.0-dot(normalize(aPatchNormal),up);
        if(grassRandom(seed+17u)<smoothstep(uTerrainRockRange.x,uTerrainRockRange.y,slope)) return;
#else
        if (biomeVisibility(root,up,seed)==0.0) return;
#endif
        vec4 variation=vec4(grassRandom(seed+6u)*6.28318530718,
            mix(uLeanRange.x,uLeanRange.y,grassRandom(seed+7u)),
            mix(uHeightMultiplierRange.x,uHeightMultiplierRange.y,grassRandom(seed+5u)),grassRandom(seed+4u));
        float rank=float(id%uint(uSlotsPerPatch))+max(grassRandom(seed+3u),1e-5);
        roots[index]=Root(vec4(root,aExpectedCandidates),vec4(up,rank),
            vec4(aPatchNormal,float(id%uint(uSlotsPerPatch))+1.0),variation);
        return;
    }
    vec4 bound=bounds[patchIndex];
    if(uFrustumCull && !patchVisible(bound.xyz,bound.w)) return;
    Root cached=roots[index];
    if(cached.normalSlot.w==0.0) return;
    vec3 root=cached.rootExpected.xyz,up=cached.upRank.xyz;
#ifdef GRASS_MORPH_SOURCE
    Cell cell=cells[patchIndex];
    float denominator=dot(cell.metric.yzw,up);
    if(cell.metric.x<=0.0 || abs(denominator)<1e-9) return;
    float radius=uintBitsToFloat(cell.geometry.w)/denominator;
    if(radius<=0.0 || isnan(radius) || isinf(radius)) return;
    root=up*radius;
    if(!surfaceBiomeVisible(root)) return;
#endif
    float d=length(root-uGrassEyeBody)*uMetersPerRadius;
    vec3 center=bound.xyz;float reach=bound.w;
    float expected=cached.rootExpected.w*grassDensityWeight(d);
#ifdef GRASS_MORPH_SOURCE
    expected=cell.metric.x*grassDensityWeight(d);
#endif
    float rank=cached.upRank.w;
    float densityFade=uAdaptiveFalloff?clamp(expected-(cached.normalSlot.w-1.0),0.0,1.0):smoothstep(rank,rank*1.2,expected);
    if (d>=uDrawDistance || densityFade<=0.0) return;
    vec4 variation=cached.variation;
    float nearDistance=uAdaptiveFalloff?uQuadDistance:min(15.0,uDrawDistance*.25);
    uint queue=(length(center-uGrassEyeBody)-reach)*uMetersPerRadius<uQuadDistance+uRootOffset ? 0u : 1u;
    root+=cached.normalSlot.xyz*(uRootOffset/uMetersPerRadius);
    float shadingDistance=length(root-uGrassEyeBody)*uMetersPerRadius;
    float low=smoothstep(uQuadDistance*.5,uQuadDistance,shadingDistance);
    float end=grassLodFadeEnd(variation.w);
    float fade=min(1.0-smoothstep(end-(uDrawDistance-nearDistance)/8.0,end,shadingDistance),
        1.0-smoothstep(uAdaptiveFalloff?max(uQuadDistance,uDrawDistance*.75):uDrawDistance*.75,uDrawDistance,shadingDistance));
    if(uAdaptiveFalloff && shadingDistance<=uQuadDistance) fade=1.0;
    fade*=densityFade;
    if (fade<=0.0) return;
    // Submitted candidate counts are bounded by the selected plan. Compaction can
    // only reduce them; each output queue has capacity for the entire plan.
    uint slot=atomicAdd(commands[queue].instances,1u)+queue*uint(uCapacity);
    blades[slot]=Blade(vec4(root,fade),vec4(up,low),variation,vec3(0),index);
}

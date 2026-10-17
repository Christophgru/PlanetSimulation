// Integer views borrow the same immutable buffers used by metadata. These
// source positions stay fixed while the live terrain's position words change.
uniform usamplerBuffer uMorphTargets,uMorphCorners;
uniform int uMorphSamples;
uint terrainMorphWord(uint offset) {return texelFetch(uMorphTargets,int(offset)).r;}
uint terrainMorphCorner(uint corner) {return texelFetch(uMorphCorners,int(corner)).r;}
#include "../../terrain/compute/morph_source.glsl"

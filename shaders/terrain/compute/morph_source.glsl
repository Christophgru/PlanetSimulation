// The consumer supplies terrainMorphWord(uint) and terrainMorphCorner(uint),
// keeping the immutable decoder usable through SSBO or integer texture views.
double terrainMorphDouble(uint offset) {
    return packDouble2x32(uvec2(terrainMorphWord(offset),terrainMorphWord(offset+1u)));
}
dvec3 terrainMorphFinePosition(uint corner) {
    uint base=9u*terrainMorphCorner(corner);
    return dvec3(uintBitsToFloat(terrainMorphWord(base+6u)),
        uintBitsToFloat(terrainMorphWord(base+7u)),uintBitsToFloat(terrainMorphWord(base+8u)));
}
dvec3 terrainMorphParentPosition(uint corner,uint samples) {
    uint base=9u*samples+9u*terrainMorphCorner(corner);
    return dvec3(terrainMorphDouble(base),terrainMorphDouble(base+2u),terrainMorphDouble(base+4u));
}
vec3 terrainMorphInitialPosition(uint corner,uint samples) {
    uint tail=9u*samples+9u*terrainMorphCorner(corner);
    return vec3(uintBitsToFloat(terrainMorphWord(tail+6u)),
        uintBitsToFloat(terrainMorphWord(tail+7u)),uintBitsToFloat(terrainMorphWord(tail+8u)));
}

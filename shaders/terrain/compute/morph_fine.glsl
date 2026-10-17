// Fine anchors need integer texture views only, including in GLSL 3.30.
// The consumer supplies terrainMorphWord(uint) and terrainMorphCorner(uint).
vec3 terrainMorphFinePosition(uint corner) {
    uint base=9u*terrainMorphCorner(corner);
    return vec3(uintBitsToFloat(terrainMorphWord(base+6u)),
        uintBitsToFloat(terrainMorphWord(base+7u)),uintBitsToFloat(terrainMorphWord(base+8u)));
}

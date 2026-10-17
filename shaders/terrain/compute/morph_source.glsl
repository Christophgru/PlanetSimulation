// The consumer supplies terrainMorphWord(uint) and terrainMorphCorner(uint),
// keeping the immutable decoder usable through SSBO or integer texture views.
vec3 terrainMorphInitialPosition(uint corner,uint samples) {
    uint tail=9u*samples+9u*terrainMorphCorner(corner);
    return vec3(uintBitsToFloat(terrainMorphWord(tail+6u)),
        uintBitsToFloat(terrainMorphWord(tail+7u)),uintBitsToFloat(terrainMorphWord(tail+8u)));
}

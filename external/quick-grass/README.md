# Quick_Grass attribution

The foliage shaders adapt SimonDev's [Quick_Grass](https://github.com/simondevyoutube/Quick_Grass),
revision `6b56164272f213e353a4045d1439d071d5a968cd`, especially
`public/shaders/grass-lighting-model-{vsh,fsh}.glsl` and
`src/base/render/grass-component.js`. The MIT notice is in [LICENSE](LICENSE).

Preserved: six-segment near blades, one-segment distant blades, tapered width,
random height/lean/orientation, wind bending, dark bases and bright yellow-green
tips, curved normals, wrapped diffuse light and approximate backscatter.
The OpenGL port uses body-local spherical frames, rendered-terrain roots,
terrain shadows, the existing atmosphere and exposure, and bounded placement.
It uses periodic 3D Perlin gradient noise for gusts, direction and flutter rather
than the demo's noise library, omits its player
collision and view-space thickening, and defaults to a shorter draw distance.
No Three.js runtime or reference textures are required.

The port now adds intermediate geometry levels, eight stable density-retention
tiers with sinking, single-tip strips and a shared instance buffer. These
adaptations are described in the [LOD journal](../../docs/journal/benchmarks/grass-lod.md).

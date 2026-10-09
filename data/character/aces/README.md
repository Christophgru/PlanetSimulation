# NASA Advanced Crew Escape Suit (ACES)

Selected by the user on 2026-10-09; this is the default runtime astronaut.
Original model credit: **NASA / Michael D. Carbajal**.

- [NASA source listing](https://science.nasa.gov/3d-resources/advanced-crew-escape-suit/)
- [Original GLB at pinned NASA revision](https://github.com/nasa/NASA-3D-Resources/blob/11ebb4ee043715aefbba6aeec8a61746fad67fa7/3D%20Models/Advanced%20Crew%20Escape%20Suit/Advanced%20Crew%20Escape%20Suit.glb)
- [NASA images and media usage guidelines](https://www.nasa.gov/nasa-brand-center/images-and-media/)

NASA makes this model freely available; preserve the credit and observe NASA's
media and insignia guidance. This project does not claim NASA endorsement or a
CC0 license. The unmodified download is `source.glb`, SHA256
`6017772d557c30ea86e2e3a7651ad2089499f3d0c577b983467650616bfaebeb`.

`suit.bin` retains all 88,872 triangles and original linear material colours,
including orange fabric and blue straps. The model is normalized to 2.1 m tall,
Y-up, facing -Z. Original loop normals/material boundaries are retained, with
byte-identical vertex records welded. Opaque rendering repairs the original
export's unintended full transmission on cloth/hardware.

The source has no skeleton or animation clips. Our authored 13-joint bind rig
and two influences per vertex drive the mesh from existing terrain foot IK,
procedural arm swing and flight poses. The helmet/body remain rigid, boots follow
the contact normals, and walking/sprinting bend the actual knees and sleeves.
There is no finger articulation or imported motion-capture animation. Custom
German upper-arm patches and the functional jetpack remain separate accessories.
The source ACES suit is a launch/entry suit; the jetpack is a game attachment.

Regenerate with Blender **4.5 LTS** (no Python package download required):

```sh
blender -b --disable-autoexec --python-exit-code 1 -P scripts/character/prepare_aces.py
```

`inspection.json` records hashes, dimensions, counts, materials and bind joints.
The little-endian deployment format has an 8-byte `ACESSUIT` signature, four
uint32 values (version 1, vertex count, index count, joint count), three float32
bind body offsets, 13 head/tail float32 joint pairs, 52-byte vertices
(position3, normal3, colour/shine4, two uint16 joints, two float32 weights), then
uint32 triangle indices. The loader validates version, bounded counts, finite
values, normalized normals, influences and indices before the one-time upload.
Only the 13 matrices are updated each draw; mesh buffers stay resident.

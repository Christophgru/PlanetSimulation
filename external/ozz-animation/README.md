# ozz-animation

Astronaut leg IK uses the unmodified MIT-licensed ozz-animation runtime,
release 0.16.0, commit `ca6328f51d857fa516edf915f3cde92eb6c90ad2`.
CMake fetches its sources; this folder preserves the upstream license.

- Source: https://github.com/guillaumeblanc/ozz-animation
- Foot IK example: https://guillaumeblanc.github.io/ozz-animation/samples/foot_ik/
- IK documentation: https://guillaumeblanc.github.io/ozz-animation/documentation/ik/
- License: [MIT](LICENSE.md), copyright Guillaume Blanc.

Only `ozz_animation` and `ozz_base` are linked. Tools, FBX/glTF importers,
samples and upstream tests are disabled. The source is not modified.
`IKTwoBoneJob` supplies leg joint corrections; this project implements gait,
stance locking, terrain contacts, pole-safe placement and the follow camera.

The astronaut model is original procedural geometry in
`src/rendering/character/AstronautRenderer.cpp`; it is not an upstream asset.
No downloaded model, texture, skeleton or animation clip is redistributed.

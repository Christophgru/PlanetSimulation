# Downloaded astronaut candidates — 2026-10-03

Three downloaded humanoid candidates, editable rigs and local renders are in
[USER_IO/astronaut_vis](../../../USER_IO/astronaut_vis/README.md). This is an
asset comparison; the game continues to render its procedural character.

| Candidate | Prepared geometry | Skeleton | Preparation |
| --- | ---: | ---: | --- |
| AstroDev / Fernando Ferreira | 1,442 triangles | 25 bones | Remove integrated backpack, cap torso, convert original PSD palette; explicit leg-root mapping needed. |
| Polygonal Mind: Astronaut #048 | 3,478 triangles | 58 bones | No backpack; retain humanoid limb chains and original painted appearance. |
| Polygonal Mind: Square Cosmonaut #111 | 4,506 triangles | 78 bones | Remove disconnected backpack; retain humanoid hip, knee and ankle chains. |

![Locally rendered astronaut candidates](../../../USER_IO/astronaut_vis/comparison.png)

The delivered versions have clear backs for the project's own jetpack. Raw
sources are retained separately; two originals include backpacks. Each
prepared Blender file embeds its texture. The GLBs contain their own mesh,
skin, texture and a named `InspectionWalk` clip. The models retain their
original colors; normalization sets a 2 m height, common facing and studio
roughness. Limb geometry is preserved. Four skin weights per vertex are
selected and normalized before both export and deformation checks.

Square Cosmonaut is the closest of these candidates to a slim suit, with
standard humanoid retargeting names. AstroDev is a compact white/gold suit.
Astronaut #048 is more cartoon-like. These accessible stylized candidates are
less detailed than Ava Turing. Sketchfab's official download endpoints still
require authentication, and the supplied Ava model is not downloadable.
Public creator downloads supplied these alternatives without an account.

## Animation evidence

The source files were inspected rather than inferring rig quality from tags.
Each delivered rig has a connected thigh–shin–foot chain on both sides, skin
weights and an editable neutral pose. A newly authored one-second FK cycle
tests knee bending and ankle compensation. The prepared models are sampled
at 31 poses each, checking finite and bounded deformation and moving ankles.
The exported GLBs are then imported again and sampled at 17 poses each,
checking normalized weights, retained chains, deformation and loop closure.
The front, rear and quarter-cycle render of each model comes from its actual
GLB. These checks establish that the assets can deform for walking, not the
naturalness of a production gait.

The inspection loop is not a terrain contact solver or a 6/12 m/s locomotion
animation. Integrating the chosen model requires skinning/material import,
bone mapping and base animation, followed by the existing planted-foot IK.
AstroDev's legs start in separate root branches; a common actor transform
must move those branches together when adapting the pelvis.

## Source and validation records

AstroDev's [original listing](https://opengameart.org/content/low-polymobile-astronaut-rigged-and-blend)
declares CC0 1.0. Polygonal Mind's
[100Avatars files](https://github.com/PolygonalMind/100Avatars)
retain the creator's [CC BY 4.0 repository license](https://github.com/PolygonalMind/100Avatars/blob/master/CCLicense.md),
credits and modification notes, despite newer releases and VRM metadata
declaring CC0. Source records pin the downloaded revision and SHA-256 hashes.
Original model licenses remain separate from the application code license.

[The manifest](../../../USER_IO/astronaut_vis/manifest.json) records geometry,
skin counts, imported pose checks and artifact hashes. [Validation logs](../../../USER_IO/astronaut_vis/validation/)
retain preparation, rendering and independent GLB/PNG checks. Reproduction
uses `scripts/character/prepare_models.py`, `render_models.py` and
`check_models.py`; the output README gives the complete commands and credits.
Blender, NumPy and archive tools are optional asset-study dependencies.

The game C++ sources, shaders, production scene and the 22-image game gallery
were untouched. Validation for this task is the asset pipeline, hash checks,
repository layout and rebuilt journal PDF; the previous game checkpoint's
56-entry CTest result remains its own evidence.

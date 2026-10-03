# Astronaut candidates

Three downloaded, skinned humanoid models are prepared for comparison. Open
the PNGs below, or import each model's `astronaut.glb` into Blender or a glTF
viewer. The matching `astronaut.blend` has packed textures and an editable rig.
All three delivered versions have clear backs for a separately mounted jetpack.

![Three astronaut candidates](comparison.png)

| Candidate | Prepared triangles | Rig | License | Front | Back | Walking pose |
| --- | ---: | --- | --- | --- | --- | --- |
| AstroDev / Fernando Ferreira | 1,442 | 25 bones; separate leg roots | CC0 1.0 | [View](astrodev.png) | [View](views/astrodev-back.png) | [View](views/astrodev-walk.png) |
| Polygonal Mind: Astronaut #048 | 3,478 | 58 bones; humanoid hip/knee/ankle chains | CC BY 4.0 retained | [View](polygonal-astronaut.png) | [View](views/polygonal-astronaut-back.png) | [View](views/polygonal-astronaut-walk.png) |
| Polygonal Mind: Square Cosmonaut #111 | 4,506 | 78 bones; humanoid hip/knee/ankle chains | CC BY 4.0 retained | [View](polygonal-cosmonaut.png) | [View](views/polygonal-cosmonaut-back.png) | [View](views/polygonal-cosmonaut-walk.png) |

Square Cosmonaut offers the slimmest suit and a familiar humanoid bone map.
AstroDev is a lightweight conventional white suit with a gold visor; its
multiple leg roots need an explicit mapping when retargeting. Astronaut #048
is the most cartoon-like option. These are stylized game candidates; they
have less surface detail than the unavailable Ava Turing reference.

## Files and preparation

Each folder in [models/](models/) retains the downloaded source, texture,
creator/license record, editable prepared Blender file, self-contained GLB
and inspection report. The original AstroDev source refers to a PSD palette;
the prepared files use its converted PNG and embed that image. Original files
retain their original geometry, including backpacks where present.

The **prepared** AstroDev model removes the integrated backpack, closes the
torso and uses the original white palette on the new faces. Square Cosmonaut
removes its disconnected backpack. Astronaut #048 had no backpack. Preparation
keeps limb geometry, normalizes height to 2 m, gives all models a common facing
direction, retains four normalized skin weights per vertex and uses a common
roughness for studio comparison. Original suit colors are retained.

`InspectionWalk` is a new one-second FK loop made for this study. It bends
both thighs, knees and ankles and demonstrates skin deformation. It is an
inspection animation, not a finished walking controller: terrain contacts,
stance locking, sprint/jump clips and integration with the game's ozz IK still
need implementation after a model is chosen. The runtime currently uses the
procedural astronaut.

The actual GLBs were imported again and sampled through the loop before
rendering nine views at 900×1080, using Blender 3.4.1 Eevee, 48 samples and Mesa
llvmpipe with eight workers. Validation checks leg chains, normalized weights, finite deformations,
loop closure, packed textures, source hashes and the saved PNGs. Full results
and artifact hashes are in [manifest.json](manifest.json) and [validation/](validation/).

## Creator credits

**Low Poly/Mobile Astronaut** by AstroDev, Fernando Ferreira (`zisongbr`),
downloaded from [OpenGameArt](https://opengameart.org/content/low-polymobile-astronaut-rigged-and-blend),
released under [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/).
Changes are described above and in its inspection report.

**Astronaut #048** and **Square Cosmonaut #111** by Polygonal Mind, downloaded
from the creator's [100Avatars repository](https://github.com/PolygonalMind/100Avatars).
The retained [repository license](https://github.com/PolygonalMind/100Avatars/blob/master/CCLicense.md)
is [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). Newer releases and
VRM metadata declare CC0; these copies keep the repository's attribution,
license and modification records. Each `source.json` pins the downloaded
repository revision and file hashes. These asset licenses remain separate
from the application's code license.

## Reproduce

The optional tools are `blender`, `python3-numpy`, `libarchive-tools` and
ImageMagick. Run from the repository root with the retained source files:

```sh
PYTHONPATH=/usr/lib/python3/dist-packages blender -b --disable-autoexec --python-exit-code 1 -t 2 -P scripts/character/prepare_models.py
for candidate in astrodev polygonal-astronaut polygonal-cosmonaut; do
  PYTHONPATH=/usr/lib/python3/dist-packages LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=8 xvfb-run -a blender -b --disable-autoexec --python-exit-code 1 -t 8 -P scripts/character/render_models.py -- --model "$candidate"
done
MAGICK_THREAD_LIMIT=1 convert USER_IO/astronaut_vis/astrodev.png USER_IO/astronaut_vis/polygonal-astronaut.png USER_IO/astronaut_vis/polygonal-cosmonaut.png +append USER_IO/astronaut_vis/comparison.png
python3 scripts/character/check_models.py --record
python3 scripts/character/check_models.py
```

The scripts disable downloaded Python execution. The renderer includes the
NumPy alias compatibility required by Debian Blender 3.4's glTF importer.
Each candidate renders in a separate process. `--views back walk` can resume
selected views; `--audit-only` repeats imported deformation checks without rendering.
Blender's unavailable optional Draco encoder does not affect these
uncompressed, self-contained GLBs.

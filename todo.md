# next to do's

document the progress and add comments such that when interrupted you can continue right where you left of
document checkpoints in USER_IO/checkpoints.md the User might move also finished todo items there.
# States: 
|State |Meaning|
|:--|--:|
|"-" or " "| not yet started|
|"p"| in progress|
|"i"| implemented|
|"t"|automated tests implemented and passed; documented; may proceed to next task|
|"v"|verified by the user|


## Remaining requested features

|Task |State|Comment|
|:--|:--:|:--|
|Refresh CPU/GPU timings and optimize toward sustained 60 FPS independently of display refresh.|t|Bounded pass complete: --uncapped added; ridge planning 8–23% faster with unchanged geometry and 12 exact image pairs; 41 test groups pass. Quadro remains above the 16.67 ms budget; RTX warm offscreen rendering fits, with streaming/reload spikes reported separately. Report: docs/journal/benchmarks/fps-60/study.md. Raw data stays local in ignored build-f5/fps-60; no quality cuts.|
|We can move out of the grass foliage area bfore the new grass is spawned, how can we fix that?|t|Latency-aware refresh and bounded terrain-height lookahead implemented. Six native Quadro production routes, including fast steep traversal, remain within the unchanged 22.5 m reserve; failing mountain max improves from 36.53 to 16.23 m with zero excess frames. Held-worker, stop/reverse, actual-root, reload, flight/input and exact replay/CPU/GL 3.3 checks pass. Production camera-centered protected density measures 120.82–124.31/m² (configured 120.72); existing adaptive far falloff and infeasibility diagnostics remain. Source/tests and report updated; raw data stays ignored/local. No commit. Report: docs/journal/architecture/terrain-gpu/grass-refresh/study.md.|
|If not already imolemented, adaptive centimeter-scale mountain geometry with GPU noise displacement and smooth sinking transitions.|p|Relief-aware sinking/filtering and curvature refinement implemented. Opt-in local physical edge/error targets and meter-based noise now implemented with topology v3 and matching small CPU/GPU normal probes. Stationary production tests: 0.5 m disk meets 5 cm edges at startup and mountain; 1.5 m mountain meets them, startup reports a cap-limited 5.67 cm edge. 410 independent float-mesh probes per position show at most 0.153 mm error; no open/reversed triangles. Production defaults remain unchanged: 1.5–3.1 s CPU planning cannot continuously cover a moving centimeter patch. Exact parent-triangle geomorph, gradual publication and reusable/streamed moving detail remain outstanding in this row. Report and configuration example: docs/journal/architecture/terrain-gpu/sinking.md.|
|The current Astronaut Models are not suitable , i want a more realistic modern looking high poly version. Remove te current Astronauts and give me 5 other examples. |p|Rejected low-poly preview assets removed; five denser NASA mesh studies rendered from actual GLBs (15 full-body/rear/detail views), with source hashes and geometry checks. These files are unrigged and Gemini is historical, so this does not complete the modern animation-ready shortlist. Official previews/credits for four rigged alternatives are included; their downloads require sign-in before deformation/export/backpack checks. See USER_IO/astronaut_vis/README.md. No new task chain.|
|Fade grass trails smoothly back to their normal wind motion near history eviction. As the bounded history fills, progressively reduce deformation through the oldest 30% of retained trail segments so old marks disappear without a sudden pop. Even when fresh the movement shall be 80% pressed down, and 20% percent wind movement, such that even when pressed down it still moves a tiny bit. | |Queued on user request. Preserve normal wind phase and fixed roots; use a continuous history-age weight shared by both GPU paths and reflections, and retain the weights/state in replay.|
|Evaluate BSON storage for long trails and expensive computed results. Profile the most time-consuming calculations and compare recomputation with serialization, storage size and read/write cost; identify which results actually benefit from persistence. Only do evaluation here, maybe document in docs/...| |Measure candidates such as trail history and terrain/atmosphere caches before choosing what to store. Document dependencies, cache keys, ownership and invalidation; avoid persisting cheap results without a measured benefit.|
|Implement beneficial BSON persistence selected by the storage evaluation, with explicit application/cache versions and steadily maintained program version increments. Erase or rebuild cached storage when a newer/incompatible program version invalidates it so stored results remain consistent.| |Depends on the storage evaluation. Validate version, schema, scene/noise/quality inputs and relevant backend dependencies; handle missing/corrupt files and interrupted writes safely. Document the version-bump policy and test cache invalidation and exact restored results.|
|Make the Jetpack emit a fire coming out the back, with modulation based on how long it runs and speed of the astronaut relative to the local planets atmosphere (first some smoke, then when faster make a blueish flame) shader with some particle smoke coming out the back that slowly diffuses into the amosphere based on the wind.|||
|Lets create a shader for the Sand around the water, that creates actually noise, based on the wind that looks like the image USER_IO/user_artifacts/image copy 2.png  |||

### Proposed scope: centimeter-scale terrain

Completed sinking work implements relief-aware sinking and smooth blending of
existing noise toward filtered parent fields. Opt-in centimeter subdivision and
meter-based noise are implemented; exact parent-triangle projection and a
continuously streamed moving detail patch remain outstanding.

- Design recorded in docs/journal/architecture/terrain-gpu/sinking.md: bounded
  shared-edge refinement uses the existing pipeline. CPU handles patch selection,
  shared-edge topology and sparse character contacts; GPU compute evaluates bulk
  displacement and normals. Preserve CPU/GL 3.3 support and avoid planet-wide
  centimeter subdivision or an unrelated renderer rewrite.
- Implemented opt-in targets: 2–5 cm edges within a few meters of the camera and approximately
  1 cm height accuracy against the selected procedural surface. Coarsen with
  distance and refine visible mountain ridges according to projected geometric
  error. Bound triangle count and live/staged/retiring memory; report when the
  requested detail cannot fit rather than claiming the target was achieved.
- Implemented planet-fixed noise configured by wavelength/amplitude in meters; initial
  rock relief could use 10–30 cm wavelengths and 1–3 cm amplitudes. Resolve each
  geometric wavelength with several samples, fade unresolved frequencies toward
  coarser levels and retain subpixel material relief. Normal-sampling steps now
  follow the shortest active physical wavelength (at most 5% of it).
- Keep the finest nearby surface unsunk. Morph geometry and noise toward the
  parent surface across LOD transitions, scaling coarse sinking with omitted
  relief/error. Shared boundary samples must agree; sinking alone does not close
  cracks or remove coarse silhouette edges.
- Verify actual geometry/silhouettes, height error, watertight transitions and
  walking/sprint stability. Version-3 surface captures retain planning anchors;
  advancing-time capture clipping now matches interactive rendering. Grass roots,
  sparse contacts, shadows and reflections
  must agree with the displaced surface. Preserve deterministic replay and reload
  ownership; record bounded frame-cost/memory results using existing tools, with
  raw output local. Remaining refinement work stays queued until requested.


# Environment dependency added for the journal preflight figure

The standalone plot generator uses optional `python3-matplotlib` (installed for
T3c5c1; restored for T3c5c2b2a inspection figures). Add it to the container dependency list to regenerate the SVG/PNG;
it is not needed by the game runtime.

```sh
apt-get update
apt-get install -y --no-install-recommends python3-matplotlib
```

# environmental issues :
current apt get has the following dependencies 
    git \
    curl \
    ca-certificates \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    libglfw3-dev \
    libglew-dev \
    libglm-dev \
    libgtest-dev \
    xvfb \
    mesa-utils \
    xauth \
    libgl1-mesa-dri \
    libglx-mesa0 \
    xdotool \
    imagemagick \
    ffmpeg 
if environmental changes are needed inside the container, let the user know by adding dependencies here as a code listing such that its easy to copy and paste:

- PNG writing requires `libpng-dev` (already available here).
- Headless input/capture checks use `xvfb`, `xauth`, `xdotool`, and `imagemagick`.
- Function-level CPU instruction profiling uses optional `valgrind` (installed
  for the CPU study; apt also installs `libc6-dbg`). It is not a build/runtime
  dependency. The existing GNU gprofng probe reported an unreliable timer.
- Astronaut candidate inspection, glTF conversion and preview renders use
  optional Blender 4.5.4 LTS (bundled NumPy/Draco) and ImageMagick, not game
  dependencies. Debian Blender 3.4 is insufficient for these NASA material/image
  imports. The portable Blender installation stays local in build-f5/astronaut-next.

```sh
apt-get update
apt-get install --no-install-recommends -y valgrind xdotool imagemagick blender python3-numpy libarchive-tools
```

The earlier CPU-profiling host lacked `xdotool` and ImageMagick (`import`,
`convert`), so its CMake configuration omitted five live X11 tests. The
current terrain-validation container has these tools and registers all 45
tests. The install command above restores them on hosts where they are absent;
reconfigure afterwards. Valgrind is only needed to recollect the independent
instruction profile.

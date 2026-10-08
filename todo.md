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
|Refresh CPU/GPU timings and optimize toward sustained 60 FPS independently of display refresh.| |One bounded profile–optimize–remeasure pass using the existing tools. Benchmark supported CPU and compute renderers at the same documented resolution, production quality (including ridge refinement), startup view and reported mountain camera; include stationary full rendering and walking/sprint, with reload/rebuild stalls reported separately. Disable vsync/frame caps or use uncapped offscreen rendering so monitor refresh cannot cap measured throughput; preserve simulation timing and measure actual rendered frames separately from cached presentation. Record the actual rendering GPU/driver, CPU critical-path time, asynchronous GPU pass timings, presentation waits, terrain planning/generation latency, memory and frame-time median/p95/p99. Target full-render p95 <=16.67 ms with bounded spikes; identify bottlenecks, implement measured improvements and compare before/after without silently reducing resolution, terrain accuracy, protected foliage density or visual features. Preserve contacts, replay, reload and CPU/GL 3.3 fallback. If 60 FPS is infeasible on the tested hardware at that quality, report the measured limit and explicit quality tradeoffs. Update one compact timing report/journal entry; raw traces/archives stay local in ignored build directories.|
|We can move out of the grass foliage area bfore the new grass is spawned, how can we fix that?|||
|If not already imolemented, adaptive centimeter-scale mountain geometry with GPU noise displacement and smooth sinking transitions.|p|Relief-aware sinking/filtering and local surface-error refinement implemented. Standard config uses geometric_error_m=0.05: shared-edge curvature probes prioritize ridges within the existing 100k cap; longest-edge propagation preserves triangle quality. At the reported mountain camera, 2,821 fixed probes improve mean error from 64 cm to 6 cm and worst error from 8.9 m to 26 cm. This remains budget limited, without a centimeter accuracy guarantee. Report: docs/journal/architecture/terrain-gpu/sinking.md. Explicit wavelength controls, a guaranteed centimeter-local mesh and exact parent-triangle geomorph remain queued. Scope below.|
|The current Astronaut Models are not suitable , i want a more realistic modern looking high poly version. Remove te current Astronauts and give me 5 other examples. |||
|Fade grass trails smoothly back to their normal wind motion near history eviction. As the bounded history fills, progressively reduce deformation through the oldest 30% of retained trail segments so old marks disappear without a sudden pop. Even when fresh the movement shall be 80% pressed down, and 20% percent wind movement, such that even when pressed down it still moves a tiny bit. | |Queued on user request. Preserve normal wind phase and fixed roots; use a continuous history-age weight shared by both GPU paths and reflections, and retain the weights/state in replay.|
|Evaluate BSON storage for long trails and expensive computed results. Profile the most time-consuming calculations and compare recomputation with serialization, storage size and read/write cost; identify which results actually benefit from persistence. Only do evaluation here, maybe document in docs/...| |Measure candidates such as trail history and terrain/atmosphere caches before choosing what to store. Document dependencies, cache keys, ownership and invalidation; avoid persisting cheap results without a measured benefit.|
|Implement beneficial BSON persistence selected by the storage evaluation, with explicit application/cache versions and steadily maintained program version increments. Erase or rebuild cached storage when a newer/incompatible program version invalidates it so stored results remain consistent.| |Depends on the storage evaluation. Validate version, schema, scene/noise/quality inputs and relevant backend dependencies; handle missing/corrupt files and interrupted writes safely. Document the version-bump policy and test cache invalidation and exact restored results.|
|Make the Jetpack emit a fire coming out the back, with modulation based on how long it runs and speed of the astronaut relative to the local planets atmosphere (first some smoke, then when faster make a blueish flame) shader with some particle smoke coming out the back that slowly diffuses into the amosphere based on the wind.|||
|Lets create a shader for the Sand around the water, that creates actually noise, based on the wind that looks like the image USER_IO/user_artifacts/image copy 2.png  |||

### Proposed scope: centimeter-scale terrain

Completed sinking work implements relief-aware sinking and smooth blending of
existing noise toward filtered parent fields. This does not yet project vertices
onto parent triangles or add centimeter subdivision.

- Before implementation, settle the bounded patch/refinement design and CPU/GPU
  ownership using the existing terrain pipeline. CPU handles patch selection,
  shared-edge topology and sparse character contacts; GPU compute evaluates bulk
  displacement and normals. Preserve CPU/GL 3.3 support and avoid planet-wide
  centimeter subdivision or an unrelated renderer rewrite.
- Start with 2–5 cm edges within a few meters of the camera and approximately
  1 cm height accuracy against the selected procedural surface. Coarsen with
  distance and refine visible mountain ridges according to projected geometric
  error. Bound triangle count and live/staged/retiring memory; report when the
  requested detail cannot fit rather than claiming the target was achieved.
- Add planet-fixed noise configured by wavelength/amplitude in meters; initial
  rock relief could use 10–30 cm wavelengths and 1–3 cm amplitudes. Resolve each
  geometric wavelength with several samples, fade unresolved frequencies toward
  coarser levels and retain subpixel material relief. Replace the roughly 25 cm
  normal-sampling step with gradients suitable for the selected detail scale.
- Keep the finest nearby surface unsunk. Morph geometry and noise toward the
  parent surface across LOD transitions, scaling coarse sinking with omitted
  relief/error. Shared boundary samples must agree; sinking alone does not close
  cracks or remove coarse silhouette edges.
- Verify actual geometry/silhouettes, height error, watertight transitions and
  walking/sprint stability. Grass roots, sparse contacts, shadows and reflections
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
  optional `blender`, `python3-numpy` and `libarchive-tools` (installed for
  this asset study), not game dependencies.

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

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
|If not already imolemented, adaptive centimeter-scale mountain geometry with GPU noise displacement and smooth sinking transitions.|p|Opt-in physical targets/noise and parent-triangle projection are implemented. V4 shares parent adjacency/bounds and a sparse mutable graph. Short near-band rebasing protects the fine core; recorded parents are honored for replay. Shared contact indices reduce warm preparation to 44–48 ms. A complete-plan probe cache removes 75k redundant stationary evaluations and improves wide short-move planning 10–18%; cached fixed-policy sinks save another 7–11 ms in production planning. Per-plan edge-node pooling and reused footprint math improve warm production planning another 7–24% in three alternating pairs. Extra CPU memory and cold/mountain/native tradeoffs are documented; no native FPS gain is claimed. All 12 production geometry fingerprints remain exact. Vertical movement now refreshes/rebases error-aware plans in both directions; resident GPU, async CPU and capture publication tests pass. Twelve detail cases, seven contact cases, all 35 core groups and all 46 RTX compute cases pass; moving replay and CPU/GL 3.3 images match exactly. Native coverage still fails (latest max offsets 1.183/2.745/9.079 m); the 1.5 m fixture also misses geometry targets. Uninterrupted moving detail and temporal publication remain incomplete. Defaults unchanged. Report: docs/journal/architecture/terrain-gpu/sinking.md; helpers: scripts/benchmarks/terrain_detail.|
|The current Astronaut Models are not suitable , i want a more realistic modern looking high poly version. Remove te current Astronauts and give me 5 other examples. |t|User selected NASA ACES. Replaces the procedural runtime suit with all 88,872 original triangles, orange/blue materials, a 13-joint GPU skin driven by existing IK/gait/flight, German arm patches and the functional jetpack. Asset source/credits and regeneration are committed; NASA/rigged shortlist remains historical. CPU skin/invalid-asset, GPU parity, input/reload/recovery, flight/exhaust and exact replay checks pass. Four README views refreshed. See data/character/aces/README.md and docs/journal/character/astronaut.md. No new task chain.|
|G1: Reduce grass allocation spikes through cached terrain metadata and plan reuse.|t|Bounded terrain metadata sharing, equivalent effective-policy reuse and inactive search skips implemented. Three paired RTX walking runs reduce allocation GPU mean 20.6% and CPU submission mean 52.9%; protected density and four exact image comparisons pass. Extra cache costs 64 bytes/triangle; overlap admission remains bounded. GPU oracles/publication, runtime/reload/replay, CPU/GL 3.3, legacy CPU grass and native Quadro held-worker refresh checks pass. Steady frame p95 changes little, ~109 ms maxima remain, and software sprint misses the unchanged reserve. Report: docs/journal/benchmarks/grass-allocation/study.md. Raw evidence stays local; G2–G4 remain queued.|
|G2: Keep persistent GPU grass root records instead of repeating placement each frame.|t|Plan-owned root/normal/variation records and patch bounds generated once before publication; per-view visibility, density, LOD and wind retained. Actual startup cache adds 103 MB; complete overlap admission stays below 1 GiB. Three RTX pairs show similar frame p95 and 1–11% higher placement GPU means, so no FPS gain is claimed. Four exact production image comparisons, cache/invalidation/parity, staging/publication, CPU/GL 3.3, runtime/reload/replay and native Quadro refresh checks pass. Existing NVIDIA trail precision failure also reproduces with G1; software assertion passes unchanged. Report: docs/journal/benchmarks/grass-roots/study.md. Raw evidence stays local; G3/G4 remain queued.|
|G3: Calculate grass wind and trail influence once per blade/frame and share it between views.|t|Plan-owned 32-byte GPU wind/trail samples shared across views, including reflection-only blades; time/history/root invalidation and complete admission implemented. Five exact production image comparisons, cache reuse, staging/publication, CPU/GL 3.3, runtime/reload/replay and native Quadro refresh checks pass. Existing strict NVIDIA trail equality failure also reproduces with frozen G2; software passes unchanged. Three RTX astronaut-camera pairs reduce combined compaction/animation GPU means 3–4%, but wall p95 rises 3–4%, so no FPS gain is claimed. Actual startup animation cache adds 51 MB; overlap remains below 1 GiB. Report: docs/journal/benchmarks/grass-animation/study.md. Raw evidence stays local; G4 and trail-fade changes remain queued.|
|G4: Update persistent grass cells by delta as the camera moves.|t|Canonical planet-fixed triangle cells retain immutable blade prefixes, regenerate entering/dirty/appended slots and retire through existing fences. Preparation is sliced into four 65,536-slot dispatches per poll with bounded source leases and complete memory admission. Nine measured updates copy 87.06% of roots; CPU submission is distributed, but root GPU time rises and 107.6 ms walking spikes remain, so no sustained FPS gain is claimed. Four legacy image pairs and fresh replay match exactly; CPU core, GPU delta/staging/publication, lifecycle and native Quadro held-worker sprint/camera/fallback checks pass. Peak logical overlap 1,001,030,344 bytes stays below 1 GiB. Report: docs/journal/benchmarks/grass-cells/study.md. Raw evidence stays local.|
|Fade grass trails smoothly back to their normal wind motion near history eviction. As the bounded history fills, progressively reduce deformation through the oldest 30% of retained trail segments so old marks disappear without a sudden pop. Even when fresh the movement shall be 80% pressed down, and 20% percent wind movement, such that even when pressed down it still moves a tiny bit. | |Queued on user request. Preserve normal wind phase and fixed roots; use a continuous history-age weight shared by both GPU paths and reflections, and retain the weights/state in replay.|
|Evaluate BSON storage for long trails and expensive computed results. Profile the most time-consuming calculations and compare recomputation with serialization, storage size and read/write cost; identify which results actually benefit from persistence. Only do evaluation here, maybe document in docs/...| |Measure candidates such as trail history and terrain/atmosphere caches before choosing what to store. Document dependencies, cache keys, ownership and invalidation; avoid persisting cheap results without a measured benefit.|
|Implement beneficial BSON persistence selected by the storage evaluation, with explicit application/cache versions and steadily maintained program version increments. Erase or rebuild cached storage when a newer/incompatible program version invalidates it so stored results remain consistent.| |Depends on the storage evaluation. Validate version, schema, scene/noise/quality inputs and relevant backend dependencies; handle missing/corrupt files and interrupted writes safely. Document the version-bump policy and test cache invalidation and exact restored results.|
|Make the Jetpack emit a fire coming out the back, with modulation based on how long it runs and speed of the astronaut relative to the local planets atmosphere (first some smoke, then when faster make a blueish flame) shader with some particle smoke coming out the back that slowly diffuses into the amosphere based on the wind.|||
|Lets create a shader for the Sand around the water, that creates actually noise, based on the wind that looks like the image USER_IO/user_artifacts/image copy 2.png  |||

### Proposed scope: centimeter-scale terrain

Completed sinking work implements relief-aware sinking and smooth blending of
existing noise toward filtered parent fields. Opt-in centimeter subdivision,
meter-based noise and exact parent-triangle projection are implemented;
uninterrupted moving detail and temporal publication remain outstanding.

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
  raw output local. The terrain row remains in progress until its moving coverage
  and transition checks pass.


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

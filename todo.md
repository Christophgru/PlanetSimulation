# next to do's

document the progress and add comments such that when interrupted you can continue right where you left of

# States: 
|State |Meaning|
|:--|--:|
|"-" or " "| not yet started|
|"p"| in progress|
|"i"| implemented|
|"t"|automated tests implemented and passed; documented; may proceed to next task|
|"v"|verified by the user|


|Task |State|Comment|
|:--:|:--:|:--:|
|Make foliage movement independant of planetary movement, such that if pressed t only the planet movement stop, but local grass movement keeps going|t| Wall-clock foliage time is independent of T pause and Y/U orbital speed. Added native GLFW test with completed-transition readiness, visible-grass guard, changing paused-time frames, zero-speed freeze and paused reload. Clean 50/50 CTest entries pass in 333.81 s; native images/CSV/evidence, README and journal/PDF updated. |
|The quick grass demo mentioned something like 6 poly grass if close and 1 pol grass if far away, is this already active? if not add it.|t| Already active: six-segment strips (six quads, 14 vertices/12 triangles) close and one tapered quad (4 vertices/2 triangles) far. Configurable transition, fixed roots, no hot-swap upload and matching fragment shading validated on both GPU paths. Clean 50/50 suite passes; README and procedural-grass journal explain polygon accounting. |
|Lets add a render Mode, where rendering might take arbitrary long but foliage is generated for a radius up to 20x the normal distance, high poly is loaded for the moon and sun, and lens flaring is simulated. |t| Implemented --offline-render/--offline-quality: 1–20x single-layer radius, bounded candidate growth, full-resolution atmosphere, finer Moon/Sun meshes and visible-Sun lens flare. Exact replay, invalid replay rejection, CPU bounds and both GPU root paths pass. Clean 51/51 CTest run (308.44 s); journal/PDF and 21 gallery images refreshed, inspected and hashes/fingerprint verified. |
|Lets give the grass (ground, not foliage ) a more brown and grey tinted color, such that it looks more like the foliage.|t| Superseded by the later user correction to match foliage tips. Ground shares midpoint tip albedo (linear RGB 0.634375, 0.74375, 0.284375), with grain and slope shading. Neutral-light GPU channel-ratio and beach-band regression passes in clean 51/51 suite; journal documents the correction and 21 gallery captures are refreshed/verified. |
|Lets adjust the foliage wind noise such that the grass stays pressed down on the trail where the small astronaut walked along, add jumping motion, that lets the astronaut jump higher or lower depending on the planets gravity, based on mass and diameter of the planet minus its rotation velocity.|t| Gravity-dependent jumping and persistent trails implemented. Body-local bounded history, fixed roots, wind suppression and slope conformance work on both GPU paths; complete trail and cached planning anchors replay exactly. All 52 CTest entries have passing results via full run plus final renderer/layout checks; 22 gallery images, journal/PDF and retained evidence updated. |
|Make the Sand more white and add some granularity to it (foliage or roughness/reflection map maybe in combination with tiny elevation noise that creates tiny dunes (only some cm high, as reference take image USER_IO/user_artifacts/image copy 2.png) that are often visible in sand in windy areas)|t| Pale granular sand with filtered, planet-fixed 18 cm wind ripples and bounded 1.4 cm normal relief; no extra triangles or collision changes. Clean 52/52 CTest suite (530.86 s); 22 gallery images, shoreline comparison, journal/PDF and capture hashes updated/verified. |
|Prepare a short journal in typst on wether its feasable to port the current application to a web assembly application and evaluate the advantages and disadvantages in a short 2 page memo. Add some graphics on how the compilation process works, what runtime dependencies there are and where the main caviats might lie.|t| Two-page Typst/PDF feasibility memo with compilation/runtime diagrams, audited dependency and renderer changes, threading/storage/HDR caveats, WebGL/WebGPU tradeoffs and acceptance gates. Compiles to exactly two pages; visually reviewed, layout/whitespace and artifact hashes verified. Feasibility assessment only; no browser port or FPS claim. |
|Make the astronaut not move his arms during jetpack phase, also in jetpack, wasd go on acceleration, calculate some upper speed limit of 100m/s for horizontal movement on sea level on the main planet (calc air pessure) but should be the same also on other planets, what power the jetpack would need to have. Falling should also be physically sound and bound by wind resistance. Assume the Astronaut is 100kg and the jetpack can thrust only down, so its thrust must be directed in the direction we want to fly in.  |t| Single-axis tilted thrust, quiet arms, 100 kg rotating-frame gravity and pressure-dependent drag; WASD commands acceleration toward 100 m/s with one main-planet-sized engine on every planet. Reproducible 8.29 kN / conditional 6.91 MW power study. All 52 entries validated via full run (51 passed) plus atmosphere timeout recheck; 19 CPU cases, exact upright/tilted replay, native input, 22 verified gallery images and journal/PDF updated.|
|The Knees of the astronaut are also always bent, make him stand with straigh knees if hes nor walking.|t| Settled standing raises the pelvis over fixed soles; torso/arms/backpack and chase camera follow the replayable offset. Original bone lengths retained; terrain/cliff reach bounded. Clean 8/8 focused checks (133.62 s), including 22 CPU cases, standing/walking/flight/trail exact replay and native input. Two standing gallery views, comparison, journal/PDF and verified capture provenance updated.|
|Make jetpack flight controls fire thrust whenever a movement button is pressed: WASD supplies directional thrust even without Space, Space supplies upward thrust, and looking down while pressing W allows descent. Keep orientation aligned to the nearby planet within 1.2 body diameters; outside that boundary switch to free outer-space orientation, allow flight to the Moon, and align to the destination body when entering its 1.2-diameter boundary. In outer-space mode always calculate gravity from the three closest celestial bodies.|t|Airborne WASD ignites directional thrust; W follows full look for descent, Space is up. World SI motion/exhaust axis, smooth local/free-space/Moon frames at centre distance 2.4 radii, closest-three gravity and moving-air drag. Clean 53/53 CTest entries pass (764.20 s), including 41 CPU cases, exact space/Moon/settled/steered-flight replay and native WASD-only ignition. Jetpack gallery view, replay evidence, journal/PDF and all 22 gallery hashes/provenance updated. See docs/journal/character/space-flight.md.|
|Make the Bubbles of the Astronaut not just appear all at once, but physically sound, with a lifetime, that slowly fades, make sure the bubbles are look through, but still have some reflection, but make it performant. Also the bubbles and the astronaut shall be influenced by the wind of the grass.|t|Incremental world-space exhaust with 1.4 s lifetimes, release tails, transparent Fresnel shells and Sun highlights; shared grass wind affects particles and atmospheric astronaut drag. Pool capped at 64, one instanced draw per view, complete history/clock replay. Clean 56/56 CTest entries pass (675.37 s), including 10 new CPU cases and 120 production GLSL wind comparisons. Lifetime/plume captures, refreshed jetpack gallery, journal/PDF and all 22 gallery hashes verified. See docs/journal/character/exhaust.md.|
|Download three diffrent high quality astronaut models, and make a render of each and drop it into USER_IO/astronaut_vis. Only choose models that we can actually use for walking motions and that have no backpack, such that we can mount a jetpack ourself|||
|Improve the offline / raytracing example in README.md: increase foliage count and use the initial surface position from the production config; consider rendering narrower vertical columns with higher terrain/foliage budgets and assembling them into the final image; make lens flare visibly apparent. Start after the preceding tasks are finished.| |Queued on user request; evaluate strip projection, overlap and image assembly while preserving full-frame lighting/exposure and flare placement. No renderer changes for this task yet.|
|Form a terrain/atmosphere CPU–GPU implementation plan before starting GPU terrain work. Audit the existing pipeline and evaluate hexagonal planet panels, seams/poles and alternatives. Define CPU ownership of subdivision/LOD/sinking and GPU ownership of noise, surface shape and atmospheric fields, including ground contacts, shadows/reflections, supported GPU paths and validation/performance budgets.| |Planning prerequisite for the following terrain/atmosphere tasks. Document CPU/GPU ownership before implementation, and assess seams, poles and spherical exceptions for hexagonal panels.|
|Implement highly optimized GPU terrain shaping according to the completed CPU–GPU plan. Pass noise/shape parameters and minimal panel/topology descriptors; calculate surface positions/normals/material inputs on the GPU. Keep CPU subdivision and sinking where the plan calls for them.| |Depends on the architecture plan. Preserve planetary coordinates, camera/astronaut contacts, water/shadow/reflection agreement, deterministic replay and a documented compatible fallback; measure transfers and generation costs.|
|Implement the planned GPU atmosphere calculations and remaining planet field work. Reuse calculations already on the GPU and move remaining suitable per-sample work there, passing parameters rather than generated field arrays.| |Depends on the architecture plan and relevant terrain interfaces. Validate optical appearance, atmosphere/terrain boundary agreement and measured CPU/GPU cost.|
|Adapt foliage allocation to measured GPU usage and available VRAM. Preserve the configured density near the planet camera; automatically choose the falloff from required density, maximum foliage distance, other foliage parameters and remaining memory/render budget.| |Plan the budgeting policy before implementation. Give near-camera density priority, account for terrain/atmosphere/reflections and other GPU allocations, use stable bounded adjustments and predictable fallbacks when usage/VRAM telemetry is unavailable, and validate density/coverage and memory bounds.|
|Fade grass trails smoothly back to their normal wind motion near history eviction. As the bounded history fills, progressively reduce deformation through the oldest 10% of retained trail segments so old marks disappear without a sudden pop.| |Queued on user request. Preserve normal wind phase and fixed roots; use a continuous history-age weight shared by both GPU paths and reflections, and retain the weights/state in replay.|
|Evaluate BSON storage for long trails and expensive computed results. Profile the most time-consuming calculations and compare recomputation with serialization, storage size and read/write cost; identify which results actually benefit from persistence.| |Measure candidates such as trail history and terrain/atmosphere caches before choosing what to store. Document dependencies, cache keys, ownership and invalidation; avoid persisting cheap results without a measured benefit.|
|Implement beneficial BSON persistence selected by the storage evaluation, with explicit application/cache versions and steadily maintained program version increments. Erase or rebuild cached storage when a newer/incompatible program version invalidates it so stored results remain consistent.| |Depends on the storage evaluation. Validate version, schema, scene/noise/quality inputs and relevant backend dependencies; handle missing/corrupt files and interrupted writes safely. Document the version-bump policy and test cache invalidation and exact restored results.|
|Lets create a shader for the Sand around the water, that creates actually noise  |||


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

```sh
apt-get update
apt-get install --no-install-recommends -y valgrind xdotool imagemagick
```

The earlier CPU-profiling host lacked `xdotool` and ImageMagick (`import`,
`convert`), so its CMake configuration omitted five live X11 tests. The
current terrain-validation container has these tools and registers all 45
tests. The install command above restores them on hosts where they are absent;
reconfigure afterwards. Valgrind is only needed to recollect the independent
instruction profile.

## Exhaust and wind checkpoint — 2026-10-03

- Bubble row is t. Bounded world-space pool emits 40/s from alternating
  nozzles, retains release tails and fades over 1.4 s. One sorted instanced
  billboard draw per view renders transparent Fresnel shells and Sun gloss;
  main drawing follows water, reflections share the same non-mutating pool.
- Shared CPU wind sampler matches grass Perlin hash/gradients, seed,
  frequencies and independent local clock. Actor drag and particle drift use
  moving air plus that wind, weighted by gas density. Ground contacts stay
  planted. Pool clocks account for camera-mode gaps; body spin/translation
  sampling is cached per particle step. Antipodal emitter turns stay finite.
- CPU: 41 existing character + 10 exhaust/wind cases pass. GPU transparency,
  HDR specular/depth/state preservation and 120 production GLSL noise samples
  pass. The clean full run passes all 56 CTest entries in 675.37 s, including
  burn/release/retirement exact PNG and complete effect/pose replay, native
  controls, paused wind, camera switching and reload.
- Fixture corrections: enabled gas in the wind test, used RGBA16F for HDR
  specular testing, and scheduled boost after jump per the existing CLI.
- Final binary build: `build-resume/bubbles-sampling-build.log`; focused CPU
  and GPU logs: `bubbles-cpu-final.log`, `bubbles-gpu-final-verified.log`.
  Full suite: `build-resume/bubbles-full-tests.log`; published logs and
  fingerprints: `docs/journal/character/exhaust/validation/`.
- Publication script completed: `build-resume/publish-exhaust.py`. Lifetime
  snapshots and downward plume views retain exact replay; one jetpack gallery
  view is refreshed and visually reviewed, other 21 retain their provenance.
  All 22 gallery hashes, source/binary fingerprints and folder limits verified.
- Explanation: `docs/journal/character/exhaust.md`; README and current
  astronaut/flight notes updated. Typst PDF rebuilt; pages 20–24 visually
  reviewed, including the new plume comparison and evaluation evidence.
  Next is the queued three-model download/render task. Production
  configuration is unchanged.

## Space-flight checkpoint — 2026-10-02

- Completed directional-only airborne ignition, full-look W/S including
  descent and Space up; grounded walking/sprinting remains 6/12 m/s.
  New component: `src/rendering/character/flight/FlightNavigation.{h,cpp}`.
- World SI position/velocity and exhaust axis persist independently of body
  motion. Full wall-time flight uses 10 ms substeps and sampled translation/
  spin. Gravity sums the three nearest centres, including the Sun.
- Planet/Moon up applies within centre distance 1.2 diameters = 2.4 radii;
  overlap hysteresis and smooth alignment preserve world state. Space pitch
  is unrestricted, and Moon handoff updates terrain, camera and lighting.
- Exact replay retains inertial view, all-body terrain planning anchors and
  LOD history. Fixed a one-pixel Moon replay mismatch by recomputing clipping
  from the actual chase eye, matching interactive flight.
- Clean full 53/53 CTest run passes in 764.20 s. The character CPU target has
  41 cases; space, Moon handoff, settled Moon up, standing/gait/steered-flight
  and grass-trail images/state replay exactly. Native input verifies airborne
  WASD ignition without Space, switching and reload.
- Fresh jetpack gallery view and space/Moon checkpoints visually reviewed;
  all 22 gallery hashes match. Other 21 images retain original provenance.
  Main Typst PDF rebuilt after publication; pages 20–22 reviewed.
- Explanation: `docs/journal/character/space-flight.md`; images, replay inputs,
  build/test logs and artifact/source/binary hashes: its `space-flight/` tree.
- Commit this completed row before starting the next bubble lifetime,
  transparency/reflection/performance and grass-wind coupling task. Preserve
  the user's unstaged history/model/bubble/sand TODO edits.

## Standing checkpoint — 2026-10-02

- Completed the straight-idle-knees row. Settled grounded poses raise the
  pelvis over fixed soles; suit, arms/backpack and chase camera share a
  replayable suit-local offset. Bone lengths and 6/12 m/s walking remain.
  Uneven ground preserves needed bend; cliff contacts cannot sink the torso.
- Clean final 8/8 focused checks pass in 133.62 s, with 22 CPU motion cases,
  HDR/reflection standing/gait/flight/trail replay, lifecycle and native input.
  This is scoped validation, not a fresh full 52-entry run. An earlier capture
  sequence exceeded 180 s; bounded allowance is now 300 s, final pass 115.28 s.
- Two standing gallery examples freshly posed/rendered and visually reviewed;
  all 22 PNG hashes and both new per-image source fingerprints verified. Other
  20 captures retain their original provenance. Legacy front pose replays
  byte-identically. Journal PDF rebuilt after publication and reviewed.
- Comparison, replays and logs: `docs/journal/character/standing/`;
  explanation: `docs/journal/character/standing.md`.
- User's next jetpack task is queued: directional input fires without Space,
  W follows the full look direction (including descent), Space is up; align
  locally within centre distance 1.2 diameters (2.4 radii), free flight outside,
  Moon/destination alignment on approach, gravity from three nearest bodies.
  Plan continuous world velocity, overlapping influence regions and frame
  transitions before implementation. These new flight controls are not yet built.
- Commit standing work before starting that flight row. Preserve other queued
  astronaut/model tasks, user history reordering and the added sand-noise row.

## Jetpack checkpoint — 2026-10-02

- Completed directed single-axis thrust, quiet arms, acceleration controls and
  the 100 kg rotating-frame force/drag model. Shared hardware is sized from the
  main planet; 100 m/s is the horizontal command target, with no velocity clamp.
  Ground walking remains 6 m/s and Shift sprinting 12 m/s.
- All 52 entries have passing results through full run plus targeted recheck:
  51 passed in 814.46 s; atmosphere scenarios timed out at 180 s after eight
  scenes. A bounded 300 s allowance rerun passed in 152.20 s, with unchanged
  application binary and assertions. This is not a second clean full run.
- 19 CPU cases pass, including actual vacuum falling beyond 50 m/s. Upright
  and tilted flight replay byte-identically; native input also passes. Camera
  chase uses its transported direction consistently in original and replay.
- Force/power study: `docs/journal/character/flight.md`. Frozen production
  inputs give 8.29 kN maximum thrust and 6.91 MW at assumed 1,000 m/s exhaust /
  60% efficiency. Propellant depletion and grass-wind coupling remain future.
- All 22 gallery images refreshed, visually reviewed and hashes/source
  fingerprint verified. Only the tilted-flight PNG changed; it exactly replays
  the study capture. Main journal PDF rebuilt after gallery publication and
  visually reviewed. Evidence: `docs/journal/character/flight/validation/`.
- Commit this feature before proceeding. Next unfinished worktree table item:
  straighten the astronaut's knees when standing still. Preserve other queued
  astronaut/model rows, the user's history reordering and added sand-noise row.

## WebAssembly memo checkpoint — 2026-10-02

- Completed the standalone two-page Typst feasibility memo and compilation /
  runtime diagrams: `docs/journal/portability/wasm.pdf` and `wasm.typ`.
- Audited native v0.0.1 at c464cd3 against primary browser/toolchain docs.
  Both desktop foliage paths need browser changes, including buffer textures;
  compare a WebGL 2 slice with a larger WebGPU renderer. No port is implemented.
- Typst 0.15.1 PDF compilation, exact two-page layout, visual review, repository
  layout and whitespace checks pass; hashes/provenance retained in validation.
  Native source and the renderer fingerprint remain unchanged from the clean
  52/52 sand checkpoint and verified 22-image gallery.
- Next unfinished worktree table item: astronaut jetpack acceleration, directed
  thrust, pressure-dependent drag, falling behaviour and power estimate.

## Sand checkpoint — 2026-10-02

- Pale neutral beach albedo, millimetre grain and triplanar 18 cm wind ripples.
  Bounded 1.4 cm crest-to-trough relief affects normals, not mesh/depth/contact.
  Screen-footprint filtering removes unresolved detail; body-fixed mapping
  handles rotation and poles. Existing shoreline and grass boundaries remain.
- Clean full 52/52 CTest run passes in 530.86 s. Production-shader checks cover
  pale colour, narrow beach, grain/ripple contrast, filtering and exact depth.
- All 22 gallery images refreshed, visually reviewed and hashes/source
  fingerprint verified; journal PDF rebuilt after publication. Frozen
  shoreline comparison and logs: `docs/journal/materials/sand/`.
- Commit this feature before proceeding to the two-page WebAssembly memo.

## TODO continuation checkpoint — 2026-10-02

- Work through the remaining table in order, preserving current scene settings.
  Reused `build-resume` (GCC 12.2, RelWithDebInfo, Xvfb/llvmpipe).
- Fresh full suite passes 49/49 in 286.14 s. Log:
  `build/todo-grass-validation-tests.log`, published under the procedural grass
  study's validation directory. Native astronaut input also passes in this run.
- Closed and committed the superseded horizon/single-layer row and optional
  JSON wind rules; compute/frustum/transfer validation is complete too. The
  October 1 paired benchmark remains historical. Its PNG hashes and payload
  counts were audited; no new hardware timing claim is made.
- Live-wind row complete: native T/speed/reload check and clean full 50/50
  suite pass in 333.81 s. Artifacts: `docs/journal/benchmarks/wind/`.
  Native test waits for two completed surface-camera reports before sampling,
  preventing startup/orbit-to-surface changes from passing as wind motion.
- Six-segment close / one-quad distant geometry confirmed and documented.
- Offline mode complete: clean 51/51 suite in 308.44 s; all 21 gallery PNGs
  regenerated and inspected, hashes/source fingerprint verified; journal PDF
  compiled. Evidence: `docs/journal/benchmarks/offline/`.
  Run `build-resume/PlanetSimulation --offline-render build/offline.png`.
- Ground-color row reconciled with the later foliage-tip palette correction;
  neutral-light channel-ratio/shore regression passes in the clean 51-entry run.
- Grass trail row complete: bounded 2,048-segment body-local history, stackless
  GPU hierarchy, fixed roots, wind suppression and slope-following deformation
  on both placement paths. Replay retains trail plus grass/terrain anchors.
- Validation: full 52-entry run passed 51 in 516.46 s; its layout check waited
  for the new screenshot. Final renderer/capture checks pass in 122.35 s after
  the slope correction; layout passes after gallery publication. All 52 have
  passing results (full run plus targeted checks, not a second clean full run).
- All 22 README images regenerated, inspected and hash/fingerprint verified;
  previous 21 PNGs remain unchanged. Journal PDF compiled. Six-metre trail
  capture records 20 segments and 977 GPU-submitted blades with exact replay.
  Evidence: `docs/journal/character/trails/`.
- Next row: white/granular sand and centimetre dunes. Preserve production
  settings, the user's added astronaut/model rows and TODO reordering.
- Eight newly requested tasks are queued after the previous rows: offline
  density/initial position/column assembly/visible flare; a CPU/GPU architecture
  plan followed by terrain, atmosphere and adaptive foliage budgets; oldest-10%
  trail fading; BSON evaluation and conditional versioned persistence. Form
  the architecture/storage plans before their corresponding implementations.

## Astronaut checkpoint — 2026-10-02

- File and command access restored. Current authorized task is the astronaut
  row. Preserve working scene settings and the existing single grass layer.
- ozz-animation's MIT-licensed foot-IK sample provides two-bone and ankle IK;
  stance locking and step selection still need application logic. Use a
  procedural comic model and locked body-local foot contacts, following the
  selected planet through translation, spin and pole traversal.
- Fresh validation tree: `build-resume`, local GLM/JSON/GoogleTest sources,
  GCC 12.2, RelWithDebInfo. ozz-animation 0.16.0 sources are at
  `build-resume/ozz-src`; CMake now requires 3.24 for upstream compatibility.
- Implemented camera 4, actual rendered-triangle foot contacts, stance locking,
  two-bone IK and chase terrain clearance. Latest user speeds are 6 m/s walking
  and 12 m/s with Shift. Space jumps; release/press while airborne arms the
  jetpack, holding sustains thrust and blue bubbles below the backpack. Grey
  and blue details and small black/red/gold upper-arm patches are visible.
- Focused validation passed four CTest entries in 88.48 s, including 13 CPU
  tests, real HDR/reflection captures, byte-identical gait/flight replay and
  native GLFW input/reload. Logs are in
  `build/resume-astronaut-*.log`, captures in `build-resume/astronaut-captures`.
- Latest user request: Sketchfab search prioritizes animatable rigs with an
  Ava Turing appearance. `docs/journal/character/sketchfab-models.md` records
  the shortlist and public API verification. Ava is not downloadable. Muko_Art
  is the recommended 11,718-triangle rigged candidate (one clip, CC BY 4.0).
  Authenticated download is required; actual skeleton/weights remain unchecked.
  No third-party model has been installed. Preserve the procedural fallback.
- Full run: 48/49 passed in 366.51 s; the new native input screenshot assertion
  failed. Replaced fixed screenshot delay with bounded input/frame observation;
  it passed a targeted run and three consecutive repetitions. The application
  binary stayed unchanged. Evidence/logs are in
  `docs/journal/character/validation/`. This is full-run plus targeted recheck,
  not a second clean full-suite run.
- All 20 gallery images regenerated, visually inspected and matched their
  SHA-256 manifest; renderer/test/script source fingerprint also verified.
  Journal PDF compiled successfully. The procedural astronaut row is t.
  External model replacement needs an authenticated asset download and actual
  skeleton/weight inspection; the search itself is documented and complete.

## Current checkpoint — single procedural grass layer and matching ground color

- Quad transition fixes reproduced in real GL: mismatched interior shading
  and visible-root replacement when candidate slot batches grow. Fragment
  width normalization and stable density ranks with coverage ramps now pass
  both regressions and compute/fallback parity. Cleaning up and documenting,
  then continue with the remaining table items.
- Latest user corrections remove the separate distant tuft layer and every
  foliage distance-layer control. Renderer/planner now live under
  `src/rendering/foliage/procedural`; the single main layer keeps quad LODs.
- Terrain grass uses the shared foliage-tip palette at midpoint variation.
  Beach/snow/seabed retain their colors; terrain biome classification and
  placement are independent of this rendering color correction.
- OpenGL 4.3 compute placement, wind, compaction and indirect draws are retained;
  OpenGL 3.3 falls back to procedural vertex generation. No CPU blade uploads.
  Focused tests, capture checks and documentation are being updated now.


- User correction takes priority: grass sinking is removed;
  terrain sinking remains. Detailed blades use six segments and low blades use
  four-vertex tapered quads with a nonzero top edge. Shared fragment shading
  keeps the palette consistent. Coverage fades replace geometric collapse.
- Grass reuses resident terrain buffers and upload only four-byte triangle
  IDs rather than 40 bytes per individual root. GLSL generates barycentric roots, height, lean, orientation,
  palette variation, Gaussian acceptance, biome rejection and Perlin wind.
  CPU planning reserves a hard candidate budget. Geometry hot-swaps from current
  triangle bounds while cached descriptors stay on the GPU. All of this is
  implemented but still being validated; do not mark t before tests/reports.
- JSON `foliage.wind_noise` exposes gust/direction/flutter frequencies, a seed
  and clock speed. The production CPU placement helper remains only as a
  reference for existing tests/benchmarks.
- Fresh build: `build-grass`, local dependency sources, GCC 13. The managed
  sandbox blocks X11 sockets; use the existing Mesa EGL harness at
  `build/startup-egl-window.so` for shader/rendering checks. Native window/input
  checks cannot run here. Baseline executable/shaders preserved in
  `build-grass/pre-change`. Next: complete focused/full available tests,
  measure descriptor transfer and isolated rendering, refresh documentation
  and artifacts, then proceed in table order.

## Progress checkpoint — 2026-10-01

- User-reported segfault investigation takes priority over horizon foliage.
  No crash reproduced yet with the GCC 12 build: startup, frozen surface
  capture, live WASD/camera switches/reload using both the frozen replay and
  working scenario, and a GDB run with 12 frames/20 m walking steps all exit
  normally. Exact triggering command/action requested from the user. Logs and
  reproduction scripts are in ignored `build-terrain/crash-*` and
  `build-terrain/reproduce_*.py`. `build/PlanetSimulation` was copied from
  another host (its CMake cache points at `/d/Programmieren/...`) and requires
  GLIBC 2.38 / newer libstdc++, so cannot run on this Debian 12 container;
  do not confuse that loader error with a reproduced segfault. GDB 13.1
  installed as an optional debugging dependency. ASan/UBSan build lives in
  `build-crash`, configured against the existing local dependency sources.
  Sanitized four-frame/20 m walking capture and live working-scenario WASD,
  camera switches and reload finish without sanitizer errors. Leak detection
  disabled for these crash probes; ASan/UBSan error checks remain enabled.
  Evidence: `crash-sanitized.log`, `crash-sanitized-live.log`, and
  `crash-sanitized-live-result.log` in `build-terrain`.
  Sanitized terrain/LOD unit tests also pass: 25 tests, 30.31 s, log
  `build-terrain/crash-sanitized-terrain-tests.log`. All rendering probes use
  llvmpipe/Xvfb; native GPU behavior remains unverified. Crash investigation
  needs the user's actual launch/action and output/backtrace before a fix can
  be identified. Horizon task remains p and is not committed as complete.
  Corrected the new feedback test to reattach production vertex source before
  relinking (Shader detaches/deletes its shaders after initial linking), and
  allowed floating-point rounding when checking a barycentric root's plane.
  CPU plan and all 16 GPU tests pass. Lifecycle test passes on rerun after a
  transient GLFW initialization failure; capture test also passes on rerun (82.25 s) after timing
  out during concurrent sanitizer compilation. Do not mark the crash fixed
  without a reproduction or trace identifying its cause.

- Horizon foliage in progress: baseline executable, shaders and frozen grass
  replay copied to `build-terrain/pre-horizon/` before changes. Plan: retain
  the 60 m detailed layer, derive a conservative far radius from eye height
  and maximum terrain relief, and upload coarse triangle descriptors instead
  of individual distant roots. GLSL generates seeds, roots, variation and
  one-triangle tufts; hard candidate/patch budgets and GPU biome rejection
  bound work. Next: implement/test the CPU patch plan, GL renderer and shader,
  verify distant coverage/roots/replays, benchmark fixed workloads without
  concurrent work, update journal/PDF/gallery, run the full suite and commit.

- Terrain LOD complete: eight distance levels derive from the existing segment
  anchors (working sequence 3/5/6/8/10/12/14/16). Bounded inward offsets keep
  finest samples unsunk; shared corners/edges remain watertight, including
  shoreline refinement. One selected mesh replaces the prior buffers; water
  uses eight levels with zero sinking. `terrain_lod.sink_depth_m` defaults to
  1 m and is further capped for small planets. This is spatial sinking with
  hysteresis; level switches can still step, without temporal morphing.
  All 45 CTest entries pass (350.77 s), including new topology/offset tests and
  all five live X11 tests. Build/logs: `build-terrain`, GCC 12, RelWithDebInfo.
  Fixed before/after captures, hashes and validation logs are published in
  `docs/journal/benchmarks/terrain-lod/`, explained in `terrain-lod.md` and the
  journal/PDF. Both fixed views keep the existing 100,000 land / 60,000 water
  triangle caps; no triangle/transfer or FPS reduction is claimed. All 17
  gallery images regenerated, hashes and source fingerprint verified; overview,
  shore, grass and terrain journal pages visually inspected. Overview has
  14,779 non-background pixels, bounds (1,1)–(794,598). No new runtime
  dependencies; Typst 0.15.1 is kept in the ignored build directory.
  Preserve the working 60 m grass distance. Next: horizon-distance foliage,
  reduced placement accuracy and shader-generated random placement.

- Resumed CPU profiling: all 42 saved capture/Callgrind artifacts, source,
  shaders and replay match their collection hashes. Published the full study
  under `docs/journal/benchmarks/cpu/`, including interactive HTML, SVGs,
  CSVs, raw traces and Callgrind data. Journal source includes the findings.
  Corrected frame slicing to exclude children of incomplete worker jobs;
  all five Python report tests pass. gprofng samples were rejected because
  its interval timer changed; Callgrind instruction counts are valid but
  are not function CPU times. No hardware FPS claim.
- The current host differs from the collection container. `build-codex` is
  retained evidence, with its old `/workspace` CMake cache. Fresh validation
  uses `build-cpu` (GCC 13.3, RelWithDebInfo, local dependencies from
  `build/_deps`), logs `build-cpu/cpu-{configure,build,tests}.log`.
  All 40 registered tests pass in 197.24 s, including trace/report and exact
  capture checks. Five X11 tests are omitted because xdotool/ImageMagick are
  missing; the original container's 45-test pass is retained separately in
  `benchmarks/cpu/environment/collection-tests.txt`. Both journal charts were
  visually inspected, PDF compiled, and all 17 existing gallery hashes checked.
  Root-owned profiling files were made editable by the user. The sandbox
  runner still fails during bubblewrap startup, so approved commands run
  outside it. CPU profiling is t; next is eight terrain LODs with sinking.
  Preserve the user's 60 m grass draw distance.

## Progress checkpoint — 2026-09-30

- CPU profiling in progress: `--cpu-trace` writes nested wall/thread-CPU scopes,
  including explicit terrain-worker lanes. `scripts/benchmarks/cpu_report.py`
  generates timeline SVG, bottom-up SVG, scope CSV and a standalone expandable
  HTML report. GNU gprofng 2.40 captured stacks but its collector warns that
  the timer changed and samples may be unreliable. Valgrind 3.19 was installed
  for independent instruction counts instead. Build/check logs: `build-codex/cpu-*.log`.
  Next: finish focused tests, collect frozen 60 m walking traces and profiler
  reports, document measured findings in the journal/PDF, run full tests, commit.

- glvertexid adaptation is implemented: `GrassLod` builds eight guarded distance
  ranges, stable density retirement tiers and one front-to-back instance buffer.
  Single-tip strips remove one degenerate triangle per blade; equal one-triangle
  levels coalesce, giving at most six draws. Shader sinking updates every frame;
  distant blades straighten before segment reduction. Terrain topology is kept.
  All 43 CTest entries pass (92.97 s; `build-codex/lod-tests.log`). The frozen
  view submits 26.8% fewer vertices and 31.5% fewer triangles, and uploads 10.0%
  fewer instance bytes. A single paired llvmpipe run reduces walking mean by
  7.2% and median by 6.6%; this is not a hardware FPS guarantee. Full analysis,
  frozen inputs, CSVs, logs and hashes are in `docs/journal/benchmarks/grass-lod.md`
  and `benchmarks/lod/`. Journal PDF compiled; all 17 gallery hashes verified,
  grass and shoreline visually inspected. Headless overview has 14,784
  non-background pixels, bounds (1,1)–(794,598). Baseline executable and shader
  tree are in `build-codex/pre-grass-lod/`; use `--runtime-dir` when comparing
  it so it cannot load current shaders. No dependencies added. Preserve the
  user's 60 m draw distance. Next: the CPU timing/profiler task immediately
  after this row. The reference itself contains a heightmap example, not eight LODs.
- Perlin wind is complete in `shaders/foliage/grass.vert`; `GrassWind.h`
  bounds the shared phase to an 8192-second period. Broad gusts, slow direction
  and fine flutter now use three body-local gradient fields with quintic fade.
  Focused GPU checks pass for lattice/time-wrap continuity, roots, both poles,
  large times, zero wind and body transforms, plus existing lighting/clipping.
  All 43 CTest entries pass (98 s); all 17 gallery images regenerated and hashes
  verified (`build-codex/perlin-tests.log`, `perlin-gallery.log`). Grass capture
  and journal pages visually inspected; PDF compiled. The overview has 14,784
  non-background pixels, bounds (1,1)–(794,598). The prior movement benchmark is
  explicitly labelled as a sine-wind workload. No dependencies added; preserve
  the user's 60 m draw distance. Pre-change executable and shader:
  `build-codex/pre-perlin/`. Next: the linked glvertexid triangle/batching/8-LOD
  techniques, then the foliage falloff parameter. Wind still follows simulation
  time; its separate pause behavior remains the later dedicated task.
- Camera-movement profiling complete: all 43 CTest entries pass (86 s), including
  fixed-step walking, foliage timing fields and exact paused replay. The working
  scene now uses the user's 60 m grass draw distance; keep that setting. Controlled
  CPU placement runs alternate baseline/optimized order and retain identical
  162,828 blades and attribute checksums. Median process CPU time is 52.505 ms
  before and 34.6775 ms after. Paired rendering runs with two llvmpipe threads
  reduce preparation per rebuild from 79.92 to 50.79 ms, but walking frame
  medians remain 872/869 ms; no hardware FPS fix is claimed. All three paired
  PNGs are byte-identical. Raw CSVs, input scenes and executable hashes are in
  `docs/journal/benchmarks/movement/`; the analysis is in `camera-movement.md`
  and the compiled journal PDF. Background terrain construction and partial
  patch update tradeoffs are documented. All 17 gallery images regenerated
  and hashes verified; only the working surface view (60 m setting) and timing
  overlay changed. Overview and grass visually inspected; overview has 14,784
  non-background pixels, bounds (1,1)–(794,598). No new dependencies.
  Earlier `movement-before/after` timings vary with host load and must not be
  used to claim a speedup. Next in table order: Perlin-noise wind, then the
  linked triangle/batching/LOD optimizations and foliage falloff parameter.
- Renderer extraction complete: main is 14 lines; `planet_app` compiles the
  implementation. `src/app` owns options/window/input, `src/app/scene` loads and
  stages CPU scenes, and `src/rendering/runtime` contains private renderer state
  and separate execution paths. Scoped GPU adapters release resources before
  the context even on construction failure. Shader failures now throw and
  release temporary shader stages. Seven lifecycle/recovery and options cases
  pass. All 43 CTest entries passed across the full run and a focused rerun:
  the original fixed-delay orbit input check also failed on the old executable;
  it now waits for a changed, settled camera frame and passes on both builds.
  Public Renderer header compiles independently. All 17 gallery hashes verified;
  all 16 deterministic PNGs match the previous gallery exactly, with only the
  timing overlay changing. Overview and shoreline visually inspected; overview
  has 14,784 non-background pixels, bounds (1,1)–(794,598).
  Pre-refactor executable and source: `build-codex/pre-renderer/`. No dependencies
  added. Next: the newly inserted planet-camera movement FPS profiling task,
  before the foliage falloff parameter.
- Mountain-altitude clipping fix validated: all 42 CTest tests passed (88 s).
  The new X11 regression compares a paused live window with its capture on
  an 80 m plateau, with the eye 2 m above the ground. The old executable
  fails; the fixed frame shows 921,217 ground pixels, bounds
  (0,0)–(1279,719), and matches the capture. Test windows retain their initial
  dimensions to avoid cursor warps changing the view during mouse capture.
  All 17 gallery images refreshed and hashes verified; only the timing
  overlay image changed. Next: extract Renderer ownership and lifecycle.

## Previous checkpoint — 2026-09-29

- Shoreline work from `a3a4b8b` is validated. Shared-edge refinement stays
  watertight and within the configured cap; local water geometry follows the
  same refinement target. Beach shading and foliage biome selection use root/
  fragment altitude. The close shoreline capture has 518,393 non-background
  pixels, bounds (0,0)–(959,539), 100,000 land and 60,000 water triangles.
  All 17 gallery images refreshed, hashes checked, shoreline and solar views
  inspected. All 41 CTest checks pass across the full run and focused reruns;
  the sole stale foliage test now respects the user's 400 m limit and README
  documents the 120 m working distance. No new dependencies.
  Next: steep-mountain camera clipping. A controlled 80 m plateau replay
  reproduces the bug: live rendering uses reference-sphere altitude to set
  its near plane, whereas captures use the configured ground clearance.
- Gaussian foliage density complete. Peak requested density is normalized
  against the integrated Gaussian to preserve the hard instance budget.
  Seeded candidate roots remain stable when moving the patch. Radial annuli,
  moving center, pole symmetry, full 41-test suite and exact paused replay
  passed. All 16 gallery images regenerated and hashes checked; grass preview
  inspected (921,598 non-background pixels, bounds (0,0)–(1279,719)).
- Compile-time stabilization complete: substantial implementations now have
  `.cpp` files and shared static libraries. All 41 CTest entries passed under
  Xvfb with two jobs; real X11 input tests now run serially to avoid focus
  collisions. Twelve public headers compile independently. Frozen grass
  capture matches the saved pre-refactor executable byte for byte. Overview
  inspected: 14,784 non-background pixels, bounds (1,1)–(794,598).
  Touching Terrain.cpp rebuilt one object and relinked ten executables in
  4.32 s (GCC/Ninja/RelWithDebInfo, two jobs). No dependencies added.
- Foliage implementation now lives in `src/rendering/foliage` and
  `shaders/foliage`, configured by `src/config/FoliageConfig.h`. The existing
  README already confirms SimonDev Quick_Grass as the reference. Adapted its
  six/one-segment blades, palette and light response with the MIT notice in
  `external/quick-grass`. Terrain roots are sampled from rendered triangles;
  same-LOD overlap is seeded independently of camera rejection. Wind uses
  simulation time; patches clear on reload. Three-metre surface preview is
  saved as the new foliage replay and inspected. CPU/GPU/full-capture tests
  and updated gallery are complete: all 41 CTest tests passed (70 s), all 16
  images refreshed and hashes checked. Tests/documentation now honor the user
  density limit of 4096 and working scene camera at 2 m. No new dependencies.
- Resumed after workspace permissions were restored. Configurable slope range
  is implemented in ScenarioConfig, TerrainSurface and the terrain shader,
  with config/CPU/GPU regressions and README/config documentation. All 39
  CTest tests passed in one run (93 s); all 15 gallery images refreshed and
  hashes checked; terrain-detail visually inspected. Overview render: 14,784
  non-background pixels, bounding box (1,1)–(794,598), 3,471 planet pixels.
  Water retains its separate smooth shader; sharp two-color reflection test passes.
  Found the likely foliage reference: https://github.com/simondevyoutube/Quick_Grass;
  asked the user to confirm while finishing the preceding row.
- Terrain material validation is complete. All 39 CTest checks passed across
  the full run and a focused rerun: the initial layout failure was the missing
  terrain image, now published; the overlay input timeout passed in isolation.
  All 15 gallery captures were regenerated and hashes checked. Inspected the
  terrain-detail image. Headless overview: 14,784 non-background pixels,
  bounding box (1,1)–(794,598), 3,466 planet pixels. No dependencies added.
- Next in table order: steep-mountain camera clearance, then the Renderer
  extraction and the newer entries below it. Preserve the
  user's new scene settings. The earlier sky-cut replay is still unavailable.

## Previous checkpoint — 2026-09-28

- Terrain material work is implemented in `shaders/terrain/basic.{vert,frag}`
  and bound per body in `src/main.cpp`. It uses two filtered three-dimensional
  noise scales, slope-based gray rock, rough diffuse reflection, and a
  centimetre-scale normal perturbation in body-local metres. No geometry or
  collision changes. `TerrainMaterialRender` checks visible sub-triangle
  detail, gray slopes, invariant depth, body rotation, distance filtering,
  and the dark night side. The initial full 39-test suite passed; final
  validation and 15-image gallery refresh follow the final visual tuning.
  A new land camera is saved under `docs/captures/replay/terrain`.
  On Mesa llvmpipe, a single 30-frame 800x600 moving benchmark gave median
  frame time 386.27 ms before and 390.15 ms after (frames 5–29). The source
  baseline was rendered from a separate shader copy; triangle counts match.

- Extreme-refraction example is t: matched 1280×720 renderer captures
  are in `docs/screenshots/refraction-extreme-{on,off}.png`, with exact replay
  JSON under `docs/captures/replay/refraction`. Both scenes share a 15 m shell, 150 kPa,
  180 K and 15° FOV; only `refraction_enabled` changes. README and Typst/PDF
  include the comparison and disclose the visible sky-edge notch. The gallery
  generator published 14 images with verified hashes. The full suite passed
  38/39 checks, then the failed ten-file folder check passed after placing
  the two new replay sidecars in their own subfolder. The JSON toggle is
  already implemented and covered by existing atmosphere tests.
- Validation build moved to ignored `/workspace/build-codex` because `/tmp`
  build trees are cleared between interrupted turns. Reuse this build.
- Cut-artifact investigation: standard scene on/off and a dense-air scene did
  not reproduce the supplied image. A controlled thin, cold atmosphere at a
  low surface camera did reproduce dark horizontal slits through the Sun.
  `shaders/atmosphere/atmosphere.frag` now limits image reprojection to pixels
  with sky depth, preserving finite-depth body silhouettes. On/off visual
  comparison and `AtmosphereRefractionRenderIntegration` pass. A small bright
  notch can remain next to the Sun; broad contours in the supplied image are
  not yet reproduced. Await its camera/time or replay details; do not mark t.
- Past orbit trail row is t: `pastOrbitTrails` orders samples from ten periods
  ago to the current epoch and gives the oldest fifth a smooth opacity ramp.
  The line shader blends per-vertex alpha. Clean build and all 39 CTest tests
  passed under Xvfb. The orbit input test failed once from a fluctuating
  screenshot, then passed on rerun and in the full suite.
- Previous next step (now completed): finish terrain validation and commit it. The broad
  sky bands in the supplied image remain open pending its camera/time replay;
  fixed-step terrain-shadow integration is a possible cause, not verified.
- Work in table order. Mark `t` automatically only after tests pass and documentation is updated.
  `v` is reserved for user verification. Commit each feature after successful tests,
  before starting the next row.
- First row: I toggle, Tab hold, bordered panel, FPS/frame/GPU pass time implemented.
  Added optional Linux NVML device-wide utilization, sampled off the render thread.
  Ambiguous adapters, software renderers, missing drivers and errors show N/A.
- First row is t: GpuUtilizationTests, PerformanceOverlayInputIntegration and
  PerformanceCaptureIntegration passed; gallery regenerated and visually reviewed.
- Environment now supports builds and headless rendering: `/tmp/planet-readme-build`,
  RelWithDebInfo, Xvfb, Mesa llvmpipe OpenGL 4.5. Physical NVIDIA validation is
  unavailable here; the optional driver ABI is exercised with a test library.
- Folder organization is t: clean `/tmp/planet-organized-build`, all 33 CTest
  tests passed (70 s), gallery regenerated, links and hashes verified.
- Automatic quality is t: scene scale falls from 100% toward 25% after
  sustained frame times over 55 ms and recovers below 32 ms. Driver-reported
  memory or `--video-memory-mb` caps the initial scale. Full 35-test suite
  passed, including live low-memory rendering and overlay input. Gallery
  regenerated. A strict 20 FPS guarantee is not possible at minimum detail.
- Pole traversal is t: view/up follow the great-circle walk step; heading
  updates in the new tangent frame. Both poles covered by regression tests.
  Full 35-test suite passed and gallery refreshed.
- Atmospheric mountain shadows are t: each planet's terrain depth map
  attenuates direct view-path scattering. A GL test checks blocked/unblocked
  scattering and map binding; sunset stays warm with finite solar-disk
  twilight. All 35 tests passed and gallery refreshed.
- Night-side ocean is t: local Sun incidence and terrain shadows gate
  reflections; invalid reflected-camera UVs are ignored. Indirect water light
  remains. Dark/day GL regression, all 35 tests and gallery refresh passed.
- O-key orbit visualization is t: ten predicted inertial revolutions include
  the moving parent for moons; labels show name, parent, axes, eccentricity,
  period, radius and mass in the Sun orbit camera. Geometry and real X11
  toggle/camera tests, all 37 CTest tests and gallery refresh passed.
- Surface-camera transition is t: manual and automatic entry interpolate eye,
  orientation and FOV for one wall-clock second while staying above sampled
  terrain/water. Paused simulation still permits the descent; changing modes
  cancels it. Geometry, real X11 midpoint, all 39 CTest tests and gallery
  refresh passed.
- Temperature-controlled atmospheric refraction is t: the existing JSON
  `temperature_k` feeds composition/density optics and curved view rays.
  Added CPU and GPU regressions that compare cool and warm air; all 39 CTest
  tests passed. README documents configuration and the future orbital heat
  calculation remains outside this row. Gallery refreshed.




# Todo History
|Task |State|Comment|
|:--:|:--:|:--:|
| add stats to the actual screen (black with white boarder) when pressing "i" including fps, and % of graphic card utilisation| v| I/Tab panel and optional Linux NVML GPU utilization implemented. GPU ABI/failure tests, real X11 input/border tests and performance replay passed; README captures refreshed. Physical NVIDIA hardware is unavailable here. |
|The Project has become a bit hard to keep track of. add a more fine granular folder strcture, such that no 10 FIles are just flying around in a single folder. Exeception may be e.g. the picture folder, but then make sure there are actually only image files in there and the jsons are seperated. | v| Grouped source/tests/shaders by subsystem; images separated from replay JSON and generation records. Clean build, all 33 CTest tests, layout/link checks and gallery generation passed. |
| adjust quality settings to be automatically chosen such that we always have at least 20 fps. Based on virtual memory, choose the degree of detail in which the scene is rendered| t | Adaptive scene scale and graphics-memory cap implemented. Controller and memory-limited live-render tests passed; full 35-test suite passed. 20 FPS is a target; software rendering reached minimum scale below it. |
| movements around the poles is really awkward, the planet camerastarts spinning when walking towards the pole.|v| View basis follows great-circle walking across either pole. North/south crossing tests, all 35 CTest tests and gallery refresh passed. |
|  Bug: The atmophere is also illuminated if mountains should block the light|v| Terrain shadow map attenuates direct atmospheric scattering; GL shadow/no-shadow tests, full 35-test suite and gallery refresh passed. |
|Bug The Oceans should not be illuminated on USER_IO/user_artifacts/image.png on the shadow_side of the Planet.|v| Local Sun incidence and terrain shadows gate water reflections; dark/day GL regression, full 35-test suite and gallery refresh passed. |
| When pressing "o" visualize the ellipsis of the planets (and moons) of some orbits in diffrent colors (average color of surface) and add a label to each planet that lists its most important parameters. (make it disappear when we have a planet cam)|t| O toggles ten predicted revolutions colored by average terrain tint; body labels show orbital/physical parameters and hide in planet cameras. Geometry, X11 input, all 37 tests and gallery refresh passed. |
|when switching from orbital cam to planet cam, add a transition phase of 1 sec where we smoothly drop to the planets surface and reorient our camera in a smooth movement|v| One-second wall-clock descent interpolates eye, orientation and FOV above sampled terrain/water for manual and automatic entry. Geometry, live X11 midpoint, all 39 tests and gallery refresh passed. |
| Add Light atmospheric light bending and also based on the temperature (set temp param by json for now, later calculate it by orbit and sun strength) |t| Existing curved-ray renderer and JSON temperature model now have CPU and GPU temperature regressions; all 39 tests, documentation and gallery refresh passed. Orbital heat calculation remains future work as requested. |
|For the ellipsis tracing visualize past ellipsis, not future, and fade the last 20% of the ellipsis to avoid a hefty cut.|t| Ten past revolutions sampled in the moving hierarchy; oldest 20% uses smooth alpha fade. Geometry, real X11 toggle and all 39 CTest tests passed; README updated. |
|Check the image at USER_IO/user_artifacts/image.png how can we avoid those cuts? maybe se some gradient field instead of rasterisation? The issue happens at the Poles. In the shadow of a mountain.|p| Controlled thin-atmosphere view reproduced dark slits through the Sun; finite-depth geometry now keeps its rasterized silhouette and a GPU regression passes. Large sky contours in the supplied screenshot still need its camera/time replay for verification. |
|Add a extreme atmosphere light bending exampe to the readme and journal with renderings. Make light bending a optional feature that can be turned on or off via json|t| Matched extreme on/off renderings, README, journal/PDF and exact replays added. Existing refraction_enabled JSON switch controls bending; gallery hashes and replay comparison passed. 38 full-suite tests passed, then the corrected layout test passed. |
|add textures, roughness and mini elevation for performant details, make steeper gradients more grey and give them a rocky look (Do so without crazy amounts of polygons, instead use the maps tro crreate rock like local landscape)|t| Planet-local procedural color, rough diffuse shading, centimetre-scale normal relief and gray steep slopes implemented without added geometry. GPU material regression, all 39 CTest checks and the 15-image gallery/hash validation passed. |
|Keep the water reflective and smooth. Make the gradient where grass becomes rock adjustable and keep grass longer before setting cliff. |t| Per-planet terrain_material slope range added (35–55° defaults), shared by gray-rock shading and broad darkening. Config/CPU/GPU regressions, smooth water reflection check, all 39 CTest tests and refreshed 15-image gallery/hash checks passed. |
|add foliage like the grass in the quickgrass demo (copy it as close as possible) |t| SimonDev Quick_Grass port implemented with instanced curved blades, distance LOD, wind, bright tips, slope/water exclusion, terrain shadows and water reflections. MIT attribution preserved. All 41 CTest tests and refreshed 16-image gallery/hash checks passed on the current scene settings. |
|Compile time has become pretty long, lets do a stabilisation commit where we try to clean up unnecessary header includes, and increase linking instead of compiling huge files new. |t| Config parsing, terrain generation, mesh/shader and foliage implementations moved into three compiled libraries; JSON removed from value-type headers. All 41 tests and 12 independent-header checks passed; grass replay byte-identical. Terrain implementation rebuild compiles one object and relinks consumers in 4.32 s. |
|Spawn the Foliage in a gaussian distribution around the current position, such that at the current position there are most and far away only view. If its coputationally too complex add 5 distance zones.|t| Gaussian placement with sigma = draw distance / 3 and integrated budget scaling implemented. Equal-area radial density, moving-camera and pole tests pass; all 41 CTest checks, exact replay and refreshed 16-image gallery/hash checks passed. |
|as visible in image USER_IO/user_artifacts/image copy.png the water boarders are still pretty rough, the sand applies to whole (huge) triangles and the grass doesn't qite reach the water. Lets think of ways to keep it performant for large scale but have accurate, fine triangular resolution when we get close to the water.|t| Shared-edge local shoreline subdivision, adaptive water shell and per-fragment altitude palette implemented; foliage uses the same altitude biome. All 41 checks pass, including topology/budget and sub-triangle beach tests. Close before/after inspected; 17-image gallery refreshed and hashes verified. |
|When the Camera gets close to a steep mountain, its possible to look inside the planet, lets fix that |t| Live near-plane clipping now uses ground clearance instead of mountain altitude, including descent. Live-versus-capture regression fails on the old executable and passes with the fix. All 42 tests pass; 17-image gallery refreshed and verified. |
|Lately whenever the position of the planet cam changes, the fps drop from 30 to 10 fps, benchmark the code, write in the joural wha parts take how long, and see if the fliage placement needs optimisation or maybe partly updates instead of whole new placement of all grass.|t| Added placement/sort/upload traces and fixed-step stationary/walking/bare benchmarks. Conservative candidate rejection and cached sort distances reduce CPU placement by 34% and measured rebuild preparation from 79.92 to 50.79 ms, with identical captures. Journal/PDF retain raw data and partial-update analysis. All 43 tests and 17 gallery hashes pass. Software-renderer walking median remains similar; the reported hardware FPS drop is not resolved by these measurements. |
|Let's outsource the rendering part from main to a Renderer Class that initializes during the constructor call and tears down in the destructor. Main should only include configparser and renderer, and maybe some commandargs parse utils, but keep it really minimal and move all the stuff in seperate classes.|t| Main reduced to 14 lines; Renderer owns startup/teardown, with separate window/input, scene loading, terrain, capture and live-render implementations. All 43 checks pass across the full run and corrected input-test rerun. Lifecycle/failure recovery tested; 17 gallery hashes verified, all 16 deterministic images unchanged. |
|For the wind, lets switch to perlin noise.|t| Gusts, direction and flutter now use periodic 3D Perlin gradient fields in body-local metres, with a seamless 8192-second clock wrap. GPU continuity/root/pole/transform checks and all 43 CTest entries passed. README, journal/PDF and all 17 gallery images refreshed and hashes verified; grass visually inspected. No added textures, geometry, instance data or dependencies. |
|Let's apply some optimisations from this repo: https://github.com/vercidium-patreon/glvertexid namely triangle optimisations, batching and 8 lods with sinking. Not all is applicable to our current setup 1:1 but these methods in general seem applicable, so let's apply them as well as possible.|t| Adapted to grass: single-tip strips, one instance upload, eight guarded LODs and smooth sinking, with compatible levels merged. Frozen view submits 26.8% fewer vertices and 31.5% fewer triangles, uploads 10.0% fewer bytes. One paired software-renderer run reduces walking mean by 7.2%; no hardware FPS guarantee. All 43 tests pass; journal/PDF and 17 gallery images refreshed, visually inspected and hashes verified. |
|Add more timing traces especially on the cpu side, how much time is spent in what functions, use a dedicated debugger/tool for the runtime analysis. Add reports of the rundtime analysis in top down and bottom up visualisation (how the time is divided/which functions are called when vs how much time is spent in what functions). Return and report if the environment doesn't provide a suitable context.|t| Nested wall/thread-CPU traces, interactive top-down/bottom-up reports and Callgrind instruction study published with verified artifacts and journal/PDF. All 40 available tests pass (197.24 s); five GUI tests require missing xdotool/ImageMagick (prior container run: 45/45). gprofng timings rejected; no hardware FPS claim. |
|Do 8 LODs also for the landscape of the planet implement sinking, such that always the most precise landscape is highest, remove the rest from the scene. |t| Eight budgeted surface-distance levels with bounded inward sinking and shared boundaries; one closed mesh, sea level preserved. All 45 tests pass; journal/PDF, fixed capture evidence and all 17 gallery images updated and verified. Spatial transitions retain small steps; 60 m grass distance preserved. |
|The Goal must be that we can cover the whole horizon with grass with reasonable fps. For that lets add grass foliage for high distance, reduce the placement accuracy, move the random placement from cpu to shader, and apply further techniques that seem suitable. |t| Superseded by user correction: one procedural grass layer, no separate horizon layer or grass sinking. GPU placement/wind and tapered quads validated; scene settings preserved. Clean 49/49 CTest run passes (286.14 s), including native input and HDR/reflection captures. Payload/performance study, journal/PDF and 20-image gallery documented. No whole-horizon or hardware FPS claim. |
|Add the perlin noise parameter to the json config|t| Optional foliage.wind_noise exposes gust/direction/flutter frequencies, clock multiplier and independent seed in JSON, with range/type validation. Both GPU paths consume the rules; captures preserve their explicit phase. Documented in README and procedural grass study. Covered by the clean 49/49 CTest run. |
|If not already implemented add frustum culling (for the foliage I think it could be difficult for the light simulation, but you may reduce triangle quality in the non camera viewed parts.), compute shaders and gpu instancing to be able to viaualize more foliage and finer landscape. Benchmark and calculate what data is sent from cpu to gpu, then add a section in the journal that identifies techniques most promising to reduce the largest delays. |t| Conservative per-pass grass frustum rejection, GL 4.3 compute placement/compaction and indirect instancing; GL 3.3 procedural fallback retained. Culling/replay hashes match and compute/fallback parity tests pass. Verified 34,664-byte grass patch upload vs 12 MB land mesh; cached grass uploads zero. Journal reports the historical paired benchmark and remaining atmosphere/terrain bottlenecks. Clean 49/49 tests pass. |
|Lets add a third person camera "4" and a actor on the same position as cam 2 that follows a small astronaut that can walk around the planet. Create a 3d model of the astronaut or download some nice MIT licensed one online (comic style). Create Walking movement such that the feet stay on the ground and dont slide over it. Search if tere is a nice library for that movement. If not, approvximate the foor movement for now and we will coma back later to that.|t| Procedural grey/blue astronaut with German arm patches, camera 4, locked planet-local contacts and MIT ozz-animation IK. Walk 6, Shift sprint 12 m/s; Space jump then jetpack boost/bubbles. All 49 entries validated via full run (48 passed) plus corrected input check (four consecutive passes); journal/PDF and 20 gallery images refreshed, inspected and hashes verified. Sketchfab shortlist recorded; an external model is not yet integrated. |

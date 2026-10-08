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


## Clean foundation

These five outcomes replace the nested terrain/atmosphere planning and benchmark
rows. Implement them using the existing architecture and tools. Completed work
and historical experiments are summarized in [USER_IO/escalded_change.md](USER_IO/escalded_change.md)
and the journal; they are not additional active tasks. Store raw benchmark output
locally in ignored build directories, including .gz/.xz files. Commit only source,
small required fixtures and compact results needed to explain the changes.

|Task |State|Comment|
|:--|:--:|:--|
|F1 — Make builds and normal tests independent of historical benchmark archives.|t|Clean RelWithDebInfo configure/build and 34/34 headless core checks pass without historical archives. Bulk data stays local; 599 old tracked raw traces removed from Git. Ten optional validators report missing data with exit 2 and regeneration commands; original hash checks and corruption rejection preserved. Storage policy and commands: scripts/benchmarks/archives/README.md. F2–F5 unchanged.|
|F2 — Fix the compute terrain stationary full-render regression.|t|GPU distance-ordering of triangle references reuses existing scratch storage without CPU blade-queue reads or quality reduction. Three alternating Quadro production normal-wind pairs pass the 1.05 p95 gate at 1.009/1.021/1.013; fixed-wind pairs pass too. Exact production blades/depth/RGB and native contacts/replay preserved. Compact result and verification limits: docs/journal/architecture/terrain-gpu/async/hardware/cost/raster/order/correction.md. Raw runs stay local; CPU remains default. F3–F5 untouched.|
|F3 — Implement automatic foliage falloff while protecting near-camera density.|t|Resident GPU planner protects the configured quad-distance region, fits distant sigma to timing/memory budgets and reports infeasible near work. Admission includes live/staged/retiring/replacement generations; cached telemetry has conservative missing-data limits and hysteresis. Effective policy is locked in replay. Normal/tiny budgets, actual protected coverage, missing/stale telemetry, exact production/walking replay and native reload/fallback audits pass. Compact results: docs/journal/architecture/terrain-gpu/grass-allocation/adaptive-falloff.md. Raw runs remain local; CPU stays default. F4/F5 unchanged.|
|F4 — Keep atmospheric highlight/exposure reduction on the GPU.| |Reuse the current optics cache, GPU tile meter and atmosphere integration; replace full tile-map CPU readback/reduction with a bounded GPU result used by the render passes. Preserve weighted 5% exclusion, partial tiles, sparse-star safeguards and current-frame main/reflection behaviour, with supported GL 3.3 fallback. Done when existing airless/day/twilight/dust/humidity/temperature/refraction and reload/replay checks preserve output, and a focused comparison confirms removed tile transfer and reports frame cost. No atmosphere model rewrite.|
|F5 — Complete a bounded terrain acceptance run and document the supported defaults.| |After F1–F4, reuse existing runners for three alternating CPU/compute pairs per stationary, 6 m/s walking, 12 m/s sprint, saved Moon handoff and scene reload case. Complete the canonical 100k-triangle transfer/CPU-work measurement. Check >=75% terrain transfer and >=50% bulk CPU field-work reduction, <=5% full-frame p95 regression at matched coverage/quality, and bounded replacement memory/contact/replay parity. Run the relevant clean-build fallback/compute/input/reload checks. Done with one compact pass/fail report and documented defaults: enable compute only if all gates pass; otherwise explicitly keep CPU supported and compute experimental without claiming the speed goal complete. Retain raw receipts locally; report failures against the existing task rather than adding another task chain.|

Order: F1 first; F2–F4 are separate implementation outcomes; F5 verifies the
combined result. A blocked hardware run does not mark its task complete or
prevent independent work on another implementation outcome.

## Remaining requested features

|Task |State|Comment|
|:--|:--:|:--|
|The current Astronaut Models are not suitable , i want a more realistic modern looking high poly version. Remove te current Astronauts and give me 5 other examples. |||
|Fade grass trails smoothly back to their normal wind motion near history eviction. As the bounded history fills, progressively reduce deformation through the oldest 30% of retained trail segments so old marks disappear without a sudden pop. Even when fresh the movement shall be 80% pressed down, and 20% percent wind movement, sch that even when pressed down it still moves a tiny bit. | |Queued on user request. Preserve normal wind phase and fixed roots; use a continuous history-age weight shared by both GPU paths and reflections, and retain the weights/state in replay.|
|Evaluate BSON storage for long trails and expensive computed results. Profile the most time-consuming calculations and compare recomputation with serialization, storage size and read/write cost; identify which results actually benefit from persistence.| |Measure candidates such as trail history and terrain/atmosphere caches before choosing what to store. Document dependencies, cache keys, ownership and invalidation; avoid persisting cheap results without a measured benefit.|
|Implement beneficial BSON persistence selected by the storage evaluation, with explicit application/cache versions and steadily maintained program version increments. Erase or rebuild cached storage when a newer/incompatible program version invalidates it so stored results remain consistent.| |Depends on the storage evaluation. Validate version, schema, scene/noise/quality inputs and relevant backend dependencies; handle missing/corrupt files and interrupted writes safely. Document the version-bump policy and test cache invalidation and exact restored results.|
|Make the Jetpack emit a fire coming out the back, with modulation based on how long it runs and speed of the astronaut relative to the local planets atmosphere (first some smoke, then when faster make a blueish flame) shader with some particle smoke coming out the back that slowly diffuses into the amosphere based on the wind.|||
|Lets create a shader for the Sand around the water, that creates actually noise, based on the wind that looks like the image USER_IO/user_artifacts/image copy 2.png  |||


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

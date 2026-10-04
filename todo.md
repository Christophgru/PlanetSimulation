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


|Task |State|Comment|
|:--:|:--:|:--:|

|Form a terrain/atmosphere CPU–GPU implementation plan before starting GPU terrain work. Audit the existing pipeline and evaluate hexagonal planet panels, seams/poles and alternatives. Define CPU ownership of subdivision/LOD/sinking and GPU ownership of noise, surface shape and atmospheric fields, including ground contacts, shadows/reflections, supported GPU paths and validation/performance budgets.|t|Source-audited plan in docs/journal/architecture/terrain-gpu/plan.md. Keep icosahedral triangle panels initially; hex/pentagon alternatives evaluated. CPU topology/sinks/sparse contacts, GPU bulk fields and grass planning, atomic generations, GL 3.3 fallback, atmosphere reductions, VRAM/density policy and phased acceptance gates defined. Analytical topology/capacity worksheet, document links, layout and rebuilt journal validated. No GPU terrain implementation or performance improvement claimed.|
|Extract the planet-field and terrain-topology contracts from the completed CPU–GPU plan (T1). Preserve the CPU backend while separating canonical radial samples, indices and sinking from bulk height/normal evaluation; introduce sparse query caching and explicit generation/backend keys.|t|Read-only PlanetField, aligned parameter/sample layouts, bounded build-local query cache, indexed radial/sink topology and validated CPU generation keys implemented. Clean 56/56 CTest groups pass (641.75 s), including seven new contract cases; 17 independent mesh snapshots and two native replay PNGs match exactly. README and journal/PDF updated, all 22 gallery hashes retained. GPU shaping remains T2; see docs/journal/architecture/terrain-gpu/contracts/study.md.|
|Implement and validate the opt-in GL 4.3 GPU terrain field evaluation proof (T2). Use the T1 topology/parameter contracts, preserve shared endpoints and legacy draw layout, query limits, bound staging and publish completed land/water together.|t|Capture-only compute field/gradient/material/sink evaluation and GPU corner expansion implemented. Five native GL cases and clean 58/58 CTest groups pass (645.09 s), including exact compute/walking replay, CPU override, real GL 3.3 fallback and locked/unknown-version rejection. Production input is 2,800,880 bytes versus 12,000,000 (76.66% reduction); native shoreline/offline PNGs match exactly. CPU mirror remains until T3; no total-frame speedup claimed. README, journal/PDF and retained evidence updated; see docs/journal/architecture/terrain-gpu/compute/study.md.|
|Replace the compute terrain contact dependency on the full CPU render mesh with sparse matching triangle-plane contacts (T3a). Index the canonical topology, evaluate only needed float-rounded/sunk vertices, bound the cache and publish matching generation keys.|t|Radial BVH, bounded 1,024-position oracle cache and matching generation publication implemented. Four sparse CPU cases, six native GL cases and clean 58/58 CTest groups pass (664.17 s). Exact source position/plane parity; GPU-vector comparisons, poisoned full CPU coordinates, cache/coverage/stale/rebinding checks pass. Walking uses 14 contact vertices; surface/walking PNGs match T2 exactly. README and journal/PDF updated; see docs/journal/architecture/terrain-gpu/contacts/study.md. Grass mirror and interactive T3b/T3c remain.|
|Move compute terrain grass metadata and deterministic slot planning to resident GPU buffers (T3b), then remove the full CPU render compatibility vectors.|t|T3b1/T3b2 complete. Four new allocation cases and all 58 CTest groups pass across two final-binary runs. Root identities, hard caps, versioned replay and CPU/GL 3.3 compatibility validated; new compute captures retain no CPU render vectors. Interactive asynchronous installation remains T3c.|
|Generate and validate resident GPU grass triangle metadata (T3b1): original IDs, area/bounds/biome eligibility and Gaussian weights from existing terrain buffers, with queried limits and transfer accounting.|t|Four new native cases and all 58 CTest groups pass (1092.60 s); exact compute/walking replay and GL 3.3 fallback preserved. See docs/journal/architecture/terrain-gpu/grass-metadata/study.md. CPU allocation and compatibility vectors remain T3b2.|
|Consume resident GPU grass metadata with deterministic ordered slot allocation and rounded-slot density search (T3b2), then remove full CPU render vectors from compute captures.|t|GPU hash selection, rounded-slot density search and ordered references implemented. All 58 CTest groups pass across two final-binary runs, including 14 native terrain/metadata/allocation cases and camera-only replay compatibility. Production probe: 2,840 input bytes, one 224-byte summary, no CPU render vectors; exact GPU/legacy replay and GL 3.3 fallback pass. README, journal/PDF and retained evidence complete; see docs/journal/architecture/terrain-gpu/grass-allocation/study.md.|
|Enable asynchronous complete compute terrain consumers and interactive opt-in operation (T3c). Validate stale/reload/allocation recovery and measure hardware total cost before default enablement.|p|Implementation plan: docs/journal/architecture/terrain-gpu/async/plan.md. T3b, T3c1 and T3c2 are tested. T3c3a transaction ownership/retirement is tested; resume at T3c3b for renderer publication, then T3c3c reload/failure recovery; preserve the CPU default. Build topology/contact indices on CPU workers, dispatch into bounded spare sets and poll completion without normal-walking waits. Publish land/water/grass/contact/shadow/reflection keys together, retain the last valid generation on failure and test body switches/reload/stale work. Hardware performance gates remain; do not infer them from llvmpipe.|
|Build bounded CPU-worker topology/contact generation with request/scene identities and stale-work handling (T3c1).|t|One persistent executor, one coalesced queued snapshot and one completion slot; immutable land/water topology and sparse contacts without full resident render evaluation. Epoch/body/field/mode/serial validation, reload ownership, exception recovery and shutdown tested. Eight worker cases and all 59 CTest groups pass (736.20 s); ten compute/CPU/replay hashes match T3b2 exactly. README, journal/PDF and evidence updated; see docs/journal/architecture/terrain-gpu/async/worker/study.md. Interactive compute remains gated.|
|Stage asynchronous GPU terrain and grass preparation without normal-frame waits (T3c2).|t|Owned land/water and resident grass submission/poll/commit; validated 224-byte summary, preallocated draw resources and 512 MiB logical admission. All 59 CTest groups pass (733.29 s); a subsequent controlled delayed-fence probe passes with all 21 native cases. App binary unchanged; ten capture/replay hashes match T3c1 exactly. README, journal/PDF and evidence updated; see docs/journal/architecture/terrain-gpu/async/gpu/study.md. Atomic publication/retirement remain T3c3; interactive compute gated.|
|Publish complete compute consumers atomically and validate retirement/reload/failure recovery (T3c3).|p|Split into T3c3a transaction ownership/retirement, T3c3b renderer publication and T3c3c transactional scene reload. T3c3a transaction ownership and fenced retirement are tested; resume at T3c3b. Capture installation remains separate until T3c3b. Interactive compute stays gated. Preserve previous generations/scenes on failure and test stale work, movement and reload.|
|Build a complete GPU generation transaction with bounded overlap, atomic consumer exchange and fenced retirement (T3c3a).|t|Off-live complete consumers, no-throw transfer, one global preparation/two sets per body, 512 MiB per-set and 1 GiB aggregate logical admission. Eight new transaction cases (six fence failures, stale/live identities, cleanup, disabled consumers, body switches and delayed retirement) pass with all 29 native cases and 59 CTest groups (756.95 s). Ten capture/replay hashes remain exact. README, journal/PDF and evidence updated; see docs/journal/architecture/terrain-gpu/async/publication/study.md. Renderer/reload integration remains T3c3b/T3c3c.|
|Connect complete generation publication to renderer frame boundaries and dependent consumers (T3c3b).| |Depends on T3c3a. Preserve distinct terrain and chase/replay grass planning eyes. Rebind contacts and refresh shadow/frame/reflection consumers before dependent draws; expose matching request/revision evidence. Preserve exact captures and CPU behavior; interactive compute remains gated.|
|Make scene reload transactional through complete GPU generation readiness and validate movement/reload recovery (T3c3c).| |Depends on T3c3b. Keep the old scene drawable until all replacement consumers are ready, reject obsolete epochs/body identities and bound staging/retirement across repeated reloads. Test valid/invalid configs, injected GPU failures, rapid movement/body switches and Moon arrival before T3c4.|
|Enable explicit interactive compute after asynchronous recovery acceptance (T3c4).| |Depends on T3c1–T3c3. Native standing/walking/sprint/jump/flight/trail, shadow/reflection and reload checks must show matching consumer identities and zero normal-frame waits. Retain CPU default, GL 3.3 fallback and locked replay rejection.|
|Measure hardware total-frame and memory costs before completing the GPU terrain migration or changing defaults (T3c5).| |Compare matched CPU/compute stationary, walking and body-switch runs. Record CPU/GPU stages, publication latency, frame percentiles and replacement memory. llvmpipe correctness is not hardware acceptance; leave pending if suitable hardware is unavailable.|
|Implement highly optimized GPU terrain shaping according to the completed CPU–GPU plan. Pass noise/shape parameters and minimal panel/topology descriptors; calculate surface positions/normals/material inputs on the GPU. Keep CPU subdivision and sinking where the plan calls for them.|p|T2, T3a and T3b complete. T3c1 bounded workers and T3c2 GPU preparation are tested. T3c3a complete transaction/retirement APIs are tested. Next: T3c3b–T3c5 for renderer land/water/grass/contact/shadow/reflection publication, reload recovery, interactive opt-in and hardware acceptance. Preserve legacy CPU fallback/replay. Hardware total-frame and replacement-memory gates remain; no hardware FPS improvement claimed.|
|Implement the planned GPU atmosphere calculations and remaining planet field work. Reuse calculations already on the GPU and move remaining suitable per-sample work there, passing parameters rather than generated field arrays.| |Plan complete (A1); depends on relevant terrain interfaces. Existing density columns and ray integration already run on GPU. Cache uniform optics, profile and move highlight reduction with identical exposure semantics; validate airless/day/twilight/dust/humidity/refraction and CPU/GPU cost.|
|Adapt foliage allocation to measured GPU usage and available VRAM. Preserve the configured density near the planet camera; automatically choose the falloff from required density, maximum foliage distance, other foliage parameters and remaining memory/render budget.| |Policy defined in architecture plan (B1): allocation ledger, protected near plateau, rounded-slot-aware sigma search and asynchronous timer/free-memory feedback. Preserve configured density where feasible; report an impossible near-region budget, use conservative unknown-telemetry fallback and lock effective replay settings. Validate coverage, hysteresis, overflow and memory bounds.|
|The current Astronaut Models are not suitable , i want a more realistic modern looking high poly version. Remove te current Astronauts and give me 5 other examples. |||
|Fade grass trails smoothly back to their normal wind motion near history eviction. As the bounded history fills, progressively reduce deformation through the oldest 10% of retained trail segments so old marks disappear without a sudden pop.| |Queued on user request. Preserve normal wind phase and fixed roots; use a continuous history-age weight shared by both GPU paths and reflections, and retain the weights/state in replay.|
|Evaluate BSON storage for long trails and expensive computed results. Profile the most time-consuming calculations and compare recomputation with serialization, storage size and read/write cost; identify which results actually benefit from persistence.| |Measure candidates such as trail history and terrain/atmosphere caches before choosing what to store. Document dependencies, cache keys, ownership and invalidation; avoid persisting cheap results without a measured benefit.|
|Implement beneficial BSON persistence selected by the storage evaluation, with explicit application/cache versions and steadily maintained program version increments. Erase or rebuild cached storage when a newer/incompatible program version invalidates it so stored results remain consistent.| |Depends on the storage evaluation. Validate version, schema, scene/noise/quality inputs and relevant backend dependencies; handle missing/corrupt files and interrupted writes safely. Document the version-bump policy and test cache invalidation and exact restored results.|
|Make the Jetpack emit a fire coming out the back, with modulation based on how long it runs and speed of the astronaut relative to the local planets atmosphere (first some smoke, then when faster make a blueish flame) shader with some particle smoke coming out the back that slowly diffuses into the amosphere based on the wind.|||
|Lets create a shader for the Sand around the water, that creates actually noise, based on the wind that looks like the image USER_IO/user_artifacts/image copy 2.png  |||


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

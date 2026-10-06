# Page to document progress of AI Agents working on this Project

## Resident reload publication tracing checkpoint — 2026-10-06

- T3c5b2b2a links asynchronous resident reload roots to bounded worker/GPU body
  attempts. Off-live preparation cannot publish; final live generation/grass
  receipts arm after whole-scene exchange and contact binding. The root closes
  after every child has a matching complete consumer draw, including an explicit
  post-exchange draw for a Sun-only scene. CSV parent, exchange offset and child
  count retain the overlapping scopes.
- Invalid config, supersession, preparation/final-exchange failure and shutdown
  close the transaction family without successful latency. Dropped children
  produce trace_incomplete. Completed-row drainage retains parent ownership;
  the fixed 64-record pool adds no worker, topology retention or GL wait.
- Seven relevant CTest groups pass (195.27 s), with 17 controlled timing cases
  and four augmented reload cases. Evidence review found missing GPU timing
  bindings in the private fixture. After that correction, focused software and
  Quadro reload rechecks validate dispatch/placement joins. Only the reload
  fixture executable changed; runtime and the other 40 executables retain their
  fingerprints. This is scoped validation rather than a new full 65-group run.
- Journal/PDF, raw traces, failure receipts, device identities and frozen inputs
  accompany the checkpoint in
  docs/journal/architecture/terrain-gpu/async/hardware/publication/transactions/.
  The user's environmental TODO suffix is preserved and stays unstaged.
- T3c5b2b2 remains partial. Resume T3c5b2b2b: synchronous legacy CPU/capture reload,
  capture draw endpoints, actual destination handoff and CPU-only grass admission.
  Physical-memory diagnostics and matched hardware acceptance follow; CPU
  terrain remains default.

## Ordinary publication tracing checkpoint — 2026-10-05

- T3c5b2b1 is tested. Ordinary CPU terrain startup/worker replacement and
  resident compute terrain/grass-only requests retain independent bounded IDs
  through their first complete consumer draw. The optional publication CSV
  records wall/frame latency, phase offsets and generation/revision/anchor keys;
  GPU work carries the same attempt into dispatch and later placement. Failures,
  rejection, coalescing, cancellation, undrawn replacement and shutdown do not
  invent successful latency. Fixed 64-record metadata ownership adds no GL
  waits, worker, future or topology retention; disabled tracing reads no clock.
- Five new controlled recorder cases, two real-worker cases and two actual
  CPU/compute frame cases pass. All ten relevant CTest groups pass on final
  inputs (456.95 s), including capture/replay, reload/destination recovery,
  native loop and existing five native input scenarios/four startup rejection
  contracts. This is scoped validation, not a new full 65-group run.
- Real GLFW software/Quadro observations join successful receipts to actual
  generation keys and frames. Software has 65 loop receipts/seven publication
  outcomes/four successful endpoints; Quadro GL 4.3 has 66/eight/four. Audits
  retain zero blocking polls/server waits/bulk reads/finishes. A CPU-only private
  draw-helper assumption was fixed after preflight and passes final validation.
- Source/build-input and all 41 executable hashes remain frozen. Twenty-two
  gallery and ten retained historical baseline hashes match. Journal/PDF and
  reviewed pages accompany raw evidence in
  docs/journal/architecture/terrain-gpu/async/hardware/publication/validation/.
- Parent T3c5b2b remains partial. Continue with T3c5b2b2: whole-scene reload
  parent/child outcomes, captures, actual destination handoff and CPU grass-only
  preparation. Then physical memory T3c5b2c and matched hardware T3c5c. CPU
  stays default; user environmental/Docker TODO suffix remains unchanged and
  unstaged.

## Complete native-loop wall checkpoint — 2026-10-05

- T3c5b2a is tested. Interactive tracing adds a separate native-loop CSV linked
  by frame number: full body wall time before polling through profiler/CPU-scope
  closure, contained poll/presentation/event-wait intervals and explicit outcomes.
  Loading continues and minimized waits close the scope; exception/disabled-clock
  behavior has controlled tests. This timer owns only scalar stack state and a
  buffered output, with no GL calls, worker, future or forced completion.
- Two new deterministic cases and seven existing timing cases pass. Four relevant
  CTest groups pass in one final-input run (89.70 s), covering capture/replay,
  two native CPU/compute fixtures and five existing input scenarios/four startup
  rejection contracts. This is scoped validation; no new full 65-group claim.
- Software and Quadro GL 4.3 each join 64 loop/frame/probe receipts: CPU 8
  rendered/7 minimized; compute 7 rendered/35 loading/7 minimized. Both audits
  retain zero blocking polls/server waits/bulk reads/finishes. The preceding
  60 s capture preflight timed out; standalone and final 120 s allowance pass
  unchanged assertions. Raw rejected and passing results are retained.
- Final source/build-input and all 41 executable hashes remain frozen. Existing
  ten baseline and 22 gallery hashes are retained. Journal/PDF and reviewed
  affected pages accompany evidence in
  docs/journal/architecture/terrain-gpu/async/hardware/loop/validation/.
- T3c5b2 is divided into tested native loop (a), pending bounded publication
  lifecycle latency/outcomes (b), and device-verified physical memory (c).
  Continue with b, then c and matched hardware T3c5c. CPU stays default;
  user environmental/Docker TODO suffix remains unchanged and unstaged.

## Bounded GPU work timing checkpoint — 2026-10-05

- T3c5b1 is tested. Optional request-keyed work receipts cover terrain field/
  expansion, grass metadata/allocation and main/reflection placement. A fixed
  64-event/128-query pool checks start/end availability, preserves pending
  ownership and reports drops/missing shutdown samples without forcing GPU
  completion. Frame CSV exports a separate full marker span; nested placement
  is not added to render-stage totals. Disabled work tracing allocates no queries.
- Seven controlled query cases, four actual-renderer frame cases and all 64
  CTest groups pass in one uninterrupted final-input run (1422.72 s). Final
  source/build-input fingerprints and all 41 executable hashes remain frozen.
  Ten baseline compute/CPU/replay PNGs and all 22 gallery hashes are unchanged.
- Quadro GL 4.3 resolves 18 work receipts and 963 frame spans, peak pending six,
  without drops, positive-timeout/flush fence polls, server waits or bulk buffer
  reads. Software validation resolves the same 18 stages; 949 of 951 frame spans
  are ready, with two explicit missing-shutdown receipts.
  Twelve small-fixture EGL captures compare prior/new/traced binaries on both
  physical GPUs and CPU/compute terrain; each triple is byte-identical.
- Raw logs, traces, image receipts and provenance are retained under
  docs/journal/architecture/terrain-gpu/async/hardware/timing/validation/.
  README and journal/PDF describe diagnostics and limits; affected PDF pages
  were reviewed. Continue with T3c5b2 native-loop wall time, publication latency/
  outcomes and physical-memory sampling, then T3c5c matched hardware acceptance.
  CPU remains default. User TODO environmental/Docker suffix remains unchanged
  and unstaged.

## Quadro versus RTX 3070 Ti benchmark checkpoint — 2026-10-05

- Both physical cards now run the unchanged production capture renderer through
  a private EGL adapter: actual UUID/renderer, GL 4.3, RGBA8/depth24/stencil8
  and four-sample context configuration verified against NVML. Driver is
  580.178.04. Production rendering/runtime sources and shaders are unchanged.
- Twelve runs, three alternating pairs per case, use the same frozen executable,
  production scene, 1280×720 camera and quality. Ninety frames per run retain
  frames 10–88; all 948 retained frames have complete GPU query samples.
  Fixed-camera means are 73.279 ms Quadro / 5.745 ms RTX (12.75×); controlled
  walking means are 78.741 / 5.959 ms (13.21×). These are uncapped offscreen
  measurements; neither case rebuilds terrain/grass in the measured interval.
- All six pairs retain matching camera, planned blades/patches and receipts.
  GPU culling differs by two blades; mean PNG channel errors stay below 0.029
  of one 8-bit level. Full logs, CSVs, twelve PNGs/sidecars, telemetry, hashes,
  reproduction tooling and standalone plot are retained in
  docs/journal/benchmarks/gpu-comparison/. README and journal/PDF include the
  comparison separately from historical different-scene Quadro timings.
- T3c5 complete preparation/publication GPU timing and matched CPU/compute,
  long-walk/body-switch/physical-memory measurements remain pending. The prior
  63/63 runtime regression remains applicable to unchanged production sources;
  this checkpoint verifies benchmark tooling and documentation. User TODO
  environmental/Docker suffix remains unchanged and unstaged.

## Shared optics and hardware context checkpoint — 2026-10-05

- A1a is tested. A scene-owned exact-key pack per planet supplies sky lighting,
  body materials, main/reflection integration and capture diagnostics. Optical
  input changes validate before replacement; invalid refreshes retain the old
  pack. Scene replacement/reorder owns independent caches. Capture metadata
  exposes evaluation/reuse counters; moving/reflected frames retain one
  evaluation per configured body. The optical model and GPU LUT/integration
  are unchanged; GPU highlight reduction remains A1b, complete cost/parity A1c.
- GPU access is now available: Quadro M1000M (2 GiB) and RTX 3070 Ti (8 GiB),
  driver 580.178.04. Use `__NV_PRIME_RENDER_OFFLOAD=1
  __GLX_VENDOR_LIBRARY_NAME=nvidia` under Xvfb to select actual Quadro OpenGL.
  Default Xvfb remains llvmpipe; RTX OpenGL selection has not been demonstrated.
- T3c5a is tested. NVIDIA returned the requested GL 3.3 context and production
  compute incorrectly fell back despite capable hardware. Window creation now
  prefers GL 4.3 core and retries GL 3.3, retaining fallback/locked replay rules.
  Four native hardware contexts identify Quadro GL 4.3; the forced fallback
  identifies Mesa GL 3.3. Five input scenarios and four shipping rejection
  contracts pass (31.594 s); standing, 6/12 m/s walking/sprint, jump/WASD/Space
  thrust, trails/exhaust, space/Moon contacts, R/file-watch reload, supersession
  and preparation/retirement recovery retain matching receipts and zero
  blocking polls/server waits/bulk reads/glFinish.
- All 63 CTest groups pass in one uninterrupted frozen-input software run
  (1005.40 s), including 20 CPU and 12 GL atmosphere cases. Focused Quadro
  atmosphere/capture checks pass (14.007 s); CPU/compute optical reload parity
  passes (8.945 s). Source/input/ten binary hashes match the final tree.
  Nine regenerated atmosphere, ten baseline and 22 gallery PNG hashes remain
  unchanged. README, journal/PDF, complete hardware/software native traces,
  captures and evidence are retained
  in docs/journal/architecture/atmosphere/optics/.
- Resume T3c5 matched hardware CPU/compute stationary, walking and body-switch
  frame/publication/physical-memory measurements. Native hardware correctness
  does not finish the cost gate. CPU stays default. User environmental/Docker
  TODO suffix remains byte-for-byte and unstaged.


## Native resident compute opt-in checkpoint — 2026-10-05

- T3c4c and T3c4 are tested. Public `--terrain-backend compute` accepts native
  resident GPU grass; enabled foliage requires compute placement. Startup/reload
  share planner validation. CPU stays default; fresh GL 3.3 requests fall back and
  locked compute replays reject. Legacy compute capture planners remain supported.
- Native delayed startup found a real profiler scope crash. Loading presentation
  now follows mesh scope destruction, while character updates wait for complete
  initial consumers and GLFW events remain active.
- Five native production-loop/visible-GLFW/X11-key processes and four shipping
  rejection contracts pass (48.09 s focused). Loading, standing, 6/12 m/s
  walking/sprint, jump/directional/Space thrust, trails/exhaust, outer-space and
  Moon contacts, R/file-watch reload, supersession/body reorder, preparation and
  retirement failure/recovery retain matching draw/contact receipts. No product
  observer or injected motion; private test hooks sample completed frames and
  control GL delay/failure. Zero blocking polls/server waits/bulk reads/glFinish.
- All 63 CTest groups pass in one uninterrupted frozen-input run (924.49 s);
  final source/build-test inputs/eight executable hashes match exactly. Native
  logical accounting peaks at 14,348,020 bytes, excluding CPU snapshots/physical
  VRAM. Ten baseline and 22 gallery PNG hashes remain exact. README, journal/PDF,
  full compressed traces and evidence are in
  docs/journal/architecture/terrain-gpu/async/interactive/native/.
- Resume T3c5 hardware total-frame and physical-memory acceptance. This environment
  has no exposed GPU device; llvmpipe is correctness evidence. Keep CPU default and
  do not mark hardware cost acceptance complete without matched GPU measurements.
  User environmental/Docker TODO suffix remains byte-for-byte and unstaged.


## Asynchronous complete reload checkpoint — 2026-10-05

- T3c4b is tested. Resident reload owns the latest immutable config/replay/options
  snapshot and a monotonically increasing attempted epoch. Nonblocking future
  worker leases supersede executing/queued/ready work, preserving the live epoch
  on failure. One running, queued and ready CPU slot remains the bound.
- Complete off-live preparation handles one body at a time, retaining meshes and
  sparse contacts rather than all full CPU builds. Restored prospective contacts
  supply final grass anchors before the shared scene/tracking/options/replay,
  camera/contact/clock and effect/cache exchange. Old-scene movement and managed
  draws continue; startup/retirement requests wait while retaining the latest
  snapshot. Old buffers remain owned until zero-timeout last-use polling succeeds.
- Four actual GL reload cases pass (26.511 s), plus fourteen worker cases.
  All 62 CTest groups pass in one uninterrupted frozen-input run (884.40 s);
  source/input/seven-binary fingerprints remain unchanged. Invalid config/replay,
  GPU/final-exchange fence failures, body reorder/count/field changes, supersession,
  delayed/failed retirement and optional-camera/empty-scene recovery pass. Saved
  replay PNGs, ten baseline and 22 gallery PNG hashes remain exact. No blocking
  fence polls, server waits or bulk readbacks are observed during reload progress.
- README, journal/PDF and retained evidence are updated in
  docs/journal/architecture/terrain-gpu/async/interactive/reload/. Resume T3c4c
  using interactive/plan.md: native compute input/movement/flight/trail/body-switch,
  shadow/reflection and R/file-watch reload acceptance before public CLI opt-in.
  CPU remains default, startup/CLI compute gates remain and T3c5 hardware cost
  acceptance is still required. User environmental/Docker TODO suffix is preserved.

## Asynchronous resident frame checkpoint — 2026-10-05

- T3c4a is tested. Resident startup/movement/local-mask preparation uses the
  persistent CPU worker, retains its bounded completed slot under GPU backpressure
  and polls GPU readiness/retirement with zero flags/timeouts. Complete consumers
  publish before character updates and managed draws. Initial loading defers
  walking and scene draws while the event loop remains active.
- Grass-only failure keeps the previous patch and contacts; failed terrain work
  retries unchanged inputs after 60 preparation frames. Stale mode results are
  rejected. Delayed/failed retirement retains ownership, admission and usable
  current draws. All-body contacts/main/shadow/reflection/water/grass receipts
  remain matching; CPU render vectors stay empty on the resident path.
- Three actual GL frame cases pass (20.148 s), plus eleven worker cases.
  All 61 CTest groups pass in one uninterrupted frozen-input run (927.29 s);
  source/input/six-binary fingerprints remain unchanged. Ten baseline and 22
  gallery PNG hashes remain exact. Zero positive-timeout/flush polls, server waits
  and bulk buffer readbacks are observed in the frame cases. README, journal/PDF
  and evidence are updated in docs/journal/architecture/terrain-gpu/async/interactive/.
- Resume T3c4b using interactive/reload-plan.md: nonblocking future-epoch worker
  ownership, one off-live body at a time, final prospective grass anchors and
  complete scene exchange. T3c4c native input acceptance must pass before opening
  interactive compute CLI. CPU remains default; T3c5 hardware gates remain.

## Movement and destination recovery checkpoint — 2026-10-05

- T3c3c3 and parent T3c3/T3c3c are tested. Prospective contacts cover every body;
  destination contact binding occurs on the handoff frame and flight/chase queries use the
  matching triangle planes. The old Moon handoff reported contacts 0 vs land 2;
  the corrected handoff binds Moon revision 2 immediately.
- Future-epoch reload supersedes queued/ready old CPU work and discards executing
  old completions before exclusive preparation, retaining the live epoch on
  failure. One running, queued and ready slot remains the bound. Normal frames
  still poll; reload waits belong to the compute capture adapter.
- Three renderer recovery cases pass (73.010 s); eleven worker cases pass
  (0.084 s); existing CPU space-flight/Moon exact replay passes (42.37 s).
  Continuous 12 m/s movement crosses terrain rebuild thresholds; invalid config/
  GPU failure retain instantaneous pose and old consumers; repeated reorders and
  field changes recover. Stale ready results cannot cross replacement, and saved
  Moon reload/fresh replay images are exact. Old buffers live until retirement;
  observed logical overlap reaches 13,179,460 bytes, below existing limits.
- All 60 CTest groups pass across an interrupted run and resumption on frozen
  inputs (952.49 s summed passing-group time). Source/input/five-binary fingerprints
  remain unchanged. Ten baseline
  and 22 gallery PNG hashes are retained. README, evidence and final journal/PDF
  are updated. See `docs/journal/architecture/terrain-gpu/async/recovery/study.md`.
  Resume T3c4 interactive opt-in; CPU stays default and interactive compute gated.
  Hardware total-frame and physical-memory gates remain T3c5.

## Renderer scene reload checkpoint — 2026-10-04

- T3c3c2 is tested. R/file-watch and Renderer::reload() share one transaction:
  config/replay snapshots, effective offline CPU scene, worker topology/contacts,
  exact body-local replay and prospective chase anchors, grass and preallocated
  tracking remain off live consumers until every body is ready. CPU and legacy
  compute use equivalent staged mesh/grass owners and the same reload boundary.
- Future-epoch exclusive preparation reuses the persistent worker without changing
  its live epoch on failure. Complete exchange advances the epoch, swaps tracking
  and replay/options, rebinds cameras/contacts, resets character/effects/input and
  invalidates frame/shadow/reflection/orbit caches. Replay trail/exhaust validation
  precedes exchange; later file changes do not affect the committed snapshot.
- One whole old scene remains owned until last-use retirement. Capture explicitly
  waits; interactive CPU retirement polls with zero timeout. Resident admission
  includes old resources under existing logical limits and the exclusive lease.
- Five renderer reload cases pass (58.680 s): changed count/field/camera,
  seven preparation/final-fence failures and staged-buffer cleanup, retained old
  draws and same-renderer recovery, CPU/legacy invalid-config recovery, exact fresh
  captures, stationary third-person replay and rejected trail/exhaust/pipeline/
  versions, file mutation after staging, optional-camera removal and repeated orbit
  reloads. Generation-owned cleanup probes reuse the bounded field-parameter cache.
- Nine worker cases pass (0.085 s); all seven owner cases (33.306 s)
  and 31 native compute cases (73.731 s) pass. All 59 CTest groups pass in one
  uninterrupted final-binary run (1051.70 s); ten compute/CPU/replay PNG hashes match
  T3c3c1 exactly. Source/input/application/native/runtime binaries remained frozen.
- The 28-page final journal was visually reviewed on pages 5–8 and 26–28.
- README, study, journal/PDF and evidence are updated; all 22 gallery hashes and
  the user's environmental/Docker TODO suffix are preserved. See
  `docs/journal/architecture/terrain-gpu/async/reload/study.md` and `validation/`.
- Resume T3c3c3: rapid movement/body switches/Moon arrival through reload/failure/
  stale-work recovery. T3c4 asynchronous interactive opt-in and T3c5 hardware
  acceptance remain pending. CPU stays default; compute interactive remains gated.
  Static renderer pipeline changes require a new renderer; logical admission is
  not total CPU/driver VRAM and llvmpipe supplies no hardware FPS claim.

## Whole-scene replacement ownership checkpoint — 2026-10-04

- T3c3c1 is tested. `SceneTerrainReplacement` owns an off-live validated CPU
  scene/config, cameras/fields and resident land/water/grass/contact consumers
  for every body. CPU topology comes from the existing scheduler; no worker is
  added. One exclusive replacement/retirement lease prevents competing preparation.
- Admission charges old published/retiring terrain sets before replacement GPU
  dispatch under the existing 512 MiB per-set/1 GiB aggregate logical ceilings.
  Every body must be ready; exact request and live consumer/config identities
  precede a last-use fence and no-throw complete scene/grass/trail/receipt exchange.
- The exchanged owner retains the entire old scene until successful zero-timeout
  retirement, preserving both resources and external reservation after timeout
  or poll failure. Only retirement deletes old resources and releases the lease.
- All seven scene-owner cases pass (25.031 s): delayed readiness/retirement,
  actual old terrain/water/grass draws, disabled consumers/trails, six preparation
  fence failures with staging cleanup, whole-scene fence failure/same-owner retry,
  failed retirement poll/recovery, eleven changed request identities, superseded
  epochs, changed live revision/contact/config, invalid config/overlap/admission
  and six replacements changing body count/order/fields and mounted cameras.
- All 31 existing native compute cases pass (48.264 s). All 59 CTest groups pass
  in one uninterrupted final-binary run (731.72 s); all ten compute/CPU/replay PNG
  hashes match T3c3b exactly. Source/input/application/native/runtime binaries
  stayed frozen through the run. Owner API checks are distinct from renderer reload.
- README, study, journal/PDF and evidence are updated. The 27-page journal was
  reviewed on pages 5–7 and 25–27; all 22 gallery hashes and the user's environmental/
  Docker TODO suffix are preserved. See
  `docs/journal/architecture/terrain-gpu/async/scene/study.md` and `validation/`.
- Resume T3c3c2: connect the owner to actual renderer reload, exchange preallocated
  tracking arrays, preserve chase/replay anchors, rebind cameras/contacts and
  invalidate shadow/frame/reflection caches. The current reload adapter remains
  unchanged. T3c3c3 movement/body-switch/Moon acceptance precedes T3c4 interactive
  opt-in. CPU stays default and compute interactive stays gated; logical payload
  is not total CPU/driver VRAM or a hardware FPS claim.

## Renderer generation publication checkpoint — 2026-10-04

- T3c3b is tested. Complete resident capture consumers publish at the
  pre-character/frame boundary. Owned grass-only replacement preserves land,
  water and sparse contacts, with bounded admission and fenced patch retirement.
- Contact planning previews restore CPU motion, cameras, contact cache/binding,
  selected bodies, input presses and replay state before GPU submission. Actual
  character steps consume the committed generation and match the prospective
  chase eye exactly. Effects and trails advance only in the real step.
- Managed scene passes validate installed keys, revisions and grass anchors
  before draws. Capture evidence records main/shadow/reflection/water/grass
  consumption and contact binding; changed land invalidates shadows and all
  publications invalidate whole-frame reuse.
- All 31 native cases pass (52.055 s), including ten transaction cases and
  grass-only transfer/failure/retirement checks. Two focused renderer cases pass
  (13.408 s), proving matching consumers after replanning and retention of old
  buffers/sidecar through a publication failure followed by same-renderer recovery.
- All 59 CTest groups pass in one uninterrupted final-binary run (789.49 s).
  All ten compute/CPU/replay PNG hashes match T3c3a exactly. Walking checks six
  matching previews, committed contacts and replay grass anchors. Frozen source,
  input, application/native/runtime binaries and draw evidence are retained.
- README, study, journal/PDF and evidence are updated. The 26-page journal was
  reviewed on pages 5–7 and 24–26. All 22 gallery hashes and the user's
  environmental/Docker TODO suffix are preserved. See
  `docs/journal/architecture/terrain-gpu/async/renderer/study.md` and `validation/`.
- Resume T3c3c: keep the old scene drawable through complete replacement GPU
  readiness and validate reload, movement/body switches and Moon arrival.
  T3c4 asynchronous interactive acceptance and T3c5 hardware gates remain pending;
  CPU stays default, legacy/GL 3.3 paths remain and compute interactive stays gated.
  Capture explicitly waits; logical reservation is not physical VRAM or hardware FPS.

## Whole-generation publication checkpoint — 2026-10-04

- T3c3a is tested. `TerrainPublication` owns complete off-live land/water/grass/
  contact resources and a publication receipt with distinct terrain/grass anchors,
  settings and face statistics. All fallible validation/allocation precedes
  readiness; publication uses no-throw ownership swaps and advances revisions once.
- Admission precedes dispatch and accounts for published, pending and retiring
  sets: 512 MiB per set, 1 GiB aggregate logical ceilings, one global GPU
  preparation and one preparing/retiring spare per body. Zero-timeout last-use
  fence polling keeps old consumers alive until retirement completes.
- All 29 native cases pass (51.154 s), including eight new transaction cases.
  Six injected fence failures (land/water/metadata/allocation/draw resources/
  retirement) preserve live consumers and release every generated staging buffer.
  Ten stale identities, delayed preparation/retirement, actual old-resource draws,
  overlap rejection, changed live destinations, disabled consumers and repeated
  body replacements pass. This tests APIs, not renderer integration.
- All 59 CTest groups pass in one uninterrupted final-binary run (756.95 s).
  All ten compute/CPU/replay PNG hashes match T3c2 exactly; worker, lifecycle/
  reload, actor, flight, grass, atmosphere, native input and adaptive quality pass.
  Source/input/application/native hashes are frozen for the run and retained.
- README, study, journal/PDF and evidence are updated. The 26-page journal was
  reviewed on pages 5–7 and 24–26; all 22 gallery hashes and the user's environmental
  Docker TODO suffix are preserved. See
  `docs/journal/architecture/terrain-gpu/async/publication/study.md` and `validation/`.
- Resume T3c3b: connect this owner to renderer frame boundaries, preserve chase/
  replay grass anchors, rebind contacts and refresh shadow/frame/reflection
  consumers before draws. T3c3c must keep the prior scene through replacement GPU
  readiness and validate movement/body-switch/reload recovery. Capture terrain
  and grass still install separately; CPU stays default and interactive compute
  remains gated. Logical payload is not physical VRAM or a hardware FPS claim.

## Asynchronous GPU preparation checkpoint — 2026-10-04

- T3c2 is tested. `TerrainGpuPreparation` owns submitted land/water; resident
  grass uses separate submission/poll/commit in `GrassPreparation.cpp`. GPU
  dependencies use ordered commands/barriers; polling uses zero timeout and
  terrain timestamps are read only after query availability.
- Grass reads one completed 224-byte summary, validates prefixes, slot caps,
  totals and keys, then creates texture views, placement program, blade queues,
  indirect commands and VAOs before a resource fence permits commit. Failed or
  stale preparation leaves the live patch intact. The exchange retains the old
  patch in its preparation owner; fenced retirement remains T3c3.
- Logical admission reserves land/water and worst-case resident grass under an
  initial 512 MiB per-preparation ceiling. The reduced capture admits 4,632,148
  bytes and allocates 524,320 draw bytes for 4,096 candidates. This is logical
  payload, not physical VRAM/free-memory or a hardware speedup claim.
- All 59 CTest groups pass in one uninterrupted application-binary run (733.29 s),
  including 20 native compute cases. A test-only controlled delayed-fence probe
  was added afterwards; all 21 native cases pass (24.294 s), including seven new
  preparation cases. The application hash remains identical; both native scopes
  and their input/binary hashes are recorded in retained evidence.
- All ten compute/CPU/replay PNG hashes match T3c1 exactly, including legacy and
  GL 3.3 fallback. Worker, lifecycle/reload, flight, grass, atmosphere, input and
  adaptive quality pass. README, study, journal/PDF and artifacts are updated;
  all 22 gallery hashes remain unchanged. See
  `docs/journal/architecture/terrain-gpu/async/gpu/study.md` and `validation/`.
- Resume at T3c3: publish complete consumers at one frame boundary, retain the
  previous scene on reload/allocation failure and fence old-resource retirement.
  Capture terrain/grass still install at separate stages; interactive compute
  stays gated until T3c3/T3c4 acceptance. Preserve CPU default and old replays.

## Bounded terrain CPU worker checkpoint — 2026-10-04

- T3c1 implementation is present in `src/rendering/geometry/jobs/`: value-owned
  snapshots, epoch/body/field/mode/serial identities, one persistent executor,
  one coalesced queued request and one bounded completion slot. Workers construct
  compute land/water topology and matching sparse contacts without render vectors.
- Runtime futures are removed. Reload advances an epoch without joining old work;
  shutdown joins before trace/context lifetime ends. Normal CPU walking polls,
  rejects incompatible/older completions and queues the closest eligible body.
  Compute captures explicitly wait for CPU preparation; GPU waits remain until
  T3c2/T3c3, and interactive compute remains gated.
- Eight new CPU cases cover controlled delays, coalescing/backpressure, repeated
  reload, body/field/mode/serial matching, exceptions, shutdown and independent
  resident snapshots. Capture integration adds worker-bound/transfer assertions.
  Build, all eight targeted cases (73 ms) and all 59 CTest groups pass in one
  uninterrupted final-binary run (736.20 s). Compute capture records two completed
  jobs, peak running/queued counts of one and no pending/errors/stale publication.
  All ten compute/CPU/replay image hashes match T3b2 exactly; GL 3.3 fallback,
  renderer lifecycle, reload, flight, atmosphere and native input pass.
- README, study, retained evidence and journal/PDF are updated. All 22 gallery
  hashes remain unchanged. Contact ownership adds a temporary canonical CPU
  topology copy during staging; no total-memory or hardware-FPS saving is claimed.
  See `docs/journal/architecture/terrain-gpu/async/worker/study.md` and its
  `validation/` directory for source/binary/artifact provenance.
- T3c1 is tested. Resume at T3c2 in `docs/journal/architecture/terrain-gpu/async/plan.md`:
  split GPU/grass submission, polling and commit; preallocate resources and bound
  spare sets. T3c3 still owns atomic publication/reload recovery; interactive
  compute remains gated until T3c4. Preserve the CPU default and old replays.

## GPU grass allocation and mirror removal checkpoint — 2026-10-04

- T3b/T3b2 are tested. Resident GPU metadata feeds deterministic hash selection,
  rounded-slot density search and ordered group/bucket references. Fresh compute
  captures save planner `gpu-v1`; legacy sidecars retain CPU planning. CPU terrain
  remains default and interactive compute is still gated.
- Compute land/water adopt GPU buffers without full CPU float render vectors or
  bulk field evaluation. Draw counts and navigation diagnostics use resident
  generation information; sparse contact topology/cache remain CPU-owned.
- Production 100,000-triangle probe: 13,019 patches, 1,048,429 candidates within
  a conservative 1,048,576 budget, 2,840 metadata/allocation input bytes and one
  224-byte summary read. Metadata/allocation storage is 6,400,160/1,456,556 bytes;
  cached preparation uploads/reads nothing. Queried placement limits and bounded
  dispatches retain global root identity. Available-VRAM/density feedback is B1.
- Camera-only replay without a `render` object now selects legacy planning unless
  explicitly overridden. Unknown planners, incompatible locked configurations,
  stale/malformed sources and limits reject; explicit CPU/GL 3.3 fallback remains.
- Final executable passes all 58 CTest groups across two runs: 1–29 before
  interruption and 30–58 on resume (785.19 s). Passing-group durations sum to
  883.03 s; this is not a single uninterrupted full-suite run. Coverage includes
  14 native terrain/metadata/allocation cases and six renderer lifecycle cases.
  Exact compute/walking/legacy replay, CPU override, atmosphere, shadow/reflection,
  character/exhaust/space-flight, live input and quality checks pass.
- Four-case allocation and roughly two-million-candidate legacy replay logs were
  regenerated on the final binaries for direct provenance. Their binary hashes
  are retained; the legacy PNG matches the gallery exactly.
- README, studies and journal/PDF updated. All 22 gallery hashes are retained;
  layout, local links and whitespace pass. Journal is 25 pages; architecture and
  evaluation/reference pages visually reviewed. Evidence is in
  `docs/journal/architecture/terrain-gpu/grass-allocation/validation/`.
- Resume at T3c1. The source-audited asynchronous plan is
  `docs/journal/architecture/terrain-gpu/async/plan.md`; T3c1–T3c5 rows define
  bounded CPU workers/contact construction, asynchronous GPU/grass staging,
  complete atomic publication/reload recovery, interactive opt-in and hardware
  acceptance. Implementation remains pending. No hardware FPS improvement is
  claimed. A1/B1, astronaut replacement, trail fading, BSON and effects remain queued.

## Resident grass metadata checkpoint — 2026-10-03

- T3b is split into T3b1 metadata and T3b2 deterministic allocation/mirror removal.
  GL 4.3 compute captures with compute placement now generate 64-byte descriptors
  directly from resident terrain buffers: original IDs, double bounds/distances,
  conservative biome eligibility and Gaussian weighted area.
- A 160-byte parameter pack plus eight control bytes per dispatch is uploaded;
  all work/storage limits are queried, actual source sizes checked, dispatches
  bounded and source generation/planning eye retained. Indexed SSBO ranges and
  program state restore on success/failure; RAII owns outputs and fences.
- CPU grass allocation remains authoritative. Resident metadata is prepared for
  T3b2; this step adds GPU work/memory and claims no frame or process-memory saving.
  No new CPU height queries, geometry uploads or renderer metadata readbacks.
- Four new native cases validate CPU eligibility/weight parity, poles/shoreline,
  sigma/tint/water/disabled/distant cases, packing/stride, forced chunks, limits,
  malformed sources/keys, GL ranges, cached zero-upload and disable cleanup.
  A 100,000-triangle oracle uploads 176 bytes across two dispatches and retains
  6,400,160 bytes; maximum weighted-area relative error is 2.20116e-14.
- Compute surface/walking PNGs match T3a exactly; both replays, explicit CPU
  override and GL 3.3 fallback pass. All 22 gallery hashes/provenance retained.
- README, journal/PDF and study updated. PDF has 25 pages; pages 5–7 and 24–25
  reviewed. Evidence in docs/journal/architecture/terrain-gpu/grass-metadata/.
- Build and all 58 CTest groups pass (1092.60 s), including ten total native GL
  terrain/metadata cases. Resume at T3b2: GPU
  rounded-slot density search, deterministic budgeted allocation, placement
  limits, versioned replay and CPU-vector removal. T3c asynchronous consumers,
  A1/B1, trail fading and BSON evaluation remain queued.

## Sparse terrain contact checkpoint — 2026-10-03

- T3a complete in `src/rendering/geometry/contacts/`. Immutable radial/sink
  topology and CPU field feed a BVH; index construction evaluates no heights.
  Only query candidates receive float-rounded/sunk positions, cached up to 1,024.
- Compute astronaut feet/chase bind this source independently of full CPU render
  vectors. Matching field/topology/backend keys are checked before mesh adoption;
  failed source construction/adoption preserves the previous mesh. Shared ownership
  and rebinding keep generation lifetime/cached triangle IDs consistent.
- Positive radial bounds include float rounding and edge tolerance. Stale,
  malformed and collapsed inputs reject; missing plane coverage fails explicitly.
  On-demand indexed queries extend coverage across fast steps/teleports without
  GPU readbacks or full-body evaluation. Existing sea clamp and analytic camera/
  noncurrent-body flight floor probes remain.
- Production 100,000-triangle / 50,002-endpoint topology: 262 distributed queries
  evaluate 2,871 heights, visit 4,332 candidate triangles / 16,576 nodes, retain
  823 positions after eviction and reuse 100 resting probes without new work.
  Logical topology/index bytes are 2,800,064 / 2,497,088; construction measured
  163.732 ms on this CPU. Worker scheduling remains T3c; no FPS/memory saving claimed.
- Initial float storage is explicitly materialized: exact position-bit tests
  caught compiler excess precision across the conversion before sinking.
- Clean build and 58/58 CTest groups pass (664.17 s), with four new sparse
  CPU cases and six total native compute cases. Pole/edge/shoreline/SI/noise parity,
  bounded cache, stale/missing coverage, generation lifetime, mismatched adoption
  and poisoned full CPU-vector independence validated.
- Six-frame walking capture uses 14 contact vertices. Surface and walking PNGs
  match the T2 hashes exactly; compute replay, trail replay, explicit CPU override
  and real GL 3.3 fallback pass. All 22 gallery hashes and provenance are unchanged.
- README and journal/PDF updated; 24 pages, pages 5–7 and 24 reviewed. Retained
  evidence is in `docs/journal/architecture/terrain-gpu/contacts/validation/`.
- Resume at T3b (GPU grass metadata and deterministic slot planning, then remove
  compatibility vectors), followed by T3c (CPU worker index construction,
  asynchronous complete consumer generations, stale/reload/failure recovery and
  hardware cost gates). Interactive compute and the overall GPU terrain task
  remain in progress. A1/B1, trail fading and BSON evaluation remain queued.

## GPU terrain compute checkpoint — 2026-10-03

- T2 complete in `src/rendering/geometry/compute/` and
  `shaders/terrain/compute/`: GL 4.3 double field/gradient/material/sink evaluation,
  canonical endpoint expansion and sequential draw indices generated on GPU.
- Opt in with `--terrain-backend compute` for capture outputs; default remains
  CPU. Saved compute replay retains its backend/version requirements. Explicit
  compute requests record a CPU fallback on GL 3.3; locked compute replays reject
  an unavailable backend. Unknown versions and malformed backend selectors reject
  before window creation; explicit CPU override remains available.
- Queried SSBO/dispatch limits, bounded parameter caching, barriers/fences and
  RAII staging implemented. Capture waits for both land/water outputs before
  publishing. Normal interactive asynchronous consumer installation is still T3.
- Production 100,000-triangle / 50,002-endpoint generation transfers 2,800,880
  bytes versus the CPU backend's 12,000,000-byte draw upload (76.66% reduction).
  Native logical generation storage is 16,600,848 bytes; this is not whole-scene
  VRAM. Six bounded dispatches; output readback has zero float position/color
  error and maximum height error 1.194e-12 m on llvmpipe.
- Full CPU compatibility vectors and 150,006 production bulk field queries remain
  for grass planning/contacts. Stage timings are retained, without a hardware or
  total-frame speedup claim. Removing this duplicated work is the next phase.
- Clean build and 58/58 CTest groups pass (645.09 s). Five native GPU cases cover
  packing, poles/lattice/extreme seeds, SI scales, mixed LOD/seams/winding/sinking,
  payloads and failed/incomplete-generation preservation. Capture integration
  covers exact compute and walking/trail replay, CPU override and real GL fallback.
- Native shoreline (960×540) and dense offline (1920×1080, 213,454 visible blades)
  PNGs match the previous gallery exactly. All 22 gallery hashes/provenance remain
  unchanged. New evidence is separate under
  `docs/journal/architecture/terrain-gpu/compute/`. README and journal/PDF updated;
  24-page journal reviewed on pages 3–7 and 24.
- Resume at T3 in the architecture plan: GPU grass metadata/slot allocation,
  sparse matching ground-contact mirror, asynchronous complete consumer generations,
  stale/reload/allocation recovery and hardware total-cost measurements. A1/B1,
  trail-history fading and BSON evaluation remain queued after terrain work.

## Terrain field/topology extraction checkpoint — 2026-10-03

- T1 complete in `src/rendering/geometry/terrain/`: read-only field oracle,
  explicit 704-byte parameter pack, 32-byte radial/sink inputs, exact-bit bounded
  height cache, indexed topology and field/topology/backend generation keys.
- TerrainSurface now plans topology before CPU bulk evaluation. Shoreline uses
  temporary legacy float-rounded positions; scratch is discarded on publication.
  CPU output still expands corners for unchanged grass/contact/replay semantics.
- Mesh retains generation/query/input-byte diagnostics; captures report them.
  CPU planning still samples heights; no GPU shaping or runtime upload reduction
  is claimed at this prerequisite stage. Existing GPU/CPU grass paths remain.
- Saved pre-refactor executable and 17 independent mesh hashes under ignored
  `build-resume/terrain-contracts/`; contract tests retain 15 smaller fixture hashes.
  New tests check layout, negative/high-frequency seeds, poles, SI units, cache
  bounds, reproducible keys, stale-field and malformed-topology rejection.
- Clean full build and 56/56 CTest groups pass (641.75 s), including seven new
  contract cases. All 17 baseline meshes and native shoreline/offline PNGs match
  exactly. Cache reload and stale-topology rejection, both poles, units, shoreline,
  foliage, contacts, shadows, atmosphere, exact replay and live input are covered.
- README and journal/PDF updated; 24-page journal reviewed on pages 3–6 and 24.
  All 22 gallery hashes remain unchanged. New evidence is retained separately in
  `docs/journal/architecture/terrain-gpu/contracts/validation/`.
- Next queued phase is opt-in GL 4.3 bulk field evaluation (T2), followed by GPU
  grass planning, sparse matching contacts and atomic dependent consumers (T3).

## CPU–GPU architecture checkpoint — 2026-10-03

- Planning prerequisite complete in `docs/journal/architecture/terrain-gpu/plan.md`.
  Audit anchored to `5993b23`; no renderer/shader/config/image changes.
- Retain the existing 320 icosahedral triangle panels. Hexagonal dual/H3,
  cube-sphere and latitude/longitude alternatives assessed, including twelve
  pentagonal exceptions, approximate H3 child boundaries, poles and seams.
- CPU owns topology/LOD/shoreline division/sinking, resource lifecycle and sparse
  simulation contacts. GPU owns bulk field/gradient/material generation and
  grass metadata/slot planning; all draw/contact consumers install one generation.
  Existing GPU atmosphere LUT/integration stays; remaining metering work is planned.
- Precision/packing, GL limits and GL 3.3 fallback, exact locked replay, contact
  corridor coverage, stale-job/reload/allocation recovery and acceptance gates
  documented. Foliage policy protects near density before reducing far shoulder;
  impossible budgets and missing telemetry have explicit fallbacks.
- Analytical worksheet validates manifold levels 0–3, Euler closure/12 pentagons,
  current 128-byte/candidate queues and estimated indexed transfer savings.
  This is not a measured GPU speedup. Documentation links/layout and journal
  compile/visual review recorded under the plan's `validation/` directory.
- At this planning checkpoint T1 was queued; it is now complete above. The CPU
  backend is preserved. Next are opt-in compute proof T2 and completion of
  dependent consumers T3; default enablement requires correctness/performance gates.

## Offline showcase checkpoint — 2026-10-03

- Current row: dense offline README example. Production LLA, clearance, NED
  direction, FOV and timestamp are retained exactly. Production config is not
  edited. Showcase changes only explicit foliage budget (12,000,000), Gaussian
  sigma fraction (0.05) and per-triangle slots (65,536); derived radius is 3 km.
- `shaders/postprocessing/flare.frag` spreads and strengthens the halo/streak
  and aperture ghosts. The controlled visible/occluded/replay test passes with
  a meaningful lower-half ghost signal (80+ pixels above 24 channel levels).
- Capture telemetry now snapshots main-view compute queues before water
  reflections overwrite them; optional output avoids interactive readbacks.
  `ScenePass.h/.cpp` and `Capture.cpp` updated; build passes.
- `scripts/benchmarks/offline_showcase.py` renders baseline, dense, no-flare,
  exact replay and central 1/10-width allocation probe. Native 1920x1080 run
  completed in `build-resume/offline-showcase/final`; all five PNGs and cached
  capture audit pass, including exact dense replay and 49,648 flare pixels
  changing by at least eight channel levels. Retained under
  `docs/journal/benchmarks/offline/showcase/`. Individual `--capture` batches
  and final `--resume` audit are supported.
- The narrow view cannot reduce candidate queues or terrain meshes in the
  current eye-centred planner. Record measured counts, exposure/flare/reflection
  constraints; defer column assembly until it can actually save allocations.
- Dense 1920x1080 PNG and replay published; all 22 gallery hashes pass and 21
  previous PNG/provenance rows are unchanged. Production config and model assets
  are untouched. Journal PDF compiles (24 pages); pages 11–15, 23–24 reviewed.
- Complete regression coverage: all 56 CTest entries pass across six grouped
  Xvfb runs (609.22 s): 1–32 (40.42 s), 33–34 (173.67 s), 35–39 (109.31 s),
  40–44 (177.70 s), 45–47 (24.56 s), 48–56 (83.56 s). Logs, build evidence,
  source/binary fingerprints and artifact hashes retained under the study's
  `validation/` directory. Layout, whitespace and gallery/provenance checks pass.
- This task is complete. The next queued task is the CPU–GPU terrain/atmosphere
  architecture plan, including panel topology and view-aware allocation, before
  any GPU terrain implementation.

## Astronaut candidate checkpoint — 2026-10-03

- Three originals, textures and licenses retained in
  `USER_IO/astronaut_vis/models/`: `astrodev`, `polygonal-astronaut`,
  `polygonal-cosmonaut`. Prepared geometry is 1,442 / 3,478 / 4,506 triangles,
  with 25 / 58 / 78 bones. AstroDev is CC0; Polygonal Mind copies retain the
  creator repository's CC BY 4.0 license and attribution despite CC0 VRM tags.
- The prepared versions have no backpack. AstroDev's integrated pack is
  removed and the closed torso capped in the original white palette; Square
  Cosmonaut's disconnected pack is removed; #048 originally has no pack.
  Sources retain original geometry. These are accessible stylized candidates,
  less detailed than Ava Turing; Sketchfab downloads require authentication.
- `scripts/character/prepare_models.py` builds packed Blender/GLB outputs,
  normalizes four skin weights and creates a one-second `InspectionWalk`.
  Connected thigh/shin/foot chains and 31 finite bounded poses per model pass.
  The GLB has one named clip. Optional Draco isn't used; importer compatibility
  handles Debian Blender 3.4's old NumPy alias.
- `scripts/character/render_models.py` imports actual GLBs, checks 17 poses
  each, loop closure/skin weights/chains, and renders front/back/walking poses
  at 900x1080, Eevee 48 samples, software GL with eight workers. All nine PNGs
  visually reviewed; clear backs and full limbs confirmed. After an interrupted
  all-model process, individual candidate/view batches completed successfully;
  CLI options allow resumable renders. Log: `USER_IO/astronaut_vis/validation/render.log`.
- Comparison assembled with ImageMagick `convert +append`; candidate README
  records reproduction commands. Independent `scripts/character/check_models.py`
  passes both `--record` and verification: three self-contained GLBs, embedded
  textures, original source hashes, animated leg chains, normalized weights,
  nine PNGs, 51 imported poses, folder limits and every manifest artifact hash.
- All 22 unchanged game-gallery images still match their generation hashes.
  CTest RepositoryLayout passes (1/1, 0.27 s). Journal PDF rebuilt successfully;
  pages 20–24 visually reviewed, including the three-candidate figure on page 22.
  Asset evidence/logs are retained in `USER_IO/astronaut_vis/validation/`.
- README, `docs/journal/character/assets.md`, candidate README and journal/PDF
  are updated. This task does not integrate a model into the game or change
  C++/shaders/configuration. Asset clips are deformation probes; production
  gait/contact/ozz retargeting remains separate work after selection.
- Candidate row is t. Next unfinished task: improve the offline/raytracing
  README example with production initial position, denser foliage and visible
  lens flare; evaluate column rendering and assembly before changing the renderer.

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
|Download three diffrent high quality astronaut models, and make a render of each and drop it into USER_IO/astronaut_vis. Only choose models that we can actually use for walking motions and that have no backpack, such that we can mount a jetpack ourself|t|Three downloaded stylized humanoid candidates in USER_IO/astronaut_vis: AstroDev, Polygonal Mind Astronaut #048 and Square Cosmonaut #111. Clear backs, packed Blender files, GLBs, retained sources/licenses, nine reviewed renders and comparison. All 93 prepared and 51 imported poses, independent GLB/PNG/hash audit and repository layout pass; journal/PDF rebuilt and pages 20–24 reviewed. Less detailed than Ava Turing; runtime integration remains separate.|
|Improve the offline / raytracing example in README.md: increase foliage count and use the initial surface position from the production config; consider rendering narrower vertical columns with higher terrain/foliage budgets and assembling them into the final image; make lens flare visibly apparent. Start after the preceding tasks are finished.|t|Native 1920×1080 production-start capture with 213,454 main-view blades versus 102,918 baseline and visibly stronger gated flare; exact replay passes. Narrow-column probe retains the same candidate/terrain allocations, so assembly awaits view-aware streaming in the architecture plan. All 56 CTest entries pass across six grouped runs (609.22 s); README, replay, journal/PDF and 22 gallery hashes verified. See docs/journal/benchmarks/offline-showcase.md.|

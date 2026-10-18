# Page to document progress of AI Agents working on this Project

## Private CPU grass preparation — 2026-10-18

- Continuing terrain **p** after `b1b969e`. Added
  `ProceduralGrass::submitCpu` through existing preparation/commit ownership.
  A scoped private patch runs the ordinary CPU planner, keeping the published
  patch/policy/draws intact. Admission charges the detached old patch and new
  terrain bytes. Returned admitted bytes cover the replacement, to be held by
  its publication owner alongside live/retiring resources. No runtime activation.
- Replay eyes stay queued across stage failures/cancellation. Commit validates
  body/mesh/source/replay identity, consumes the eye once and transfers the old
  patch to preparation ownership for retirement. Disabled CPU foliage is valid;
  current retained phase attaches on commit. Descriptor preparation is synchronous
  and ordinary CPU draw-time compute caches remain lazy.
- New regression tests exact live output, policy/budget/eye preservation, errors
  after real descriptor writes and buffer deletion, overlap rejection, cancellation,
  stale mesh/replay commits, successful transfer and disabled foliage. Corrected
  an initial test expectation: restorePlanningEye intentionally invalidates the
  public planning receipt while keeping old draw data. Existing retained CPU
  tests now prepare privately on GL3.3/vertex and native compute routes.
- Focused three cases pass GL3.3 2.226 / RTX 1.848 / Quadro 1.851 s. Across
  baked/live modes: 5,362 root queries, 10,240 unchanged native cache records,
  max plane errors below 0.231 mm. All 35 core groups pass (73.61 s), 58 RTX
  cases (102.156 s), and 26 software grass/render cases (15.660 s). Build
  (67 steps) and test correction rebuild (2 steps) pass. Headless inspected and
  exact: 14,779 non-background pixels, bounds `(1,1)–(794,598)`, 3,471 planet
  pixels. Recovery/frame/reload/streaming all pass (297.09 s), with 15 historical
  PNGs byte-exact. Final two-step test rebuild and strengthened overlap oracle
  pass on GL3.3/RTX/Quadro: terrain-only admission succeeds while combined
  old/new foliage fails at the same cap.
  Logs: `build-f5/resume/detail-cpu-stage-*`; timings are validation, not FPS.
- User also queued mode 2 astronaut steering/view 2 trails/camera 4 zoom as
  the last feature row, separately committed in `5529b38`.
- Keep **p**, default `Transient` and prior native coverage failures. Next:
  complete CPU terrain/water/grass publication, coverage policy, live runtime
  scheduling and parent-rebase continuity. User scenario edits remain unstaged.

## Installed prepared GPU phases with rollback — 2026-10-18

- Continuing terrain **p** after CPU adapter commit `ab17035`.
  `TerrainPublication::updateInstalledPhase` validates installed mesh/contacts/
  grass identity and complete admitted halo coverage before live dispatch.
  Missing/static/replayed builds and scene-replacement leases return false;
  identical phases skip dispatch. Call before character/contact updates/draws.
- Failed live dispatch resubmits the previous footprint from immutable GPU
  targets before rethrowing; shared versions stay old. A second failure requires
  renderer recovery explicitly. Timing keeps the installed request identity.
  No CPU mirrors/readback, buffer allocation, fences or client waits are added.
  Pending terrain state remains private; grass-only commit attaches live state.
- Moving publication test now uses the installed API and paused grass draw
  before each replacement. New regression checks rejection, lease freeze,
  exact GPU rollback after real writes, unchanged contacts/version, successful
  retry, persistent-error recovery, pending terrain/grass-only updates and
  disabled foliage. Four focused cases pass RTX 20.661 / Quadro 21.317 /
  software 53.902 s. Ten swaps / 1,260 plane probes / 768 sampled roots retain
  max swap 0.0312387 mm and exact GPU/contact parity; max root errors RTX
  0.175509 / Quadro 0.163132 / software 0.137597 mm (unchanged 2 mm gate).
- All 35 core groups pass (75.30 s), all 58 RTX compute cases (103.561 s),
  and forced GL3.3 CPU/root regressions (2.726 s). Overview inspected/decoded,
  byte-exact: 14,779 non-background pixels, bounds `(1,1)–(794,598)`,
  3,471 planet pixels, 11,040 bright pixels. Validation timings are not FPS.
  Build (50 steps) and final source/test rebuild (23 steps) pass. Recovery/frame/
  reload/streaming integration all pass (307.38 s); all 15 historical PNGs stay
  byte-exact. Logs/captures:
  `build-f5/resume/detail-installed-phase-*`.
- Keep **p**, default `Transient` and previous native coverage failures.
  Runtime activation still needs coverage policy, phase scheduling, CPU prepared
  publication and parent rebases. User scenario edits remain unstaged.

## Live CPU deformation with shared contact/grass phases — 2026-10-18

- Continuing terrain **p** after `2dcd8b1`. Explicit
  `Mesh::prepareCpuDeformation` seeds shared state from the baked phase and
  preallocates reusable interleaved scratch. `deformCpu` evaluates immutable
  mapped endpoints, uploads the existing VBO and publishes CPU positions/state
  together. Generation/revision, attributes, indices/cells and targets stay fixed.
  Same phases skip upload; invalid shape/scale rejects before writes. Upload
  errors roll back old GPU contents; rollback failure requires renderer recovery.
- CPU scratch capacity is accounted, transferred with mesh swaps and released
  on fallback/destroy. Ordinary CPU uploads remain baked; explicit preparation
  must precede grass/contact binding. GL3.3 driver synchronization is possible;
  no FPS/nonblocking claim. Vector contacts now share phase versions through
  character, preview and reload bindings; grass reuses original roots/envelopes.
- Extended retained CPU tests cover both baked and live paths at pole/oblique
  orientations, dry/mixed/wet/return phases, reflection/paused animation,
  exact vertices/attributes/source buffers, ownership and rejection. A fault
  after actual VBO writes proves exact rollback and successful retry; identical
  phases prove zero upload calls. Shared vector contacts cover copies/rebinding.
- Real renderer CPU replay now advances the live phase while immutable baked
  metadata stays fixed, captures it, then reloads/replays exact PNGs. All eight
  existing backend/phase combinations pass (67.295 s). All 35 core groups pass
  (69.34 s), all 57 RTX compute cases (96.280 s), all 25 software grass/render
  cases (27.227 s). Two focused cases pass GL3.3 (1.884 s), RTX (1.733 s),
  Quadro (2.687 s): 5,362 root queries across baked/live modes, maximum plane
  errors 0.194173 / 0.230774 / 0.230762 mm respectively, and 10,240 unchanged
  cached records per native run. Validation timings are not FPS measurements.
- Headless overview inspected and byte-exact: 14,779 non-background pixels,
  bounds `(1,1)–(794,598)`, 3,471 planet pixels. Recovery/frame/reload/streaming all pass
  (284.72 s); all 15 historical PNGs remain byte-exact. Full build (87 steps) and final
  source/test rebuild (20 steps) pass. Logs/captures:
  `build-f5/resume/detail-cpu-live-*`.
- No runtime activation. Keep **p**, default `Transient` and previous native
  coverage failures. Next: coverage policy, installed live GPU phase handling,
  runtime scheduling and parent rebases. User scenario edits remain unstaged.

## Advance ready prepared phases before publication — 2026-10-18

- Continuing terrain **p** after `325406d`. Prepared worker results now carry
  their fine preparation radius (zero for static/replayed builds), transferred
  to publication receipts. Shared `terrainPhaseFitsPreparation` checks domain,
  scale and finite plan/radius values at worker admission, GPU admission and
  private phase updates. GPU admission rejects corrupt/inconsistent coverage.
- `TerrainPublication.updatePreparedPhase(body, phase, compute)` updates only
  ready prepared land. Missing/unfinished/static/replayed/grass-only work returns
  false; invalid coverage preserves staging; GPU error discards only staging and
  increments failed. Same footprint skips dispatch. Immutable roots survive;
  commit attaches the updated shared state for plane/animation refresh on draw.
- GPU publication test now starts 0.1 m behind the desired mask, updates privately
  before all ten swaps and expects phase2 across land/grass/contacts. Audits no
  buffer allocation/fences/waits during updates; out-of-range/coverage rejection,
  unchanged live phase and unsupported replay/grass-only updates are covered.
  New failure case injects an actual post-dispatch GL error, checks old consumers,
  resources and budget, then retries successfully. Worker tests check coverage
  scalar propagation, replay reset and malformed coverage helpers.
- All build rounds (70, 34 and 19 steps) plus final no-op pass. Review added
  old vertex-content preservation, configured-radius consistency (including
  finite forged radii), and fault audit proving repeated phases skip dispatch.
  GPU timing now binds phase work to the staged request. Focused worker/replay
  tests pass (7.060 s); three GPU cases pass on RTX (19.408 s) and Quadro
  (20.709 s); two software cases pass (34.679 s). Ten swaps / 1,260 probes /
  768 sampled roots: max swap 0.0312387 mm, exact GPU/contact parity and
  max root errors RTX 0.146162 / Quadro 0.157761 / software 0.131971 mm.
  All 35 core groups pass (72.16 s); all 57 RTX cases pass (95.012 s).
  Headless overview inspected/decoded and byte-exact: 14,779 non-background
  pixels, bounds `(1,1)–(794,598)`, 3,471 planet pixels. Recovery, frame,
  reload and streaming integration pass (280.95 s); all 15 historical PNGs
  remain byte-exact. Logs under
  `build-f5/resume/detail-prepared-update-*`. No runtime activation; user scenario edits remain unstaged.
  Next: runtime coverage policy, continuous live phase/CPU fallback and parent
  rebases. Keep **p**, default `Transient` and previous native coverage failures.

## Prepared worker requests — 2026-10-18

- Continuing terrain **p** after `c362269`. `TerrainBuildRequest.preparedPhase`
  requests a retained live-preparation build before its topology identity exists.
  It is exclusive with a replay record. Admission validates the phase/physical
  scale, streaming V4 and the chord-distance triangle-inequality bound placing
  the entire phase core/transition inside the configured fine preparation halo.
- Worker seals the new replay record after planning, rejects limited/absent fine
  geometry before bulk evaluation/contacts/GPU staging, and uses the existing
  retained CPU bake or mirror-free compute paths. Defaults and water unchanged.
- New worker case exercises CPU, nonresident compute and resident compute;
  prepared/replay parity, prospective contacts, malformed/ambiguous/uncovered
  requests, an infeasible halo and distant geometry, then request reuse. Shared
  microrelief setup extracted from existing worker replay test. The prepared GPU
  publication regression now uses this normal worker request instead of adding
  a replay record to its result after the fact.
- Full 70-step build and final no-op pass. Focused worker/replay tests pass
  (6.116 s); updated GPU publication/replay passes on RTX (15.430 s) and Quadro
  (16.044 s), max swap 0.0312387 mm, exact GPU/contact parity, sampled roots
  below 0.163 mm. All 35 core groups pass (71.16 s) and all 56 RTX cases
  pass (93.769 s). Headless overview inspected/decoded and byte-exact: 14,779
  non-background pixels, bounds `(1,1)–(794,598)`, 3,471 planet pixels.
  Recovery, frame, reload and streaming integration all pass (281.89 s);
  all 15 historical PNGs remain byte-exact. Logs:
  `build-f5/resume/detail-prepared-worker-*`. No runtime activation; user scenario edits remain unstaged.
  After this increment: runtime coverage admission/phase advance and parent
  rebases remain. Keep **p**, default `Transient` and previous native failures.

## Prepared GPU publication probe — 2026-10-18

- Continuing terrain **p** after `b045c38`. Added a staged GPU regression to
  `tests/geometry/publication/test_publication.cpp`. Shared microrelief request
  setup was extracted from the existing replay-phase test without changing it.
- New case advances the live footprint, stages the next prepared topology with
  that initial phase, checks that live buffers/revision/state remain unchanged
  before publication, then compares actual GPU mesh intersections, sparse
  contacts and drawn compute grass roots. Two locations, five moving swaps each;
  shared grass generation/revision/phase receipts and old state lifetime checked.
- Build first exposed GLM double-vector times integer in test code; corrected to
  a double scalar. Initial RTX/Quadro runs were stopped after the unbounded
  grass-root oracle proved too costly. The test now samples up to 64 roots per
  draw queue and seven angles per radial band (1,260 plane probes), preserving
  all bands/locations and error bounds. Per-publication progress is logged.
  Build and focused replay/new case pass on RTX (16.458 s), Quadro
  (17.024 s); new software case passes (30.311 s). Ten swaps, 1,260 probes,
  768 sampled roots: max swap 0.0312387 mm, exact GPU-plane/contact parity,
  max root errors RTX 0.145909 / Quadro 0.160110 / software 0.142474 mm.
  All 56 RTX cases pass (68.095 s); final no-op build passes. Headless
  overview inspected/decoded and byte-exact: 14,779 non-background pixels,
  bounds `(1,1)–(794,598)`, 3,471 planet pixels. Logs:
  `build-f5/resume/detail-prepared-gpu-*`. Production/activation unchanged.
  User scenario edits remain unstaged.

## Prepared surface publication probe — 2026-10-18

- Continuing terrain **p** after `47105c9`. Added a CPU surface regression that
  compares successive prepared plans under one common moving footprint, at both
  oblique and polar locations through five moves/turns/return. It probes the
  active core, blend edges, both preparation boundaries and shared coarse parent.
- Uses actual baked float meshes as independent ray-hit oracles for the shared
  sparse contact phase; compares pre/post-publication surface positions rather
  than relying only on common triangle IDs. There are 3,040 radial probes, a
  1 mm swap bound and a separate oracle-parity check. Build and focused test
  pass (22.306 s): all 10 plans change topology, maximum swap difference
  0.0379581 mm and exact sparse/baked oracle parity. All 35 core groups pass
  (52.43 s), including the 16 detail cases.
  Logs stay local in `build-f5/resume/detail-prepared-surface-*`.
- Runtime still uses `Transient`; no moving activation or parent-rebase claim.
  User scenario edits remain unstaged. Snapshot increment is committed.

## Application deformation snapshots — 2026-10-18

- Continued the terrain **p** task after `6d96ba6`. Capture emits per-body
  `terrain_deformation` only for retained sources. An original pose has a null
  footprint; an absent source has a null array entry. `SceneSource` validates
  body labels, array length, schema and physical scale before renderer creation.
  Snapshot replay requires capture mode; moving runtime activation remains gated.
- Initial capture requests carry records and restore plan/parent anchors.
  Computed snapshots install mirror-free land even with the CPU grass planner.
  Character previews bind the prospective phase. Reload restores the same
  anchors and phases in private staging before publication. `previewPhase()`
  returns mutable context-owned state for temporary mesh storage; contact
  wrappers continue to borrow const state.
- New `test_deformation_replay.cpp` covers invalid records and eight combinations:
  CPU, resident compute, compute/CPU planner and third-person, each with original
  and active phases. Fresh replay/reload PNGs and emitted records match exactly.
  Stale-topology reload throws `invalid_argument` and preserves the live epoch,
  vertex buffer and shared state. An incorrect initial no-throw test expectation
  was corrected to match the existing API.
- Full build passes. Focused tests: 59.523 s; all 35 core groups: 39.68 s; all
  55 RTX cases: 71.585 s. The focused replay tests also pass on Quadro (see local log).
  Forced OpenGL 3.3 active CPU replay PNG and record match
  exactly. Recovery, frame, reload and streaming integration all pass (264.12 s).
  All 15 historical streaming PNGs remain byte-exact. Third-person capture inspected:
  39,435 terrain pixels. Overview remains byte-exact: 14,779 non-background
  pixels, bounds `(1,1)–(794,598)`, 3,471 planet pixels. These are validation
  timings, not FPS measurements. Logs: `build-f5/resume/detail-phase-capture-*`.
- Next: prepared streaming, uninterrupted coverage and parent rebases. Keep **p**,
  default `Transient` storage and the previous native failures until those checks
  pass. User scenario edits remain unstaged.

## Worker/staging deformation replay — 2026-10-18

- Continued terrain p afterdaeaae5. Build request/result optional deformation
  record; admission validates field/version/scale, then exact rebuilt topology
  before bulk work. CPU bakes/retains original targets; compute emits metadata
  plus sparse contacts for mirror-free retained upload (also nonresident). Water
  unchanged. TerrainCpuBuild.previewPhase() creates context-only prospective
  contact phase. GPU requiredBytes validates record against sealed source and
  generation selects Retained/initialPhase; existing fences/publication apply.
- New worker case covers original/active CPU/nonresident compute/resident compute,
  CPU parity/retention, preview and stale field/scale/topology. New55th GPU case
  stages original then active record, preserves old live state until publish,
  checks shared grass/land/contact versions, actual draw receipt/contact parity,
  and rejects corrupted stage without live mutation. Build completes after test
  trace macro fix (56+14 steps),final no-op pass. Worker2.975 s,publication focus
  RTX1.640/Quadro2.051 s,35 core groups40.09 s,55 RTX cases73.383 s and frame+
  streaming119.24 s pass. Fifteen PNGs/inspected+decoded overview exact:14,779
  pixels,bbox(1,1)–(794,598),3,471 planet. Concurrent times notFPS. Recovery/reload
  passed atdaeaae5. Logs detail-replay-builds-* local; diff checks pass. API only;
  no capture/CLI/source adapters yet.
- Next connect records through capture, plan/parent anchors and reload, then
  prepared streaming/parent rebases. Keep p/Transient/native failures. User config
  remains unstaged.

## Initial GPU phase within generation fence — 2026-10-18

- Continued terrain p afterbfc01b2. TerrainCompute.generate optional initialPhase
  requires Retained/matching physical scale, validates before GPU allocation,
  applies pose after immutable target packing and before end timestamp/fence.
  Added dispatches counted; no extra GPU buffer/input storage. Shared private
  deformSource helper used by initial and live APIs. Live mirror-free/contact
  scale restrictions and GL binding/program restoration preserved.
- TerrainComputeBuffers creates retained morphState at revision0, initial pose
  makes1; Mesh.loadComputedTerrain transfers same state/targets/contacts instead
  of recreating state0. No post-ready phase allocation. New54th GPU case checks
  fence/readiness/readback, CPU parity, unchanged attrs/indices/raw target words/
  storage, state transfer, contacts, live follow-up and invalid inputs. Full72-step
  build/final no-op pass. Focus2 casesRTX3.713/Quadro3.792 s; initial pose changes
  62,572 corners with zero CPU position error, also software6.868 s.35 core groups
  40.12 s,54 RTX cases73.697 s,forcedGL3.3 CPU source/upload2.136 s and full
  recovery/frame/reload/streaming211.49 s pass. All15 PNGs and inspected/decoded
  overview exact:14,779 pixels,bbox(1,1)–(794,598),3,471 planet. Concurrent times
  validation,notFPS. Logs detail-initial-phase-* local; diff checks pass.
- No runtime caller selects Retained or initial phase yet. Next wire versioned
  records through capture/CPU/compute/reload, then prepared streaming/parent
  rebases. Keep p and old native failures. User config remains unstaged.

## Versioned deformation replay record — 2026-10-18

- Continued terrain p aftere37c40a. Pure CPU TerrainMorphReplay version1 stores
  immutable field/topology IDs as canonical decimal strings, contract versions,
  physical scale and optional footprint. Original retained pose has null phase;
  distinguish it from no retained source in the eventual per-body array. Allows
  CPU/compute override while binding field/topology/scale. No buffer payload.
- Reject missing/unsupported versions, malformed/overflowing hashes, nonnumeric/
  nonfinite values, nonunit eyes and invalid radii/transitions; writer validates
  before JSON can replace nonfinite values with null. Two tests cover original/
  moving exact bits, IDs beyond53 bits, backend-neutral match and bad inputs.
  Two-step build/final no-op, focused2 ms,35 core groups27.83 s pass. Data-only;
  renderer unchanged sincee37c40a render/replay validation. Logs detail-phase-codec-*
  ignored/local; diff checks pass. User config stays unstaged.
- Not yet connected to capture emission or CLI replay. Next add optional initial
  phase to retained compute generation within its completion fence and transfer
  shared phase state through buffers to Mesh; then wire capture/reload records.
  After replay, prepared streaming and parent rebases remain. Keep p/Transient.

## CPU grass on baked deformation — 2026-10-18

- Continued terrain p after82e1d77. CPU.prepare binds mesh-owned source through
  integer texture views, passes morphData into conservative plan and consumes
  sorted envelope bounds for vertex LOD. Source changes reset live surface;
  existing root regeneration NaNs animation time to refresh paused samples.
  Prewarm surface program for compute-capable CPU grass. Defaults unchanged;
  ordinary fallback clears source/views. Empty draw has no phase receipt.
- New real-upload rendering case: pole/oblique, shuffled map, stored keys, five
  dry/mixed/wet poses. Candidate count/cached root records stable; vertex and
  compute visible roots exact; plane error<1mm; paused phase refresh/reflection
  reuse, repeated fine roots, explicit placement disablement, fallback/disabled
  cleanup pass. First focus GL3.3 10.958 s/RTX3.961 s/Quadro3.917 s. Added scalar
  diagnostics:2,681 visible queries exact on eachGPU,5,120 cached records
  immutable,six changed views. MaxplaneRTX.230774 mm/Quadro.230762 mm/forced
  GL3.3.194173 mm. Final focused RTX.497/Quadro.389/GL3.3.571 s pass. Full20-step
  build+two-step diagnostics/final no-op pass;35 core groups42.03 s,53 RTX cases
  73.970 s,25 software wind/render/compute/upload55.464 s,frame+streaming125.52 s
  pass. All15 PNGs and overview exact:14,779 pixels,bbox(1,1)–(794,598),3,471 planet.
  Raw PNG whiteSun/11,040 bright pixels confirms preview artifact. Concurrent
  durations validation,notFPS. Recovery/reload passed at4128298 before these
  adapters. Logs detail-cpu-grass-* ignored/local; diff checks pass.
- No source copying/readback in adapter; CPU upload keeps mesh revision rather
  than fake phase. Next capture footprint replay, prepared streaming and parent
  rebases. Keep p/Transient/native failures. User config remains unstaged.

## Conservative CPU grass planning — 2026-10-18

- Continued terrain p after4128298. planGrass optional retained source uses fine
  descriptors plus conservative8-endpoint cross-product area and padded sphere,
  matching GPU envelope math. Sorted output has optional morphBounds; default
  vector stays empty and historical arithmetic/selection preserved. Coarse biome
  rejection postponed for source mode, so wet/collapsed poses retain potential
  roots. Existing hard caps/protected infeasibility unchanged. Validate shape,
  corner IDs, scale and finite endpoints.
- Added CPU plan regression:1,000 independently weighted/float-rounded poses,
  pole/oblique, collapsed/translated parents, protected/ordinary policies. Checks
  stable counts/anchors/bounds, area fits, mixed areas>2x both uniform endpoints,
  infeasibility and invalid inputs. Full82-step build/final no-op pass. FocusCPU
  6 ms,35 core groups39.86 s,53 RTX cases70.895 s,frame+streaming118.22 s pass.
  Added64 CPU/GPU triangle slot/bounds comparisons to existing envelope case;
  two-step rebuild and focusRTX71/Quadro668 ms pass, including112,192 existing
  phase checks. Slots exact; sphere components agree within1e-12. All15 PNGs
  and overview exact:14,779 pixels,bbox(1,1)–(794,598),3,471 planet. PNG decoder
  confirms white Sun/11,040 bright pixels despite tool preview. Concurrent times
  are validation, not FPS evidence. Recovery/reload/software grass passed in
  preceding4128298. Logs detail-cpu-grass-plan-* ignored/local; diff checks pass.
- Planner API only; CPU grass preparation does not yet bind source/consume bounds.
  Next wire adapter and actual root parity onGL3.3/compute, then phase replay,
  prepared streaming and parent rebases. Keep p/Transient and native failures.
  User config remains unstaged.

## CPU retained-source mesh upload — 2026-10-18

- Continued terrain p afterdf8d2c2. CPU Mesh.loadTerrain retains CPU owner plus
  integer target/map GPU buffers, using onlyGL3.3 operations. Validate identity,
  version, scale/phase, finite targets/unit radial, expanded map and baked pose
  before changing live mesh. Original positions exact; explicit phase bounded
  by4 float epsilons for stored endpoints. Capacity/allocation reject cleanly.
- Baked CPU poses keep mesh revision; no fake live morph state. Swap/release/
  ordinary upload/fallback handle source ownership together. New CPU-capacity
  and GPU-payload accessors feed vector telemetry, terrain logical storage and
  CPU staging/reload admission. Existing compute accounting unchanged.
- Added context-backed TerrainMesh regression in test_grass_render.cpp for both
  poses: raw words/map/VBO, byte totals, malformed data/pose rejection preserving
  live buffers/revision, swap, fallback, buffer deletion and last-owner lifetime.
  Full67-step build+19-step relink/final no-op pass.35 core groups40.90 s,
  53 RTX cases75.173 s,24 software wind/render/compute/upload cases140.879 s
  and recovery/frame/reload/streaming230.23 s pass. All15 PNGs and inspected
  overview exact:14,779 pixels,bbox(1,1)–(794,598),3,471 planet. Final cell-key
  count preflight guard has20-step rebuild and focused GL3.3/RTX/Quadro passes
  .997/1.106/1.108 s. Unaffected broad checks precede that opt-in guard. Concurrent
  durations are validation, not FPS evidence. Logs detail-cpu-upload-* local;
  diff checks pass.
- Still no runtime Retained selection. Next CPU fine-anchor grass, capture phase
  replay, prepared streaming and parent rebases; keep p and last native failures.
  User config remains unstaged.

## CPU-owned immutable deformation source — 2026-10-18

- Continued terrain p after72b3edd. TerrainMorphStorage now belongs to shared
  geometry. CPU evaluateTopology can opt into Retained for original or explicit
  phases; returns shared immutable TerrainMorphData alongside baked geometry.
  Integer layout matches GPU:72 bytes/sample+4/corner, radial double bits, fine
  floats, projected parent doubles, exact original emitted floats. Baked phase
  and physical scale are metadata; targets/map never depend on the active phase.
- Parents are evaluated once per sample even for originally fine samples; query
  stats count that cost. DefaultTransient has no target allocation. Unknown mode
  and non-morph retained requests reject. Decoders check bounds; data outlives
  topology/geometry. Worker ready-slot memory counts actual retained capacities.
- Added15th detail case with both unit scales, three phases, exact baked/original
  vertices, identity, raw-bit/NaN-like words, map/bytes/counts, invalid inputs and
  lifetime. Existing retained GPU case compares all raw radial words/corner IDs
  and endpoint positions with its unchanged2mm CPU/GPU tolerance. Scheduler
  memory test exercises retained and ordinary ready slots. Full117-step build,
  accounting relink, two-step oracle rebuild and final no-op pass.35 core groups
  40.66 s,53 RTX cases72.042 s, focused retained RTX2.899/Quadro16.662 s and
  scheduler3 ms pass. Float anchor oracle compares float representation exactly;
  original CPU/GPU tolerance unchanged. Frame+streaming146.34 s pass. All15 PNGs
  and overview exact:14,779 pixels,bbox(1,1)–(794,598),3,471 planet. Raw PNG also
  confirms SunRGBA254,254,254,255 and11,040 bright pixels despite tool preview.
  Independent concurrent timings are validation, not FPS evidence. Recovery and
  reload passed in preceding72b3edd. Logs detail-cpu-source-* ignored/local.
- This is CPU-owned source only. Mesh upload/CPU grass/capture adapters remain,
  then prepared streaming and parent rebases. Runtime retention stays Transient,
  keep terrain p and old native coverage failures. User config stays unstaged.

## Consumer phase receipts — 2026-10-18

- Continued terrain p after f6f7455. SceneTerrainConsumers tracks independent
  land/contact/grass availability and main/reflection/shadow/grass draw phases.
  Reflection grass now has its own mesh/phase receipt. Sidecar telemetry exposes
  these plus bound character contact and observed planted-pose phases.
- Grass exposes available versus last-drawn phase. New commits start undrawn;
  advancing shared state preserves the old draw receipt. Compute reports the
  refreshed surface cache; vertex placement reports its live source phase.
  Scene preflight rejects mismatched grass, and publication profiling requires
  phase agreement plus a refreshed character pose. Scalar-only; no GPU storage,
  readback or waits added. Defaults still Transient.
- Moving GPU test checks draw/preparation separation, both paths and delta commit.
  New real-frame publication test rejects each of eight stale consumer phases,
  stale planted-pose version and synthetic unmatched mesh/grass state. Synthetic
  phase only proves rejection; actual plane parity remains in the GPU fixture.
  Frame helpers compare every emitted phase. Full66-step build/final no-op pass;
  35 core groups32.88 s,53 RTX cases88.009 s, focused RTX/Quadro/software live
  surface11.979/6.152/31.393 s, focused renderer2 cases35.943 s pass. All2,704
  root path comparisons remain exact and plane errors below.231 mm. Recovery,
  frame, reload and streaming integration pass304.13 s combined. Fifteen PNGs
  plus inspected overview exact:14,779 pixels,bbox(1,1)–(794,598),3,471 planet.
  Validation durations include independent concurrent jobs, not FPS evidence.
  Logs detail-consumer-phase-* ignored/local; diff checks pass.
- Next CPU immutable fine-anchor source/grass, capture footprint replay, prepared
  streaming and parent rebases. Keep p and the recorded native coverage failures.
  User config stays unstaged.

## Retained grass with compatibility mirrors — 2026-10-17

- Continued terrain p after10e28f3. Grass.prepare selects immutable resident
  staging for compute terrain with retained source even with a CPU compatibility
  mirror (residentTerrain=false). Complete validation/ownership stays in existing
  submit path; ordinary CPU/Transient selection unchanged.
- New53rd GPU case attaches an initially submerged mirror and verifies potential
  roots exist. Drop the mirror for production GPU deformation, reattach its old
  frozen copy, then preparation reuses without uploads and visible roots follow
  current live positions. Mirror stays unchanged. Primitive's mirror-free update
  restriction is honored; no new mirror/deformation policy introduced.
- Full20-step build, two-step fixture rebuild and final no-op pass. FocusRTX.073 s
  /Quadro.460 s; software5.201 s; fullRTX53 cases64.10 s and inspected render pass.
  Overview exact:14,779 pixels, bbox(1,1)–(794,598),3,471 planet pixels. Full default
  core/runtime/replay/fallback matrix passed at10e28f3 immediately before this
  selector-only change; no unaffected broad rerun. Logs detail-grass-mirror-*
  ignored/local; diff checks pass. User config remains unstaged.
- Next CPU fine-anchor/grass phase support, capture/consumer records, prepared
  streaming and parent rebases. Keep terrain p; retention remains Transient at
  runtime and last native coverage failure remains unresolved. Completed seven
  separate tested/documented commits since08cd634 in this continuation.

## Explicit CPU phase evaluation — 2026-10-17

- Continued terrain p after459c0ac. TerrainSurface.evaluateTopology(t, footprint)
  bakes positions without copying/recanonicalizing topology. Shared private
  evaluator preserves the old adapter. Validate footprint, morph topology and
  physical scale before field work. Explicit-weight position helper preserves
  arithmetic; query statistics count effective phase weights. Generation, indices,
  sinks, cell keys and fine normals/colors stay tied to the immutable plan.
- Added14th TerrainDetail case: two physical unit scales, three moving masks,
  independent copy/reweight/canonicalize oracle, exact positions/attributes/IDs,
  unchanged source fingerprint, diagnostic counts and invalid inputs. Retained
  GPU test now uses the new CPU phase API; established2mm tolerance unchanged.
  FocusCPU1.354 s, RTX2.982 s, Quadro2.993 s pass; retained fixture reports zero
  vertex/contact error on both GPUs, including218NaN-like radial low words.
- Full117-step build and final no-op pass. Full35 core groups44.38 s,52 RTX cases
  82.33 s, frame+matrix154.50 s, recovery+reload142.06 s and inspected render pass.
  All fifteen historical PNGs and overview exact:14,779 pixels,
  bbox(1,1)–(794,598),3,471 planet pixels. Logs detail-cpu-phase-* ignored/local;
  diff checks pass. User config remains unstaged.
- This returns baked geometry only; CPU upload advances mesh revision. No live
  GPU phase or complete CPU grass/replay pipeline is attached. Next fix retained
  grass selection with CPU compatibility mirrors, then CPU fine-anchor/grass,
  capture/consumer phase records, prepared streaming and parent rebases. Runtime
  retention remains Transient; keep p and last native coverage failures.

## View-cache terrain phase keys — 2026-10-17

- Continued terrain p after b6ea8b6. Mesh.deformationRevision exposes shared state
  revision (zero without phase). geometryRevisions carries publication/phase
  pairs, so paused frame reuse misses when only deformation changes. Shadow
  cache/maps include a separate optional phase revision alongside existing Sun,
  extent, resolution and mesh revision. revision() retains mesh-only receipt
  meaning; ScenePass supplies both keys. Same-phase views reuse the map.
- Extended actual Renderer character-phase integration with fixed camera/time
  cache reuse before phase change and invalidation afterward, without changing
  mesh revision. Added pure shadow-cache phase case. Retained GPU surface case
  renders actual depth through five phases at each pole/oblique fixture, checks
  changed depth, and confirms same-phase cache hits. Map1024 contains the sampled
  center pixel of the small triangle; no production readback added.
- Full70-step build and final no-op pass. Three focused shadow-cache cases pass;
  runtime integration1.640 s, GPU source/shadow RTX.778 s /Quadro.730 s. All ten
  phase depth updates/hits pass; root path agreement remains exact, plane error
  below.231 mm. Full35 core groups42.90 s,52 RTX cases83.62 s, frame+matrix187.12 s,
  recovery+reload139.43 s and inspected render pass. All fifteen historical PNGs
  and overview exact:14,779 pixels, bbox(1,1)–(794,598),3,471 planet pixels.
- Logs detail-phase-views-* ignored/local; diff checks pass. Next CPU explicit
  phase evaluation/capture replay and complete consumer phase records, then
  prepared streaming and parent rebases. Runtime retention still Transient;
  keep terrain p and last native coverage failures. User config remains unstaged.

## Alternate grass placement on live terrain — 2026-10-17

- Continued terrain p after 5a0b752. Vertex placement reads fine anchors and the
  current live plane/area, projects fixed up rays before height/biome/density
  checks, and uses the same canonical corner order as compute plane refresh.
  Fine integer decoder shared through morph_fine.glsl (GLSL3.30 compatible).
- Shader::source exposes existing relative include expansion for vertex loading
  and feedback probes; no second loader. Additional integer samplers always use
  units12/13 to avoid aliasing, views bound only for source patches. No extra
  GPU buffer storage, field evaluations or CPU geometry transfer.
- Actual comparison exposed missing descriptor VAOs in resident staging. Now
  create them before ready alongside compute output VAOs. Explicit configuration
  check exposed old compute-placement admission guard; retained sources now
  support compute_placement=false. Ordinary Transient restriction stays the same.
  No draw-time resource creation is added.
- Extended production TF test compares visibility/root positions for all slots
  through dry/mixed/wet phases, all36vertices/blade, plus explicit false-config
  preparation. All2,704 visible roots agree exactly on RTX, Quadro and software
  (path error0); GPU plane error <=.231 mm, software <=.205 mm, unchanged1mm
  test limit. Immutable roots, paused animation, delta commit and views still pass.
- Full81-step build, VAO19-step +test2-step +final guard19-step builds pass, final
  no-op build passes. Final RTX52 cases 65.28 s; Quadro source+envelope 0.242 s;
  software source case27.995 s. Existing23 wind/render/compute cases pass120.405 s
  before the VAO/admission-only fixes. Default35 core groups39.62 s, frame+matrix
  208.42 s, recovery+reload176.23 s and inspected render pass. Those broad default
  checks preceded the final source-only guard correction; final full RTX/focused
  Quadro/software validate that correction. All15 historical PNGs and overview
  exact:14,779 pixels, bbox(1,1)–(794,598),3,471 planet pixels.
- Logs detail-grass-vertex-* ignored/local; diff checks pass. Next complete view
  cache/capture phase handling, prepared streaming and parent-rebase continuity.
  Runtime retention still Transient; keep terrain p and last native failures.
  User config remains unstaged.

## Grass roots following live planes, 2026-10-17

- 0cf70f2 commits conservative allocation envelopes below. Continued same terrain
  p. Source-backed resident grass now selects MorphEnvelope. Cells/root generation
  use retained fine positions as canonical anchors; metadata Original still reads
  exact original positions. Source/legacy delta reuse is separated. Existing
  synthetic staging fixture now fills real fine/parent endpoints too.
- New deformation/surface.comp and cells/GrassSurface.cpp update current plane,
  area*density and padded bounds once per shared phase. Plane uses spare cell
  geometry.w/metric.yzw, preserving immutable fingerprint xyz, key and spans.
  View compaction projects immutable cached up rays onto that live plane; current
  area drives density. Root records are never mutated by live updates.
- Source generation filters fixed tint/slope but retains wet/snow potential roots;
  compaction checks actual projected height/landscape biome. Patch attaches Mesh
  phase during no-throw commit; animation cache includes surface revision even
  at paused wind time. Surface kernel prewarmed before ready; swap/destruct and
  fallback transfer/reset state. Runtime terrain still Transient (inactive).
- Added test_grass_surface.cpp in existing GPU executable: synthetic retained
  triangle, shuffled map, initially submerged parent / dry fine endpoints,
  production TerrainCompute.deform, pole + oblique direction, plane oracle,
  fixed up/variation/immutable roots, paused phase refresh and repeat view reuse,
  wet/dry filtering and delta preparation across phase advancement. Not run yet.
  Full 66-step build, two-step fixture rebuild and final no-op build pass.
  Both GPUs pass all seven focused cases: RTX 3.964 s, Quadro 4.001 s. New test
  checks 2,704 visible roots / 1,104 changed common positions; maximum plane error
  0.000230846 m RTX / 0.000230894 m Quadro (<1 mm). Staging now reserves 491,520
  slots in 64 cells under conservative metadata (122/119 sliced polls). Initial
  new-case allocation error was fixture cleanup: a selected deleted program was
  restored in the second orientation. Scoped program unbinding fixes the fixture;
  no production workaround. Full 35 core groups pass (41.77 s), RTX 52 cases
  pass (82.50 s), inspected render passes and exact overview hash/diagnostics.
  Frame+streaming (154.64 s), recovery+reload (143.20 s) pass too. All 15 historical
  PNGs and overview exact: 14,779 pixels, bbox (1,1)–(794,598), 3,471 planet pixels.
  Logs detail-grass-surface-* stay ignored/local; diff checks pass.
- Next alternate grass placement and complete view/capture coherence, prepared
  streaming and parent-rebase continuity. Runtime remains Transient; compute
  source views are implemented but this is not uninterrupted moving coverage.
  Keep terrain p and last native gate failures. User config remains unstaged.

## Conservative grass motion envelopes, 2026-10-17

- 25ddc41 commits immutable cell/root staging below. Continuing same terrain p:
  GrassMetadataMode::MorphEnvelope is a separate opt-in third kernel/cache mode;
  requires complete retained source, never aliases original geometry caches.
  Defaults and runtime preparation remain Original/Transient.
- Shared decoder exposes fine float and parent double endpoints. Envelope kernel
  uses maximum cross magnitude over all eight independent corner combinations,
  plus final float-rounding perturbation bounds. Endpoint sphere similarly pads
  all six endpoints. Conservative eligibility defers biome filtering to placement
  so a wet/collapsed initial triangle cannot discard later possible grass.
  No new GPU buffer storage or uniform bytes; existing allocator/infeasibility
  diagnostics unchanged. Not yet connected to moving root placement/cell bounds.
- Added tests/foliage/planning/test_morph_envelope.cpp to existing GPU executable:
  four orientations, 16 synthetic triangles each, reversed sample mapping,
  independent endpoint grid/random blends with float-rounded positions, bounds,
  area and allocated-slot checks, mode-cache isolation, invalid modes/sources and
  explicit per-triangle capacity failure. Configure and full 26-step build passed, plus final two-step
  real-terrain test rebuild. Both GPUs pass 112,192 synthetic phases (16 mixed
  maxima exceed both uniform endpoints) and 156,288 real retained triangle
  queries through four footprints. RTX full 51 cases passes (74.79 s), Quadro
  focused two cases 1.597 s; core 35 groups 37.75 s; frame 82.72 s. Streaming
  matrix passes too (49.17 s; combined frame+matrix 131.90 s). All fifteen PNGs
  and overview match historical baseline exactly: 14,779 non-background pixels,
  bbox (1,1)–(794,598), 3,471 planet pixels. Logs detail-grass-envelope-* stay
  local. Diff checks pass; no moving coverage/FPS claim.
- Need dynamic root projection/bounds and phase animation invalidation, then
  view/capture coherence, prepared streaming and parent rebases. Keep terrain p;
  native coverage last collected still fails. User config remains unstaged.

## Immutable cell/root staging — 2026-10-17

- Continued terrain p after 08cd634. Optional TerrainMorphSource flows through
  submitResident/publication/resident prepare, metadata reuse and commit checks.
  Mesh and preparation buffers expose borrowed source getters. Runtime generation
  remains Transient, so defaults stay off. CPU fallback clears source/views.
- Patch owns two integer texture views (units 12/13) into existing retained
  targets/corner map, with capacity checks and preparation-state restoration.
  Separate cached cell/placement variants share the exact original-position
  decoder; shader includes live under shaders/foliage/deformation. Swap/destruct
  transfer all programs. No extra GPU buffer storage or CPU field evaluation.
- Regression uses a shuffled corner map and fresh seeded preparation while live
  VBO heights change every poll. All 303,104 roots and 74 cells stay exact;
  valid roots also lie in their original triangles. Four 65,536-slot dispatches
  per poll remain the bound. RTX needs 18 polls/updates, Quadro 97; these are test
  diagnostics, not production timings. Checks source getter ownership, units
  12/13 + active unit restoration, delta prefix reuse, clear/cancel and fallback.
- Full 74-step build and final 3-step test rebuild pass. Focused RTX six cases
  pass (3.554 s); Quadro source+retained cases pass (6.161 s). Full 35 core groups
  (42.12 s), 50 RTX cases (81.50 s), frame+streaming (154.84 s) and recovery+reload
  (143.57 s) pass. All fifteen historical PNGs and inspected overview exact:
  14,779 non-background pixels, bbox (1,1)–(794,598), 3,471 planet pixels.
  Logs detail-immutable-roots-* stay ignored/local; diff checks pass.
- Next: conservative area/slot envelopes, current root projection/bounds and
  animation phase invalidation. Then views/capture, prepared streaming and parent
  rebases. Keep terrain p and the last failing native coverage results; this
  increment proves stable staging input, not moving coverage or an FPS gain.
  User config remains unstaged.

## Immutable grass metadata from retained targets — 2026-10-17

- Continued terrain p after 69750f7. Retained packing stores exact original
  emitted float3 bits in three formerly reserved words/sample. Read original
  words before overwriting the head; copy fine words out before overwriting tail.
  Layout stays 72 bytes/sample: head radial double3 + fine float3; tail projected
  parent double3 + original float3. Same storage, input and dispatch count.
- TerrainMorphSource borrows targets/corner map/sample count/generation. Shared
  GLSL decoder fetches original bits directly, avoiding repeated interpolation.
  Grass metadata accepts it through a separate shader variant. Default shader
  code stays the same through an include; source identity joins geometry reuse.
  Validate generation/count/buffer IDs and sizes/seven bindings; restore ranges
  5/6 too. Four extra uniform bytes per submission; no extra GPU buffer storage.
- New metadata case compares every fresh/cached record with original geometry
  through four moving footprints; live VBO metadata changes in 78,766 summed
  triangle queries while immutable records stay exact. Checks wrong/truncated
  sources, cache isolation and GL ranges. Retained case checks every original
  position word plus exact radial bits (218 NaN-like low words), contact/vertex
  parity, ownership and default output. Both cases pass on RTX 3070 Ti (11.90 s,
  metadata 1.22 s) and Quadro M1000M (14.61 s, metadata 1.65 s); zero vertex/contact
  error in this fixture. These include cold compilation and validation reads,
  not production frame timings. Initial interpolation prototype was simplified
  to raw positions before this final validation.
- Final full 60-step build passes. 35 core groups (37.55 s), 49 RTX cases (73.40 s),
  frame (83.00 s), streaming/replay/fallback matrix (48.58 s), inspected render
  pass; all fifteen historical PNGs and overview exact: 14,779 non-background
  pixels, bbox (1,1)–(794,598), 3,471 planet pixels. Logs detail-immutable-bits-*;
  earlier prototype evidence detail-immutable-source-* stays ignored/local.
- Runtime source/retention still disabled. Next propagate immutable decoding to
  cells/root generation, reserve conservative area/slot envelopes, project fixed
  root rays onto current planes/update bounds and invalidate animation on phase.
  Then all views/capture, prepared streaming and parent-rebase continuity. Keep
  the same p row; old native gate failure remains as last measured. No fresh
  moving coverage/FPS claim. User config untouched and must stay unstaged.

## Runtime character deformation versions — 2026-10-17

- Continued the same terrain p after 84cbf7c. SurfaceContact::Version exposes
  {mesh, deformation} and reads live shared state; revision() keeps its mesh-only
  publication identity. Copying/rebinding for a preview cannot hide a changed
  phase from the actual astronaut's last observed version.
- Runtime mesh-backed character bindings now carry mesh.morphState. Planted-foot
  refresh compares both versions. Character/reload previews save/restore the
  complete version; reload reset and planet switch update it consistently.
  Prospective worker contacts stay static until a matching GPU phase exists.
  Runtime preparation still uses Transient; no moving deformation enabled yet.
- Extended the pure CPU case with planted feet, live versions/copies and final
  ownership release. Added a real prepareAstronaut / prospective preview/restore
  integration case using a synthetic CPU phase; it checks changed foot heights
  with unchanged surface directions. It does not draw the synthetic phase;
  actual GPU-plane parity remains covered by the retained-target case.
- Full 66-step build + final no-op build pass. Nine focused contacts (6.79 s) and
  focused runtime case (1.62 s) pass. Full 35 core groups (46.58 s), 48 RTX cases
  (86.11 s), frame (69.71 s isolated), recovery+reload (147.12 s combined), and
  15-render replay/fallback matrix (83.69 s) pass. Initial parallel frame CTest
  passed (112.51 s) but display wrapper returned 143 after completion; isolated
  rerun exits 0 without source changes. All fifteen historical PNGs and inspected
  overview exact: 14,779 pixels, bbox (1,1)–(794,598), 3,471 planet pixels.
- Logs detail-character-phase-*; report sinking.md; diff checks pass. Next grass
  consumer notes below, then all views/capture, prepared streaming and parent
  rebases. Same native failure remains open as last collected; no fresh coverage
  or FPS claim. Terrain remains p; later todos untouched. Dirty user config
  remains untouched and must never be staged with these changes.

Continuation notes for the remaining grass consumer (proposal, not implemented):
- Keep each root's fixed radial (`upRank.xyz`) and variation; project that ray
  onto its current triangle plane, rather than rerunning randomized placement.
  Update triangle planes, actual area/density metric and bounds once per phase;
  then compact all views from the same data. Invalidate animation on phase even
  when time/history are unchanged. Default paths must remain byte exact.
- Candidate slots need a conservative area envelope: the norm of the triangle
  cross product is convex in each independent vertex blend, so its maximum is
  among the eight fine/parent endpoint combinations (also cover float rounding).
  Reserve slots from that bound, evaluate visible density from current area;
  preserve explicit infeasible-budget diagnostics. Do not silently undersample.
- Immutable original positions and metadata are now implemented above. Extend
  the shared uint decoder to cell/root generation too, keeping G4 source leases
  and fingerprints valid while live positions move. Normals/colors remain in
  the unchanged VBO under the current position-only contract. No extra VBO copy
  is needed. Metadata stability alone does not complete root/slot/view behavior.
- Whole-generation grass commit can attach Mesh's shared phase in the existing
  no-throw commitPrepared transfer (pass live Mesh there). Keep mesh generation
  and per-frame phase distinct. Admission/capture/views/prepared streaming/rebase
  continuity still need actual implementation and native validation.

## Shared contact deformation phase — 2026-10-17

- Continued the same terrain p after a48cc4d. Retained Mesh now owns pure-CPU
  TerrainMorphState separately from immutable topology/generation. Validated
  footprint changes advance a monotonic revision. GPU deformation publishes the
  next phase after successful submission; bad input/backend leaves it unchanged.
- SurfaceContact optionally shares the phase and clears its triangle-ID cache on
  changes, including same-source rebinding. Explicit phase-aware sparse triangles
  cache fine/parent endpoints lazily; total static positions + dynamic target
  records remains ≤1024. Dynamic records are larger; no memory-saving claim.
  Warm endpoints need no field evaluations for phase-only queries. Static APIs
  and aliased snapshots remain unchanged and validated after deformation queries.
- Phase ownership follows Mesh swap/fallback/destruction; a contact wrapper can
  retain CPU state/source after GPU buffers retire. Wrong source generation or
  physical scale is rejected by the deformer. No live runtime retention enabled.
- Full 102-step rebuild + final rebind fix rebuild pass. Nine focused contact
  cases pass (6.70 s); focused RTX retained-target case (3.00 s) reports zero
  position/contact error. Full 35 core groups (42.41 s), 48 RTX cases (77.15 s),
  frame integration (90.31 s), 15-render replay/fallback matrix (67.68 s) pass.
  All fifteen historical PNGs and inspected overview match exactly: 14,779 pixels,
  bbox (1,1)–(794,598), 3,471 planet pixels. Evidence detail-contact-phase-*.
- Report sinking.md; todo remains p. Next connect runtime contact bindings and
  planted-foot refresh to phase changes; then grass roots/animation, all views/
  capture, prepared streaming and parent-rebase continuity. Old native gate still
  fails as last collected; no fresh moving-coverage or FPS claim. User production
  config untouched and must stay unstaged. Later todo rows still queued.

## Retained GPU morph targets — 2026-10-17

- 65a4187 commits prepared-overlap proof below. Added standalone optional retained
  GPU targets; default Transient and every runtime caller remain unchanged.
  Requires v4; Mesh with bound contacts rejects deformation until shared phase.
- detail_targets.comp stores fine float3 / parent double3; phase 3 packs exact
  radial double3 after expansion in existing 72-byte/sample unique scratch.
  uint words preserve double bits (218 NaN-like low words checked exactly).
  morph.comp has no field/noise inputs, writes only positions and restores GL
  program/SSBO ranges. Warmed in retained generation; packing counted in timing.
- Mesh transfers unique scratch and corner map, releasing them on destruction,
  upload/fallback or swap. Working/admitted bytes match transient generation but
  live retention increases by 72*N+4*C: 1,877,712 bytes in the 39,116-face fixture;
  closed 100k-face sphere adds 4,800,144 live bytes. No extra GPU buffer storage;
  optional shader programs cached. No memory reduction or FPS claim.
- Focused RTX case checks six moving/return centers against CPU reweighted topo,
  shared-corner bits, unchanged attrs/indices, buffer lifetime after prep release,
  GL state, invalid/contact guard and swap/destroy/CPU fallback. Reports zero
  CPU/GPU position difference, preserving existing 2 mm tolerance; final exact-bit
  check passes (1.94 s). First compile caught reserved GLSL sample; corrected.
- Full 62-step rebuild; 35 core groups (41.67 s), 48 RTX cases (79.08 s), frame
  (93.05 s), 15-render replay/fallback matrix (105.65 s) pass. Final test-only bit
  assertions rechecked focused; no production changes afterward. All fifteen old
  PNGs and inspected overview exact: 14,779 pixels, bbox (1,1)–(794,598), 3,471 planet.
- Evidence: build-f5/resume/detail-targets-*; MORPH_TARGETS line in -bits.log.
  Report sinking.md; final diff/whitespace checks pass. Runtime temporal behavior is
  NOT enabled. Moving gate still fails as last measured in 9e2fbb4 snapshot pair.
- Next: shared deformation state through the existing SurfaceContact wrapper,
  preserving the contact oracle's immutable topology/static generation contract.
  Do not mutate aliased topology or silently change static oracle geometry.
  Then grass/root animation invalidation, all views/capture, prepared-patch
  streaming and parent-rebase continuity. Keep the same terrain p row and all
  historical v4/default paths. User config remains untouched and unstaged.

## Prepared fine-overlap prerequisite — 2026-10-17

- 9e2fbb4 commits immutable staging below. Continued same terrain p; later TODOs
  untouched. CPU probe compares exact radial triangle sets within a 1 m active
  region across .15/.30 m moves, turns and return, with a fixed parent/field.
- Existing .5 m core changes 1,580–3,532 triangle references there. At 100k cap,
  1.5 m prepared core preserves every checked interior triangle at oblique and
  pole poses, all fully fine/unsunk. About 39.1k/29.7k triangles. 2 m core also
  passes (58.9k/42.4k). No timing gain or universal-budget claim.
- Added TerrainDetail.PreparedFineOverlapKeepsInteriorTrianglesStableAcrossTurnsAndPoles.
  Focused case passes (4.05 s); full 13-detail suite passes (8.33 s), log
  build-f5/resume/detail-overlap-detail-suite.log. No production source changed.
  Prototype source/CSVs in ignored build-f5/resume/detail-overlap*.
- Documented in sinking.md; suite/diff checks passed and committed as 65a4187.
  Runtime still uses the configured radius for BOTH tessellation and morphing.
  This is only a prerequisite regression, not an implemented runtime halo.
- Next design: retain fine/parent targets and exact radial coordinates on GPU
  rather than rerunning noise per frame. Existing unique scratch is 72 bytes per
  vertex; after corner expansion it can hold radial double3 + fine float3 in
  its first 36-byte region, parent double3 in the second 36-byte region. Use a
  uint SSBO view for exact double bit packing, avoiding float NaN payload issues.
  Transfer unique scratch and corner-map buffers with Mesh under existing ledger;
  add optional compute variant/packing phase, then a cheap position-only dispatch.
  Not implemented. First validate standalone GPU/CPU parity and lifecycle; runtime
  enablement MUST wait for shared contacts/grass/view/capture phase and prepared
  overlap, including parent rebases. Preserve all old v4 capture/default paths.
  Terrain remains p. User config edit remains unstaged.

## Immutable terrain staging snapshots — 2026-10-17

- dc2d24a records the rejected cache below. TerrainTopologySnapshot now shares
  the contact oracle's validated private descriptors with GPU staging. Worker
  moves rather than copies topology into contacts; alias ownership retains the
  oracle until the last consumer releases it. Contacts cannot move/copy/assign.
  Public raw DTO snapshot/compute entry points still validate fully. Water owns
  a separately validated snapshot; admission keeps identity/consumer/byte checks.
  No shader, format, GPU allocation, quality or replay changes.
- Full 121-step build; 35 core groups (30.18 s), 47 RTX cases (77.75 s), frame
  (147.46 s), 15-render matrix (107.86 s), recovery/reload (162.87 s) pass. New
  ownership/corruption/lifetime and exact GPU buffer tests pass. All fifteen old
  images and inspected overview exact: 14,779 pixels, bbox (1,1)–(794,598),
  3,471 planet pixels. CPU compatibility evaluation keeps existing validation.
- Fresh isolated native pair, identical input JSON: render-thread full validation
  AND fingerprint scans 192/210/45→0/0/0. Warm publication medians, initial excluded,
  141.80→98.09 / 112.85→90.44 / 390.56→354.19 ms (~31/20/9% in one traced pair).
  RSS mixed: 205.4→206.0 / 143.0→137.6 / 157.5→152.4 MiB; no all-memory/FPS claim.
- Gate remains 2: after max offsets 1.121/2.282/8.085 m; outside 496/534,
  672/690, 792/829 frames; wide geometry misses 579. At most one queued/running/
  ready job, no bulk reads/blocking polls/finishes/server waits; logical overlap
  <75 MB. Moving/temporal acceptance still incomplete; terrain stays p.
- Logs/report: build-f5/resume/detail-snapshot-*, native-summary.json; before
  helper/probe -before-snapshot are 892824f baselines. See terrain-gpu/sinking.md.
  User config edits untouched and remain unstaged. Final diff review complete;
  this increment is tested/documented. Moving/temporal work remains next.

## Retained refinement-value pilot rejected — 2026-10-16

- 892824f is the latest implementation commit. A bounded derived-cache pilot
  retained fine samples/intrinsic triangle metrics under the parent mutex, with
  2*budget+2 sample / 2*budget metric caps and saturation/reset fallback. It kept
  geometry history-free and all 12 production/six wide quality fingerprints exact.
- Thirteen detail cases (including new saturation/fresh-parent comparisons), 35
  core groups (59.00 s), 46 RTX cases (77.54 s), frame integration (93.88 s),
  15-render matrix (73.85 s), and visually inspected exact overview all passed.
  All 15 old capture images exact. Pilot source/tests are now reverted, not shipped.
- Three alternating pairs: warm production −14.7/−14.3/+0.2/−11.0%; wide warm
  −2.0 to −16.3%, but cold wide +17.0%. Wide RSS 34,716–34,772→46,128–46,524 KiB.
  Repeat requests fall 75,353→0; moved requests 75,359→4,551. Height evaluations
  unchanged. Field-request/hit columns retained in optional production benchmark.
- Native gate still exits 2 and coverage worsens: offsets 1.668/3.339/11.050 m;
  outside 378/402, 458/479, 716/750 frames; 530 wide frames miss geometry targets.
  Rejected: mixed warm benefit does not justify cold/memory/end-to-end cost.
- Raw pilot source patch, CSVs, logs and native receipts: ignored
  build-f5/resume/detail-refinement-values-*. Frozen before helpers remain valid
  892824f baselines. No new task row; terrain remains p, user config untouched.
- Next meaningful bottleneck found: one topology is validated/hashed separately
  by contact construction, admission, GPU-stage construction and compute submission.
  Plan an immutable owned snapshot shared from validated contact topology; retain
  full validation for mutable DTO entry points. It can remove main-thread scans
  and a staging copy without the rejected cache. No snapshot code implemented yet.

## Refinement allocation/footprint reuse — 2026-10-16

- 88ff0e9 commits vertical refresh below. Continue terrain p; no later TODO started.
  Standard per-plan PMR pool recycles small edge-map nodes (1,024 blocks/chunk,
  ≤128-byte blocks; bucket arrays upstream). Destruction releases all chunks;
  no allocator shared across workers or published plans. Local footprint distance
  reused after the world-distance guard, preserving high-altitude diagnostics.
- Three alternating CPU pairs preserve every non-timing production/wide column.
  Warm production medians improve 6.8–24.2%; wide warm medians 1.2–22.1%.
  Cold/legacy results mixed (up to 7.2% slower). Wide six-plan peak RSS rises
  30,716→34,540 KiB. Report describes the memory/cold tradeoff; no general FPS claim.
- Full build; 35 core groups (39.04 s), 46 RTX cases (78.78 s), frame integration
  (96.05 s), 15-render replay matrix (70.97 s) pass. Inspected overview exact:
  14,779 pixels, bounds (1,1)–(794,598), 3,471 planet pixels. Existing independent
  oracles used; no implementation-mirroring allocator test added.
- Native warm main-body publication medians (initial excluded) 149.99→119.05,
  121.01→117.87, 467.52→417.29 ms. Coverage gate still exits 2: maximum offsets
  1.183/2.745/9.079 m; outside 352/375, 389/412, 686/721 frames; wide geometry
  misses 483. Nonblocking/ownership checks pass; logical overlap <75 MB.
- Evidence: build-f5/resume/detail-refinement-overhead-*; frozen helper/native
  probe use -before-refinement-overhead. Terrain remains p. Retained refinement
  and temporal publication still required; user config edits untouched.

## Vertical terrain refresh correction — 2026-10-16

- 09e308d commits sink reuse below. Corrected two height-sensitive terrain bugs:
  streaming parent reuse ignored radial movement, and both runtime schedulers
  skipped purely vertical changes. A 498 m drop previously submitted no new job
  (2→2); the stale high shell has 1,992 faces versus a fresh low shell's 3,048.
- Parent reuse now bounds absolute radial movement as well as horizontal travel.
  Runtime combines its existing angular metric with radial movement for
  geometric-error-aware terrain; other refresh behavior is unchanged. Recorded
  parents retain their one-shot replay override. No new GPU buffers or formats.
- CPU tests cover descent/ascent, fresh-plan equality, both world-unit conventions
  and ordinary planning after recorded-parent replay. GPU tests compare complete
  geometry/contacts; worst new position difference is 0.0596 mm. Runtime tests
  verify complete published generations after descent/ascent on resident GPU,
  async CPU and synchronous capture CPU, with no bulk reads or blocking waits.
  The high-altitude error-domain guard is also preserved.
- Full build, 35 core groups (36.11 s), all 46 RTX compute cases (72.69 s), full
  frame integration (134.13 s) and expanded 15-render capture matrix (65.25 s)
  pass. Historical high-parent replay and CPU/GL 3.3 images are exact; GPU/CPU
  image mean difference is 0.00003665, below the unchanged 0.1 limit. All eleven
  earlier captures and twelve production topology/work/index fingerprints match.
- Overview visually inspected and byte-identical: 14,779 non-background pixels,
  bounds (1,1)–(794,598), 3,471 planet pixels. Historical high-parent image is
  compatibility evidence, not a live-detail acceptance image. No FPS claim.
  Logs: build-f5/resume/detail-vertical-*, detail-descent-before.log. Before-fix
  CPU/ascent/runtime witnesses fail as expected; the separate linker-race launch
  is not regression evidence. Report: docs/journal/architecture/terrain-gpu/sinking.md.
  Moving/temporal acceptance remains open; terrain stays p. User config untouched.

## Reuse evaluated sink descriptors — 2026-10-16

- 3c1b8be commits the complete-plan cache bound below. Continue the same terrain
  p row: local-detail export recalculates profile/omitted-relief/curvature sinks
  for vertices/probes whose sampleAt already computed those exact fixed-policy
  values. Reuse their stored sinks; continue converting untouched coarse parent
  samples. Reuse the existing normalized eye for footprint/transition distance.
- First prototype retains all twelve production and six wide fingerprints/work
  fields, saving only a few ms. Also memoize the original parent samples' fine
  sinks: field/policy checked, initialized under existing streamDetail mutex,
  reset with the parent, no borrowed field references. Eight bytes per original
  vertex (~200 KB for production's 50k-face shell). No descriptor/quality/GPU
  memory change; extra CPU backing is bounded by the existing parent cap.
- Both builds pass. Payload is 200,016 bytes for the production 50k-face shell
  (upper bound 400,016 bytes at 100k, plus container/policy overhead). No headers
  or GPU/replay contract changes. All twelve production fingerprint/work/index
  fields remain exact. Warm CPU planning saves 7–11 ms in one isolated cohort:
  startup 101.60/100.63→90.78/93.29 ms, mountain 127.14/120.43→116.00/109.45.
  Cold roughly unchanged. Wide short-move timings are mixed despite exact fields.
- Eleven detail/seven contact cases, all 35 core groups (34.29 s), all 45 RTX
  compute/consumer cases (85.44 s), moving/rebased/recorded-parent replay + CPU/
  GL3.3 matrix (64.95 s) pass. Seven earlier images exact; overview inspected,
  byte-identical: 14,779 pixels, bounds (1,1)–(794,598), 3,471 planet pixels.
- Native export medians improve 2.90→1.68 / 2.95→1.24 / 8.41→4.77 ms, but
  total publication worsens 114.98→149.99 / 118.76→121.01 / 415.90→467.52 ms
  in this traced cohort. Gate still exits 2: max offsets 2.105/2.605/13.441 m;
  outside 298/314, 384/403, 592/626; wide geometry misses in 427 frames.
  Nonblocking/ownership checks pass; GPU overlap <75 MB. No native FPS claim.
- Callgrind instruction profile finishes six exact working-set requests, 1.426B
  instruction refs (fingerprinting 11.8% self plus substantial reassessment/
  allocation). Instrumented timings are not native performance measurements.
  Existing valgrind install used; no environment dependency changes. Evidence:
  build-f5/resume/detail-sink-*, detail-refinement-callgrind*; frozen before helper/
  native probe use -before-sink-export. Terrain remains p; keep later TODOs queued
  and preserve the user's production config edits.

## Streaming probe working set — 2026-10-16

- 181c5dc commits the parent-contact-index reuse below. Native wide-core plans
  still repeatedly evaluate ~75k field heights while the shared height cache is
  capped at 60k entries (twice the 30k triangle cap). Added a regression with the
  native fixture's 300/1000 m near/mid bands and 1.5 m core. Before fix, a
  stationary repeat evaluates 75,353 heights with zero hits; 0.15 m movement
  evaluates 75,168 with only 191 hits. Geometry remains deterministic.
- Increase only the streaming cache capacity to 4*triangle_budget+2. Refinement
  creates <=2*budget+2 vertex/edge probes and <=2*budget centroid probes; the old
  capacity cannot retain even one wide plan. The new bound is 120,002 at 30k,
  400,002 at the maximum 100k cap, with lazy allocation and existing bounded
  batch eviction. Parent replacement still drops the cache/owned field together.
  No new GPU memory, quality target, field/shader contract or default setting.
- Full 32-step build passes. Regression now repeats with 0 evaluations; 0.15 m
  movement evaluates 4,551 with 70,808 hits. Fresh replay generations match.
  All eleven detail cases, 35 core groups (32.67 s), 45 RTX compute cases
  (65.04 s), replay/CPU/GL3.3 matrix (48.77 s) pass. Seven earlier images exact;
  overview inspected and byte-identical (14,779 pixels, bounds (1,1)–(794,598),
  3,471 planet pixels). Twelve short production fingerprints/work/index fields
  remain exact; those 0.5 m plans are parity controls, not cache-stress timings.
- Add terrain_detail_benchmark --working-set (optional trace path) for six cold,
  repeat/move/return anchors. Three alternating pairs against the frozen 181c5dc
  planner object improve warm planning 10–18% (236–263→203–216 ms); all geometry/
  limit/request fields and fingerprints match. Native helper's existing 300/1000
  m bands are explicit now; its three generated input documents are identical.
- Native traced route pair remains mixed: publication walking 118.01→114.98 ms,
  sprint 113.15→118.76 ms, wide sprint 387.49→415.90 ms. Gate exits 2: max
  offsets 1.219/2.593/9.142 m; outside 385/407, 391/411, 734/771; wide-core
  geometry misses in 515 frames. CPU RSS peak samples 150.1–162.4 MB vs prior
  147.9–157.3 MB (partial periodic observations, not hard bounds). GPU overlap
  <75 MB; ownership checks pass with zero blocking polls, bulk reads or finishes.
- Evidence: build-f5/resume/detail-probe-capacity-* and detail-probe-working-*.
  Frozen before helper/probe use -before-probe-capacity; the matched working-set
  baseline links the current helper object with TerrainStreamingBeforeProbeCapacity.o
  from 181c5dc, same compiler flags. Report includes the native non-improvement.
  Terrain remains p; later TODOs queued and user's production config preserved.

## Parent contact-index reuse — 2026-10-16

- 8f809fa commits the tested triangle-metadata optimization below. Continued the
  same terrain p row with an immutable coarse contact index plus a local tree
  containing only descendants of split parent triangles. Original-face IDs are
  exported during refinement; untouched parent faces map to current face IDs.
  Queries sort current IDs to retain rendered-plane tie rules. Shared tree
  traversal is const and charges visits to each querying generation's counters.
- Acceleration is private CPU metadata, excluded from field/topology hashes and
  GPU descriptors. Canonicalization clears it; contacts also require its sealed
  topology hash and matching parent field/policy. Parent and local trees own their
  inputs across worker snapshots, parent rebases and scene destruction.
- Added a regression comparing accelerated contacts to the independently rebuilt
  full index across movements/rebase, exact vertices/poles and local rings; checks
  invalidation, sorted/unique candidates and surviving an old source's lifetime.
  Also cover a uniform 11,520-face base consuming the whole budget: the empty
  local tree retains exact whole-planet contacts. Full 121-step ABI rebuild and
  final helper build pass. Frozen 8f809fa helper/probe:
  terrain_detail_benchmark-before-parent-index / terrain_native_probe-before-parent-index.
- Isolated RTX parent shader pilot (no tracked shader edits) shares repeated
  parent positions in phase 2. Three warm pairs remain image-exact but show no
  useful gain: micro fixture median 1.1295→1.1336 ms; production startup
  5.1466→5.1057 ms (~0.8%). Cold warmups excluded. Keep current shaders. Raw
  detail-parent-rtx-{pilot,production} directories remain ignored/local.
- All 35 core groups, seven contact cases (final AstronautMotionTests 4.48 s),
  all 45 RTX compute/consumer cases (62.3 s), and moving/rebased/recorded-parent
  replay + CPU/GL3.3 captures (79.9 s) pass. Seven earlier images remain exact;
  overview inspected and byte-identical: 14,779 non-background pixels,
  bounds (1,1)–(794,598), 3,471 planet pixels.
- Two production before/after cohorts retain all 12 geometry fingerprints and
  work fields. Warm contact prep 80–93→44–48 ms; combined plan + contact cost
  startup 187–194→142–147 ms, mountain 179–186→165–171 ms. Cold mountain cost
  rises 1399–1413→1440–1474 ms and pure warm mountain planning rises ~19 ms;
  report retains both limitations. Shared parent backing is 2,648,704 bytes plus
  a 200k-byte map per plan; only 4,447–7,173 faces enter local trees.
- Native walking medians: contact index 19.82→10.84 ms, full CPU build
  63.05→54.17 ms, publication 138.39→118.01 ms. Gate still exits 2:
  max offsets 1.210/2.389/8.665 m; outside 423/446, 408/429, 725/765;
  1.5 m fixture also misses geometry in 554 frames. Zero blocking polls,
  bulk reads or finishes; logical overlap <75 MB. No coverage/FPS claim.
- Logs/CSVs/traces: build-f5/resume/detail-contact-parent-*; before/after CSVs
  have a second -2 cohort, native trace in detail-contact-parent-rtx-trace.
  Terrain remains p, later TODOs queued, user's production edits preserved.

## Contact-index preparation optimization — 2026-10-16

- Continued the sole terrain p row after e75c6ae. NVIDIA is available again;
  the RTX 3070 Ti traced native receipt still fails coverage (baseline max
  offsets 1.553/3.164/9.451 m). CPU contact-index construction takes a median
  28.40 ms of the first walking route's 70.40 ms full CPU build.
- Cache exact per-triangle radial bounds/centroid sums during BVH construction.
  Temporary 72 bytes/triangle, released before publication; same padding, axes,
  leaf size, ID ties and candidate order, unchanged retained index/GPU memory.
  No header/ABI, field contract, shader or generation-hash changes.
- Extend terrain_detail_benchmark with independent contact wall/thread CPU time,
  retained index size and ordered candidate-list fingerprints. Twelve before/
  after v3/v4 fingerprints, geometry/work fields, index sizes and candidate
  fingerprints are exact. V4 production contact prep improves 124–133→80–89 ms
  (30–39%) in one isolated cohort. Native walking medians: contact index
  28.40→19.82 ms, CPU build 70.40→63.05 ms, publication 146.45→138.39 ms.
  These are diagnostics, not an FPS/coverage acceptance claim.
- New contact regression compares disconnected partial meshes against exhaustive
  rendered-plane contacts at ten uneven/leaf-boundary sizes. All five contact
  cases, all 35 core groups, all 45 RTX compute/consumer cases, moving/rebased/
  recorded-parent replay and CPU/GL 3.3 captures pass. Overview inspected and
  byte-identical (14,779 pixels, bounds (1,1)–(794,598), 3,471 planet pixels).
- Native after receipt still exits 2: max offsets 1.476/2.805/9.309 m; outside
  frames 315/332, 518/539, 763/797. The 1.5 m core also misses geometry in
  537 frames at the unchanged 30k cap. Nonblocking/ownership audits pass; logical
  overlap <75 MB. Do not loosen targets or mark terrain t. Continuous moving
  detail/temporal publication remain; later TODOs stay queued in original order.
- Local logs/receipts: build-f5/resume/detail-contact-{build,core,oracles,
  rtx-compute,streaming,render}.log, detail-contact-rtx-after-trace,
  detail-rtx-return-trace, detail-contacts-{before,after}.csv. Frozen baseline
  benchmark/native binaries are in the same ignored directory. Preserve the
  user's uncommitted configs/scenarios/solar_system.json edits.

## Recorded parent replay correction — 2026-10-16

- be1078b commits the dispatch/workload verification below. Continued the same
  terrain p row: the safer near-band reuse limit can replace a recorded v4 parent
  during replay. A geometry-only fixture reproduces an unexpected 2.102 m anchor
  replacement with the old path. Its edited geometry disables grass and uses null
  quotas; retaining quotas for a different mesh produced an invalid fixture in the
  first pilot (not a renderer regression).
- Added TerrainSurface::prepareReplayParent, called only by the existing capture
  adapter with an idle planner. It rebuilds the recorded parent exactly and marks
  it for the next plan, including worker snapshots; normal planning then resumes
  the safe distance bound. Explicit repeated preparation rebuilds even nearby
  anchors. No new shader, topology format, GPU memory or live-mode override.
- Full 102-step build passed. Ten detail CPU cases pass, including one-shot
  restoration, normal rebasing afterward, exact nearby-anchor replacement and
  unsupported/invalid-input rejection. The first expanded capture matrix passes
  (64.3 s), honoring the old parent and replaying it exactly.
- Final expanded matrix passes (41.4 s), including recorded-parent CPU/GL 3.3
  routes: both backend image means are 0.0, saved replay is exact, and all seven
  earlier images remain byte-identical. Historical-anchor capture inspected.
  All 35 core groups pass (29.55 s); all 45 software compute/consumer cases pass
  (77.6 s). Overview is byte-identical to the inspected prior image: 14,779
  non-background pixels, bounds (1,1)–(794,598), 3,471 planet pixels.
- Logs: build-f5/resume/detail-recorded-parent-{core.log,streaming-final.log,
  cpu.log,compute-all.log,render.log,build.log,before-capture.log}; captures in
  build-f5/terrain-streaming-captures. Preserve the user's production config.
- Terrain remains p. Moving coverage and temporal publication remain incomplete;
  NVIDIA NVML is unavailable. Later TODO rows stay queued in their original order.

## Parent dispatch verification — 2026-10-16

- Short near-band correction is committed as 1932457; parent adjacency work is
  6978326. Renderer production sources/shaders remain at that corrected behavior.
- Investigated redundant parent evaluation within the same p row. Work counters
  show 27.6–28.6k needed parent positions versus 81.6–85.7k unconditional calls
  in production fixtures. Counts describe requirements within each vertex, not
  GPU invocations/timing. All twelve benchmark fingerprints remain unchanged.
- A branch-only shader prototype preserved all three paired images exactly but
  showed no software timing gain: median generation 38.318→39.703 ms over three
  alternating pairs. Reverted it; no shader change is shipped. Do not infer a
  GPU/FPS gain from the 66% scalar redundancy. NVIDIA still fails NVML setup.
- Retained diagnostics and a stronger moving-parent oracle: force 64-thread
  chunks across descriptor triplets and ensure zero-weight, repeated-parent and
  real parent-plane cases are covered. Final restored shader passes all 45
  software compute/staging/cell/publication/allocation cases (77.7 s). Latest
  renderer also has passing 35 core groups, nine CPU detail cases, short-band
  rebase replay and original CPU/GL 3.3 images, as recorded below.
- Raw evidence remains ignored/local: build-f5/resume/detail-parent-work.csv,
  detail-parent-pairs/results.json, detail-parent-final-compute.log and
  detail-final-compute-all.log. Report/README updated. Preserve the user's
  uncommitted production foliage edits. Terrain stays p: continuous moving
  coverage and temporal publication remain unfinished; later TODOs stay queued.

## Short near-band core correction — 2026-10-16

- Parent adjacency optimization is committed as 6978326. Continued the same p
  terrain row with a correctness fix: the old 10 m minimum parent lifetime lets
  a 2 m near band filter/sink its moving core. New regression fails at 3 m travel
  with a core sample sunk 0.0609883703 m. Limit reuse to half the difference between near
  distance and core radius; existing atomic replacement resets shell/adjacency/probes.
- Nine CPU detail cases and all 35 core CTest groups pass (25.95 s). Eleven
  terrain compute cases pass, including the new short-band rebase; max CPU/GPU
  position difference in that case is 0.059605 mm. No tolerance changes.
- Expanded moving-capture matrix passes (34.8 s): a 1 m near band forces a
  1.802 m parent rebase, final core edge is 4.431 cm without a limit, and replay
  from the saved final parent is exact. Original four images remain byte-exact
  against 6978326. New short-near capture visually inspected; overview remains
  nonblank (14,779 pixels, bounds (1,1)–(794,598), 3,471 planet pixels).
- Evidence: build-f5/resume/detail-short-near-{before.log,core.log,compute.log,
  streaming.log,render.log,build-final.log}; CTest captures in
  build-f5/terrain-streaming-captures. NVIDIA NVML remains unavailable. Prior
  software movement receipts remain failures; terrain stays p and later TODOs
  remain queued. Next work is still continuous moving detail / temporal
  publication, with shared terrain/grass/contacts and bounded ownership.

## Terrain adjacency continuation — 2026-10-16

- Continued the priority p terrain row. Cache one immutable coarse adjacency map
  and face bounds with the parent; materialize only visited/refined mutable edges.
  Child faces retain compact parent IDs. Replacement publishes shell/adjacency
  together after successful builds. Canonicalization avoids an immediate duplicate
  checksum; public consumers retain full checksum/structure verification.
- Isolated production benchmark: warm startup 154.6–173.0→99.5–100.9 ms;
  mountain 195.6–202.2→98.1–102.7 ms. Cold planning remains 1,716 / 1,288 ms.
  All twelve before/after topology fingerprints and geometry/work counts agree.
  Mutable edges are 6.7–10.3k versus 75k cached parent edges. This is CPU planning
  evidence, not sustained FPS or moving coverage acceptance.
- Full build, all 35 core groups, eight CPU detail cases, all 44 llvmpipe compute
  cases and the moving replay/CPU/GL 3.3 capture matrix pass. Four images also match
  the prior committed final stream-reuse captures exactly. Overview inspected:
  14,779 non-background pixels, bounds (1,1)–(794,598), 3,471 planet pixels.
- Corrected the pre-existing G4 slicing test's poll counter: include the completing
  poll and require >4 actual root dispatches, retaining the <=4/poll check and
  source-lease/cancellation assertions. The original failed on synchronous Mesa
  completion; all 44 software cases pass after the correction (77.3 s).
- Native helper now checks geometry targets as well as anchor coverage, records
  actual detail/edge counts in the private probe, supports explicit --software and
  optional --trace, and clears stale result summaries before each run. NVIDIA is
  currently unavailable (`nvidia-smi`: NVML Unknown Error). Software coverage still
  exits 2: max offsets 22.39/45.38/28.74 m on .5 walk/.5 sprint/1.5 sprint, with
  5/7, 7/9 and 3/5 outside frames. The 1.5 m core also misses geometry targets.
  No blocking polls/bulk reads/finishes; logical overlap <75 MB. Do not mark t.
- Evidence: build-f5/resume/detail-adjacency-{final.csv,core.log,compute-final.log,
  test-final.log,streaming.log,render.log,captures,movement}; original benchmark
  fingerprints in detail-before-fingerprints.csv. Report: sinking.md; helper
  commands in scripts/benchmarks/terrain_detail/README.md. Preserve the user's
  uncommitted production foliage settings. All later TODO rows remain queued.
- Follow-up above fixes the short configured near-band lifetime. Sustained
  fine-patch coverage and temporal publication still need work in this same row.

## TODO continuation — 2026-10-16 (in progress)

- User requested the p rows first, each tested/documented/committed, followed by
  the remaining TODOs in table order. Preserve the user's production foliage
  density/width/quad-distance edits; they are still unstaged.
- G4 is t and committed as 26dbff8. Report and compact receipt:
  docs/journal/benchmarks/grass-cells. Three alternating RTX pairs retain 9,973
  frames; nine updates copy 87.06% of roots. GPU root time rises; walking p95
  changes 8.909→8.698 ms, maxima remain 107.6 ms. No sustained FPS gain is claimed.
  Logical overlap peaks at 1,001,030,344 bytes, below 1 GiB. Four legacy PNG pairs
  and fresh canonical replay are exact. Dedicated Quadro held-worker compute
  sprint, camera and CPU sprint routes pass the unchanged 22.5 m reserve.
- All 35 core groups passed. All six lifecycle/recovery/frame/reload/shadow/
  legacy-capture groups passed (678.13 s); 43/44 GPU cases passed, then the
  terrain-v4 unknown-version fixture was corrected to 99 and its focused check
  plus moving-detail GPU parity passed. Full historical capture now passes its
  unchanged timeout. Evidence stays ignored in build-f5/resume.
- Terrain remains p. This continuation commits the bounded experimental
  local_detail_streaming increment: topology v4, parent-triangle spatial
  projection, one reusable coarse shell and bounded owned-field probe cache,
  direct ID compaction, parent-anchor replay and separate shader variants.
  Defaults and the user's production config edits remain unchanged.
- Final optimized build passes all 35 core CTest groups, all 44 RTX compute
  cases and seven detail CPU cases. The cache lifetime/determinism test passes.
  The final moving fixture has identical GPU/CPU positions and exact compute/
  CPU/GL 3.3 images and saved-parent replay. Invalid anchors fail before window
  creation. Default terrain GLSL matches G4 after preprocessing. Prior broader
  lifecycle/recovery/frame/reload/shadow/historical captures passed all six groups.
- Production CPU planning benchmark is reproducible through optional target
  terrain_detail_benchmark. At the start/mountain poses, cold v4 planning costs
  1,759/1,351 ms; warm moving plans cost 153–200 ms versus 1,858–2,891 ms for v3.
  Actual 0.5 m core edges stay below 5 cm. Transfer rises to about 5.0–5.3 MB;
  no sustained FPS or continuously covered fine patch is claimed.
- Native coverage is NOT accepted. The reproducible helper
  scripts/benchmarks/terrain_detail/movement.py exits 2: 0.5 m walking/sprint
  and 1.5 m sprint cores lag by up to 1.71/3.40/9.37 m. Most sampled frames lie
  outside the requested core. Terrain publications continue and all native
  blocking/bulk-read/finish audits are zero. Receipts: build-f5/resume/stream-
  movement; the earlier pilot is in stream-native. Do not change thresholds or
  mark the terrain row t. See sinking.md for the complete receipt and limits.
- Next priority is still this terrain row: avoid rebuilding/publishing a whole
  replacement for each local movement, retain/update moving detail and smooth
  temporal publication while preserving shared contacts/grass/shadow/reflection
  ownership and bounded memory. Height-probe reuse alone does not solve it.
  Trail fading and all subsequent TODOs remain untouched, in their existing order.
- Initial tracked diff saved locally in build-f5/resume/inherited.patch. The
  temporary sandbox namespace failure was resolved when permissions returned.

## G3 shared grass animation — 2026-10-10

- Implemented G3 only: a 32-byte plan-owned GPU record caches wind samples and
  trail direction/strength with an input stamp. First-visible blades are
  evaluated once; main/reflection views reuse them, including reflection-only
  blades. Compaction and LOD remain per view; GL 3.3 retains vertex animation.
  Time/history/root changes invalidate stamps; disable/reload/retirement release
  storage. G4 and trail-fade behavior remain queued. No new TODO chain.
- Bounded direct dispatch passes cache-miss checks; the indirect pilot did not.
  GPU counts reject unused threads before cache access, with no CPU readback.
  Native rendering skips diagnostic counter increments. Separate profiler stages
  measure compaction, animation and the two indirect geometry draws.
- Three alternating RTX astronaut-camera pairs include live 6 m/s walking and
  growing trails (156–159 segments). Both-view compaction/animation GPU means
  fall 3.2% idle and 4.3% walking. Overall FPS does not improve: wall p95 rises
  4.2% / 2.7%, with walking maxima around 109 ms. Root CPU submission mean rises
  3.60→15.82 ms; driver/observer/contention effects are not isolated. Offscreen
  EGL, extra profiler queries and external RTX load limit conclusions.
- Actual startup animation cache adds 51,199,648 bytes; conservative overlap
  admission peaks at 980,997,768 bytes, below 1 GiB. Protected density remains
  40.72/m². Startup, ACES, mountain, walking-trail replay and fresh adaptive
  startup PNGs match G2 exactly, with unchanged candidates and policy.
- Nineteen software grass and nineteen RTX staging/publication checks pass;
  eighteen of nineteen RTX grass checks pass. The strict trail equality failure
  also reproduces with frozen G2; software passes the assertion unchanged.
  Eleven CPU groups pass. Native Quadro held-worker compute sprint, 80 m/s camera
  and CPU sprint pass the unchanged 22.5 m reserve and nonblocking audits.
  All six runtime/lifecycle/recovery/shadow/reload/capture CTest groups pass
  (503.63 s), including standing/walking replay and CPU/GL 3.3 fallback.
  G3 is t; G4 and trail-fade work remain queued.
- Report: docs/journal/benchmarks/grass-animation/study.md. Raw captures,
  timings, telemetry and failed pilot evidence stay ignored/local in build-f5/g3.
  Preserve the user's unstaged production density/width/quad-distance edits.

## G2 persistent GPU grass roots — 2026-10-10

- Implemented G2 only: plan-owned 64-byte root/normal/variation records and
  16-byte selected-patch bounds, generated before resident publication.
  Visibility, density, LOD and wind remain per view. CPU-planned compute grass
  also caches roots; GL 3.3 retains vertex placement. No G3/G4 work or new tasks.
- Terrain/config/seed/selection changes invalidate roots; wind time, small
  camera movement and separate main/reflection views retain them. Nonadaptive
  quad distance keeps its existing geometry-only behavior. Admission includes
  all cache storage across live/staged/retiring plans.
- Three alternating RTX pairs show no FPS gain: idle placement GPU mean rises
  about 10–11%, walking about 1–2%; wall p95 changes less than 1%. Root generation
  averages 0.892 ms GPU, with retained CPU submission outliers up to 30.56 ms.
  Walking maximum frames remain about 116 ms. External RTX load and offscreen
  EGL presentation limit conclusions.
- Actual startup cache: 103,035,472 bytes. Conservative idle logical residency
  is 426,579,036 bytes; walking overlap peaks at 852,997,736, below 1 GiB.
  Protected 40.72/m² density and policy remain unchanged. Startup, ACES,
  mountain and fresh adaptive startup PNGs match G1 exactly.
- RTX packing, 16 grass and 19 staging/publication checks pass; 11 CPU groups
  pass. The strict trail test passes on software GL; NVIDIA tiny wind-dependent
  differences also reproduce with G1's original placement shader. Native Quadro
  sprint/camera/CPU held-worker routes pass the unchanged reserve and audits.
  All six software runtime/lifecycle/recovery/reload/capture CTest groups pass
  (384.05 s), including the unchanged strict trail assertion. G2 is t; G3/G4
  remain queued.
- Report: docs/journal/benchmarks/grass-roots/study.md. Compact receipts are
  versioned; raw evidence stays ignored/local in build-f5/g2. Preserve the
  user's unstaged production density/width/quad-distance configuration edits.

## G1 grass preparation reuse — 2026-10-10

- Implemented G1 only: bounded shared GPU terrain metadata, equivalent effective
  policy reuse and proven inactive allocation-search skips. Camera descriptors
  remain plan-owned; source generation/buffer/config changes invalidate reuse.
  Protected density, candidate caps, ordering, replay and CPU/GL 3.3 paths stay
  intact. No G2–G4 implementation or new TODO chain.
- Three alternating RTX native-input-loop pairs reduce walking allocation GPU
  mean 18.375→14.583 ms (20.6%) and CPU submission mean 1.318→0.621 ms (52.9%).
  Repeated-generation metadata mean 0.381→0.163 ms; fresh metadata costs more
  (0.337→0.393 ms). Steady p95 is essentially unchanged; ~109 ms maximum frames
  remain. EGL pbuffer rendering and unrelated GPU load limit desktop FPS claims.
- Extra cache storage is 64 bytes/triangle (6.4 MB at Earth cap), conservatively
  admitted for each overlapping plan. Peak logical overlap is 593,797,736 bytes
  versus 580,997,736 before, below the existing 1 GiB cap.
- Verified RTX UUID, complete timing samples, native nonblocking audits,
  configured 40.72/m² protected density and no near-region infeasibility.
  Startup, ACES and mountain locked replay images match the baseline exactly;
  fresh adaptive startup also matches. Allocation submissions: 239 before,
  143 after and 47 for locked replay.
- Eleven metadata/allocation and twelve publication GPU tests, nine CPU CTest
  groups and six software runtime/capture groups pass. Legacy grass capture now
  explicitly selects its intended CPU upload path and passes. Unchanged native
  Quadro held-worker sprint/80 m/s camera and CPU sprint routes pass, including
  actual near-root counts, stop/reverse and nonblocking audits. Software sprint
  exceeds the unchanged 22.5 m reserve (29.98 m); retain that speed limitation.
  Final rebuilt cache/policy checks pass. G1 is t; G2–G4 remain queued.
  Raw evidence and frozen binaries remain ignored/local in build-f5/g1.
- Journal: docs/journal/benchmarks/grass-allocation/study.md, compact summary and
  frozen inputs; reusable runner/reporter in scripts/benchmarks/grass_allocation.
  Preserve the user's unstaged production density/width/quad-distance edits.

## Current NVIDIA feature timings — 2026-10-09

- Restarted after the user closed another renderer instance. Both physical
  NVIDIA GPUs began at 0% utilization; discarded the contended pilot.
- Completed 30 primary runs (2,970 retained frames) and six separate CPU
  diagnostics on Quadro M1000M / RTX 3070 Ti. Verified GPU UUIDs, complete
  samples, no cached scene renders and unchanged binary/config/ACES asset.
- Journal: docs/journal/benchmarks/feature-costs/study.md, compact summary,
  configuration snapshot and vector GPU plot. Reusable runner/reporter:
  scripts/benchmarks/feature_costs. Raw traces/images/telemetry remain local in
  build-f5/feature-costs/restarted; no new TODO rows or renderer changes.
- Warm frame medians: RTX 3.33–6.15 ms, Quadro 23.38–73.78 ms. Quadro startup
  main atmosphere/HDR is 30.64 ms, opaque 18.39 ms, reflected geometry/atmosphere
  16.56 ms. These stage scopes do not isolate astronaut or grass shading cost.
- CPU Earth topology takes about 3.25 s at startup, including 1.46 s shoreline
  and 1.38–1.41 s surface-error refinement. Walking capture refreshes retain
  roughly 3.6 s stalls; grass allocation also introduces 84–127 ms GPU bursts.
  Capture synchronization is not a native UI-stall or sustained-FPS measurement.
- Preserved the user's unstaged density/width/quad-distance edits in the
  production config. Adaptive allocation remains active; endpoint grass counts
  differ between cards, so no fixed-workload hardware speedup is claimed.
- Python helper compilation, five existing CPU-report tests, pooled data/hash
  validation, repository layout, whitespace and visual plot checks pass.

## NASA ACES runtime replacement — 2026-10-09

- User selected NASA ACES; only that astronaut item was implemented. The row is
  now t. No new TODO rows or unrelated features were added.
- Runtime camera 4 uses all 88,872 triangles, original orange/blue materials,
  our 13-joint/two-influence GPU skin, existing terrain IK and arm/elbow motion.
  Walking stays 6 m/s, sprinting 12 m/s. Jump/flight, planet orientation,
  exhaust, grass trails and replay continue through the existing pose.
- Custom backpack/nozzles are preserved, German patches follow the sleeves,
  and view/model transforms are precomposed in double precision. CPU mesh copies
  are released after the one-time upload (about 6.47 MiB GPU geometry).
- Selected source.glb, version-1 suit.bin, source/prepared hashes and NASA /
  Michael D. Carbajal credit live in data/character/aces. Reproduction uses
  scripts/character/prepare_aces.py with Blender 4.5 LTS. Other source models,
  Blender, logs and raw test output remain ignored/local. No authored clips or
  finger articulation are claimed.
- CPU character/asset tests and the other 34 core groups pass. GPU transform
  feedback checks production skinning/colours, including explicit Mesa GL 3.3.
  Native RTX captures verify walking, flight/exhaust and exact replay; a Quadro
  front capture also passes. Lifecycle, terrain recovery/frame/reload, real
  astronaut keyboard input and the corrected late-failure fixture pass.
- Four README astronaut PNGs/replays and per-image provenance are refreshed;
  fingerprints now include runtime data. Model docs and comparison history
  distinguish the original static NASA mesh from our runtime rig.
- The selected asset is the current default. Resume other requested work only
  when asked; the astronaut replacement requires no additional task chain.


## Detailed astronaut renders — 2026-10-09

- Continued only the requested astronaut comparison. The existing row remains p;
  no additional TODO rows. Five new NASA mesh studies have 15 local front/rear/
  close-up renders and three labelled comparison sheets. Gallery and source notes:
  USER_IO/astronaut_vis/README.md; journal: docs/journal/character/assets.md.
- Rejected AstroDev/Polygonal Mind preview assets and obsolete scripts are removed
  from the active tree. Historical revision a316d1a and ignored local backup
  build-f5/astronaut-next/rejected-low-poly preserve them. Runtime code/config and
  the procedural astronaut are unchanged.
- NASA source revision 11ebb4ee043715aefbba6aeec8a61746fad67fa7 is pinned by
  SHA256 and length. Imported triangles: Z2 30,904; EMU 343,455; Mark III 69,202;
  ACES 88,872; Gemini 114,067. Gemini's 396 repeated-index degenerates are counted
  independently and discarded on import. All five actual files have zero skins,
  bones and clips: they do not close the modern animation-ready model request.
- Blender 3.4 produced broken specular/image results. Final renders use original
  GLBs in Blender 4.5.4 LTS / Cycles, RTX 3070 Ti CUDA, 64 samples, denoising,
  a shared studio and 2 m display height. Preview-only full-transmission/IOR=1
  repairs restore opaque cloth/hardware; receipts list each affected material.
  Original geometry, colours/textures and partially transmitting visors remain.
  EMU attachments are visibly rough; Gemini is explicitly historical.
- Four clearly labelled official rigged-candidate previews are separate from
  our renders. To the Stars is closest to the Ava visual direction; jgilhutton's
  rigged multires EMU offers sculpt detail but needs control/finger/multires checks.
  Antropik has five advertised clips; soph has a basic walk but less realism.
  Sign-in downloads remain unavailable. A paid BlenderKit option is linked,
  without purchase or claimed redistribution/animation verification.
- Fetch/import/image/source/count/hash checks, Python compilation, repository
  layout and git whitespace checks pass. Original models, Blender, rejected
  backup and verbose logs stay ignored/local; no large source/model dumps staged.
- Reproduce using scripts/character/{fetch,render,compare,check}_realistic.py;
  commands are in the gallery. Resume within the same row by inspecting acquired
  preferred rigs: license/credits, joint chains/weights, walking/sprint/jump/flight
  and finger/shoulder/knee deformation, export and pack separation, then grey/blue
  details and German arm flag before runtime integration.

## Bounded centimeter terrain and physical noise — 2026-10-09

- Continued the existing terrain row, which remains p; no new TODO chain.
  Design and results: docs/journal/architecture/terrain-gpu/sinking.md.
  Starting HEAD was 1218cab (the preceding grass fix is already committed).
- Implemented opt-in `local_detail_radius_m`, physical `local_edge_m` and
  `local_error_m`, and bounded distance relaxation via `local_transition_m`.
  Shared longest-edge splitting prioritizes the core, preserves watertightness
  and keeps the existing 100k cap. Captures report actual local edges, sampled
  error and `local_detail_limited` instead of claiming infeasible targets.
- Added planet-fixed `wavelength_m` noise (exclusive with JSON frequency),
  radius-aware lattice limits and matching CPU/GPU normal steps at 5% of the
  shortest active physical wavelength. Bulk heights/normals stay on compute;
  topology/error selection and sparse contacts stay on the existing CPU workers.
  Finest local terrain is unsunk; unresolved waves fade with shared spacing.
  Topology v3 uses unchanged 32-byte samples/128-byte policy/704-byte field.
  Versions 1/2 retain their behavior; unknown v4 is rejected.
- Four production stationary runs, with 410 independent float-mesh height probes
  each, have zero open/reversed triangles and maximum height error 0.153 mm.
  A 0.5 m disk meets 5 cm edges at startup and the reported mountain. A 1.5 m
  mountain disk meets them; startup reaches the cap and correctly reports a
  5.666 cm edge. CPU planning takes 1.5–3.1 s: production defaults stay unchanged
  because the whole-shell planner cannot continuously follow a tiny moving disk.
- Version-3 surface captures now preserve actual planning eyes; surface benchmark
  clipping follows the current frame, matching interactive rendering. This fixes
  a terrain fingerprint mismatch and one-level pixel differences after advancing
  orbital time. The 30-frame production capture, compute replay and CPU override
  are byte-identical (SHA256 a79401ff80e915f50804f90ab34e13102acb2c89c719f5a9eee9544f085b4b83).
- Full build and 43 relevant CTest groups have passing results: 35 core plus
  compute, lifecycle, recovery, frame, reload, capture, native input and held-worker
  grass refresh. Five new CPU cases and the new GPU parity/contact case pass.
  The four affected renderer/capture groups pass again after the replay fix.
  Version-3 walking/trail replay, CPU override, GL 3.3 fallback, legacy-to-v3 reload,
  consumer revision agreement and invalid-anchor rejection are covered.
- Quadro M1000M stationary 480×270, 30-frame uncapped receipt: first frame
  2,517.443 ms; 29 warm median/p95 wall times 11.777/14.595 ms and GPU spans
  11.477/14.298 ms. Stage admission 280,833,196 bytes; zero CPU render mirror/bulk
  evaluations; protected density policy remains 120.72/m² through 15 m. This is
  a small stationary offscreen observation, not a full-resolution/moving 60 FPS
  guarantee. Raw artifacts stay ignored/local under build-f5/ridge-refinement
  and the existing integration output directories.
- Remaining within this same row: exact parent-triangle geomorph, gradual
  publication and a reusable/streamed moving detail patch, with movement/cost
  verification before enabling local centimeter detail in production.

## Latency-aware grass refresh verified on GPU — 2026-10-09

- Existing grass task is t. Interactive refresh observes body-local motion,
  previews upcoming terrain heights, learns actual clamped surface movement
  during preparation and advances the grass center within a bounded lead.
  Publication checks actual coverage after stops/turns; CPU and saved capture
  schedules, placement reserve, density/capacity settings and ownership persist.
- Six native production Quadro routes pass at unchanged configuration/scale.
  The previously failing mountain 320×180 route improves from 36.53 m / seven
  excess frames to 16.23 m / zero. All routes and their stopping frames stay
  within 22.5 m. Peak logical 554.08 MiB remains below 1 GiB; sampled device
  peak 850.06 MiB. UUID is GPU-2cefee61-6b3b-a670-c390-c7449dad3f79.
- Full build and six focused unit/native/frame/reload/replay groups pass.
  Extended held-worker stop/reverse checks pass, including true 79.95 m/s in
  the small fixture, populated near roots and CPU compatibility. Fence fixture
  ordering was corrected to hold terrain after an independent grass refresh.
- Blocking production inspections at startup/60 m measure camera-centered
  protected density 120.82–124.31/m² versus configured 120.72. Actor-centered
  outer bands include ground beyond the camera's protected radius and thin
  under the existing adaptive soft budget; near-infeasible flags remain explicit.
  Production 80 m/s wall speed and global density/60 FPS guarantees are not claimed.
- Report: docs/journal/architecture/terrain-gpu/grass-refresh/study.md.
  Repeatable optional hardware check: tests/app/terrain/native/refresh/test_slope.py.
  All raw output stays ignored under build-f5/grass-refresh and
  build-f5/terrain-grass-refresh. No new TODO rows and no commit; HEAD is 92b9940.
  Next queued topic is the existing centimeter terrain refinement row; it was
  not advanced during this fix.

## GPU restored; fast steep traversal still exceeds grass headroom — 2026-10-09

- NVIDIA device opens, NVML and GLX now work. Every native receipt verifies the
  Quadro M1000M UUID GPU-2cefee61-6b3b-a670-c390-c7449dad3f79. RTX is visible
  but these native checks use Quadro. Code remains commit 92b9940.
- Held-worker regression passes on GPU, with actual 79.97 m/s in the small
  fixture, seven compute refreshes while terrain is held, populated near roots,
  and CPU compatibility. This result alone does not certify production density.
- Six production movement runs use unchanged 100k terrain / 120.72 density,
  paused orbit, live wind and real input. Both 12 m/s sprint and 1280×720 camera
  routes stay inside the 22.5 m margin. All grass-only refreshes occur with CPU
  work pending. No preparation failures; native audits/consumer agreement pass.
  Logical peak 580,997,736 bytes remains below 1 GiB.
- Blocking root/eligible-area inspections show startup near density
  119.25–122.06/m², including a near_infeasible flagged snapshot. Mountain
  waypoints have no eligible visible ground within the analyzed 0–30 m bands;
  farther grass is visible. Do not infer missing nearby grass from that biome.
- Grass TODO stays p: at 320×180 the fast mountain route exceeds its margin in
  seven frames, max 36.53 m (34.65 m radial change, 10.68 m sphere arc).
  Allocation costs 89–91 ms GPU; ready takes 124–131 ms and rendered publication
  147–162 ms. Fixed half-margin refresh is insufficient for fast steep movement.
  Production 80 m/s input reaches only 44–61 m/s wall travel due to the existing
  movement-step clamp. A true full-quality 80 m/s claim remains unsupported.
- Next change belongs to the same existing grass task: measured 3D movement
  and publication latency should guide refresh/limited lookahead. Keep density,
  bounded ownership and locked replay. No new TODO rows, runtime edits or commits.
- Results: docs/journal/architecture/terrain-gpu/grass-refresh/study.md. Raw
  data remains ignored under build-f5/grass-refresh/{nvidia-regression,
  production-nvidia,production-speed-nvidia,production-roots}. The earlier
  diagnostic-settle-roots driver failure is excluded; corrected rerun passed.

## Grass replenishment implemented; production GPU verification pending — 2026-10-09

- Removed the pending terrain-worker veto from interactive compute grass-only
  submission. Refresh requests start at half the unchanged placement margin
  (11.25 m within production's 22.5 m). Installed terrain remains the owner;
  density/cutoff/caps, CPU path, saved policies and capture scheduling stay intact.
- Added TerrainGrassRefreshIntegration with a private held CPU builder and real
  native input. Compute sprint / configured 80 m/s surface-camera and CPU sprint
  all refresh during the held interval, over approximately 92 m of travel.
  Restoring HEAD's refresh code makes the regression fail for grass starvation.
- Actual main-view roots and opaque grass pixels are inspected beyond 80 m while
  terrain is held. Near root bands remain populated; compute reservations peak
  at 33.3 MB, generation consumers agree and native wait/readback audits pass.
  The software surface-camera run reaches only about 12 m/s of wall travel due
  to its existing movement-step clamp, so it does not certify true 80 m/s.
- Full build and final TerrainGrassRefreshIntegration pass. Software reload,
  compute capture/replay and native input integration checks all pass, including
  CPU/GL 3.3 fallback. Logs: build-f5/grass-refresh/{build-probe.log,
  test-refresh-coverage.log,compatibility.log}. Only comments/docs changed since.
- NVIDIA access was lost after the timing measurements: both device opens return
  EPERM, nvidia-smi fails and NVIDIA GLFW context creation fails. Grass TODO stays
  p; do not advance to later features before production GPU coverage checks.
- Report: docs/journal/architecture/terrain-gpu/grass-refresh/study.md. Local raw
  receipts/snapshots/control binary: build-f5/grass-refresh and
  build-f5/terrain-grass-refresh. Timing binaries preserved as
  build-f5/fps-60/{PlanetSimulation-timing,terrain-native-timing}. Commit requested
  by the user; raw benchmark data stays local.

## 60 FPS timing pass completed; grass replenishment next — 2026-10-09

- First remaining TODO is t. Added --uncapped and replaced ridge edge tree
  lookups with a bounded reserved hash table. Final isolated planning medians
  improve 2,559 to 2,346 ms at startup and 2,000 to 1,535 ms at the mountain
  pose (8.3% / 23.3%), with unchanged topology, queries and residual error.
- Twelve alternating before/after CPU/compute image/workload pairs are exact.
  Warm GPU/render cost is unchanged: Quadro cannot meet 16.67 ms at this quality;
  RTX warm offscreen p95 fits. Multi-second capture/reload and native generation
  spikes remain explicit. Native 6/12 m/s speeds stay within 2%; no cached frames
  or resolution changes enter the measured windows. Both reloads publish epoch 2.
- Full build, 33 core and eight focused integration groups pass. Native timing
  initially failed an old CPU-unavailable ledger assertion; validator now checks
  current CPU/compute reservations and the 1 GiB bound. Recheck passes.
- Compact report: docs/journal/benchmarks/fps-60/study.md. Raw receipts,
  prototypes/removed shader experiments and failures are ignored in
  build-f5/fps-60. No shader/config quality changes retained; nothing committed.
- Next existing grass TODO is p. Root cause identified in InteractiveTerrain.cpp:
  a pending terrain CPU job vetoes grass-only submission. Current refresh
  threshold equals the placement's entire movement allowance, leaving no time
  for GPU preparation/publication. Plan: reuse installed terrain for grass while
  topology computes, and request within existing placement headroom. Actual
  coverage, fast surface-camera movement, bounds/replay/fallback checks pending.

## Curvature-guided mountain refinement implemented — 2026-10-08

- Reproduced the reported latitude/longitude, 126.914 m reference-sphere altitude
  and 2 m clearance. The coarse triangular ridge came from distance/whole-face
  steepness subdivision; sinking alone could not resolve its silhouette.
- Standard config now sets geometric_error_m=0.05. Shared midpoint/center probes
  prioritize local interpolation error within the unchanged 100k triangle cap.
  Conforming longest-edge propagation prevents cracks and collapsed triangle
  angles. GPU bulk shape/normals, CPU/GL 3.3 rendering, sparse contacts, grass,
  reflections and shadows share the refined topology.
- At 2,821 fixed probes near that camera, mean height error improves 0.642 to
  0.061 m, p95 2.323 to 0.158 m, maximum 8.914 to 0.259 m. The closed mesh has
  Euler characteristic 2, no reversed triangles and no missed contacts. The
  sampled target remains budget limited; guaranteed centimeter detail and
  parent-triangle geomorph remain in the same existing TODO.
- Pass: full build, 34 core groups (40 terrain cases), all 36 NVIDIA compute
  cases, and six lifecycle/recovery/frame/reload/capture/native groups. Supplied
  pose compute/replay/CPU override PNGs match byte for byte. Refined capture and
  native fixtures cover GL 3.3 fallback, walking/sprint, flight/Moon and reload.
- CPU planning increases from a measured 569 to 2,599 ms at this pose; async
  compute workers retain background rebuilds. No FPS/full-frame p95 claim.
  Resident fixture peak is 12,854,508 logical bytes; supplied-pose production
  stage admission is 280,833,196 bytes with protected foliage density retained.
- Before/after captures (including paired lighting diagnostics), raw tests,
  probe source and receipts stay local in build-f5/ridge-refinement. Report:
  docs/journal/architecture/terrain-gpu/sinking.md. No staging or commits.

## Relief-aware terrain sinking implemented — 2026-10-08

- Standard config enables terrain_lod.relief_sinking. The finest near surface
  stays unchanged/unsunk; existing noise blends into coarser filtered fields.
  Sink depth follows omitted relief plus a curvature estimate, capped by config
  and planet size. Shared radials agree after shoreline refinement; normals
  include sink gradients. Water stays at its physical level without filtering.
- CPU contacts/cache and GPU bulk evaluation share a bounded 128-byte generation
  policy. Topology version 2 fingerprints the policy; old configs/replays retain
  version 1 and exact legacy geometry. Worker/reload/publication consumers carry
  the selected version; unknown versions still fail. Memory admission charges
  the GPU pack, while readyVectorBytes keeps its vector-only meaning.
- Pass: full build, all 34 core groups (37 terrain cases), all 36 NVIDIA compute
  cases, lifecycle/recovery/frame/reload, exact capture/replay/CPU override,
  GL 3.3 fallback and native walking/sprint/flight/Moon/reload. Final production
  compute/replay/CPU override PNGs match exactly, with 1,599,906 foliage slots.
- Canonical 100k transfer remains 76.66% lower than CPU, bulk CPU evaluations
  150,006 to zero; warm GPU field/expansion median 39.513 ms. No new full-frame
  p95 claim. Report: docs/journal/architecture/terrain-gpu/sinking.md.
- Only the sinking/noise-filter portion was authorized and completed. The same
  terrain TODO stays p: centimeter subdivision, explicit wavelength controls
  and exact parent-triangle geomorph remain queued. Spatial filtering does not
  eliminate generation/topology steps. Raw data local in build-f5; no commits.

## Compute default promoted — 2026-10-08

- Fresh launches and captures now default to compute terrain with resident
  `gpu-v1` grass planning, following the passing F6 acceptance gates. Explicit
  `--terrain-backend cpu` and automatic GL 3.3 fallback remain supported.
  Saved replay backends remain authoritative; older replays without backend
  metadata retain CPU behavior unless explicitly overridden.
- Full incremental build passes. All 30 lifecycle cases, terrain frame/native
  input and expanded compute capture groups pass, covering default compute,
  explicit CPU, walking/sprint, reload, historical/exact replay and GL 3.3
  fallback/locked rejection. Repository layout and diff whitespace checks pass.
- Standard config capture without config/backend flags selects managed compute,
  sparse contacts and zero CPU render vectors. PNG SHA256 remains
  `40053d719757f468dee76a4fd0b694dfced61bf7981d263b5ee3d5aad718ba22`,
  identical to the accepted F6 production image, with 1,599,906 candidate slots.
- README, completed F6 record and foundation audit reflect the new default.
  Algorithms and measured backend implementations are unchanged; existing F6
  benchmark hashes predate this selection change. Logs/capture stay local under
  build-f5/compute-default-*. Nothing staged or committed; no additional TODOs.

## F6 completed — 2026-10-08

- Supported CPU planning now shares resident compute's protected-near/fitted-tail
  allocation, stable rounded slots, memory/timing controller and explicit deficit
  reporting. GL 3.3 uses the same profile/coverage; historical render captures
  retain legacy distribution. Saved effective policy survives CPU override and
  reload, rejecting insufficient memory rather than silently thinning quality.
- CPU admission charges terrain, queues, references and live/replacement/retiring
  owners. Terrain uploads preflight old+new storage. Fixed a production startup
  bug by consuming fresh worker cache before CPU admission and refreshing the
  inherited replacement controller; the untraced cache regression passes.
- Production fresh CPU / compute / CPU override produce byte-identical PNGs,
  identical policy and 1,599,906 slots: 120.72 blades/m² through 15 m, sigma
  8.435062249191105 m, no near deficit. Common cost input retains density/width
  with full 2M allowance for moving/replacement topology.
- Both 25 m walking/sprint common-view inspections pass exact 1.0 ratios in all
  three bands for eligible area, root density, fade and composed coverage. All
  sample minimums pass; composed grass pixels exactly 624,964 / 625,183 per backend.
- All 15 alternating matched-quality cost pairs pass unchanged <=1.05 p95:
  stationary .995/.922/1.021; walking .783/.795/.766; sprint .656/.665/.601;
  grounded Moon .728/.983/1.049497; reload 1.007/1.003/1.034. Identical frozen
  source/tool/application/probe hashes throughout; no measured outliers removed.
  Moon uses a public landed pose; handoff remains separately tested.
- Walking 403–448 frames at 5.978–5.995 m/s; sprint 351–405 at 11.987–11.995 m/s.
  Peak logical overlap 871,735,956 bytes <1 GiB (CPU reload 551,430,464); largest
  periodic device-wide used sample 884,146,176 bytes. Every reload finishes epoch
  2 with one publication, equal final planning eye and no failure/pending/retiring
  resources. CPU reload still stalls synchronously; retained worst frame 2,579 ms
  versus compute 176 ms. No claim that p95 describes that stall.
- Pass: 34 core groups, 35 NVIDIA compute cases, 32 Mesa render cases, GL 3.3
  protected/fallback/replay, six integration groups plus recovery/reload audits;
  final cache fix additionally rechecked memory, CPU/legacy reload, allocation
  and production default/override. Xvfb GLFW initialization race passes on rerun
  with -noreset. Full feedback verification is Mesa; historical NVIDIA exact-trail
  feedback limitation is not resolved by F6.
- Report: docs/journal/architecture/terrain-gpu/grass-allocation/cpu-integration.md.
  Raw captures/traces/compressed receipts stay ignored/local under
  build-benchmarks/runs/f6-20261008/. Setup attempts remain excluded/local.
  CPU stays default, compute opt-in experimental. F6 marked t; no new TODO chain,
  staging or commits. Other user features remain queued until requested.

## F5 completed — bounded acceptance decision, 2026-10-08

- Fresh optimized build-f5 built from pinned dependency sources, without project
  objects or historical archives. Quadro M1000M / NVIDIA 580.178.04 graphics UUID
  verified; RTX 3070 Ti is not the rendering device for these receipts.
- Fifteen alternating CPU/compute pairs completed: stationary, 300 m walking,
  450 m sprint, common saved production Moon handoff and production seed reload.
  Rates 5.990–5.991 / 11.982–11.989 m/s. No measured outliers filtered; replacement
  preparation/publication/retirement included. Largest compute logical overlap
  871,735,956 bytes <1 GiB; contacts and consumer revisions coherent throughout.
- Canonical 100k triangles: 76.659% cold / 76.665% warm terrain-transfer reduction;
  bulk CPU field evaluations 150,006→0. CPU generation median 400.002 ms versus
  compute CPU submit 13.0627 ms; GPU median 34.6794 ms, capture wait separate.
- Quality fails: stationary effective density 47.149 CPU versus 120.72 compute.
  Independent common-view coverage at 25 m finds root-density ratios
  2.615/1.539/0.000 in 0–5/5–15/15–30 m bands; far-band sample minimum fails.
  Moon trajectories also differ after natural flight. Faster diagnostic frames
  therefore do not satisfy matched-quality p95 acceptance. CPU remains supported
  and default; compute remains opt-in experimental. Speed goal not claimed done.
- Pass: 34 core groups, 35 NVIDIA compute cases, eight integration groups and
  three exact capture/replay groups; 30 Mesa GL 4.3 terrain/foliage cases; Mesa
  GL 3.3 atmosphere 12 pass / one explicit GPU-only skip. Native input covers
  forced fallback, default CPU, locked replay rejection, directional boost,
  Moon/reload/fault recovery. Production boosted PNG/pose replays exactly.
- Small measurement-only changes: canonical receipt, bounded Moon/reload wrapper,
  production-tilt-aware public flight seed with optional visible boosted launch,
  single-case route reruns and F3 effective-budget-aware receipt assertions.
  Unmatched budgets still fail comparison. Initial obscured seed/wrapper setup
  failures and 197-frame 300 m sprint are retained locally; sprint rerun at 450 m
  preserves the existing 240-frame minimum. No runtime/default/tolerance changes.
- Compact report: docs/journal/architecture/terrain-gpu/acceptance.md. README and
  todo reflect the completed evaluation and failed acceptance decision. Raw logs,
  traces, captures and receipts stay ignored/local in
  build-benchmarks/runs/f5-20261008/. No new TODO chain or unrelated feature work.

## F4 completed — 2026-10-08

- GL 4.3 highlight selection now exactly histograms the existing RGBA16F peaks,
  weights partial tiles by actual pixel area and preserves the actual-white-pixel
  sparse-star guard. Three dispatches use fixed 131,076-byte histogram storage;
  GL 3.3 retains the supported CPU reference. Optics/integration are unchanged.
- One 8-byte double exposure receipt replaces the full tile-map readback and CPU
  sort. Existing passes consume the current-frame value, cached frames retain it,
  and fixed reduction resources survive viewport changes. Scalar synchronization
  remains; no zero-readback claim.
- Quadro 1080p focused saturated wall median/p95: 3.558/4.123→1.580/1.754 ms;
  sparse 1.951/2.306→1.521/1.690 ms. Transfer 259,200→8 bytes. At 129×121, fixed
  dispatch work adds 0.08–0.11 ms. This is focused presentation, not F5 acceptance.
- Pass: 34 headless core groups; 13 NVIDIA and 13 Mesa GL 4.3 atmosphere cases
  (72 CPU-reference selections), 12 Mesa GL 3.3 cases plus explicit compute skip;
  five renderer reload and four interactive reload cases. Nine canonical Mesa
  atmosphere and three twilight scenarios pass their existing replay checks.
  Twelve PNGs/metrics per driver match that driver's pre-change baseline exactly.
- Pre-change NVIDIA airless camera-snippet/twilight saved-replay failures remain
  recorded; no tolerance changes. One intermediate software run used a stale
  binary after a shader-format change and is excluded; final runs rebuilt cleanly.
- Compact report: docs/journal/architecture/atmosphere/highlights.md. Raw receipts,
  CSV and captures remain ignored/local in build-benchmarks/runs/f4-20261008/.
  F4 is complete. F5, CPU terrain default and unrelated user documents are unchanged.

## F3 implementation and verification — 2026-10-08

- Resident GPU planning protects the configured quad-distance region and fits
  the distant Gaussian tail before reducing near density. Near-infeasible and
  per-triangle-cap cases report a deficit and keep queues bounded.
- Admission includes terrain/planner/queues and live/staged/retiring/replacement
  reservations. Existing worker supplies cached VRAM without trace files;
  missing/stale readings use 64 MiB stage / 256 MiB aggregate limits. Existing
  frame queries/utilization drive a small quota controller with cooldowns.
- Effective version-1 per-body policy survives capture/replay/reload; incapable
  admission rejects a locked replay rather than thinning its saved quality.
- Quadro production initial camera retains 120.72 blades/m² through 15 m;
  sigma 8.435062249191105 m fits 1,599,906 candidates under 1,600,000. No near
  deficit. Logical reservation 290,578,652 bytes. Exact production PNG/policy
  replay; raw runs remain ignored/local in build-benchmarks/runs/f3-20261008/.
- Pass: 34 core groups, 34 NVIDIA compute cases, seven Mesa allocation/admission
  cases, 14 Mesa foliage render cases, 13 renderer frame/memory cases, four reload
  cases, capture contracts and full native input/reload/space/Moon/fallback
  audits. Native audits
  retain zero blocking/bulk reads/finishes/GL memory queries. A GL 3.3-only
  NVIDIA feedback test explicitly skips the new compute coverage case; Mesa
  GL 4.3 runs it. Existing F2 trail-equality limitation remains unchanged.
- Compact report: docs/journal/architecture/terrain-gpu/grass-allocation/adaptive-falloff.md.
  CPU stays default. F3 is complete; F4/F5 and unrelated user files remain untouched.

## F2 completed — 2026-10-08

- NVIDIA device opens and native Quadro GLX work again. Hardware order controls
  identify the main detailed-grass draw: CPU/compute medians 4.343/8.014 ms;
  exact near-first permutations bring both to about 3.434 ms.
- Resident allocation now distance-orders triangle references within slot
  levels on GPU, using existing rank scratch and stable triangle-ID ties.
  No added buffers or CPU Blade queue reads; density/quality/placement unchanged.
- Fresh fixed-wind baseline ratios 1.099687/1.062302/1.089736 fail. Corrected
  fixed ratios 1.023953/1.018824/1.012084 and normal-wind production ratios
  1.008686/1.021469/1.012668 all pass the unchanged 1.05 stationary p95 gate.
  Each cohort has three alternating pairs and 1,440 measured frames.
- Exact before/after production Blade multisets, commands, matrices, depth
  and displayed RGB; main counts 35,327/153,563, reflection 35,019/165,221.
  Runtime audits retain zero blocking waits, bulk reads, finishes/memory queries.
  Logical compute overlap stays 290,578,652 bytes. Initial plan allocation GPU
  median increases from 20.318 to 25.532 ms; moving/reload acceptance is F5.
- 31 compute integration tests pass (direct 44.96 s, CTest 40.80 s); four
  independent allocation-oracle tests pass on NVIDIA and software GL. Thirteen
  software grass-render tests and scene-replay/CPU-plan/placement groups pass.
  Native input checks pass contacts/replay, 6/12 m/s, thrust, reload/watch,
  space/Moon, delayed clock, default CPU, GL 3.3 fallback and startup rejection.
- Retain first cold compute CTest timeout (120 s) and the pre-existing NVIDIA
  trail exact-equality failure at test_grass_render.cpp:393. That same trail
  test fails unchanged F1; its CPU-planned fixture does not use the new sort.
  No tolerance or unrelated runtime fix; no full-suite success claim.
- Compact report: cost/raster/order/correction.md and correction.json. Raw
  receipts and lossless storage stay in ignored build-benchmarks/runs/f2-20261008/.
  CPU stays default. Stop after F2; F3–F5 and unrelated user files untouched.

## F1 completed — 2026-10-08

- Clean source snapshot (no historical compressed maps/JSONL traces) configured
  and built with GCC 12.2, Ninja and RelWithDebInfo. The interrupted build resumed
  incrementally. With DISPLAY unset, all 34 core CTest groups pass in 13.51 s,
  including six archive-policy cases. Rendering/performance acceptance is F5.
- 9,011 raw files (10,421,009,829 bytes) moved without deletion into ignored
  build-benchmarks/archives/; 599 previously tracked raw traces removed from Git.
  Source fixtures, compact results and original evidence hashes remain versioned.
- Ten direct optional validators report UNAVAILABLE/exit 2 with new-run commands
  when payloads are absent. A complete stationary archive revalidates 1,440
  measured and 1,955 excluded frames. Changing a gzip header while preserving
  decoded bytes still fails the original hash check. Original validators are
  retained byte-for-byte; validation results never overwrite historical summaries.
- Twelve active benchmark writers restrict output to local build/external dirs.
  Policy/catalogue: scripts/benchmarks/archives/. Logs and compact receipt remain
  local in build-f1/. No gameplay/shader changes; F2–F5 untouched. Stop after F1.

## Blade-order private controls; native GPU access blocker — 2026-10-07

- Keep T3c5c2b4 as one remaining TODO outcome: diagnose and fix the regression,
  then verify affected full workloads. Detailed checkpoints stay in the journal
  rather than new TODO topics. CPU default and original 1.05 gate retained.
- Source difference found: CPU descriptors are camera-distance ordered within
  slot levels; resident references retain triangle order. This is a hypothesis,
  not causal attribution. No shipping source/shader optimization was applied.
- New private `terrain_order_probe` reuses the frozen native/wind wrappers.
  Reads actual main/reflection queues, saves every 64-byte Blade and command,
  applies stable native/near/far permutations using actual float camera depths,
  and counts excluded reads/query/roundtrip uploads. Restores original buffers.
  Every frame explicitly rejects timing/migration acceptance; captured frames
  join their own inspection receipt exactly.
- Final software CTest passes in 35.27 s, including fresh-output reuse:
  18 snapshots, exact contents/matrices/depth and zero RGB pixel changes.
  Main detailed/quad counts 772/396; reflection 11/166 on both backends.
  All queues nonempty. Retain 57 CPU and 1,253 compute native observations.
- Independent archive validation checks all 72 final/preflight snapshots and
  1,675 payloads, byte-verified after gzip: 110,730,278 raw to 32,018,882 stored.
  Coarse empty-quad 50.23 s fixture, first refined run and the 18-frame
  preflight before explicit per-frame exclusion flags remain separate.
  Exact-double assertion exposed 1.421e-14 m contact roundoff; exact float roots,
  native camera and GPU matrices are verified and raw double drift is reported.
- Shipping/shaders and all 45 previous executables/fixtures unchanged;
  final 46 executable/input hashes frozen. Exact old driver recovery reproduces
  coarse preflight input-tree hash. New probe/scoped checks only, no full suite.
- Native Quadro fixture fails before frames: GLFW window creation fails;
  offloaded GLX returns X_GLXCreateNewContext BadValue, EGL returns 0x3001.
  NVIDIA0/1/control/modeset/UVM opens all return EPERM despite available NVML
  inventory/telemetry. Sysfs-derived temporary DRM nodes also fail and were
  removed. This is external device access, not a benchmark or code regression.
- Preserve all failures and restore device access before using the recorded
  fresh native fixture/production commands. Full study, helper sources,
  provenance, codec receipts, raw images/queues/frames and validator:
  cost/raster/order/. Journal/PDF updated; user TODO footer unchanged/unstaged.

## Stationary wind/raster controls — 2026-10-07

- T3c5c2b3's control investigation is tested and documented. Full-draw cost
  acceptance fails; resume T3c5c2b4 cause/correction and affected full-workload
  repeats. CPU stays default; the original 1.05 gate and earlier failures remain.
- File/shell/GPU access recovered after the sandbox interruption. The original
  runner had completed all 54 runs; no batch was restarted or overwritten.
  Quadro M1000M and RTX 3070 Ti are accessible; this study uses verified Quadro.
- Three alternating pairs per native/fixed/indexed wind and full/discard/
  suppress control: 27 pairs, 12,960 measured and 18,320 excluded frames.
  All 426 upper outliers remain. Independent raw frame/GPU/device/geometry/
  pose/anchor/worker/publication/wait/readback checks pass.
- Fixed full native p95 ratios 1.094387/1.150303/1.144083 fail; all fixed
  discard/suppress pairs and indexed ablations remain below 1.05. Indexed
  full pairs 2/3 fail. The repeatable difference is raster-enabled/downstream
  work; root population, order, overdraw and chunking causes are not proven.
  Native suppress pair 1 also fails, retaining background variability.
- Final software control CTest passes in 69.00 s; native Quadro also checks
  27 controls/108 frames. Actual main/reflection shader times and excluded
  primitive/passed-sample queries verify controls on legacy/resident/vertex
  paths. Qualification is not timing, production-density or migration acceptance.
- Shipping/shaders, fixtures and 44 previous executables unchanged; 45 final
  executable/input hashes frozen throughout measurements. No new full suite.
  Earlier successful label/count preflights and a Path adapter validator error
  remain separate. All 703 data payloads match source bytes after decoding:
  152,526,975 raw to 7,740,877 stored bytes. Gallery/historical captures unchanged.
- Full study, source/probe provenance, raw archive, independent validator,
  result distributions and Matplotlib SVG: cost/raster/. Journal/PDF updated.
  User environmental footer remains byte-identical and unstaged.

## Fresh qualified analytic route pairs — 2026-10-07

- T3c5c2b2c3 and parent b2c tested. Resume stationary wind/opaque-raster
  investigation T3c5c2b3. CPU remains default; earlier failed samples,
  configured near-density and timing/migration gates remain recorded/pending.
- Twenty-four native Quadro inspections form twelve passing alternating pairs
  across walking/sprint at 25/350 m. Gates remain 5% per band/pair and 1%
  area convergence; maximum production change 0.61349%, native tags exact.
- Adjacent frames bracket first crossings: overshoot up to 4.665 m, bracket
  width 5.988 m and paired native root separation 4.448 m. Native plans,
  topology/field/anchors/revisions and effective-density receipts retained.
- Five same-topology pairs share the smaller generated rounded-1-mm root set;
  seven different-topology pairs share at most 0.0224%. This is diagnostic
  correlation, not causal proof or the opaque-visible density metric.
- Full independent raw reconstruction passes: final 40 live snapshots,
  120 grids, 360 biome checks and 80 queue/depth snapshots; three preflight
  queues separately retained. Two scoped software groups pass (126.34 s),
  four driver tests and eight fresh verified native fixtures pass.
- Float inverse-view clip mismatch fixed using the actual double-precision
  chase eye, retaining exact projection assertions. Only the private probe
  rebuilt; product/shaders and 43 other executables unchanged.
- All 13,012 route frames are excluded from timing. Native displayed color
  proxy retains 13.45% edge-area uncertainty; no physical compositing bound.
- Byte-verified lossless storage keeps 2,136 raw entries and all failures,
  including interrupted codec and manifest-order packaging preflights.
  Full study/provenance/validator, journal/PDF: cost/coverage/repeats/.
- User environmental footer in todo.md remains byte-identical and unstaged.


## Live analytic inspection qualification — 2026-10-07

- T3c5c2b2c2 tested; parent b2c remains in progress. Resume b2c3 three
  alternating pairs each walking/sprint at 25/350 m; retain the 5% pair gate.
- Private coverage_live pass draws installed land/other-body VAOs and full
  astronaut meshes independently at 1x/2x/4x, using fixed-projection viewport
  tiles. GPU target 4.25 MiB; C++ tile read buffers 1.0625 MiB; existing native
  diagnostic targets and Python arrays are additional, not included in that bound.
- Three frozen software CTest groups pass in 161.68 s. Eight software/eight
  verified Quadro fixtures plus four unchanged production snapshots qualify
  60 grids and 180 biome diagnostics. Max convergence 0.78655%; production
  max 0.27135%; native opaque tags exact. 64 queue/depth receipts rechecked.
- Native color-comtribution mask stays at display resolution; production
  boundary-area uncertainty reaches 13.45% near the astronaut. No physical
  supersampled composition, timing or three-pair route acceptance is claimed.
- Plans/anchors/publication/workload/revisions stay native; complete character,
  trail/effect telemetry restored. Shipping source/shaders and 43 other
  executables unchanged. NativeSession close race fixed and final checks repeated.
- Coarse convergence, mixed analyzer schema, preliminary rebuild and clean-exit
  handshake preflights retained separately. Full lossless evidence, validator,
  journal/PDF in cost/coverage/live/. CPU default and wind/raster b3 remain.
- User environmental footer in todo.md remains byte-identical and unstaged.



## Known-geometry analytic area qualification — 2026-10-07

- T3c5c2b2c1 is tested; b2c parent stays in progress. Resume b2c2 live
  production inspection integration/convergence, then b2c3 fresh route pairs.
- New private terrain_area_probe uses unchanged matched ground shaders and
  actual float-rounded triangle inputs. GPU owns raster visibility/eligibility
  and planes; excluded CPU owns ray intersections and analytic band integrals.
- Four fixtures cover front/65-degree planes, foreground occlusion and an
  82-degree grazing silhouette with 1000 m scale/rotation/world translation.
  Independent exact polygon/circle references use homogeneous frustum and
  projective silhouette clipping, including horizon-crossing foreground.
- One software CTest passes in 53.80 s; 16 identical-scope native Quadro
  snapshots pass with actual GL 3.3 and UUID verification. All 32 final raw
  snapshots rechecked independently. Max 4x reference error 0.34497%, 2x/4x
  change 0.42118%, tile change 0.000551%; gates remain 1%/1%/0.01%.
- GPU target bound is 2.25 MiB logical attachments; CPU raw maps reach
  112.5 MiB plus analysis temporaries. This is excluded diagnostic readback,
  not native timing or measured NVML peak. Software is llvmpipe, not hardware.
- Initial coarse grazing convergence failure and refined off-axis tile edge
  failure retained. Actual rounded reference replaces ideal construction;
  unchanged projection plus translated viewport/scissor removes edge rounding.
- Journal/PDF, full lossless final/preflight maps, inputs/logs/provenance and
  archive validator in cost/coverage/area/. Product source/shaders and all 43
  previous executables are unchanged. No new full-suite or route acceptance.
- b2c2 must qualify production biomes/planes and subpixel character/other-body
  occlusion at the declared mask resolution; merely upscaling native stencil
  is insufficient. Preserve live generations/anchors/publication/workload and
  verify restoration/ordinary inspections before frozen b2c3 routes.
- Earlier coverage failures, overshoot/topology-root variance and stationary
  regressions remain. CPU default, wind/raster investigation b3 and later
  migration/near-density tasks remain. User environment notes stay unstaged.


## Matched native foliage comparison — 2026-10-07

- T3c5c2b2b comparison/inspection qualification is documented and tested;
  overall coverage and cost acceptance remain pending. Resume T3c5c2b2c first.
- The private probe wraps CPU GrassRenderer::prepare only for controlled
  renderScene inspections, keeping actual native generations/anchors/budgets.
  Common first-CPU camera/projection/full grounded pose/trails, scene time 12 s,
  and empty exhaust are shared. Saved motion/trail owner state is restored.
  Workload, publication and geometry revisions must stay unchanged.
- Full grass/suppressed-grass scenes retain character/water/atmosphere/tone map.
  Nearest opaque grass stencil 5 plus RGB difference >=1 is a composed proxy;
  both exposures retained. Live VAO ground pass exports offsets, weighted area,
  geometric plane and unweighted area. Ray/plane root visibility fixes a
  pixel-center bias with an analytical test. Flare/overlays are outside scope.
- Two scoped software groups pass in 44.98 s; eight Quadro fixtures pass.
  Independent raw checks cover 48 native snapshots, including 40 controlled
  (8 software +8 Quadro fixtures +24 production routes). Product source/shaders,
  app/timed probe and 42 prior executables/driver fixtures stay unchanged;
  all 43 final executable/input hashes remain frozen through measurements.
- Three alternating pairs each walking/sprint at first 25/350 m crossings
  retain 12,910 excluded inspection frames. Eight of twelve primary pairs pass
  the declared +/-5% per-band gate. Walking-25 pair 1 has near density/fade
  ratios 1.05362/1.05772; same ground but different topology/seeds, zero exact
  generated-root overlap. Do not loosen/remove this discrepancy.
- Sprint-350 outer finite-quad area ratios 0.94390/0.93951/0.94340 fail.
  Analytic differential area sensitivity gives 1.01073/1.00559/1.01030,
  identifying finite-quad/silhouette bias. These are post hoc diagnostics,
  not retroactive acceptance or proof of actual ground loss. b2c plan:
  GPU live raster eligibility/planes in bounded tiles; CPU excluded analytic
  integration; qualify known planes/silhouettes and <1% 2x/4x band convergence,
  then fresh frozen three-pair runs. First crossings overshoot up to 5.518 m;
  common roots are 25.546/25.048/351.020/355.518 m, not exact target landmarks.
- Stationary repeat via unchanged ordinary timed probe retains 1,440 measured
  +1,610 excluded frames. p95 ratios 1.05789/1.11058/1.04965: two fail 5%.
  GPU opaque p95 differs by 4–6 ms; placement means only +0.18–0.35 ms per
  view. Record/match wind clocks and isolate grass raster/overdraw (b3).
  Wind was active and absent from timed receipts; effect clocks are not wind.
- Original import/path/NumPy launcher failures and superseded pixel-center
  preflight retained separately. Full queues/depth/colors/stencil/ground maps,
  traces and inputs in journal cost/coverage/matched/validation. Standard XZ
  losslessly shrinks 52 large ground maps by 187.4 MiB; decoded lengths/SHA
  match original readbacks. Validator supports gzip/XZ. This is not runtime
  BSON caching. No new full-suite or migration-speedup claim; CPU stays default.
- Gallery and ten historical PNG hashes unchanged. Rebuilt/reviewed journal
  figure/PDF and TODO document remaining gates. Preserve the user's unstaged
  environmental footer in todo.md byte-for-byte; only stage the agent prefix.


## Live native foliage inspection — 2026-10-07

- T3c5c2b2a is tested; b2 parent remains in progress and b2b is next.
  terrain_coverage_probe links unchanged product libraries, reusing NativeProbe
  with a renamed presentation interposer. Only this private binary hooks draws.
- It reads both main-view command/64-byte Blade queue prefixes before reflection
  reuse and opaque depth before grass/after both queues. Raw buffers and masks
  retain actual camera matrices, field/topology/revision, publication before/after,
  grass anchor/effective density, wind, full astronaut pose and trail history.
  Synchronizing read counts/bytes and exclusion from timing are explicit; normal
  native audits remain separate, so zero ordinary reads does not hide inspection.
- Two final scoped software tests pass (22.93/28.56 s), including reused output.
  Eight actual Quadro CPU/compute airless/HDR standing/moving fixture snapshots
  and two production 1280x720/100k Earth/2M budget 350 m sprint snapshots pass.
  Independent archive checks cover all 26 final roots/depth/native-frame joins,
  masks, band counts and verified hardware UUIDs. No new full CTest run claimed.
- An 8.00 s rerun exposed stale snapshot readiness after an initial 53.70 s pass.
  JSON now publishes atomically, requests remove stale metadata and native-frame
  joins verify fresh pose/distance. Failed mixed old/new preflight artifacts/log
  remain separate, excluded from final qualification.
- Production actual crossings are CPU 350.122 m, compute 350.711 m, wind
  35.438/35.569 s and trails 536/563. Main queues are 25,947+159,532 versus
  25,656+158,001; later reflection queues differ. Opaque grass pixels
  674,457/673,039 are illustrative, not parity. These endpoints do not reproduce
  the larger earlier compute sprint lags; repeating common landmarks is essential.
- Shipping renderer sources/shaders, app, timed native probe, all 42 prior
  executables and driver fixtures are unchanged; only the inspection executable
  is added. Raw final/preflight evidence and frozen new test/script inputs are
  in journal cost/coverage/validation; README, journal/PDF and TODO updated.
- Resume T3c5c2b2b: fixed common camera/wind/pose/trail state and repeated native
  generation landmarks, eligible visible-ground area, later character/water/
  atmospheric occlusion and declared tolerances. Root queues/opaque masks alone
  do not establish roots/m² or final rendered density. Prior stationary p95
  ratio 1.0853 still needs investigation/repeat; Moon/reload/final gates and
  protected near-density B1 remain. CPU stays default.

## Native route cost receipts — 2026-10-06

- T3c5c2b1 is tested: twelve fresh Quadro native production 400 m routes,
  three alternating pairs each for actual 6/12 m/s W/Shift. The common
  [5,400) m cohorts retain 5,276 measured +3,797 excluded frames and all
  461 upper outliers; 192 fixed 25 m landmarks retain actual workload/keys.
- Quality/budgets/device pass; roots/cameras match within 6.42/6.44 cm.
  Native p95 compute/CPU ratios are 0.7056–0.7312 walking and 0.5557–0.7045
  sprint. Some small distance bins are slower on compute; no spikes removed.
  These are cost preflight, not accepted migration speedups.
- Landmark effective density ratios span 0.9710–1.0438. Sprint root-to-grass
  planning-eye p95 is 25.15–25.28 m CPU versus 33.44–34.64 m compute,
  maximum 43.82 m compute. This includes the chase offset, not actual
  root-density evidence. Configured 120.72 blades/m² remains budget-scaled;
  protected near-density B1 is still pending.
- Full measured GPU/frame records and 12,798 GPU work rows are ready. All 511
  publications (published/coalesced/shutdown) and memory age/status rows remain.
  Periodic device-wide peaks are CPU 740.5 MiB, compute 769.5–797.125 MiB;
  compute full-run logical overlap is 554.08 MiB. CPU unmanaged ledger is null.
  The first complete CPU preflight exposed a missing-ledger analyzer bug;
  original trace/fingerprint/failure log is retained, separate from fresh pairs.
- Runtime/probe/all 42 executables/driver fixtures remain unchanged from speed
  validation. Final source/new-script inputs and raw streams are frozen in the
  journal cost/routes/validation archive. Independent validator checks joins,
  speed, landmarks, quality/budgets, bins, outcomes and device/memory scopes.
  No new runtime build or full CTest run is claimed; gallery hashes unchanged.
- Resume T3c5c2b2 using cost/routes/study.md inspection plan: actual native
  near-root root distribution and grass-only masks at fixed landmarks/pose/wind,
  retaining live terrain/grass anchors and generation identities. Synchronizing
  readbacks belong in separate excluded runs; synchronous capture replanning
  cannot substitute for the native live generation. Fresh stationary ratio
  1.0853 still needs stage/workload investigation and repeat controls.
  Moon/reload T3c5c3 and final gates T3c5c4 remain. CPU stays default.

## Grounded native speed prerequisite — 2026-10-06

- T3c5c2 split into T3c5c2a actual speed and T3c5c2b matched route cost/coverage.
  Production Quadro baseline fails real-time speed: CPU/compute walking
  3.504/3.605 m/s, sprint 7.028/7.167 m/s; animation rates alone hid the issue.
- Interactive grounded motion now consumes elapsed time in <=50 ms camera/contact
  advances, bounded to one second after suspension. Installed contact/draw
  generations and once-per-frame terrain/GPU publication remain coherent;
  flight and jump activation retain their existing integration.
- Six CTest groups pass in the initial 217.51 s batch. The augmented native
  group passes in 92.11 s after correcting its timing fixture: first confirmed
  input plus >=10 intervals, warm flat/coarse clock scene without foliage.
  The original noisy input/reload and space/Moon scenes remain. Runtime and all
  42 executables stay unchanged through the Python fixture correction. Failed
  preflights are retained; final clock wall speeds are 5.965–6.003/11.886–12.011.
- T3c5c2a is tested: final natural and +250 ms production 100 m checks measure
  walking 5.971–5.978 and sprint 11.906–11.971 m/s. Sixteen speed/preflight
  runs retain 933 movement +4,231 excluded frames, including the initial short
  CPU sprint failure (91 ms endpoint difference); no outliers were filtered.
- Three fresh stationary pairs preserve exact geometry/pose/anchors and scalar
  work: 1,440 measured +1,942 excluded frames, 3,486 ready GPU work rows and
  twelve publications. p95 ratios 1.0368/1.0227/1.0853 include observer overhead;
  pair 3 exceeds the 5% migration limit. Full GPU span p95 is also higher, but
  the traces do not isolate a cause. Investigate complete stages/workloads and
  repeat affected controls before final acceptance. CPU remains default.
- Full compressed receipts, corrected/failed software fixtures, frozen before/
  after source/input/42-binary hashes, standalone speed figure and independent
  checks are in the journal cost/movement study; journal/PDF and TODO updated.
  Resume T3c5c2b three alternating longer route pairs, distance landmarks and
  rendered near-root density. Moon/reload T3c5c3 and final field/memory/transfer
  T3c5c4 remain. Existing effective foliage scaling is not protected density.

## Production native stationary preflight — 2026-10-06

- T3c5c1 completes three alternating CPU/resident-compute pairs on verified
  Quadro GLX at shipping 1280x720, 100k Earth triangles and a 2M foliage budget.
  Field/topology, root/camera and planning anchors match exactly; effective
  density matches at 47.149, below configured 120.72 under existing scaling.
- 1,440 measured frames have complete GPU spans; 1,955 excluded frames and
  3,494 ready GPU work rows remain archived. Primary observer overhead is
  included. CPU/compute native p95 ratios 0.9696/1.0260/1.0492 approach the 5%
  limit; this is preflight, not an accepted migration speedup.
- Sampled device used peaks are CPU 496.4 MiB and compute 521.9 MiB. No reload
  occurs; physical replacement peaks and rendered near-root coverage remain.
- Two scoped native CTest groups pass (63.63 s). Runtime source/application and
  other 41 executables remain unchanged; final source/input/42-binary hashes
  and complete raw receipts are retained in the journal cost/preflight study.
- Installed optional python3-matplotlib for the journal figure generator;
  its copyable install command is above the environmental notes in todo.md.
- Resume T3c5c2 actual 6/12 m/s routes, then T3c5c3 Moon/reload and T3c5c4
  canonical bulk field-time/transfer and cross-case gates. CPU stays default.
- Preserve the user's unstaged environmental footer in todo.md; its existing
  text is unchanged and must not be staged with agent TODO progress.

## Physical-memory diagnostic checkpoint — 2026-10-06

- T3c5b2c adds optional .memory.csv: one persistent UUID-verified NVML reader,
  context setup/shutdown NVX, query/observation ages, stale/unsupported/errors,
  logical overlap, partial CPU vector capacities and process RSS.
- A separate 64-event queue retains lifecycle metadata through latest-snapshot
  lock contention. Skipped snapshots and queue overflow remain counted; driver
  reads/filesystem work stay on the worker, with joining only at teardown.
- Eight controlled checks pass. Twelve scoped CTest groups have passing results:
  11 pass in the final runtime batch (429.91 s), and a corrected invalid-radius
  fixture gives a passing full renderer group (67.55 s). Runtime and other 41
  executable hashes are unchanged through that test-only correction.
- Three Quadro renderer cases, two native CPU/compute fixtures and four EGL
  traced/untraced pairs on Quadro/RTX pass. Native memory queries/waits/readbacks
  remain zero; lifecycle events join publication roots and images remain exact.
- Journal/PDF and frozen 42-executable evidence are in
  docs/journal/architecture/terrain-gpu/async/hardware/memory/.
  Twenty-two gallery and ten historical PNG hashes match; user footer preserved.
- Resume T3c5c matched production hardware costs. Physical samples are device-wide
  and can miss peaks; startup sampling is asynchronous, CPU vectors are partial,
  and CPU terrain remains the default.

## Complete publication lifecycle checkpoint — 2026-10-06

- T3c5b2b2b completes capture/synchronous CPU, legacy and resident reload,
  actual destination contact binding and CPU grass-only admission. Captures
  require a returned complete scene draw; cached presentation admits nothing.
  Reload roots/children arm after live exchange. Handoffs exclude previews and
  flight travel; grass-only attempts exclude initial/replacement mesh planning.
- Admission-scoped mode, origin and contact-binding offset extend the bounded
  64-record CSV. Stack scopes retain failure closure without successful latency.
  Off-live synchronous GPU work keeps child keys with an empty frame field;
  full grass timing identity survives unchanged plans and later draws.
- Capture replay can replace a committed child before drawing. That child ends
  replaced_before_draw and the root trace_incomplete after the scene draw;
  replacement terrain succeeds independently. Equivalent grass remains consumable.
- Ten relevant CTest groups pass on final frozen inputs (366.53 s), including
  23 controlled cases. Six Quadro OpenGL 4.3 checks pass. Both renderers verify
  nine capture reload roots/twelve children, 57 GPU joins/30 off-frame events,
  CPU grass success/failure, actual Moon handoff/exact fresh replay, and seven
  injected reload fence failures with recovery. Rejected legacy-key/generation
  checks are preserved; this is scoped correctness validation, not cost acceptance.
- Source/build-test fingerprints and all 41 executable hashes remain frozen;
  22 gallery and ten historical PNG hashes match. Journal/PDF, raw receipts,
  device identities and validation are in
  docs/journal/architecture/terrain-gpu/async/hardware/publication/lifecycle/.
- T3c5b2b2 and parent T3c5b2b are tested. Resume T3c5b2c device-verified physical
  memory sampling outside the critical loop, then matched hardware T3c5c.
  CPU terrain stays default. The user's environmental TODO suffix is preserved
  byte-for-byte and remains unstaged.

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

|F6 — Unify automatic foliage budgeting in the CPU and compute renderers.|t|Shared protected-near/fitted-tail allocation, GL 3.3 fallback, cached memory/timing signals, conservative admission, controller inheritance and locked replay now work in the supported CPU path as well as resident compute. Production CPU/compute/CPU-override PNGs and policy match exactly; walking/sprint coverage is exact in all three bands. All 15 matched-quality cost pairs pass the unchanged <=1.05 p95 gate (maximum 1.049497); live/staged/retiring overlap stays below 1 GiB. Core, allocation, contact/replay, fallback and reload checks pass. Report: docs/journal/architecture/terrain-gpu/grass-allocation/cpu-integration.md. Compute now defaults for fresh runs after F6 acceptance; CPU override/GL 3.3 fallback and historical replay compatibility retained. Raw data local, changes uncommitted.|
|F1 — Make builds and normal tests independent of historical benchmark archives.|t|Clean RelWithDebInfo configure/build and 34/34 headless core checks pass without historical archives. Bulk data stays local; 599 old tracked raw traces removed from Git. Ten optional validators report missing data with exit 2 and regeneration commands; original hash checks and corruption rejection preserved. Storage policy and commands: scripts/benchmarks/archives/README.md. F2–F5 unchanged.|
|F2 — Fix the compute terrain stationary full-render regression.|t|GPU distance-ordering of triangle references reuses existing scratch storage without CPU blade-queue reads or quality reduction. Three alternating Quadro production normal-wind pairs pass the 1.05 p95 gate at 1.009/1.021/1.013; fixed-wind pairs pass too. Exact production blades/depth/RGB and native contacts/replay preserved. Compact result and verification limits: docs/journal/architecture/terrain-gpu/async/hardware/cost/raster/order/correction.md. Raw runs stay local; CPU remains default. F3–F5 untouched.|
|F3 — Implement automatic foliage falloff while protecting near-camera density.|t|Resident GPU planner protects the configured quad-distance region, fits distant sigma to timing/memory budgets and reports infeasible near work. Admission includes live/staged/retiring/replacement generations; cached telemetry has conservative missing-data limits and hysteresis. Effective policy is locked in replay. Normal/tiny budgets, actual protected coverage, missing/stale telemetry, exact production/walking replay and native reload/fallback audits pass. Compact results: docs/journal/architecture/terrain-gpu/grass-allocation/adaptive-falloff.md. Raw runs remain local; CPU stays default. F4/F5 unchanged.|
|F4 — Keep atmospheric highlight/exposure reduction on the GPU.|t|Exact GL 4.3 histogram preserves weighted 5% exclusion, partial tiles, sparse stars and current-frame/cached/manual/reflection behaviour; GL 3.3 keeps the supported reference. At 1080p readback falls 259,200→8 bytes and focused saturated median 3.558→1.580 ms; small-view dispatch overhead is documented. Core, render/fallback, canonical scenario/replay and reload checks pass; twelve images/metrics per driver match baseline. Report: docs/journal/architecture/atmosphere/highlights.md. Raw runs local; F5 untouched.|
|F5 — Complete a bounded terrain acceptance run and document the supported defaults.|t|Bounded evaluation complete: 15 alternating production pairs, canonical 100k measurement and fresh-build core/compute/input/reload/fallback/replay checks. Transfer drops 76.66%; bulk CPU field evaluations fall to zero; bounded replacement/contact audits pass. Rendered foliage/quality differs, so matched-quality full-frame speed acceptance fails. CPU remains supported/default and compute opt-in experimental; speed goal is not claimed complete. One compact report: docs/journal/architecture/terrain-gpu/acceptance.md. Raw receipts stay local; no additional task chain.|
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

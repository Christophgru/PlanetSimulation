# G3: shared grass wind and trail samples — 2026-10-10

## Implementation

View-specific compaction keeps its existing visibility, density and LOD policy.
A following GPU pass evaluates Perlin wind and the trail hierarchy once for each
visible root, caching three wind samples, trail direction/strength and an input
stamp. Later views reuse matching stamps; reflection-only blades are evaluated
on their first visible view. The vertex shader fetches the cached trail sample
instead of traversing the hierarchy for every vertex. Shape bending and normal
calculation retain their vertex-dependent height/LOD formulas.

The stamp changes with wind phase, trail-history revision or root replacement.
Identical animation inputs can also reuse samples across paused frames. Seed,
terrain, plan, disable and reload paths retain G2's invalidation and ownership.
Caches are GPU-only and plan-owned; no global accumulating storage, per-frame
bulk readback or CPU wait was added. CPU-planned compute grass shares animation
too; GL 3.3 retains the original vertex wind/trail path. The separate trail-fade
and G4 delta-streaming tasks remain queued; flattening, history eviction, wind
phase and replay state are unchanged here.

The additional cache costs **32 bytes/candidate**, plus 16 bytes of bounded
validation-counter storage per plan. Counter increments run only in transform
feedback diagnostics, never in ordinary native rendering. Admission includes
cache bytes across live/staged/retiring plans; cache allocation and shader
creation occur before resident publication. Buffer/texture limits are checked,
and GL state is restored during preparation. Three profiler stages distinguish
`grass_placement` (visibility/compaction), `grass_animation` (shared samples and
copies) and `grass_draw` (the two indirect geometry draws).

Animation uses a direct launch bounded by admitted candidate capacity. GPU draw
counts reject unused threads before accessing roots or samples. The indirect
pilot did not pass cache-miss assertions in this context; the direct reference
passes the same checks. This choice avoids count readback and dispatch-buffer
aliases, while launching some empty threads. Raw failed pilot evidence remains
local, and no unmeasured claim about its root cause is made.

## Measurement

The frozen baseline is G2 commit `6e57072`, RelWithDebInfo (`-O2`). Both builds use
NASA ACES, identical shaders/settings apart from G3, and the user's current
40.72/m² density, 0.13 m width, 15 m quad distance, 150 m draw distance and
2,000,000 candidate cap. Adaptive budgets and live wind remain enabled.

Three alternating before/after pairs cover idle and real 6 m/s W-input walking
at 1280×720 in the **astronaut camera**. Walking grows real persistent trails;
this differs from G2's surface-camera route. Each leg settles terrain and warms
at least three seconds/30 frames before measuring six seconds/100 frames.
Startup/warmup/drain are excluded, all measured outliers remain, and independent
wall-time movement and trail-growth checks guard the workload. Camera-only
replay pauses orbital motion, with live character/wind clocks.

The runner verifies RTX 3070 Ti UUID
`GPU-1795f01b-f1fd-61a0-3b79-199560dd0793`, complete GPU samples/frame joins,
protected density and native nonblocking audits. Real GLFW/X11 input runs with
NVIDIA EGL pbuffer rendering; this does **not** qualify desktop presentation or
sustained desktop FPS. Unrelated RTX utilization is present and retained in
telemetry. CPU submission, GPU work and enclosing wall spans overlap; nested
work must not be added to its enclosing geometry pass.

## Results

Milliseconds, pooled samples. G2 placement includes wind; G3 splits compaction
and animation. Their disjoint GPU scopes can be combined for that comparison.
Draw time is already included in the opaque/reflection pass and must not be
added to it.

| Metric | G2 | G3 |
|:--|--:|--:|
| Idle main placement GPU mean | 0.623 | 0.522 |
| Idle reflection placement GPU mean | 0.623 | 0.509 |
| Idle main / reflection animation GPU mean | unavailable | 0.108 / 0.068 |
| Idle placement + animation, both views | 1.246 | 1.206 |
| Walking main placement GPU mean | 0.640 | 0.523 |
| Walking reflection placement GPU mean | 0.619 | 0.503 |
| Walking main / reflection animation GPU mean | unavailable | 0.114 / 0.066 |
| Walking placement + animation, both views | 1.259 | 1.205 |
| Walking main / reflection grass draw GPU mean | unavailable | 0.810 / 0.399 |
| Walking enclosing opaque GPU mean | 1.923 | 1.923 |
| Walking enclosing reflection GPU mean | 1.304 | 1.212 |
| Idle wall p95 | 8.624 | 8.990 |
| Walking wall p95 / p99 | 10.702 / 12.154 | 10.993 / 18.084 |
| Walking maximum wall frame | 107.893 | 109.206 |
| Walking root preparation CPU mean | 3.605 | 15.824 |
| Walking allocation GPU mean | 14.958 | 15.312 |
| Peak logical admitted overlap bytes | 852,997,736 | 980,997,768 |

The measurements contain 2,576/2,559 idle and 2,020/1,999 walking frames.
There are nine measured root preparations in each walking variant and none
during idle. Trail histories grow from zero to 156–159 segments, and wall-time
walking speeds measure 5.946–5.957 m/s. This does not measure a full 2,048-segment
history. Candidate counts and protected policy remain equivalent; naturally
different frame schedules produce slightly different final walking anchors.

Combined compaction/animation GPU means fall **3.2% idle and 4.3% walking**.
Walking reflection GPU mean falls 7.1%; the enclosing opaque mean is unchanged.
G2 has no exclusive grass-draw timer, so the G3 draw cost cannot establish a
draw-only speedup. Animation reuse is established independently by cache-miss
tests, including zero repeat evaluations and reflection-only evaluations.

**No overall FPS improvement is demonstrated.** Wall p95 rises 4.2% idle and
2.7% walking, and root CPU submission outliers remain. Extra G3 profiler queries,
new cache traffic/allocation, asynchronous refresh scheduling and unrelated GPU
load affect these observations; their individual effects are not isolated.
The root timer measures CPU submission, including possible driver delays,
rather than CPU evaluation of blade positions. Retained RTX utilization spans
22–91%, with approximately 43% external utilization before the run; reported
device memory spans 211–1,040 MiB. These physical totals include other processes
and are distinct from logical admission. Delta streaming remains G4's scope.

At startup, 1,599,989 candidates use **51,199,648 animation bytes**. The added
command footer costs 16 bytes. Admission conservatively reserves 64,000,016
extra bytes per plan at the 2,000,000 cap, or 128,000,032 across two plans.
Peak admitted overlap remains below the unchanged 1 GiB limit. Protected density
stays 40.72/m² without near-region infeasibility, and fresh adaptive startup
matches G2's policy and image. Raw timing/capture/telemetry data stays local in
ignored `build-f5/g3`; no compressed benchmark archives are added to Git.

## Correctness and reproduction

RTX validates the 32-byte animation layout with field offsets 0/12/16. New
transform-feedback checks verify one evaluation for each first-visible blade,
reuse across separate views, reflection-only blades after an empty main view,
time/trail/root invalidation and release when compute placement is disabled.
All 19 software grass tests and all 19 RTX staging/publication tests pass.
Eighteen of 19 RTX grass checks pass; the unchanged strict trail precision
assertion also fails with the frozen G2 binary/shaders. The same assertion passes
on software GL. It was not weakened or treated as a new passing GPU check.
Eleven CPU/configuration/planning/layout/archive/timing/memory CTest groups pass.
All six software lifecycle, recovery, frame, reload, shadow and compute capture
groups pass (503.63 s), including standing/walking replay, CPU override and
GL 3.3 fallback.

Startup, NASA ACES, the user's mountain location and a walking-trail replay are
**byte-identical PNGs** against G2, with identical candidate counts and policy.
A fifth fresh adaptive startup image is also exact. The native Quadro held-worker
test passes compute sprint at 11.945 m/s (maximum anchor offset 11.45 m), the
80 m/s camera route (12.41 m), and CPU sprint (22.46 m), all within the unchanged
22.5 m reserve. Stop/reverse maxima remain within that margin. Actual near-root
snapshots are retained as coverage receipts, without claiming matched route
pixels. Native audits contain no blocking polls, finishes, server waits,
interactive bulk readbacks or synchronous VRAM queries.

The [compact summary](summary.json) retains pooled/per-pair measurements, raw
hashes and validation receipts; [inputs.json](inputs.json) freezes the scenario
and camera override. Use the [existing runner](../../../../scripts/benchmarks/grass_allocation/README.md)
with a frozen G2 probe/shader root and `--camera-mode astronaut --stages
grass_metadata grass_allocation grass_roots grass_placement grass_animation
grass_draw`. Failed indirect-pilot evidence also remains local.

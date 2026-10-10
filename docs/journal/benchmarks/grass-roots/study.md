# G2: persistent GPU grass roots — 2026-10-10

## Implementation

Each published grass plan owns immutable GPU candidate records: unoffset root,
expected density, radial up, density rank, terrain normal, validity/slot and
random shape/LOD variation. A separate small patch-bound buffer rejects
invisible terrain patches before reading their roots. The GPU generates these
records once during preparation, before the resource fence permits publication.
Dynamic per-view visibility, Gaussian density, LOD, compaction and wind retain
their existing behavior. Reflection views select their own visible candidates.

The added storage is **64 bytes/candidate + 16 bytes/selected triangle**.
Admission reserves these bytes for every live/staged/retiring plan, alongside
the existing two 64-byte output queues. It conservatively charges bounds for
all source triangles and candidate storage for the admitted capacity. Adaptive
falloff can therefore lower distant density when the complete overlap cannot
fit; protected density and infeasibility reporting keep their existing policy.
The hardware SSBO limit still applies independently to the output queues.

Terrain generations, source buffers, selected candidates, seed and static
configuration changes invalidate the cache. Small camera movement, different
view matrices and advancing wind do not rewrite roots. Nonadaptive quad-distance
changes retain their existing geometry-only behavior. CPU-planned grass on GL
4.3 generates its cache on the first draw; GL 3.3 uses the original vertex path.
Reload, disable and retirement release plan-owned buffers. No global cache,
interactive bulk readback or wait was introduced. Diagnostic root readback is
used only by tests.

The new `grass_roots` profiler stage measures preparation separately from
`grass_placement`, which now measures dynamic visibility/LOD/compaction and
wind. G3 animation sharing and G4 overlap retention remain queued: whole-plan
refreshes still regenerate roots, and wind/trails still run for each view.

## Measurement

Paired native RTX runs use the same method and current production configuration
as the [G1 study](../grass-allocation/study.md), with G1 commit `7b14a49` as the
baseline. Both variants retain 40.72 blades/m², 0.13 m width, 15 m quad distance,
150 m draw distance, the 2,000,000 hard candidate cap and the NASA ACES asset.
Three alternating before/after pairs cover idle and real 6 m/s W-input walking
at 1280×720, with live wind, adaptive budgets and a paused orbital replay.
Startup and at least three seconds/30 frames of warmup are excluded; all
measured outliers remain. GPU event costs, CPU submission and wall spans overlap
and must not be added.

Each run verifies RTX 3070 Ti UUID
`GPU-1795f01b-f1fd-61a0-3b79-199560dd0793`, complete samples/frame joins,
protected density and nonblocking native audits. Rendering uses a NVIDIA EGL
pbuffer with real GLFW/X11 input. These measurements do **not** qualify desktop
presentation or sustained desktop FPS. The RTX has unrelated external load;
retained telemetry includes that contention. Raw evidence stays local in ignored
`build-f5/g2`; normal tests do not require it.

## Results

Milliseconds, pooled samples; CPU and GPU costs overlap.

| Metric | G1 | G2 |
|:--|--:|--:|
| Idle main placement/wind GPU mean | 0.551 | 0.610 |
| Idle reflection placement/wind GPU mean | 0.562 | 0.619 |
| Walking main placement/wind GPU mean | 0.575 | 0.584 |
| Walking reflection placement/wind GPU mean | 0.557 | 0.564 |
| Walking main placement CPU submission mean | 0.190 | 0.171 |
| Walking reflection placement CPU submission mean | 0.134 | 0.160 |
| Root preparation GPU mean / p95 | — | 0.892 / 1.070 |
| Root preparation CPU mean / median / max | — | 12.347 / 0.770 / 30.559 |
| Idle wall p95 | 9.367 | 9.385 |
| Walking wall p95 | 9.220 | 9.285 |
| Walking maximum wall frame | 112.714 | 116.405 |
| Conservative idle logical bytes | 296,979,036 | 426,579,036 |
| Maximum walking logical overlap bytes | 593,797,736 | 852,997,736 |

There are 2,308/2,289 idle and 2,392/2,375 walking frames. Nine changed walking
plans generate roots; **no root events occur during measured idle frames**.
Each view still runs placement/animation every rendered frame. Four of nine
root CPU submissions take 23.9–30.6 ms; these outliers are retained. Preparation
GPU time alone does not explain CPU submission stalls or whole-frame hitches.
Total retained RTX utilization spans 22–92%, including unrelated external load
of approximately 65% before our runs.

**This pass does not demonstrate an FPS improvement.** Per-view GPU means rise
about 10–11% when idle and 1–2% when walking. Enclosing wall p95 changes less
than 1%; streaming maxima remain above the 16.67 ms target. Persistent data adds
memory traffic as well as removing geometric/random/biome recomputation; these
receipts do not isolate the hardware bottleneck. The cache provides the fixed
root data required by queued G3/G4, rather than completing shared animation or
delta streaming in this task.

Startup retains 1,599,989 candidates and 39,761 selected triangles: actual root
and bound buffers total **103,035,472 bytes**. The conservative admission delta
is 129,600,000 bytes per plan (2,000,000 × 64 + 100,000 × 16), or 259,200,000
bytes for the observed two-plan walking overlap. Peak logical overlap remains
below the unchanged 1 GiB cap. This conservative receipt differs from actual
buffer bytes and physical VRAM telemetry; neither should be labeled the other.
Protected density stays 40.72/m² with no near-region infeasibility in every
measured frame. Candidate counts and effective policy remain unchanged.

## Correctness and reproduction

NVIDIA verifies the root record's 64-byte std430 stride and field offsets
0/16/32/48. RTX tests verify immutable records across wind, small camera movement
and reflection views, seed/terrain invalidation, compacted root/wind parity
against vertex grass at both poles, protected density and frustum reach.
All 19 staging/publication checks pass, including complete memory admission,
pre-publication readiness, failure recovery, retirement and GL-state restoration.

Three production fixed views—startup, NASA ACES and the reported mountain—are
**byte-identical PNGs** against G1's saved replay, with identical candidates and
policy. A fourth fresh adaptive startup image also matches G1 exactly. Replay
round trips remain exact, including compute standing/walking, main/reflection,
centimeter detail, CPU override and GL 3.3 fallback capture checks.

The strict existing trail test passes on software GL. On NVIDIA it reports tiny
wind-dependent position differences with both the changed shader and G1's
original placement shader. The isolated baseline check bypasses root generation
and dispatches G1's original per-frame placement; it reproduces the failure.
This hardware limitation is retained in local logs; the assertion is unchanged.
The other 16 RTX grass rendering checks pass. Eleven CPU/configuration/planning,
layout/archive, timing and memory CTest groups pass. All six software runtime,
lifecycle, recovery, reload, main/reflection grass and compute capture groups
pass (384.05 s); final repository layout/archive checks also pass.

The unchanged native Quadro held-worker test passes: compute sprint reaches
90 m at 11.94 m/s, with maximum anchor offset 11.57 m; the 80 m/s camera route
reaches 90 m with maximum offset 12.48 m. Stop/reverse maxima are 11.35/12.19 m,
inside the unchanged 22.5 m reserve. CPU sprint also passes. The diagnostic
snapshot contains 72/629 compute roots in the 0–5/5–15 m bands at configured
1/m²; the CPU route contains 74/631. Routes differ slightly, so those are
coverage receipts rather than a pixel-matched comparison. No blocking fence
polls, server waits, finishes, interactive bulk reads or synchronous VRAM queries
occur in the measured native runs.

The [compact summary](summary.json) retains pooled/per-pair timings, workloads,
hashes, memory and visual/refresh receipts. [inputs.json](inputs.json) freezes the
configuration and benchmark camera override. Reproduce using the existing
[paired runner](../../../../scripts/benchmarks/grass_allocation/README.md) with
`--stages grass_metadata grass_allocation grass_roots grass_placement` and a
frozen G1 probe/shader root. Raw traces, images and telemetry remain local.

# F3: protected near density and automatic distant falloff

Implemented 2026-10-08 for the opt-in resident `compute` / `gpu-v1` planner.
CPU remains the supported default. F4/F5 and unrelated requested features are
unchanged. Raw receipts are local under ignored
`build-benchmarks/runs/f3-20261008/`.

## Implementation

The existing metadata/allocation shaders now support a protected radius equal
to `quadDistanceMeters()`. Metadata retains raw triangle area in this mode.
The GPU reserves conservative near work (triangle reach plus walking margin)
at configured density, then uses the existing 32-step search to fit the distant
Gaussian width. Width is bounded by `draw_distance_m * gaussian_sigma_fraction`.
The tail is continuous at the protected radius. Placement uses the same profile;
LOD/outer retention fades start outside the protected region. Fractional last
blades carry fractional coverage instead of attenuating full near blades.

Configured near density outranks the soft render allowance when hard capacity
permits it. If conservative near work or a per-triangle slot cap prevents full
near density, the summary reports `near_infeasible`; near patches take priority
over distant patches and submitted work stays bounded. Reports print on entry
or capacity change, rather than on every moving plan.

Before dispatch, publication fits capacity to remaining whole-generation memory,
including terrain, metadata, planner scratch, both blade queues and commands.
Live, pending, retiring and external old-scene reservations all remain charged
through the existing transaction/retirement ledger. Fresh cached physical memory
allows half the free bytes, less reservations added after that sample. Missing
or >2-second-old telemetry uses a 64 MiB stage / 256 MiB aggregate ceiling.
Logical limits, the GL block limit and the optional memory cap further constrain
admission. Zero free memory rejects a new stage and preserves live consumers.

The existing sampler also runs without a trace destination; it writes no files
in that mode. Reads and driver work remain on its worker. Frame query results
are collected only after availability and are enabled for resident policy with
the overlay hidden. Three distinct samples above 41.667 ms (and >=90% utilization
when available) reduce the soft quota by 20%; three below 25 ms recover 10%.
A two-second cooldown and 25%/16 MiB memory hysteresis bound stationary replans.
Missing timing/utilization does not invent load. No new quality framework,
bulk readback, queue storage or allocation dispatch phases were introduced.

Version-1 `render.foliage_policy` stores each body's effective capacity, budget,
density, protected radius, sigma and near-deficit flag. Replay validates and
locks these values, including after reload. It rejects insufficient admission
rather than reducing saved quality. Older/camera-only replays use live policy;
an explicit CPU override retains legacy behavior.

## Focused results

Quadro M1000M, 2 GiB, NVIDIA 580.178.04; production initial camera, 480×270.

|Check|Result|
|:--|:--|
|Production requested/effective near density|120.72 / 120.72 blades/m²; no deficit|
|Protected radius / effective sigma|15 m / 8.435062249191105 m|
|Candidate capacity / soft budget / submitted|2,000,000 / 1,600,000 / 1,599,906|
|Main-view drawn blades|140,857|
|Logical resident reservation|290,578,652 bytes|
|Production replay|Exact PNG bytes and effective policy|
|Normal synthetic GPU policy|8 blades/m² preserved; sigma 3.15701 → 1.08101 m when budget changes 80 → 40|
|Near-only soft allowance|Raised from 1 to 24 slots to protect the three near triangles|
|Tiny capacities 1/2/3/10 and per-triangle cap|Deficit reported; no far displacement or capacity overflow; exact locked references/counts|
|Protected render coverage|4 blades/m² over 525 m²; weighted coverage within 0.01 of 2,100; no near LOD/outer thinning|

Verification on the final source:

- 34/34 headless core groups pass, including policy hysteresis/replay and the
  untraced/nonblocking sampler cache.
- 34/34 NVIDIA compute integration cases pass (46.28 s). Seven allocation and
  admission cases also pass on Mesa OpenGL 4.3.
- 14/14 foliage render/compaction cases pass on Mesa OpenGL 4.3. The protected
  coverage case explicitly skips on the NVIDIA test executable's GL 3.3 context;
  its allocation tests and shipping captures use real NVIDIA GL 4.3 compute.
- Capture checks pass exact surface/walking/tiny-budget policy replay, explicit
  CPU override, invalid policy rejection, legacy terrain parity and GL 3.3
  fallback/locked-unavailable rejection. Adaptive foliage intentionally changes
  quality; legacy-planner captures retain the CPU/GPU terrain pixel comparison.
- 13/13 renderer frame/memory and 4/4 focused reload cases pass, including untraced resident sampling,
  delayed GPU completion, reload overlap/failure/supersession and retirement.
- Native input checks pass loading, 6/12 m/s movement, jump/directional thrust,
  trails, reload/watch/supersession, space/Moon replay handoff, delayed clock,
  default CPU, GL 3.3 fallback and startup rejection contracts. Runtime audits
  record zero blocking polls, bulk reads, server waits, finishes and GL memory
  queries. Noisy-fixture animation speeds were 5.978 / 11.955 m/s.

This verifies F3 behavior and bounded work, not combined performance acceptance.
F2's old coverage/performance numbers remain historical. F5 must assess matched
quality with the combined implementation; no new frame-time speed claim or
backend default switch is made here. The pre-existing NVIDIA GL 3.3 trail
exact-equality failure recorded in F2 was not changed or waived.

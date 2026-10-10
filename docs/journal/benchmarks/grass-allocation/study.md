# G1: bounded grass preparation reuse — 2026-10-10

Grass allocation now costs **20.6% less GPU time and 52.9% less CPU submission
time** in the paired RTX walking runs. Camera refreshes can share immutable
terrain metadata; equivalent effective policies keep their existing plan.
Protected density and deterministic images are unchanged. Frame p95 changes
little, and approximately 109 ms maximum frames remain: this pass improves
grass preparation without resolving every native streaming hitch.

## Implementation

- Each terrain generation can own a shared GPU buffer containing triangle
  center, reach, physical area and static biome eligibility. Camera distance,
  weighting and policy parameters remain private to each plan. Reuse requires
  matching source VBO/EBO, generation, triangle count, physical scale and biome
  inputs. Live/staged/retiring plans own the cache; no accumulating global cache
  or CPU geometry readback was added.
- A telemetry revision reuses the live plan only when the effective capacity,
  budget, configuration and metadata parameters still match, and the existing
  anchor still covers the predicted path. Budget reductions, terrain/config
  changes, camera refreshes and explicit locked replay anchors retain their
  preparation/publication path. Admission failure retains the live plan.
- Hash selection skips its 32 search iterations when triangle count proves
  the candidate budget sufficient. Locked replay skips its inactive density
  search. Adaptive allocation avoids Gaussian calculations for ineligible
  triangles and returns early when the full requested tail already fits.
  Active searches retain 32 iterations, the original slot rounding, stable
  hash selection and near-first reference sort. No sort precision was reduced.
- Added geometry storage is **64 bytes/triangle**: 6.4 MB at the unchanged
  100,000-triangle Earth cap. Admission conservatively charges that shared
  storage per plan. Steady logical residency rises from 290,579,036 to
  296,979,036 bytes; maximum walking overlap rises from 580,997,736 to
  593,797,736 bytes, below the existing 1 GiB total cap. A fresh metadata pass
  does more classification work; its measured mean rises 16.8% (0.337→0.393 ms).

## Measurement

The [compact summary](summary.json) includes pooled and per-pair results,
workload snapshots, binary/shader/configuration/asset hashes and local evidence
hashes. [inputs.json](inputs.json) freezes the production configuration and
benchmark camera override. Raw traces, images, logs and GPU telemetry stay in
ignored `build-f5/g1`; optional historical data is not required by normal tests.

The baseline is commit `479bc63`, built RelWithDebInfo (`-O2`) before this
change. Both variants use the same NASA ACES asset and the user's current
production grass values: **40.72 blades/m², 0.13 m width, 15 m quad distance,
150 m draw distance and 2,000,000 hard candidate cap**. Terrain refinement and
render quality are unchanged. Adaptive falloff remains active; every measured
frame reports configured protected density with no near-region infeasibility.

Twelve native-loop runs cover three alternating before/after pairs of idle and
6 m/s walking. Each run settles the surface camera, warms for at least three
seconds/30 frames, then measures at least six seconds/100 frames at 1280×720.
The camera-only replay pauses orbital motion; wind follows real elapsed time.
Walking uses real W input and wall-time movement. Retained event counts vary
with route/publication timing (nine baseline allocations, ten changed), so this
is a paired route study rather than an identical-event microbenchmark.

Each run verifies RTX 3070 Ti UUID
`GPU-1795f01b-f1fd-61a0-3b79-199560dd0793`, NVIDIA 580.178.04, complete GPU
samples, frame joins and zero cached-scene renders. Actual GLFW/X11 input and
the production loop run through a private NVIDIA EGL pbuffer adapter; there is
**no desktop presentation or sustained desktop FPS claim**. Probe trace output
is outside the renderer's measured loop spans. Native audits retain zero
blocking fence polls, server waits, bulk reads, finishes and synchronous VRAM
queries. All measured outliers remain; percentiles use nearest rank on pooled
samples. CPU submission, GPU work and enclosing wall spans overlap and must
not be added.

The RTX had approximately 40–48% unrelated utilization before our processes.
The retained 500 ms telemetry spans 12–93% total utilization during the study.
Alternating runs reduce order bias but do not remove contention. These results
cannot be directly compared with the earlier idle-device
[feature-cost study](../feature-costs/study.md), whose roughly 84–127 ms
allocation bursts used synchronous captures and different GPU operating/load
conditions. CPU topology/refinement and per-frame grass placement/wind remain
outside G1's changes.

## Results

Milliseconds; allocation and metadata rows contain preparation events, while
frame rows contain all retained measured frames.

| Metric | Before | After |
|:--|--:|--:|
| Walking allocation GPU mean | 18.375 | 14.583 |
| Walking allocation GPU p95/max | 19.571 | 15.887 |
| Walking allocation CPU submission mean | 1.318 | 0.621 |
| Repeated-generation metadata GPU mean | 0.381 | 0.163 |
| Repeated-generation metadata GPU median | 0.345 | 0.075 |
| Fresh-generation metadata GPU mean | 0.337 | 0.393 |
| Idle wall p95 | 9.298 | 9.499 |
| Walking wall p95 | 9.236 | 9.261 |
| Walking wall p99 | 12.075 | 10.669 |
| Walking maximum wall frame | 108.626 | 109.073 |

There are 2,321/2,294 retained idle frames and 2,399/2,398 walking frames.
Repeated-generation metadata uses six/seven events, including the changed
variant's retained 0.716 ms outlier; fresh metadata uses three events per
variant. Allocation means improve in every pair (17.3%, 21.0%, 23.2%). Steady
frame timing is effectively unchanged under this load. Persistent roots,
shared animation and delta cell streaming remain the queued G2–G4 scope.

## Correctness and reproduction

On verified RTX, all 11 metadata/allocation tests and 12 publication tests pass.
Independent CPU oracles cover Gaussian weights, eligibility, slot boundaries,
hash selection, protected density and infeasible budgets. New checks cover
immutable cache sharing, changed-input invalidation, bounded ownership and
equivalent versus reduced policy reuse. Three production fixed views—startup,
NASA ACES and the reported mountain—are **byte-identical** before/after using
the baseline's saved replay, including identical candidates and policy.
A separate fresh adaptive startup capture also matches exactly. Startup
allocation submissions fall from 239 to 143; locked replay uses 47.

Nine CPU planning/configuration/archive/layout CTest groups and six software
runtime, reload, recovery, main/reflection grass and compute capture groups pass.
Capture coverage includes CPU/GL 3.3 and exact locked replay. The older grass
capture fixture assumed CPU upload counters while using the newer compute
default; it now explicitly selects its intended CPU planner and passes all
visibility, upload, paused reuse and replay assertions.

The unchanged held-worker refresh test passes on native Quadro M1000M GLX:
compute sprint and 80 m/s camera movement reach 90 m while keeping maximum
anchor offsets at 11.46/12.04 m, within the unchanged 22.5 m reserve. Stop/reverse
maximums are 19.99/11.31 m. CPU sprint also passes. Independent GPU root counts
in the 0–5/5–15 m bands are 72/633 at configured 1/m² (CPU 72/632). Native
nonblocking audits pass. The software sprint attempt fails its speed/headroom
gate, reaching 29.98 m offset; its evidence is retained, and this pass does not
qualify fast interactive streaming on software rendering. The margin assertion
was neither relaxed nor removed.

A final compile-time restriction prevents copying the geometry owner's buffer
handle; rebuilt cache/policy tests pass. The summary distinguishes measured
binaries/source hashes from the final build and fixture correction.

See the [runner instructions](../../../../scripts/benchmarks/grass_allocation/README.md)
to reproduce paired runs and independent aggregation. Use a frozen baseline
probe plus its shaders/assets and keep all regenerated raw data local.

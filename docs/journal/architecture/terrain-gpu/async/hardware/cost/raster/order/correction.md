# F2 — stationary compute rendering correction

2026-10-08, Quadro M1000M, driver 580.178.04. Graphics access is restored:
NVIDIA device opens and a native NVIDIA GLX context succeed. The previous
device-denial results remain historical evidence in [study.md](study.md).
CPU remains the supported default; F3–F5 and migration acceptance remain open.

The existing production order probe identifies the main detailed-grass draw
as order-sensitive. At fixed wind 12 s, its median GPU draw time is 4.343 ms
with CPU planning and 8.014 ms with resident planning. Exact near-first blade
permutations bring both to 3.434–3.435 ms; far-first takes 11.116–11.127 ms.
All permutations preserve complete blade contents, commands, matrices, opaque
depth and displayed RGB. These blocking controls diagnose ordering; their
timings are excluded from full-frame acceptance. Passing samples are not a
fragment-invocation or standalone overdraw measurement.

The runtime correction distance-orders GPU triangle references within each
candidate-slot level when a plan is built, matching CPU near-first submission.
A parallel merge uses the existing reference/rank buffers, double distance
keys and triangle-ID tie breaks. It adds no allocation or CPU blade-queue
readback. Roots, seeds, density, budgets, geometry selection, placement and
wind math are unchanged. Main/reflection atomic compaction stays on the GPU.
The corrected native compute detailed draw is 4.452 ms in the same private
control. This is approximate near-first submission, not an exact per-view
blade-depth sort.

Three alternating CPU/compute full-draw pairs use the unchanged production
scene, initial camera, 1280×720 viewport, 100,000 Earth triangles and 2,000,000
candidate budget. Each run retains 240 measured frames after at least 30
grounded warmup frames and three seconds. All outliers and startup/close
observations stay local. Complete native GLFW wall time includes presentation
and observer overhead. No blocking order controls run during timing.

| Pair | Baseline fixed-wind ratio | Corrected fixed-wind ratio | Normal-wind CPU p95 ms | Normal-wind compute p95 ms | Normal-wind ratio |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1 | 1.099687 | 1.023953 | 75.101 | 75.754 | 1.008686 |
| 2 | 1.062302 | 1.018824 | 75.383 | 77.002 | 1.021469 |
| 3 | 1.089736 | 1.012084 | 75.105 | 76.056 | 1.012668 |

All corrected pairs pass the original compute/CPU p95 ≤1.05 stationary gate.
The baseline failures remain recorded. This closes the F2 stationary cost
gate, not the combined movement/Moon/reload/transfer acceptance in F5.
Sorting adds plan-build work: initial allocation GPU medians are 20.318 ms
before and 25.532 ms after in the fixed-wind runs, with individual startup
samples up to 57.217/72.841 ms. These are initial-plan observations, not a
movement-cost acceptance result. Logical compute overlap remains 290,578,652
bytes in every timed run.

Before/after production captures preserve exact 64-byte blade multisets,
indirect commands, matrices, opaque depth and displayed RGB on both backends.
Main detailed/quad counts are 35,327/153,563; reflection counts are
35,019/165,221. Runtime audits find zero bulk GPU reads, blocking polls,
server waits, explicit finishes or render-thread memory queries. Fixed-state
inspection and functional tests establish preservation; stationary timings
alone do not establish general moving-camera coverage.

Verification passes 31 compute integration cases (CTest repeat: 40.80 s),
the four allocation-oracle cases on NVIDIA and software GL, 13 software grass
render cases, and the existing scene-replay/CPU-plan/placement groups. Native
input checks pass walking/sprint, jump/thrust/trails, reload/watch/supersession,
space/Moon contacts and replay, delayed clocks, default CPU, GL 3.3 fallback
and four startup-rejection contracts. The first cold compute CTest exceeded
its unchanged 120 s limit; the complete binary then passed in 44.96 s and
CTest passed in 40.80 s. Those earlier results remain local.

One pre-existing NVIDIA GL 3.3 trail check still fails exact float equality at
`test_grass_render.cpp:393`. It also fails in the unchanged F1 executable/source
snapshot; its CPU-planned fixture never runs the new resident allocation sort.
The same grass suite passes on software GL. No trail code or test tolerance
was changed, and no all-tests-passing claim is made.

Compact data and source/executable fingerprints: [correction.json](correction.json).
Raw outputs, failures and lossless storage receipts stay ignored under
`build-benchmarks/runs/f2-20261008/`. The baseline used the unchanged F1 source
snapshot; corrected runs freeze the working-tree source and probe hashes.
The existing order and stationary runners are sufficient to reproduce this
result; no new benchmark framework or TODO chain was added.

```sh
__NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia xvfb-run -a \
  python3 -B scripts/benchmarks/terrain_cost/raster/run.py \
  --probe build-resume/tests/terrain_raster_probe \
  --output-dir build-benchmarks/runs/f2-repeat-native \
  --expected-uuid GPU-2cefee61-6b3b-a670-c390-c7449dad3f79 \
  --wind-mode native --raster-mode full
```

Use a fresh output directory. For matched-wind diagnosis use `--wind-mode fixed`.

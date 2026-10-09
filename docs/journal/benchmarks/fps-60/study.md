# CPU/GPU timing and 60 FPS pass — 2026-10-09

At unchanged production quality, warm offscreen rendering fits 16.67 ms on the RTX 3070 Ti. The Quadro M1000M exceeds that budget. Rebuild/reload spikes still prevent a general sustained-60-FPS claim. This bounded pass adds `--uncapped` and improves CPU ridge planning; it does not change terrain accuracy, foliage settings, shaders or visual features.

## Measurement

- Fixed 1280×720, standard production config from `e769028`, Earth 100k triangle cap, relief sinking and `geometric_error_m=0.05`, configured density 120.72/m², protected radius 15 m and cutoff 150 m. The standard startup camera is −17.2307275444°, 79.3919745759°, 2 m clearance, time 0. The reported mountain camera is −18.7260251555°, 113.2376248869°, 2 m clearance (126.914 m reference-sphere altitude), time 180.346673067.
- Existing `gpu_compare/EglCapture.cpp` adapter: verified device UUID, RGBA8/depth24/stencil8/4-sample pbuffer, OpenGL 4.3, NVIDIA 580.178.04; uncapped without monitor presentation. Quadro: `GPU-2cefee61-6b3b-a670-c390-c7449dad3f79`, 2 GiB. RTX: `GPU-1795f01b-f1fd-61a0-3b79-199560dd0793`, 8 GiB. RTX also carried other work; device telemetry is retained. CPU build is RelWithDebInfo (`-O2`), with no concurrent compilation/tests in final measurements.
- Three alternating before/after pairs per stationary view/backend: 120 frames each, first 20 and final readback frame excluded, 99 full renders each. All 12 image pairs match byte for byte, with identical triangles, foliage policy, candidate/draw counts and exposure. GPU/render medians differ by approximately −1% to +1%; no warm rendering speedup is claimed.
- Moving captures advance orbit/wind by 1/60 s and walk by 0.1 or 0.2 m per rendered frame (6/12 m/s of benchmark time). They synchronously complete rebuilds. Native runs use real W/Shift, wall-time movement, paused orbital replay, live wind, ≥3 s/30-frame warmup, 101 stationary full renders and ≥20 frames over 12 m for each movement leg. Native scale stays 1; cached frames are zero. These short routes are diagnostics, not endurance certification.
- Stationary captures lock a common saved production foliage policy. Moving inputs omit the render block so both backends use current adaptive allocation. Final policies report no near infeasibility and retain configured density. Automatic tail/budget differences remain in raw receipts; moving hardware/backend timings are not an exact workload speedup comparison. These scalar checks do not establish continuous grass coverage between publications.
- Percentiles use nearest rank, without removing measured outliers. CPU wall, thread CPU and asynchronous GPU scopes overlap; do not add them together.

## Confirmed CPU improvement

Ridge refinement now uses a reserved hash table instead of tree traversal for shared-edge lookup. Split/face order remains explicit; the table is never iterated to choose geometry. Capacity is bounded by the closed mesh’s 3/2 edges per face. All fixed-pose topology fingerprints, sample/query counts, residual errors and rendered images are unchanged.

Isolated planner runs record wall and Linux thread-CPU time. Each executable builds both fixed poses four times; run 0 is warmup. Baseline includes a later recheck (six warm samples); final implementation has three.

| Pose | Before wall median ms | After wall median ms | Change | Before/after thread CPU ms |
|:--|--:|--:|--:|--:|
| startup | 2558.51 | 2345.79 | -8.3% | 2552.80 / 2343.14 |
| mountain | 2000.46 | 1535.01 | -23.3% | 1996.70 / 1533.09 |

Prototype tracing identified the affected refinement scope; shoreline refinement remains substantial. This reduces generation latency rather than the GPU cost of an already installed mesh. Display-shader specialization and redundant grass-vertex arithmetic experiments did not establish a convincing benefit and were removed.

## Final offscreen full-render timings

Cells are **median / p95 / p99 milliseconds**, 99 full frames per run. Moving values include synchronous capture rebuilds; their multi-second p99 values are real stalls. Startup/warmup is separate.

| View / motion | Quadro CPU | Quadro compute | RTX CPU | RTX compute |
|:--|--:|--:|--:|--:|
| startup / stationary | 46.59 / 47.20 / 47.36 | 45.65 / 46.39 / 46.52 | 4.41 / 6.66 / 7.08 | 4.46 / 6.15 / 7.73 |
| startup / walk | 81.17 / 136.84 / 3210.19 | 81.06 / 124.48 / 2993.31 | 6.38 / 8.80 / 3206.95 | 6.37 / 9.15 / 2895.00 |
| startup / sprint | 69.39 / 142.96 / 3313.57 | 71.79 / 125.28 / 3056.84 | 6.69 / 8.90 / 3276.77 | 6.66 / 9.70 / 2931.63 |
| mountain / stationary | 27.55 / 29.88 / 30.12 | 38.27 / 72.42 / 72.85 | 3.45 / 5.07 / 5.67 | 3.56 / 6.10 / 6.90 |
| mountain / walk | 28.57 / 31.25 / 2038.56 | 29.39 / 30.97 / 1937.59 | 3.72 / 5.61 / 2092.83 | 3.49 / 5.82 / 1865.34 |
| mountain / sprint | 26.41 / 78.71 / 2075.41 | 26.55 / 33.18 / 1992.77 | 3.83 / 5.70 / 2133.56 | 3.48 / 6.93 / 1838.89 |

Representative asynchronous **GPU medians**, compute backend, milliseconds. Opaque includes terrain/grass; reflection geometry and reflected atmosphere are separate. Frame span additionally exposes pipeline idle/submission gaps.

| GPU / stationary view | GPU total | Opaque | Reflection | Reflection atmosphere | Main atmosphere | GPU span |
|:--|--:|--:|--:|--:|--:|--:|
| Quadro / startup | 45.74 | 12.71 | 5.90 | 7.00 | 19.17 | 45.75 |
| Quadro / mountain | 38.36 | 16.01 | 8.40 | 7.19 | 5.28 | 38.37 |
| RTX / startup | 4.26 | 0.97 | 0.88 | 0.49 | 1.61 | 4.29 |
| RTX / mountain | 3.20 | 1.09 | 1.00 | 0.42 | 0.54 | 3.23 |

Across final Earth land generation events, GPU field/expansion and grass metadata/allocation medians are:

| GPU | Land field ms | Land expansion ms | Grass metadata ms | Grass allocation ms |
|:--|--:|--:|--:|--:|
| Quadro | 39.68 | 1.32 | 2.69 | 75.51 |
| RTX | 4.70 | 0.10 | 0.31 | 18.32 |

## Native wall time, presentation and memory

Quadro GLX/Xvfb, requested swap interval 0 through the shipping `--uncapped` option. Cells are **median / p95 / p99 milliseconds**, including observer and presentation overhead. Native results have a third-person view and wall-time wind, so they are a separate cohort from the fixed surface captures.

| View / backend | Stationary wall | Walking wall | Sprint wall | Walking / sprint m/s |
|:--|--:|--:|--:|--:|
| startup / compute | 45.27 / 50.19 / 50.70 | 53.40 / 73.90 / 97.09 | 82.31 / 88.09 / 222.66 | 5.910 / 11.927 |
| startup / cpu | 52.15 / 57.61 / 121.71 | 59.01 / 77.17 / 78.95 | 98.54 / 301.06 / 331.45 | 5.903 / 12.045 |
| mountain / compute | 34.02 / 34.95 / 35.73 | 34.62 / 35.46 / 35.55 | 36.34 / 46.00 / 226.92 | 5.988 / 11.975 |
| mountain / cpu | 34.54 / 35.45 / 36.13 | 45.66 / 75.92 / 79.22 | 44.98 / 70.39 / 176.40 | 5.989 / 11.900 |

Native stationary presentation medians are 3.12–3.42 ms; observer medians 0.643–0.838 ms. Event polling and all frame stages remain in CSV. A separate traced startup capture measures render-thread CPU medians 42.74 ms (compute) and 42.77 ms (CPU), versus wall medians 42.95/42.97 ms. These CPU clocks include NVIDIA driver work/spinning: atmosphere, reflection and grass draw submission dominate the traced render thread. They are not pure application-computation time or an additive GPU budget.

- Startup compute reload: request-to-publication/retirement 6294.1 ms; maximum measured reload-frame wall 218.5 ms. Compute retains interactive old-generation drawing during preparation; CPU reload stalls its loop. Both publish epoch 2 without reported failure.
- Startup cpu reload: request-to-publication/retirement 3673.3 ms; maximum measured reload-frame wall 3595.4 ms. Compute retains interactive old-generation drawing during preparation; CPU reload stalls its loop. Both publish epoch 2 without reported failure.

| Cohort | Peak sampled device-used MiB | Peak observed process RSS MiB | Peak logical reservation MiB |
|:--|--:|--:|--:|
| Quadro captures | 660.25 | 188.94 | 554.08 |
| RTX captures | 1161.19 | 200.68 | 277.12 |
| Quadro native CPU/compute | 543.25 | see per-run memory CSV | 831.35 |

Device memory includes other applications and periodic samples can miss peaks. Reservations are bounded ownership/admission accounting, not total physical VRAM or total process memory. Native peak stays below 1 GiB; worker/queue bounds, generation-consumer agreement and nonblocking GL publication audits pass.

## Limit and next existing task

On the Quadro, opaque grass/terrain, reflections and atmosphere already exceed the 16.67 ms budget at this quality. The measured limit is roughly 20–40 warm full FPS depending on view, with lower movement throughput and rebuild spikes. No quality-preserving 60 FPS claim is made for that adapter. The RTX passes the warm offscreen p95 budget in these samples, including moving captures, but multi-second synchronous capture rebuilds remain. RTX native streaming/presentation was not tested by the capture-only EGL adapter.

`--uncapped` removes the application’s display-refresh wait request and leaves elapsed-time physics intact. Maintaining quality on a faster GPU is supported by these offscreen results. Reducing scene resolution, atmosphere/refraction work, reflections, or grass detail would require explicit visual tradeoffs; none were applied. Existing interactive resolution adaptation remains unchanged and was held at scale 1 in native receipts.

The next existing grass task must address camera-relative coverage and publication latency. A locked saved foliage policy can fail when movement rebuilds a terrain patch whose required slots exceed its old budget (`Invalid GPU grass budget summary`); both original narrow-budget and increased-budget diagnostic failures are retained. Standard adaptive inputs refit successfully, but target-density scalars cannot prove the camera never outruns ready grass. Do not treat a cached presentation, sparse scalar receipts, or a shorter planning time as proof of continuous coverage.

## Validation and retained evidence

- Full build; 33 core CTest groups and eight focused lifecycle/recovery/frame/reload/compute/capture/native/timing groups pass. Capture tests include replay, CPU override and GL 3.3 fallback. The timing test’s obsolete CPU-ledger expectation was corrected to check current reservations and the 1 GiB bound for both backends.
- Twelve alternating before/after image/workload pairs are exact. Four production native sessions pass their ownership/readback audits, scale/full-render checks, 6/12 m/s speed checks and CPU/compute reloads.
- Raw results, failed/intermediate experiments, topology planner sources/traces, commands, GPU telemetry, UUID-verified memory samples and binaries stay in ignored `build-f5/fps-60/`; no archives or binary evidence are added to the repository. Final receipts: `paired/results.json`, `after-device{0,1}/results.json`, `native/results.json`, `cpu-diagnostic/results.json`, `planning-{before,before-recheck,after}.log`, `core-tests.log`, `integration-tests.log`, `native-timing-recheck.log`. Local `measure.py`, `paired.py`, `native.py` and `planning.cpp` record reproduction commands and selection logic. The shared EGL and native tooling remains under `scripts/benchmarks` and `tests/app/terrain/native`.

# T3c5 — matched hardware cost acceptance

Hardware contexts and native correctness now pass on Quadro M1000M; see the
[access/context checkpoint](../../../atmosphere/optics/study.md). Both Quadro and
RTX 3070 Ti now supply verified EGL OpenGL 4.3 contexts in the
[matched GPU comparison](../../../../benchmarks/gpu-comparison/study.md).
That comparison uses default CPU terrain and does not rebuild during its short
measured walk; it does not establish the resident compute cost gate. CPU terrain
remains default until the existing cost, transfer and memory gates pass on
matched hardware workloads.

## Measurement gaps to close first

The first diagnostic prerequisite, [T3c5b1](timing/study.md), adds bounded
request-keyed work timestamps for terrain field/expansion, grass metadata,
allocation/prefix and main/reflection placement, plus full frame GPU spans.
It retains missing/dropped samples and does not change publication readiness.
[T3c5b2a](loop/study.md) adds complete native-loop wall receipts, including
event polling, loading presentation, profiler collection and minimized waits.
[T3c5b2b](publication/lifecycle/study.md) completes request-to-publication
latency/outcomes. [T3c5b2c](memory/study.md) adds device-verified physical-memory
sampling; T3c5c owns the matched cost experiments.

`FrameProfiler` retains asynchronous per-render-pass GPU timestamps, CPU scopes,
worker build duration and transfer counters. Its `gpu_ms` sums render stages;
resident preparation runs inside a CPU-only mesh scope and is measured in the
new independent work stream. `gpu_frame_span_ms` now exports the full frame
marker span. `frame_ms` begins after event polling; `interactive.frame` CPU
tracing also covers polling. Neither stage sums nor native correctness traces
alone establish complete terrain cost or request-to-publication latency.

Before matched cost acceptance:

1. Completed in T3c5b1: bounded asynchronous timestamps for terrain field, triangle metadata,
   rounded-slot allocation/prefix and grass placement dispatches. Associate
   samples with request serial, scene epoch and field/topology identity. Separate
   nested stages to avoid adding overlapping intervals. Query only available
   results; retain missing/dropped counts and never force completion for tracing.
2. T3c5b1 exports the frame start/end GPU span separately from summed stage work.
   T3c5b2a exports complete native-loop wall, polling, presentation and event-wait
   intervals in a separate frame-linked CSV. Retain
   CPU worker topology/contact-index durations, render-thread submission,
   draw/presentation and complete native loop wall times. CPU/GPU intervals can
   overlap; do not sum them as a total frame time.
3. Completed in T3c5b2b: request-to-complete-consumer-publication latency in wall milliseconds
   and frames, including superseded/rejected work and destination handoff. Keep
   movement, draw and contact receipts attached to the same generation.
4. T3c5b2c samples physical dedicated-memory availability outside the critical frame
   loop, using context-specific NVX telemetry where available and device UUID
   checks for NVML. Record baseline, steady state and replacement peaks alongside
   owned/admitted logical bytes and CPU snapshots. GPU-wide free-memory changes
   can include other processes; report that scope and uncertainty explicitly.
5. Confirm identical near-camera foliage coverage/density, geometry quality and
   effective budgets. Capability differences must not silently switch backends
   or disable consumers; record actual renderer/version and planner each run.

These diagnostics must preserve existing asynchronous publication: no positive
fence timeout, flush poll, server wait, bulk readback, worker join or glFinish in
normal movement/reload. Validate their bounded lifetime, ready/missing samples,
shutdown and failure/supersession behavior before interpreting costs.

## Matched experiments

Use the frozen production configuration, same viewport, geometry/foliage/shadow/
reflection quality, fixed spin/orbit state and camera or saved initial pose.
Compare CPU terrain with its compatible grass planner against compute terrain
with resident `gpu-v1` planning; preserve user-visible configured density and
budgets. The native acceptance fixture's 10k triangles/128 blades is a
correctness fixture, not a substitute for production cost.

Collect stationary, normal walking at 6 m/s, sprint at 12 m/s and a saved
outer-space-to-Moon handoff. Use actual native input and common body-local path
landmarks; asynchronous publication may produce different frame numbers, so
align by walked distance/handoff state rather than requiring identical dispatch
schedules. Include a replacement/reload segment to expose memory overlap. Keep
startup, steady-state and replacement samples separate. Match VSync/presentation
settings and report them; a presentation cap can hide compute cost.

Run at least three alternating CPU/compute pairs per case on the same actual
context/device. Retain warmup policy, full raw traces and all excluded samples.
Use a fixed measured duration/route and enough frames for stable p50/p95/p99;
report sample count, missing GPU samples, outliers and between-run spread. Do not
replace a slow run with a small fixture or silently discard replacement spikes.
Archive exact source/build/test inputs, binary/config/replay hashes, driver,
renderer/version, UUID, viewport and effective budgets with every pair.

## Decisions

Apply the [original ownership plan's acceptance gates](../../plan.md): at least
75% less terrain input transfer on the canonical 100k-triangle workload; at least
50% less bulk CPU field-evaluation time in matched optimized builds; no
same-workload total-frame p95 regression above 5%; bounded logical/physical
replacement memory; correct near-density and numerical/contact/root parity.
Report CPU worker, submission and individual GPU stages separately. A stage
improvement can coexist with a total-frame regression. If any gate fails, retain
CPU default, document the failing case and fix the measured bottleneck before
repeating affected pairs. Hardware correctness alone does not complete T3c5.

T3c5b1 closes work-query/frame-span diagnostics; T3c5b2a supplies complete native
wall intervals. T3c5b2b supplies complete publication latency/outcomes; T3c5b2c supplies physical
memory observations with explicit scope, age and missed-peak limits. T3c5c
matched measurements remain pending. A1a coefficient reuse and
T3c5a context/correctness are separate tested
prerequisites; A1b highlight reduction and adaptive foliage allocation remain
independent unfinished tasks.


## T3c5c execution checkpoints — 2026-10-06

The cost experiments are separated into reviewable checkpoints:

- **T3c5c1:** production native workload/stationary preflight. Preserve the
  shipping 1280x720 window, 100k Earth cap, 2M candidates and all optical consumers.
  A camera-only public replay freezes time zero and orbital/spin state. Explicit
  CLI planner overrides select legacy CPU versus resident gpu-v1. The private
  probe requests swap interval zero, inspects only owned CPU scalars and records
  primary observer overhead inside the uncorrected native wall. Three alternating
  pairs retain all startup/warmup/close exclusions. Matching scalar density,
  budgets and topology are prerequisites; they do not prove rendered coverage.
- **T3c5c2:** actual native 6/12 m/s walking and sprint routes, with distance
  landmarks, rebuilds, complete frame percentiles and near-root/image coverage.
  Retain stationary preflight as baseline; repeat it if a relevant implementation
  or workload changes. Report effective speed against both animation and real
  wall time, and preserve the fixed quality contract.
- **T3c5c3:** common saved outer-space/Moon handoff and whole-scene reload,
  including complete destination contacts/draws and replacement memory.
- **T3c5c4:** matched optimized 100k canonical field-work/transfer experiment,
  cross-case full-frame/coverage/replacement gates and final default decision.

The new stationary tooling is `scripts/benchmarks/terrain_cost/run.py`.
Do not treat its preflight percentiles as migration acceptance: the private
observer remains included, tails from 240 frames have limited precision, and
movement/reload can expose costs absent from a stationary run. The existing
CPU budget scaling can reduce the effective density below the configuration;
record that explicitly rather than claiming the protected near-density policy
(B1) is already implemented.


[T3c5c1 production stationary preflight](cost/preflight/study.md) now passes
three alternating Quadro native pairs with exact field/topology, camera/root
and planning anchors, identical scalar foliage work and 1,440 complete measured
GPU/native receipts. CPU/compute native p95 ratios span 0.9696–1.0492 with the
primary observer included. All 1,955 excluded frames remain archived. Two scoped
native CTest groups pass (63.63 s); runtime source and application are unchanged.
Resume T3c5c2 actual 6/12 m/s routes and rendered coverage, then T3c5c3 Moon/reload
and T3c5c4 field-time/transfer and cross-case gates. CPU stays default.

T3c5c2 is further split into **T3c5c2a real-time speed prerequisite** and
**T3c5c2b longer matched route cost/coverage**. Production input first exposed
a 50 ms grounded clock cap: actual CPU/compute walking was 3.504/3.605 m/s
and sprint 7.028/7.167 m/s despite correct animation rates. The
[speed study](cost/movement/study.md) fixes this with short grounded advances,
a one-second suspension bound and independent monotonic-wall checks. Use
`terrain_cost/speed.py` for short prerequisite runs, retaining full production
quality. The runtime change requires fresh stationary controls. These short
single input runs do not replace the three alternating pairs, distance-aligned
rebuild/spike samples or rendered near-root density checks in T3c5c2b.

T3c5c2a now passes final production 100 m natural/delayed speed checks and
scoped native clock/suspension validation. Fresh stationary p95 ratios are
1.0368/1.0227/1.0853; the third pair exceeds the migration limit and needs
complete stage/workload investigation plus repeat controls before final gates.
Use these fresh controls for the changed runtime. Resume T3c5c2b matched
longer-route costs and rendered near-root coverage; CPU stays default.

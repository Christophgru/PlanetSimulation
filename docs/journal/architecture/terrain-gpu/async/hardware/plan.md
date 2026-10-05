# T3c5 — matched hardware cost acceptance

Hardware contexts and native correctness now pass on Quadro M1000M; see the
[access/context checkpoint](../../../atmosphere/optics/study.md). RTX 3070 Ti is
visible through NVML but has not supplied a tested OpenGL context. Measure the
actual Quadro first. CPU terrain remains the default until the existing cost,
transfer and memory gates pass on matched hardware workloads.

## Measurement gaps to close first

The current `FrameProfiler` records asynchronous per-render-pass GPU timestamps,
CPU scopes, worker build duration and transfer counters. Its `gpu_ms` sums scoped
GPU stages. Resident initial/movement preparation runs inside a CPU-only mesh
scope, so field/metadata/allocation/placement dispatches are missing from that
sum. Existing frame start/end GPU timestamps are allocated but their span is not
exported. `frame_ms` begins after event polling; `interactive.frame` CPU tracing
also covers polling. Neither CSV `gpu_ms` nor native correctness traces alone
establish complete terrain GPU cost.

Before benchmarking:

1. Add bounded asynchronous timestamps for terrain field, triangle metadata,
   rounded-slot allocation/prefix and grass placement dispatches. Associate
   samples with request serial, scene epoch and field/topology identity. Separate
   nested stages to avoid adding overlapping intervals. Query only available
   results; retain missing/dropped counts and never force completion for tracing.
2. Export the frame start/end GPU span separately from summed stage work. Retain
   CPU worker topology/contact-index durations, render-thread submission,
   draw/presentation and complete native loop wall times. CPU/GPU intervals can
   overlap; do not sum them as a total frame time.
3. Record request-to-complete-consumer-publication latency in wall milliseconds
   and frames, including superseded/rejected work and destination handoff. Keep
   movement, draw and contact receipts attached to the same generation.
4. Sample physical dedicated-memory availability outside the critical frame
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

Implementation of the missing diagnostics and matched measurements remains
pending. A1a coefficient reuse and T3c5a context/correctness are separate tested
prerequisites; A1b highlight reduction and adaptive foliage allocation remain
independent unfinished tasks.

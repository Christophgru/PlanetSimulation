# T3c5b1 — bounded GPU work timings and frame spans

The optional performance trace now measures resident terrain and foliage GPU
work independently of render-stage totals and exports the full timestamp span
of each sampled frame. This closes the dispatch-timing gap in the
[hardware acceptance plan](../plan.md); publication latency, full native-loop
wall time, physical memory and matched CPU/compute acceptance remain pending.
CPU terrain remains the default.

## What is recorded

`--performance-trace path.csv` retains the existing frame CSV and adds
`path.csv.gpu-work.csv`. Existing render-stage names and `gpu_ms` keep their
meaning. Appended frame columns are:

| Column | Meaning |
| --- | --- |
| `gpu_frame_span_ms` | Frame start/end GPU timestamps, including work and gaps between those markers; empty when unavailable. |
| `gpu_status` | `ready`, `ring_full`, `missing_shutdown`, or `untraced`; missing samples do not become zero work. |
| `gpu_query_polls` | Collection attempts for the frame, each checking timestamp availability without waiting. |
| `gpu_events_dropped` | Stage scopes beyond the existing 63-event frame limit. |

The separate work stream measures five stages: terrain field evaluation,
terrain corner expansion, grass triangle metadata, ordered allocation/density
search/prefix work, and grass placement/compaction. Timestamp markers bracket
dispatches and their required barriers. CPU submission wall time is recorded
separately. Placement records identify the main or reflection view and stop
before the subsequent grass draw, so their interval is contained in the
corresponding render stage. **Do not add work timings to `gpu_ms` or to CPU
times**: that would count nested or overlapping work twice.
These timestamp intervals can include submission gaps and barrier costs; they
are not isolated kernel execution or occupancy measurements. Buffer allocation
and upload sit outside the individual dispatch intervals but inside their frame
span when submitted between its markers. Tracing overhead still needs a matched
measurement before interpreting performance acceptance.

Each work receipt identifies its originating frame, scene epoch, request serial,
body, field/topology fingerprints and versions, and backend. Complete resident
preparation supplies request identities; committed grass patches retain them
for later draws. Water records carry their actual water field/topology keys
under the same request. Legacy/unmanaged paths explicitly report
`identity_valid=0` when no complete request identity exists. A grass-only plan
uses the installed terrain request serial; publication latency and separate
replacement-attempt identities are still T3c5b2.
The validity flag describes the epoch/serial/body request attachment; the
field/topology keys and versions are recorded in their own columns.

## Bounds and nonblocking ownership

One trace-owned pool has 64 event slots, at most 128 timestamp query names.
Records outlive temporary terrain buffers, cancellation and old-generation
retirement. Begin/end markers use `glQueryCounter`; collection checks both
queries with `GL_QUERY_RESULT_AVAILABLE` before reading either result. Busy
events retain their slots. Exhaustion writes a `dropped` receipt and continues
rendering without new query objects or forced completion. Allocation failure
likewise records a missing measurement without throwing a profiling exception;
existing GL context/error handling remains in force.

Statuses distinguish `ready`, `failed_ready` (the timed scope unwound through
an exception), `dropped`, `allocation_failed`, and `missing_shutdown`. A ready
dispatch time does not imply its generation was published. Shutdown makes one
nonblocking collection pass, emits missing receipts, and deletes owned query
names while the context is alive. It does not flush, finish, wait on a fence,
join workers, or read terrain buffers. Query names are reused only after a
receipt is resolved. Optional context-thread bindings restore previous
bindings/identities and are not inherited by CPU workers. Disabled work tracing
allocates no work queries.

The existing eight-frame/63-stage ring remains bounded. Frame span and each
stage result now have explicit readiness checks. `frame_ms` still starts after
native event polling, and the GPU span covers only the frame markers, not a
whole application loop. Neither metric alone finishes the total-cost gate.

## Validation and continuation

Controlled query tests check delayed start/end availability, fixed capacity and
slot reuse, missing shutdown receipts, allocation failure, exception/cancel
receipts, nested owner/request restoration, CPU-worker isolation, separate
frame-span versus nested placement accounting, and frame-ring/event overflow.
A renderer integration case exercises real resident preparation/publication and
main/reflection placement while auditing zero positive-timeout/flush fence
polls, server waits and bulk buffer reads. Raw results and frozen inputs are
retained in `validation/`.

The focused hardware case identifies Quadro M1000M, OpenGL 4.3.0 NVIDIA
580.178.04. It resolves 18 work receipts: three field and three expansion
intervals (Earth land/water and Moon land), two metadata and two allocation
intervals, and four main/four reflection placement intervals. Peak pending
work is six, with zero drops and zero positive-timeout/flush fence polls,
server waits or bulk buffer reads. Its 963 frame receipts all have available
frame spans. The small 10,000-triangle/128-candidate fixture establishes
diagnostic correctness; these are not production cost measurements.

Twelve EGL captures additionally compare the preceding binary, the new binary
without tracing and the new traced binary on both physical GPUs, for CPU and
resident-compute terrain. Each group of three PNGs is byte-identical on its
GPU/backend. Actual device UUID, GL context format and binary hashes are
verified. The fixture uses 10,000-triangle terrain, 4,096 grass candidates,
256-pixel shadows and 320×180 output. It checks rendering parity rather than
hardware speed. Inputs, images, logs and frame/work traces are retained under
`validation/images/`.

All 64 CTest groups pass in one uninterrupted run on Mesa llvmpipe/Xvfb
with two rendering workers (1422.72 s), including seven controlled query cases
and four actual-renderer frame cases. The software timing case also resolves
18 work receipts with no drops, and 951 frame receipts (949 ready spans and
two explicitly missing at shutdown); its audit records
zero positive-timeout/flush fence polls, server waits and bulk buffer reads.
Final source/build-input fingerprints and all 41 application/test executable
hashes match the frozen inputs after the full run. Ten compute/CPU/replay PNGs,
all 22 README gallery images and the preceding GPU comparison artifacts retain
their recorded hashes. The full run, controlled receipts, software/hardware
traces and provenance are archived in `validation/`. The journal PDF was rebuilt
and its affected pages reviewed. This run validates correctness; its duration
is not a matched performance comparison with earlier checkpoints.

Continue with T3c5b2 native-loop timing, request-to-complete-publication latency
and device-verified physical dedicated-memory sampling. Then T3c5c runs matched
hardware CPU/compute stationary, 6 m/s walking, 12 m/s sprint, Moon arrival and
reload experiments. The earlier
[Quadro/RTX comparison](../../../../../benchmarks/gpu-comparison/study.md)
measured rendering on default CPU terrain and is not a substitute for those
acceptance experiments.

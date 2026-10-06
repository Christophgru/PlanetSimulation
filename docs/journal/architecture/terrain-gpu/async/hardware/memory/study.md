# T3c5b2c — physical-memory diagnostics

`--performance-trace path.csv` additionally writes `path.csv.memory.csv`. CPU
terrain remains default. This checkpoint supplies diagnostics for T3c5c; the
small correctness fixtures do not establish production performance or memory
acceptance.

## Identity and ownership plan

The CPU owns observation metadata, the resident admission ledger, CPU topology
and process accounting. The active GL context owns UUID/NVX queries. A single
persistent sampling worker owns optional NVML initialization, the UUID-selected
device handle, reads, CSV output and library shutdown. No new GPU allocation,
CPU/GPU terrain policy, adaptive-quality input or publication gate is introduced.

At context setup, query `GL_NUM_DEVICE_UUIDS_EXT` through EXT external-object
support and accept exactly one nonzero device UUID. Unsupported or multi-device
contexts remain unverified. NVIDIA memory reads require this UUID, a successful
NVML handle lookup by UUID and an identical UUID read back from that handle.
Renderer/model names never select the memory device. Optional Linux dynamic
loading avoids a CUDA/NVML SDK dependency. Unsupported platforms, absent library
or symbols, initialization/lookup/read errors, mismatches and invalid counters
have distinct statuses; their physical byte fields remain empty. Zero free
bytes is a valid reading.

The external-object [Khronos specification](https://registry.khronos.org/OpenGL/extensions/EXT/EXT_external_objects.txt)
defines the context device UUID queries. The [NVML device API](https://docs.nvidia.com/deploy/nvml-api/latest/api/group__nvmlDeviceQueries.html)
defines UUID lookup/readback and total/used/free accounting. NVML reports
**device-wide** memory, including other processes; it is not this renderer's
allocation total.

## Observation boundaries and uncertainty

The renderer-ready observation occurs after fixed renderer owners are initialized
and before terrain admission. Its first NVML read is asynchronous: query start,
end and metadata age expose any delay and possible overlap with subsequent GPU
activity. It is a labelled startup checkpoint, not a guaranteed pre-admission
physical baseline. Matched experiments must retain that delay and arrange a
quiet setup/warmup boundary when they need an isolated baseline.

Periodic reads occur every 250 ms after the previous read completes. Startup,
steady/capture, replacement and shutdown carry frame, live epoch and request
serial. Reload events also carry the publication root attempt and proposed epoch,
including requested, preparing, exchange, failure and supersession. Retirement
records the release of old scene owners. Runtime phase transitions retain event
rows even when a short loading/minimized interval lies between periodic reads.
Loading/minimized observations update
the same scalar snapshot; the worker continues independently of drawing.

Event rows retain metadata from their original observation and reference the
worker's cached physical read. `sample_after_observation` explicitly identifies
a read that finished after that event, including delayed startup/draining. Query
time, observation age, sample age and `stale` (over two seconds) are separate.
Event rows are not instantaneous replacement peaks. Periodic reads can miss
transient peaks; sampled maxima are lower bounds on a run's true physical peak.
Read failures clear reported physical bytes instead of silently retaining a
previous successful value.

NVX dedicated, total-available and current-available counters are queried only
at setup and shutdown, with their own timestamp/age/status. They are
context-issued checkpoints and driver estimates, not continuous measurements or
per-process ownership. The [NVX specification](https://registry.khronos.org/OpenGL/extensions/NVX/NVX_gpu_memory_info.txt)
describes these counters. They do not replace UUID verification for NVML. The
existing adaptive-quality startup reader remains unchanged.

## Logical and CPU accounting

The resident ledger includes live, staged and retiring admission reservations.
A whole-scene transaction reserves the old scene externally; after exchange the
live ledger retains that reservation until retirement. Therefore overlap is
`max(live_reserved_bytes, replacement_reserved_bytes)`, not their sum. These are
conservative logical reservations for managed terrain/grass, not driver physical
bytes or an accounting of every shader, framebuffer and renderer resource.
CPU/legacy backends explicitly report this managed ledger as unavailable.

CPU vector bytes count capacities of live/off-live/retired terrain mesh vertices,
indices and face zones, plus the scheduler's stable ready-result geometry and
topology vectors. The scheduler uses `try_lock`, retains its ready slot and
never reads mutable running-worker scratch. Busy observations leave its fields
empty. Fields, sparse contacts, configuration, allocator overhead and running
scratch are excluded from this **partial** vector count. Worker-side Linux
process RSS has a separate timestamp and includes the whole process/allocator,
including worker and library pages. RSS and vector capacities overlap and must
not be added. CPU snapshots are independent of physical GPU measurements.

## Bounds and shutdown

The renderer updates its scalar latest observation with `try_lock`. Lifecycle
events use a separate fixed 64-slot queue with one context producer and one
worker consumer; release/acquire cursors keep event submission independent of
the latest-state mutex. Ordinary skipped snapshots increment
`skipped_latest_observations` and `dropped_observations`; an event retained while
latest-state copying is busy increments only the former. Ring overflow increments
`dropped_events` and `dropped_observations`. Thus no lock contention discards a
lifecycle event, while bounded queue overflow remains explicit. The worker copies
latest/final scalars under the mutex and moves its bounded event batch outside it.
All driver, process-accounting and filesystem work occurs outside that lock. There are no GL calls on that worker, no frame-thread NVML
calls, and no diagnostic driver queries, waits, joins or file writes in native
movement/reload observation hooks. Disabled tracing creates no reader or
observation clock. NVX/UUID reads are limited to context setup/shutdown.

The shutdown checkpoint precedes destruction of remaining GPU resources; it is
not a post-teardown reclamation measurement. Shutdown copies the final observation,
wakes and joins the one reader before
closing its output or unloading NVML. Joining is confined to renderer teardown,
including constructor failure; it never affects publication readiness. NVML
has no cancellation here: a stalled driver can delay teardown, but cannot create
unbounded tasks or block frame submission. Output is best-effort after the
initial open check; filesystem failures during later worker writes do not change
rendering decisions. Concurrent processes and GPU utilization telemetry can
change the observed device-wide values.

## Validation

Eight controlled cases pass, covering UUID rejection/readback, errors, valid zero
free memory, bounded slow-reader ownership, cached staleness, factory/read
exceptions, output-open failure and shutdown. A deterministic held-latest-lock
regression retains a failure's attempt/epoch while counting an ordinary snapshot
as skipped. The stable scheduler ready slot has a separate vector-capacity check.

Twelve scoped CTest groups have passing results. An early run found CSV escaping
and a private fixture retiring before inspecting overlap. Snapshot-lock loss of
a lifecycle event motivated the separate queue. The final runtime batch passes
11/12 groups in 429.91 s: the remaining fixture used a valid empty native scene
as invalid input. Changing only that test to a negative planet radius gives a
passing full renderer group (67.55 s). Runtime source and the other 41 executables
retain the preceding fingerprints; final source/build-test/all 42 executables and
two driver-fixture hashes match after validation. This is a scoped run plus a
fixture correction/recheck, not an uninterrupted 12/12 or a new full 66-group run.

Three actual Quadro renderer cases cover disabled diagnostics, CPU capture/reload,
resident replacement overlap, invalid input, supersession and retirement. Their
12 lifecycle events independently join five publication roots. Two Quadro native
CPU/compute fixtures verify startup/loading/steady/minimized/resumed receipts,
matching UUIDs and NVX/NVML values, with zero memory queries, blocking/flush fence
polls, server waits, bulk readbacks or explicit finishes inside frames. Equivalent
software checks preserve unsupported physical readings and the default CPU path.

Four small CPU/resident capture pairs on Quadro and RTX verify context/NVML UUIDs
and exact traced/untraced PNGs. These use 320×180, 10k triangle budgets and 128
blades, not the production acceptance workload. Asynchronous startup age and
cached/event/sample scope remain in every raw trace. Gallery (22) and historical
baseline (10) hashes match. The journal PDF is updated and visually reviewed.

Exact inputs, commands, raw/compressed traces, failed preflight logs and results
are retained in [validation/evidence.json](validation/evidence.json). These checks
establish diagnostic correctness only; T3c5c still owns matched production
CPU/compute stationary, 6/12 m/s walking/sprint, Moon and reload cost experiments.

# T3c5b2b1 — ordinary request-to-consumer publication

Interactive `--performance-trace path.csv` adds `path.csv.publications.csv`.
This checkpoint implements ordinary CPU terrain startup/replacement and resident
compute terrain/grass-only attempts. The enclosing T3c5b2b task remains partial:
reload transactions, capture endpoints, actual destination handoff and CPU
grass-only preparation are T3c5b2b2. Physical-memory diagnostics remain T3c5b2c,
matched hardware acceptance T3c5c; CPU terrain stays default.

## Boundaries and identity

Admission starts a steady-clock interval before worker validation/submission or
synchronous CPU construction. Resident grass starts before GPU submission.
CPU start/end, GPU submission/readiness and complete mesh/grass installation
have separate offsets from that start. The successful endpoint follows the
first returned full `renderScene` call with the matching live terrain generation,
water, grass and contact receipts. The selected astronaut contact revision must
match when drawing the character. Cached presentation does not invent a draw.
This is CPU observation of complete consumer submission, not GPU completion or
visible monitor presentation. Do not add it to frame or GPU costs.

Each attempt has an independent monotonically increasing integer. Grass-only
replacements deliberately retain the terrain serial but receive new attempt
IDs. GPU work retains this ID through dispatch and subsequent grass placement;
join the two CSVs by `attempt`, and join frame/native-loop CSVs by frame number.
An attempt of zero in GPU work means it has no publication receipt in this scope.
Attempt identities are diagnostic only: terrain matching, replay and generation
fingerprints retain their existing contracts.

The publication record contains epoch/request/body/name hash, field/version/
backend/local-mask identity, requested eye, start/end frames, wall milliseconds,
phase offsets, prepared generation fields/topologies/revisions, resident grass
anchor and outcome. Body-name hashes use FNV-1a over UTF-8 bytes; indices apply
within that epoch. Terrain request eyes are body-local world units; grass request
eyes and resident grass anchors are normalized by planet radius. CPU grass
anchors are unmeasured and left empty. Unknown frame numbers, unobserved phases
and successful latency on unsuccessful outcomes are empty rather than zero.
Elapsed frames are the difference of frame numbers, not a count of draws; loading
and minimized iterations can be included.

`publication_ms`/`publication_frames` are populated only for `published`.
`elapsed_ms`/`elapsed_frames` also describe rejected, coalesced, obsolete,
CPU-failed, preparation-failed, replaced-before-draw and shutdown attempts.
A complete installed generation replaced before drawing is `replaced_before_draw`.
Shutdown drops in the worker are explicit `shutdown`; unfinished renderer-side
work is `missing_shutdown`. Late worker notifications cannot overwrite terminal
records or reused slots. Cancellation closes the logical interval without waiting
for an executing snapshot to release ownership; CPU-end offsets can remain empty.

## Ownership and remaining scope

One fixed 64-record array retains active attempts and terminal rows awaiting
collection. On exhaustion, tracing skips the new receipt without delaying
admission, incrementing an explicit `trace_overflow` summary; that row has no
latency. The context thread copies and frees terminal slots under a short mutex,
then writes and flushes CSV outside that mutex. Worker callbacks update scalar
metadata only. No GL calls, queries, fences, workers, futures, topology retention
or new readiness waits are added by this recorder. The existing scheduler is
stopped before the trace owner is destroyed. Disabled tracing opens no output,
reads no diagnostic clock and retains no attempt.

Ordinary prepared/running work is closed when a reload cancels it or commits a
new epoch. Off-live reload builds are deliberately outside this checkpoint and
are not recorded as successful live publication. Whole-scene parent/child
outcomes and handoff/capture/CPU grass endpoints must be implemented before
T3c5b2b is complete. Trace overhead is part of traced execution; small fixtures
and injected delays validate scope, not production performance.

## Validation

Five new controlled recorder cases validate exact wall/frame intervals,
matching generation/revision/anchor endpoints, independent grass identities,
undrawn replacement, fixed-slot exhaustion/draining/reuse, ignored late worker
notifications, unknown frames, epoch cancellation, missing shutdown and disabled
clock behavior. Two real-worker cases cover validation/admission rejection,
coalescing, CPU failure, shutdown and epoch cancellation while ownership remains
executing. These extend the timing target to 14 cases and worker target to 16.

Two new actual-renderer cases validate default CPU startup/worker replacement
and resident compute startup, grass-only failure/recovery and terrain replacement.
Each successful receipt is joined to a separately captured draw generation,
revision, serial and frame. GPU work also joins its independent attempt IDs.
The final renderer fixtures produce three CPU successful rows, four compute
successful rows and one failed compute preparation; no dropped starts.

All ten relevant CTest groups pass in one final-input run (456.95 s): renderer
lifecycle, movement/destination recovery, ordinary frames, asynchronous reload,
workers, compute preparation, controlled timing, performance capture/replay,
real native-loop timing and native input. The existing five native scenarios and
four shipping rejection contracts retain their checks. This is scoped validation;
the complete 65-group suite was not rerun here. A preflight CPU case exposed an
assumption in the shared private draw helper that managed GPU consumer metadata
existed on the CPU path. The helper now checks the managed flag; the rejected
preflight and passing final evidence are retained.

The two existing production GLFW loop fixtures now independently sample ordinary
mesh generation keys and join publication receipts to their actual consumer
frame. Mesa software observes 65 loop receipts (15 CPU/50 compute), seven
publication outcomes and four successful draw endpoints. Quadro OpenGL 4.3.0
NVIDIA 580.178.04 observes 66 loop receipts (15 CPU/51 compute), eight publication
outcomes and four successful endpoints. CPU/compute audit counters remain zero
for positive-timeout/flush polls, server waits, bulk reads and explicit finishes.
The Quadro name/version come from the actual context; physical UUID/memory
verification remains a separate task. Small 10,000-triangle/128-candidate fixtures
and injected readiness/presentation delays do not establish production cost.

`validation/` retains raw controlled receipts, renderer and native observations,
traces, logs, input/executable fingerprints and artifact hashes. Sorted relative
paths and bytes separated by NUL delimiters fingerprint source (`src`, `shaders`)
and build/test inputs (`tests`, `configs`, `scripts`, root CMake). All 41
executable hashes match after validation. The 22 gallery images and ten retained
historical baseline PNG hashes remain unchanged; the latter were not regenerated
by a complete new capture sweep. Journal PDF and changed pages were reviewed.
Resume T3c5b2b2 before treating publication lifecycle diagnostics as complete.

# T3c5b2b2a — asynchronous resident reload publication

Interactive resident compute reloads now start a whole-scene diagnostic attempt
before loading and validating the requested configuration. Its epoch is the
monotonically assigned reload-attempt epoch, which can differ from the live
scene epoch while preparation is pending. Per-body terrain requests receive
independent child attempt IDs, propagated through the CPU worker and GPU work.
Final grass replanning belongs to the same body's preparation and retains its
child ID; it does not invent another terrain request serial.

Each child's GPU-ready offset observes its first off-live body preparation.
Its prepared generation/grass anchor is recorded only after final grass planning,
atomic whole-scene exchange, tracking/options exchange and live contact binding.
The appended `scene_exchange_ms` on the root observes this completed exchange.
Children cannot publish before the root exchanges. A child succeeds only after
an actual complete `renderScene` returns with matching live land, water, grass,
contact generation and consumer revisions. After all admitted children succeed,
the same render endpoint closes the reload root. Empty scenes require a returned
complete scene draw after exchange too. Cached presentation never closes it.
Wall/frame latency includes admission, queueing, old-resource retirement,
preparation and waiting for that draw. This is CPU observation of draw submission,
not GPU completion or monitor presentation; it must not be added to GPU/frame
costs.

The publication CSV appends `parent_attempt`, `scene_exchange_ms` and
`child_attempts` to its existing columns. Roots have kind `reload`, no body or
prepared body generation, and a child count; children have kind `terrain` and
link to their root. Each child's `attempt` joins request-keyed GPU work, including
final grass placement. Root and child durations overlap and must not be summed.

Invalid configuration produces `invalid_config`; latest-request replacement
produces `superseded`; GPU preparation, preview planning and final-exchange
failure produce `preparation_failed`. The root closes every still-active child
on terminal failure. A CPU child may already have its own `cpu_failed` outcome.
Retirement poll failure keeps the latest request open and the old live scene
usable. Unexchanged pending reloads close as `shutdown` when the renderer stops.
Committed but never-drawn receipts retain the existing `missing_shutdown` or
later-epoch `obsolete` outcome. None receives successful publication latency.

Roots and children share the existing fixed 64-record scalar pool. Completed
rows own slots until context-thread drainage; no additional worker, future,
scene/topology copy, GL operation or wait is introduced. Root child counts are
maintained when children close, so draining/reusing child slots cannot lose
ownership. If a child cannot be admitted to tracing, overflow is explicit and
its root becomes `trace_incomplete`, without successful scene latency. Disabled
tracing retains no records and reads no trace clock. Tracing IDs are excluded
from replay, generation fingerprints and terrain identity matching.

## Validation

Seven scoped CTest groups pass in 195.27 s. After the private GPU-binding
correction, the four reload cases pass software CTest in 26.75 s and actual
Quadro OpenGL 4.3 in 13.19 s. Each renderer records 14 reload roots (seven
published, three superseded, two invalid configurations and two preparation
failures), 15 children and 88 child-linked GPU events. All 138 GPU events per
renderer resolve as ready; trace/GPU overflow and blocking polls, server waits
and bulk readbacks remain zero. Successful dispatch and placement events match
their child's epoch, serial, body and final land/water generation. Saved replay
remains byte-identical within each renderer. Frozen source, build/test inputs,
all 41 executable hashes, raw traces and logs accompany this study in
`validation/`. This is scoped correctness validation, not a full 65-group run or
a performance-acceptance result.
The controlled-clock cases exercise draw gating, partial-body completion after
row drainage, invalid/superseded/failed/shutdown families, empty-scene draws and
bounded-pool overflow. Actual-renderer reload cases retain delayed old-scene
movement, same-field supersession, invalid configuration, GPU failure, failed
final-exchange fence, exact saved-pose replay, startup reload, reordered/removed
bodies and a Sun-only scene. Successful child rows are checked against actual
consumer generation/revision/grass-anchor receipts and share their root's final
draw frame. The GL audit rejects positive-timeout/flush polls, server waits and
bulk buffer readbacks. Private fixture frame counters and explicit GPU work
bindings verify child dispatch/placement joins without claiming full native-loop
measurement. An evidence review found missing work bindings in the initial
fixture; they were added before the final reload-specific software/hardware
rechecks. Runtime code and the six other scoped test executables were unchanged.

## Remaining scope

T3c5b2b2 remains partial. Synchronous legacy CPU reload, capture reload adapters,
capture render endpoints, actual non-preview destination handoff and CPU-only
grass admission are T3c5b2b2b. Existing capture/replay checks do not imply new
capture publication coverage. Physical-memory sampling remains T3c5b2c and
matched hardware performance acceptance T3c5c. CPU terrain remains default.

# Bounded terrain CPU preparation (T3c1) — 2026-10-04

The [asynchronous implementation plan](../plan.md) begins with CPU ownership and
stale-work control. This checkpoint introduces that stage in
`src/rendering/geometry/jobs/` and integrates it with terrain preparation. GPU
terrain/grass readiness and atomic multi-consumer publication remain T3c2/T3c3;
interactive compute remains unavailable until T3c4. CPU terrain is still default.

## Snapshot and executor contracts

`TerrainBuildRequest` owns a surface/field snapshot, planet settings, previous face
zones, local planning eye and metre scale. Its identity includes scene epoch,
request serial, body index **and name**, field fingerprint/versions, backend,
resident-planner flag and local/distant mode. Validation rejects mismatched
field/radius/scale/name/version, invalid selectors and nonfinite/zero eyes before
changing the queue. Settings are immutable within a scene epoch; config reload
advances that epoch, so older water/LOD settings cannot install into a new scene.

`TerrainBuildScheduler` owns one persistent CPU thread, one latest queued request
and one completed result. Newer submitted serials replace the queued snapshot;
the executing snapshot retains its ownership. The worker stops dispatching while
a completed result occupies its slot, preventing an undrained completion queue.
Counters report submissions, coalescing, obsolete work, errors, completed work
and peak executing/queued counts. A client must consume returned results; the
executor does not bound a caller that deliberately retains arbitrary results.

The worker builds outside the scheduler mutex. Polling and identity/counter
operations take the short state lock, never wait for a build or GPU fence. This
is bounded scheduling, not a lock-free API or a guarantee about frame latency.
Worker exceptions become completion errors and do not terminate the executor.
Shutdown drops queued work, wakes the executor and joins the executing snapshot.

Epoch changes drop queued/ready old work without joining an executing build.
When that build finishes, its result or error is discarded if the epoch changed.
The same executor survives repeated reloads, so old futures and their destructors
no longer wait in the reload path. The renderer joins before CPU trace and context
ownership ends, including constructor failure unwinding. Workers own no GL objects
and make no context calls. Trace binding labels each execution as `terrain worker`.

## CPU outputs and runtime integration

Compute requests construct canonical land/water topology and a matching
`SparseTerrainContacts` index on the worker. The result carries immutable field
snapshots for context-thread submission. In resident mode, land/water geometry
headers contain statistics but no float vertices or draw indices, and bulk
height/normal/material evaluation remains zero. Contact construction also evaluates
no heights; bounded lazy contact positions are queried after ownership transfers
to the simulation thread.

The contact source owns its radial topology; GPU input staging temporarily keeps
another canonical copy until submission finishes. This adds bounded CPU staging
memory relative to T3b2's move-after-submit construction. For a 100,000-triangle
canonical land topology the logical duplicate is about 2.8 MB, excluding fields,
water, BVH and allocator overhead. The resident 12 MB shaped land mirror remains
absent. No total memory saving, hardware speedup or available-VRAM estimate follows
from these array counts.

The extracted builder preserves legacy CPU evaluation and the compatibility
planner's expanded geometry. Ordinary CPU capture/startup and local/distant mode
changes keep their existing synchronous path. CPU walking rebuilds now use the
bounded executor; one closest eligible body is queued after examining the scene.
Moving more than ten metres past a pending anchor coalesces a follow-up. Completion
may install a still-compatible older anchor and then queue a newer one, avoiding
perpetual rejection while walking.

Before installation, the renderer checks epoch, body index/name, field/version,
backend/planner, local mask and serial ordering. Eye motion alone does not make a
matching topology obsolete. A result older than the last installed serial, from
a reordered/replaced body, or from an incompatible mode cannot install. A worker
error retains the prior mesh and records its failed anchor; another attempt needs
changed epoch/mode or at least ten metres of movement, avoiding per-frame retries.
GPU allocation/publication recovery is still a later checkpoint.

Compute capture uses an explicit exclusive CPU wait, then submits GPU land/water
from the returned field/topology and adopts the already-constructed contacts.
The context thread checks header/contact generation agreement before submission.
Existing capture-only GPU waits remain. Capture diagnostics add
`render.terrain_cpu_worker`, including the observed one-worker/one-queue peaks,
failures, obsolete/rejected completions and pending status. Frame reuse also
accounts for current-epoch CPU work. Scene-wide reload publication remains T3c3;
this stage only removes worker joins and stale CPU installations from reload.

## Validation

Eight CPU cases use controlled gates rather than timing guesses to cover queue
replacement, delayed polling, completed-result backpressure, repeated epochs,
body reordering and field/mode/version/serial mismatch, failure recovery and
shutdown. The resident snapshot test changes the caller's inputs after submission,
compares exact canonical samples/indices and confirms empty render vectors,
zero bulk evaluation and zero contact heights at construction. Separate CPU/legacy
compute checks retain exact expanded geometry.

All eight targeted cases pass (73 ms), and all **59 CTest groups pass in one
uninterrupted final-binary run (736.20 s)** on GCC 12.2/Mesa llvmpipe under Xvfb
with two rendering workers. Compute capture checks assert two completed CPU jobs,
peak executing/queued counts of one, no pending work/errors and unchanged
scalar-only GPU allocation readback. All ten compute/CPU/replay PNG hashes match
T3b2 exactly, including legacy compute and GL 3.3 fallback. Actor, flight, exhaust,
grass, atmosphere, renderer lifecycle, reload, native input and adaptive quality
checks pass. All 22 gallery hashes and their earlier provenance remain unchanged.

Retained [evidence](validation/evidence.json), [full test log](validation/tests.log),
[worker cases](validation/worker.log) and [capture report](validation/capture-results.json)
record the tested source, executable and artifact fingerprints. Layout, local
links and the regenerated journal/PDF are checked. This correctness run establishes
neither target-GPU performance nor complete asynchronous GPU publication.

Next is T3c2: stage GPU terrain and grass resources, poll completion without
normal-frame waits and prepare allocation failures before publication. The CPU
worker alone does not enable interactive compute or establish complete atomic
land/water/grass/contact/shadow/reflection ownership.

# Asynchronous terrain consumers (T3c) — implementation plan, 2026-10-04

This is the next phase after [T3b2 allocation](../grass-allocation/study.md).
It specifies changes to the audited runtime; it does not enable interactive
compute or claim hardware performance. The [ownership plan](../plan.md) remains
the acceptance contract. CPU terrain and legacy captures keep their existing
paths while this pipeline is introduced behind the compute option.

## T3b2 baseline seams and required changes

| Source | Behavior at T3b2 | Required change |
| --- | --- | --- |
| [TerrainMeshes.cpp](../../../../../src/rendering/runtime/TerrainMeshes.cpp) | CPU walking rebuilds use a future. Its completion branch installs CPU geometry directly. The compute branch constructs contacts and waits for land/water on the context thread. | Workers return canonical topology, statistics, immutable field snapshots and a matching contact index. Compute completion uses a separate staged generation. |
| [ProceduralGrass.cpp](../../../../../src/rendering/foliage/procedural/ProceduralGrass.cpp) | Resident preparation generates metadata/allocation, waits for capture and replaces the live patch. | Split submission, nonblocking polling, completed-summary validation and patch adoption. Staging must not mutate live textures, descriptors or queues. |
| [ScenePass.cpp](../../../../../src/rendering/runtime/ScenePass.cpp) | Grass preparation occurs before rendering. Shadows are keyed by land revision; reflections reuse the current mesh/patch vectors. | Compute passes consume one published generation. No preparation wait or partial adoption during a pass. |
| [Interactive.cpp](../../../../../src/rendering/runtime/Interactive.cpp) | Reload destroys old meshes after CPU scene construction and swaps futures; future destruction can wait for workers. Pending work contributes to frame-cache invalidation. | Keep work ownership across reload, invalidate it with a scene epoch and publish a replacement scene only when its required consumers are ready. Pending GPU work must also invalidate cached presentation. |
| [Character.cpp](../../../../../src/rendering/runtime/Character.cpp) | Matching sparse contacts bind by installed mesh revision; queries may extend the bounded cache. | Rebind at the same publication boundary as land and grass. CPU field snapshots must outlive their contacts. |
| [CommandLineOptions.cpp](../../../../../src/app/CommandLineOptions.cpp) | Compute requires a capture output. | Retain that gate until recovery and native input acceptance pass; then permit explicit interactive compute while CPU stays default. |

The existing CPU future completion branch must never install the empty geometry
headers used by resident compute. Removing the CLI gate first would expose this
incomplete path. Reload also needs explicit outstanding-job ownership before
future containers can safely be replaced without a walking-frame wait.

## CPU and context-thread ownership

CPU workers receive immutable snapshots of the field, planet settings, planning
eye, previous face zones and request identity. They select/subdivide topology,
reconcile sinks, build water topology and construct `SparseTerrainContacts`.
They perform no GL calls and no full shaped-render evaluation for `gpu-v1`.
The mutable contact position cache is used only after ownership transfers to the
simulation thread; construction and contact queries must not race.

Use a bounded worker scheduler, initially one executing build and one coalesced
latest request. Requests for the active/approached body take priority; other
bodies retain their last valid generation. A newer request replaces queued work,
not a worker-owned snapshot. Completion carries scene epoch, body identity,
request serial, field/topology versions and planning settings identity. An epoch
or body mismatch discards the result before GL submission. Eye motion alone
does not invalidate useful work indefinitely: accept a still-covered anchor and
queue a follow-up, while mode/body/config changes invalidate incompatible work.
Join the scheduler at renderer shutdown; reload must not destroy running futures
or borrowed trace/context state. Cooperative cancellation can shorten obsolete
builds, but correctness cannot depend on immediate cancellation.

All GL resources, submissions and destruction stay on the current context thread.
It submits land and water, then metadata and allocation using barriers in the
same ordered context. Dependencies do not require CPU fence waits. Poll existing
fences with zero timeout; retrieve the fixed allocation summary only after the
allocation fence signals. Interactive code must never call `waitForCapture` or
request shaped vertices, triangle metadata or reference arrays. Capture code can
drive the same state machine to completion with explicit waits for deterministic
output. Report scalar summary bytes separately from diagnostic reads.

## Staged generation and publication

Use a move-only staged owner with states `CpuQueued`, `CpuBuilding`, `GpuPending`,
`Ready`, `Published`, `Retiring`, `Failed` and `Obsolete`. It owns land/water GPU
outputs, contact source, terrain statistics/zones/anchor, grass metadata,
allocation, summary and the resource handles needed by the completed patch.
The identity is a bundle: land and contacts share one topology key; water can
have a different field/topology key, linked by the same request identity.
Grass also carries its settings/seed/planning-anchor identity. A body index alone
is insufficient across reload or changes in the planet list.

Before marking `Ready`, validate every key, buffer size, summary cap, completion
flag and required consumer. Allocate/check all patch texture/VAO/indirect/output
resources before publication, so a first draw cannot introduce an unhandled
allocation failure after replacing the previous generation. Disabled foliage
and water have explicit empty consumer states with the same request identity.

Publication occurs once at a frame boundary before contacts or scene passes use
the new generation. Transfer the complete bundle with operations that cannot
allocate or throw. Increment land revision once, replace water and grass, rebind
contacts, update face zones/anchor/statistics and invalidate shadow/frame caches.
Main terrain, grass, water and reflections then share that installed request.
Shadows must be regenerated for the new land revision before its first dependent
draw; expose the consumed request/revision for acceptance checks. Do not publish
water first and defer grass until a later frame.

The previous generation remains drawable throughout preparation. On failure,
release only staged resources and retain its meshes, contacts, patch and revision.
Record a bounded diagnostic and retry only on changed inputs or bounded backoff.
For a new body with no valid generation, keep a loading state and defer grounded
walking until matching contacts exist; do not draw one generation while querying
an unrelated terrain oracle. Initial capture may explicitly wait. Treat a scene
reload as a transaction: invalid configuration or failed replacement preserves
the prior scene, and old-epoch work can never install into the new scene.

## Capacity and retirement

Initially allow two complete logical sets per body: one published and one
preparing or retiring. Limit simultaneous GPU preparation globally to one body.
After publication, fence the old set's last use and poll retirement; do not start
another spare for that body until the retired slot is released. Coalesce requests
while either the CPU or GPU slot is occupied. Account for topology/index/cache,
input staging, resident terrain, metadata/allocation, blade queues and replacement
overlap. Bound parameter caches and retired jobs across repeated reloads.

Keep the queried block/work-group limits and conservative candidate cap from
T3b2. Admission uses the full generation ledger and an explicit fallback when a
spare cannot fit. This does not implement the available-VRAM/density controller
(B1). Unknown memory telemetry must not silently promise a budget. Driver physical
allocation and total-frame cost still require hardware measurement.

## Implementation checkpoints and acceptance

1. **T3c1: scheduler and staged CPU outputs.** Add request identities, bounded
   scheduling/coalescing, worker contact construction and stale/epoch handling.
   Keep interactive compute gated. Test delayed jobs, exceptions, body reordering,
   repeated reload, shutdown and absence of full render-vector evaluation.
2. **T3c2: asynchronous grass and GPU readiness.** Separate prepare/commit APIs;
   stage all resource allocations and poll without waits. Test delayed fences,
   scalar-only reads, disabled consumers, limits, memory admission and failure
   preservation. Retain exact capture/replay behavior through the same pipeline.
3. **T3c3: complete atomic publication and recovery.** Commit all consumers at one
   boundary and fence retirement. Inject failures at each preparation stage;
   verify unchanged previous revisions/keys and bounded resources. Test camera
   movement, rapid body switches, Moon arrival, valid/invalid reload and no stale
   completion publishing into another scene.
4. **T3c4: explicit interactive opt-in.** Only after prior gates, allow compute
   without a capture output. Native standing/walking/sprint/jump/flight/trail,
   reflection and shadow checks record matching consumer identities and zero
   normal-frame waits. Exercise GL 3.3 fallback and locked replay rejection.
5. **T3c5: hardware acceptance.** Record CPU topology/index/submission costs, GPU
   field/metadata/allocation/placement costs, publication latency, total frame
   percentiles, logical/physical memory and near-density behavior. Compare CPU
   and compute under matched stationary/walking/body-switch settings. llvmpipe
   validates correctness only. CPU remains default until actual hardware gates
   pass; missing hardware leaves this checkpoint pending.

These checkpoints are dependencies, not claims of completed implementation.
Keep numerical field/plane/root parity and versioned replay tests from T1–T3b;
new scheduler tests should assert failure/publication behavior rather than merely
repeat the state machine's implementation.

The [T3c1 worker checkpoint](worker/study.md) implements bounded CPU scheduling,
value snapshots and epoch/body/mode/serial validation. Eight worker cases and all
59 CTest groups pass (736.20 s), with exact capture/replay compatibility. Resume
at T3c2; remaining steps in this plan are unchanged.

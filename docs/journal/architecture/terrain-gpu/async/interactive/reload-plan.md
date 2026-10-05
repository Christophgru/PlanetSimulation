# T3c4b — asynchronous reload design notes

T3c4b is implemented and tested; see [implementation and acceptance](reload/study.md).
These design notes retain the ownership decisions. Keep the public CLI
capture gate until T3c4c native input acceptance; implement resident compute
reload first, preserving the existing CPU and capture adapters.

## Reuse and ownership

`SceneTerrainReplacement` already owns an off-live prepared scene, complete body
consumers, a scene-replacement lease, aggregate admission and old-scene retirement.
Its `poll()` and `pollRetired()` are nonblocking. The blocking work is in the
renderer adapter: `executeForReload()` for each CPU body, all-build retention for
the prospective chase preview, and `waitForCapture()` for each GPU body.

Introduce one pending renderer reload owner for config/replay/options, prepared
scene, tracking, request identities and progress. Only that owner can submit
replacement work. Reserve a monotonically increasing attempted epoch, including
abandoned requests, so a superseded result cannot match a newer request using the
same field. Live epoch changes only at complete publication.

The persistent scheduler needs nonblocking future-epoch ownership operations:
begin replacement, submit future work, inspect/poll completion and abort/commit.
Beginning or superseding must drop queued/ready obsolete work while an executing
snapshot retains ownership and finishes normally. Aborting keeps the live epoch
usable; committing advances it. Never join the worker or call its exclusive
capture adapter during frame progress. Keep one running, queued and ready slot.

Acquire the existing scene lease only after unfinished live GPU staging has been
cancelled and renderer staging identity cleared. Freeze live terrain/grass
publication during replacement so the owner's captured buffer/key identities
remain valid. Existing character, orbit updates, input and managed draws continue
against the old complete scene. Poll existing retirement before beginning another
replacement; retain only the latest requested snapshot while retirement is busy.

## Bounded body preparation and final chase planning

Prefer retaining completed off-live body meshes/contacts over collecting full CPU
topology outputs for every body. Build one body through the worker, submit its
complete GPU generation with a provisional anchor and poll its completion before
reusing the preparation slot. Store only required tracking and contacts after
submission. Charge every off-live generation and any temporary grass spare under
the existing logical admission limits.

After all off-live contacts exist, use the existing restored prospective preview
against those contacts to obtain the replacement chase eye. Replan grass-only
consumers with their final anchors before whole-scene publication; saved body-local
replay anchors retain their exact values. Extend the scene owner with an owned
grass-only replanning operation and keep controller readiness separate from
provisional all-body readiness. Validate final receipts/anchors before exchange.
If a provisional anchor differs so little that no replacement is needed, prove
that the final receipt still meets the chosen replay/interactive contract.

This sequencing avoids another CPU executor and an array of retained full topology
outputs. It adds a second grass pass where final prospective contacts change the
anchor. Measure that cost in hardware acceptance rather than claim a speedup.

## Boundary and acceptance

Factor or reuse the existing renderer's complete exchange: tracking, config/replay,
options, epoch, clock, camera binding, selected contacts and effect/cache reset.
All allocations, replay validation, clip/speed validation and fence creation must
precede its no-throw exchange. Reset frame-core staging/backoff state at commit.
The native loop resets transition/telemetry/elapsed clocks and reports success
only when publication actually commits, not when a request is accepted.

A newer valid or invalid reload request supersedes pending preparation; failed
latest work retains the current live scene. Preserve old resources until a
zero-timeout last-use poll succeeds. Test delayed CPU and GPU work, supersession
of executing/queued/ready results, changed body counts/orders/fields, invalid
config/replay, admission/fence failure, repeated recovery, live motion/draws during
preparation and old-buffer retirement. Instrument zero flags/timeouts and no
server waits throughout request progress. Capture waits remain explicit adapters.

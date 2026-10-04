# Complete scene replacement ownership (T3c3c1) — 2026-10-04

This checkpoint implements the whole-scene transaction owner following the
[renderer publication checkpoint](../renderer/study.md) and
[asynchronous plan](../plan.md). T3c3c is split into ownership (T3c3c1), renderer
reload integration (T3c3c2) and movement/body-switch/Moon acceptance (T3c3c3).
The current renderer reload adapter remains unchanged until T3c3c2. CPU remains
default and interactive compute remains gated.

## Ownership and readiness

`SceneTerrainReplacement` validates a replacement config into an independent
`PreparedScene`, including orbital state, terrain fields and mounted cameras.
It owns separate mesh arrays, grass state and a complete `TerrainPublication`
manager. The existing CPU scheduler supplies value-owned topology/contact builds;
this owner introduces no additional worker. Requests bind the replacement epoch,
body index/name, field/version, backend/resident mode, serial and build anchor.
Submission rejects changes to any of those values before GPU dispatch.

Per-body GPU staging uses the tested preparation/metadata/allocation/draw-resource
pipeline. Polling publishes ready body resources only into the replacement's
private arrays. Readiness requires every configured body; completing the first
body cannot publish a partial scene. Capture adapters explicitly wait. Normal
preparation and retirement polls use zero-timeout fences.

The live publication owner grants one replacement lease. It excludes another
replacement and normal terrain/grass submissions until staging is cancelled or
the old scene is retired. Live terrain/grass remain drawable during this lease.
Renderer integration must prioritize reload CPU jobs, suspend competing terrain
submission and retain the replacement object through retirement; it must not
start another executor or exchange contact/camera pointers before publication.

## Memory and complete exchange

The candidate manager reserves all old published/retiring terrain sets as external
bytes before dispatch. Its 1 GiB logical aggregate admission includes those old
bytes plus replacement sets; the 512 MiB per-set ceiling still applies. Admission
failure preserves the live scene and dispatches no new terrain buffers. Initial
body retirement fences hold empty destinations and contribute no duplicate bytes.
This ledger covers the established terrain/grass reservation, not all CPU state,
shader programs, driver physical VRAM or the future foliage density controller.

Before scene transfer, the owner validates the current epoch/config, body count,
names/field identities, land/water handles, revisions, keys, contact source and
grass key/revision/anchor against its live snapshot. It validates every candidate
consumer, then allocates a last-use fence following the old scene's final draws.
Any failure leaves the live consumers intact.

The successful boundary uses no-throw swaps of CPU scene/config, land/water arrays,
grass patches/trails/planning state and publication receipts/accounting. Grass
shader references retain stable addresses. Body reordering and count changes
travel with their own fields, contacts, cameras and generation identities. The
supplied live epoch advances only on complete exchange.

After exchange, the replacement object owns the entire old scene and its retired
per-body generations. The live manager still charges the external bytes and keeps
the exclusive lease. A timeout or failed retirement poll preserves those owners
and reservations. Only a successful last-use poll destroys old terrain/grass
resources, removes the external reservation and permits a new replacement.

## Renderer work remaining

The ownership API exchanges the complete scene payload. T3c3c2 must additionally
prepare and exchange the renderer's tracking arrays, reset/rebind character and
camera state, invalidate frame/shadow/reflection caches, preserve chase/replay
anchors and connect the reload adapter. T3c3c3 must exercise actual renderer
movement, rapid body switches and Moon arrival with matching consumed identities.
These remain separate acceptance tasks; owner tests do not enable interactive
compute or claim a transactional reload-key implementation.

## Validation

Seven native scene-owner cases cover delayed body readiness and retirement with
zero blocking polls; actual draws of old terrain/water/grass; complete scene,
disabled-water/grass and trail exchange; six per-body fence failures with complete
staging-buffer cleanup; whole-scene fence failure followed by retry in the same
transaction; failed retirement polling followed by recovery; eleven stale request
variants, superseded epochs and changed live revisions/contacts/config; invalid
configs, exclusive overlap and pre-dispatch admission; and six replacements with
changed body counts/order, fields and mounted cameras after old-scene destruction.

The build and all seven scene-owner cases pass (25.031 s). All 31 existing
native compute cases pass (48.264 s). All **59 CTest groups pass in one uninterrupted
final-binary run (731.72 s)**. All ten compute/CPU/replay PNG hashes match T3c3b
exactly and all 22 gallery hashes are retained.

[Evidence](validation/evidence.json), [full regression](validation/tests.log),
[seven scene cases](validation/native.log),
[31 native compute cases](validation/compute-native.log) and
[capture results](validation/capture-results.json) retain frozen source/input/binary
and artifact provenance. README, journal/PDF, layout and local links are updated.
Resume at T3c3c2; renderer reload and movement/Moon acceptance remain pending.

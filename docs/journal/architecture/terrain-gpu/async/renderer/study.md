# Renderer generation publication (T3c3b) — 2026-10-04

This checkpoint connects [complete transactions](../publication/study.md) to the
resident compute capture renderer, following the [asynchronous plan](../plan.md).
CPU, legacy compute grass and GL 3.3 fallback retain their existing paths.
Interactive compute stays gated; transactional scene reload/recovery is T3c3c
and interactive acceptance is T3c4.

## Frame boundary and character planning

The renderer owns `TerrainPublication` after grass in declaration order, so the
transaction and retiring resources are destroyed before grass, meshes and the
current context. Resident worker outputs for bodies requiring new terrain remain
off live resources. The capture adapter prepares complete land/water/grass/contact
consumers, explicitly waits through the same preparation APIs and publishes
before the actual character step, frame reuse decision or scene passes.

Grass needs the chase eye produced from terrain contacts. A character planning
preview uses prospective sparse contacts and intended revisions. A scoped restore
copies and restores CPU motion, contact binding/cache, cameras, selected bodies,
view, input presses and replay state, including exceptional exits. Planning
skips trail/exhaust mutation and replay-anchor consumption. The actual character
step occurs after publication and must produce the exact preview eye. This is a
capture planning adapter, not a new asynchronous interactive update loop or a
hardware performance result.

Grass and terrain anchors stay distinct. The terrain request retains its build
eye; grass uses the prospective chase eye or saved replay planning eye. Replay
anchors are applied in preparation rather than invalidating an already published
patch during character restore. Effects, trails, jump inputs and motion advance
once, in the actual character step.

Face-zone storage is allocated before GPU publication and then exchanged without
allocation. After transfer the renderer updates statistics/anchors, binds selected
contacts, invalidates terrain shadows for changed land and invalidates whole-frame
reuse. Shadow generation occurs before dependent scene draws. Reflection passes
consume the same immutable installed meshes and patch as the main pass.

## Grass-only replacement and retirement

Moving within a terrain rebuild threshold can cross the smaller grass rebuild
threshold. `submitGrass()` stages resident metadata/allocation/draw resources with
the same land, water, contact keys and revisions. It exchanges only grass and the
receipt at the boundary, leaving terrain handles and contacts unchanged. The old
patch remains owned until a last-use retirement fence signals.

Grass-only replacements use the same one-global-preparation/per-body-spare rules,
zero-timeout polling and aggregate admission. Their logical reservation
conservatively counts a complete set even though terrain is shared. Capture can
explicitly wait for retirement before reusing a slot; normal polling never waits.
CPU planning clears the stored resident key so telemetry cannot report an old
resident generation after a planner change.

## Consumed identity evidence

Managed scene passes validate installed land/water/grass/contact keys, revisions
and grass anchor before any dependent draw. They never independently prepare a
managed resident patch. Capture telemetry records actual main, reflection, shadow,
water and grass draw revisions, the installed request epoch/serial, source keys,
planning anchor and active contact binding. Disabled consumers are explicit:
empty water has an installed revision but no draw, and disabled shadows or
undrawn grass report no draw revision.

## Validation

Native owner tests cover grass-only transfer/retirement without terrain deletion
and four injected grass replacement failures. Renderer tests check consumed
main/shadow/reflection/water/grass identities after grass replanning, then inject a
complete replacement retirement-fence failure. Old terrain buffers and the prior
capture sidecar remain intact; the same renderer retries successfully and releases
its context on destruction. Exact surface/walking/replay capture checks also
assert consumed keys/revisions and preview/contact/replay-anchor consistency.

Two renderer cases pass (13.408 s). All ten compute/CPU/replay PNG hashes match
T3c3a exactly; the walking fixture checks six matching character previews and
land/grass/main/shadow/reflection revision 1 on both bodies. All 31 native cases
pass (52.055 s), including ten transaction cases. The full build and all **59 CTest
groups pass in one uninterrupted final-binary run (789.49 s)**. All 22 gallery hashes
are retained. No transactional scene reload,
asynchronous interactive compute, available-VRAM policy or hardware FPS acceptance
is claimed by this checkpoint.

[Evidence](validation/evidence.json), [full regression](validation/tests.log),
[31 native cases](validation/native.log),
[renderer recovery](validation/runtime.log) and
[capture results](validation/capture-results.json) retain source/input/binary and
artifact provenance. README, journal/PDF, layout and local links are updated.
Resume at T3c3c for transactional scene reload and movement/body-switch recovery.

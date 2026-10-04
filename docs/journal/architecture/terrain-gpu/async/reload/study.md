# Transactional renderer scene reload (T3c3c2) — 2026-10-04

This checkpoint connects the [whole-scene owner](../scene/study.md) to
`Renderer::reload()` and the shared R/file-watch boundary. It follows the
[asynchronous plan](../plan.md). CPU stays default; compute captures explicitly
wait and interactive compute remains gated. Movement/body-switch/Moon acceptance
is T3c3c3, preceding asynchronous interactive opt-in in T3c4.

## Preparation and publication

A fresh `SceneSource` snapshots configuration and replay together, validates
versions and resolves camera/time/quality inputs. Initial capture and reload use
the same stored replay snapshot: later file changes cannot alter an already
staged scene or its first character step. Backend/planner, capture dimensions,
offline mode, atmosphere resolution and lens-flare pipeline must remain unchanged;
a change is rejected while the current renderer remains usable. Foliage quality
transformations are applied to the effective CPU scene before terrain preparation.

Reload preallocates body tracking, face zones, terrain anchors and consumer
receipts. Compute topology/contact jobs use the existing persistent executor in
an exclusive future epoch. A failed build leaves the live epoch usable; successful
complete scene exchange advances it once. No extra worker or worker join occurs.
Normal CPU walking continues to use bounded asynchronous jobs and epoch checks.

A scoped CPU preview derives third-person chase anchors using prospective
contacts and replay pose, restoring old motion/contact/camera/input state before
GPU submission. Exact body-local terrain anchors avoid world/local round-trip
rounding. Replay trail and exhaust use shared parsers and production restore
validation before publication; previews do not advance these effects.

All resident bodies pass private complete GPU preparation before the tested owner
exchanges scene/config, land/water/grass/contact resources and receipts. CPU and
legacy compute prepare equivalent off-live mesh/grass owners and allocate a final
last-use fence before their exchange. After the final fallible operation, tracking
arrays and replay/options are swapped, cameras/contacts rebound, character/input/
effects reset, orbit data cleared and shadow/reflection/frame caches invalidated.
The next character step must match the prospective chase eye exactly; scene draws
validate matching committed keys and revisions.

## Retirement and failure recovery

The renderer retains one whole old scene until its last-use fence succeeds.
Resident overlap remains charged under the existing 512 MiB per-set/1 GiB
aggregate logical admission and exclusive replacement/retirement lease. Capture
can explicitly wait before the next draw/reload; interactive CPU retirement polls
with zero timeout. Shutdown destroys retired owners before live publication,
grass, meshes and context. Logical terrain admission is not total driver VRAM.

Any config/replay, CPU preparation or GPU/final-fence failure before publication
releases staged resources and preserves live scene/config/replay, buffers, contacts,
character and cameras. Reload does not overwrite capture files; the next capture writes them.
Failed future-epoch jobs do not prevent reuse of the same renderer or worker.
Retirement timeout or failure retains the old owner and reservation for retry.

## Validation

Five renderer cases check changed body count/field/camera and exact fresh-instance
capture parity; all six per-body preparation fences plus the final scene fence
with complete staged-buffer cleanup, old-scene draws and same-renderer recovery;
CPU and legacy compute invalid-config retention and exact fresh captures;
third-person replay anchors, malformed trail/exhaust and changed pipeline rejection,
file mutation after staging, exact image replay and later invalid-version rejection;
and optional surface-camera removal with repeated planet-orbit reloads.
A ninth controlled CPU scheduler case proves failed and successful future-epoch
preparation reuse one worker while the live epoch remains usable until exchange.

All five renderer reload cases pass (58.680 s), nine worker cases pass
(0.085 s), seven scene-owner cases pass (33.306 s) and all 31 native
compute cases pass (73.731 s). All **59 CTest groups pass in one uninterrupted
final-binary run (1051.70 s)**. Four changed/repeated reload images match fresh instances
byte for byte; replay images remain exact. All ten baseline compute/CPU/replay PNG
hashes match T3c3c1 exactly and all 22 gallery hashes are retained.

The seven-failure cleanup probe reuses warmed field parameters: those bounded
704-byte cache entries belong to the persistent compute helper. Every generated
generation-owned staging buffer is released on failure. Planet-orbit checks use
image parity because the existing capture path emits sidecars only for surface
views. Resident surface sidecars prove new epochs and matching consumers.

[Evidence](validation/evidence.json), [full regression](validation/tests.log),
[renderer cases](validation/native/runtime.log), [owner cases](validation/native/owner.log),
[native compute](validation/native/compute.log), [worker cases](validation/native/worker.log),
[capture results](validation/capture-results.json) and [reload results](validation/reload-results.json)
retain frozen source/input/binary and artifact provenance. README, journal/PDF,
layout and local links are updated. Resume T3c3c3; interactive compute stays gated.

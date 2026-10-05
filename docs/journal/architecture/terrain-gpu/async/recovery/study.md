# Movement and destination recovery — T3c3c3

This checkpoint extends the [renderer reload transaction](../reload/study.md)
with acceptance while walking, reordering bodies and arriving at the Moon. CPU
remains the default. Interactive compute stays gated until T3c4; compute captures
use explicit wait adapters. Mesa llvmpipe supplies correctness evidence only.

## Contact planning and handoff

A character preview must plan against every body's prospective contacts, not
only the departure body. On a destination change, the actual character binds the
destination's committed contacts in the same frame. Flight collision queries and
chase clearance use the matching triangle planes, including LOD sinking. Preview
restoration preserves live motion, contacts, cameras, input and effects before
GPU submission. Trails and exhaust advance only in the committed character step.

## Pending worker ownership

Future-epoch reload preparation supersedes queued and ready old work without
changing the live epoch. An executing old snapshot retains ownership until it
finishes; its result is discarded before exclusive replacement work proceeds.
The executor remains bounded to one running, queued and ready slot. A failed
replacement leaves the live epoch usable. These waits belong to the explicit
capture reload adapter; asynchronous interactive acceptance remains T3c4.

## Acceptance

Three actual renderer cases pass (73.010 s). The movement fixture advances one
metre per frame with a 1/12-second character step, exercising continuous 12 m/s
motion over thirteen frames. It crosses the 10 m terrain-rebuild threshold,
retains pose/sidecar/buffers after invalid config and an injected preparation
fence failure, then recovers through three body reorders and Earth field changes.
Old buffer names are tested alive before last-use retirement and deleted directly
after retirement, before later allocations could reuse names.

A ready old worker result is superseded through both a failed and a successful
replacement. Continued live movement after failure and the reordered replacement
record matching epochs and all draw/contact consumers; obsolete counts increase
from one to two with no stale renderer publication. Two controlled CPU cases
also cover running-plus-queued snapshots and ready-plus-queued backpressure.
All eleven worker cases pass (0.084 s), including failed future work followed by
live-epoch recovery on the same executor.

The Moon fixture starts from a captured jump pose, moves its inertial navigation
position near the Moon, then advances a real physics frame into destination
orientation. The departure Earth frame changes to Moon without teleportation;
Moon contacts bind at revision 2 on that exact handoff frame. The old binary
reported revision 0 against Moon land revision 2. Saved Moon pose reload, repeated
reload and a fresh instance reproduce the arrival PNG exactly. An injected reload
failure preserves the complete instantaneous pose; a following zero-time capture
retains navigation position and the effect clock while validating all consumers.
That continuation may recompute low-bit view/pose values; its image is not used
as an exact saved-pose replay check. Existing CPU space-flight capture and exact
Moon replay pass independently (42.37 s).

Draw-key and prospective/actual chase-eye validation run on each capture frame.
Retained sidecars represent the final frame of each run: four movement states,
two stale-work states and four Moon/recovery states. Each records matching main,
shadow, applicable reflection/water/grass and contact keys/revisions. The selected
astronaut contact revision equals its body's land revision. Three reload boundary
records retain 4,284,452–6,240,608 external old-scene bytes until retirement;
observed logical overlap peaks at 13,179,460 bytes across these fixtures, below the
existing 512 MiB per-set and 1 GiB aggregate limits. Published frame states have
zero external retirement bytes and no pending work.

All **60 CTest groups pass across two runs on frozen inputs
(952.49 s summed passing-group time)**. The first run was interrupted after
46 passing groups; the remaining 14 pass on resumption. Completed groups were
not rerun. Ten compute/CPU/replay PNG hashes match T3c3c2 exactly and all 22
gallery hashes are retained. Source, test inputs and five binaries remain
identical to their fingerprints taken before the run. Logical accounting excludes
total CPU snapshots, shader programs and driver physical VRAM; it is not an
available-VRAM measurement.

[Evidence](validation/evidence.json), [regression](validation/tests.log),
[renderer recovery](validation/native/recovery.log), [worker cases](validation/native/worker.log),
[CPU space-flight](validation/native/space-flight.log), [before-state proof](validation/before.json),
[recovery traces](validation/recovery-results.json) and [baseline captures](validation/capture-results.json)
retain the acceptance scope and source/binary/artifact provenance. README,
journal/PDF, folder limits and local links are updated. The user's environmental/
Docker TODO suffix is preserved byte-for-byte.

The next checkpoint is T3c4 in the [asynchronous plan](../plan.md): explicit
interactive opt-in with native movement/input coverage and zero normal-frame
waits. Hardware total-frame and physical-memory acceptance remains T3c5.

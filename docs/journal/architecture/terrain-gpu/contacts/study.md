# Sparse matching terrain contacts — 2026-10-03

This completes the contact prerequisite T3a of the [CPU–GPU plan](../plan.md).
The [T2 compute renderer](../compute/study.md) now installs a contact source made
from its canonical radial topology and immutable CPU field oracle. Compute
astronaut contacts no longer read the full CPU render mesh. Grass planning still
uses that compatibility mesh; the overall terrain migration remains in progress.
Interactive compute remains gated until the other T3 consumers are ready.

`SparseTerrainContacts` owns radial/sink samples, triangle indices and a radial
bounding-volume hierarchy. Building the hierarchy evaluates no heights, normals
or material fields. A query intersects the hierarchy with its outward radial ray,
sorts the small candidate set by original triangle ID, and evaluates only needed
positions. The existing `SurfaceContact` triangle cache and intersection equations
retain triangle-plane height, face normals and shared-edge tie behavior.

The position oracle uses the same radial × height formula, initial float
conversion, normalized inward sinking and final float conversion as terrain
evaluation. It avoids gradient/color calculations. At most 1,024 positions are
cached. The initial float conversion is explicitly materialized to prevent the
compiler retaining excess double precision across its round trip. Reaching cache
capacity clears it before another insertion. Resting
contacts reuse their triangles and positions. A step or teleport outside cached
coverage synchronously evaluates candidates from the index, so coverage does not
depend on the GPU generation latency or a guessed analytic ground height.
Sea-level clamping remains in the character consumer.
Camera startup and noncurrent-body flight floor probes retain their existing
analytic oracle; current-body feet and chase probes use matching triangle planes.

Hierarchy bounds enclose each triangle's positive radial cone. Bounds expand by
1e-6 body-radius units for the two float conversions and existing barycentric
edge tolerance. The source rejects stale field/topology keys, malformed topology,
empty surfaces and a conservative radius/relief/sink bound that can collapse
through the origin. That rejection happens before publication. CPU terrain retains
its existing mesh contact route; unsupported experimental contact inputs fail
clearly rather than install a mismatched generation.
An uncovered sparse query fails explicitly instead of substituting an analytic
height for a missing rendered plane.

The render thread constructs the source before publishing the completed
land/water pair. Mesh adoption checks its field/topology/backend key against the
GPU output before replacing any handles. Shared ownership keeps an old contact
generation alive until the character binds its replacement. CPU uploads and mesh
destruction release the mesh's source; rebinding and clearing release the
character's reference and invalidate cached triangle IDs. CPU workers and contact
queries issue no GL calls, and contacts never read shaped GPU buffers.

The new capture `render.terrain_contacts` diagnostics report the source backend,
generation keys, index construction time, candidate/node visits, height
evaluations, resident positions and topology/index bytes. Those bytes are logical
array sizes, excluding vector capacity, allocator and hash-table overhead. The
small cache adds memory, and the compatibility render vectors remain for grass.
This checkpoint does not claim a whole-process memory reduction or frame speedup.

Four CPU cases compare sparse contacts and position bits against rendered meshes
at poles, exact endpoints, mixed LOD, shoreline sinks, extreme-frequency noise,
Earth/Moon scales and metre-native units. A production 100,000-triangle case
checks index visits, lazy evaluation, capacity eviction, opposite-side coverage
and resting reuse. Additional checks reject stale/collapsed sources, preserve a
previous binding after a failed bind and verify generation lifetime/replacement.

A sixth native compute case compares sparse planes with actual GPU vertices,
poisons every CPU render coordinate to prove contact independence, rejects a
mismatched source without replacing the installed mesh, and checks switching back
to CPU. Capture integration checks matching contact/render keys, zero height work
without an astronaut, bounded walking queries and exact walking/trail replay.
Validation and measurements are retained under [validation/](validation/).

On the 100,000-triangle production topology, 262 globally distributed contact
queries evaluate 2,871 heights and visit 4,332 candidate triangles / 16,576 index
nodes. Only 823 positions remain resident after bounded eviction; a subsequent
100 resting probes require no new height evaluations or index searches. Logical
topology and index storage are 2,800,064 and 2,497,088 bytes. A serial diagnostic
run builds the index in 163.732 ms on this CPU; it belongs on the worker path in
T3c and is not an achieved interactive dispatch budget.

The six-frame walking fixture uses 14 height evaluations and resident positions,
with four index searches. Surface and walking PNG hashes match the dated T2
checkpoint exactly on llvmpipe. The full grass render mirror is still present,
so these sparse-query counts are separate from retained bulk evaluation work.

The clean build and all 58 CTest groups pass (664.17 s), including four
sparse CPU cases, six native compute cases and integrated replay/fallback/input
checks. Local links, layout, whitespace, gallery and artifact hashes pass. The
rebuilt 24-page journal was reviewed on pages 5–7 and 24.

Next is T3b: resident GPU triangle metadata and deterministic grass slot planning,
then removal of the full compatibility render vectors. Asynchronous complete
consumer publication, stale/reload/allocation recovery and hardware total-cost
gates remain required before interactive compute and the overall T3 task finish.

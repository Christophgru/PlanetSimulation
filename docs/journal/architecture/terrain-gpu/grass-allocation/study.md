# GPU grass allocation and compute mirror removal — 2026-10-03

T3b2 consumes the [resident metadata](../grass-metadata/study.md) with a versioned
GPU allocator. Fresh `--terrain-backend compute` captures use `gpu-v1`; CPU
terrain remains the default. Land and water compute builds now carry topology
and statistics without evaluating or retaining full CPU float render vectors.
Draw index counts are independent of those vectors. Sparse matching contacts
continue to own their canonical topology and bounded position cache.

The allocator reads triangle eligibility and double Gaussian-weighted areas from
resident buffers. Sixty-four-lane groups reduce eligible count, total area and
rounded-slot work. A GPU controller combines the small group summaries; no
triangle metadata returns to the CPU. If eligible patches exceed the effective
candidate budget, a 32-step integer search selects the lowest original-triangle
hashes. The xor-shifts and odd multiplication in `grassHash` form a bijection,
so distinct triangle IDs have distinct hashes and selection has no tie race.

If requested density exceeds the rounded work cap, a 32-step double search lowers
it. Each probe uses the same strict power-of-two comparisons and floored slot cap
as the CPU planner. Work sums use doubles to represent integer probe counts beyond
32-bit range. The final candidate total must fit the effective budget before any
references are scattered. Empty eligibility yields zero density/work.

Final groups compute per-level counts and local ranks. The GPU controller writes
ordered group offsets and 17 batch prefixes, then a scatter produces the resident
triangle-ID buffer. Each bucket is ordered by original triangle ID; threads and
atomics do not decide which patches receive capacity. This differs from the
legacy CPU planner's distance order, so the saved planner is explicitly `gpu-v1`.
Root identity remains triangle ID xor seed plus candidate rank. The rendering
kernel still evaluates actual density, biomes, view culling, wind and near/far
geometry per root. Main, shadow and reflection views share the same allocation.

Allocation uses seven storage bindings, with queried field-compatible block and
work-group limits. Resident source sizes, nonempty/known generation keys and
versions are checked before allocation. Group dispatches are capped at 1,024
work groups and obey the queried maximum. The effective new-mode candidate budget
is the smaller of configured `max_blades` and queried block bytes divided by 128,
covering two 64-byte output queues per candidate. This is a conservative allocation
policy, not a measurement of available VRAM or achieved near-camera density. The
legacy CPU planner keeps its existing capacity policy for replay compatibility.
The [shader-storage specification](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_shader_storage_buffer_object.txt)
describes storage limits, layouts and barriers; its limit is not a total VRAM
budget. Dynamic memory/usage/density policy remains B1 in the architecture plan.

Placement dispatches also obey queried group counts and preserve global candidate
IDs across chunks. New-mode placement caps a chunk at 65,536 candidates; legacy
placement retains its previous dispatch size when it fits the queried limit.
Storage barriers separate producers, reductions, controllers, scatter and
compaction; vertex/indirect barriers precede draws. GL program and indexed storage
ranges restore after allocation submission. RAII releases temporary and resident
buffers and fences. A failed standalone allocation cannot replace an earlier
owned output. Complete asynchronous multi-consumer recovery remains T3c.

Compute captures deliberately wait for allocation completion and read one
224-byte summary containing density, budget, counts, offsets and validation
status. There are no terrain, metadata or reference readbacks in the renderer.
The summary API supports polling for a later asynchronous installer. Tests alone
read references for validation. Cached preparation uploads and reads nothing.
Capture metadata reports both stages' input/resident bytes and dispatch counts,
the summary read bytes, effective budget, placement density and CPU land/water
render-vector bytes. GPU-generated references have zero CPU patch-upload bytes.
GPU preparation statistics use conservative six-segment work bounds; actual main
view near/far blade counts remain independently reported by capture diagnostics.

A new compute generation performs no bulk CPU height/normal/material evaluation;
CPU topology planning/classification queries and sparse contact height probes
remain. The radial topology/BVH is a CPU consumer, not a full shaped-render mirror.
Water adopts its GPU draw buffers with its own index count. Navigation captures
use field/topology/version/backend keys for resident generations, rather than a
hash of missing CPU float data; legacy captures retain their mesh-byte hash.

Saved compute sidecars without `terrain_grass_planner` select the CPU compatibility
planner, preserving older replay. `--terrain-grass-planner cpu` explicitly selects
that route; `--terrain-grass-planner gpu` opts into the new allocator. Unknown saved
planner versions reject before window creation. An explicit terrain CPU override
retains the old fallback behavior. Fresh compute requests whose enabled foliage
uses `compute_placement: false` retain CPU grass planning and render vectors;
a locked new-mode replay rejects that incompatible configuration. GL 3.3 fresh
compute requests fall back to CPU terrain; locked compute replays still reject.
Interactive compute remains unavailable until T3c.

Four new native cases compare slots, density, prefixes and exact reference order
against an independent CPU oracle. They cover exact/adjacent power boundaries,
non-power slot caps, partial groups, forced single-group dispatches, conservative
buffer-limited budgets, one/two/seven-patch hash selection, empty eligibility,
invalid/truncated sources, zero groups/tiny buffers, previous-output preservation,
repeat determinism and a 100,000-triangle preparation with no CPU render vectors.
Capture integration checks zero bulk CPU evaluation, zero land/water render
vectors and patch uploads, scalar-only readback, hard caps, exact compute/walking
replay, legacy planner replay, explicit CPU override, GL 3.3 fallback and unknown
planner rejection.

The native 100,000-triangle case retains 13,019 eligible patches and submits
1,048,429 candidates within the conservative 1,048,576-candidate limit. Metadata
and allocation upload 2,840 bytes; metadata and allocation logical storage are
6,400,160 and 1,456,556 bytes, excluding shared terrain and eventual blade queues.
The CPU render mirror formerly held 12,000,000 bytes for land alone. These are
per-generation logical array sizes; replacement overlap, driver allocation,
physical VRAM, CPU BVH/topology/caches and total-frame cost remain separate.
The small surface fixture matches CPU pixels exactly on llvmpipe and both compute
surface/walking replays pass. This checkpoint claims no hardware FPS improvement.

Final validation on October 4 covers all 58 CTest groups on the rebuilt executable:
groups 1–29 passed before interruption, and groups 30–58 passed in the resumed
run (785.19 s). The sum of passing group durations is 883.03 s; this is grouped
validation, not a single uninterrupted sweep. The native integration group
contains 14 terrain/metadata/allocation cases. The six-case renderer lifecycle
group also checks camera-only replay without a `render` object, including an
explicit GPU-planner override. Capture, atmosphere, shadow/reflection, character,
wind, live input and quality groups all have passing results.

The retained four-case allocation log and exact 1,999,246-candidate legacy gallery
replay were regenerated on the final binaries to give direct provenance; both
binary identities are recorded. Final-binary capture integration reruns the
GPU/legacy replays, compatibility fallback, hard-cap/transfer assertions and
rejection cases. All 22 gallery hashes remain unchanged. Layout, 86 local document links
and whitespace pass. The rebuilt 25-page journal was reviewed on pages 5–7 and
24–25; revised architecture pages were reviewed again.

Validation evidence is retained in [validation/](validation/). The overall GPU
terrain task remains in progress: the [T3c implementation plan](../async/plan.md)
must move topology/contact indexing onto
workers, asynchronously publish complete consumers, handle stale/body/reload/
allocation cases and pass hardware total-cost gates before default enablement.

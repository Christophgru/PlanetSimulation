# Relief-aware terrain sinking

## Retained GPU position targets — 2026-10-17

A standalone, opt-in compute primitive can now move a detail blend footprint
without evaluating the procedural field again. `TerrainMorphStorage::Transient`
remains the default; no runtime caller enables retained storage. Retention requires
a detail topology. Its optional shader variant evaluates the existing generation,
stores fine positions and exact projected parent targets, then repacks scratch
after ordinary corner expansion. The existing 72 bytes per canonical sample hold:

| Region | Data |
|:--|:--|
| First 36 bytes/sample | Radial double3 and fine float3 |
| Second 36 bytes/sample | Projected parent double3 and reserved words |

The storage uses uint words for double packing. This preserves low words that
would be NaNs if treated as numeric floats. The test verifies every radial bit,
including 218 such low words in the fixture. Fine positions retain the existing
float displacement contract; this does not introduce a new large-planet precision
claim. Parent/radial data acquire no additional packing quantization.

`Mesh` optionally takes the unique-target and canonical-corner-map buffers from
preparation. Upload/fallback, destruction and swap release or transfer both.
No extra GPU buffer storage is allocated during generation, and admitted working
bytes match transient generation. **More storage remains live** afterward:
`72*unique_samples + 4*corners`, or 1,877,712 bytes (1.79 MiB) in the 39,116-face
fixture. A closed 100,000-face sphere retains 4,800,144 bytes (4.58 MiB).
The existing conservative ledger already charges these buffers through generation
ownership; optional shader programs are separately cached like the existing
compute variants. Retention is not a general memory reduction.

A small deformation shader reads these targets and updates only VBO positions.
It has no field/noise inputs, allocates no buffers, performs no readback or wait,
and preserves normals, colors, indices and cell identities. It restores program,
SSBO ranges and generic binding, with barriers for vertex, storage and texture
consumers. The shader is warmed during retained generation on the same compute
instance. Invalid footprints, legacy/nonretained meshes and CPU mirrors are
rejected. Meshes with bound contacts are also rejected until a shared deformation
phase is implemented; the application therefore cannot accidentally enable this
partial path for its live terrain consumers.

The focused RTX test moves the center through 0/0.05/0.15/0.30/0.15/0 m, comparing
every output position with independently reweighted CPU topology. This fixture
reports zero position difference while retaining the established 2 mm tolerance.
Shared vertex positions remain bit-identical, attributes/indices are unchanged,
and static retained/transient output matches exactly. The test releases original
preparation, checks target survival, SSBO range/program restoration, invalid-input
and contact guards, swaps, destruction and CPU fallback. Final bit-preservation
check passes in 1.94 s; the earlier focused test passes in 2.20 s.

The full rebuild, all 35 core groups (41.67 s), all 48 RTX cases (79.08 s), frame
integration (93.05 s), expanded 15-render replay/fallback matrix (105.65 s) and
render inspection pass. All fifteen historical PNGs and the overview are
byte-identical: 14,779 non-background pixels, bounds (1,1)–(794,598), 3,471 planet
pixels. The final added bit assertions were checked with the focused case; no
production code changed after the full suite. The first GPU attempt caught a
reserved GLSL identifier, corrected before these passing checks.

This is a position-deformation primitive, **not runtime temporal publication**.
Shared contacts/grass/view/capture phase, prepared-patch streaming, and parent
rebase continuity remain required. No new live-coverage or FPS result is claimed;
the previous moving gate failure remains open and terrain stays **p**. Raw logs
and counters remain local under `build-f5/resume/detail-targets-*`.

## Check prepared fine overlap before temporal deformation — 2026-10-17

An isolated CPU probe compares exact radial triangle sets across five short moves,
turns and a return, using the same parent and field. With the existing 0.5 m fine
core, 1,580–3,532 triangle references change within the surrounding 1 m region
in the oblique/pole fixtures. A larger prepared region is needed before moving
that blend domain independently of topology publication.

With a 1.5 m fine core and a 100,000-triangle cap, every tested triangle in the
interior 1 m region is preserved in both directions across these moves. All
checked corners remain fully fine and unsunk; parent/fine policies match and
quality targets are met. The probe uses about 39,100 triangles at the oblique
pose and 29,700 at the pole. A 2 m core also preserves the same interior, using
about 58,900/42,400 triangles. This is a geometry precondition, not a claim that
larger regions always fit the cap or improve planning speed.

A new automated regression retains the 1.5 m case, exact bit-pattern triangle
comparison, both locations, turns/return, field-policy equality, unsunk corners
and budget/quality checks. The focused case passes in 4.05 seconds and the full
thirteen-case detail suite passes in 8.33 seconds. The runtime
still uses one radius for tessellation and spatial morphing: this test does not
activate a preparation halo, moving blend center or temporal publication.
Contacts, grass, all render views and capture state must share any later
deformation phase. The existing moving coverage gate and terrain **p** remain.
Raw probe source/CSVs and test logs stay local under
`build-f5/resume/detail-overlap*`.

## Share validated topology with GPU staging — 2026-10-17

One resident generation previously copied its descriptors into the contact oracle,
then revalidated and rehashed the staging copy during admission, GPU-stage
construction and compute submission. Those repeated scans run on the render
thread. In the fresh before cohort they account for 192/210/45 validation and
fingerprint calls, totaling 1,300/1,425/585 ms across the three native routes.

`TerrainTopologySnapshot` now owns validated immutable descriptors. A compute
worker moves its topology into `SparseTerrainContacts`, whose constructor still
performs full validation. It then stages an alias of that private topology under
the same shared owner. Contacts cannot be copied, moved or assigned, so the
aliased address and descriptors stay stable; lazy position queries only modify
the contact cache. The alias retains the oracle through worker/staging lifetimes
and releases it normally when its final owner retires. Derived contact-parent
maps are stripped before aliasing, as in the existing oracle construction.

Water gets a separately validated owned snapshot. Admission retains its complete
identity, consumer and memory checks. A typed compute overload consumes these
snapshots without repeating full scans. The mutable `TerrainTopology` compute
entry still validates every input; constructing a snapshot from a raw DTO also
validates it. Canonicalization still checks structure and fingerprints plans on
the worker. CPU compatibility evaluation retains its existing validation.
No descriptor, shader, capture format, quality target or GPU allocation changes.
One bulk CPU staging copy is removed; no retained value cache is introduced.

An isolated RTX before/after pair uses identical input JSON for each route:

| Route | Warm publication before (ms) | After (ms) | Render-thread validation/fingerprint calls |
|:--|--:|--:|--:|
| 0.5 m walking | 141.80 | 98.09 | 192→0 |
| 0.5 m sprint | 112.85 | 90.44 | 210→0 |
| 1.5 m sprint | 390.56 | 354.19 | 45→0 |

Medians exclude the initial main-body generation. These improvements are about
31%, 20% and 9% in this single traced pair, not a sustained FPS claim. Sampled
process RSS peaks are mixed: 205.4→206.0, 143.0→137.6 and 157.5→152.4 MiB.
RSS includes driver/allocator pages and does not directly measure descriptor
ownership. Both cohorts retain at most one queued/running/ready job, zero bulk
readbacks, blocking polls, finishes and server waits, with logical GPU overlap
below 75 MB.

The unchanged coverage gate still exits 2. Maximum offsets improve in this pair
1.605→1.121, 2.411→2.282 and 8.918→8.085 m, but the after cohort remains outside
its cores in 496/534, 672/690 and 792/829 measured frames; 579 wide-core frames
miss geometry targets. Frame counts differ, so absolute counts are not a coverage
speedup metric. This does not complete uninterrupted moving detail or temporal
publication; terrain remains **p**.

The complete 121-step rebuild, all 35 core groups (30.18 s), all 47 RTX cases
(77.75 s), frame integration (147.46 s), expanded 15-render replay/fallback matrix
(107.86 s), and recovery/reload checks (162.87 s combined) pass. New CPU tests
cover immutable access, corrupt DTO rejection, alias lifetime through producer
release and lazy contact queries, and final owner destruction. GPU tests compare
all position/index/diagnostic-height buffers exactly and reject stale fields,
empty snapshots and mutated DTOs. Existing memory-admission and failure tests
remain active. All fifteen earlier images and the inspected overview are
byte-identical: 14,779 non-background pixels, bounds (1,1)–(794,598), 3,471 planet
pixels. Raw results stay local under `build-f5/resume/detail-snapshot-*`.

## Rejected retained-value cache pilot — 2026-10-16

A bounded pilot reused fixed fine samples and intrinsic triangle error/edge
metrics across plans. Internal stable midpoint IDs identified cached values;
selection and current-plan IDs were still reconstructed, preserving deterministic
replay. It capped samples at `2*budget+2` and metrics at `2*budget`, bypassing
insertions on saturation and resetting before the next plan or parent/policy
change. All twelve production and six wide quality/contact fingerprints stayed
exact. Repeat height-cache requests fell 75,353→0 and a 0.15 m move's requests
75,359→4,551, with actual field evaluations unchanged.

Three alternating pairs give warm production changes −14.7%, −14.3%, +0.2% and
−11.0%; warm wide cases improve 2.0–16.3%. Cold wide planning is 17.0% slower.
Wide process RSS rises 34,716–34,772→46,128–46,524 KiB; full production-matrix RSS
is lower (63,552–63,908→60,572–60,716 KiB), but that high-water measurement includes
legacy plans and cannot establish a reduction in retained-cache memory.

Native coverage worsens in this single traced cohort: max offsets are
1.668/3.339/11.050 m, outside counts 378/402, 458/479 and 716/750 frames, with 530
wide geometry misses. The unchanged gate exits 2; nonblocking/ownership checks
still pass. The pilot is **rejected**: these mixed warm results do not justify
its cold, memory and end-to-end costs. Its cache and associated test changes are
reverted. The optional production benchmark retains appended height request/hit
columns to distinguish lookup work from actual evaluations in future studies.

Before rejection, all thirteen detail cases, 35 core groups (59.00 s), 46 RTX
cases (77.54 s), frame integration (93.88 s), 15-render replay/fallback matrix
(73.85 s), all fifteen historical image comparisons and the visually inspected
overview passed. Raw pilot patch and receipts remain ignored/local under
`build-f5/resume/detail-refinement-values-*`. This adds no task chain or completion
claim; retained refinement and temporal publication remain outstanding.

## Recycle refinement nodes and footprint calculations — 2026-10-16

The instruction profile identified repeated edge-node allocation/free during
longest-edge splitting. The existing keyed adjacency map now uses a standard
per-plan `std::pmr::unsynchronized_pool_resource`: at most 1,024 blocks per chunk,
small blocks up to 128 bytes, large bucket arrays allocated upstream. Erased
nodes can serve later child edges; map destruction precedes resource destruction,
and all chunks are released when the plan returns. No mutable allocator is shared
between workers or retained by published geometry. Live graph size still obeys
the existing 3/2-edge-per-face bound. No new dependency is required.

Local refinement also reuses its early ground-footprint distance when assigning
core/transition priority. It assigns that distance only after the original
world-distance guard; high-altitude faces outside the error domain continue to
report no completed local detail. Nonlocal calculations retain their previous
order. Both changes preserve split ordering and all rendered descriptors.

Three alternating CPU pairs preserve every non-timing column in all twelve
production rows and six wider working-set rows, including topology/candidate
fingerprints, query counts, edge/error limits, shared-index sizes and transfer
bytes. Median warm thread CPU planning times are:

| Pose | Offset | Before (ms) | After (ms) | Change |
|:--|--:|--:|--:|--:|
| Startup | 0.15 m | 85.47 | 79.62 | −6.8% |
| Startup | 0.30 m | 95.63 | 72.51 | −24.2% |
| Mountain | 0.15 m | 107.27 | 92.90 | −13.4% |
| Mountain | 0.30 m | 110.32 | 88.30 | −20.0% |

Warm wide-core medians improve 1.2–22.1%. Cold results are mixed: startup streaming
improves 4.5%, mountain streaming worsens 6.8%, and historical full-rebuild rows
range from 1.7% faster to 7.2% slower. The six-plan wide process peak RSS increases
30,716→34,540 KiB in a separate before/after sample. Pooling trades CPU memory for
less allocation work; this is not a reduction in all planning costs.

A single traced native RTX cohort reduces warm main-body publication medians
149.99→119.05 ms walking, 121.01→117.87 ms sprint and 467.52→417.29 ms wide sprint
(excluding the initial generation). Splitting medians fall 36.86→25.68,
34.08→32.66 and 288.78→234.61 ms. The unchanged coverage gate still exits 2:
maximum offsets are 1.183/2.745/9.079 m, with 352/375, 389/412 and 686/721 frames
outside the requested cores; 483 wide frames miss geometry targets. Ownership
and nonblocking checks pass: no blocking polls, bulk reads or finishes, with peak
logical GPU overlap below 75 MB. This does not establish sustained FPS or completed
moving coverage. Retained refinement and temporal publication remain open.

Full build, 35 core groups (39.04 s), all 46 RTX compute cases (78.78 s), frame
integration (96.05 s) and the expanded 15-render replay/fallback matrix (70.97 s)
pass. The inspected overview is byte-identical: 14,779 non-background pixels,
bounds (1,1)–(794,598), 3,471 planet pixels. No new tests mirror allocator details;
the existing independent geometry, contact, altitude, publication and replay
oracles exercise the changed paths. Raw CSVs, memory sample, logs and native
receipts stay local under `build-f5/resume/detail-refinement-overhead-*`.

## Refresh after vertical movement — 2026-10-16

Adaptive planning uses distance to the eye, but both runtime refresh paths only
measured angular travel. Pure descent from 500 m to 2 m submitted no new job
(the before-fix counter remained 2→2). Even an explicit planner call retained
its high shell because parent reuse also ignored height: 1,992 parent faces
instead of the fresh low shell's 3,048. Ascent can bring ridge surfaces closer
as well, so the correction applies in either radial direction.

Both schedulers now combine their existing angular distance with absolute radial
movement when geometric-error refinement is enabled. Streaming parent reuse
bounds radial movement by the existing half-near-band reserve. Parent topology,
adjacency, contact index, query cache and sink memo rebase together. Other runtime
refresh behavior is unchanged. Historical recorded parents still apply to the
next plan exactly once; later ordinary calls resume normal height-aware planning.
No GPU buffer, descriptor, quality target or capture version changes are needed.

New CPU tests verify descent/ascent against fresh plans in metre and kilometre
world units, recorded-parent compatibility, and the high-altitude distance guard
that prevents unassessed faces from counting as completed detail. GPU tests cover
whole-mesh and rendered-plane contact parity, with maximum new position difference
0.0596 mm. Frame tests publish complete high/low/high generations through resident
GPU, asynchronous CPU and synchronous capture CPU paths. The GPU path records
zero bulk readbacks, blocking polls and server waits, with one queued/running job
at a time and peak reserved overlap 7,086,400 bytes in this fixture.

Full build, all 35 core groups (36.11 s), 46 RTX compute cases (72.69 s), full
frame integration (134.13 s), and an expanded 15-render capture matrix (65.25 s)
pass. The matrix adds historical high-parent rendering/replay, CPU and GL 3.3;
replay/fallback images are exact and GPU/CPU mean difference is 0.00003665 against
the unchanged 0.1 limit. All eleven earlier capture images and all twelve
production topology/work/index fields remain exact. The inspected overview stays
byte-identical: 14,779 non-background pixels, bounds (1,1)–(794,598), 3,471 planet
pixels. The historical high-parent view establishes compatibility, not live quality.

Height changes can now schedule additional adaptive work; these checks establish
correct refresh/publication, not an FPS gain. They do not resolve the moving-core
coverage failures or add temporal publication. The terrain row remains **p**.
Raw witnesses and results stay local under `build-f5/resume/detail-vertical-*`
and `detail-descent-before.log`; capture outputs remain in the integration folders.

## Reuse fixed-policy sink descriptors — 2026-10-16

Local-detail export used to repeat sink calculations for every vertex/probe,
including those already evaluated by `sampleAt`. New samples now retain their
exact computed sink. Original parent samples memoize their fine-policy sinks with
the existing refinement parent, checked against the complete policy and field
fingerprint. Initialization/use is serialized by `streamDetail`'s existing mutex;
rebasing drops the memo. It holds values rather than borrowed field references.
The same normalized eye also supplies footprint and transition distances.

The payload costs eight bytes per original parent vertex: 200,016 bytes for the
production 50,000-face shell, at most 400,016 bytes under the 100,000-triangle cap,
plus small policy/container overhead. Only the current refinement parent retains
this memo; published topologies contain ordinary sink descriptors. GPU buffers,
admission, noise sampling, triangle selection, quality targets and replay formats
are unchanged. The cold fill uses the same float sink conversion as ordinary
samples, including historical recorded-parent captures.

All twelve production topology/candidate fingerprints and geometry/work/index
fields remain exact. One isolated CPU cohort gives these warm planning times:

| Pose | Offset | Before (ms) | After (ms) |
|:--|--:|--:|--:|
| Startup | 0.15 m | 101.60 | 90.78 |
| Startup | 0.30 m | 100.63 | 93.29 |
| Mountain | 0.15 m | 127.14 | 116.00 |
| Mountain | 0.30 m | 120.43 | 109.45 |

This saves about 7–11 ms of warm planning in this cohort. Cold times remain
roughly unchanged (1,767→1,769 ms startup, 1,404→1,393 ms mountain). Three
alternating wide-working-set pairs also retain every fingerprint/query/limit
field, but total CPU times are mixed; no wider performance gain is claimed.

Native RTX export scopes fall from 2.90→1.68 ms walking, 2.95→1.24 ms sprint,
and 8.41→4.77 ms wide sprint. Total publication is worse in this single traced
cohort: 114.98→149.99, 118.76→121.01 and 415.90→467.52 ms, respectively.
The unchanged gate exits 2, with maximum offsets 2.105/2.605/13.441 m and
298/314, 384/403, 592/626 frames outside their cores; 427 wide-core frames also
miss geometry targets. These results do not establish a native FPS or moving
coverage gain. Nonblocking/ownership checks pass, with zero blocking polls,
bulk reads or finishes and logical GPU overlap below 75 MB.

All eleven detail cases, seven contact cases, all 35 core groups and all 45 RTX
compute/consumer cases pass. Moving/rebased/recorded-parent replay and CPU/GL 3.3
captures pass, seven earlier images remain exact, and the inspected overview is
unchanged (14,779 non-background pixels, bounds (1,1)–(794,598), 3,471 planet
pixels). A separate Callgrind instruction profile completes all six working-set
requests with matching geometry/work fields; it records 1.426 billion instruction
references, including 11.8% self references in topology fingerprinting and
substantial reassessment/allocation work. Instrumented times are not performance
evidence. Raw outputs remain local under `build-f5/resume/detail-sink-*` and
`detail-refinement-callgrind*`. Retained moving refinement and temporal publication
are still required; terrain remains **p**.

## Fit a complete streaming probe working set — 2026-10-16

The 1.5 m native fixture exposes a cache sizing error: its 30,000-triangle budget
allowed more than 75,000 fine-field probes, while the cache held only 60,000.
An identical stationary repeat therefore evaluates 75,353 heights with zero hits;
a 0.15 m move evaluates 75,168 with only 191 hits. The cache flushes before it can
retain one full plan, even though topology/field policy are unchanged.

The streaming-only limit is now `4*triangle_budget+2` entries. For a closed parent
with F faces, at most (N−F)/2 shared-edge splits fit the N-face cap. Original
vertices plus all edge probes fit `2*N+2`; at most `2*N` triangle-centroid probes
are assessed. The old capacity omitted the centroids. New limits are 120,002 at
30k and 400,002 at the maximum 100k cap. Allocation remains lazy, batch eviction
bounded, and parent replacement drops the cache/owned field together. This is
additional CPU memory; GPU buffers, quality targets, default settings and field/
topology/shader contracts are unchanged. Long movement can still evict probes.

The regression now repeats with zero field evaluations. The 0.15 m move evaluates
4,551 heights with 70,808 hits; generation, samples and indices match fresh-parent
planning exactly. The optional benchmark's `--working-set` mode reproduces this
independently with offsets 0, 0, 0.15, 0.30, 0.30 and 0 m. Three alternating
before/after pairs, identical helper/RelWithDebInfo flags with the frozen 181c5dc
planner object as baseline, give these median CPU planning wall times:

| Request | Before (ms) | After (ms) |
|:--|--:|--:|
| Cold | 523.01 | 510.89 |
| Same anchor | 242.09 | 203.07 |
| Move 0.15 m | 248.56 | 207.56 |
| Move 0.30 m | 263.08 | 216.27 |
| Repeat 0.30 m | 236.53 | 211.81 |
| Return to start | 241.38 | 212.96 |

Warm improvement is 10–18% in this bounded stationary/short-move cohort. All six
fingerprints and geometry/limit/request fields match in every pair. The fixture's
geometry limit remains visible. All twelve short production rows also retain
their topology/candidate fingerprints, work, input and index-size fields; those
0.5 m plans do not exhaust the previous cap and serve as parity controls.

This does **not** fix sustained native movement. The single traced RTX route
pair is mixed: walking publication 118.01→114.98 ms, sprint 113.15→118.76 ms,
1.5 m sprint 387.49→415.90 ms. Maximum offsets are now 1.219/2.593/9.142 m;
385/407, 391/411 and 734/771 frames lie outside their cores, and 515 wide-core
frames also miss geometry targets. The gate still exits 2. Periodic process RSS
peaks are 150.1–162.4 MB versus 147.9–157.3 MB in the earlier run (allocator,
driver and worker pages included; samples can miss peaks). Logical GPU overlap
remains below 75 MB, with zero blocking polls, bulk reads or finishes. No native
FPS or continuous-coverage gain is claimed. The helper now explicitly freezes
the existing 300/1,000 m bands; all three generated inputs match prior inputs.

All eleven detail cases, all 35 core groups, and all 45 RTX compute/consumer cases
pass. Moving/short-band/recorded-parent replay and CPU/GL 3.3 captures pass; seven
earlier images remain byte-exact. Overview inspected and unchanged (14,779
non-background pixels, bounds (1,1)–(794,598), 3,471 planet pixels). Raw evidence:
`build-f5/resume/detail-probe-capacity-*`, `detail-probe-working-*`. Continuous
moving detail and temporal publication remain outstanding in the terrain **p** row.

## Share coarse contact indices between detail plans — 2026-10-16

V4 now retains an immutable coarse contact index with its parent shell. Local
refinement exports each live triangle's original parent face. Untouched faces
map parent IDs to current IDs; only descendants of split faces enter the new
generation's local index. Query both trees and sort current triangle IDs before
testing rendered planes, preserving shared-edge/pole tie rules. Longest-edge
children lie in their original parent's radial cone, and displacement/morphing
remains radial; existing direction padding still covers float conversion.
Broad-phase false positives can change, so candidate-list fingerprints change;
exact contacts are compared against an independently rebuilt full index.

The acceleration is private CPU metadata, outside the GPU/replay contract and
generation hash. Public canonicalization clears it; a separately resealed changed
contract also bypasses it through the stored topology hash. Parent field/policy
must match. Shared traversal is const and updates only the querying generation's
counters. Parent replacement prepares the new shell/indices before installation;
old contacts retain their own immutable backing across rebases and scene/snapshot
destruction. No new worker, GPU input or interactive readback is introduced.

In the production cohort, a 50,000-face parent is shared while local trees contain
4,447–7,173 faces instead of rebuilding all 54,426–57,142. Shared coarse topology/
index backing costs 2,648,704 bytes plus a 200,000-byte current-face map per plan.
Per-owner index counters conservatively include shared nodes: 1.266–1.277 MB
before versus 1.577–1.608 MB after. Topology counters also charge shared parent
topology; summing owners does not measure unique RSS. Running scratch and derived
CPU acceleration remain outside the worker's ready-vector diagnostic; process RSS
includes them. GPU admission and descriptor bytes are unchanged.

Two before/after production CPU cohorts (GCC 12, RelWithDebInfo) retain all twelve
topology fingerprints and geometry/work fields exactly. Warm contact preparation
falls from 80–93 to 44–48 ms. Include planning and contact copies/validation when
assessing the complete CPU cost:

| Pose/phase | Before plan + contacts (ms) | After (ms) |
|:--|--:|--:|
| Startup, warm moving | 187–194 | 142–147 |
| Mountain, warm moving | 179–186 | 165–171 |
| Startup, cold | 1,807–1,808 | 1,797–1,822 |
| Mountain, cold | 1,399–1,413 | 1,440–1,474 |

The parent index is built during cold planning, so it is not a cold-start win.
Pure warm mountain planning also rises from about 100–104 to 118–123 ms in these
cohorts; the contact saving still improves the combined warm cost. No FPS gain or
continuous-coverage acceptance is inferred from these CPU timings.

In the native RTX walking diagnostic, median contact-index construction falls
19.82→10.84 ms, full CPU build 63.05→54.17 ms, and request-to-publication
138.39→118.01 ms. Sprint publication is 127.66→113.15 ms; the 1.5 m fixture
is 417.31→387.49 ms. These traced single-route comparisons retain profiling
overhead. The unchanged gate still exits 2: maximum anchor offsets are
1.210/2.389/8.665 m, with 423/446, 408/429 and 725/765 frames outside their
cores. The 1.5 m case also misses geometry in 554 frames. Ownership/nonblocking
audits pass with zero blocking polls, bulk reads or finishes and logical overlap
below 75 MB. Moving coverage and temporal publication remain outstanding.

All 35 core groups and all 45 RTX compute/consumer cases pass. Seven contact
cases cover full-mesh equality, shared/fresh-index equality across movement and
rebasing, manual/canonical invalidation, surviving old snapshots, and a
base-budget fixture with an empty local tree. Moving/short-band/recorded-parent
replay and CPU/GL 3.3 captures pass; all seven earlier capture images remain exact.
The inspected overview is byte-identical (14,779 non-background pixels,
bounds (1,1)–(794,598), 3,471 planet pixels).

An isolated phase-2 shader pilot additionally shared repeated parent evaluations.
Three warm RTX image pairs are exact, but median generation changes only
1.1295→1.1336 ms in the micro fixture and 5.1466→5.1057 ms in a production
startup fixture (~0.8%). Cold warmups are excluded. That shader change is not
retained; current shaders are unchanged. Raw evidence stays local under
`build-f5/resume/detail-contact-parent-*` and `detail-parent-rtx-{pilot,production}`.

## Cache triangle metadata during contact-index construction — 2026-10-16

NVIDIA is available again (RTX 3070 Ti, UUID
`GPU-1795f01b-f1fd-61a0-3b79-199560dd0793`). The native traced movement check
still fails, so this continuation stays within the terrain **p** row. Its first
walking trace spends a median 28.40 ms building the sparse contact index out of
70.40 ms for the full CPU build. Median request-to-publication time is 146.45 ms;
parent reuse alone does not deliver continuous moving detail.

The contact-index builder now computes each triangle's exact radial bounds and
unscaled centroid sum once. Median partition comparisons read that temporary
metadata instead of repeatedly fetching three indirect vertices. Split axes,
padding, triangle-ID tie rules, leaf size, candidate ordering and lazy displaced
positions are unchanged. Scratch costs 72 bytes per triangle (7.2 MB at the
100,000-triangle cap), exists only during the worker build, and is released before
publication. Retained index sizes and GPU admission are unchanged. No bulk field
evaluation, extra retained generation or new worker is added.

One isolated before/after production planning cohort, RelWithDebInfo/GCC 12,
includes owned contact-topology copies, contract validation and index building:

| V4 pose | Step | Contact preparation before (ms) | After (ms) |
|:--|--:|--:|--:|
| Startup | cold | 124.30 | 86.99 |
| Startup | 0.15 m | 126.41 | 88.71 |
| Startup | 0.30 m | 126.61 | 87.67 |
| Mountain | cold | 133.20 | 81.57 |
| Mountain | 0.15 m | 133.08 | 81.20 |
| Mountain | 0.30 m | 128.82 | 79.92 |

This is a 30–39% reduction in contact preparation in this cohort, not an FPS
claim. All twelve v3/v4 topology fingerprints, geometry/work fields and retained
index sizes match. The benchmark additionally hashes complete ordered candidate
lists at poles/axes, 256 distributed directions and sampled exact vertices;
all twelve candidate fingerprints match. The native walking trace's median
contact index falls 28.40→19.82 ms, full CPU build 70.40→63.05 ms and
request-to-publication 146.45→138.39 ms. Profiling overhead is retained; these are
one paired diagnostic run rather than a performance acceptance cohort.

The unchanged native gate still exits 2. Maximum published-anchor offsets after
the change are 1.476 m (0.5 m walking core), 2.805 m (0.5 m sprint core) and
9.309 m (1.5 m sprint core), with 315/332, 518/539 and 763/797 frames outside
their respective cores. The 1.5 m fixture also misses geometry targets in 537
frames; its 30,000-triangle budget remains unchanged. All ownership/nonblocking
audits pass, with zero blocking polls, bulk reads or finishes and peak logical
overlap below 75 MB. These failing receipts must not be presented as acceptance.

All 35 core groups pass; all five contact cases pass, including the new
independent full-mesh comparison for disconnected partial meshes with 1, 7, 8,
9, 15, 16, 17, 31, 32 and 33 triangles. All 45 RTX compute/staging/cell/
publication/allocation cases pass. Moving/short-band/recorded-parent replay and
CPU/GL 3.3 captures pass; overview bytes are unchanged and visually inspected
(14,779 non-background pixels, bounds (1,1)–(794,598), 3,471 planet pixels).
Raw evidence stays local under `build-f5/resume/detail-contact-*`,
`detail-contacts-{before,after}.csv` and `detail-rtx-return-trace`. Continuous
moving detail and temporal publication remain outstanding.

## Preserve recorded parent anchors — 2026-10-16

The safer reuse bound also needs a capture adapter: an older v4 document can
record a parent beyond today's reuse distance. Priming that parent through the
ordinary planner silently replaces it again when building the final eye. A
geometry-only fixture demonstrates a 2.102 m unexpected replacement.

`prepareReplayParent` now rebuilds the exact recorded parent and retains it for
the next capture plan. The marker is consumed through the same shared cache used
by worker snapshots. Historical filtered/sunk positions are retained in that
snapshot; subsequent ordinary planning resumes the current safe reuse bound.
Preparing another recorded parent always rebuilds it, including nearby anchors.
The existing capture path owns an idle planner; live operation does not call this
adapter. Invalid anchors and non-streaming use are rejected. The GPU contract,
shader packs, field version and generation hashing are unchanged.

The historical-anchor fixture uses explicitly disabled foliage and null quotas
after editing geometry; quotas from another mesh are not a valid replay input.
The original grass-enabled moving/replay matrix remains in the same test.

Ten detail CPU cases and all 35 core groups pass. The expanded capture matrix
passes in 41.4 s: recorded-parent replay is exact, CPU and GL 3.3 images are exact,
and the GPU/CPU mean channel error is zero for both normal and recorded-parent
fixtures. All seven prior images remain unchanged. All 45 software compute/
staging/cell/publication/allocation cases pass (77.6 s). The historical-anchor
image is inspected; the overview remains byte-identical with 14,779
non-background pixels, bounds (1,1)–(794,598), and 3,471 planet pixels. Evidence
stays local in `build-f5/resume/detail-recorded-parent-*` and the CTest capture
directory. Moving coverage and temporal publication remain unfinished.

## Parent workload and dispatch verification — 2026-10-16

The planning helper now distinguishes the unconditional parent-position count
from the positions needed within each vertex. Positive blend weights need three
positions for an actual triangle, one for three identical descriptors, and none
at zero weight. Production v4 cases require 27,579–28,590 such positions versus
81,645–85,719 unconditional evaluations, about 66% redundant work by this scalar
count. This excludes fine-field/normal work and sharing between vertices; it is
not a count of issued invocations or a GPU timing prediction. All twelve topology
fingerprints remain unchanged.

A temporary shader branch skipped unused/duplicate evaluations while retaining
the existing dispatch layout. Three alternating 480×270 llvmpipe replay pairs
give these complete terrain-generation timings:

| Pair | Existing phase (ms) | Branch experiment (ms) | Images |
|:--|--:|--:|:--|
| 1 | 41.116 | 39.365 | exact |
| 2 | 36.986 | 39.703 | exact |
| 3 | 38.318 | 44.378 | exact |
| Median | 38.318 | 39.703 | |

There is no observed gain in this small software cohort. The shader experiment
was reverted; both compute variants remain identical to 1932457. No hardware or
FPS improvement is claimed. NVIDIA NVML still reports `Unknown Error`.
The useful retained changes are workload diagnostics and stronger oracle
coverage: the moving-parent test now forces 64-invocation chunks, crossing
three-descriptor boundaries, and checks that zero-weight, repeated-descriptor
and actual parent-plane inputs all occur. It still compares the whole GPU mesh
and sparse contacts against independent CPU results without relaxing tolerances.

After restoring the shader, all 45 software field/staging/cell/publication/
allocation cases pass (77.7 s). The final production renderer is unchanged from
the tested near-band correction; its 35 core groups and moving/short-band replay
matrix remain passing. Local evidence: `build-f5/resume/detail-parent-work.csv`,
`detail-parent-pairs/results.json` and `detail-final-compute-all.log`.

## Keep short near-band cores fully resolved — 2026-10-16

A focused regression exposed a v4 correctness bug: the 10 m minimum parent
reuse distance could outlive a short finest-field band. With a valid 2 m near
band, a 0.5 m core and 3 m of movement, the old path sinks an interior core
sample by 60.988 mm and filters its detail. The fine field is intentionally fixed
to the parent's anchor while cached probes refer to it, so geometry selection
alone cannot fix that mismatch.

Parent reuse is now limited to half the clearance between the configured near
distance and core radius. Rebase replaces the shell, adjacency and field-probe
cache together before that band is exhausted. Orbit reuse is unchanged. Short
bands consequently rebuild more frequently; this fixes fine-core correctness
and does not resolve the existing moving-coverage/temporal-publication limits.

The new CPU case traverses, reuses and rebases a 2 m near band, checking every
retained core sample for zero sinking, zero parent blend and the fully resolved
field height. It fails before the fix and passes afterward. All nine detail cases
and all 35 core groups pass. Eleven terrain compute cases pass, including a new
short-band rebase with CPU/GPU position error below 0.06 mm, closed edges and
unchanged numerical tolerances.

The capture matrix also forces rebases using a 1 m near band: the parent moves
1.802 m, the final core's maximum edge is 4.431 cm without a limit flag, and
saved-parent replay is byte-exact. The four original moving/replay/CPU/GL 3.3
images remain unchanged. The expanded matrix passes in 34.8 s. The overview
render remains nonblank with 14,779 non-background pixels, bounds
(1,1)–(794,598), and 3,471 planet pixels; the short-band image is inspected.
Evidence stays local in `build-f5/resume/detail-short-near-*` and the CTest
`terrain-streaming-captures` directory. NVIDIA remains unavailable; no hardware
movement acceptance or completed terrain TODO is claimed.

## Cached parent adjacency — 2026-10-16

The optional v4 planner now retains immutable coarse-edge adjacency and radial
face bounds alongside its one cached parent shell. Each moving plan materializes
mutable edge records only for refinement/probes and any neighboring faces reached
by longest-edge propagation. Untouched parent connectivity stays shared. A closed
parent with F faces retains 3F/2 edges and F bounds; mutable live edges remain
bounded by 3/2 of the existing triangle cap. Transient child faces carry one
parent-face ID instead of copying three radial/sink descriptors. The final
128-byte morph records, vertex order, field probes, split priorities and GPU
contract are unchanged. Parent and adjacency are replaced together after both
builds succeed; rebase/mode changes replace the pair and reload owns a new cache.

Canonicalization reserves the expected closed-shell vertex count and validates
structure after calculating its fingerprint, avoiding an immediate redundant
recalculation. Public consumers still verify structure and the complete checksum;
the regression also mutates a valid-range blend weight to check stale-data
rejection. No fingerprint algorithm, validation tolerance or detail target changed.

CPU traces now distinguish adjacency preparation, initialization, parent
assessment, splitting, export, canonicalization/checksums and sparse-contact
index construction. The planning helper records exact fingerprints and parent /
mutable edge counts. The native helper optionally records CPU/publication traces
and explicitly supports llvmpipe functional runs. Its gate now also rejects
published cores that miss their configured edge/error targets, separately from
camera coverage; no GPU readback is added to native observation.

The software run exposed a pre-existing G4 test assumption: its counter excluded
the completing poll and therefore required an extra asynchronous fence poll.
Counting every poll and additionally requiring more than four root dispatches
checks actual slicing, including drivers that signal immediately. The per-poll
four-dispatch limit, live-generation checks and source-lease/cancellation checks
remain enforced.

An isolated GCC 12 RelWithDebInfo run with the same production inputs measures
warm plans at 99.5–100.9 ms at startup (previously 154.6–173.0 ms), and
98.1–102.7 ms at the mountain reference (previously 195.6–202.2 ms). Cold plans
remain expensive at 1,716 / 1,288 ms. All twelve v3/v4 cold/moving benchmark rows
retain their exact fingerprints, triangle/evaluation counts, edge/limit results
and transfer bytes. Each parent has 75,000 edges; moving plans materialize only
6,679–6,754 mutable edges at startup and 9,695–10,255 at the mountain pose.
These are CPU planning results, with no concurrent build/render/test work.
They are not a moving-coverage or GPU/FPS claim.

Validation passes all 35 core groups, eight detail CPU cases and all 44 software
compute/staging/cell/publication/allocation cases. The moving capture, parent
replay, CPU and GL 3.3 images agree exactly and match the prior committed final
captures (`stream-reuse`). The headless overview is inspected and nonblank:
14,779 non-background pixels, bounds (1,1)–(794,598), 3,471 planet pixels.

NVIDIA hardware validation could not run: NVML initialization fails with
`Unknown Error`. Explicit llvmpipe native routes still fail the unchanged
coverage gate: the 0.5 m walking/sprint cases have maximum offsets of 22.39 /
45.38 m (5/7 and 7/9 measured frames outside), and the 1.5 m sprint case has
28.74 m maximum offset (3/5 outside). Speeds are 5.40 / 10.12 / 6.98 m/s;
these software routes are functional diagnostics, not a hardware comparison.
The 1.5 m core also misses its geometry target in all five sampled frames.
Ownership/nonblocking audits pass with zero blocking polls, bulk reads or
finishes; logical overlap stays below 75 MB. The helper returns exit 2.
Uninterrupted movement and temporal publication remain incomplete; terrain stays
**p**. Raw evidence is local in `build-f5/resume/detail-adjacency-*` and
`detail-before-fingerprints.csv`.

## Experimental reusable parent mesh — 2026-10-16

`terrain_lod.local_detail_streaming: true` opts into topology version 4. It
requires the existing local-detail settings; omission preserves versions 1–3
and the production configuration. One coarse parent shell is shared by immutable
worker snapshots. Half the triangle budget is reserved for that parent (subject
to the existing minimum mesh size); the remainder can refine the moving disk.
The cache holds one parent, replacing it after half the near distance or a
local/orbit mode change. Field/config reload creates a new surface/cache.

Local refinement skips distant field probes and starts from this reusable
parent. Each inserted sample retains the three original parent radial/sink
descriptors. Both sides of an edge still split together. Outside the core, a
smoothstep over `local_transition_m` blends the fine position toward the radial
intersection with the actual parent triangle, including its float rounding and
sinking. Original parent vertices use their original positions. This closes the
spatial transition onto the parent surface rather than merely blending analytic
noise fields. GPU compute evaluates the descriptors; CPU fallback and sparse
contacts use the same projection. Grass, shadows and reflections consume the
resulting published positions.

The additional descriptor pack is 128 bytes per unique sample plus a 128-byte
parent policy, included in topology transfer, worker memory and GPU admission.
A temporary 36-byte position tail per unique sample lets a separate parent
phase evaluate each descriptor without duplicating the noise evaluator three
times in one invocation. This scratch tail is also admitted. The implementation
uses the existing eight compute bindings. Parent planning eyes are recorded
and validated in replay so a moving capture can reconstruct its original parent
before rebuilding its final local disk. Unknown versions and invalid anchors
are rejected before opening a window.

Final focused validation: all 35 core groups and all 44 RTX compute cases pass;
seven terrain-detail CPU cases pass, including cache lifetime/determinism.
The GPU moving-detail test checks both closed topology and sparse contacts.
The initial kernel differed by at most 0.119209 mm on RTX 3070 Ti; the final
parent-position phase gives identical CPU/GPU positions in this moving fixture. A production
startup capture has 2,929 core triangles, a 4.439 cm maximum core edge and no
local-detail limit; it retains zero bulk CPU evaluation requests. Its 800×600
image was inspected: 479,998 non-background pixels, bounds (0,0)–(799,599).
Raw evidence stays ignored in `build-f5/resume`.

A bounded in-memory height-probe cache also survives worker snapshots. Its
referenced field is owned by the shared cache. Fine-field policy stays fixed for
the parent's lifetime, while geometric selection and blend weights follow the
current eye; cached values therefore retain their exact meaning. At most twice
the triangle budget in probes are retained; reaching that capacity clears the
query cache. Rebasing/reload replaces its policy/field and orbit mode releases it.
Canonical parent IDs and edge-owned midpoints allow direct ID compaction instead
of repeated whole-shell ordered maps. Normal and optional morph compute variants
share GLSL source; preprocessing the normal variant reproduces G4 exactly.

The reproducible CPU-only benchmark is documented in
`scripts/benchmarks/terrain_detail/README.md`. An isolated production run (same
100k cap, 0.5 m core/transition, two 0.15 m eastward moves) measures:

| Position | Whole-shell warm CPU | Reusable-parent warm CPU | Warm field evaluations |
|:--|--:|--:|--:|
| Production start | 2,881–2,891 ms | 153–174 ms | 2,504–2,524 versus 373,296–373,376 |
| Mountain reference | 1,858–1,866 ms | 198–200 ms | 2,697–2,877 versus 359,230–359,231 |

Cold parent/patch planning is 1,759 / 1,351 ms. Core edges remain below 5 cm
without a local-limit flag. The earlier reusable-parent pilot without probe
reuse/ID compaction took 386–485 ms warm. These are CPU topology timings, not
GPU publication time or a frame-rate/continuous-coverage guarantee. The optional
mode transfers about 5.0–5.3 MB of topology descriptors in these cases, versus
2.8 MB for the 100k v3 baseline; the refined v4 mesh has 54–57k triangles, so this
is a bounded architectural comparison rather than matched-work FPS evidence.

Software moving captures reproduce exactly from saved parent anchors. CPU and
compute images match exactly in the focused fixture; explicit Mesa GL 3.3 matches
CPU output. Invalid parent anchors fail before renderer initialization. Seven
CPU detail cases and all 44 RTX field/staging/cell/publication/allocation cases
pass after the final cache/shader changes. A prior broad run also passes all six
lifecycle/recovery/frame/reload/shadow/historical-capture groups (678.13 s).
The final compact-scene `--render-test` is nonblank: 14,779 non-background
pixels, bounds (1,1)–(794,598), with 3,471 planet pixels. The overview and moving
480×270 detail capture were visually inspected.

A separate real-input RTX pilot on flat micro-relief records the remaining
coverage failure: over four-second routes, the 0.5 m walking core has a maximum
anchor lag of 1.71 m (783/823 sampled frames outside it); the 0.5 m sprint core
lags 3.40 m (783/811 outside), and the 1.5 m sprint core lags 9.37 m (962/1002
outside). Speeds are 5.92 / 11.82 / 11.87 m/s, including input delivery. Terrain
continues publishing and all bulk-read/blocking-poll/GL-finish audits are zero;
logical overlap is 60–75 MB. These small fixtures already fail the core-coverage
condition. They cannot justify a production moving-detail claim. The reproducible
native helper deliberately returns exit 2 for an unmet coverage gate.

This path reuses coarse
planning, but rebuilds the bounded local refinement and uploads a replacement
generation. Existing fences publish its consumers together; publication is not
yet a continuous temporal geomorph. Parent replacement and sustained walking/
sprint coverage need further work before enabling this setting by default.

## Bounded local detail design — 2026-10-09

The next implementation uses the existing longest-edge planner, with an opt-in
ground-centered detail disk. CPU workers own selection, shared edge IDs and
error probes. GPU compute owns bulk displacement and normals; sparse contacts
use the same immutable field and policy. Both incident faces split together,
preserving a closed shell without introducing a second mesh or GPU readbacks.

`local_detail_radius_m`, `local_edge_m`, `local_error_m` and
`local_transition_m` select physical edge/error targets. The core disk receives
priority over its transition and distant curvature refinement. The existing
triangle cap and staged/resident admission remain authoritative; capture metadata
reports actual local edge/error maxima and infeasibility. These are sampled
error targets, not a mathematical error bound between arbitrary probes.

Planet-fixed noise accepts `wavelength_m` instead of dimensionless `frequency`.
It resolves to radius-in-meters divided by wavelength, retaining the existing
eight-noise parameter block. Normal probes use at most 5% of the shortest active
wavelength. Unresolved octaves fade using the shared policy's physical spacing;
the core has zero sinking. A nonzero finest spacing introduces topology version
3; legacy versions 1 and 2 retain their layout and behavior.

This pass will expose and verify the feature before enabling it in production.
Rebuild distance shrinks with the selected disk, but current background planning
can still lag a moving camera. Exact projection onto parent triangles and a
continuously streamed detail patch remain outstanding in the same TODO row.
Stationary centimeter targets must not be represented as a moving-camera guarantee.

### Implementation receipt

Local targets and physical noise are implemented. Existing production defaults
remain unchanged because whole-shell CPU planning is too slow for a small moving
patch. To opt in, add these keys to a planet's existing `terrain_lod`:

```json
{
  "relief_sinking": true,
  "geometric_error_m": 0.05,
  "local_detail_radius_m": 0.5,
  "local_edge_m": 0.05,
  "local_error_m": 0.01,
  "local_transition_m": 3.0
}
```

An optional `surface_noise` layer can use
`{"amplitude_m":0.01,"wavelength_m":0.25,"octaves":1,"seed":-71}`.
`wavelength_m` specifies base noise lattice spacing in reference-sphere meters;
octaves divide it by lacunarity. Choose either wavelength or legacy frequency,
never both. At 5 cm spacing, a 25 cm layer is resolved; a 10 cm layer needs finer
edges or is filtered out. Original fields, hashes and 704-byte field/128-byte
policy layouts are retained when these options are omitted.

Stationary production probes use the unchanged 100k cap, production landscape,
water and existing noise plus the optional layer above. 410 fixed off-center
radial probes per run check the actual float render mesh against the selected
field. No compiler or other GPU workload ran concurrently.

| Camera | Core radius | Local triangles | Maximum edge | Maximum probe error | Local target limited | CPU planning |
|:--|--:|--:|--:|--:|:--:|--:|
| Production start | 0.5 m | 2,929 | 4.443 cm | 0.050 mm | no | 3,102.788 ms |
| Production start | 1.5 m | 23,122 | 5.666 cm | 0.050 mm | yes | 2,452.080 ms |
| Reported mountain | 0.5 m | 5,033 | 4.580 cm | 0.153 mm | no | 1,640.244 ms |
| Reported mountain | 1.5 m | 41,452 | 4.998 cm | 0.125 mm | no | 1,534.243 ms |

All four runs reach 100,000 triangles because distant refinement still uses the
remaining budget. They have zero open/non-manifold edges and zero reversed render
triangles. Each topology transfers 2,800,192 bytes. The very small probe errors
are observations at these points, not guarantees for arbitrary procedural cliffs.
Core diagnostics conservatively include faces whose radial bounding disk touches
the core, so edge targets also cover its boundary. `local_detail_limited` checks
both the measured edge target and sampled error ratio.

Raw sources/receipts stay ignored under `build-f5/ridge-refinement/detail-*`.
Exact parent-triangle projection, gradual publication changes and reuse/streaming
of a moving fine patch remain in the existing terrain row. Reducing rebuild
distance alone cannot cover walking/sprint motion during seconds of planning.

Verification: full RelWithDebInfo build, all 35 core CTest groups and eight
terrain integration groups have passing results (43 groups total). Five new CPU
cases cover physical units/lattice limits, config parsing, independent 1 mm
central-difference normals, actual float-mesh edges/interior probes at the equator
and pole, deterministic closed topology and cap exhaustion. All 37 NVIDIA compute
cases pass; the new physical-noise case has identical CPU/GPU positions/colors,
maximum height difference 3.34e-14 m and normal difference 2.98e-8 radians.

Version-3 standing CPU/GPU and locked replay PNGs are byte-identical (SHA256
`923e07efc8fae18756cd08da81c1fd7cc777aad85cba1a35be8aacf07785c1a8`).
Walking replay is also exact, with sparse contacts, coherent grass/shadow/reflection
revisions and 16,595,304-byte peak logical publication reservation. CPU override,
GL 3.3 fallback, version-4 rejection, reload from legacy to version 3, native
walking/sprint/flight and the held-worker grass-refresh regression pass. The
initial added walking fixture incorrectly replaced broad height noise below sea
level and moved less than the 20 cm trail threshold. Its land elevation and path
length were corrected. After the subsequent anchor/clipping fix, all four
affected renderer/capture groups pass again in 134.76 seconds; the final capture
CTest passes in 49.00 seconds.

A short stationary mountain capture uses production foliage settings and the
0.5 m opt-in disk on the Quadro M1000M, at 480×270 with 30 uncapped offscreen frames
and 1/60 s simulation steps. After separating the 2,517.443 ms first frame
(2,238.166 ms topology planning), 29 warm frames have median/p95 wall time
11.777/14.595 ms and GPU frame span 11.477/14.298 ms (nearest-rank p95). There are no warm terrain
rebuilds. The GPU field generation is 43.957 ms; terrain/grass stage admission is
280,833,196 bytes, with zero bulk CPU evaluation requests/render vectors and
unchanged 120.72/m² protected density policy, without a near-infeasible flag.
This small stationary offscreen run is not a full-resolution or moving-camera
60 FPS result. Image inspection confirms the displaced terrain renders, though
the supplied camera looks toward a dark slope; centimeter detail is concentrated
under the camera rather than across the entire distant mountain.

That moving-time benchmark also revealed two surface-capture replay dependencies:
terrain was regenerated at the final camera/time instead of its original planning
anchor, and clipping planes still came from the first frame. Version-3 surface
captures now retain the small planet-local planning-eye array, validated before
opening a window; surface clipping follows the current frame, matching interactive
rendering. The final 30-frame production capture, saved compute replay and CPU
override have identical topology fingerprints and PNGs (SHA256
`a79401ff80e915f50804f90ab34e13102acb2c89c719f5a9eee9544f085b4b83`).
Legacy captures without the optional anchors retain their previous loading path;
character captures retain their existing pose/terrain anchors. The automated
version-3 capture check now covers advancing orbital time and rejects corrupt
planning anchors before renderer initialization.

2026-10-08. Implements the sinking/filtered-noise part of the centimeter-terrain
proposal. Implemented and verified; no commits made.

The standard scene enables `terrain_lod.relief_sinking`; omitted/false preserves
legacy geometry and topology version 1. Enabled mode uses topology version 2.
Both replay versions are accepted, while unknown versions remain rejected.
Water explicitly disables this model. Setting sink depth to zero disables only
the inward offset, allowing filtered terrain without displacement.

One immutable 128-byte policy per generation carries the camera radial, radius,
distance interval and parent sample spacings. Seven smooth transitions blend
existing noise weights between increasingly coarse procedural parent fields.
Noise wavelengths with >=4 samples retain their weight; <=2 samples are removed.
Original octave normalization is retained, avoiding amplification of remaining
frequencies; fully unresolved octaves skip their noise evaluations. Broad landscape terms use the same filter; constant elevation stays
unchanged. Finest nearby terrain uses the original field and zero sink.

Sinking is the lesser of the configured/small-planet cap and omitted amplitude
bound plus a sphere-curvature estimate. Omitted relief uses noise amplitudes and
octave weights, without additional CPU field evaluations. The curve is evaluated
once per topology sample after shoreline refinement. Shared radials get identical
offsets, independent of face ownership, hysteresis and budget-selected levels.

GPU bulk heights/normals and the bounded CPU contact/cache oracle evaluate the
same policy. Normals include the changing sink gradient as well as filtered
noise, preventing a shading mismatch on the displaced surface. The policy is part of the topology fingerprint, transfer statistics
and live/staged/retiring admission. Existing radial descriptors stay 32 bytes;
there is no per-vertex morph upload or full CPU render mirror. GLSL's missing
double acos is handled by a half-angle atan series; a 100,001-point scalar sweep
finds maximum error 1.33e-15 radians.

This is continuous spatial blending of the procedural field, not exact projection
onto a coarser triangle mesh. Existing generation rebuilds (~10 m movement),
topology changes and publication can still step. Centimeter subdivision, explicit
wavelength controls, exact parent-triangle geomorph and a centimeter mesh-error
guarantee remain in the existing terrain task; none is claimed complete here.

## Verification

- Full RelWithDebInfo build passes. All 34 core groups pass, including 37 terrain
  cases, frozen legacy geometry hashes, equator/pole watertightness, near-surface
  invariance, continuity at every noise parent boundary, omitted-relief bounds,
  sink-aware normals and zero-depth filtering.
- All 36 NVIDIA compute cases pass, including mixed-level relief/sinking,
  shoreline endpoints, sparse contacts, source-version rejection, staging,
  overlap admission, retirement and GL binding restoration. Production 100k
  positions/colors match CPU exactly; maximum height difference is 1.31e-12 m.
- Lifecycle (30 cases), recovery, frame, reload and expanded compute capture
  groups pass. Capture checks cover exact CPU/GPU, walking/contact replay,
  legacy captures, CPU override, GL 3.3 fallback and unknown topology version 3.
- Native input runs with relief sinking enabled: walking/sprint, manual/watch
  reload, flight and Moon contacts, explicit CPU and GL 3.3 fallback pass. Main
  walk/sprint wall speeds are 5.980 / 12.004 m/s. Main logical overlap peaks at
  16,979,008 bytes in the bounded fixture; existing 1 GiB admission gates pass.
- Final standard-config compute, saved compute replay and explicit CPU override
  produce byte-identical 480×270 PNGs and 1,599,906 candidate slots. Configured
  near foliage policy remains 120.72 blades/m² through 15 m, sigma 8.435062249 m.
  PNG SHA256: `32d87edad33633b4d354200eee742969ebd5d7664e42a129c3459af1e5e921fa`.
- Repository layout and whitespace checks pass. One old test's unsupported
  topology version was updated from 2 to 3; ready-memory diagnostics retain their
  existing vector-only meaning, with the fixed policy charged in GPU admission.

The existing canonical 100k receipt retains the original <=25% transfer gate:
2,801,008 bytes cold / 2,800,304 warm versus 12,000,000 CPU shaped bytes (76.66%
reduction), exactly 128 bytes above the previous pipeline. CPU bulk evaluations
remain 150,006 versus zero on the compute path. Three warm trials, without a
concurrent build, report median CPU bulk 538.570 ms, GPU submit 15.211 ms and GPU
field/expansion 39.513 ms. These are generation timings on the tested Quadro,
not full-frame p95 results or a repeat of the F6 cost cohorts.

Raw captures, logs and receipts stay local under `build-f5/relief-sinking-*` and
the existing ignored integration-output directories. No staging or commits.

## Local ridge refinement — 2026-10-08

The user's `image copy 3.png` reveals undersampled geometry: the old planner
tests steepness on entire base faces and discards optional steep detail first
under budget pressure. Sinking changes displacement/filtering, not the number
of samples along a crest. Raising noise frequency alone cannot fix that.

The standard scene now enables `terrain_lod.geometric_error_m=0.05` alongside
relief sinking. Omitted/zero preserves legacy tessellation. The planner reserves
half the existing triangle budget for local refinement, measures triangle-center
and shared-edge midpoint error against the rendered filtered/sunk field, and
prioritizes error divided by `max(geometric_error_m, 0.001*distance_m)`. The local
region ends at the existing middle distance. This concentrates triangles on
changes in slope rather than spending them on every steep planar surface.
Shorelines retain a separate quarter-budget allowance when needed; water disables
this land refinement and stays at its physical level.

Each bisection splits both incident triangles. Longest-edge propagation first
follows larger neighboring angular edges until the selected edge is longest on
both sides, preventing repeated shortest-edge splits from collapsing angles.
Both sides share one radial midpoint and sink. Ordering is deterministic;
triangle/probe scratch is bounded by the configured cap, and final canonicalization
removes unused probes. CPU workers still choose topology and evaluate error
probes; GPU bulk displacement/normals, sparse contacts, foliage, shadows and
reflections consume that same topology. No extra GPU height readback is added.

The minimum split footprint is 4 cm (its edge halves are approximately 2 cm).
This is a sampled, distance-scaled target, not a bound between arbitrary probes
or a guarantee that the entire nearby mesh has centimeter edges. Capture metadata
reports `error_refined_triangles`, `remaining_error_ratio` and
`error_budget_limited`. Exact parent-triangle geomorph and explicit wavelength
controls remain in the existing terrain task.

At latitude -18.726025155509635°, longitude 113.23762488691663°, reference-sphere
altitude 126.91396763254018 m, with 2 m clearance, 2,821 identical radial contact
probes cover a 90 m disk. The independently sampled analytic field is unsunk
throughout that disk:

| Metric | Previous planner | Surface-error refinement |
|:--|--:|--:|
| Triangles | 99,126 | 100,000 |
| Mean absolute height error | 0.642115 m | 0.060993 m |
| 95th-percentile error | 2.322859 m | 0.157881 m |
| Maximum error | 8.914115 m | 0.258581 m |
| Open/non-manifold edges / reversed triangles / missed contacts | 0 / 0 / 0 | 0 / 0 / 0 |
| Euler characteristic | 2 | 2 |

The refined mesh adds 60,798 triangles to its reduced 39,202-triangle baseline.
Remaining sampled error ratio is 1.921896, explicitly budget limited. A single
local planning receipt on the Quadro host measured 569 ms before / 2,599 ms
after; refinement increases CPU planning work despite retaining the same GPU
triangle cap. These are generation timings, not an FPS or full-frame p95 claim.
Interactive compute rebuilds remain on the background worker.

Paired renderer captures use the exact supplied pose and time. Additional
`*-lit.png` diagnostic captures set identical ambient fill and disable automatic
exposure in both scenes so the shadowed ridge is easy to compare; production
lighting is unchanged. Captures, configs, contact probe source and raw receipts
stay local under `build-f5/ridge-refinement/`.

Validation: full build, all 34 core groups (40 terrain cases), and all
36 NVIDIA compute cases pass. New tests cover independent interior height probes,
closed shared edges at poles/shorelines, deterministic generation, budget exhaustion,
config validation and CPU/GPU sparse-contact agreement with refinement enabled.
The first cold GPU CTest invocation exceeded its 120-second suite timeout;
the complete unchanged test binary then passed all 36 cases in 54.05 seconds
using a writable local NVIDIA shader cache. The final standard CTest invocation
also passes within its unchanged timeout (49.66 seconds). The supplied-pose compute capture,
saved replay and explicit CPU override produce identical PNGs:
`291bd1690189965b2150e766fbbe9b826b6c0f68e790a03be663dca764cc1f40`.
GPU captures retain zero CPU render vectors and zero bulk CPU evaluation requests;
the extra CPU work reported above consists of topology/error planning probes.

All six lifecycle/recovery/frame/reload/capture/native integration groups pass
(287.51 seconds total). Capture and native fixtures enable the new refinement,
including exact walking replay, CPU override and GL 3.3 fallback. Native checks
cover input, jump/thrust, trails, watch/manual reload, supersession, space navigation
and Moon contacts. Main walking/sprint wall speeds are 5.994 / 11.968 m/s;
resident fixture overlap peaks at 12,854,508 bytes with no bulk reads, blocking
polls or GL finishes. The supplied-pose production capture admits 280,833,196
stage bytes, preserves 120.72 blades/m² through 15 m and reports no near-density
deficit. No full-frame cost-cohort claim or new TODO chain is added.

# Planet-field and terrain-topology contracts — 2026-10-03

This implements T1 of the [CPU–GPU plan](../plan.md). TerrainSurface now
builds an indexed radial/sink topology and evaluates it through a separate CPU
planet-field oracle. GPU shaping remains the next task; the current renderer
still uploads its legacy expanded vertex/index buffers and CPU planning still
queries surface heights. No measured GPU speedup or upload reduction is claimed.

## Boundaries and layouts

`src/rendering/geometry/terrain/PlanetField.h` owns camera-independent noise,
landscape, material, sea level and SI scaling. Its read-only API retains the old
double-precision lattice/hash/interpolation and normal stencil. Landscape seed
offsets now explicitly wrap unsigned 32-bit values, avoiding signed overflow at
extreme seeds. Ordinary seeds and pre-refactor fixture output remain unchanged.

The version-1 parameter pack is 704 bytes: five double4 groups (scale, landscape
shape/mask, material, gradient), two uint4 groups (flags, seeds), and eight
64-byte noise records. All unused fields are zero. Compile-time assertions check
size and offsets; signed seeds retain their 32-bit patterns. This is an explicit
std430-compatible contract for the future backend, not a new shader or GL upload.

`TerrainTopology` carries unique 32-byte double radial/sink samples and 32-bit
triangle indices. IDs follow first use of exact radial/sink bits within a build;
triangle order and winding remain legacy-compatible. They are not yet persistent
hierarchical IDs across LOD generations. Shoreline subdivision uses temporary
float-rounded planning positions, matching historical split decisions. Scratch
is released before the topology is published.

The CPU evaluator computes positions, normals and material factors once per
unique input, then expands indexed corners into the existing nine-float vertices
and sequential indices. Sink distances retain the original float rounding and
are applied after the first float position conversion. Grass IDs, collision
triangles, shadow/reflection consumers and locked replay keep their existing
layout. Runtime land uploads therefore remain 120 bytes per triangle; the
smaller topology input size is reported separately, not as transferred bytes.

## Cache and generation checks

Each topology/evaluation build owns a bounded height-query cache (8,192 entries
by default). Keys use exact radial bits, with no quantization or shared worker
state. On capacity exhaustion the cache starts a new bounded batch; zero
capacity disables it. Replacing the field parameters clears stale heights.
Invalid directions never enter the cache. Separate planning/evaluation request,
evaluation and hit counters expose the amount of work each boundary performs.
The cache is a sparse reuse mechanism; this stage still builds full CPU planning
and render output and does not yet implement a sparse collision-only mirror.

Generation keys contain field and topology fingerprints, field/topology versions
and the explicit CPU backend. Fingerprints are FNV-1a over semantic numeric words
in little-endian order, excluding padding; they are content identifiers, not
cryptographic integrity checks. Evaluation rejects a different field, changed
sample/index content with a stale key, unsupported versions/backends, malformed
indices, non-unit/nonfinite radials and invalid sink distances. Mesh stores the
key and counters; capture metadata writes 64-bit fingerprints as decimal strings
to retain precision. Existing replay inputs need no new settings.

## Validation

The independent pre-refactor executable and snapshot harness were saved before
editing runtime sources (baseline commit `4b1c2ff`). Seventeen baseline meshes
cover flat shoreline, negative-seed ridged terrain, a Moon-sized body, uniform
subdivision, equator, both poles, orbit and two production-config views.
All vertex, normal, color, index and sink bytes match exactly. Fifteen smaller
hash fixtures remain in seven `TerrainContracts` cases, alongside layout/seed,
high-frequency, SI-unit, bounded-cache/reload, generation and invalid-input checks.

Two native replays match their previous gallery PNGs byte-for-byte: the
960 × 540 shoreline and 1920 × 1080 dense offline showcase. Both land meshes
contain 100,000 triangles and 50,002 canonical samples. The input contract is
2,800,064 bytes, alongside the unchanged 12,000,000-byte runtime land upload.
Shoreline planning requests/evaluations are 63,377/61,616; offline planning is
81,117/79,761. Each CPU evaluator performs 150,006 requests/evaluations (one
height and two normal-stencil samples per canonical input). These are final-build
operation counts, not before/after timing or GPU performance measurements.
The offline capture retains 213,454 main-view blades, 1,535,998,624 grass working
bytes and the previous visible lens flare. Its larger foliage allocation has
not changed. All 22 gallery image hashes and their dated provenance are retained.

A clean full build and all 56 CTest groups pass in 641.75 seconds, including
the seven new contract cases, terrain/grass/contact tests, shadows/water,
atmosphere, exact replay and live X11 input. The journal compiles to 24 pages;
pages 3–6 and 24 were visually reviewed without clipping.

Final renderer, replay, layout and journal results are retained in
[validation/](validation/). Native captures are in [captures/](captures/).
The previous architecture worksheet and gallery provenance remain dated evidence;
this checkpoint records new results separately. Timing on llvmpipe is software
renderer evidence, not hardware FPS or a GPU migration performance claim.

The next phase is opt-in GL 4.3 field evaluation (T2), followed by GPU grass
planning, sparse contact coverage and atomic dependent consumers (T3). Existing
GL 3.3/CPU fallback remains the baseline until the plan's parity and performance
gates pass.

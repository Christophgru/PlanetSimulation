# Grass triangles, batching and eight distance levels

## Scope of the reference

The [glvertexid example](https://github.com/vercidium-patreon/glvertexid/tree/25f1d1d292eeb9cd50984d933890b1ec00d4c97c)
reconstructs a regular heightmap's horizontal coordinates from `gl_VertexID`,
uploads heights, and joins triangle strips with degenerate vertices. Its pinned
revision is `25f1d1d292eeb9cd50984d933890b1ec00d4c97c`. That example does not contain
an eight-LOD or sinking implementation. The changes below adapt those additional
requested techniques to this renderer's grass.

Grass already reconstructs its shape from `gl_VertexID` and uses instancing.
The terrain has irregular spherical faces, mixed edge densities, height-derived
normals and local shoreline splits. A regular height-only grid would require a
different terrain representation. This change keeps the watertight terrain
topology and applies the optimizations to foliage. No reference source code or
new runtime dependency is imported.

## Implementation and quality tradeoffs

Each blade strip now ends at one tip rather than two coincident tip vertices.
Six segments submit 13 vertices and 11 triangles; one segment submits three
vertices and one triangle. This removes a degenerate tip triangle from every
blade. A single sorted instance buffer replaces the separate near/far buffers.
GL 3.3 VAOs address contiguous ranges of that buffer, without requiring indirect
drawing, base-instance support or compute shaders.

There are eight logical distance levels. For a 60 m draw distance their nominal
geometry thresholds are:

| Level | Distance from eye (m) | Segments | Vertices | Submitted triangles |
|---|---|---:|---:|---:|
| 0 | 0–7.5 | 6 | 13 | 11 |
| 1 | 7.5–9.375 | 5 | 11 | 9 |
| 2 | 9.375–11.25 | 4 | 9 | 7 |
| 3 | 11.25–13.125 | 3 | 7 | 5 |
| 4 | 13.125–15 | 2 | 5 | 3 |
| 5 | 15–30 | 1 | 3 | 1 |
| 6 | 30–45 | 1 | 3 | 1 |
| 7 | 45–60 | 1 | 3 | 1 |

The CPU subtracts the patch movement allowance before selecting a level: 9 m
for this scene. Thus the uploaded geometry stays sufficiently detailed while
walking within the cached patch. The last three levels use identical geometry
and merge into one draw. There are at most six draws per planet per pass,
compared with two previously, but just one instance upload per patch rebuild
instead of two. This trades a small number of draw submissions for less vertex
and instance work; the benchmark below measures the result.

Blades straighten between 3.75 and 7.5 m, before any geometry reduction is
possible. Removing the now-collinear intermediate vertices preserves shape.
The corresponding distances scale down for draw ranges below 60 m and are
capped at these values for larger ranges. This deliberately simplifies shape
and shading earlier than the previous 7.5–15 m transition.

Eight deterministic retention groups additionally thin the distant Gaussian
candidates. They have equally spaced sinking intervals from 15 to 60 m in the
working scene. Each interval retires another eighth of the candidate population
on average. Sinking and collapse use smoothstep every frame; the final group
reaches zero at the draw limit. The existing outer-quarter fade also applies.
The geometry bands and retention intervals serve different purposes and have
different boundaries. Near-camera density, candidate seeds, placement budget,
wind phase and terrain sampling remain the same. Distant grass becomes sparser.

CPU rejection uses the same hashed retention group as the shader, plus the
movement allowance and a small rounding guard. It removes only candidates that
cannot become visible before the next rebuild. Root attributes are preserved,
and the main and reflected views share the same LOD decisions.

## Measurements

The same initial 60 m scene produces these work counts; each instance is
40 bytes, and triangle counts include submitted degenerate or fully sunk
geometry:

| Work per planet | Before | After | Reduction |
|---|---:|---:|---:|
| Vertices per pass | 1,477,562 | 1,081,766 | 26.8% |
| Triangles per pass | 1,151,906 | 788,746 | 31.5% |
| Instance bytes per rebuild | 6,513,120 | 5,860,400 | 10.0% |

Generated candidate count remains 162,828. The guarded upload retains 146,510
instances. Median CPU classification/sorting/gathering time over ten warmed
iterations is 15.01 ms. Work counts are deterministic; timing is host-specific.

A paired software-renderer run uses Mesa 22.3.6 llvmpipe with two worker threads,
GCC 12.2.0 `-O2`, Xvfb, 640×360 pixels and 20 frames per case. The first three
and final readback frame are discarded, leaving 16 measured frames. Walking
advances 2 m per frame with orbital/wind time frozen. The bare control disables
grass only. All measured frames have valid GPU timings.

| Case | Before mean / median (ms) | After mean / median (ms) |
|---|---:|---:|
| stationary | 40.72 / 40.65 | 41.39 / 41.27 |
| walking | 1576.97 / 1387.17 | 1463.22 / 1295.95 |
| walking-bare | 708.99 / 534.98 | 720.05 / 554.24 |

Walking mean improves by 7.2% and median by 6.6% in this single paired run.
The bare control varies slightly, and this is not a hardware-FPS guarantee.
Both versions rebuild grass on six measured frames and install three terrain
meshes. Mean grass preparation per rebuilding frame remains similar. The
paused stationary case reuses its scene, so it does not measure grass draw cost.

| Walking pass / stage | Before mean (ms) | After mean (ms) |
|---|---:|---:|
| cpu_mesh_ms | 162.60 | 162.18 |
| foliage_placement_ms | 15.18 | 15.28 |
| foliage_sort_ms | 5.98 | 5.83 |
| foliage_upload_ms | 0.37 | 0.31 |
| gpu_opaque_ms | 574.69 | 511.93 |
| gpu_reflection_ms | 270.55 | 193.66 |
| gpu_reflection_atmosphere_ms | 116.24 | 116.87 |
| gpu_atmosphere_ms | 407.34 | 434.11 |

Preparation substage means include frames that reuse the patch. CPU stage wall
times and GPU times overlap and must not be added. Captures build terrain
synchronously; interactive walking builds terrain in the background. The
foliage-disabled final PNGs match exactly. Grass PNGs intentionally change with
the earlier shape simplification and reduced distant density. Raw CSVs, inputs,
logs and hashes are in [lod/](lod/comparison.json).

## Validation and reproduction

CPU tests check eight contiguous batches, original attributes, vertex-work
bounds and visibility throughout the movement allowance. Transform feedback
checks CPU/GPU fade endpoints, smooth sinking, full retirement and matching
distant silhouettes for every segment count. A GL primitive query compares the
actual submitted triangle count with renderer accounting. Existing shadow,
night, reflection clipping, paused-frame reuse and exact replay tests exercise
the new path as well.

`foliage_benchmark` reports before/after work counts from the same generated
roots, and capture logs report vertices, triangles, batches and instance bytes
per planet per pass. Instance payloads are uploaded on rebuild only, not each frame;
these counts exclude driver allocation overhead.
The benchmark script's `--runtime-dir` selects a saved directory containing
`shaders/`; both executable and shader-tree hashes are recorded, so an old
executable cannot silently use the new shaders in a baseline comparison.


```sh
cmake --build build-codex --target PlanetSimulation foliage_benchmark -j 2
./build-codex/tests/foliage_benchmark docs/journal/benchmarks/lod/input.json
LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=2 xvfb-run -a python3 scripts/benchmarks/camera_movement.py --binary build-codex/PlanetSimulation --config docs/journal/benchmarks/lod/scenario.json --replay docs/journal/benchmarks/lod/input.json --frames 20 --warmup 3 --output-dir build-codex/lod-repeat
```

# Camera movement and foliage preparation — 2026-09-30

These measurements predate the Perlin wind change. The recorded executable
hashes and capture hashes identify the earlier sine-wind workload; rerunning
with the current shader measures a different rendering workload.

Walking invalidates the completed-scene cache and occasionally rebuilds terrain
and grass. Grass preparation is measurable, but it does not explain the whole
frame-time increase. This experiment isolates placement, sorting, uploads and
render passes; it does not reproduce the reported 30-to-10 FPS change on a
physical GPU.

## Method and evidence

The paired runs use GCC 12.2.0, `RelWithDebInfo` (`-O2`), Mesa 22.3.6 llvmpipe,
Xvfb and `LP_NUM_THREADS=2`. Resolution is fixed at 640×360 with adaptive quality
disabled by the capture path. Each case has 36 frames, discarding frames 0–4
and 35. The remaining 30 frames have frozen orbital time and either no movement
or a 2 m forward step per frame. The bare control disables foliage only.
The input uses the documented grass camera and the user's current scene:
60 m draw distance, 1200.72 requested blades/m² and a 200,000-blade cap.

The baseline retains the previous placement and sorting algorithms, with the
same timing instrumentation and walking option as the optimized executable.
The baseline revision and executable hashes, raw CSVs and summaries are retained
under [movement/](movement/comparison.json). Earlier exploratory runs with
uncontrolled renderer thread counts varied substantially and are excluded.
Even the controlled rendering comparison is one pair of runs, not a statistical
claim about end-to-end performance on other hardware.

Captures build terrain synchronously to preserve deterministic geometry.
Interactive walking already builds land and water in the background and installs
them on the render thread. `terrain_build_ms` measures the whole job, so in an
interactive trace it overlaps previous frames and must not be added to the
installing frame's duration. GPU timestamps on llvmpipe measure software-renderer
work. CPU stage wall times include driver waits; CPU and GPU times overlap.

All three final PNGs are byte-identical between baseline and optimized builds.

Mean milliseconds over the 30 measured frames (except the explicitly labelled
rebuild-only column):

| Version / case | Frame mean | Frame median | GPU passes | CPU mesh | CPU foliage per rebuild |
|---|---:|---:|---:|---:|---:|
| Before / stationary | 36.64 | 36.59 | 2.09 | 0.00 | 0.00 |
| Before / walking | 1089.03 | 872.44 | 924.25 | 136.45 | 79.92 |
| Before / walking-bare | 620.87 | 478.28 | 481.32 | 137.87 | 0.00 |
| After / stationary | 37.26 | 36.96 | 2.20 | 0.00 | 0.00 |
| After / walking | 1033.55 | 869.35 | 878.71 | 136.34 | 50.79 |
| After / walking-bare | 639.14 | 482.17 | 498.60 | 138.96 | 0.00 |

Both walking runs rebuilt grass on 10 of the measured frames and installed
five terrain meshes. All 30 stationary frames reused the completed scene. All
measured frames have valid GPU samples. Sorting and placement below are mean
times on grass-rebuild frames only:

| Preparation | Before (ms) | After (ms) |
|---|---:|---:|
| foliage_placement_ms | 55.59 | 35.65 |
| foliage_sort_ms | 23.51 | 14.26 |
| foliage_upload_ms | 0.83 | 0.87 |

The optimized walking run spends these mean times in GPU passes:

| Pass | Walking (ms) | Walking without grass (ms) |
|---|---:|---:|
| gpu_shadows_ms | 6.26 | 6.18 |
| gpu_opaque_ms | 353.84 | 156.42 |
| gpu_reflection_ms | 75.99 | 76.52 |
| gpu_reflection_atmosphere_ms | 103.57 | 104.61 |
| gpu_water_ms | 11.51 | 11.78 |
| gpu_atmosphere_ms | 327.53 | 143.09 |

Preparation costs fall by about 36%, but median walking frame time barely
changes (872 to 869 ms). The average includes periodic synchronous terrain
builds and software-renderer variation. This does not establish a comparable
interactive FPS improvement. Even with grass disabled, rendering and terrain
construction consume much more time than the optimized placement step.

## What changed

Placement computes a conservative upper bound on Gaussian acceptance for each
terrain triangle. Candidates outside that bound are rejected before square root,
barycentric geometry and per-root exponential work. The random sequence and
candidate identities stay unchanged. Sorting computes each squared distance
once and sorts compact keys, then gathers the near/far instance buffers. This
avoids recalculating double-precision distances inside every sort comparison.

Three isolated CPU runs per version alternate order (before/after,
after/before, before/after), each with twelve placements and the first two
discarded. Median process CPU time falls from **52.505 to 34.6775 ms (34.0%)**;
median wall time falls from 52.586 to 34.683 ms. Every iteration produces the
same **162,828 blades**, with the same checksum of every root, up vector and
variation component. This microbenchmark measures placement alone, without
rendering, sorting, uploads or terrain construction.

## Partial updates and remaining costs

The existing patch cache already avoids rebuilding every frame. It moves after
15% of draw distance (9 m in this scene), or whenever the terrain mesh revision
changes. A 60 m grass radius therefore encounters patch movement updates as well
as the terrain's roughly 10 m update threshold. A paused stationary view reuses
the entire scene; comparing it directly with walking measures that cache benefit
as well as movement costs.

An incremental patch would reduce CPU placement and upload spikes, but it needs
stable terrain tile identities: current candidates are seeded by triangle index,
and changing LOD can change those indices. Reusing old roots across arbitrary
mesh revisions could leave blades floating or buried. A future tiled cache should
rebuild changed tiles, retain overlap and discard stale background jobs; moving
placement and sorting into the terrain job is another way to reduce render-thread
stalls. Neither is needed to retain the measured local optimization here.

Rendering remains a separate cost. Grass is already instanced and uses near/far
blade geometry, but it is drawn in both the main and reflected scenes. The trace
includes that work in `gpu_opaque_ms` and `gpu_reflection_ms`; the preparation
stage's GPU time is zero. Pass-specific culling, reduced distant blade detail
and less overlapping grass are candidates for the later rendering tasks. They
need measurements on the target GPU before choosing a larger redesign.

## Reproduce

Build with `BUILD_TESTING=ON`, then run the archived scene rather than relying
on subsequent edits to the working configuration:

```sh
cmake --build build-codex --target PlanetSimulation foliage_benchmark -j 2
LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=2 xvfb-run -a python3 scripts/benchmarks/camera_movement.py --binary build-codex/PlanetSimulation --replay docs/journal/benchmarks/movement/input.json --config docs/journal/benchmarks/movement/scenario.json --output-dir build-codex/camera-movement
./build-codex/tests/foliage_benchmark docs/journal/benchmarks/movement/input.json
```

To measure normal interactive walking on the target GPU, omit the software-renderer
variables and launch `./build-codex/PlanetSimulation --performance-trace
build-codex/live-walking.csv`. Record stationary and walking intervals separately,
including whether `T` was paused. Use `cpu_foliage_ms` and its placement/sort/upload
breakdown for preparation spikes, `cpu_mesh_ms` for installation stalls, and the
GPU pass columns for rendering costs.

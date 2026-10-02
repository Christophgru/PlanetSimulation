# Procedural grass, quads and terrain-buffer reuse

The user requested a closer match between grass distance levels and that all
random placement and Perlin noise run on the GPU. Grass sinking is removed;
landscape sinking remains independent.

## What crosses the CPU/GPU boundary

Terrain already uploads interleaved positions/normals/colors and triangle
indices. Grass exposes those existing buffers as OpenGL 3.3 R32F and
R32UI buffer textures: these are views, not copies. A grass plan uploads only
**one four-byte triangle ID per selected patch**. The seed, density, Gaussian
width, height/lean ranges, biome controls, wind frequencies and camera are small
uniforms. There are no uploaded grass vertices, individual root records,
random-value arrays, wind textures or noise fields. A cached frame uploads zero
grass buffer bytes. `foliage_upload_bytes` in the performance CSV records the
actual triangle-list payload per frame, excluding uniforms and driver overhead.

The CPU still scans terrain triangles, computes conservative patch bounds,
reserves a hard candidate budget, sorts patches and selects draw geometry. It
does not generate production grass roots. GLSL hashes the triangle ID,
placement seed and candidate slot to construct uniform barycentric samples;
then it generates height, lean, orientation and palette variation. The shader
reads terrain normals/colors at those roots and performs Gaussian, water,
snow, slope and biome rejection. The existing CPU root generator is retained
as a comparison oracle for tests and the historical placement benchmark.

OpenGL 4.3 uses a compute pass to evaluate placement and wind once per candidate,
compact accepted blades into two GPU queues and generate indirect draw counts.
Each generated blade takes 64 bytes; both queues reserve space for the full
candidate plan, plus two 16-byte draw commands (`128 * capacity + 32` bytes). Capacity retains the largest plan until cleanup.
That is GPU working memory, not transfer. Interactive rendering never reads the
counts back; capture diagnostics do. Core OpenGL 3.3 retains procedural vertex
generation, and `compute_placement=false` selects it explicitly.

The current frozen capture independently confirms 100,000 reserved candidates,
8,666 patch IDs, 10,984 drawn blades, 130,716 vertices and 108,748 triangles.
Its initial grass upload is 34,664 bytes and the next three cached frames upload
zero bytes. The indirect queues reserve 12,800,032 bytes; that allocation is
distinct from CPU-to-GPU traffic. Culling on/off and replay PNG hashes match on
the current Xvfb host. The [checkpoint evidence](procedural/validation/todo-grass-evidence.json)
preserves these counts and hashes.

At the frozen scene's 100,000-triangle land ceiling, the interleaved land mesh
and indices upload 12,000,000 bytes: about 346 times its grass patch list.
Grass is only 0.288% of that combined land-plus-grass buffer payload. This
comparison excludes water, uniforms and driver overhead. It supports focusing
future transfer work on persistent terrain tiles and partial mesh updates,
while the existing timing study points to atmosphere and opaque grass execution
for frame-time improvements. Lower buffer traffic alone is not an FPS result.

The user subsequently removed the separate horizon tuft layer and all its
configuration controls. The sole grass layer uses `enabled`, `draw_distance_m`
and `max_blades`; the working scene retains its configured cutoff. The shared terrain
buffers and four-byte triangle list still bound CPU-to-GPU traffic.

## Shape and LOD

Following [Quick_Grass's geometry selection](https://github.com/simondevyoutube/Quick_Grass/blob/main/src/base/render/grass-component.js),
six-segment strips use 14 vertices/12 triangles, and distant tapered quads use
4 vertices/2 triangles. A top edge at 10% of root width makes both low triangles
nondegenerate. Current triangle bounds select high/low geometry every rendered
frame, including movement within a cached plan. No root regeneration or buffer
upload is required for a geometry swap. The October 2 clean 50/50 CTest run also validates this geometry checkpoint.
`GPUGeneratedNearRootsHotSwapWithoutAnUpload` and
`ConfigurableQuadDistanceChangesGeometryWithoutReplacingRoots` exercise the
production GPU paths; full-blade pixel checks cover interior shading agreement.
The requested six/one polygon convention counts quads: the actual rasterized
counts are twelve/two triangles. The existing geometry is already active.

A triangle stays detailed until its
nearest possible root has passed the detail threshold; by then all blades are
straight, so removing intermediate vertices preserves their shape. Reflection
passes share the main camera's geometry choice.

The fragment shader evaluates the same nonlinear base-to-tip palette for both
geometries. This avoids vertex interpolation turning the low quad into a
uniformly brighter blade. Eight stable retention tiers use deterministic pixel
coverage dithering instead of sinking or shrinking. Grass roots and heights
remain fixed while coverage fades. Dithering can be visible at low resolution;
the high/low swap does not perform temporal crossfading.

A full-blade pixel regression exposed a second mismatch: interpolating a side
UV directly across a tapered quad produced different interior shading from six
segments, despite identical edges. The shader now interpolates physical lateral
offset and normalizes it by the local blade width per fragment. Before the fix,
the neutral-light swap differed by up to 13 RGB levels; the regression now
allows at most one rounding level and less than 0.02 mean channel error.

Candidate acceptance also used to divide by batch size, reshuffling visible
roots when a power-of-two slot boundary was crossed. Candidates now use their
stable slot index plus a seeded fraction as a density rank. Gaussian expected
count controls a smooth coverage ramp from that rank to 1.2 times the rank.
Growing batches preserves existing blades, and movement fades density instead
of abruptly toggling roots. The conservative CPU bound still reserves all
potentially visible slots. Terrain topology changes can still reseed roots;
this does not establish persistence across a newly tessellated mesh.

## Terrain color

The terrain grass biome and foliage now share `shaders/foliage/palette.glsl`.
Ground uses midpoint tip variation, linear RGB (0.634375, 0.74375, 0.284375).
This matches the unlit center of a typical blade tip. Terrain grain, slope
shading and distinct foliage lighting can still change their displayed colors.
Beach, snow and seabed retain their existing planet tint. CPU terrain color
factors remain biome classification data so this appearance change does not
change placement or terrain tessellation.

## Early frustum rejection

The GPU tests conservative triangle spheres, expanded by the maximum configured
blade reach, against the active pass's six frustum planes. Rejection occurs
immediately after loading positions, before normals/colors, random root
sampling, biome checks and wind. Main and reflected views use their own
frusta. `frustum_culling=false` supports an identical-image control. This is
early rejection in both generation paths. Compute compaction reduces drawn
instances; the OpenGL 3.3 vertex fallback retains reserved draw counts. It avoids expensive work on invisible grass without a new
CPU buffer upload or a requirement for compute-shader support.

## Wind configuration

`foliage.wind_noise` accepts `gust_frequency`, `direction_frequency` and
`flutter_frequency` in cycles per metre (0.001–100), `speed_multiplier` (0–16),
and integer `seed`. Defaults retain the 12.5 m / 50 m / approximately 1.4 m
fields. The speed multiplier is applied before the existing 8192-second clock
wrap, preserving periodic continuity. A zero multiplier freezes wind. Both
main and reflection passes share the same procedural field and clock. No CPU
noise samples or GPU noise texture are required.

## Transfer accounting

The frozen 40 m / 100,000-candidate grass view selects 8,666 terrain triangles.
Its only grass buffer upload is **34,664 bytes**. The compute pass reserves
12,800,032 bytes on the GPU and draws 8,678 detailed blades plus 2,306 quads in
the main view: 130,716 vertices and 108,748 triangles. The fallback reserves
686,640 vertices / 486,640 triangles before shader rejection. These are distinct
work counts: the candidate budget is not the number of surviving blades.

The older CPU-root capture of the same frozen scene uploaded 1,243,200 bytes
for 31,080 root records; the current triangle list is 97.2% smaller. Placement
and retention changed, so this is payload accounting, not an identical-output
performance comparison. The removed horizon layer is excluded from this
comparison. Terrain's existing mesh upload is also excluded; its buffers are
shared by both procedural paths without copying. Uniform and driver overhead
are not measured by `foliage_upload_bytes`.

## Validation and measurements

Real GLSL tests cover placement/terrain-buffer reads, quad hot-swapping without
uploads, fixed roots and heights, biome rejection, both poles, wind continuity,
frustum rejection and compute/fallback position agreement. Full-scene captures
check walking, unchanged terrain tessellation, exact paused replay and identical
images with culling enabled/disabled. Terrain pixel tests check the shared tip
palette's channel ratios under neutral light and the narrow beach transition.

The recorded October 1 benchmark used Mesa llvmpipe through the EGL test-window
harness while that host's managed sandbox blocked X11 sockets. Those timing
measurements have not been rerun on the current host.

Fresh October 2 validation uses GCC 13, RelWithDebInfo and Mesa llvmpipe under
native GLFW/Xvfb with two driver threads. All **49 CTest entries pass in one
clean run (286.14 s)**, including native camera controls, astronaut input/reload,
the full grass/HDR/reflection capture and terrain-shadow checks. The previous
astronaut screenshot timing failure does not recur. The raw
[full-suite log](procedural/validation/todo-grass-validation-tests.log) and
[audited payload evidence](procedural/validation/todo-grass-evidence.json) are
preserved. Physical GPU performance remains unverified by this validation.

The current 20-image gallery and its source fingerprint were refreshed in the
astronaut commit; this checkpoint changes documentation only. The user removed
the separate horizon layer, so validation covers the existing single layer at
its configured draw distance. It does not claim whole-horizon coverage or a
hardware frame-rate improvement.

## Paired walking benchmark

The [vertex fallback](procedural/vertex/summary.json) and
[compute path](procedural/compute/summary.json) use the same frozen 40 m scene,
640 × 360 resolution, eight frames and 2 m walking steps with frozen orbit time.
Frames 2–6 are measured; two warmup frames and final readback are excluded.
Runs were serial, with two llvmpipe threads and no concurrent build/render job.
This is one five-sample software-renderer pair, not a hardware FPS guarantee.
Both final PNGs are byte-identical (SHA-256
`3d55c2fd5debd5a45aed3ddbcc09f07e305fa442132e58826623e50cd632df67`).

| Mean time / measured work | Vertex fallback | Compute |
| --- | ---: | ---: |
| Frame wall time | 3125.60 ms | 2792.21 ms |
| GPU frame time | 2452.93 ms | 2115.67 ms |
| Opaque GPU pass | 1071.50 ms | 711.78 ms |
| Reflection geometry GPU pass | 175.56 ms | 208.36 ms |
| Reflected atmosphere GPU pass | 193.14 ms | 193.60 ms |
| Main atmosphere GPU pass | 934.08 ms | 923.16 ms |
| CPU mesh stage | 661.54 ms | 665.63 ms |
| CPU grass preparation | 10.70 ms | 10.52 ms |
| Grass rebuilds | 2 | 2 |
| Triangle-ID bytes uploaded | 70,956 | 70,956 |

Mean frame time falls 10.7% in this pair; the median falls from 2439.38 to
2092.56 ms. Compute mainly reduces opaque work, while dispatch/compaction costs
make the sparse reflection pass slightly slower. Grass preparation is a small
part of the frame. Capture terrain construction is synchronous: one measured
rebuild costs approximately 3.3 s; the interactive renderer performs that work
in its existing background terrain job. CPU and GPU timings overlap and must
not be summed as independent delays.

Next useful targets are main-atmosphere execution, sharing placement/wind work
between main and reflection passes, and reducing terrain rebuild/transfer cost.
The frozen planet's land mesh alone is 12 MB, compared with 34.7 KB for its grass
plan. Stable terrain tiles with partial buffer updates or GPU terrain generation
would address a much larger transfer than eliminating the remaining triangle
IDs. These should be profiled on the target graphics card before choosing a
larger redesign.

Reproduce either path with the same shader tree and the stored scene settings:

```sh
python3 scripts/benchmarks/camera_movement.py --binary build-grass/PlanetSimulation \
  --replay docs/journal/benchmarks/procedural/vertex/walking/input.json \
  --config docs/journal/benchmarks/procedural/vertex/scenario.json \
  --output-dir build-grass/recheck-vertex --frames 8 --warmup 2 --cases walking
```

The `--config` file is the `scenario` object from the stored input; use its
compute counterpart for the second run. Run with an OpenGL display, or the
EGL harness and environment documented in `todo.md` on this restricted host.
The stored summaries include commands, executable/shader/input hashes and raw
per-frame CSVs.

The [full 41-test suite](procedural/validation/full-suite-before-transition.log)
passed in 317.60 s before the final transition correction. After that correction,
the [affected GL and full-scene capture suites](procedural/validation/final-grass-and-capture-tests.log)
passed in 83.62 s. The dedicated [transition regressions and compute parity](procedural/validation/transition-regressions-after.log)
also pass; their retained before-fix logs reproduce both failures. Five native
X11 input checks cannot be registered here because xdotool/ImageMagick are absent.

## Configurable quad distance and local animation

`foliage.quad_distance_m` selects the end of the blade-to-quad morph in metres;
morphing starts at half that distance. Zero preserves the automatic
`min(15, draw_distance_m / 4)` threshold. Positive values up to 400 are
clamped to the draw distance. It affects geometry and shading in the vertex
and compute paths, including conservative triangle-bound selection, without
changing root placement, density, or retention tiers. Runtime changes update
draw selection without a grass upload; JSON reload uses normal scene reload.

Interactive grass time advances with wall time independently of `T`, `Y`,
and `U`. Whole-frame reuse is disabled while enabled foliage has nonzero wind
strength and speed, so pausing orbital motion cannot accidentally freeze a
cached grass image. Capture/replay wind remains tied to the explicit capture
time for deterministic evidence. No additional GPU buffers are uploaded.

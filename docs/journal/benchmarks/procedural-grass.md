# Procedural grass, quads and terrain-buffer reuse

The user requested a closer match between grass distance levels and that all
random placement and Perlin noise run on the GPU. Grass sinking is removed;
landscape sinking remains independent.

## What crosses the CPU/GPU boundary

Terrain already uploads interleaved positions/normals/colors and triangle
indices. Both grass layers expose those existing buffers as OpenGL 3.3 R32F and
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

Using only rules for every triangle would remove even the triangle-ID list,
but would evaluate large amounts of invisible terrain or require GPU work-list
construction and indirect draws. The small bounded plan retains core OpenGL
3.3 support while avoiding per-blade transfer. Moving computation to the GPU
reduces transfer; it does not guarantee a higher frame rate.

## Shape and LOD

Following [Quick_Grass's geometry selection](https://github.com/simondevyoutube/Quick_Grass/blob/main/src/base/render/grass-component.js),
six-segment strips use 14 vertices/12 triangles, and distant tapered quads use
4 vertices/2 triangles. A top edge at 10% of root width makes both low triangles
nondegenerate. Current triangle bounds select high/low geometry every rendered
frame, including movement within a cached plan. No root regeneration or buffer
upload is required for a geometry swap. A triangle stays detailed until its
nearest possible root has passed the detail threshold; by then all blades are
straight, so removing intermediate vertices preserves their shape. Reflection
passes share the main camera's geometry choice.

The fragment shader evaluates the same nonlinear base-to-tip palette for both
geometries. This avoids vertex interpolation turning the low quad into a
uniformly brighter blade. Eight stable retention tiers use deterministic pixel
coverage dithering instead of sinking or shrinking. Grass roots and heights
remain fixed while coverage fades. Dithering can be visible at low resolution;
the high/low swap does not perform temporal crossfading.

## Early frustum rejection

The GPU tests conservative triangle spheres, expanded by the maximum configured
blade reach, against the active pass's six frustum planes. Rejection occurs
immediately after loading positions, before normals/colors, random root
sampling, biome checks and wind. Main and reflected views use their own
frusta. `frustum_culling=false` supports an identical-image control. This is
vertex-stage rejection: reserved candidates and submitted primitive counts
remain unchanged. It avoids expensive work on invisible grass without a new
CPU buffer upload or a requirement for compute-shader support.

## Wind configuration

`foliage.wind_noise` accepts `gust_frequency`, `direction_frequency` and
`flutter_frequency` in cycles per metre (0.001–100), `speed_multiplier` (0–16),
and integer `seed`. Defaults retain the 12.5 m / 50 m / approximately 1.4 m
fields. The speed multiplier is applied before the existing 8192-second clock
wrap, preserving periodic continuity. A zero multiplier freezes wind. Both
layers evaluate the field procedurally and share the same seed/phase. No CPU
noise samples or GPU noise texture are required.

## Validation and measurements

Validation and fixed-view measurements are being collected in `build-grass`.
Published measurements will distinguish reserved candidates from visible blades
and exclude compilation or concurrent work from timing runs. Rendering uses
Mesa llvmpipe through the existing EGL test-window harness because the managed
sandbox blocks X11 sockets. Native window/input and physical GPU behavior remain
unverified in this environment.

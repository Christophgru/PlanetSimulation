# Opt-in GPU terrain field evaluation — 2026-10-03

This implements T2 of the [CPU–GPU plan](../plan.md), using the completed
[T1 contracts](../contracts/study.md). A GL 4.3 compute pass now evaluates height,
gradients, material factors and inward sinking from indexed radial inputs. A
second phase expands canonical samples into the existing nine-float vertex
layout and generates sequential indices on the GPU. These buffers drive land,
water, shadows, reflections and procedural grass in the capture renderer.

T2 is a capture proof. CPU subdivision and sinking decisions remain, and the
full CPU compatibility mirror still supplies grass planning and astronaut
triangle contacts. The normal interactive backend remains CPU. Removing that
mirror, moving grass metadata/slot planning and installing asynchronous complete
consumer generations are T3; the overall GPU terrain TODO remains in progress.
No total-frame speedup or completed CPU-mirror removal is claimed here.

## Using the backend

```bash
LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=2 xvfb-run -a \
  build-resume/PlanetSimulation \
  --replay docs/captures/replay/terrain/shoreline-detail.png.json \
  --terrain-backend compute --surface-capture shoreline-gpu.png
```

`--terrain-backend cpu` is the default and explicit override. The compute option
requires a capture/render output until T3 is complete. New sidecars retain the
actual backend; a compute sidecar selects compute on replay. Unsupported saved
field/topology versions are rejected before window startup. An explicit CPU
override can deliberately use the same saved scenario/camera on CPU. Existing
sidecars without a backend selector continue to use CPU.

If the context lacks the required compute/storage/work-group capabilities, an
explicit compute request falls back to CPU with a recorded reason. A locked
compute replay fails when its backend is unavailable, instead of silently
changing the image's backend. Oversized buffers, shader/allocation/fence failures
abort the experimental capture with a clear error; the CPU option remains
available. No incomplete generation replaces an installed mesh.

## Evaluation, transport and lifetime

`shaders/terrain/compute/field.comp` uses double radial/noise arithmetic, identical
unsigned hash/seed wrap and the CPU noise interpolation/octave normalization.
The tangent branch and finite-difference stencil match the oracle. Double
sine/cosine arrive in the field pack; fifth-power ridges use explicit products.
The shader uses double literals and precise intermediate expressions. Final
position conversion precedes sinking, matching the CPU's float-rounding order.
CPU rock-angle thresholds require two small uniform doubles, rather than a
per-vertex transcendental computation.

The 704-byte std430 pack and 32-byte radial/sink records retain the T1 layout.
Driver resource queries verify offsets and array strides. Canonical output is a
scalar float array, so three vec3 members cannot introduce a hidden 48-byte
vertex stride. Each shared endpoint is computed once and copied bit-for-bit
through the corner map. CPU triangle order, winding and the hard budget remain.

The generator queries SSBO block size/binding count and compute block/local-size/
group limits. Every allocation is checked against the block/address limits before
submission. Dispatch chunks contain at most 65,536 invocations and never exceed
the queried group count. Fields are cached in at most sixteen tiny immutable
parameter buffers; radial descriptors and corner indices are uploaded per changed
generation. Oversized whole blocks use the CPU option at this stage; streamed
panels/ranged buffers remain later work. Both active and spare logical allocations
must still fit memory; a broader residency ledger is part of T3/B1.

After field evaluation, an SSBO barrier protects expansion. After expansion,
barriers cover vertex, index, texture-buffer, SSBO and validation-read consumers.
A fence exposes nonblocking polling. Capture callers explicitly wait; normal
interactive walking never enters this experimental wait path. Land and water
are staged and both complete before either is published. RAII owns staging,
queries, fences, shader programs and outputs while the renderer context is alive.
Mesh adoption keeps the CPU compatibility vectors without uploading them and
rejects incomplete or incompatible outputs. Generation/backend keys and mesh
revisions identify the resulting buffers to current consumers.

The logical transfer counter includes descriptor/map bytes, newly uploaded
parameter packs, the rock-range uniform and all dispatch-control uniforms.
`generation_peak_bytes` counts one generation's declared GPU output/staging
storage including diagnostic storage when requested; it is not measured VRAM,
whole-scene residency or driver/JIT overhead. Staging is released after adoption.
CPU planning/evaluation counters continue to report compatibility-mirror work;
GPU mode does not hide that CPU cost. Timestamp queries cover dispatch/barrier
execution; CPU bulk, host submission, capture wait and validation read times are
reported separately in the parity log.

## Validation

Five native GL cases cover double packing, both poles, the tangent branch,
negative lattice boundaries, maximum-frequency fBm/ridged noise, extreme seeds,
SI scales, mixed LOD, shoreline splitting and sinking. Readbacks compare unsunk
height, final positions, normals, colors and indices with the CPU oracle.
Canonical endpoint bits, two incident faces per edge, outward winding and the
production triangle cap are checked. Injected small block limits and mismatched
fields cannot replace an active mesh; a poisoned CPU-vector sentinel proves
mesh adoption does not upload shaped CPU vertices.

The capture regression covers main/reflection/foliage rendering, walking/trails,
exact compute replay, explicit CPU override, genuine Mesa GL 3.3 fallback and
rejection of locked/unavailable and malformed/unknown-version compute replays.
CPU/GPU PNG comparisons have a numerical tolerance across drivers; exact equality
is recorded where observed on this llvmpipe host. Native production comparisons
and the complete suite are retained under [validation/](validation/), with
[captures/](captures/) separate from the dated gallery. No gallery provenance is
rewritten.

The production fixture contains 100,000 triangles and 50,002 unique endpoints.
Its first compute generation transfers 2,800,880 bytes, including uniforms, versus
12,000,000 bytes for the legacy shaped vertex/index upload: 76.66% less input.
Six bounded dispatches produce the draw buffers. Declared generation storage is
16,600,848 bytes without height diagnostics, or 17,000,856 bytes with them.
The production readback has maximum unsunk-height error 1.194e-12 m, zero final
position/color error and maximum normal-angle error 3.332e-8 radians. These are
observations from llvmpipe; the regression retains explicit cross-driver bounds.

One serial diagnostic run measured CPU bulk evaluation at 412.527 ms, host
submission at 313.308 ms, GPU dispatch/barriers at 174.344 ms, capture wait at
0.101 ms and validation read at 25.760 ms. Software-driver execution can occur
inside submission and overlap these intervals, so they must not be added.
These figures describe individual stages, not an end-to-end hardware speedup.
The compatibility mirror still performs 150,006 bulk field queries for this
production land mesh; compute adds work while that mirror remains.

The native 960×540 shoreline and 1920×1080 offline PNGs are byte-identical to the
previous gallery. The offline view retains 213,454 visible blades and its gated
lens flare. All 22 gallery hashes and their dated provenance remain unchanged.
The retained capture-regression results also show exact surface, compute replay,
walking/trail replay and explicit CPU-override images on this host.

The clean full build and all 58 CTest groups pass (645.09 s), including the five
native compute cases and the capture regression. Repository layout, local links,
whitespace and artifact hashes pass. The rebuilt journal contains 24 pages;
exported pages 3–7 and 24 were visually reviewed.

The [GLSL 4.30 specification](https://registry.khronos.org/OpenGL/specs/gl/GLSLangSpec.4.30.pdf)
defines double built-ins, precise expressions and storage layouts. The
[Khronos barrier reference](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glMemoryBarrier.xhtml),
[fence reference](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glFenceSync.xhtml)
and [dispatch reference](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glDispatchCompute.xhtml)
explain the visibility, completion and queried dispatch constraints used here.

Next is T3: GPU grass metadata and slot allocation, a sparse matching collision
mirror, asynchronous complete land/water/grass installation, stale/reload/allocation
recovery and hardware total-cost measurements before enabling compute by default.

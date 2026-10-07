# T3c5c2b2c1 — qualify analytic area on known GPU geometry

2026-10-07, after `41f956d`. This checkpoint completes the known-geometry
prerequisite of T3c5c2b2c. Live production terrain convergence and fresh route
comparisons remain pending. The shipping renderer/shaders, ordinary timed and
matched inspection probes, and all 43 previous executables are unchanged.
CPU terrain remains the default.

## Method and ownership

The private `terrain_area_probe` links existing product libraries and uses the
matched inspection ground shaders without modifying them. GPU ownership is
triangle rasterization, visible-surface depth, interpolated attributes, expected
eligibility and geometric plane orientation. CPU ownership is excluded readback,
analytic ray/plane intersection, differential area and distance-band integration.
No noise field, foliage allocation or native generation is rebuilt here.

Four fixtures cover a front-facing plane, a 65-degree oblique plane, that plane
with an ineligible foreground triangle, and an 82-degree grazing plane with
foreground occlusion, a 1,000 m body scale, rotation and world translation.
The foreground can cross the background plane's projected horizon. Green
background is eligible, red foreground occludes it but contributes zero area.
These are geometric fixtures, not a complete validation of production biome
transitions or native character/water compositing.

The final base viewport is 640×360. Sampling factors 1, 2 and 4 share geometry,
camera and projection. At 4×, the 2,560×1,440 image is assembled from tiles with
edges 256 and 191. Both methods use the **unchanged full projection**, a translated
full-size viewport, and a bounded scissor. This avoids per-tile projection
rounding changing silhouette membership. The largest GPU target has two RGBA32F
attachments plus depth: **2,359,296 logical bytes (2.25 MiB)**, counting depth as
four bytes per pixel. This is an attachment accounting bound, not an NVML peak
measurement. CPU raw maps still occupy up to 112.5 MiB; analysis temporaries are
additional. All readbacks synchronize this separate diagnostic process and are
excluded from native cost acceptance.

For unit camera-space plane normal `n`, original pixel ray
`d=(x/fx,y/fy,-1)`, and plane intersection depth `z`, the differential area is
`4(z·s/r)² / (W H |fx fy| |n·d|)`, with body metres-per-radius `s` and uniform
model radius `r`. Recorded weighted/unweighted derivative areas supply expected
eligibility; their magnitude is not the new geometric denominator. Intersecting
the original pixel ray corrects sampled position interpolation before measuring
body-local 0–5, 5–15 and 15–30 m bands. Nonuniform model scale is rejected.

The exact reference does not consume GPU areas, normals or pixel samples. It
transforms the float-rounded input vertices/matrices in double precision, clips
the plane polygon against the homogeneous frustum and foreground screen-edge
halfplanes, then integrates polygon/circle intersections through exact segment
and sector areas. This handles the grazing horizon without projecting invalid
foreground rays onto the background plane. Circle, square, half-circle and
reversed winding checks validate the reference primitives. The original ideal
construction is retained alongside actual float-rounded inputs. Each rounded
triangle is integrated on its own plane; the reference does not assume that
rounding preserves perfect coplanarity across the original quad.

Before each GPU batch the harness publishes its gates: **4× reference error
below 1% and 2×/4× change below 1% in every band**, at least 100 eligible 4× samples
per band, and **tile-size change below 0.01%**. An undersampled band does not pass.

## Final results

One scoped software CTest passes in **53.80 s**. The same four fixtures and 16
GPU snapshots pass through the actual native Quadro context. Each snapshot has
two float maps, exact inputs, readback/resource counts and a context receipt.
The archive validator independently reconstructs all pixel rays, areas, bands
and gates from **32 final GPU snapshots**; it explicitly reuses the frozen exact
polygon/circle oracle. Software GL is llvmpipe, not hardware timing evidence.

| Fixture | Software maximum 4× area error | Quadro maximum 4× area error | Quadro maximum 2×/4× change |
|---|---:|---:|---:|
| Front plane | 0.00135% | 0.00135% | 0.03480% |
| Oblique plane | 0.03036% | 0.03020% | 0.03207% |
| Foreground silhouette | 0.03073% | 0.03056% | 0.05918% |
| Scaled grazing silhouette | 0.34497% | 0.33410% | 0.42118% |

The largest tile-size change is 0.000551%, below the declared 0.01% gate.
The sparsest final band has 1,683 eligible samples on software GL and 1,690 on
Quadro. All native hardware receipts identify `Quadro M1000M/PCIe/SSE2`,
OpenGL **3.3.0 NVIDIA 580.178.04**, and UUID
`GPU-2cefee61-6b3b-a670-c390-c7449dad3f79`. GL 3.3 suffices for these diagnostic
raster shaders; this is not a new interactive compute backend or RTX result.

## Retained qualification failures

An initial reference assertion exposed a foreground triangle crossing the
grazing plane's horizon. Projective silhouette clipping fixes the reference;
the final exact oracle uses actual rounded inputs instead of ideal coordinates.
The first GPU batch at 320×180 failed the grazing scene: nearest-band 2×/4×
change was **3.16%**, and ideal-reference 4× error was **1.36%**. The full inputs,
maps, report and log remain under `validation/preflight/coarse/`.

A 640×360 batch passed reference and convergence gates but failed the stricter
tile gate on the ordinary silhouette: **0.01571%** nearest-band area change.
Off-axis per-tile projection multiplication changed one edge sample. The fixed
projection/translated viewport implementation resolves this; the superseded
batch remains under `validation/preflight/off-axis/`. Tolerances are unchanged,
and those two batches remain failures rather than final qualification evidence.

All final and superseded float maps are archived losslessly in standard XZ with
a 16-byte delta filter. Decoded lengths/SHA-256 match original readback bytes.
The 128 maps occupy 281.16 MiB encoded versus 3,382.03 MiB decoded.
Inputs, build/test logs, frozen source/executable provenance and all artifact
digests accompany them. This is diagnostic archival storage, not the later BSON
runtime cache. Existing gallery, route and matched-study artifacts stay intact.

## Remaining plan

1. **T3c5c2b2c2:** integrate the qualified bounded-tile raster/analytic accumulator
   into excluded live-generation inspections. Keep actual terrain/grass plans,
   revisions and workload/publication identities unchanged. Check production
   2×/4× per-band convergence below 1%, biome boundaries, silhouettes and
   character/other-body occlusion at the chosen sampling resolution. Retain an
   explicit composed-mask grid and quantify edge error; do not silently reuse
   an upscaled native stencil as high-resolution occlusion proof. Exercise CPU
   and compute, ordinary inspections, restoration and resource bounds.
2. **T3c5c2b2c3:** only after live qualification, freeze the method, tighten or
   explicitly bracket first-crossing overshoot, investigate topology-dependent
   root placement variance and repeat three alternating pairs for walking and
   sprint at 25/350 m. Retain the original ±5% per-pair gate and all failures.
3. **T3c5c2b3:** match recorded wind clocks, isolate opaque raster/overdraw and
   repeat stationary timing. The prior stationary regressions remain unresolved.

These results do not retroactively accept the earlier finite-quad route failures,
establish protected near-camera density, or complete terrain migration gates.

## Reproduction

```sh
cmake --build build-resume --target terrain_area_probe -j2
env LIBGL_ALWAYS_SOFTWARE=1 PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 \
  xvfb-run -a ctest --test-dir build-resume \
  -R '^TerrainAnalyticAreaIntegration$' --output-on-failure
env __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia \
  PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 xvfb-run -a \
  python3 tests/app/terrain/native/coverage/area/test_area.py \
  --probe build-resume/tests/terrain_area_probe \
  --output-dir build-resume/fresh-area-quadro \
  --expected-uuid GPU-2cefee61-6b3b-a670-c390-c7449dad3f79
env PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 \
  python3 docs/journal/architecture/terrain-gpu/async/hardware/cost/coverage/area/validate.py
```

Uses existing NumPy/GLFW/Xvfb dependencies; no new runtime or container dependency.

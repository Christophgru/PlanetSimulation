# T3c5c2b2b — matched live-plan rendered foliage coverage

2026-10-07. Work after `715ef30`. The shipping renderer, shaders, timed native
probe and 42 prior executable/driver fixtures remain unchanged. This study uses
excluded inspection routes; their frame costs cannot be substituted for native
route timing. CPU remains the default backend.

## Declared experiment

Three alternating CPU/compute pairs each at 25 m and 350 m, walking and sprint,
use the production time-zero replay and unchanged 1280×720, 100k Earth triangles,
2M blade budget, water, shadows and atmosphere. Each route is a fresh native
process with real X11 input, ready standing warmup of at least three seconds and
30 frames, and a single inspection at its first actual distance crossing. The
process closes after the inspection so its blocking delay cannot affect a later
landmark. First crossings can overshoot the requested distance; raw native roots
and distances are retained, never reported as exact 25/350 m positions.

The first CPU crossing supplies common view/projection, full grounded pose and
trail history for all six processes at that landmark/case. All use scene time
12 s for wind. Existing terrain/grass generations, planning eyes, candidates,
effective density, budgets and contact revision stay native. No worker poll,
completion wait, contact refresh, or synchronous preparation replaces them.
The private GNU linker wrapper skips CPU grass preparation only during these
controlled scene renders; ordinary native frames call the original function.
Both live workload and publication receipts plus mesh revisions must compare
exactly before/after the controlled renders.

A full scene with grass and a second full scene suppressing grass draws retain
character, water, atmospheric integration and tone mapping. Main grass writes
private stencil tag 5, terrain 2/3, character 4. After final compositing, a nearest
opaque grass pixel contributes if at least one 8-bit RGB channel differs by one
level from the suppressed-grass image. This is a composed contribution proxy:
water/atmospheric attenuation is included, and scene-wide highlight metering can
also change with grass. Both final exposures are retained. Lens flare and
application overlays follow `renderScene` and are outside this scope. Stencil
alone is not treated as final color contribution, and color difference alone is
not treated as a grass mask.

A separate RGBA32F ground pass draws the actual live terrain VAO using the common
matrices. It records body-local offsets from the common root, weighted square
metres per pixel, geometric plane normal and unweighted area. Vertex subtraction
before interpolation avoids loss of precision in pixel derivatives. The area
is the cross product of pixel derivatives of the live triangle surface. Height,
water clearance, green/beach/snow tint and interpolated-normal rock blend mirror
the placement shader. Fractional rock eligibility is its expected retention
probability, not one seed's stochastic rejection. The denominator excludes
Gaussian density falloff and pixels whose bare final stencil is not Earth
ground; water-ineligible ground and character/other-body occlusion are excluded.
This is a raster approximation to eligible visible triangle area, not total
planet area or guaranteed configured density.

Generated roots are counted separately from visible roots. A root must be in
frustum, project onto eligible bare ground, and lie at or ahead of the local
live triangle plane along its actual camera ray, allowing 0.15 m behind-plane
view depth. The plane calculation avoids pixel-center depth bias, demonstrated
by an analytical oblique-ray regression. Boundary pixels can sample an adjacent
triangle; the tolerance is explicit. Root centers can be visible independently
of their wind-bent blade silhouette. Counts and fade sums are normalized by
eligible visible area in common body-local Euclidean bands 0–5, 5–15 and 15–30 m.
Projected covered eligible area uses the final composed grass mask over those
same bare-ground pixels; it is not a physical blade count.

The primary tolerance, declared before production measurements, is ±5% for
compute/CPU eligible area, visible-root density, visible fade density and
composed projected coverage in every band. Each band requires at least 100
eligible pixels and 100 visible roots. An empty/undersampled band does not pass.
Configured versus effective density stays explicit; parity alone does not
satisfy the later protected-near-density budgeting task.

## Results

The two scoped software groups pass in **44.98 s** (matched 26.73 s,
ordinary inspection 18.23 s). Eight matched CPU/compute airless/HDR native
Quadro fixture snapshots also pass. The oblique-ray regression rejects
behind-plane and parallel rays while retaining ground/above-ground roots.
Independent archived checks cover **48 final native snapshots**, including
**40 controlled snapshots**: eight software, eight Quadro fixtures and 24
production routes. All **12,910 production inspection frames** are excluded
from cost acceptance. Every production band has at least 2,691 visible roots.
Context/NVML UUID is `GPU-2cefee61-6b3b-a670-c390-c7449dad3f79`, Quadro M1000M.

| Crossing / route | Declared passing pairs | Visible-root density ratio range | Composed coverage ratio range |
|---|---:|---:|---:|
| 25 m / sprint | 3/3 | 0.9982–1.0037 | 0.9996–1.0005 |
| 25 m / walking | 2/3 | 1.0000–1.0536 | 1.0000–1.0167 |
| 350 m / sprint | 0/3 | 0.9623–1.0379 | 0.9921–1.0008 |
| 350 m / walking | 3/3 | 0.9835–1.0097 | 0.9955–1.0073 |

**Eight of twelve pairs pass the declared combined gate.** Walking 25 m pair 1
has 5.36% higher nearest-band root density and 5.77% higher fade density on
compute, while composed coverage is only 1.67% higher. Eligible area and surface
positions are identical in that band. Its CPU/compute revisions are 1/2, topology
keys differ, and the generated near roots share no exact positions (3,644 versus
3,819). Effective density differs by only 0.56%. Placement seeds derive from
triangle IDs, so topology-dependent placement realization is a source-linked
explanation to test, not proof that density policy or lag caused the discrepancy.

The three sprint 350 m pairs fail on their **finite-quad area proxy** in the
15–30 m band: compute/CPU 0.94390, 0.93951 and 0.94340. Nearer-band areas are
identical. Expected biome/rock retention is one in this outer support, so the
proxy difference is not a change in rock probability. It decomposes into
19.69–22.10 m² of net pixel-membership loss and 28.63–32.87 m² of common-pixel
area-weight change. These values alone must not be described as actual loss of
visible ground: pixel derivatives are a finite-quad approximation, especially
sensitive at triangle silhouettes and grazing views.

A **post hoc analytic sensitivity check** intersects the saved live triangle
plane with the camera ray and evaluates its differential area at the pixel
center. Its outer-band compute/CPU ratios are **1.01073, 1.00559 and 1.01030**.
This changes the interpretation of the primary area failures and identifies the
area estimator as a required qualification step. It does not revise the
declared tolerance or retroactively accept these pairs. For unit camera-space
plane normal `n`, unnormalized camera ray `d=(x/z,y/z,-1)`, positive view depth
`z` in metres, viewport `W,H`, and projection scales `fx,fy`, the differential
square metres per pixel are `4 z² / (W H |fx fy| |n·d|)`. Saved eligibility is
recovered from weighted/unweighted derivative area, so the sensitivity keeps the
original biome probability. Pixel-edge, subpixel coverage and band-boundary
convergence still need qualification on known geometry before a fresh frozen
batch. The figure shows primary filled markers and analytic area open markers.

The shared roots are actual first-CPU crossings: **25.546, 25.048, 351.020 and
355.518 m**, respectively. Native first-crossing overshoot reaches 5.518 m;
other processes render that exact common state while retaining their own native
plans. This is a controlled common-view sample of live generations, not exact
interpolated 25/350 m route-cohort acceptance. Every native crossing and
root-to-common-root difference is retained. Sprint 350 m root-to-plan-eye is
7.53–9.43 m CPU versus 15.77–19.66 m compute. The 25 m sprint qualification
preflight observed 4.93 m CPU versus 28.03 m compute and passed its coverage
comparison; lag magnitude alone does not establish thinning. At 350 m effective
density is 57.34–58.79, versus configured 120.72. Protected configured near
density remains the separate budgeting task.

## Stationary regression repeat

Three fresh alternating pairs run the unchanged ordinary timed probe, with
1,440 measured and 1,610 excluded frames. Native p95 compute/CPU ratios are
**1.05789, 1.11058 and 1.04965**: two exceed the 5% migration limit. No outlier
is removed. The historical failed ratio 1.08533 remains retained in the movement
study. Exact paired field/topology, root/camera/planning eyes and scalar workload,
all GPU status rows, twelve published initial attempts and verified memory/device
samples pass the reused independent stationary validator.

| Pair | Native p95 CPU / compute, ms | GPU frame-span p95 CPU / compute, ms | GPU opaque p95 CPU / compute, ms |
|---|---:|---:|---:|
| 1 | 81.872 / 86.611 | 79.628 / 84.784 | 20.200 / 24.914 |
| 2 | 83.560 / 92.799 | 81.323 / 90.563 | 22.901 / 28.639 |
| 3 | 84.005 / 88.176 | 82.337 / 86.804 | 23.942 / 28.140 |

Stage percentiles are separate distributions; they cannot be added or subtracted
as a decomposition of whole-frame p95. The repeated GPU opaque distributions
are higher; CPU opaque p95 differs by less than 0.16 ms. Measured main grass
placement means increase only 0.18–0.35 ms per view, and reflection means by
0.18–0.35 ms. Resident placement uses bounded 65,536-candidate dispatch chunks,
but chunk overhead alone is not established as the cause of the roughly 4–6 ms
opaque p95 differences. Raster/overdraw work needs isolation. Wind remains active
in these timed controls; actual wind phase is not in their native receipts.
Character effect-clock intervals differ (about 3.2–21.8 s CPU versus 3.2–23.3 s
compute), and are not substitutes for wind time. Record and match wind phases
before attributing the regression to the resident implementation.

## Qualification failures and archival storage

A 6.01 s first harness attempt imported the wrong `analyze` module through the
wrapper's module path. Renaming the new module exposed a 0.16 s missing-parent
path failure. Both logs and the first generated snapshot remain. The corrected
29.93 s software preflight used the superseded pixel-center root-visibility
estimator; its eight snapshots/results remain separate from final qualification.
The current ray/plane method fixes that bias and passes the analytical case and
final scoped groups. A first production launcher failed before opening a scene
because historical `inspect.py` shadowed standard `inspect` needed by NumPy;
the subdirectory/bootstrap fixes it. All failed/superseded artifacts are explicit;
none are silently counted as final results.

Every final natural/controlled raw queue, depth, color, stencil and ground map,
full frame/performance/work/publication/memory trace, input, and discrepant sample
is archived. Large float ground maps use standard lossless XZ with byte-delta
filtering; **52 maps save 187.4 MiB** versus gzip. Decoded lengths and SHA-256
prove identical original inspection bytes. Other buffers/traces retain gzip.
The standalone validator supports both; extract XZ/gzip into a temporary folder
to use the frozen original analyzer, which accepts raw or gzip files. This is
archival encoding, not the later runtime BSON-cache implementation.

Source/shader and 42 prior executable/driver hashes match the previous checkpoint;
only the private probe and test/driver inputs change. All 43 final executable
hashes stay frozen through production and stationary measurements. Gallery and
ten historical image hashes are unchanged. This is scoped inspection/archive
qualification, not a new full CTest suite, true visible-area qualification,
coverage acceptance or migration speedup. CPU stays default.

## Next work

T3c5c2b2c first qualifies analytic area and pixel-edge/band convergence on known
planes and silhouette scenes. GPU ownership stays live-mesh rasterization of
positions, geometric planes and biome eligibility in bounded tiles; CPU owns
excluded readback, analytic area/visibility accumulation and comparisons. Use
2×/4× sampling with an explicit <1% per-band convergence target, retain native
anchors/generations and declare the method before fresh three-pair runs. Tighten
or explicitly bracket large first-crossing overshoots. Diagnose topology-driven
near-root placement variance without loosening the existing per-pair tolerance.
T3c5c2b3 matches recorded wind and isolates opaque raster cost before repeating
stationary total-cost controls. Moon/reload, final transfer/field/memory gates and
protected-near-density budgeting remain.

## Reproduction

Build `terrain_coverage_probe`; run both `TerrainMatchedCoverageIntegration` and
`TerrainCoverageInspectionIntegration` under software GL. Native Quadro routes
use the existing PRIME GLX/Xvfb environment:

```sh
env __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia \
  xvfb-run -a -s '-screen 0 1600x900x24' \
  python3 scripts/benchmarks/terrain_cost/coverage/matched.py \
  --probe build-resume/tests/terrain_coverage_probe \
  --output-dir build-resume/fresh-matched-sprint-350 \
  --expected-uuid GPU-2cefee61-6b3b-a670-c390-c7449dad3f79 \
  --distance 350 --case sprint --pairs 3
```

Use a fresh output directory; repeat with walking and 25 m. NumPy is required
for the excluded analyzer, alongside the existing GLFW/X11 tooling. No extra
runtime dependency is introduced. Historical `inspect.py` shares a Python
standard-library module name, so the new driver lives in a subdirectory and
loads standard `inspect` before adding the older driver directory.

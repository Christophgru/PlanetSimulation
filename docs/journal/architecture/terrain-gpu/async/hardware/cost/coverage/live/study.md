# T3c5c2b2c2 — qualify live analytic area inspection

2026-10-07, after `07f8563`. Live inspection qualification is tested. Fresh
three-pair route acceptance remains T3c5c2b2c3; earlier failed coverage and
stationary timing gates remain failures. CPU terrain stays the default.
Shipping sources/shaders and 43 previous executables are unchanged; only the
private coverage probe is rebuilt. A private session close-handshake fix is
included in the frozen test inputs.

## GPU and CPU ownership

Opt-in `coverage_live` extends the existing excluded matched inspection. The
GPU rasterizes the installed land VAOs, expected biome/rock retention and live
triangle planes at 1×, 2× and 4× sampling. It draws the actual Sun/other-body
land meshes and the full procedural astronaut into depth/stencil at **each**
sampling grid. Primary land writes attributes only where it wins that depth
comparison. Occluders draw first with GL_LESS, so equal quantized depths
favor opaque occluders; this conservative tie rule is explicit. The native 1×
identity check guards observed differences, while 4× has no full scene-color
reference. This is independent high-resolution opaque occlusion, rather than
an enlarged native stencil. The private vertex shader preserves the product's
world/view/projection operation order; native 1× opaque tags are checked against
the grass-suppressed composed scene.

The GPU uses the qualified unchanged-projection, translated-viewport and bounded
scissor method. A fixed 256×256 target has four RGBA32F attachments and
DEPTH24_STENCIL8: **4,456,448 logical bytes (4.25 MiB)**. The C++ readback loop
streams rows into full map files using at most **1,114,112 bytes (1.0625 MiB)**
of float/tag tile buffers. Extra normal/height/material maps are read at 1× only.
Tile/read/file sizes are verified from receipts. These are incremental resource
bounds, not a measured physical memory peak. Existing native-size composed and
legacy ground targets remain allocated; at 1280×720 their combined diagnostic
attachment bound with the new target is **42.92 MiB**, excluding product-owned
render resources. Python holds up to 450 MiB of 4× raw plane/ground maps, plus
analytic arrays; it does not share the C++ tile memory bound.

Excluded CPU analysis reuses the qualified analytic ray/plane differential-area
accumulator and original-pixel-ray position correction. Eligibility has no
Gaussian density falloff. Actual Blade queue roots use their own ray/plane depth
intersection with the existing one-sided 0.15 m tolerance and freshly sampled
opaque/eligible tags. Native plans, anchors, generation revisions and consumer
publication/workload identities remain unchanged. The probe checks complete
astronaut/trail/effect telemetry before and after restoration; subsequent normal
frames retain the zero-blocking/readback audit.

## Declared qualification gates and biome checks

Every band (0–5, 5–15, 15–30 m) must change **less than 1% from 2× to 4×**, with
at least 100 eligible 4× pixels. The 1× native opaque-tag mismatch limit is
0.01% of the viewport. No area or convergence samples are dropped at biome
thresholds. CPU double-precision checks reconstruct radial height from recorded
positions and independently evaluate expected retention from raw material and
normal attributes. GPU single-precision height rounding is bounded by eight
float epsilons times body scale. Discontinuous threshold ties within 2e-6 are
reported separately in predicate verification; the actual sampled area retains
them. Expected-retention error must remain below 0.002 absolute.

Nine separate uniform-only diagnostic renders per snapshot exercise water
accept/reject, green accept/reject, meadow/beach/snow, fractional rock retention
and a beach-transition preset on the same installed land VAO. These changed
uniforms do not modify native scene config or contribute to area/coverage
measurements. Both rejection and retention are checked, including fractional
rock retention. The beach-transition preset yields no eligible flat-fixture
pixels; its name does not establish that those particular triangles straddle a
boundary. Actual production predicates also include mixed eligible/ineligible
land. No exact discontinuous threshold ties occurred in the final snapshots.

## Results

Three scoped software CTest groups pass in **161.68 s**: ordinary inspection,
matched compatibility and the new live area fixture. The live group takes
114.86 s and covers CPU/compute, airless/HDR and stationary/3 m walking states
at 640×360. Eight corresponding native Quadro fixtures pass with verified UUID.
Four additional native production snapshots use the unchanged 1280×720,
100,000-triangle and two-million-candidate workload: one CPU/compute pair at a
25 m walking crossing and one at a 350 m sprint crossing. All route frames are
excluded from cost acceptance.

| Batch | Live snapshots | Maximum 2×/4× band change | Native 1× tag mismatches |
|---|---:|---:|---:|
| Software fixtures | 8 | 0.78655% | 0 |
| Quadro fixtures | 8 | 0.76187% | 0 |
| Production walking 25 m | 2 | 0.04023% | 0 |
| Production sprint 350 m | 2 | 0.27135% | 0 |

The final **20 live snapshots / 60 sampling grids / 180 biome diagnostics** all
pass. Raw reconstruction also checks 64 native/controlled queue-depth snapshots
and 28 matched scene receipts, their frame/pose joins, publication/workload
identities, resource counts and device receipts. The validator explicitly
reuses the frozen qualified analytic accumulator; production area has no new
independent exact polygon oracle. The earlier exact known-geometry reference
remains the mathematical prerequisite.

Production height rounding is at most 0.2212 mm and the largest CPU/GPU expected
retention difference is 1.172e-6. The walking view contains 5,087 other-body opaque
pixels at 1× and 81,442 at 4×; the sprint view contains none. Astronaut masks grow
from roughly 10,100 native pixels to 161,000 at 4×. Nonprimary opaque pixels
contribute zero primary-land attributes/area at every grid. Native Quadro
receipts identify UUID `GPU-2cefee61-6b3b-a670-c390-c7449dad3f79`; software GL is
llvmpipe. There is no new RTX or native timing claim.

The production routes retain actual native crossings: walking CPU/compute
**25.3481/25.2052 m**, sprint **350.9676/350.8143 m**. Common inspection controls
use each CPU reference crossing while preserving both native generations/plans.
The sprint outer eligible areas differ (760.4223 versus 758.3220 m²), despite
matching near bands; no common topology is substituted. Both single pairs meet
the original ±5% coverage ratios and sample minimums. They are qualification
observations, not the required three alternating pairs across all four route
cases, and do not retroactively accept earlier failed samples.

## Native composed-mask limitation

Full grass and grass-suppressed scene renders retain their native display grid,
shared wind/pose/trail/view and real water/atmosphere/tone composition. A grass
stencil pixel with at least one RGB code of contribution remains the declared
color proxy. Only this displayed mask is expanded for area weighting; land,
eligibility and opaque occlusion are measured afresh at higher resolutions.

The analysis separately sums eligible area touching a four-neighbour native
mask boundary. On production walking this reaches **13.45%** of near-band
eligible area (0.618% middle, 0.0392% outer); sprint reaches **7.14%** near the
astronaut. This bound describes a native-pixel mask-edge perturbation, not a
proof of supersampled compositing or physical blade coverage. The metric remains
explicitly display-resolution coverage. Fresh pair comparisons must retain this
scope and edge uncertainty; they must not claim a 1% physical compositing error.

## Retained preflights and harness correction

The first 320×180 live fixture fails the unchanged area gate with a **1.608%**
middle-band 2×/4× change. The final 640×360 base viewport resolves its
undersampling, without loosening the 1% gate. A larger preliminary run failed
because the analyzer was changed during that run and expected a new biome schema;
its partial raw maps/logs remain as a failed, unfrozen preflight. A subsequent
passing preliminary software batch began during a probe rebuild and is retained
separately; only the repeated frozen final batch is qualification evidence.

A preliminary Quadro batch completed four passing inspections but hit the test
session's close race: the application exited successfully before another control
presentation row. `NativeSession.close()` now writes the close control, joins
the process, then checks the complete trace, clean exit and native audit. It
requires no hypothetical final presentation after shutdown. Eight fresh Quadro
snapshots and all three repeated software groups pass after this correction.

Complete final and preflight raw maps are losslessly archived with decoded
lengths/SHA-256, frozen inputs/provenance and artifact hashes. Standard XZ with
a 16-byte delta filter stores new live float/stencil maps; deterministic gzip
stores compatibility payloads. This is diagnostic archival storage, not the
later versioned BSON runtime cache. The 2,177 raw payloads decode to
10,000.11 MiB and occupy 2,168.04 MiB encoded; identical encoded payloads are
reused by content hash during archival.

The first two archive-validation passes verified payloads and fixture
reconstructions but found missing native and legacy matched analysis sidecars
for the four production snapshots. Those summaries and masks were generated
from the already archived raw queues, depths and composed images; no captured
map or gate changed. Both failed packaging checks are retained under
`validation/logs/preflight/live-archive-*-validation.log`. Every native, matched
and live sidecar is audited before the final repeated reconstruction.

## Next task and reproduction

Resume **T3c5c2b2c3** with the qualified method frozen: three alternating pairs
each walking/sprint at 25/350 m. Preserve the original ±5% per-band/per-pair gate,
explicitly bracket first-crossing overshoot and investigate native topology/root
placement variance. Keep native composed-mask scope and edge uncertainty in all
claims. Stationary wind/raster investigation b3 remains separate; CPU default and
all later migration/near-density gates remain.

```sh
cmake --build build-resume --target terrain_coverage_probe -j2
env LIBGL_ALWAYS_SOFTWARE=1 PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 \
  xvfb-run -a ctest --test-dir build-resume \
  -R '^(TerrainCoverageInspectionIntegration|TerrainMatchedCoverageIntegration|TerrainLiveAreaIntegration)$' \
  --output-on-failure
env __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia \
  PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 xvfb-run -a \
  python3 tests/app/terrain/native/coverage/test_inspection.py \
  --probe build-resume/tests/terrain_coverage_probe --live \
  --expected-uuid GPU-2cefee61-6b3b-a670-c390-c7449dad3f79 \
  --output-dir build-resume/fresh-live-quadro
env __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia \
  PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 xvfb-run -a \
  python3 scripts/benchmarks/terrain_cost/coverage/matched.py \
  --probe build-resume/tests/terrain_coverage_probe --live --pairs 1 \
  --expected-uuid GPU-2cefee61-6b3b-a670-c390-c7449dad3f79 \
  --case sprint --distance 350 --output-dir build-resume/fresh-live-sprint
env PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 \
  python3 docs/journal/architecture/terrain-gpu/async/hardware/cost/coverage/live/validate.py
```

No new runtime dependency, user-facing control or shipping shader is introduced.

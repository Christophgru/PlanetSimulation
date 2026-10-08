# Relief-aware terrain sinking

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

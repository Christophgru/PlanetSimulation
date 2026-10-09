# Relief-aware terrain sinking

## Bounded local detail design — 2026-10-09

The next implementation uses the existing longest-edge planner, with an opt-in
ground-centered detail disk. CPU workers own selection, shared edge IDs and
error probes. GPU compute owns bulk displacement and normals; sparse contacts
use the same immutable field and policy. Both incident faces split together,
preserving a closed shell without introducing a second mesh or GPU readbacks.

`local_detail_radius_m`, `local_edge_m`, `local_error_m` and
`local_transition_m` select physical edge/error targets. The core disk receives
priority over its transition and distant curvature refinement. The existing
triangle cap and staged/resident admission remain authoritative; capture metadata
reports actual local edge/error maxima and infeasibility. These are sampled
error targets, not a mathematical error bound between arbitrary probes.

Planet-fixed noise accepts `wavelength_m` instead of dimensionless `frequency`.
It resolves to radius-in-meters divided by wavelength, retaining the existing
eight-noise parameter block. Normal probes use at most 5% of the shortest active
wavelength. Unresolved octaves fade using the shared policy's physical spacing;
the core has zero sinking. A nonzero finest spacing introduces topology version
3; legacy versions 1 and 2 retain their layout and behavior.

This pass will expose and verify the feature before enabling it in production.
Rebuild distance shrinks with the selected disk, but current background planning
can still lag a moving camera. Exact projection onto parent triangles and a
continuously streamed detail patch remain outstanding in the same TODO row.
Stationary centimeter targets must not be represented as a moving-camera guarantee.

### Implementation receipt

Local targets and physical noise are implemented. Existing production defaults
remain unchanged because whole-shell CPU planning is too slow for a small moving
patch. To opt in, add these keys to a planet's existing `terrain_lod`:

```json
{
  "relief_sinking": true,
  "geometric_error_m": 0.05,
  "local_detail_radius_m": 0.5,
  "local_edge_m": 0.05,
  "local_error_m": 0.01,
  "local_transition_m": 3.0
}
```

An optional `surface_noise` layer can use
`{"amplitude_m":0.01,"wavelength_m":0.25,"octaves":1,"seed":-71}`.
`wavelength_m` specifies base noise lattice spacing in reference-sphere meters;
octaves divide it by lacunarity. Choose either wavelength or legacy frequency,
never both. At 5 cm spacing, a 25 cm layer is resolved; a 10 cm layer needs finer
edges or is filtered out. Original fields, hashes and 704-byte field/128-byte
policy layouts are retained when these options are omitted.

Stationary production probes use the unchanged 100k cap, production landscape,
water and existing noise plus the optional layer above. 410 fixed off-center
radial probes per run check the actual float render mesh against the selected
field. No compiler or other GPU workload ran concurrently.

| Camera | Core radius | Local triangles | Maximum edge | Maximum probe error | Local target limited | CPU planning |
|:--|--:|--:|--:|--:|:--:|--:|
| Production start | 0.5 m | 2,929 | 4.443 cm | 0.050 mm | no | 3,102.788 ms |
| Production start | 1.5 m | 23,122 | 5.666 cm | 0.050 mm | yes | 2,452.080 ms |
| Reported mountain | 0.5 m | 5,033 | 4.580 cm | 0.153 mm | no | 1,640.244 ms |
| Reported mountain | 1.5 m | 41,452 | 4.998 cm | 0.125 mm | no | 1,534.243 ms |

All four runs reach 100,000 triangles because distant refinement still uses the
remaining budget. They have zero open/non-manifold edges and zero reversed render
triangles. Each topology transfers 2,800,192 bytes. The very small probe errors
are observations at these points, not guarantees for arbitrary procedural cliffs.
Core diagnostics conservatively include faces whose radial bounding disk touches
the core, so edge targets also cover its boundary. `local_detail_limited` checks
both the measured edge target and sampled error ratio.

Raw sources/receipts stay ignored under `build-f5/ridge-refinement/detail-*`.
Exact parent-triangle projection, gradual publication changes and reuse/streaming
of a moving fine patch remain in the existing terrain row. Reducing rebuild
distance alone cannot cover walking/sprint motion during seconds of planning.

Verification: full RelWithDebInfo build, all 35 core CTest groups and eight
terrain integration groups have passing results (43 groups total). Five new CPU
cases cover physical units/lattice limits, config parsing, independent 1 mm
central-difference normals, actual float-mesh edges/interior probes at the equator
and pole, deterministic closed topology and cap exhaustion. All 37 NVIDIA compute
cases pass; the new physical-noise case has identical CPU/GPU positions/colors,
maximum height difference 3.34e-14 m and normal difference 2.98e-8 radians.

Version-3 standing CPU/GPU and locked replay PNGs are byte-identical (SHA256
`923e07efc8fae18756cd08da81c1fd7cc777aad85cba1a35be8aacf07785c1a8`).
Walking replay is also exact, with sparse contacts, coherent grass/shadow/reflection
revisions and 16,595,304-byte peak logical publication reservation. CPU override,
GL 3.3 fallback, version-4 rejection, reload from legacy to version 3, native
walking/sprint/flight and the held-worker grass-refresh regression pass. The
initial added walking fixture incorrectly replaced broad height noise below sea
level and moved less than the 20 cm trail threshold. Its land elevation and path
length were corrected. After the subsequent anchor/clipping fix, all four
affected renderer/capture groups pass again in 134.76 seconds; the final capture
CTest passes in 49.00 seconds.

A short stationary mountain capture uses production foliage settings and the
0.5 m opt-in disk on the Quadro M1000M, at 480×270 with 30 uncapped offscreen frames
and 1/60 s simulation steps. After separating the 2,517.443 ms first frame
(2,238.166 ms topology planning), 29 warm frames have median/p95 wall time
11.777/14.595 ms and GPU frame span 11.477/14.298 ms (nearest-rank p95). There are no warm terrain
rebuilds. The GPU field generation is 43.957 ms; terrain/grass stage admission is
280,833,196 bytes, with zero bulk CPU evaluation requests/render vectors and
unchanged 120.72/m² protected density policy, without a near-infeasible flag.
This small stationary offscreen run is not a full-resolution or moving-camera
60 FPS result. Image inspection confirms the displaced terrain renders, though
the supplied camera looks toward a dark slope; centimeter detail is concentrated
under the camera rather than across the entire distant mountain.

That moving-time benchmark also revealed two surface-capture replay dependencies:
terrain was regenerated at the final camera/time instead of its original planning
anchor, and clipping planes still came from the first frame. Version-3 surface
captures now retain the small planet-local planning-eye array, validated before
opening a window; surface clipping follows the current frame, matching interactive
rendering. The final 30-frame production capture, saved compute replay and CPU
override have identical topology fingerprints and PNGs (SHA256
`a79401ff80e915f50804f90ab34e13102acb2c89c719f5a9eee9544f085b4b83`).
Legacy captures without the optional anchors retain their previous loading path;
character captures retain their existing pose/terrain anchors. The automated
version-3 capture check now covers advancing orbital time and rejects corrupt
planning anchors before renderer initialization.

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

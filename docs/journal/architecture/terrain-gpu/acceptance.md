# Combined terrain acceptance — F5, 2026-10-08

CPU remains the supported default. Compute remains an explicit experimental
option (`--terrain-backend compute`): the final foliage profiles differ, so the
combined matched-quality full-frame speed goal is not accepted. This closes the
bounded F5 decision; it does not claim the migration performance goal complete.
No runtime setting or acceptance tolerance changes in F5.

## Protocol and scope

Fresh RelWithDebInfo `build-f5` with BUILD_TESTING, reusing pinned dependency
source checkouts but no project objects/archives. Historical benchmark data was
absent and unnecessary. Quadro M1000M, 2 GiB, NVIDIA 580.178.04, UUID
`GPU-2cefee61-6b3b-a670-c390-c7449dad3f79`, NVIDIA PRIME GLX under Xvfb,
1280×720 native GLFW, requested swap interval zero, scene scale 1, production
`configs/scenarios/solar_system.json`, paused orbit/spin and live wall-clock wind.
The enumerated RTX 3070 Ti is not the graphics device for these results.

Three alternating CPU/compute pairs per case. Stationary, saved Moon handoff
and reload have at least 240 measured frames and ten seconds after three seconds
and 30 ready warmup frames. Walking traverses 300 m and sprint 450 m with real W/Shift
input at 6/12 m/s: measure from 5 m to the endpoint, retaining labelled
input/start/endpoint exclusions. The longer sprint preserves the 240-frame minimum.
No measured outliers removed. Native wall includes presentation and observer
inspection. Exact observer/performance/native frame joins and UUID-verified
periodic NVML samples are required. Terrain/foliage logical reservation bounds include overlapping
published/staged/retiring resources; physical device-wide samples include other
processes and may miss short peaks. They are not exact allocation high-water marks.

Moon uses the same public saved airborne production pose for both backends,
inside the Moon orientation boundary, with initial reference-body change checked
before warmup. Natural flight continues; handoff/startup latency is reported
separately. Reload increments the first Earth noise seed and includes request,
preparation, publication and retirement in the timed interval. A separate rendered
coverage comparison uses common camera/pose/trails/wind and live plans at 25 m;
its blocking inspection frames are entirely excluded from timing.

## Gates

|Gate|Result|Evidence / limit|
|:--|:--:|:--|
|Terrain input transfer reduction >=75%|PASS|76.659% cold, 76.665% warm|
|Bulk CPU field-work reduction >=50%|PASS|150,006 evaluations to zero; scoped CPU stage median 96.73% lower|
|Matched rendered foliage/quality|FAIL|CPU legacy density 47.149 versus protected compute 120.72 blades/m² at the stationary start; effective candidate allowances differ|
|Full-frame p95 <=1.05 at matched quality|NOT ACCEPTED|Diagnostic ratios do not establish this gate because coverage/profile and Moon trajectories differ|
|Bounded replacement memory and coherent consumers/contacts|PASS|All completed production native audits; largest compute overlap 871,735,956 bytes <1 GiB|
|Clean build, compatibility and replay|PASS|34 core groups, 35 compute cases, 11 integration/replay groups, Mesa render/fallback checks and exact production replay|

The failed quality gate alone prevents enabling compute by default. The GPU's
protected near profile is an intentional F3 behavior, not a matched replacement
for the CPU's globally scaled Gaussian. This report closes the requested bounded
evaluation rather than changing either profile or creating another task chain.

## Canonical 100,000-triangle field work

Topology is built once outside both intervals. One warmup and three alternating
trials use 50,002 unique samples and the production field. CPU shaped-mesh input
is 12,000,000 bytes; warmed compute input is 2,800,176 bytes (76.665% less), and
cold compute input including parameters is 2,800,880 bytes (76.659% less).
Bulk CPU field evaluations fall from 150,006 to zero. Both transfer >=75% and
bulk CPU work >=50% gates pass.

|Trial|CPU generation ms|Compute CPU submit ms|GPU field ms|Capture-only wait ms|
|--:|--:|--:|--:|--:|
|1|480.493|15.6174|34.7466|35.3670|
|2|400.002|13.0627|34.6255|35.1560|
|3|361.282|11.8047|34.6794|35.3162|

Median CPU generation/packing is 400.002 ms; compute CPU submission/bookkeeping
is 13.0627 ms (96.73% lower). These are distinct scoped stages, not frame timings.
GPU median is 34.6794 ms; explicit completion waits belong only to this capture
measurement, not interactive rendering. The independent production parity test
finds zero position/color difference, height error 1.37135e-12 m and normal error
4.21468e-8 radians. CPU topology/sinking, sparse contacts and an 8-byte exposure
receipt remain: no claim that all CPU work or synchronization disappeared.

## Full-frame receipts

Ratios are compute/CPU p95. These are diagnostic at unequal foliage quality,
never accepted speedups. All pairs alternate backend order.

|Case / pair|CPU p95 ms|Compute p95 ms|Ratio|
|:--|--:|--:|--:|
|Stationary 1|72.520|47.216|0.651|
|Stationary 2|76.124|50.669|0.666|
|Stationary 3|75.586|52.881|0.700|
|Walking 300 m 1|166.417|106.463|0.640|
|Walking 300 m 2|178.153|110.910|0.623|
|Walking 300 m 3|173.090|108.749|0.628|
|Sprint 450 m 1|194.653|114.717|0.589|
|Sprint 450 m 2|198.413|105.138|0.530|
|Sprint 450 m 3|200.378|103.437|0.516|
|Moon 1|17.407|11.829|0.680|
|Moon 2|11.756|11.678|0.993|
|Moon 3|11.795|10.718|0.909|
|Reload 1|74.188|71.583|0.965|
|Reload 2|80.675|72.431|0.898|
|Reload 3|80.721|74.829|0.927|

Walking rates are 5.990–5.991 m/s; distance-aligned root differences are at most
0.01022 m and chase-camera differences 0.000296 m. Sprint rates are
11.982–11.989 m/s, root differences at most 0.08725 m and chase-camera differences
0.08447 m. Each route meets the existing ±5% speed and >=240-frame checks.
These are measured route differences, not exact CPU/compute pose identity.
Moon starts replay identically
but naturally evolves; warm-window roots differ by about 152–153 m and topology
also differs. Its functional handoff passes; its speed ratio is not matched cost.

Reload request-to-publication/retirement is 2,454–2,517 ms CPU versus 2,828–2,850 ms
compute. These are the first observed completed frames, not precise GPU fence
completion timestamps; publication and retirement land in the same observed
frame. CPU's retained worst measured frame is 2,467.45 ms, versus 198.98 ms compute:
p95 alone hides the synchronous CPU replacement stall. All three replacements
finish at epoch 2, one publication, no pending/retiring resource or failed request.
Largest measured device-wide memory is 794,034,176 bytes (757.25 MiB);
logical overlap peaks at 871,735,956 bytes (831.35 MiB) during compute reload.
These independent scopes should not be subtracted from one another.

The separate 25 m rendered inspection fails every distance band's unchanged 5%
comparison. Common projection, pose, trails and wind are identical; eligible
visible area agrees within 0.0007%. CPU/compute composed grass pixels total
571,045/580,808, which hides the distribution change:

|Band m|CPU visible roots / eligible m²|Compute roots / eligible m²|Root-density ratio|Composed coverage ratio|
|:--|--:|--:|--:|--:|
|0–5|45.847|119.870|2.615|1.104|
|5–15|45.378|69.823|1.539|1.002|
|15–30|40.087|0.000|0.000|0.482|

Fade-weight ratios are 2.877/1.692/0.000. The far compute band has zero visible
root centers and fails the existing 100-root minimum; all bands have >47k
eligible pixels. Native crossing roots differ by 0.09454 m and live topologies
differ. This is an excluded diagnostic inspection, not a qualified tiled-area
convergence study or a passing coverage claim. These receipts cannot certify
matched quality, even where composed near pixels nearly saturate.

## Verification, provenance and reproduction

Pass on the fresh build:

- 34/34 headless core CTest groups, including configuration/replay contracts,
  topology/contacts, adaptive admission and repository layout (19.38 s).
- 35/35 NVIDIA compute cases, including canonical transfer/CPU work and numeric
  production parity (41.15 s).
- 8/8 NVIDIA lifecycle, recovery, frame/reload, compute capture, atmosphere,
  native timing and native input CTest groups (195.95 s). Native input verifies
  WASD/Space ignition, 6/12 m/s, asynchronous failure/supersession recovery,
  space/Moon contacts, default CPU, Mesa GL 3.3 fallback and locked replay rejection.
- 3/3 NVIDIA astronaut, space-flight and grass capture groups, with exact
  image/state replay (69.14 s). The production boosted capture independently
  replays identical PNG bytes and pose, SHA-256
  `ac51dba81eff4c3b8788e020bc366e16395eb1161f9dc7034adf4b4f63b0f90f`.
- 30/30 Mesa GL 4.3 terrain/material/grass/wind/shadow cases (28.29 s).
  Mesa GL 3.3 atmosphere: 12 pass, one explicit GPU-only skip (2.86 s).
  The opt-in atmosphere cost benchmark remains disabled, as before.
- All 30 completed production native runs pass backend/worker/nonblocking audits.
  All 15 managed compute runs additionally check coherent land/grass/contact/optical
  consumer revisions and bounded reservations. Worker queues stay <=1; blocking
  readiness polls, server waits, bulk buffer reads, glFinish and GL memory queries
  stay zero. Route comparisons report the measured positional differences above.
  F4's scalar exposure readback still exists. Final layout/whitespace checks
  pass and rebuilding reports no work.

This is the relevant selected qualification, not a claim that every registered
CTest group was run. Historical NVIDIA GL 3.3 trail-float equality and unrelated
canonical replay limitations remain documented in F2/F4; this work changes no
runtime or tolerance to hide them. Mesa runs the complete foliage suite here.

Raw captures, JSONL/CSV, logs and memory/publication receipts stay ignored/local
in `build-benchmarks/runs/f5-20261008/`; none is required to build or run tests.
Only this compact result and reusable measurement code belong in Git. Base
revision: `0d982812cd4aed96cb6dac2bd0fb5e9faa24d8eb`. Runtime source hash:
`09378af1aead0eafd5dd860c6b597fcdff015243023e2adea5264400605781d0`.
Production config SHA-256:
`f8dd8d33d9482f6072ca15cd9a6827179f0c625fdb2d746e6fa6509bbcd94ae0`.
Native probe SHA-256:
`83942ff93d8c1b45b08cf717727cf552e011092d95b153f80898ba8b77d96ecb`.
Application SHA-256:
`d65a24291bd0dd1a11a3cd9a41e4bcb0e4eba009780cc7e542988fa0f937447e`.
Stationary/walking tool-test fingerprint:
`7c6c2e0d6bca04581c76661d2bc948b0f4f1922bf8425eeabb5002fe0aea8932`.
Final transition/sprint/coverage tooling fingerprint:
`9d998be4ad90053918c0118cfd16fddc8b1928f6236908090fa43e7f9f310f10`.
The runtime and binary hashes remain identical. Each cohort freezes source,
tools, binaries and inputs throughout its own runs.

Excluded setup attempts remain local and are not substituted into the tables:
the first production Moon launch was hidden by foliage; a public boosted launch
passed visibility. Initial wrapper frame-selection errors were corrected to
check the saved position before physics and handoff in its own observed frame.
The first 300 m sprint yielded 197 frames and failed the unchanged
240-frame minimum; all three accepted diagnostic sprint pairs were rerun at
450 m. These are explicit setup/sample failures, not filtered frame outliers.

Qualification commands (choose fresh ignored output directories; set `UUID` to
the graphics-context device, not merely the first NVML device):

```sh
ctest --test-dir build-f5 -L core --output-on-failure
export __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia
xvfb-run -a build-f5/tests/terrain_compute_tests
xvfb-run -a python3 -B scripts/benchmarks/terrain_cost/run.py \
  --probe build-f5/tests/terrain_native_probe --expected-uuid "$UUID" \
  --output-dir build-benchmarks/runs/f5-new/stationary
xvfb-run -a python3 -B scripts/benchmarks/terrain_cost/routes.py \
  --probe build-f5/tests/terrain_native_probe --expected-uuid "$UUID" \
  --distance 300 --case walking --output-dir build-benchmarks/runs/f5-new/walking
xvfb-run -a python3 -B scripts/benchmarks/terrain_cost/routes.py \
  --probe build-f5/tests/terrain_native_probe --expected-uuid "$UUID" \
  --distance 450 --case sprint --output-dir build-benchmarks/runs/f5-new/sprint
xvfb-run -a python3 -B scripts/benchmarks/terrain_cost/transitions.py \
  --probe build-f5/tests/terrain_native_probe --binary build-f5/PlanetSimulation \
  --expected-uuid "$UUID" --output-dir build-benchmarks/runs/f5-new/transitions
xvfb-run -a python3 -B scripts/benchmarks/terrain_cost/coverage/matched.py \
  --probe build-f5/tests/terrain_coverage_probe --expected-uuid "$UUID" \
  --distance 25 --case walking --pairs 1 --output-dir build-benchmarks/runs/f5-new/coverage
```

Run cohorts sequentially with frozen source/scripts/binaries. Rendered coverage
requires NumPy and deliberately cannot share a timing cohort. Existing native
input qualification exercises default CPU, forced Mesa GL 3.3 fallback and
locked replay rejection, alongside compute movement/Moon/reload. Those reduced
fixtures establish behavior, not production performance.


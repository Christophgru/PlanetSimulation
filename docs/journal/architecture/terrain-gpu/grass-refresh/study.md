# Grass replenishment during terrain planning — 2026-10-09

Interactive compute refresh now accounts for upcoming height changes and measured
preparation latency. The previously failing production mountain route stays
within the unchanged 22.5 m placement reserve: maximum offset is 16.23 m,
compared with 36.53 m / seven excess frames before this correction.

## Latency-aware correction

`InteractiveTerrain.cpp` observes planet-local motion even with preparation
pending. `GrassRefresh` forecasts third-person motion from measured 3D velocity;
surface walking previews four advances on a camera copy, resampling canonical
terrain heights. Surface prediction learns the distance actually traveled while
a patch prepares, because its 50 ms movement clamp makes GPU wall time a poor
estimate of future movement. The live camera, contacts, trails and effects do
not advance during this preview.

The prediction uses 1.5 times observed preparation time/movement, responds
immediately to slower preparation and decays slowly toward faster results.
Forecasts are bounded to 0.5 seconds; the initial surface horizon is 75 ms.
Refresh starts at half the existing reserve or when a forecast sample reaches
85% of it. Both grass-only and full-terrain submissions center their grass at
the bounded future eye, with lead limited to twice the existing reserve.
Before publication, coverage is checked against the actual camera; obsolete
predictions after a stop/turn retain the installed generation. Scene/mode
changes and large pose/time discontinuities discard stale motion.

Placement bounds, density, cutoff, candidate capacity, terrain budgets, seeds,
bounded preparation/retirement ownership and CPU rendering are unchanged.
Capture uses its original deterministic schedule and saved policy. The four
sparse height queries do not move bulk displacement or placement back to CPU.

The repeatable optional NVIDIA regression is
`tests/app/terrain/native/refresh/test_slope.py`. It uses the reported mountain
camera, unchanged production configuration, real W/Shift, paused orbits/live
wind, uncapped presentation and at least three seconds / 30 warmup frames.
No worker/fence delays are injected. All six routes pass; all recorded frames
after stopping also remain within the reserve.

| View / movement / viewport | Travel m | Wall speed m/s | Grass-only refreshes | Max moving offset m | Excess frames |
|:--|--:|--:|--:|--:|--:|
| Mountain / camera / 320×180 | 241.32 | 73.59 | 13 | 16.23 | 0 |
| Startup / camera / 320×180 | 242.04 | 62.24 | 12 | 17.03 | 0 |
| Startup / sprint / 1280×720 | 72.34 | 11.90 | 6 | 13.95 | 0 |
| Mountain / sprint / 1280×720 | 72.39 | 11.78 | 4 | 11.59 | 0 |
| Startup / camera / 1280×720 | 240.03 | 49.87 | 12 | 17.76 | 0 |
| Mountain / camera / 1280×720 | 241.56 | 60.32 | 12 | 16.75 | 0 |

The Quadro UUID is verified in every memory receipt. All generation-consumer
and nonblocking native audits pass, with no preparation failures. Peak logical
reservation remains 580,997,736 bytes (554.08 MiB), below 1 GiB; sampled
physical usage peaks at 891,355,136 bytes (850.06 MiB). The original surface
movement clamp still applies; these wall speeds are not a full-quality 80 m/s
or 60 FPS guarantee. Conservative `near_infeasible` diagnostics still occur,
including in the fast mountain run; this change does not make an insufficient
fixed candidate budget feasible or certify density in every biome.

Full build and all six focused test groups pass: `GrassRefreshTests` (five
cases), `TerrainFrameIntegration` (26.11 s), `TerrainReloadFrameIntegration`
(11.34 s), `TerrainComputeCaptureIntegration` (62.56 s, including saved exact
replay / CPU / GL 3.3), `TerrainNativeInputIntegration` (62.59 s, including
flight/Moon), and `TerrainGrassRefreshIntegration` (44.47 s).

The held-worker test now includes a stop and 25 m reverse route. Compute sprint
and true 79.95 m/s surface walking stay below 12.54 m offset, including turns;
CPU sprint also passes. Actual inspected nearby compute roots are 72 / 629 in
0–5 / 5–15 m bands around the astronaut, with 3,170 opaque grass pixels. The
fence-retirement fixture now selects a full terrain stage before holding its
fence, so an earlier independent grass refresh cannot stall the fixture.

Two separate blocking production inspections at startup and 60.04 m sprint
travel verify main-view roots against independently rasterized eligible ground.
Camera-centered 0–5 / 5–15 m densities are 121.03 / 121.97 initially and
124.31 / 120.82 after moving (configured 120.72; within 3%). These inspections
are excluded from timing acceptance. `matched/measure.py` accepts an optional
camera distance center; its default astronaut-centered analysis is unchanged.
The protected 15 m radius belongs to the camera: astronaut-centered 5–15 m
measurements include ground beyond it, where the unchanged adaptive controller
may reduce far falloff under GPU load. Those measurements here are 106.62 and
70.32/m², with soft budgets 1,024,000 and 779,936, rather than the earlier
1,600,000. Do not report them as full-density camera coverage or hide the
near-infeasible flag at the moving waypoint.

Accepted production receipts are local in ignored
`build-f5/grass-refresh/{motion-fix-clamped,motion-fix-all,motion-fix-roots}`;
held-worker receipts are in `build-f5/terrain-grass-refresh`. Final build/test
logs are `motion-fix-build.log`, `motion-fix-tests.log` (initial fixture failures)
and `motion-fix-recheck.log` (corrected fixtures pass). The initial
wall-time-only forecast in `motion-fix-slope` overpredicted clamped travel and
is excluded from acceptance. No new TODO rows or commits were added.

Run the production regression with native NVIDIA GLX:

```sh
__NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia \
  xvfb-run -a -s '-screen 0 1280x1024x24 -noreset' \
  python3 tests/app/terrain/native/refresh/test_slope.py \
    --probe build-f5/tests/terrain_native_probe \
    --output-dir build-f5/grass-refresh/production-check --all
```

## Initial worker-veto / half-margin change

The initial change allowed interactive compute grass to refresh on the installed terrain while the
CPU plans its replacement. Refresh started after half the existing placement
margin is consumed. Production's 150 m cutoff and 0.15 rebuild fraction still
reserve 22.5 m; the interactive request threshold is now 11.25 m.

The previous pending-worker veto delayed grass for the full 1.5–2.3 second
terrain planning interval measured in the [timing pass](../../../benchmarks/fps-60/study.md).
Using the entire margin as the request threshold also left no allowance for GPU
preparation and publication. These two changes address those delays without
changing density, cutoff, candidate capacity, triangle budget or placement seeds.
CPU rendering and capture/replay retain their previous refresh schedule. Saved
policies remain locked; a replay with an insufficient fixed budget still fails
explicitly rather than changing its recorded policy.

## Regression and actual roots

`TerrainGrassRefreshIntegration` runs the production GLFW loop with real X11
W/Shift input. A private linker wrapper holds only the CPU terrain builder after
loading. GPU preparation, nonblocking fence polling, publication and rendering
remain active. The test releases the worker and drains its bounded queue before
shutdown. Restoring `e769028`'s `InteractiveTerrain.cpp` in a separate local control
executable fails the same test with “Grass starved behind the held terrain
builder.”

The fixture uses an airless, green sphere with no terrain noise or landscape,
water below the ground, 10k terrain budget, density 1/m², 100k candidate cap,
production 150 m cutoff / 22.5 m margin and 320×180 presentation. It isolates
refresh and ownership; it is not a production density or FPS qualification.
The completed run used Mesa llvmpipe (LLVM 15.0.6), OpenGL 4.5.

| Backend / input | Travel m | Refreshes while terrain held | Largest plan offset m | Wall speed m/s |
|:--|--:|--:|--:|--:|
| Compute / sprint configured 12 m/s | 92.60 | 7 | 15.14 | 12.14 |
| Compute / surface camera configured 80 m/s | 91.97 | 8 | 12.00 | 12.19 |
| CPU / sprint configured 12 m/s | 91.06 | 5 | 21.20 | 12.15 |

Installed terrain generation/revision stays fixed throughout the held interval.
Compute publishes grass-only generations within the existing 22.5 m allowance;
both backends retain density 1/m² with no near-density infeasibility. Peak
compute reservation is 33,311,112 bytes, below the existing 1 GiB limit. Native
audits observe zero blocking polls, server waits, bulk reads, `glFinish` calls
or live driver-memory queries. Main, grass, contacts, shadows and reflections
retain their generation/revision agreement.

At more than 80 m of travel, a separate blocking inspection copies the actual
main-view grass queues and measures depth before/after their draws, while the
terrain worker is still held. Inspection reads are excluded from native audits;
these routes are not timing acceptance. Generated root counts in the 0–5 m and
5–15 m bands around the astronaut are 65 / 533 for compute and 71 / 518 for CPU.
Reference flat-disk populations at uniform density 1 are approximately 79 / 628;
the regression permits 40% variation for seeded sampling and camera-centered
falloff at the outer astronaut band. There are 2,485
and 1,885 opaque grass pixels respectively. This demonstrates newly reached
nearby roots and visible grass; it does not measure final atmospheric/water/
astronaut composition or certify production biome coverage.

The surface camera still applies its existing 50 ms movement-step clamp. On
this slow software renderer its configured 80 m/s produces only 12.19 m/s of
wall-time travel. That check exercises the configured input and refresh path;
it does not establish replenishment at a true 80 m/s on production hardware.

Full build and the final root-inspecting `TerrainGrassRefreshIntegration` pass
(48.97 s). Focused software compatibility checks also pass:
`TerrainReloadFrameIntegration` (32.64 s), `TerrainComputeCaptureIntegration`
(52.23 s, including exact saved replay / CPU override / GL 3.3 fallback), and
`TerrainNativeInputIntegration` (93.71 s, including reload, flight/Moon contacts
and CPU / GL 3.3 input compatibility). Logs remain local in
`build-f5/grass-refresh/{build-probe.log,test-refresh-coverage.log,compatibility.log}`.

## NVIDIA verification of the initial half-margin change

GPU access is restored. Both NVIDIA device files and `/dev/nvidiactl` open,
`nvidia-smi` succeeds and GLX creates a Quadro M1000M context. The physical
context UUID is verified in every run's memory receipts:
`GPU-2cefee61-6b3b-a670-c390-c7449dad3f79`, NVIDIA 580.178.04, 2 GiB.
The RTX 3070 Ti is visible to NVML but was not used for these native tests.

The complete held-worker regression passes on NVIDIA, including actual
79.97 m/s surface-camera travel. Compute sprint travels 90.03 m at 11.94 m/s
with seven grass refreshes and a maximum 11.95 m plan offset. The 80 m/s route
travels 90.18 m with seven refreshes and a 13.01 m maximum offset. The CPU case
also passes, including actual nearby roots and visible grass. This remains the
small 10k-terrain / density-1 fixture; production checks follow separately.

Native production runs use the unchanged scenario, 100k terrain cap, density
120.72/m², 15 m protected radius, 150 m cutoff and 22.5 m placement margin.
Orbits are paused by a camera-only replay; wind is live. Input is real W/Shift,
presentation is uncapped, scale stays 1, warmup is at least three seconds / 30
frames and no cached frames enter the moving windows. No terrain-worker or GPU
fence delays are injected. The two 320×180 runs reduce pixel work for a faster
movement stress check; their terrain and foliage settings remain unchanged.

| View / movement / viewport | Travel m | Wall speed m/s | Grass-only refreshes | Max plan offset m | Frames outside 22.5 m |
|:--|--:|--:|--:|--:|--:|
| Startup / sprint / 1280×720 | 72.28 | 11.90 | 7 | 13.70 | 0 |
| Mountain / sprint / 1280×720 | 72.35 | 11.94 | 7 | 12.81 | 0 |
| Startup / camera / 1280×720 | 240.03 | 43.81 | 19 | 17.76 | 0 |
| Mountain / camera / 1280×720 | 241.52 | 57.09 | 17 | 21.15 | 0 |
| Startup / camera / 320×180 | 243.46 | 60.90 | 20 | 15.90 | 0 |
| Mountain / camera / 320×180 | 241.46 | 54.86 | 23 | 36.53 | 7 |

All grass-only refreshes in those movement windows occur while CPU terrain
work is pending. No preparation failures or native blocking-wait/readback audit
violations occur. Generation consumers agree. Peak logical reservation is
580,997,736 bytes (554.08 MiB), within 1 GiB; peak sampled device-wide usage is
888,406,016 bytes (847.25 MiB). The existing 50 ms surface-camera movement-step
clamp reduces wall speed on this GPU; setting 80 m/s is not proof of achieving
that speed at production quality.

Separate blocking inspections at stationary, approximately 24 m and 60 m sprint
waypoints copy actual main-view grass roots, compare depth, and render matched
bare/grass images plus independent expected eligible ground area. They are
excluded from timing acceptance. At startup the measured visible roots per
eligible m² are:

| Travel m | 0–5 m band | 5–15 m band | Composed grass pixels |
|--:|--:|--:|--:|
| 0.00 | 121.40 | 119.58 | 770,614 |
| 24.82 | 121.57 | 121.02 | 629,643 |
| 60.75 | 122.06 | 119.25 | 644,704 |

Nearby density remains within approximately 1.3% of configured 120.72/m² in
these snapshots. Startup allocation reports `near_infeasible` in some frames,
including the final snapshot, despite these populated near bands. That flag
can include conservative protected triangle slot limits; it must not be
silently cleared or treated as proof that every location meets density. At the
mountain waypoints there is no eligible visible ground within the analyzed
0–30 m bands, so a near-density ratio is unavailable. Farther grass remains
visible (168 / 8,970 / 49,924 composed pixels); zero nearby roots there does not
establish a streaming hole.

**The existing grass task remains in progress.** The reduced-viewport mountain
stress run exceeds the placement allowance in seven frames. At its worst frame
(native 1657, profile 1656), the camera is 36.53 m from the published grass
anchor: 34.65 m is radial height change and the reference-sphere arc is only
10.68 m. Nearby grass allocation takes approximately 89–91 ms on the GPU;
submission to GPU-ready takes 124–131 ms and first rendered publication takes
147–162 ms in the affected interval. The CPU terrain worker is running, while
grass-only generations continue publishing. This is evidence that the fixed
half-margin trigger lacks sufficient allowance for fast steep traversal, rather
than a recurrence of the removed pending-worker veto. Exceeding the conservative
margin does not by itself prove a visible hole, but prevents a coverage guarantee.

That verification identified the remaining work in this same grass TODO: account for measured 3D
movement, including slope/height changes, and GPU preparation/publication latency
when choosing refresh timing and bounded lookahead. Preserve density, memory
bounds, installed terrain ownership and saved replay. No runtime code changed
and no new TODO rows or commits were added during this verification.

Hardware receipts are local under ignored
`build-f5/grass-refresh/{nvidia-regression,production-nvidia,production-speed-nvidia,production-roots}`
with the corresponding sibling `.log` files. `diagnostic-settle-roots` contains
an earlier inspection-driver settling-predicate failure and is excluded from
accepted results; the corrected rerun supplies every snapshot above.

Raw results, snapshots, build logs, the failing control and a prepared production
before/after route script remain local under ignored `build-f5/grass-refresh`
and `build-f5/terrain-grass-refresh`. The finished timing binaries were copied
to `build-f5/fps-60/{PlanetSimulation-timing,terrain-native-timing}` before this
change, so its measurements are distinct from the new refresh schedule. The
commit contains source, tests and compact documentation; raw benchmark data
stays local.

Run the regression with an available graphical context:

```sh
xvfb-run -a -s '-screen 0 1280x1024x24 -noreset' \
  ctest --test-dir build-f5 -R '^TerrainGrassRefreshIntegration$' --output-on-failure
```

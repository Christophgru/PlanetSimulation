# Grass replenishment during terrain planning — 2026-10-09

Interactive compute grass can now refresh on the installed terrain while the
CPU plans its replacement. Refresh starts after half the existing placement
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
shutdown. Restoring HEAD's `InteractiveTerrain.cpp` in a separate local control
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

## Remaining verification

NVIDIA device access was lost after the completed timing pass. Opening either
GPU or `/dev/nvidiactl` now returns `EPERM`; `nvidia-smi` fails with “Unknown
Error” and NVIDIA GLFW context creation fails. Mesa software rendering remains
available. The existing grass TODO remains in progress until actual production
GPU routes at both reported cameras verify continuous placement headroom,
nearby roots/density and bounded publication at real sprint / fast surface-camera
movement. A finite margin also cannot guarantee coverage through arbitrary
renderer stalls or relocation.

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

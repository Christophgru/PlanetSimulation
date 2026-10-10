# Grass preparation benchmark

Use a Release or RelWithDebInfo build with the native probe enabled. Before
changing the renderer, preserve its probe binary and a root containing its
`shaders`, `configs` and `data` (the unchanged assets may be symlinked). Keep
these snapshots and all raw output in an ignored build directory. Both builds
must use the same optimization flags and assets.

```sh
xvfb-run -a python3 -B scripts/benchmarks/grass_allocation/run.py \
  --before-probe build-f5/g1/baseline-native-probe \
  --before-root build-f5/g1/baseline-root \
  --after-probe build-f5/tests/terrain_native_probe \
  --output-dir build-f5/g1/native --device 1
python3 -B scripts/benchmarks/grass_allocation/report.py \
  build-f5/g1/native/results.json --output build-f5/g1/summary.json
```

The runner compiles a private EGL adapter, verifies the physical GPU UUID and
uses real GLFW/X11 keyboard input in the production interactive loop. Rendering
and swaps use a NVIDIA EGL pbuffer. This measures preparation and native loop
spans, without qualifying desktop presentation or sustained desktop FPS.

Three alternating pairs measure stationary and 6 m/s walking cases at 1280×720
with the production configuration. A camera-only replay pauses orbital motion;
wind and adaptive budgets remain live. Startup and warmup are excluded, query
results are drained after the measured interval, and measured outliers remain.
The runner checks frame joins, density, GPU sample completeness and native
nonblocking audits. The reporter independently reconstructs pooled statistics
and hashes raw evidence. Repeated metadata generations identify opportunities
for geometry reuse; their label is not a standalone buffer-identity audit.

For G2, preserve the G1 baseline and add
`--stages grass_metadata grass_allocation grass_roots grass_placement` to the
runner. The reporter then includes one-time root generation and per-view
placement/animation CPU and GPU costs. Root events are expected during plan
preparation, rather than every idle frame. Keep G1 and G2 raw output in separate
ignored directories.

For G3, preserve the G2 baseline and use `--camera-mode astronaut --stages
grass_metadata grass_allocation grass_roots grass_placement grass_animation
grass_draw`. The astronaut walks through real growing trails; the runner checks
grounded readiness, wall-time walking speed and trail growth. The reporter
separates per-view compaction, shared animation and geometry draws. G2 lacks
the exclusive draw timer, so compare the common enclosing opaque/reflection
passes as well. Additional G3 timer queries are observer overhead. See the
[shared animation study](../../../docs/journal/benchmarks/grass-animation/study.md).

This benchmark requires NVIDIA EGL, GLFW/X11, Xvfb, xdotool, a C++ compiler and
nvidia-smi. GPU utilization telemetry includes other processes; retain and
report contention. Raw output is optional local evidence and is never needed
for normal builds/tests. See the [measured study](../../../docs/journal/benchmarks/grass-allocation/study.md).

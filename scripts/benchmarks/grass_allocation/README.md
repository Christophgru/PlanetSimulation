# G1 grass preparation benchmark

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

This benchmark requires NVIDIA EGL, GLFW/X11, Xvfb, xdotool, a C++ compiler and
nvidia-smi. GPU utilization telemetry includes other processes; retain and
report contention. Raw output is optional local evidence and is never needed
for normal builds/tests. See the [measured study](../../../docs/journal/benchmarks/grass-allocation/study.md).

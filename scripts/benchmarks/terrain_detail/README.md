# Local detail planning receipt

Build and run from the repository root after configuring the project:

```sh
cmake --build build-f5 --target terrain_detail_benchmark -j 4
mkdir -p build-f5/terrain-detail
./build-f5/terrain_detail_benchmark > build-f5/terrain-detail/planning.csv
```

An optional scene JSON path replaces the production scene. The first planet is
sampled at the production starting latitude/longitude and the mountain reference
pose. Each variant starts with a new surface, requests a 0.5 m core, 5 cm edges,
1 cm sampled error and a 0.5 m transition, then advances twice by 0.15 m east.
All other scene/terrain/noise/budget settings are retained. Baseline topology v3
rebuilds the shell; streaming v4 can reuse its coarse parent and height probes.
The first row for each variant includes cold parent planning. Subsequent rows
are warm moving plans, rather than repeated stationary requests.

The CSV reports wall and thread CPU time, actual triangle count, counted field
height evaluations, core edge/limit status, parent reuse and topology transfer
bytes. Times are diagnostics, never machine-dependent test assertions. Run
without other compilation/rendering work when collecting wall timings; thread
CPU time is also retained to distinguish scheduling delays. Record compiler,
configuration and source revision with retained evidence.

This measures CPU topology planning only. It does not measure GPU preparation,
publication delay, desktop FPS, collision error or moving patch coverage. Use
the automated detail/parity/replay tests and native input loop separately.
The [terrain report](../../../docs/journal/architecture/terrain-gpu/sinking.md)
records the implementation and its remaining limits. Keep raw CSVs and binaries
in ignored build directories.


Collect native walking/sprint coverage independently on NVIDIA EGL:

```sh
xvfb-run -a python3 -B scripts/benchmarks/terrain_detail/movement.py \
  --probe build-f5/tests/terrain_native_probe \
  --output-dir build-f5/terrain-detail/movement --device 1
```

The helper reuses the existing native-session and EGL adapter implementations,
verifies the physical GPU UUID, and preserves real GLFW/X11 input. Three flat
micro-relief fixtures measure a 0.5 m walking core, 0.5 m sprint core and 1.5 m
sprint core for four seconds at 320×180. It retains published anchor offsets,
wall-time speed, publication counts and nonblocking/memory audits. Input delivery
at the beginning is included in reported wall speed. This is a terrain coverage
check, not a production performance/FPS cohort or a test of temporal morphing.

The helper writes all receipts and returns **2** if any measured camera sample
lies outside its requested core. That failure must remain visible while the
terrain TODO is in progress; gathering receipts successfully is not coverage
acceptance. Existing normal tests do not require NVIDIA hardware or these raw
receipts. Current coverage failures and the cold/warm planning distinction are
recorded in the linked report.

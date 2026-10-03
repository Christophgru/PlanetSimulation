# PlanetSimulation engineering journal

[Read the paper](paper.pdf) or [edit the Typst source](paper.typ). The paper
explains the physical models behind the renderer, why its approximations were
chosen, and alternative approaches. It includes 12 generated vector diagrams
and current renderer captures.

The SVG diagrams are explanatory schematics, generated without third-party
Python packages:

```sh
python3 docs/journal/make_figures.py
```

With Typst 0.15.1 or newer installed, compile from the repository root:

```sh
typst compile --root . docs/journal/paper.typ docs/journal/paper.pdf
```

The raster figures refer to the existing, versioned
[README screenshot gallery](../screenshots/). Capture commands, timestamps and
SHA-256 hashes are recorded in [generation.json](../captures/generation.json).
The Quadro M1000M benchmark plotted in the paper reproduces numbers already
reported in the project README; it is not a new measurement.

The [camera-movement study](benchmarks/camera-movement.md), also included in the
paper, records new software-renderer measurements of walking, foliage placement,
sorting and uploads. Frozen inputs, raw traces and before/after hashes are kept
beside the study. It distinguishes capture-time terrain construction from the
interactive background path and does not claim a hardware FPS improvement.

The [grass LOD study](benchmarks/grass-lod.md) measures the later single-tip
strips, shared instance buffer, eight distance levels and gradual sinking,
including the extra draw submissions and reduced distant density.

The [CPU profiling study](benchmarks/cpu-profiling.md) adds nested wall/thread-CPU
traces, top-down timelines and caller trees, bottom-up self-cost reports, and
independent Callgrind instruction attribution. Its
[standalone walking report](benchmarks/cpu/walking/report/report.html) is
interactive. The study records rejected gprofng sampling and the limits of
software-renderer measurements.

The [terrain LOD study](benchmarks/terrain-lod.md) describes eight surface
levels, shared inward offsets, a single closed mesh and fixed before/after
shoreline and grass captures. It records triangle and transfer budgets and
the limits of spatial sinking.

The [astronaut implementation study](character/astronaut.md) records planted
foot IK, camera 4, 6/12 m/s movement, gravity-dependent jumping, jetpack controls
and deterministic replay. The [Sketchfab shortlist](character/sketchfab-models.md)
compares downloadable rigs with the requested Ava Turing reference and records
the remaining download and skeleton-inspection requirements.

The [sand material study](materials/sand.md) documents pale granular beaches,
centimetre wind-ripple normal relief, distance filtering and shoreline captures.

The [two-page WebAssembly memo](portability/wasm.pdf) assesses a browser port,
with compilation/runtime diagrams, dependency changes and WebGL/WebGPU tradeoffs.

The [CPU–GPU terrain and atmosphere plan](architecture/terrain-gpu/plan.md)
audits current ownership and chooses triangular icosahedral panels for the first
migration. It defines GPU bulk fields, CPU subdivision/sinking and sparse contacts,
shared generation installation, atmosphere reductions, foliage budgets, fallback
and validation gates. Its checked worksheet contains design arithmetic, not a
runtime speedup measurement.

The [field/topology extraction](architecture/terrain-gpu/contracts/study.md)
implements its first prerequisite: a packed field oracle, indexed radial/sink
inputs, bounded height cache and validated generation keys, with exact legacy
CPU mesh/replay behavior. GPU field evaluation remains the next phase.

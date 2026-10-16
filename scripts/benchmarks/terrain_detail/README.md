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
bytes. It also retains the exact topology fingerprint and immutable-parent /
mutable-edge counts. Compare fingerprints, triangles, queries, edge/error limits
and transfer bytes when changing planner internals; a faster run alone does not
establish equivalent geometry. Times are diagnostics, never machine-dependent test assertions. Run
without other compilation/rendering work when collecting wall timings; thread
CPU time is also retained to distinguish scheduling delays. Record compiler,
configuration and source revision with retained evidence.

The `contact_*` columns separately measure construction of the sparse contact
oracle, including its owned topology copy, validation and spatial index. Its
wall/thread CPU times are excluded from planner times. `contact_index_bytes`
counts retained nodes, order arrays and current-face mappings (including shared
parent indices when used), while `contact_candidate_fingerprint`
hashes the complete ordered broad-phase results for axis/pole directions, 256
distributed directions and sampled exact mesh vertices. Compare both fingerprints
and index size when optimizing index construction. These queries do not evaluate
the displaced field; contact/mesh equality remains covered by automated tests.

For streaming contact reuse, `contact_parent_triangles` counts the shared coarse
shell and `contact_local_triangles` counts only descendants of split parent faces
in the generation's local tree. `contact_shared_parent_bytes` includes shared
parent topology/index backing. Index/topology byte counters charge shared backing
to each owner conservatively; do not add those counters to claim unique process
memory. The acceleration can change broad-phase false positives and therefore
candidate fingerprints; it must retain exact rendered-plane contact results and
the ascending current-triangle tie rule. Re-canonicalizing a topology invalidates
the acceleration and exercises the independent complete-index path.

`reference_parent_positions` counts the current unconditional three parent
positions per morph sample. `needed_parent_positions` counts only positive parent
weights and collapses identical descriptors within each vertex to one position.
This is an input-derived per-vertex requirement, not a dispatch/invocation or
timing counter. It excludes fine-field/normal work and reuse across vertices.
The current shader retains its unconditional parent phase: simply branching
around redundant evaluations showed no timing gain in the software pilot.

This measures CPU topology planning only. It does not measure GPU preparation,
publication delay, desktop FPS, collision error or moving patch coverage. Use
the automated detail/parity/replay tests and native input loop separately.
The [terrain report](../../../docs/journal/architecture/terrain-gpu/sinking.md)
records the implementation and its remaining limits. Keep raw CSVs and binaries
in ignored build directories.

An optional second positional argument records CPU scopes for the planner:

```sh
./build-f5/terrain_detail_benchmark configs/scenarios/solar_system.json \
  build-f5/terrain-detail/cpu.json > build-f5/terrain-detail/profiled.csv
```

The trace separates parent preparation, mutable refinement initialization,
initial assessment, splitting, export, canonicalization and fingerprint checks.
Use a separate run without tracing for timing comparisons.


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
lies outside its requested core, or a published core fails its configured
edge/error targets (`unmet_detail_frames`). The private native probe records
those geometry diagnostics from owned CPU scalars without a GPU readback.
That failure must remain visible while the
terrain TODO is in progress; gathering receipts successfully is not coverage
acceptance. Existing normal tests do not require NVIDIA hardware or these raw
receipts. Current coverage failures and the cold/warm planning distinction are
recorded in the linked report.

Add `--trace` to a separate native run to record `cpu.json`, `frames.csv` and
publication/GPU phase sidecars for each route. Receipts identify profiled runs.
If NVIDIA is unavailable, `--software` explicitly selects llvmpipe and records
that renderer without attempting an EGL adapter or NVML device lookup. It runs
the same coverage and ownership/nonblocking/memory assertions and still exits
2 for any uncovered frame. Software coverage is functional evidence only and
cannot establish NVIDIA performance or replace the hardware acceptance run.

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
The historical Quadro M1000M optimization plot reproduces numbers already
reported in the project README. The new [Quadro versus RTX 3070 Ti study](benchmarks/gpu-comparison/study.md)
adds twelve matched physical-GPU runs with raw traces, captures, telemetry and
verified device identities. Its standalone Matplotlib plot measures offscreen
throughput, with limits on interactive FPS and terrain-migration conclusions.

The [native loop timing study](architecture/terrain-gpu/async/hardware/loop/study.md)
adds frame-linked complete wall receipts for polling, loading presentation and
minimized waits. The [publication tracing study](architecture/terrain-gpu/async/hardware/publication/study.md)
adds bounded ordinary CPU/compute terrain and resident grass receipts through
complete consumer draws. The [reload tracing study](architecture/terrain-gpu/async/hardware/publication/transactions/study.md)
adds bounded whole-scene attempts through exchange and complete consumption,
with failed/superseded transactions retained. The [capture/contact/CPU grass study](architecture/terrain-gpu/async/hardware/publication/lifecycle/study.md)
completes synchronous reload, capture draw, actual handoff and pure CPU grass
admission tracing. The [memory diagnostic study](architecture/terrain-gpu/async/hardware/memory/study.md)
adds UUID-verified NVML sampling, context NVX checkpoints and logical/CPU
receipts with explicit age, unsupported/error and missed-peak limits. Matched
production CPU/compute cost acceptance remains pending. The
[production stationary preflight](architecture/terrain-gpu/async/hardware/cost/preflight/study.md)
adds three alternating native Quadro pairs with exact geometry/pose/anchors,
matching scalar foliage work and complete raw/excluded receipts. Native p95
ratios span 0.9696–1.0492 with observer overhead included; movement, rendered
coverage, Moon/reload and final field/memory gates remain.

The [grounded speed prerequisite](architecture/terrain-gpu/async/hardware/cost/movement/study.md)
checks distance against independent monotonic wall time. A 50 ms frame cap
reduced native production walking and sprinting despite correct animation-clock
rates; bounded grounded substeps restore elapsed-time movement. Longer matched
routes and rendered near-root coverage remain T3c5c2b. Fresh stationary
controls reach 8.5% slower compute p95 in one pair, above the migration limit;
CPU stays default and the cost investigation remains.

The [production route receipts](architecture/terrain-gpu/async/hardware/cost/routes/study.md)
complete three alternating native CPU/compute pairs each for 400 m walking and
sprint, with 5,276 measured and 3,797 excluded frames. Whole-route compute/CPU
native p95 ratios are 0.7056–0.7312 and 0.5557–0.7045 respectively. Sprint
grass-anchor distances are larger on compute, and independent rendered
near-root coverage remains pending; these are cost preflight receipts. The
archive includes every spike, fixed-distance landmarks, device/memory outcomes,
unchanged runtime/binary hashes and independent checks. CPU stays default.

The [live foliage inspection](architecture/terrain-gpu/async/hardware/cost/coverage/study.md)
adds a separate private native probe that reads both main Blade queues before
reflection reuse and compares opaque depth before/after grass. Twenty-six final
software/hardware snapshots, including two production sprint observations,
retain actual generations, pose/wind/trails and independently checked masks.
Fixed-view eligible-ground and final rendered-density acceptance remains pending;
shipping app and timed probe remain unchanged.

The [matched live-plan study](architecture/terrain-gpu/async/hardware/cost/coverage/matched/study.md)
adds fixed camera/pose/trail/wind scene inspection and eligible-area/root/composed
coverage receipts for 24 fresh Quadro routes. Eight of twelve declared pairs
pass. Analytic area sensitivity reveals finite-quad estimator bias at grazing
triangle boundaries; qualification and fresh repeats follow below. Three unchanged
timed stationary pairs retain two >5% regressions, localized for further wind
and opaque-raster controls. Raw buffers, all discrepancies, independent checks
and lossless storage hashes are retained; CPU stays default.

The [analytic area qualification](architecture/terrain-gpu/async/hardware/cost/coverage/area/study.md)
passes known planes and foreground silhouettes, including grazing views and
production-scale transforms, on software GL and native Quadro. Thirty-two
raw GPU snapshots meet exact area and 2×/4× convergence gates below 1%.
Fixed-projection viewport tiling bounds targets to 2.25 MiB and resolves a
retained tile-edge rounding failure. Live qualification follows below; fresh
matched route repeats follow below; earlier failures remain recorded.

The [live analytic inspection qualification](architecture/terrain-gpu/async/hardware/cost/coverage/live/study.md)
passes 20 CPU/compute snapshots and 60 GPU sampling grids on software GL and
verified Quadro, including four unchanged production walking/sprint inspections.
Actual terrain, other bodies and astronaut meshes provide fresh occlusion at
each grid; maximum 2×/4× area change is 0.78655%, with exact native opaque tags.
The native composed color mask retains a measured edge-area uncertainty;
fresh three-pair route coverage follows below; stationary timing acceptance remains pending.
Raw maps, 180 biome checks, retained preflights and private close-harness correction
are archived with frozen inputs. CPU stays default.

The [fresh analytic route comparison](architecture/terrain-gpu/async/hardware/cost/coverage/repeats/study.md)
retains 24 verified Quadro inspections and twelve passing alternating pairs
across walking/sprint at 25/350 m. All area-convergence changes remain below 1%
(maximum 0.61349%); pair gates remain 5%. Actual first crossings are bracketed,
including 4.665 m overshoot and 4.448 m pair root separation. Generation/anchor
and raw root-set diagnostics retain topology variance. Native displayed color
coverage keeps its 13.45% edge-area limitation. Two repeated software groups
and eight native fixtures requalify a strict clip-provenance correction; the
shipping app and 43 other executables stay unchanged. The stationary wind/raster
study follows below; CPU remains default.

The [stationary wind/raster study](architecture/terrain-gpu/async/hardware/cost/raster/study.md)
completes 54 verified Quadro runs across native/fixed/indexed wind and
full/discard/suppressed grass draws. Independent checks reconstruct all 27
pairs, 12,960 measured frames and every retained outlier. Fixed-wind full
draws fail the 5% limit in all three pairs (9.44–15.03% slower); matching wind
does not remove the regression. Ablations locate the repeatable difference
in raster-enabled and downstream scene work without proving an overdraw or
ordering cause. Software/Quadro controls and 703 byte-verified payloads remain
archived. Correction and full-workload repeats follow as T3c5c2b4; CPU stays
default and migration acceptance remains pending.

The [blade-order control prerequisite](architecture/terrain-gpu/async/hardware/cost/raster/order/study.md)
adds a separate private inspection probe and a passing 35.27 s software CTest.
All 18 final snapshots preserve exact Blade multisets, matrices and depth,
with zero displayed RGB differences across native/near/far permutations.
Both detailed/quad queues are populated in main and reflection. Independent
archive checks include 54 preflight snapshots and 1,675 byte-verified payloads.
Physical-GPU qualification and production diagnosis remain blocked: direct
NVIDIA device opens return EPERM, GLX/GLFW and EGL cannot create contexts,
while NVML inventory remains available. CPU stays default; the ordering cause,
bounded GPU correction and full-workload repeats remain pending.

The [GPU timing prerequisite](architecture/terrain-gpu/async/hardware/timing/study.md)
adds bounded request-keyed dispatch timings and complete frame timestamp spans.
It separates nested placement costs from render-stage totals and preserves
nonblocking publication. The publication studies above add latency/outcomes;
the memory study adds physical/logical/CPU observations. Matched CPU/compute
acceptance continues with actual 6/12 m/s routes after stationary preflight.

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
CPU mesh/replay behavior.

The [GPU field evaluation proof](architecture/terrain-gpu/compute/study.md) adds an
opt-in capture backend with double field sampling, GPU-generated draw buffers,
bounded dispatches and completed land/water publication. CPU/GPU packing and
numerical parity, exact replay and GL 3.3 fallback are checked. The full CPU mirror
and normal interactive migration remain T3; no total-frame speedup is claimed.

The [sparse contact prerequisite](architecture/terrain-gpu/contacts/study.md)
uses canonical topology and a bounded lazy position cache for matching compute
triangle-plane contacts. Grass still requires the compatibility vectors; GPU
grass planning and asynchronous interactive publication remain queued.

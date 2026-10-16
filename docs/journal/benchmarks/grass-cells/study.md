# G4: delta updates for persistent grass cells — 2026-10-10

## Implementation

Existing terrain triangles are the cells. Their planet-local angular corners
produce a canonical 96-bit identity and corner permutation, independent of
triangle numbering, winding, height and sinking. Quantization uses an exact
2^40 scale (about 6 micrometres on Earth). Compute terrain derives the identities
on the GPU from its existing double radial descriptors; CPU terrain uses a
separate oracle. The identities occupy a 16-byte-per-triangle tail in the terrain
index buffer, outside the unchanged terrain draw range. No terrain-shaped CPU
readback or second topology upload is required.

A bounded GPU hash table matches the replacement plan against its live source.
Canonical positions, normals and colors detect dirty cells. Matching cells copy
their immutable root prefixes; entering/dirty cells and appended slots generate
new records. Density changes update expected-count scalars while preserving
positions and random ranks. Budget decreases retain prefixes. Seed, variation,
biome and other immutable placement changes invalidate reuse. Changed angular
footprints caused by remeshing enter and retire as cells; this is not a separate
uniform planet grid that survives arbitrary subdivision.

Resident root preparation submits at most four 65,536-candidate dispatches per
poll. Published grass continues drawing until the replacement finishes. A shared
source lease survives cancellation and clear, and existing last-use fences retire
departing buffers. Matching and copying run on the GPU. After a signaled fence,
the CPU reads only a fixed 32-byte update summary, in addition to the existing
224-byte allocation summary. Diagnostic bulk reads exist only in tests.

Each plan adds 64 bytes per selected cell, a uint hash table with the next power
of two at least twice the cell count, and 32 counter bytes. Admission reserves
the full source triangle count and includes the terrain index tail across
live/staged/retiring generations, within the existing 1 GiB overlap limit.
There is no accumulating cell store or disk cache.

This implementation avoids recalculating unaffected placement, but copies
retained records into compact replacement arrays. It still allocates replacement
arrays and runs the existing bounded whole-plan metadata/allocation passes.
The dispatch bound covers root copying/generation, not a hard millisecond limit
on the driver or the entire refresh. Shared wind/trail samples and view-specific
density, visibility, LOD and reflections continue every frame.

New captures record `foliage_cell_identity_version: 1`. Full historical policy
captures without this field use legacy triangle identities (version 0), so
their saved blade layout remains reproducible. Camera-only replay uses the new
default. Unsupported identity versions fail before rendering. GL 3.3 uses the
same canonical corner/seed convention without the compute cell cache.

## Validation and measurement

Results are being collected in ignored `build-f5/g4`. Raw traces, binaries and
captures stay local. G4 is the only TODO in this change; trail fading remains
queued.

### Paired native-loop receipt — 2026-10-16

Three alternating pairs use the G3 baseline preserved in `build-f5/g4` and the
current build, identical production inputs, 1280×720, 6 m/s astronaut walking,
live wind/trails and paused orbital motion. NVIDIA EGL UUID selection identifies
RTX 3070 Ti. Each measured leg retains six seconds after startup and warmup;
there are 9,973 retained frames across stationary and walking cases. These are
pbuffer/native-input-loop results, not desktop presentation measurements.
The canonical cell identities deliberately change fresh-run blade layout;
historical full-policy replay preserves G3 layout for exact comparisons.

| Walking measure | G3 | G4 |
|:--|--:|--:|
| Wall p95 | 8.909 ms | 8.698 ms |
| Maximum wall frame | 116.221 ms | 107.575 ms |
| Complete-plan roots GPU mean (9 plans each) | 0.610 ms | 1.337 ms |
| Complete-plan roots CPU submission mean | 18.121 ms | 6.528 ms |
| Active-frame roots CPU submission mean | 18.121 ms | 0.816 ms |
| Active-frame roots CPU submission maximum | 28.100 ms | 27.713 ms |
| Cell matching GPU mean per plan | — | 0.088 ms |

The nine measured G4 updates copy a mean 87.06% of root slots. Retained GPU
records are copied into replacement arrays, so less placement calculation does
not mean less GPU bandwidth or less total GPU time. Cell lookup/copy overhead
more than doubles complete-plan root GPU time in these runs. CPU submission is
spread across eight events per plan; an allocation/driver outlier still takes
27.7 ms. Dispatch bounds therefore do not promise a millisecond frame bound.
No sustained FPS gain or elimination of streaming spikes is claimed. The
collector build also contains the opt-in experimental v4 terrain work; that
option is disabled in all paired scenes. Frozen binary/input/shader hashes and
the local measurement-source patch identify the measured build.

Stationary p95 is 7.236→7.198 ms. Walking allocation GPU means are essentially
unchanged, 14.275→14.260 ms. The protected policy remains 40.72 blades/m² through
15 m, with no near-infeasible flag. Actual startup cell storage adds 3,069,024
bytes to the grass plan, plus 16 bytes per terrain triangle in the shared index
tail. Conservative live/staged/retiring admission peaks at 1,001,030,344 bytes,
below 1,073,741,824 bytes; G3 peaks at 980,997,768. This is logical admission,
not a claim about total driver memory.

The [compact receipt](summary.json) retains paired endpoints, stage summaries,
UUID, inputs and raw hashes. Reusable collection/reporting is documented in
`scripts/benchmarks/grass_allocation/README.md`. Raw timing/telemetry/native
snapshots stay ignored in `build-f5/resume/g4-native`.


### Correctness validation

All 35 CPU/core CTest groups pass. Four delta-cell tests cover movement, slot
budget growth/shrink, stable identities under triangle reorder/winding changes,
terrain-dirty cells, source cancellation/clear leases, dispatch bounds and legacy
replay versions. Nineteen GPU staging/publication cases pass. Full GPU execution
also exposed a terrain-v4 test that still treated version 4 as unsupported; its
unknown-version fixture now uses 99 and the focused rerun passes.

Four 1280×720 historical full-policy views (surface, ACES, mountain and walking
trails) match G3 PNGs exactly. Fresh canonical placement and its replay match
exactly, with unchanged candidate counts and policy. The renderer lifecycle
group passes. Moving v4 terrain captures also match CPU, compute, replay and
GL 3.3 output in their focused fixture. Additional recovery/frame/reload/capture
regression groups are being run as a follow-up to the focused acceptance checks.

On native Quadro, held-worker compute sprint, 80 m/s surface camera and CPU
sprint remain inside the unchanged 22.5 m grass reserve: maximum plan offsets
11.66, 13.18 and 22.49 m. Stop/reverse offsets stay inside it too. Root-band
counts remain nonempty and all bulk-read/blocking-poll/GL-finish audits are zero.
The CPU route has little spare margin; these flat fixture routes are not a
production-mountain coverage guarantee. A first invocation used the ordinary
probe, which cannot hold workers; the corrected run uses the dedicated refresh
probe and passes all three routes.

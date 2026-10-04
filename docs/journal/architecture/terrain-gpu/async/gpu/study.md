# Asynchronous GPU preparation (T3c2) — 2026-10-04

This stage follows the [CPU worker](../worker/study.md) and the
[asynchronous ownership plan](../plan.md). It supplies context-thread submission,
polling and patch commit APIs while interactive compute remains gated. Captures
use explicit wait adapters. Atomic publication, scene reload transactions and
fenced retirement remain T3c3; neither hardware performance nor available VRAM
is inferred from this correctness checkpoint.

## Owned preparation

`TerrainGpuPreparation` owns the CPU result and submitted land/water buffers.
It validates matching field/header/contact keys and enabled water before
submission, then submits both surfaces in the ordered context. `poll()` checks
both fences without a nonzero timeout. Terrain timestamps are retrieved only
after both query results report availability. `waitForCapture()` explicitly
drives these same consumers to completion for deterministic output.

`ProceduralGrass::submitResident()` accepts resident source buffers and a matching
generation, settings, normalized planning eye and intended mesh revision. It
generates metadata and allocation into a separate owned `Preparation`. Ordered
GPU commands and barriers allow submission directly after terrain dispatch;
CPU terrain completion is not a dependency of metadata submission. Disabled
foliage has an explicit empty ready patch without metadata, allocation or summary.
The caller must retain its source buffers and current context through completion.

`poll()` returns false while allocation or resource completion remains pending.
It reads exactly one 224-byte summary after allocation completion and checks
metadata/allocation generation and planning-eye agreement. Validation now checks
the expected budget and slot cap, ordered prefixes, patch/candidate totals using
64-bit arithmetic, density finiteness and the completed density-search iteration.
No shaped vertices, triangle metadata or reference arrays return to the CPU.

Before reporting ready, preparation creates terrain buffer-texture views, the
placement program, blade queues, indirect commands and both draw VAOs. A resource
fence covers this setup. Resident draws require those resources and reuse their
existing capacity. CPU and legacy compute retain their older planner and draw
allocation behavior. Dynamic trail uploads remain a separate scene effect.

Patches are individually owned rather than raw-handle elements moved by vector
growth. Their destructors release views, queues, descriptors, allocation and VAOs,
including partial preparations. Submission and polling preserve caller program,
VAO, generic buffers, active texture and indexed SSBO ranges. A preparation error
marks only its staged owner failed; the published patch remains untouched.

`reserve(index)` creates a destination before publication. `commit()` rejects
unready/failed work, unreserved slots and mismatched generation, source buffer IDs
or mesh revision before exchanging ownership. The exchange allocates no resources;
the old patch moves into the preparation owner. The caller controls its release.
This is patch ownership transfer, not yet fenced multi-consumer retirement.

## Logical byte admission

The initial ceiling is **512 MiB per admitted preparation**, independent of
unreliable free-memory telemetry. Terrain admission reserves land/water parameter,
canonical input, unique/corner output, index and scratch buffers plus worst-case
resident grass resources. Grass admission includes metadata (160 + 64 bytes per
triangle), allocation summary/groups/references/ranks/offsets, 128 bytes per
budgeted candidate for two blade queues, and 32 indirect-command bytes.
The candidate budget also respects the queried SSBO block limit.

Admission rejects an over-limit preparation before dispatch. Grass submission
accepts an explicit occupied-byte count; the capture adapter includes land and
water terrain working bytes. Actual queue capacity uses the completed candidate
count, which may be smaller than the admitted reservation. Capture metadata
reports admitted bytes, allocated draw bytes and prepared-resource status.

This ledger describes logical buffer payload and conservative generation overlap.
It excludes driver object overhead, shader storage, CPU snapshots, small cached
field packs and shared dynamic effects. It is not physical VRAM or total process
memory. Runtime captures prepare one generation at a time. Global spare/retiring
set admission and release across interactive reload/body switches remain T3c3;
the available-VRAM/near-density controller remains B1.

## Capture and compatibility boundaries

The capture terrain installer drives the new land/water owner to completion.
Resident grass preparation then uses submit/wait/commit at the original scene-pass
planning eye. Keeping that point preserves chase-camera and saved grass-anchor
semantics. CPU and legacy compute planners keep their existing paths. Replay
planning overrides are consumed only after a successful resident commit.

Land/water and grass are still installed at separate capture stages. The
interactive compute gate stays in place until T3c3 connects complete bundles,
failure recovery and retirement to one frame boundary, then T3c4 passes native
movement/reload acceptance. No asynchronous interactive renderer is claimed here.

## Validation

Seven native GL cases cover chained submission before terrain polling, delayed
consumption and unready commit rejection, preallocated resource bytes retained
through draw, previous-patch preservation on admission/stale failures, explicit
disabled foliage and water, missing consumers/queried limits, corrupted scalar
summaries and context binding/range restoration. Existing compute/CPU/replay,
GL 3.3 fallback and worker tests remain required.

All **59 CTest groups pass in one uninterrupted application-binary run (733.29 s)**,
including 20 native compute cases, worker ownership, lifecycle/reload, actor,
flight, grass, atmosphere, native input and adaptive quality. Afterwards a test-only
probe was added to hold the GL client-wait response at `GL_TIMEOUT_EXPIRED` for
three polls: every call uses zero flags/timeout, no summary is read or patch
published, and real completion succeeds after the RAII probe restores the loader.
The rebuilt native target passes all **21 cases (24.294 s)**, including all seven
preparation cases. The application remains byte-identical. This is full regression
plus a native extension check, not a second full 59-group run.

Compute capture checks pass: all ten compute/CPU/replay PNG hashes match T3c1
exactly. The reduced fixture admits 4,632,148 logical bytes and allocates 524,320
draw-resource bytes (4,096 candidates); the summary remains 224 bytes and metadata
readback remains zero. All 22 gallery hashes and their provenance are retained.

[Evidence](validation/evidence.json), [full regression](validation/tests.log),
[final 21 native cases](validation/native.log),
[the full run's 20 native cases](validation/compute.log) and
[capture results](validation/capture-results.json) retain source/input/binary and
artifact provenance for both native scopes. README, journal/PDF, layout, links
and gallery checks are updated. Resume at T3c3; CPU remains the default.

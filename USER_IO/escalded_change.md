# Delivered changes and remaining work

Reviewed 2026-10-07 against the commit history through `2353a01`, the current
source, `todo.md`, and `USER_IO/escalated_todo.md`.

The project now has a working GPU terrain implementation, resident GPU grass
planning, asynchronous generation/reload handling, and usable astronaut controls.
It does **not** yet have an accepted faster GPU terrain default, automatic foliage
allocation that protects configured near density, or the completed atmosphere
reduction. Many later commits built inspection tools and documented experiments
instead of finishing those outcomes. I expanded the tracking too far; the nested
validation tasks should be treated as engineering history, rather than separate
features the project still needs.

This report consolidates the results. It does not introduce another task hierarchy.

## What the commits actually delivered

| Area and representative commits | Result in the project | Remaining limitation |
| --- | --- | --- |
| Application structure and terrain LOD — `688039a`, `7e13c82`, `1e9aef2` | Shared implementations compile once; configuration parsing and renderer lifecycle have separate ownership. Terrain has eight distance levels, shoreline refinement, shared boundaries and inward sinking. | CPU still owns topology/subdivision and sparse collision sampling, as allowed by the agreed plan. Hexagonal panels were evaluated; the implementation retains triangular panels. |
| Procedural foliage and materials — `fdcfbbe`, `dfab364`, `50c3815`, `c464cd3` | GPU grass placement/culling, a single procedural grass layer, distance LOD, near strips/far quads, configurable wind, camera-centred density and pale granular sand with filtered ripple relief. | Candidate caps can lower effective density. Sand detail is shader normal relief; the later requested wind-driven sand behaviour remains unfinished. |
| Astronaut movement — `c966928`, `d77ef01`, `643d25f`, `fa7ad78` | Camera 4, planted-foot IK, grey/blue suit and German arm patches; walking at 6 m/s, Shift sprint at 12 m/s, jumping, directional airborne thrust and Space-up boost. Planet/Moon orientation changes at 1.2 diameters from the centre; flight gravity uses the three nearest bodies. A slow-frame movement bug was corrected. | The runtime character is procedural. Downloaded rigs and comparison animations are asset preparation, not an integrated realistic animated astronaut. |
| Trails and exhaust — `49604e3`, `a72e6a6` | Persistent grass deformation, bounded trail history, shared wind and backpack bubble particles, including reflection and saved replay state. | Old trail segments still disappear on eviction without the requested recovery fade. Smoke transitioning to a blue flame is not implemented. |
| Offline README image and lens flare — `b86ca7d`, `5993b23` | Denser foliage at the production starting camera, reproducible captures and visibly stronger, occlusion-gated flare. The dense showcase submitted about twice the main-view blades of the ordinary offline profile. | Column assembly was investigated, not implemented: a narrower viewport kept the same terrain/candidate allocations. This remains an OpenGL capture renderer; these commits did not add a ray tracer. |
| GPU terrain and grass ownership — `6853fa5`, `8ee7e8e`, `82e87a8`, `5e1b732`, `ed896c8` | Shared planet-field/topology contracts; compute height/gradient/material/geometry evaluation; sparse matching contacts; resident triangle metadata and deterministic GPU slot allocation. New compute operation no longer keeps full CPU land/water render vectors. | The path remains opt-in. Early transfer measurements support the architecture, but the complete performance/memory acceptance is unfinished. |
| Asynchronous generation and reload — `64655d9` through `9b27091` | Bounded worker scheduling, stale-result rejection, GPU preparation, complete generation publication, retirement and transactional scene reload. Walking, flight, body switches, Moon arrival and failure recovery are integrated. `--terrain-backend compute` is available; CPU and GL 3.3 fallback remain. | Correctness and recovery are implemented; they do not establish that the compute path is faster in every workload. |
| Atmosphere reuse — `b1a549c` | Reference optics are cached and shared. Density-column lookup, scattering and refraction already run on the GPU. OpenGL context negotiation now prefers compute-capable GL 4.3 with GL 3.3 fallback. | Highlight metering still reads a tile map to the CPU and reduces it there. GPU exposure reduction is pending. Small configuration coefficients and simulation queries can reasonably remain on CPU. |
| Profiling and GPU comparison — `cdd1122`, `0adb32a`, `acede72` through `7e19bc6` | CPU profiling, GPU work/frame timing, full native-loop timing, publication outcomes and device-verified memory telemetry. A matched offscreen comparison measured the RTX 3070 Ti about 12.75–13.21 times faster than the Quadro M1000M. | This compares different cards under the same renderer. It does not prove the new terrain backend improves performance. Sampled device memory does not establish every transient allocation peak. |
| Terrain performance investigation — `4aa5006` through `dbe2234` | Production stationary/routes, foliage inspection, eligible-area measurements, fresh coverage comparisons and wind/raster/order controls. Coverage comparisons ultimately passed their declared pair gates. The tooling identifies where further correction is needed. | Fixed-wind stationary full rendering remains about 9.44–15.03% slower on compute, exceeding the 5% limit. Removing/discarding grass eliminates the repeatable failure, but an ordering/overdraw cause and runtime correction have not been established. The final order controls were software qualification only. |
| Journal and archive cleanup — documentation commits, `b3a2f0b`, `2353a01` | Extensive studies, screenshots, replay metadata and a rebuilt journal PDF; large XZ/GZ payloads were subsequently removed from version control. | Archive manifests/validators still reference removed payloads. They are present as untracked local files here, so this checkout masks a fresh-clone reproducibility gap. |

The initial 100,000-triangle compute proof reduced terrain input transfer from
12,000,000 to 2,800,880 bytes (76.66%). The later resident grass-allocation probe
used 2,840 input bytes and one 224-byte summary, with no full CPU render vectors.
These are scoped transfer results; the remaining acceptance must include total
CPU field work, full-frame rendering and replacement memory.

Historical tests support the delivered implementations, including CPU fallback,
compute parity, native input, replay and reload recovery. Later diagnostic commits
used scoped checks; they were not a fresh full-suite acceptance of the entire
current project. This review did not rerun benchmarks or the application tests.

## Work needed for a clean foundation

These are the remaining outcomes, in practical order. They can be tracked without
splitting every measurement or implementation step into another TODO topic.

1. **Make a clean checkout self-contained.** Consolidate the overlapping TODO
   parent/subtask rows into a short list of unfinished outcomes and move completed
   experiments into the journal. Decide which small fixtures belong in Git and
   which historical captures belong in an external downloadable archive. Update
   manifests, validators and reproduction instructions accordingly. For example,
   the blade-order manifest references 1,568 files that are currently untracked;
   its existing validator cannot work from the current committed tree alone.
   Keep historical failures documented, but separate optional research captures
   from normal build/test requirements. Finish with a clean-checkout build and
   the relevant CPU/fallback/compute/replay/input/reload checks.

2. **Finish the GPU terrain migration decision.** Fix the demonstrated stationary
   full-render regression and verify the affected workload with the existing
   tools. Complete the remaining Moon-handoff/reload cost and memory checks and
   the promised transfer/CPU-work measurements. Reuse the established 5% frame
   regression limit and coverage/contact checks. If compute cannot meet that
   target, explicitly retain it as experimental and keep CPU as the supported
   default; do not mark the speed goal complete. The last native diagnosis was
   blocked by graphics-device `EPERM` and GL context failures despite NVML
   inventory access. That is an environment blocker, not another code feature.

3. **Implement the requested automatic foliage budget.** Existing hard caps,
   telemetry and render-resolution adaptation do not provide this policy. Protect
   configured density in a defined near-camera region, then adjust distant
   falloff/coverage using available memory and measured rendering cost. Account
   for live and replacement allocations, handle unavailable telemetry, and save
   effective settings for deterministic replay. Report when the near region
   itself cannot fit instead of silently reducing all density. Use the completed
   CPU–GPU plan rather than starting another planning chain.

4. **Finish the remaining atmosphere GPU work.** Replace tile-map CPU readback and
   reduction with GPU highlight/exposure reduction, preserving the weighted 5%
   exclusion, sparse bright-star handling and agreement between main/reflection
   views. Verify visual parity and actual transfer/frame-cost improvement. The
   existing GPU atmosphere integration and optics cache should be reused.

Once these outcomes are resolved, new gameplay and visuals can build on clear
resource ownership, predictable budgets, supported defaults and reproducible
checks. A wholesale renderer rewrite, hexagonal topology conversion, more private
inspection tools, or BSON caching is not established as a prerequisite for that
foundation.

## Foundation recheck — 2026-10-08, after F1–F5
The renderer has a usable CPU-supported foundation; the historical status above is superseded by this review.

Checked the runtime code as well as the F1–F5 results. Archive-independent builds
and local-only benchmark storage are implemented (item 1). GPU near-first grass
ordering fixes the original stationary regression; transfer/CPU-work, Moon/reload
and memory checks are complete, and retaining CPU as default satisfies item 2's
explicit experimental-backend alternative. GPU highlight/exposure reduction is
integrated on GL 4.3 with the supported GL 3.3 reference (item 4); the remaining
8-byte exposure receipt is not a missing bulk reduction. Async ownership,
publication, retirement, sparse contacts and replay are integrated.

**F6 update — 2026-10-08:** The integration work identified below is complete.
The CPU default now shares protected density, automatic falloff, conservative
memory admission and locked replay with resident compute. Production images and
walking/sprint coverage match exactly; all 15 matched-quality cost pairs pass
the unchanged 1.05 p95 gate, with bounded replacement memory. Compute is now the
default for fresh runs; CPU override/GL 3.3 fallback and historical replay
compatibility remain supported. No additional foundation
task chain is needed. See the [F6 report](../docs/journal/architecture/terrain-gpu/grass-allocation/cpu-integration.md).
The following list records the pre-F6 audit and is retained as history.

**Outstanding implementations / integration limits (pre-F6):**

1. **Automatic foliage on the supported default (item 3).** Protected near density,
   memory/cost-driven distant falloff, missing-telemetry limits and saved policy
   are implemented for resident compute, but the CPU default still uses legacy
   global density scaling. `Renderer.cpp` enables adaptive budgeting only when
   `terrainPublication` exists and forces CPU terrain to the CPU grass planner;
   `GrassPlan.cpp` still scales requested density against the whole Gaussian area.
   To deliver this behavior in normal default operation, reuse the existing policy
   in the supported CPU path, or make compute an accepted default after the next
   item. This does not require replacing either renderer or the ownership system.

2. **Equivalent foliage quality before promoting compute (item 2, conditional).**
   Integrate a common protected-near/distant-falloff policy, or another explicitly
   equivalent rendered distribution, for the compared CPU/compute paths. Preserve
   that coverage during movement and generation replacement within the existing
   memory bounds. F5 cannot accept current faster frames: stationary densities
   differ (47.149 versus 120.72 blades/m²); the separate 25 m inspection has
   root-density ratios 2.615/1.539/0.000, and the far compute band fails its sample
   minimum. Those receipts establish unmatched quality, not the specific cause
   of a placement defect. After any runtime correction, reuse existing coverage,
   contact/replay and <=5% p95 acceptance checks with matched poses/quality; do not
   equate the naturally diverging Moon trajectories with matched cost. This is
   required to claim the GPU speed goal or change defaults, **not** a blocker to
   expanding the supported CPU foundation. F5 completed the evaluation, not that
   speed goal.

These availability/promotion limits do not reopen the completed bounded F1–F5
work; retaining the supported CPU default is still a valid foundation decision.
No other foundation implementation is established by this audit. In particular,
zero-byte exposure readback, hexagonal panels, BSON, a renderer
rewrite and new inspection frameworks are not prerequisites. Evidence:
[F5 acceptance](../docs/journal/architecture/terrain-gpu/acceptance.md),
[F3 policy](../docs/journal/architecture/terrain-gpu/grass-allocation/adaptive-falloff.md),
[F4 reduction](../docs/journal/architecture/atmosphere/highlights.md) and
[local archive policy](../scripts/benchmarks/archives/README.md).

## Requested features still unfinished

These remain product work, separate from the foundation above:

- **Realistic astronaut:** the three prepared stylized candidates were rejected.
  Replace that shortlist with the requested five modern, more realistic rigged alternatives, then
  integrate the selected model with skinning, locomotion/flight animation and the
  existing terrain-contact IK. Candidate renders alone do not finish this request.
- **Trail recovery:** add continuous fading before history eviction and retain
  some normal wind motion on fresh pressed grass. The latest TODO specifies the
  oldest 30% of history and an 80% pressed/20% wind mix; the earlier request used
  10%, so that choice should be settled when implementing the existing feature.
- **Jetpack visuals:** add duration/speed-dependent smoke and blue flame with
  wind-driven diffusion, using the existing bounded exhaust system.
- **Sand motion:** extend the existing granular/ripple shader to the requested
  wind-driven shoreline appearance; static ripple relief is already present.
- **Persistence:** evaluate actual costly reusable results before adding BSON.
  If persistence is beneficial, implement versioned dependency keys, invalidation
  and safe writes. Trail replay already exists; a persistent cache is a separate
  feature and should not delay the core migration without evidence of benefit.
- **Offline column assembly:** remains an optional memory/quality extension.
  It needs per-column streaming, consistent projections, exposure and reflections;
  cropping the existing full allocation does not deliver the requested gain.

## Source references

- [Current backlog](../todo.md) and [escalated tracking](escalated_todo.md).
- [CPU–GPU ownership plan](../docs/journal/architecture/terrain-gpu/plan.md).
- [Interactive compute implementation](../docs/journal/architecture/terrain-gpu/async/interactive/native/study.md).
- [Stationary regression evidence](../docs/journal/architecture/terrain-gpu/async/hardware/cost/raster/study.md)
  and [latest software order controls](../docs/journal/architecture/terrain-gpu/async/hardware/cost/raster/order/study.md).
- [Atmosphere implementation and pending reduction](../docs/journal/architecture/atmosphere/optics/study.md).
- [GPU comparison](../docs/journal/benchmarks/gpu-comparison/study.md),
  [offline showcase](../docs/journal/benchmarks/offline-showcase.md),
  [astronaut behaviour](../docs/journal/character/astronaut.md),
  [trail behaviour](../docs/journal/character/trails.md), and
  [prepared astronaut candidates](astronaut_vis/README.md).

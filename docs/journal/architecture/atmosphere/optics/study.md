# A1a — shared reference atmosphere optics

Each prepared scene now owns one validated reference-air coefficient pack per
planet. Sky incident lighting, all body-material bindings, main/reflection
atmosphere composition and capture diagnostics consume the same pack. The
[CPU–GPU ownership plan](../../terrain-gpu/plan.md) keeps this small uniform
calculation on CPU; density-column lookups, scattering and refraction integration
already run on GPU. This step implements reuse without changing those models.

## Keys and ownership

The exact key contains enabled/refraction flags, shell radius multiplier,
pressure, temperature, suspended-water fraction, droplet radius, all six
composition percentages and physical planet radius in metres. Equivalent SI
radii reuse the pack across world-unit changes. Camera, orbital position, body
orientation, Sun direction, water and materials remain per-frame inputs outside
the key. Disabled and zero-pressure bodies retain valid vacuum packs.

The fixed-size cache validates and calculates before replacing an entry. Invalid
refreshes throw while preserving the previous pack. Replacement scenes have
independent caches; the existing atomic scene exchange moves configuration and
cache together. Body reorder/count changes therefore rebuild the correct slots.
This cache represents configured reference air, not a future spatial weather
solver's `LocalAirState` overrides. No GPU allocation, query or readback is added.

Capture metadata adds `render.atmosphere_optics_evaluations` and
`render.atmosphere_optics_reuses`. Evaluations equal the configured planet count
for an unchanged scene, including moving and reflected views; they reset with a
new prepared scene. These counters describe coefficient calculation rather than
GPU integration or image reuse. The lookup uses exact input equality, avoiding
approximate-key changes in optical output.

## Acceptance

Five CPU cases compare every pack field against the direct reference, change each
optical input, check equivalent units and independent slots, and retain old packs
through invalid pressure/radius refreshes and move/reorder replacement. A new
actual-GL binding case reads coefficient and dynamic Sun uniforms and checks
that disabling air refreshes its pack. Existing atmosphere GL tests retain
refraction, silhouettes, reduced-resolution edges, highlight protection and
cached exposure coverage.

Production performance captures check one evaluation per planet across paused,
moving, main/reflection and full-resolution frames, with exact replay. Renderer
reload tests reject invalid pressure without changing the old PNG or evaluation
count, then change pressure/temperature/refraction and body count. CPU and legacy
compute replacements match fresh captures exactly.

All 63 CTest groups pass in one uninterrupted frozen-input software run
(1005.40 s), including 20 CPU atmosphere cases and 12 GL atmosphere cases.
Focused Quadro atmosphere/capture checks pass in 14.007 s; the CPU/legacy
compute optical reload case passes in 8.945 s on the GPU. Five native
input scenarios and four startup rejection contracts pass in 31.594 s.
Complete traces identify Quadro GL 4.3 for input/space/Moon/default CPU and Mesa
GL 3.3 for forced fallback. Native frames record zero blocking fence polls,
server waits, bulk buffer reads or explicit finishes. Nine regenerated atmosphere PNGs match the pre-cache aggregate exactly. Ten
baseline compute/CPU/replay PNGs and 22 gallery hashes are unchanged. Source/build-test inputs and ten
executable hashes match the final tree exactly.

Final measured test results and frozen inputs are retained in
[validation evidence](validation/evidence.json). Software regression and hardware
correctness are recorded separately; cross-driver PNG equality is not assumed.

## GPU access and remaining work

On 2026-10-05 the environment exposed a Quadro M1000M (2 GiB) and GeForce RTX
3070 Ti (8 GiB), both through NVIDIA driver 580.178.04. Default Xvfb still chose
Mesa llvmpipe. The following route created actual hardware OpenGL 4.6 on the
Quadro:

```sh
__NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia xvfb-run -a glxinfo -B
```

The first hardware native run exposed a context-negotiation bug: requesting
GL 3.3 on NVIDIA returned exactly that version, so production compute fell back
to CPU despite available GL 4.6 hardware. Window creation now tries GL 4.3 core
first, then retries GL 3.3 core when unsupported. All startup dimensions, MSAA,
depth/stencil settings and GLFW lifetime cleanup remain shared between attempts.
The native probe records `GL_RENDERER` and `GL_VERSION`, retaining hardware proof
in complete frame traces. Forced GL 3.3 fallback/rejection processes select
Mesa explicitly, because NVIDIA does not honor Mesa's version override. RTX
visibility through NVML is confirmed; an RTX OpenGL context has not been selected.

The [T3c5 hardware measurement plan](../../terrain-gpu/async/hardware/plan.md)
now identifies missing compute-stage timing, generation publication latency and
physical-memory telemetry before matched CPU/compute stationary, walking and
body-switch measurements. Those costs remain
pending and CPU terrain remains the default. Native correctness is not a matched
hardware performance comparison.

A1b still needs GPU highlight reduction with existing weighted 5% exclusion,
partial-tile weights and the sparse-star safeguard. The current 1920×1080 meter
still reads 32,400 RG32F tiles (259,200 bytes) and sorts them on CPU. A1c then
compares complete atmosphere costs and parity. No transfer reduction or hardware
FPS improvement is attributed to A1a.

Update 2026-10-08: [F4](../highlights.md) completes exact GPU highlight selection
and its focused transfer/frame-cost comparison. The earlier pending A1b/A1c
notes above describe the A1a checkpoint; they are superseded by F4. Combined
terrain acceptance remains the existing F5 outcome, with CPU terrain default.

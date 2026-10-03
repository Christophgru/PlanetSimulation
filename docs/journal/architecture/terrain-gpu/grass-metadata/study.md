# Resident GPU grass triangle metadata — 2026-10-03

T3b1 implements the metadata prerequisite of the [CPU–GPU plan](../plan.md).
For opt-in compute terrain captures with compute grass placement, grass
preparation now generates a resident descriptor per original terrain triangle.
It reads the existing float-rounded/sunk VBO and index buffer through SSBO views.
The renderer uploads no shaped geometry or triangle metadata and reads no
metadata back. CPU terrain and GL 3.3 retain their existing grass route.

Each 64-byte std430 descriptor holds a double center/reach bound, double
Gaussian-weighted area and center distance, the original triangle ID and an
eligibility flag. The shader preserves the CPU planner's conservative distance,
degenerate-area, normal, rock slope, water, snow and non-landscape tint tests.
Area uses square metres; bounds remain body-local and distances use metres.
Eligibility uses the requested density before rounded-slot budget adjustment.
Original triangle IDs remain available for the existing seed/rank root identity.

Only a 160-byte parameter pack and two 32-bit controls per bounded dispatch are
uploaded. The pack includes the planning eye, scale, Gaussian and rebuild rules,
requested density, material/biome thresholds and flags. It uses the same small
CPU scalar formulas as the current planner; no CPU height queries are introduced.
The shader implements the nonpositive double Gaussian exponential with binary
range reduction and 18 Taylor terms, since GLSL 4.30 exposes `exp` for float types
and `ldexp` for doubles. The accepted-patch distance bound and minimum supported
sigma fraction keep the exponent at or above -200. The [official GLSL 4.30
specification](https://registry.khronos.org/OpenGL/specs/gl/GLSLangSpec.4.30.pdf)
describes these function overloads in sections 8.2–8.3.

The implementation queries SSBO block/binding and compute block/group/invocation
limits, bounds dispatches to 65,536 triangles and checks the actual source buffer
sizes before allocation. Failure leaves previously owned metadata untouched.
The output records terrain generation keys and its planning eye. Shader-storage
barriers order resident producers/consumers; a fence supports nonblocking polling.
Explicit waits and descriptor readback are diagnostic APIs used only by native
tests. Indexed SSBO ranges, generic storage binding and program state are restored.
RAII owns the descriptor/parameter buffers and completion fence.

Metadata lifetime follows the grass plan. A mesh revision or planning-eye rebuild
creates a replacement; cached preparation uploads nothing. Disabling foliage or
clearing the renderer releases the buffers. Capture diagnostics record resident
bytes, input bytes, dispatches and diagnostic read bytes under
`render.foliage_gpu_metadata`. Grass preparation upload counts include both the
existing CPU triangle-ID upload and these new parameters/controls; the total
working-byte statistic includes metadata. Parameter bytes are resident until
replacement; terrain source buffers are shared and excluded from this additional
allocation count. A replacement can temporarily overlap the previous generation;
these are per-generation logical sizes, not driver allocation/VRAM measurements.

Four native cases validate descriptor/parameter driver offsets and 64-byte array
stride; CPU eligibility and weighted-area parity at both poles, shoreline/sunk
mixed LOD, minimum/maximum sigma, disabled water, green/red tint, disabled foliage
and an eye above the ground envelope; forced one-group partial dispatches; invalid
keys/versions, truncated sources, zero dispatch and tiny block limits; range
restoration on success/failure; production capacity, CPU-vector-independent
metadata, zero cached uploads and disable cleanup. The CPU oracle plans from the
actual GPU float output so field-evaluation error cannot hide metadata differences.

The 100,000-triangle case retains 6,400,160 bytes and uploads 176 bytes across two
dispatches. It finds 20,913 eligible patches in the deliberately large-budget,
one-slot oracle fixture. The maximum relative weighted-area error across all
fixtures is 2.20116e-14 on llvmpipe. The one-group 10,000-triangle probe uses 157
dispatches and uploads 1,416 bytes, including every control update. Diagnostic
readback is explicitly counted; the real capture route reports zero read bytes.

The build and all 58 CTest groups pass (1092.60 s), including ten total native GL
terrain/metadata cases. Compute surface and walking PNGs match T3a exactly, and
both locked replays, explicit CPU override and real GL 3.3 fallback pass. All 22
gallery hashes/provenance, local document links, layout and whitespace checks
pass. The rebuilt 25-page journal was reviewed on pages 5–7 and 24–25.
Validation results are retained in [validation/](validation/).

This checkpoint adds metadata memory/work while the CPU planner still computes
its own descriptors and allocation. It claims no CPU saving, frame speedup or
memory reduction. Captures still report `allocator: cpu`; shader metadata does
not yet select rendered grass. This keeps candidate/root selection, image order,
hard work caps and existing replay unchanged during validation. T3b2 must consume
these buffers with deterministic ordered allocation, rounded-slot density search,
queried placement capacity/dispatch limits and a versioned replay contract, then
remove the full CPU render vectors. T3c still owns asynchronous complete consumer
publication and interactive/hardware cost gates. T3b remains in progress.

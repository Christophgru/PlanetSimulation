# F4 — GPU highlight reduction

2026-10-08. The GL 4.3 path now selects the highlight exposure on the GPU.
A 1920×1080 frame transfers one 8-byte exposure receipt instead of 32,400
RG32F tiles (259,200 bytes), removing 99.9969% of this readback and its CPU
construction/sort. GL 3.3 retains the supported CPU reference. Terrain backend
selection is independent; CPU terrain remains the default.

## Exact selection and ownership

The existing 8×8 tile pass, optics cache, density columns and atmospheric
integration are unchanged. Composed HDR targets are RGBA16F, so each nonnegative
tile peak maps exactly to a positive half-float bit pattern. A fixed 32,768-bin
integer histogram counts **pixel area**, including partial edge tiles; one extra
counter sums actual near-white pixels. Clear, histogram and selection dispatches
use 131,076 bytes of histogram storage plus an 8-byte result texture.

The final workgroup scans descending bins and picks the first peak whose
cumulative area exceeds `floor(width * height / 20)`. Equal-peak tiles may share
a bin because any tile order within that bin selects the same peak. If actual
near-white pixels fit the 5% allowance, requested exposure is retained regardless
of how many tiles sparse stars touch. Zero/infinite peaks, the white threshold
and the rule against brightening retain the CPU reference behaviour. Changing
the HDR source format would require reconsidering this exact half-float key.

The result stores the double exposure as two integer words, preserving the
existing capture receipt precision. Its bounded synchronous read remains;
this change does **not** claim zero readback or zero CPU/GPU synchronization.
The existing float uniform consumes the current frame's result. Reflections
remain linear until main-view composition, manual exposure bypasses the meter,
and cached presentation retains the selected source/exposure. Each renderer
owns its reduction resources; viewport changes recreate tiles while reusing the
fixed histogram, result and shader. Destruction releases them with the context.
No new scene setting, replay version or atmosphere model is introduced.

## Focused frame cost

Quadro M1000M, NVIDIA 580.178.04, hardware GL 4.3 context, RelWithDebInfo.
The opt-in comparison runs the actual HDR finish/tone-map path against the
original CPU meter in the same process, with three alternating pairs, ten warmup
frames per cohort and 120 measured frames per path/pattern/size. Sparse input is
one white row; saturated input is white throughout. Each pair requires identical
displayed bytes and matching exposure. Allocation/compilation is excluded.
This is a focused metering/presentation frame, not the production terrain scene
or F5's combined full-render acceptance. GPU query time spans the command
interval, including CPU-induced gaps; it is not isolated shader busy time.

| Size / input | CPU reference median / p95 wall ms | GPU median / p95 wall ms | Reference → GPU readback |
| --- | ---: | ---: | ---: |
| 129×121 sparse | 0.197 / 0.262 | 0.278 / 0.379 | 2,176 → 8 B |
| 129×121 saturated | 0.213 / 0.352 | 0.324 / 0.447 | 2,176 → 8 B |
| 640×480 sparse | 0.784 / 0.932 | 0.775 / 0.867 | 38,400 → 8 B |
| 640×480 saturated | 0.960 / 1.084 | 0.875 / 0.963 | 38,400 → 8 B |
| 1920×1080 sparse | 1.951 / 2.306 | 1.521 / 1.690 | 259,200 → 8 B |
| 1920×1080 saturated | 3.558 / 4.123 | 1.580 / 1.754 | 259,200 → 8 B |

At 1080p, GPU command-interval medians are 1.156/1.200 ms (sparse/saturated)
versus 1.606/3.208 ms for the reference. The fixed dispatch/histogram work adds
about 0.08–0.11 ms at 129×121; no universal FPS improvement is claimed.

## Verification

- 34/34 headless core groups pass.
- 13/13 NVIDIA and 13/13 Mesa GL 4.3 atmosphere cases pass. The independent
  CPU-sort oracle covers 72 selections, including tied/zero/infinite/subnormal
  peaks, non-integral requested exposure, partial tiles and clipping boundaries.
- Forced Mesa GL 3.3 passes all 12 supported cases and explicitly skips the
  compute-only oracle. Existing resize, manual-exposure, cached presentation,
  refraction/temperature and sharp-depth-edge cases are retained.
- The canonical nine atmosphere and three twilight scenarios pass on Mesa,
  including their saved-state and camera-snippet replay assertions. All twelve
  PNGs and render metrics match their pre-change Mesa baseline exactly. The
  corresponding twelve NVIDIA captures also match their same-driver baseline
  exactly; cross-driver image equality is not assumed.
- Five renderer reload and four interactive reload cases pass on NVIDIA,
  retaining invalid-input rollback, body-count changes, pressure/temperature/
  refraction refresh, current main/reflection consumers and exact saved replay.
- Focused old/new presentation comparisons pass identical displayed bytes and
  exposure checks for all six size/input combinations. The final build is
  incremental-clean; raw payloads are untracked and ignored.

The pre-change NVIDIA canonical runners also recorded tiny airless camera-
snippet and twilight saved-replay differences. Those baseline failures remain
in the local receipts; they were not weakened or relabelled as F4 successes.
Canonical end-to-end replay acceptance above is the passing Mesa run.

Raw CSV, captures, baseline binaries and logs stay local in ignored
`build-benchmarks/runs/f4-20261008/`. No archive is required for normal tests.
Reproduce the focused comparison from the repository root:

```sh
cmake --build build-resume --target PlanetSimulation atmosphere_render_tests -j 8
__NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia \
  PLANET_HIGHLIGHT_BENCHMARK=build-resume/highlight-cost.csv \
  xvfb-run -a build-resume/tests/atmosphere_render_tests \
  --gtest_also_run_disabled_tests --gtest_filter='*DISABLED_HighlightFrameCostComparison'
```

Normal `atmosphere_render_tests` runs the parity/correctness cases without the
benchmark. Set `__NV_PRIME_RENDER_OFFLOAD=0`,
`__GLX_VENDOR_LIBRARY_NAME=mesa`, `MESA_GL_VERSION_OVERRIDE=3.3` and
`MESA_GLSL_VERSION_OVERRIDE=330` to exercise fallback; use `4.3`/`430` for Mesa
compute. Existing atmosphere/twilight manifests and renderer reload tests remain
the end-to-end checks. F5 and unrelated feature tasks remain unchanged.

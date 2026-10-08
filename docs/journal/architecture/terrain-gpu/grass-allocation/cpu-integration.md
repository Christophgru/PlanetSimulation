# F6: shared automatic foliage policy

Completed 2026-10-08. CPU and compute now share the protected-density policy;
matched rendered coverage and all 15 bounded cost pairs pass. The subsequent
2026-10-08 default promotion makes fresh runs use compute with resident GPU grass
planning. CPU remains supported via `--terrain-backend cpu` and GL 3.3 fallback.
Saved replays retain their backend; older replays without backend metadata retain CPU.

Fresh CPU and resident compute runs now protect configured near-camera density
and fit the distant tail with the existing memory/timing controller. CPU planning
uses the same raw triangle areas, rounded slots, near priority, stable hashes and
32-probe fitting rules. Both compute placement and the GL 3.3 vertex fallback use
the same protected profile and fractional coverage. Terrain topology, contacts and
physics are unchanged. The promotion changes startup selection, not the measured
CPU/compute implementations; the frozen benchmark hashes below predate it.

CPU admission charges terrain, both worst-case blade queues, triangle references
and commands, including old scene ownership during replacement/retirement.
Terrain uploads check the old-plus-new allowance before replacing buffers.
GL 3.3 conservatively reserves queues even when direct instancing needs none.
The existing cached sampler runs with the overlay/trace hidden; missing/stale
telemetry retains the 64 MiB stage / 256 MiB aggregate limits. Logical limits are
512 MiB per stage and 1 GiB aggregate; the optional cap and GL storage limit may
reduce capacity. These are conservative renderer reservations, separate from
periodic device-wide physical samples and CPU mesh vectors.

Reload inherits the controller and restores saved effective policy. CPU override
of a compute capture also restores that policy, including on GL 3.3. Admission
rejects insufficient memory rather than silently reducing recorded quality.
Historical render captures without `foliage_policy` retain their legacy profile;
camera-only replays use the current automatic policy. Near-infeasible reporting
includes physical/per-triangle deficits encountered after a locked replay moves.

## Verification

- Ten headless plan/controller cases and actual CPU/GPU allocation comparisons
  pass, including normal/busy budgets, near-only work, tiny limits and replay.
- All 35 NVIDIA compute cases pass. Mesa GL 4.3 passes all 32 terrain/foliage
  render cases. GL 3.3 checks protected coverage, exact replay and locked-memory
  rejection: 4 blades/m² across 525 m² retains weighted coverage 2,100 ±0.01.
- Expanded capture checks produce exact CPU/GPU fixture PNGs, CPU replay/override,
  GL 3.3 replay/override, walking contact replay and tiny-budget policy agreement.
- All 34 core groups and six final renderer integration groups pass, including
  native default/fallback, flight, contact replay, reload and fault recovery.
  Recovery and interactive reload groups also pass. A lifecycle startup failure
  was an Xvfb/GLFW initialization race; all 29 cases pass with Xvfb `-noreset`.
- Production fresh CPU, compute and explicit CPU override produce byte-identical
  surface PNGs, the same policy and exactly 1,599,906 candidate slots.
- Initial native production preflight caught stale first-frame CPU controller
  signals despite a fresh sampler cache. CPU uploads and replacement preparation
  now consume that cache before admission. The new untraced startup regression,
  all four memory cases, CPU/legacy reload and six allocation cases pass again.
- At both 25 m walking/sprint crossings, all three distance bands have exact 1.0 ratios
  for eligible area, visible-root density, weighted fade and composed coverage;
  all unchanged sample minimums pass. Native roots differ 0.326 / 0.087 m before
  the controlled common-pose render; live topology is identical. Composed grass
  pixels are exactly 624,964 / 625,183 on both backends.
- All 15 bounded native cost pairs pass at matched scalar policy/pose inputs and
  the unchanged <=1.05 p95 gate. All cost cohorts retain identical frozen source,
  test/tool, application and probe hashes; no measured outliers are removed.

## Default promotion verification

The subsequent default switch passes the full incremental build, all 30 renderer
lifecycle cases, terrain frame/native input checks and expanded compute captures.
The latter verify default compute, explicit CPU, exact saved/historical replay,
GL 3.3 automatic fallback and locked-compute rejection. Standard-config startup
without config/backend flags selects managed compute, `gpu-v1`, sparse contacts
and zero CPU render vectors. Its 480×270 production PNG is byte-identical to the
accepted F6 image (SHA256
`40053d719757f468dee76a4fd0b694dfced61bf7981d263b5ee3d5aad718ba22`),
with the same policy and 1,599,906 candidate slots. Repository layout and whitespace
checks pass. Verification output remains local in `build-f5/compute-default-*`.
The measured algorithms are unchanged; the bounded cost cohorts below were not
repeated for a startup-selection change.

## Production protocol

Reuse the F5 native runners, Quadro M1000M / NVIDIA 580.178.04, 1280×720, scene
scale 1, production scenario, paused orbit/spin and normal wall-clock wind.
Graphics UUID `GPU-2cefee61-6b3b-a670-c390-c7449dad3f79`; RTX 3070 Ti is not the
rendering device. Retain alternating backend order, three pairs per cost case,
three seconds / 30 warmup frames, >=240 measured frames and unchanged 1.05 p95
limit. Walking uses 300 m and sprint 450 m at 6/12 m/s. Reload includes preparation,
publication and retirement; no measured outliers are removed.

The existing runners optionally consume public `render.foliage_policy` to lock
quality. The production surface capture yields 120.72 blades/m², protected radius
15 m, sigma 8.435062249191105 m, no deficit, and 1,599,906 slots under a 1,600,000
soft allowance / 2,000,000 capacity. The common input retains this density/width
and uses the same 2,000,000 hard allowance on both backends to leave headroom for
moving/replacement topology. This locks quality for cost; independent automatic
controller/admission checks remain necessary.

Moon steady timing uses a public grounded Moon capture after a physical landing,
with identical saved pose rather than diverging airborne trajectories. The
existing separate handoff/space capture tests still check navigation and gravity.
Matched rendered coverage uses common projection, pose, trail and wind with each
native live plan; all blocking inspection frames are excluded from cost.
The embedded departure-camera metadata is normalized to the production scenario;
the public Moon camera and landed pose are preserved. Setup failures (required
render dimensions, first-frame cache consumption, embedded departure camera)
remain local and excluded; no completed cost cohort was repeated or filtered.

Raw captures, traces, receipts and compressed payloads stay local/ignored in
`build-benchmarks/runs/f6-20261008/`. No new benchmark framework or TODO chain.

## Acceptance result

Ratios are compute/CPU native full-frame p95 at the shared policy. Every row
passes 1.05. Moon pair 3 is close to the limit; its actual ratio is retained.

|Case / pair|CPU p95 ms|Compute p95 ms|Ratio|
|:--|--:|--:|--:|
|Stationary 1|64.433|64.120|0.995144|
|Stationary 2|70.517|65.003|0.921803|
|Stationary 3|70.550|72.035|1.021050|
|Walking 1|172.846|135.266|0.782585|
|Walking 2|176.678|140.412|0.794733|
|Walking 3|180.206|137.954|0.765536|
|Sprint 1|201.079|132.000|0.656460|
|Sprint 2|191.320|127.152|0.664602|
|Sprint 3|211.780|127.366|0.601407|
|Moon 1|19.363|14.093|0.727844|
|Moon 2|14.730|14.478|0.982949|
|Moon 3|14.495|15.212|1.049497|
|Reload 1|84.267|84.842|1.006817|
|Reload 2|84.642|84.924|1.003334|
|Reload 3|84.426|87.319|1.034262|

Walking retains 403–448 measured frames at 5.978–5.995 m/s; sprint 351–405 at
11.987–11.995 m/s. Configured/effective density stays 120.72 on every measured
frame. Policy equality holds at every distance landmark; maximum aligned root
differences are 0.00675 / 0.06578 m and chase-camera differences 0.000376 / 0.02527 m.
Moon roots are identical on both backends, with no airborne trajectory mismatch.

Largest logical overlap is 871,735,956 bytes (<1 GiB), with CPU reload peak
551,430,464 bytes. Largest periodic device-wide used sample is 884,146,176 bytes;
it includes other processes and may miss short peaks. Every reload finishes at
epoch 2 with one publication, matching final character planning eye, zero failures
and no pending/retiring resources. Request-to-publication/retirement is
2,497–2,641 ms CPU versus 2,898–2,942 ms compute (observed complete frames,
not exact fence completion timestamps). The worst retained measured reload frame
is 2,578.98 ms CPU versus 175.82 ms compute: CPU replacement remains synchronous,
and p95 alone does not express that stall.

Frozen runtime/shader digest:
`c8ad71ed37185b7d57941f0e9ad02c8d187c984520d7b5887c6e36bee5147c61`.
Frozen build/test/tool digest:
`669d8aee5d71a4f652b9a24475fd8e30b920d01f4ca25b4932d13df605a9eb23`.
Raw reports additionally retain executable/input/method hashes and exact frame joins.

The unchanged terrain field pipeline retains [F5's transfer/bulk-work evidence](../acceptance.md)
(76.66% transfer reduction, bulk CPU field evaluations to zero). F6 closes the
CPU-policy integration and matched-quality gap; historical unmatched F5 speed
ratios remain diagnostic. Full foliage feedback verification here uses Mesa;
this does not resolve F5's pre-existing NVIDIA exact-trail feedback limitation.
No claim of speed across other GPUs or zero synchronization is made.

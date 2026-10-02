# Persistent astronaut grass trails

Grounded movement in camera 4 records a continuous path in body-local SI
metres. Existing blades bend along that path, retaining their terrain roots,
placement seeds, density, distance fade and close/far geometry. Pressing the
centre down to 8% of blade height above the path and extending it along the path by 95% of its
length makes the trail persistent under animated wind. The walking segment
retains its terrain slope so tips lie along the ground instead of being pushed
under rising hills. Full deformation holds
within 25 cm; smooth blending ends at 60 cm, making a 1.2 m wide soft edge.
The same field reaches opaque and water-reflection draws.

This is a bounded geometric effect rather than simulated grass elasticity.
There is no time-based recovery. The latest 2,048 segments persist until scene
reload; walking far enough evicts the oldest segment. An initial placement,
idle contact jitter below 20 cm, airborne movement, water and relocation over
1.5 m do not generate connecting marks. Switching cameras breaks the active
path but preserves previous marks. Terrain remeshing preserves the history;
roots resample the current terrain independently. Extreme height changes can
move roots outside the recorded capsules because the history stores the
original contact positions, not an elastic surface attachment.

## GPU representation

A balanced, radius-expanded segment BVH uses four RGBA32F buffer texels per
node. Bounds store an escape index so GLSL traverses without a stack, skipping
whole subtrees whose capsules cannot touch a root. A maximum history yields
4,095 nodes, 16,380 texels and 262,080 payload bytes. The hierarchy is rebuilt
and uploaded only when the history changes; stationary and reflected draws
reuse it. The single-root GPU placement paths share the same vertex deformation;
no CPU blade geometry or replacement root list is created.

Segment bounds and endpoints use float metre offsets around a double-precision
body-local origin. Shader roots subtract the corresponding normalized origin
before conversion to metres. This avoids storing metre-scale detail directly
in a float coordinate near the planet radius, although terrain vertices and
normalized roots retain the renderer's existing float precision limit.

## Reproducibility and validation

`astronaut_pose.grass_trail` stores segment endpoint pairs in capture sidecars.
The sidecar also retains `grass_plan_eye`, the cached planning anchor: a
walking capture may use a plan generated before its final camera position.
Rebuilding at the final eye changes budget-derived density and nearby roots.
`terrain_plan_eye_world_units` preserves the selected terrain build anchor too:
walking below the 10 m rebuild threshold retains an earlier mesh, whose LOD
sinking affects the grass plan. Restoring only the grass anchor was insufficient
in the first focused run (110 differing pixels); the terrain anchor must match.
Replay checks the capacity, finite coordinates and relocation bound before
restoring the field. Older sidecars without the new member remain compatible.
Airborne captures preserve any earlier marks and add none in flight.

The CPU test exercises sampling, idle persistence, flight/landing gaps,
relocation, camera interruption, FIFO bounds, deterministic hierarchy restore,
invalid input and accelerated-versus-brute-force distances at Earth radius.
Production-shader transform feedback checks centre height, unchanged roots and
coverage, wind suppression, untouched surrounding blades, both poles, six
segments and one quad, vertex and compute placement, body transforms and mesh
revision. The astronaut integration test checks recorded walking marks,
absence of marks during an initial airborne capture and byte-identical
foliage/HDR/reflection replay.

The controlled 960×540 pair contains 977 GPU-submitted blades and 20 path segments
across 5.9826 m of grounded movement. Walking and replay SHA-256 are
`ab77800ed2e2a0717e11f79478d47a717395658346ad63264889ba37cc0b00df`;
the unpressed control is
`cc30d3c980502854c0081b51485e2413abf6c3d7f2788432f81e1ea58da39093`.
All pose values, planning anchors and lighting match between the two controls.
[Control](trails/images/control.png) and [pressed trail](trails/images/pressed.png)
are actual renderer captures; their [sidecars](trails/replay/) and
[commands/hashes](trails/validation/capture-evidence.json) are retained.

To reproduce the comparison:

```sh
LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=2 xvfb-run -a \
  python3 scripts/benchmarks/grass_trails.py \
  --binary build-resume/PlanetSimulation --output-dir build-resume/trail-study
```

Validation combines the full 52-entry run and targeted final checks. The full
run passed 51 entries in 516.46 s; RepositoryLayout failed because the new
README screenshot was not yet published. After the slope correction, both
renderer/capture entries passed again in 122.35 s. Gallery publication then
completed and RepositoryLayout passed separately. All 52 entries therefore
have passing results; this is a full run plus targeted rechecks, not a second
clean full-suite run.

All 22 README images were regenerated, inspected and verified against their
SHA-256 manifest and renderer-source fingerprint. The previous 21 PNGs remain
byte-identical; the new trail view reproduces the controlled capture above.
The journal PDF compiles. Raw [validation logs](trails/validation/) retain the
initial replay failure, interrupted intermediate run and final checks.
Commands use `LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=2` under Xvfb on GCC 12.2
and Mesa llvmpipe. This validation does not establish hardware frame rates.

History-pressure fading and versioned BSON persistence are queued separately
in `todo.md`; the current history still evicts its oldest mark without fading.

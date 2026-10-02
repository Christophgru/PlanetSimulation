# Planet-local astronaut and follow camera

Camera **4** shares camera 2's selected planet and surface position. WASD moves
the astronaut at **6 m/s**, or **12 m/s** with either Shift key held. Mouse look
controls the follow camera; Esc releases the cursor. T pauses orbit/spin, while
walking and airborne motion continue on local elapsed time.

Space on the ground launches a jump at 10 m/s. Release and press Space again
while airborne to engage the jetpack; subsequent presses boost and holding
Space sustains upward thrust. Two streams of blue bubbles emerge underneath
the backpack during thrust. Release Space to coast, then fall under gravity.
WASD commands acceleration in flight. The suit tilts along the single thrust
axis; arms stay still throughout the armed jetpack phase. Landing clears the
jetpack state.

## Model and animation

The current model is original procedural geometry: a grey suit with blue
details, dark visor, backpack, articulated limbs and small black/red/gold
German patches on the upper arms. It supplies a working fallback while an
external skinned model is selected; the [Sketchfab search](sketchfab-models.md)
recommends Muko_Art's downloadable rigged astronaut. That asset has not been
integrated, and its CC BY license differs from the TODO's initial MIT request.

The MIT-licensed [ozz-animation](../../../external/ozz-animation/README.md)
runtime supplies two-bone leg IK. Application logic alternates swinging feet
and preserves the opposite stance contact in planet-local coordinates. Swing
targets sample the ground and use a smooth interpolation plus 15 cm lift.
Cadence increases with movement speed; substeps bound time and displacement
before solving the joints. Boots align to their contact normals.

Contacts intersect the resident rendered terrain triangles, including their
LOD sinking. A small triangle cache avoids repeated whole-mesh scans for
stationary feet. Mesh replacement invalidates the cache and resamples stance
height at the same radial direction. Missing mesh coverage falls back to the
analytic terrain; enabled water remains a walkable floor, matching camera 2.
This is radial surface collision, rather than arbitrary mesh physics.

The model and contacts follow the selected planet's translation and spin.
Double precision is retained until the IK job is expressed near the actor
origin. The chase camera aims near the torso, stays above terrain and shortens
its arm when a ridge intervenes. It uses the surface field of view and a 20 cm
near plane. Orbit and first-person controls retain their existing behavior.

## Flight forces

The 100 kg actor integrates altitude-dependent gravity, centrifugal and
Coriolis acceleration, and quadratic atmospheric drag in planet-local metres.
Pressure depends on configured gas composition, sea-level pressure and
Kelvin temperature. Falling has a drag-dependent terminal speed, without
an artificial vertical velocity clamp; vacuum has no drag terminal speed.

Space arms a short thrust pulse; holding it sustains thrust. WASD commands
a horizontal acceleration toward 100 m/s while boosting, bounded by one
engine's thrust capacity. Releasing the controls preserves inertia and allows
drag to slow the actor. The engine is sized from the main planet and reused
on other planets; lower attainable speed is possible in dense air or high
gravity. The nozzles emit along the tilted suit's downward axis; no independent
sideways force bypasses the suit orientation. Arms remain still while armed.

The [force model and power study](flight.md) gives the equations, numerical
production-scene estimate and assumptions. Effective drag area, exhaust speed
and efficiency are explicit modelling choices. Propellant depletion, thermal
limits and atmospheric wind coupling are not yet simulated. Surface collision
remains radial; descent lands and replants both feet.

## Rendering and reproducibility

The astronaut participates in opaque, HDR, atmosphere and water-reflection
passes and receives terrain shadowing. A small terrain contact-darkening term
anchors planted feet where the planet-wide shadow texture is too coarse.
The actor does not yet cast a full dynamic body shadow. Bubbles are a bounded
32-sphere procedural effect using local time; the reflection shares the phase.

Capture metadata stores the complete gait, joints, contacts, 3D velocity,
suit thrust axis, thrust force, pressure/power diagnostics, flight state and
bubble phase. `--astronaut-capture` selects camera 4. For deterministic flight:

```sh
./build/PlanetSimulation --astronaut-capture build/jetpack.png \
  --benchmark-frames 8 --benchmark-step 0 --benchmark-character-step .04 \
  --benchmark-jump-frame 0 --benchmark-boost-frame 3
./build/PlanetSimulation --replay build/jetpack.png.json \
  --astronaut-capture build/jetpack-replay.png
```

## Initial locomotion validation (historical)

Initial validation passed on GCC 12.2 and Mesa llvmpipe/Xvfb: 13 CPU tests check
planted feet, joint reach, pole traversal, mesh revisions, chase clearance,
6/12 m/s controls, gravity, boost and landing. Render integration checks visible
character pixels and byte-identical walking and airborne replays. Native GLFW
input checks camera switching, WASD, two Space presses, sustained thrust and
scene reload. The renderer lifecycle suite also passes with the new resources.
The combined focused run passed all four CTest entries in 88.48 seconds.

The subsequent full run passed 48 of 49 entries in 366.51 s. The live astronaut
input check sampled a screenshot before its expected jump/boost change was
observable. That test now waits for processed Space callbacks and a changed
frame within a bounded deadline, and preserves logs/images on failure. It then
passed once and three more consecutive repetitions. All 49 entries therefore
have passing results for the unchanged application binary; this was a full
run plus a targeted recheck, not a second clean full-suite run. Raw logs and
the executable hash are in [validation/](validation/evidence.json).

All 20 README gallery captures were regenerated with the same renderer,
visually inspected and verified against the image manifest and renderer-source
fingerprint. The updated Typst journal PDF compiles successfully.

The current gait is an approximation with rapidly alternating steps at high
speed. Natural running, animation blending, planted-foot yaw limits and robust
climbing over steep ledges need a skinned model and further locomotion work.
Persistent grass trail deformation is now implemented; see [trail behavior, limits and validation](trails.md).

The acceleration/drag update is validated separately in [the flight study](flight.md).

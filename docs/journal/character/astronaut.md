# Planet-local astronaut and follow camera

Camera **4** shares camera 2's selected planet and surface position. WASD moves
the astronaut at **6 m/s**, or **12 m/s** with either Shift key held. Mouse look
controls the follow camera; Esc releases the cursor. T pauses orbit/spin, while
walking and airborne motion continue on local elapsed time.

Space on the ground launches a jump at 10 m/s. In the airborne phase, WASD
fires thrust without Space, W/S follows the full look direction (including
look-down descent), and Space commands up. Releasing movement controls coasts
under gravity and gas drag. Bubbles emerge underneath the tilted backpack;
arms stay still throughout the armed jetpack phase. Landing clears flight.
Within 1.2 diameters from a body's centre (2.4 radii), up aligns to that planet
or moon; outside every such region, free space orientation allows unrestricted
camera pitch and travel toward the Moon. Position and velocity persist in
world space, and flight gravity always sums the three nearest bodies.
See [space-flight behavior and validation](space-flight.md).

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

On level ground, settled standing extends both knees and raises the pelvis
approximately 30 cm, preserving the existing leg lengths and locked soles.
The torso, arms, backpack and chase camera follow this suit-local adjustment.
The body settles over the two ankle targets with a 0.10 s response time once
the final walking swing finishes. Walking lowers the pelvis for joint reach;
flight retains its tucked-leg pose. Uneven ground limits extension to the
lower ankle's reach, allowing the other knee to bend. Fully extended chains
use an analytic collinear midpoint to avoid a singular float IK bend plane.
Replay stores the body offset; old sidecars default to their original offset.
See the [standing comparison and validation](standing.md).

Contacts intersect the resident rendered terrain triangles, including their
LOD sinking. A small triangle cache avoids repeated whole-mesh scans for
stationary feet. Mesh replacement invalidates the cache and resamples stance
height at the same radial direction. Missing mesh coverage falls back to the
analytic terrain; enabled water remains a walkable floor, matching camera 2.
This is radial surface collision, rather than arbitrary mesh physics.

The grounded model and contacts follow the selected planet's translation and spin;
airborne state remains inertial when that planet moves.
Double precision is retained until the IK job is expressed near the actor
origin. The chase camera aims near the torso, stays above terrain and shortens
its arm when a ridge intervenes. It uses the surface field of view and a 20 cm
near plane. Orbit and first-person controls retain their existing behavior.

## Flight forces

The 100 kg actor integrates world-space gravity from the three nearest celestial
centres and quadratic drag relative to their translating/rotating air. Pressure
depends on configured gas composition, sea-level pressure and temperature.
Falling has no artificial velocity clamp; vacuum has no drag terminal speed.
The near-body controller targets 100 m/s along the commanded 3D axis; free-space
thrust requests acceleration without a speed ceiling. A shared main-planet-sized
engine limits force along the smoothly tilted suit axis. Orbit pause/speed changes
the prescribed bodies' motion while local character simulation continues.

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
bubble phase. World navigation also stores inertial position, velocity, up,
exhaust axis, reference body and mode. `--astronaut-capture` selects camera 4. For deterministic flight:

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

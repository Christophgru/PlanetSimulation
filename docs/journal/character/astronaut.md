# Planet-local astronaut and follow camera

Camera **4** shares camera 2's selected planet and surface position. WASD moves
the astronaut at **6 m/s**, or **12 m/s** with either Shift key held. Mouse look
controls the follow camera; Esc releases the cursor. T pauses orbit/spin, while
walking and airborne motion continue on local elapsed time.

Space on the ground launches a jump at 10 m/s. Release and press Space again
while airborne to engage the jetpack; subsequent presses boost and holding
Space sustains upward thrust. Two streams of blue bubbles emerge underneath
the backpack during thrust. Release Space to coast, then fall under gravity.
WASD remains active in flight. Landing clears the jetpack state.

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

## Flight approximation

Effective radial gravity uses
`max(0.25, G × mass / radius² − ω² × radius × cos(latitude)²)` in SI units,
where `ω = 2π / spin_period` (zero for a nonspinning planet). The 0.25 m/s² floor
is a playability choice. The radius is the configured reference radius; the
model omits altitude-dependent gravity, Coriolis acceleration and drag.
With jetpack inactive, larger gravity shortens the same 10 m/s launch.

An airborne boost adds an 8 m/s impulse and a short thrust pulse. Sustained
thrust supplies a net upward acceleration of 20 m/s², with upward velocity
capped at 35 m/s and downward velocity capped at 50 m/s. These jetpack values
are arcade controls, not a rocket or propellant model. Horizontal movement
uses the existing spherical surface controller. The floor clamps airborne
height when rising terrain intersects the actor; descent lands the actor and
replants both feet.

## Rendering and reproducibility

The astronaut participates in opaque, HDR, atmosphere and water-reflection
passes and receives terrain shadowing. A small terrain contact-darkening term
anchors planted feet where the planet-wide shadow texture is too coarse.
The actor does not yet cast a full dynamic body shadow. Bubbles are a bounded
32-sphere procedural effect using local time; the reflection shares the phase.

Capture metadata stores the complete gait, joints, contacts, flight state and
bubble phase. `--astronaut-capture` selects camera 4. For deterministic flight:

```sh
./build/PlanetSimulation --astronaut-capture build/jetpack.png \
  --benchmark-frames 8 --benchmark-step 0 --benchmark-character-step .04 \
  --benchmark-jump-frame 0 --benchmark-boost-frame 3
./build/PlanetSimulation --replay build/jetpack.png.json \
  --astronaut-capture build/jetpack-replay.png
```

Focused validation passed on GCC 13 and Mesa llvmpipe/Xvfb: 13 CPU tests check
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
The grass-trail deformation TODO remains separate and unfinished.

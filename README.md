# PlanetSimulation

| Solar view | Planet surface | Planet orbit |
|:--:|:--:|:--:|
| <a href="docs/screenshots/solar-view.png"><img src="docs/screenshots/solar-view.png" width="220" alt="Sun and planet render test"></a> | <a href="docs/screenshots/surface-view.png"><img src="docs/screenshots/surface-view.png" width="220" alt="Planet surface render test"></a> | <a href="docs/screenshots/planet-orbit.png"><img src="docs/screenshots/planet-orbit.png" width="220" alt="Planet orbit render test"></a> |

These [render-test screenshots](docs/screenshots/) are captured from the
configured development scene; click an image to view it at full size.

A C++20/OpenGL project that renders a moving Sun, Earth, and Moon from
`configs/scenarios/solar_system.json`.

The visual direction for later landscape and vegetation steps is the
[Quick Grass live demo](https://simondevyoutube.github.io/Quick_Grass/) and its
[GitHub source](https://github.com/simondevyoutube/Quick_Grass). The renderer
currently has broad terrain and water; grass remains future work.

The development scene uses kilometers for world coordinates: the Sun is 5 km
across, Earth is 2 km across, and the Moon is 540 m across. The Earth–Moon
center of mass travels between 10 and 15 km from the Sun.
The surface camera starts 30 m above sampled terrain or water, whichever is
higher, and walks at 80 m/s.
The development config stores planets in a `planets` array; the surface
camera's `planet_index` selects an entry in that array.

## Orbits and rotation

Each body has a unique `name` and positive `mass_kg`. Every planet or moon has
an `orbit.parent` naming another body; all parent chains must reach the Sun.
Parents may appear after their children in the array. Unknown parents,
duplicate names, self-orbits, and cycles are rejected when loading or reloading.
The Sun is the sole root and has no `orbit`.

| Setting | Meaning |
| --- | --- |
| `orbit.semi_major_axis`, `orbit.semi_minor_axis` | Large and small half axes in `distance_unit`; require `0 < b <= a`. Omitting `b` gives a circle. |
| `orbit.mean_anomaly_deg` | Initial phase measured uniformly in time; 0 starts at periapsis, 180 at apoapsis. |
| `orbit.inclination_deg` | Orbital plane tilt relative to world XY. |
| `orbit.ascending_node_deg` | Rotation of the tilted orbital plane about world +Z. |
| `orbit.periapsis_deg` | Direction of periapsis within that plane. |
| `rotation.period_seconds` | Axial rotation period; 0 stops spin, a negative value reverses it. |
| `rotation.axial_tilt_deg` | Tilt of the body's north axis about world +X, independent of orbital phase. |
| `rotation.phase_deg` | Initial rotation about the body's own +Z axis. |

Orbital planes use world coordinates, independent of the parent's spin.
Earth's outer orbit lies in XY. Earth's axial tilt and the Moon's orbital
inclination are both **20°**, with a zero ascending node, so the Moon orbits
in Earth's equatorial plane. Both bodies spin once every **60 seconds**;
the Moon's orbital period is **120 seconds** and the Earth–Moon collective's
outer period is **300 seconds**. These are elapsed simulation seconds at normal
speed, including frames that take longer to render. Reloading resets the epoch.
Press **T** to pause or resume all orbital motion and axial spin. Camera controls
remain active while paused, and resuming continues from the frozen time without
a jump. Press **Y** to halve or **U** to double the speed of orbits and spin
(from 1/1024× to 1024×; starts at 1×). Each press prints the current multiplier.
Speed changes preserve the current orbital phase and leave camera controls at
their usual speed. Changing speed while paused takes effect on resume.
Reloading preserves the paused/running state and speed multiplier.

Speeds follow [Newton's form of Kepler's laws](https://science.nasa.gov/learn/basics-of-space-flight/chapter3-3/).
Using axes in meters, `e = sqrt(1 - b²/a²)`, `μ = G × (parent mass + collective mass)`,
`T = 2π × sqrt(a³/μ)`, and `v² = μ × (2/r - 1/a)`, with
`G = 6.67430e-11 m³ kg⁻¹ s⁻²`. The minor axis determines eccentricity and the
variation in speed around the ellipse; the major axis and masses set the period.
Kepler's equation supplies the position and velocity at an absolute time,
keeping ellipses stable without accumulated integration error.

The collective mass includes the body and all descendants. For Earth's outer
orbit, the combined Earth–Moon mass is used. Within that collective, Earth
and Moon move on opposite sides of their shared center of mass in inverse
proportion to their masses. The Sun also recoils, conserving the whole system's
center of mass and momentum. The configured Sun `position` is its position at
time zero. An orbit's axes specify the **relative separation between the parent
body and the child collective's center of mass**. Multiple siblings contribute
to their parent's recoil. This is a hierarchy of prescribed two-body ellipses;
tidal effects and gravitational perturbations between branches are not modeled.

The example masses are deliberately scaled for this small, fast scene:

| Body | Mass (kg) |
| --- | ---: |
| Sun | 1.2194532287919512e19 |
| Earth | 6.338938161361669e17 |
| Moon | 7.923672701702087e15 |

They were calculated using `M_earth + M_moon = 4π² × (2500 m)³ / (G × 120²)`
and `M_sun = 4π² × (12500 m)³ / (G × 300²) - (M_earth + M_moon)`,
with an Earth:Moon mass ratio of 80:1. Spin is configured independently of mass.

Legacy unnamed planets receive names such as `planet_0` and default to orbiting
`sun`. A legacy `orbit_radius` becomes both axes. `orbit_speed` and planet
`position` remain readable metadata; dynamic positions and speeds come from
the orbital elements. Terrain, water, local coordinates, and surface cameras
share the same body rotation. Planet orbit cameras follow translation while
letting the surface turn beneath them.

## Sunlight, moonlight, and config descriptions

`sun.absolute_magnitude` sets **bolometric absolute magnitude**, with a lower
number giving more light. The example uses **3.5** and emission RGB
`[1.0, 0.99, 0.97]` for a brighter, nearly white Sun. The
[IAU magnitude scale](https://arxiv.org/abs/1510.06262) gives
`L = 3.0128e28 × 10^(-0.4 M_bol)` watts; a magnitude difference of −5 means
100 times the luminosity. The nominal Sun is about magnitude 4.74.

The `lighting` section groups ambient fill, display exposure, a reference
distance, and the reflection switch. `reference_distance` uses `distance_unit`
and normalizes illumination from one nominal solar luminosity to 1 at that
distance. This is a display scale for the miniature scene; the absolute
magnitude still determines the intrinsic luminosity. Direct light follows
inverse-square distance falloff. `exposure` affects the final image after
adding sunlight, reflected light, and ambient fill. An exponential tone curve
and sRGB encoding keep the brighter Sun and terrain highlights displayable.

Each planet's `reflection.geometric_albedo` and `reflection.color` determine
the strength and RGB tint of sunlight it sends to other bodies. The Moon uses
albedo 0.12; setting it to 0 disables its reflection. Setting
`lighting.reflections_enabled` to false disables all bounced sunlight.
Reflection uses a
[Lambert sphere phase function](https://www.aanda.org/articles/aa/pdf/2018/02/aa31192-17.pdf):
`Φ(α) = [sin(α) + (π−α) cos(α)] / π`, where `α` is the Sun–Moon–receiver angle.
Full Moon sends the most light toward Earth, quarter phase sends `1/π` of that,
and new Moon sends none. The outgoing light is proportional to the Moon's
illuminating sunlight, albedo, color, `Φ`, and squared radius/separation ratio.
Sun–Moon and Moon–Earth distance falloff are both included.

Illumination is evaluated once per body per frame and shared by terrain,
water, and water reflection passes. Direct sunlight still shades surface
normals. Reflected light is averaged over the receiving sphere (intercepted
power divided over four times its cross-sectional area), as an inexpensive
uniform contribution to the whole planet. The same calculation allows
Earthshine on the Moon and multiple reflectors. This is one bounce only, with
no eclipses or recursive light exchange. The averaged reflected contribution
does not receive local terrain shadows.

Direct sunlight uses a Sun-facing depth map for each planet: a foreground
mountain blocks sunlight on terrain and water behind it, even when the hidden
surface faces the Sun. The maps contain the complete rendered terrain mesh,
including off-camera geometry, and follow the body's spin and orbital motion.
They are generated once per frame and reused in the main and water reflection
passes. Sun rays are parallel within each planet, consistent with its shared
lighting direction; shadows between separate celestial bodies are not modeled.

`lighting.shadows.enabled` toggles terrain shadows (default true).
`resolution` is the square depth map size, a power of two from 256 to 4096
(default 2048). `bias_texels` controls the slope-adjusted comparison offset
(0..4, default 0.5); excessive bias can erase small shadows. Filtered edges
reduce aliasing. Detail is limited by the map resolution and the current terrain
LOD. Shadow projection uses body-local coordinates, avoiding precision loss
from orbital translation. Ambient fill and averaged moonlight remain in shadow.

Each config block now has a `description` and a `parameter_descriptions` map.
These explain units, limits, effects, and reserved settings while keeping the
actual parameter values directly editable. Metadata is ignored by the parser.
Ambient fill now belongs under `lighting.ambient_light`; older configs can
still use `skybox.ambient_light` as a fallback.

Use `--config PATH` to load another scenario, including render-test fixtures.
The selected file also becomes the interactive reload/watch target.

## Prerequisites

- CMake 3.16 or newer, Git, and a C++20 compiler
- OpenGL 3.3 support, GLFW 3, and GLEW development libraries
- A graphical display for GLFW, including the hidden-window render test

CMake fetches nlohmann/json, GLM, and GoogleTest during the first configure, so that
step needs network access. On Ubuntu/Debian, install the local build dependencies
with:

~~~bash
sudo apt update
sudo apt install build-essential cmake git libgl1-mesa-dev libglfw3-dev libglew-dev
~~~

## Configure, build, and test

Run these commands from the repository root. Repeating the configure and build
commands updates an existing build directory.

~~~bash
cmake -S . -B build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
~~~

Run all registered tests:

~~~bash
cd build
ctest --output-on-failure
cd ..
~~~

The full suite includes an OpenGL render test. On a machine without a display,
run CTest under Xvfb from the build directory:

~~~bash
cd build
xvfb-run -a ctest --output-on-failure
cd ..
~~~

For a focused check, run the config test executable from the repository root:

~~~bash
./build/tests/config_tests
~~~

For a release build without tests, use a separate directory:

~~~bash
cmake -S . -B build-release -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel 2
~~~

## Run the renderer

Run from the repository root because the executable loads `configs/` and
`shaders/` relative to its working directory. In interactive mode, left-drag
to orbit around the Sun: dragging right moves the camera left, and dragging up
moves it down. The scroll wheel requests 4% zoom steps and eases into each one.
Press `1` to orbit the Sun, `2` for the planet surface view, or `3` to orbit the
planet selected by `surface_camera.planet_index` (the first planet if no surface
camera is configured). Both orbit views use left-drag and the wheel. In surface
mode, the mouse looks around without a
button and `W`, `A`, `S`, `D` walk along the planet while maintaining clearance
above ground or water at the configured `walk_speed_mps` (80 m/s in the
development scene). Returning to planet orbit from the surface keeps the
camera on the same side of the planet.
The cursor is captured in surface mode. Press `Esc` to release it while staying
in mode `2`, so the config can be edited; press `2` again to resume mouse look.
Press `1` or `3` to switch to an orbit view. Surface controls also activate
automatically when either orbit camera comes within `1.1 × planet diameter`
of the planet center.
Local Down then points radially toward the planet. Close the window to exit.
Saving `configs/scenarios/solar_system.json` reloads the Sun, planets, terrain,
water, and camera starting values automatically after the file settles for
about 0.1 s. Press `R` to request a manual reload at any time.
The active view stays selected when it is still configured, and its position
resets to the edited start value. A released surface cursor remains free after
reload until `2` is pressed again. An invalid or partly written JSON file is
reported on stderr and leaves the current scene running; press `R` again
after fixing it. Shader files still require an application restart.

~~~bash
./build/PlanetSimulation
~~~

When planet mode starts, and every five seconds while it stays active, stdout
prints the camera's world position and look direction, followed by a
planet-local position labeled latitude and longitude in degrees and altitude
above the spherical reference radius in meters, plus ground clearance, then a
`surface_camera start value` JSON object. Replace the existing `surface_camera`
object in `configs/scenarios/solar_system.json` with that printed object to
start at the saved position and view. The `direction_ned` array is ordered
North, East, Down in the selected planet's local frame. The printed `up_ned`
array preserves image orientation when looking nearly straight up or down.
Both are optional; without `direction_ned`, the surface camera starts aimed at
the Sun. The JSON `altitude` is the requested clearance above sampled terrain or water
in the configured world distance unit (`0.002` km is 2 m in the development
scene). The printed LLA altitude varies with terrain or water height, while the reusable
config snippet keeps the requested clearance and walking speed.
Surface mouse look keeps heading and pitch independent: horizontal mouse
movement changes compass heading, while vertical movement changes only pitch.
Pitch stops at 89.9 degrees above or below the local horizon so the view cannot
cross the vertical pole and flip or begin to roll.

Each planet can configure `surface_noise` as a list of overlapping functions.
Each function has a `type` (`value_fbm` for smooth hills or `ridged_fbm` for
ridges), `seed`, `amplitude_m`, `frequency`, `octaves`, `persistence`, and
`lacunarity`. The function heights add together. The development planet combines
6.8 m smooth hills and 4 m ridged relief. Smaller amplitudes can be hard to
see beside the 10–18 m broad terrain features and water. `noise_seed` is only
the fallback for `surface_noise` functions that omit `seed`; the development scene specifies a
seed on each function. `terrain_landscape` adds a broad continental height
field, low-roughness plain regions, and ridge-weighted cliffs. Its own `seed`
controls those large-scale shapes, including the broad cliff regions. Change
`terrain_landscape.seed` for a noticeably different broad landscape; saving
the config rebuilds the mesh automatically. The
optional `enabled` switch, `elevation_offset_m`, `continent_amplitude_m`, `continent_frequency`,
`plain_threshold`, `cliff_threshold`, `cliff_amplitude_m`, `cliff_frequency`,
`ridge_smoothing`, and `seed` control that shape. `ridge_smoothing` rounds the
otherwise pointed absolute-noise crest; `0` preserves a sharp crest and the
development value `0.25` softens high-angle ridge peaks. Areas below the water
level form ocean basins.
Vertex colors interpolate green plains, pale cliffs, and dark seabed across
triangles, without visible triangle outlines. Sunlight and moonlight illuminate
the terrain; land textures remain future work.

`terrain_lod` divides terrain into near, middle, and far surface-distance
zones. Its `near_surface_distance_m` and `mid_surface_distance_m` are distances
along the planet from the camera's radial position. The development scene uses
16 edge segments near the camera, 8 in the middle, and 3 far away;
`max_edge_segments`, `medium_edge_segments`, and `base_edge_segments` configure
those densities. Nearby middle/near faces whose sampled rise-over-run exceeds
`steep_slope_threshold` can use `steep_edge_segments` instead. The development
scene raises those faces from 16 to 32 segments. If `max_triangle_budget` would
be exceeded, middle-zone refinements are removed before near-zone refinements;
the choice within each zone is stable while the camera moves. All
vertices sample the same planet-fixed height function;
nearby faces have more vertices and therefore resolve finer noise, while far
faces sample it sparsely. The ground no longer changes height when the camera
moves and the mesh is rebuilt. Adjacent faces share the same boundary samples
even when their densities differ, so the shell stays closed. The terrain mesh is rebuilt
after about 10 m of camera movement on the surface, keeping the faster walk
from rebuilding it too often. Walking rebuilds the CPU geometry in the
background and uploads it on the main thread when ready, so camera input and
rendering continue during noise evaluation. The configurable
`max_triangle_budget` caps the development terrain at 100,000 triangles per
planet. The older `lod_near_diameters` and `lod_far_diameters` remain in the
config for compatibility with earlier distance tests; the renderer uses the
surface-distance zones.
Faces retain their current detail level for another 20 m while the camera
moves away from a zone boundary, so walking back and forth does not repeatedly
switch their tessellation.

`water` sets `enabled`, `level_m`, `color`, `opacity`, and
`reflection_fraction` for a translucent spherical sea. The development scene
uses 50% opacity and a 50% mix of water color and reflected sky/Sun
light. Opaque terrain hides water above its level and remains visible through
water below it. The reflection pass includes the sky and opaque bodies, including
terrain, lit by the same sunlight and reflected body light. It does not refract
the scene. The water shell uses the same
triangle budget as the land, but its uniformly subdivided geometry stays fixed
while the camera moves because the sea has no height noise.

The terrain choice follows NVIDIA's guidance on [broad and fine procedural
noise](https://developer.nvidia.com/gpugems/gpugems3/part-i-geometry/chapter-1-generating-complex-procedural-terrains-using-gpu)
and [camera-centered terrain detail](https://developer.nvidia.com/gpugems/gpugems2/part-i-geometric-complexity/chapter-2-terrain-rendering-using-gpu-based-geometry).
The water blend is an early approximation of the techniques described in
[Effective Water Simulation from Physical Models](https://developer.nvidia.com/gpugems/gpugems/part-i-natural-effects/chapter-1-effective-water-simulation-physical-models).

Render the same configured scene to a PNG and print background, Sun, planet,
and blue water-like pixel counts and bounding boxes, plus terrain triangle and
zone-face counts:

~~~bash
./build/PlanetSimulation --render-test build/render-test.png
~~~

Capture the configured surface camera view and confirm a configured body is visible:

~~~bash
./build/PlanetSimulation --surface-render-test build/surface-render-test.png
~~~

Capture the initial planet-orbit view and confirm that the planet is visible:

~~~bash
./build/PlanetSimulation --planet-render-test build/planet-render-test.png
~~~

For resolution diagnostics, append `--render-size WIDTH HEIGHT` to any render
test command. The output reports mesh preparation and GPU-complete render time;
the actual framebuffer dimensions are printed because a window manager may
resize the hidden window.

Append `--simulation-time 37` to capture the moving scene at a deterministic
time in seconds. The test suite captures both the initial scene and the surface
and planet orbit views after 37 seconds. Orbital unit tests cover the requested
periods, mass scaling, descendant masses, barycenters, momentum, elliptical
energy and angular momentum, highly eccentric orbits, unit conversion, long
runs, the 20° equatorial alignment, and cameras attached to rotating terrain.

The saved surface view looks across water toward flatter land and cliffs. The
surface render test checks for a visible configured body. The orbit
render test checks that both bodies are visible and separate.

The surface camera is configured in `configs/scenarios/solar_system.json`.
Its `planet_spherical_ned` reference frame follows the selected planet's center:
latitude is measured from its equator toward +Z, longitude from +X toward +Y,
and LLA altitude outward from its spherical reference radius. North, East,
and Down form the local orientation frame. The development start view is saved
with latitude, longitude, clearance, and `direction_ned` in that config file.
Latitude and longitude determine local vertical, but they do not
determine camera roll. `up_ned` remains optional and lets a saved view preserve
its roll, especially when looking almost straight up or down.

The render test hides its GLFW window but still needs a display. On a Linux
machine without one, install `xvfb` and run:

~~~bash
xvfb-run -a ./build/PlanetSimulation --render-test build/render-test.png
~~~

The release executable uses the same commands with `build-release` in place of
`build`.

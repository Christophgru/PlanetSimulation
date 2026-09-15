# PlanetSimulation

| Solar view | Planet surface | Planet orbit |
|:--:|:--:|:--:|
| <a href="docs/screenshots/solar-view.png"><img src="docs/screenshots/solar-view.png" width="220" alt="Sun and planet render test"></a> | <a href="docs/screenshots/surface-view.png"><img src="docs/screenshots/surface-view.png" width="220" alt="Planet surface render test"></a> | <a href="docs/screenshots/planet-orbit.png"><img src="docs/screenshots/planet-orbit.png" width="220" alt="Planet orbit render test"></a> |

These [render-test screenshots](docs/screenshots/) are captured from the
configured development scene; click an image to view it at full size.

A C++20/OpenGL project that currently renders a static Sun and a configured planet from
`configs/scenarios/solar_system.json`.

The visual direction for later landscape and vegetation steps is the
[Quick Grass live demo](https://simondevyoutube.github.io/Quick_Grass/) and its
[GitHub source](https://github.com/simondevyoutube/Quick_Grass). The renderer
currently has broad terrain and water; grass remains future work.

The development scene uses kilometers for world coordinates: the Sun is 1 km
across, the planet center is 10 km from the Sun, and the planet is 200 m across.
Its surface camera starts 2 m above sampled terrain or water, whichever is
higher, and walks at 8 m/s by default.
The development config stores planets in a `planets` array; the surface
camera's `planet_index` selects an entry in that array.

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
above ground or water at the configured `walk_speed_mps` (8 m/s in the
development scene). Returning to planet orbit from the surface keeps the
camera on the same side of the planet.
The cursor is captured in surface mode; press `1`, `3`, or `Esc` to return to an
orbit view and release it. Surface controls also activate automatically when
either orbit camera comes within `1.1 × planet diameter` of the planet center.
Local Down then points radially toward the planet. Close the window to exit.
Saving `configs/scenarios/solar_system.json` reloads the Sun, planets, terrain,
water, and camera starting values automatically after the file settles for
about 0.1 s. Press `R` to request a manual reload at any time.
The active view stays selected when it is still configured, and its position
resets to the edited start value. An invalid or partly written JSON file is
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

Each planet can configure `surface_noise` as a list of overlapping functions.
Each function has a `type` (`value_fbm` for smooth hills or `ridged_fbm` for
ridges), `seed`, `amplitude_m`, `frequency`, `octaves`, `persistence`, and
`lacunarity`. The function heights add together. The development planet combines
6.8 m smooth hills and 0.35 m ridges. `noise_seed` is only the fallback for
`surface_noise` functions that omit `seed`; the development scene specifies a
seed on each function. `terrain_landscape` adds a broad continental height
field, low-roughness plain regions, and ridge-weighted cliffs. Its own `seed`
controls those large-scale shapes. Change `terrain_landscape.seed` for a
noticeably different broad landscape, then press `R` to rebuild its mesh. The
optional `enabled` switch, `elevation_offset_m`, `continent_amplitude_m`, `continent_frequency`,
`plain_threshold`, `cliff_threshold`, `cliff_amplitude_m`, `cliff_frequency`,
and `seed` control that shape. Areas below the water level form ocean basins.
Vertex colors interpolate green plains, pale cliffs, and dark seabed across
triangles, without visible triangle outlines. Land lighting and textures are
still future work.

`terrain_lod` divides terrain into near, middle, and far surface-distance
zones. Its `near_surface_distance_m` and `mid_surface_distance_m` are distances
along the planet from the camera's radial position. The development scene uses
16 edge segments near the camera, 8 in the middle, and 3 far away;
`max_edge_segments`, `medium_edge_segments`, and `base_edge_segments` configure
those densities. Only the first, broad octave of each surface-noise function is
evaluated far away. Finer octaves fade in through the middle zone and are fully
evaluated nearby. Adjacent faces share the same boundary samples even when
their densities differ, so the shell stays closed. The terrain mesh is rebuilt
after about 10 m of camera movement on the surface, keeping the faster walk
from rebuilding it too often. The configurable
`max_triangle_budget` caps the development terrain at 60,000 triangles per
planet. The older `lod_near_diameters` and `lod_far_diameters` remain in the
config for compatibility with earlier distance tests; the renderer uses the
surface-distance zones.
Faces retain their current detail level for another 20 m while the camera
moves away from a zone boundary, so walking back and forth does not repeatedly
switch their tessellation.

`water` sets `enabled`, `level_m`, `color`, `opacity`, and
`reflection_fraction` for a translucent spherical sea. The development level
is 0 m, with 50% opacity and a 50% mix of water color and reflected sky/Sun
light. Opaque terrain hides water above its level and remains visible through
water below it. The reflection currently samples sky colors and Sun direction;
it does not reflect terrain or refract the scene. The water shell uses the same
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

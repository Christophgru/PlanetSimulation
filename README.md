# PlanetSimulation

A C++20/OpenGL project that currently renders a static Sun and a configured planet from
`configs/scenarios/solar_system.json`.

The development scene uses kilometers for world coordinates: the Sun is 1 km
across, the planet center is 10 km from the Sun, and the planet is 200 m across.
Its surface camera starts 2 m above sampled terrain and walks at 2 m/s.
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
moves it down. Use the scroll wheel to zoom. Press `1` for the orbit view or `2`
for the planet surface view. In planet mode, the mouse looks around without a
button and `W`, `A`, `S`, `D` walk along the planet while maintaining ground
clearance.
The cursor is captured in planet mode; press `1` or `Esc` to return to orbit
and release it. Planet controls also activate automatically when the orbit
camera comes within `1.1 × planet diameter` of the planet center. Local Down
then points radially toward the planet. Close the window
to exit.

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
the Sun. The JSON `altitude` is the requested clearance above sampled terrain
in the configured world distance unit (`0.002` km is 2 m in the development
scene). The printed LLA altitude varies with terrain height, while the reusable
config snippet keeps the requested clearance.

Each planet can configure `surface_noise` as a list of overlapping functions.
Each function has a `type` (`value_fbm` for smooth hills or `ridged_fbm` for
ridges), `seed`, `amplitude_m`, `frequency`, `octaves`, `persistence`, and
`lacunarity`. The function heights add together. The development planet combines
0.8 m smooth hills and 0.35 m ridges. Lower elevations are darker and higher
ones lighter. Color is sampled at mesh vertices and blended across triangles,
so triangle outlines do not appear; this is a color effect without lighting.

Mesh detail rises as the camera approaches the planet. `terrain_lod` configures
`base_edge_segments`, `max_edge_segments`, `lod_near_diameters`, and
`lod_far_diameters`. The development scene uses 3 to 16 edge segments, advancing
one at a time; the schema permits a base setting as low as 1. This gives
smaller density steps than the old fourfold subdivision jumps. The hard
cap is 81,920 triangles per planet; the mesh is rebuilt only when its selected
density changes.

Render the same configured scene to a PNG and print background, Sun, and planet
pixel counts and bounding boxes:

~~~bash
./build/PlanetSimulation --render-test build/render-test.png
~~~

Capture the configured surface camera view and confirm a configured body is visible:

~~~bash
./build/PlanetSimulation --surface-render-test build/surface-render-test.png
~~~

The saved surface view looks across the smoothly colored planet with the Sun above its
horizon. The surface render test checks for a visible configured body. The orbit
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

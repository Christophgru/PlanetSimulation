# PlanetSimulation

A C++20/OpenGL project that currently renders a static Sun and one planet from
`configs/scenarios/solar_system.json`.

## Prerequisites

- CMake 3.16 or newer, Git, and a C++20 compiler
- OpenGL 3.3 support, GLFW 3, and GLEW development libraries
- A graphical display for GLFW, including the hidden-window render test

CMake fetches nlohmann/json and GoogleTest during the first configure, so that
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

For a focused check, run either test executable from the repository root:

~~~bash
./build/tests/test_matrix
./build/tests/config_tests
~~~

For a release build without tests, use a separate directory:

~~~bash
cmake -S . -B build-release -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel 2
~~~

## Run the renderer

Run from the repository root because the executable loads `configs/` and
`shaders/` relative to its working directory. Close the window to exit
interactive mode.

~~~bash
./build/PlanetSimulation
~~~

Render the same configured scene to a PNG and print background, Sun, and planet
pixel counts and bounding boxes:

~~~bash
./build/PlanetSimulation --render-test build/render-test.png
~~~

The render test hides its GLFW window but still needs a display. On a Linux
machine without one, install `xvfb` and run:

~~~bash
xvfb-run -a ./build/PlanetSimulation --render-test build/render-test.png
~~~

The release executable uses the same commands with `build-release` in place of
`build`.

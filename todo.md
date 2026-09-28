# next to do's

document the progress and add comments such that when interrupted you can continue right where you left of


|Task |State|Comment|
|:--|--|--:|
| add stats to the actual screen (black with white boarder) when pressing "i" including fps, and % of graphic card utilisation| p| I toggle and bordered black FPS/frame/GPU-time panel implemented; utilization percentage and runtime validation remain. |
|The Project has become a bit hard to keep track of. add a more fine granular folder strcture, such that no 10 FIles are just flying around in a single folder. Exeception may be e.g. the picture folder, but then make sure there are actually only image files in there and the jsons are seperated. | -| |
| adjust quality settings to be automatically chosen such that we always have at least 20 fps. Based on virtual memory, choose the degree of detail in which the scene is rendered| - |
| movements around the poles is really awkward, the planet camerastarts spinning when walking towards the pole.|-|
|  The atmophere is also illuminated if mountains should block the light|-|
|The Oceans should not be illuminated on USER_IO/user_artifacts/image.png on the shadow_side of the Planet.| -||
| when pressing "o" visualize the ellipsis of the planets (and moons) of 10 orbits in diffrent colors (aferage color of surface)|-|
| Add Light atmospheric light bending and also based on the temperature (set temp param by json for now, later calculate it by orbit and sun strength) |-|

# environmental issues :
if environmental changes are needed inside the container, let the user know by adding dependencies here:
- 

## Progress checkpoint — 2026-09-28

- `src/main.cpp` toggles statistics on I press (repeats ignored), preserves
  the Tab hold shortcut, and profiles GPU passes while the panel is visible.
  Events are polled first so profiling and drawing agree on visibility.
- `src/rendering/PerformanceOverlay.h` draws a two-pixel white border around
  a black panel. Existing FPS/frame/GPU-time measurements are retained.
- Remaining: actual graphics-card utilization percentage, with an unavailable
  state on unsupported hardware. GPU milliseconds divided by frame time must
  not be presented as device utilization. Then validate input and rendering.
- Clean CMake configuration in `/tmp/planet-todo-build` failed: OpenGL headers
  and libraries are missing. GLFW headers and Xvfb are also absent. Existing
  `build` cache points at another machine; use a clean build. Runtime validation
  has not been performed.
- Resume with telemetry and build/render checks for the first row, then move
  to automatic quality. Other rows have not been changed.

# States: 
|State |Meaning|
|:--|--:|
|"-" or " "| not yet started|
|"p"| in progress|
|"i"| implemented|
|"t"|tested and documented|

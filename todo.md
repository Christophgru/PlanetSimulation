# next to do's

document the progress and add comments such that when interrupted you can continue right where you left of


|Task |State|Comment|
|:--:|:--:|:--:|
| add stats to the actual screen (black with white boarder) when pressing "i" including fps, and % of graphic card utilisation| v| I/Tab panel and optional Linux NVML GPU utilization implemented. GPU ABI/failure tests, real X11 input/border tests and performance replay passed; README captures refreshed. Physical NVIDIA hardware is unavailable here. |
|The Project has become a bit hard to keep track of. add a more fine granular folder strcture, such that no 10 FIles are just flying around in a single folder. Exeception may be e.g. the picture folder, but then make sure there are actually only image files in there and the jsons are seperated. | v| Grouped source/tests/shaders by subsystem; images separated from replay JSON and generation records. Clean build, all 33 CTest tests, layout/link checks and gallery generation passed. |
| adjust quality settings to be automatically chosen such that we always have at least 20 fps. Based on virtual memory, choose the degree of detail in which the scene is rendered| t | Adaptive scene scale and graphics-memory cap implemented. Controller and memory-limited live-render tests passed; full 35-test suite passed. 20 FPS is a target; software rendering reached minimum scale below it. |
| movements around the poles is really awkward, the planet camerastarts spinning when walking towards the pole.|t| View basis follows great-circle walking across either pole. North/south crossing tests, all 35 CTest tests and gallery refresh passed. |
|  Bug: The atmophere is also illuminated if mountains should block the light|-|
|Bug The Oceans should not be illuminated on USER_IO/user_artifacts/image.png on the shadow_side of the Planet.| -||
| When pressing "o" visualize the ellipsis of the planets (and moons) of some orbits in diffrent colors (average color of surface) and add a label to each planet that lists its most important parameters. (make it disappear when we have a planet cam)|-||
|when switching from orbital cam to planet cam, add a transition phase of 1 sec where we smoothly drop to the planets surface and reorient our camera in a smooth movement|||
| Add Light atmospheric light bending and also based on the temperature (set temp param by json for now, later calculate it by orbit and sun strength) |-|

# environmental issues :
if environmental changes are needed inside the container, let the user know by adding dependencies here:
- PNG writing requires `libpng-dev` (already available here).
- Headless input/capture checks use `xvfb`, `xauth`, `xdotool`, and `imagemagick`.

## Progress checkpoint — 2026-09-28

- Work in table order. Mark `t` automatically only after tests pass and documentation is updated.
  `v` is reserved for user verification. Commit each feature after successful tests,
  before starting the next row.
- First row: I toggle, Tab hold, bordered panel, FPS/frame/GPU pass time implemented.
  Added optional Linux NVML device-wide utilization, sampled off the render thread.
  Ambiguous adapters, software renderers, missing drivers and errors show N/A.
- First row is t: GpuUtilizationTests, PerformanceOverlayInputIntegration and
  PerformanceCaptureIntegration passed; gallery regenerated and visually reviewed.
- Environment now supports builds and headless rendering: `/tmp/planet-readme-build`,
  RelWithDebInfo, Xvfb, Mesa llvmpipe OpenGL 4.5. Physical NVIDIA validation is
  unavailable here; the optional driver ABI is exercised with a test library.
- Folder organization is t: clean `/tmp/planet-organized-build`, all 33 CTest
  tests passed (70 s), gallery regenerated, links and hashes verified.
- Automatic quality is t: scene scale falls from 100% toward 25% after
  sustained frame times over 55 ms and recovers below 32 ms. Driver-reported
  memory or `--video-memory-mb` caps the initial scale. Full 35-test suite
  passed, including live low-memory rendering and overlay input. Gallery
  regenerated. A strict 20 FPS guarantee is not possible at minimum detail.
- Pole traversal is t: view/up follow the great-circle walk step; heading
  updates in the new tangent frame. Both poles covered by regression tests.
  Full 35-test suite passed and gallery refreshed.
- Next row: block direct atmospheric illumination behind terrain.

# States: 
|State |Meaning|
|:--|--:|
|"-" or " "| not yet started|
|"p"| in progress|
|"i"| implemented|
|"t"|automated tests implemented and passed; documented; may proceed to next task|
|"v"|verified by the user|

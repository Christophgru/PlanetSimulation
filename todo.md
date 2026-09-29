# next to do's

document the progress and add comments such that when interrupted you can continue right where you left of


|Task |State|Comment|
|:--:|:--:|:--:|
| add stats to the actual screen (black with white boarder) when pressing "i" including fps, and % of graphic card utilisation| v| I/Tab panel and optional Linux NVML GPU utilization implemented. GPU ABI/failure tests, real X11 input/border tests and performance replay passed; README captures refreshed. Physical NVIDIA hardware is unavailable here. |
|The Project has become a bit hard to keep track of. add a more fine granular folder strcture, such that no 10 FIles are just flying around in a single folder. Exeception may be e.g. the picture folder, but then make sure there are actually only image files in there and the jsons are seperated. | v| Grouped source/tests/shaders by subsystem; images separated from replay JSON and generation records. Clean build, all 33 CTest tests, layout/link checks and gallery generation passed. |
| adjust quality settings to be automatically chosen such that we always have at least 20 fps. Based on virtual memory, choose the degree of detail in which the scene is rendered| t | Adaptive scene scale and graphics-memory cap implemented. Controller and memory-limited live-render tests passed; full 35-test suite passed. 20 FPS is a target; software rendering reached minimum scale below it. |
| movements around the poles is really awkward, the planet camerastarts spinning when walking towards the pole.|v| View basis follows great-circle walking across either pole. North/south crossing tests, all 35 CTest tests and gallery refresh passed. |
|  Bug: The atmophere is also illuminated if mountains should block the light|v| Terrain shadow map attenuates direct atmospheric scattering; GL shadow/no-shadow tests, full 35-test suite and gallery refresh passed. |
|Bug The Oceans should not be illuminated on USER_IO/user_artifacts/image.png on the shadow_side of the Planet.|v| Local Sun incidence and terrain shadows gate water reflections; dark/day GL regression, full 35-test suite and gallery refresh passed. |
| When pressing "o" visualize the ellipsis of the planets (and moons) of some orbits in diffrent colors (average color of surface) and add a label to each planet that lists its most important parameters. (make it disappear when we have a planet cam)|t| O toggles ten predicted revolutions colored by average terrain tint; body labels show orbital/physical parameters and hide in planet cameras. Geometry, X11 input, all 37 tests and gallery refresh passed. |
|when switching from orbital cam to planet cam, add a transition phase of 1 sec where we smoothly drop to the planets surface and reorient our camera in a smooth movement|v| One-second wall-clock descent interpolates eye, orientation and FOV above sampled terrain/water for manual and automatic entry. Geometry, live X11 midpoint, all 39 tests and gallery refresh passed. |
| Add Light atmospheric light bending and also based on the temperature (set temp param by json for now, later calculate it by orbit and sun strength) |t| Existing curved-ray renderer and JSON temperature model now have CPU and GPU temperature regressions; all 39 tests, documentation and gallery refresh passed. Orbital heat calculation remains future work as requested. |
|For the ellipsis tracing visualize past ellipsis, not future, and fade the last 20% of the ellipsis to avoid a hefty cut.|t| Ten past revolutions sampled in the moving hierarchy; oldest 20% uses smooth alpha fade. Geometry, real X11 toggle and all 39 CTest tests passed; README updated. |
|Check the image at USER_IO/user_artifacts/image.png how can we avoid those cuts? maybe se some gradient field instead of rasterisation? The issue happens at the Poles. In the shadow of a mountain.|p| Controlled thin-atmosphere view reproduced dark slits through the Sun; finite-depth geometry now keeps its rasterized silhouette and a GPU regression passes. Large sky contours in the supplied screenshot still need its camera/time replay for verification. |
|Add a extreme atmosphere light bending exampe to the readme and journal with renderings. Make light bending a optional feature that can be turned on or off via json|t| Matched extreme on/off renderings, README, journal/PDF and exact replays added. Existing refraction_enabled JSON switch controls bending; gallery hashes and replay comparison passed. 38 full-suite tests passed, then the corrected layout test passed. |
|add textures, roughness and mini elevation for performant details, make steeper gradients more grey and give them a rocky look (Do so without crazy amounts of polygons, instead use the maps tro crreate rock like local landscape)|t| Planet-local procedural color, rough diffuse shading, centimetre-scale normal relief and gray steep slopes implemented without added geometry. GPU material regression, all 39 CTest checks and the 15-image gallery/hash validation passed. |
|Keep the water reflective and smooth. Make the gradient where grass becomes rock adjustable and keep grass longer before setting cliff. |||
|add foliage like the grass in the quickgrass demo (copy it as close as possible) |||
# environmental issues :
current apt get has the following dependencies 
    git \
    curl \
    ca-certificates \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    libglfw3-dev \
    libglew-dev \
    libglm-dev \
    libgtest-dev \
    xvfb \
    mesa-utils \
    xauth \
    libgl1-mesa-dri \
    libglx-mesa0 \
    xdotool \
    imagemagick \
    ffmpeg 
if environmental changes are needed inside the container, let the user know by adding dependencies here as a code listing such that its easy to copy and paste:

- PNG writing requires `libpng-dev` (already available here).
- Headless input/capture checks use `xvfb`, `xauth`, `xdotool`, and `imagemagick`.

## Progress checkpoint — 2026-09-29

- Terrain material validation is complete. All 39 CTest checks passed across
  the full run and a focused rerun: the initial layout failure was the missing
  terrain image, now published; the overlay input timeout passed in isolation.
  All 15 gallery captures were regenerated and hashes checked. Inspected the
  terrain-detail image. Headless overview: 14,784 non-background pixels,
  bounding box (1,1)–(794,598), 3,466 planet pixels. No dependencies added.
- Next in table order: preserve smooth reflective water and expose the
  grass-to-rock slope range. Then foliage; asked the user for the exact
  QuickGrass URL/repository before matching that demo.

## Previous checkpoint — 2026-09-28

- Terrain material work is implemented in `shaders/terrain/basic.{vert,frag}`
  and bound per body in `src/main.cpp`. It uses two filtered three-dimensional
  noise scales, slope-based gray rock, rough diffuse reflection, and a
  centimetre-scale normal perturbation in body-local metres. No geometry or
  collision changes. `TerrainMaterialRender` checks visible sub-triangle
  detail, gray slopes, invariant depth, body rotation, distance filtering,
  and the dark night side. The initial full 39-test suite passed; final
  validation and 15-image gallery refresh follow the final visual tuning.
  A new land camera is saved under `docs/captures/replay/terrain`.
  On Mesa llvmpipe, a single 30-frame 800x600 moving benchmark gave median
  frame time 386.27 ms before and 390.15 ms after (frames 5–29). The source
  baseline was rendered from a separate shader copy; triangle counts match.

- Extreme-refraction example is t: matched 1280×720 renderer captures
  are in `docs/screenshots/refraction-extreme-{on,off}.png`, with exact replay
  JSON under `docs/captures/replay/refraction`. Both scenes share a 15 m shell, 150 kPa,
  180 K and 15° FOV; only `refraction_enabled` changes. README and Typst/PDF
  include the comparison and disclose the visible sky-edge notch. The gallery
  generator published 14 images with verified hashes. The full suite passed
  38/39 checks, then the failed ten-file folder check passed after placing
  the two new replay sidecars in their own subfolder. The JSON toggle is
  already implemented and covered by existing atmosphere tests.
- Validation build moved to ignored `/workspace/build-codex` because `/tmp`
  build trees are cleared between interrupted turns. Reuse this build.
- Cut-artifact investigation: standard scene on/off and a dense-air scene did
  not reproduce the supplied image. A controlled thin, cold atmosphere at a
  low surface camera did reproduce dark horizontal slits through the Sun.
  `shaders/atmosphere/atmosphere.frag` now limits image reprojection to pixels
  with sky depth, preserving finite-depth body silhouettes. On/off visual
  comparison and `AtmosphereRefractionRenderIntegration` pass. A small bright
  notch can remain next to the Sun; broad contours in the supplied image are
  not yet reproduced. Await its camera/time or replay details; do not mark t.
- Past orbit trail row is t: `pastOrbitTrails` orders samples from ten periods
  ago to the current epoch and gives the oldest fifth a smooth opacity ramp.
  The line shader blends per-vertex alpha. Clean build and all 39 CTest tests
  passed under Xvfb. The orbit input test failed once from a fluctuating
  screenshot, then passed on rerun and in the full suite.
- Previous next step (now completed): finish terrain validation and commit it. The broad
  sky bands in the supplied image remain open pending its camera/time replay;
  fixed-step terrain-shadow integration is a possible cause, not verified.
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
- Atmospheric mountain shadows are t: each planet's terrain depth map
  attenuates direct view-path scattering. A GL test checks blocked/unblocked
  scattering and map binding; sunset stays warm with finite solar-disk
  twilight. All 35 tests passed and gallery refreshed.
- Night-side ocean is t: local Sun incidence and terrain shadows gate
  reflections; invalid reflected-camera UVs are ignored. Indirect water light
  remains. Dark/day GL regression, all 35 tests and gallery refresh passed.
- O-key orbit visualization is t: ten predicted inertial revolutions include
  the moving parent for moons; labels show name, parent, axes, eccentricity,
  period, radius and mass in the Sun orbit camera. Geometry and real X11
  toggle/camera tests, all 37 CTest tests and gallery refresh passed.
- Surface-camera transition is t: manual and automatic entry interpolate eye,
  orientation and FOV for one wall-clock second while staying above sampled
  terrain/water. Paused simulation still permits the descent; changing modes
  cancels it. Geometry, real X11 midpoint, all 39 CTest tests and gallery
  refresh passed.
- Temperature-controlled atmospheric refraction is t: the existing JSON
  `temperature_k` feeds composition/density optics and curved view rays.
  Added CPU and GPU regressions that compare cool and warm air; all 39 CTest
  tests passed. README documents configuration and the future orbital heat
  calculation remains outside this row. Gallery refreshed.

# States: 
|State |Meaning|
|:--|--:|
|"-" or " "| not yet started|
|"p"| in progress|
|"i"| implemented|
|"t"|automated tests implemented and passed; documented; may proceed to next task|
|"v"|verified by the user|

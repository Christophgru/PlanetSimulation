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
|Keep the water reflective and smooth. Make the gradient where grass becomes rock adjustable and keep grass longer before setting cliff. |t| Per-planet terrain_material slope range added (35–55° defaults), shared by gray-rock shading and broad darkening. Config/CPU/GPU regressions, smooth water reflection check, all 39 CTest tests and refreshed 15-image gallery/hash checks passed. |
|add foliage like the grass in the quickgrass demo (copy it as close as possible) |t| SimonDev Quick_Grass port implemented with instanced curved blades, distance LOD, wind, bright tips, slope/water exclusion, terrain shadows and water reflections. MIT attribution preserved. All 41 CTest tests and refreshed 16-image gallery/hash checks passed on the current scene settings. |
|Compile time has become pretty long, lets do a stabilisation commit where we try to clean up unnecessary header includes, and increase linking instead of compiling huge files new. |t| Config parsing, terrain generation, mesh/shader and foliage implementations moved into three compiled libraries; JSON removed from value-type headers. All 41 tests and 12 independent-header checks passed; grass replay byte-identical. Terrain implementation rebuild compiles one object and relinks consumers in 4.32 s. |
|Spawn the Foliage in a gaussian distribution around the current position, such that at the current position there are most and far away only view. If its coputationally too complex add 5 distance zones.|t| Gaussian placement with sigma = draw distance / 3 and integrated budget scaling implemented. Equal-area radial density, moving-camera and pole tests pass; all 41 CTest checks, exact replay and refreshed 16-image gallery/hash checks passed. |
|as visible in image USER_IO/user_artifacts/image copy.png the water boarders are still pretty rough, the sand applies to whole (huge) triangles and the grass doesn't qite reach the water. Lets think of ways to keep it performant for large scale but have accurate, fine triangular resolution when we get close to the water.|t| Shared-edge local shoreline subdivision, adaptive water shell and per-fragment altitude palette implemented; foliage uses the same altitude biome. All 41 checks pass, including topology/budget and sub-triangle beach tests. Close before/after inspected; 17-image gallery refreshed and hashes verified. |
|When the Camera gets close to a steep mountain, its possible to look inside the planet, lets fix that |t| Live near-plane clipping now uses ground clearance instead of mountain altitude, including descent. Live-versus-capture regression fails on the old executable and passes with the fix. All 42 tests pass; 17-image gallery refreshed and verified. |
|Let's outsource the rendering part from main to a Renderer Class that initializes during the constructor call and tears down in the destructor. Main should only include configparser and renderer, and maybe some commandargs parse utils, but keep it really minimal and move all the stuff in seperate classes.||| 
|Lets add a parameter that determines how sharply the grass foliage falls of |||
|Lets add a third person camera "4" and a actor on the same position as cam 2 that follows a small astronaut that can walk around the planet. Create a 3d model of the astronaut or download some nice MIT licensed one online (comic style). Create Walking movement such that the feet stay on the ground and dont slide over it. Search if tere is a nice library for that movement. If not, approvximate the foor movement for now and we will coma back later to that.|||
|Make foliage movement independant of planetary movement, such that if pressed t only the planet movement stop, but local grass movement keeps going|||
|The quick grass demo mentioned something like 6 poly grass if close and 1 pol grass if far away, is this already active? if not add it.|||
|Lets add a render Mode, where rendering might take arbitrary long but foliage is generated for a radius up to 20x the normal distance, high poly is loaded for the moon and sun, and lens flaring is simulated. |||
|Lets give the grass (ground, not foliage ) a more brown and grey tinted color, such that it looks more like the foliage.|||
|Lets adjust the foliage wind noise such that the grass stays pressed down on the trail where the small astronaut walked along, add jumping motion, that lets the astronaut jump higher or lower depending on the planets gravity, based on mass and diameter of the planet minus its rotation velocity.|||
|Make the Sand more white and add some granularity to it (foliage or roughness/reflection map maybe in combination with tiny elevation noise that creates tiny dunes (only some cm high, as reference take image USER_IO/user_artifacts/image copy 2.png) that are often visible in sand in windy areas)|||
|Prepare a short journal in typst on wether its feasable to port the current application to a web assembly application and evaluate the advantages and disadvantages in a short 2 page memo. Add some graphics on how the compilation process works, what runtime dependencies there are and where the main caviats might lie.|||


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

## Progress checkpoint — 2026-09-30

- Mountain-altitude clipping fix validated: all 42 CTest tests passed (88 s).
  The new X11 regression compares a paused live window with its capture on
  an 80 m plateau, with the eye 2 m above the ground. The old executable
  fails; the fixed frame shows 921,217 ground pixels, bounds
  (0,0)–(1279,719), and matches the capture. Test windows retain their initial
  dimensions to avoid cursor warps changing the view during mouse capture.
  All 17 gallery images refreshed and hashes verified; only the timing
  overlay image changed. Next: extract Renderer ownership and lifecycle.

## Previous checkpoint — 2026-09-29

- Shoreline work from `a3a4b8b` is validated. Shared-edge refinement stays
  watertight and within the configured cap; local water geometry follows the
  same refinement target. Beach shading and foliage biome selection use root/
  fragment altitude. The close shoreline capture has 518,393 non-background
  pixels, bounds (0,0)–(959,539), 100,000 land and 60,000 water triangles.
  All 17 gallery images refreshed, hashes checked, shoreline and solar views
  inspected. All 41 CTest checks pass across the full run and focused reruns;
  the sole stale foliage test now respects the user's 400 m limit and README
  documents the 120 m working distance. No new dependencies.
  Next: steep-mountain camera clipping. A controlled 80 m plateau replay
  reproduces the bug: live rendering uses reference-sphere altitude to set
  its near plane, whereas captures use the configured ground clearance.
- Gaussian foliage density complete. Peak requested density is normalized
  against the integrated Gaussian to preserve the hard instance budget.
  Seeded candidate roots remain stable when moving the patch. Radial annuli,
  moving center, pole symmetry, full 41-test suite and exact paused replay
  passed. All 16 gallery images regenerated and hashes checked; grass preview
  inspected (921,598 non-background pixels, bounds (0,0)–(1279,719)).
- Compile-time stabilization complete: substantial implementations now have
  `.cpp` files and shared static libraries. All 41 CTest entries passed under
  Xvfb with two jobs; real X11 input tests now run serially to avoid focus
  collisions. Twelve public headers compile independently. Frozen grass
  capture matches the saved pre-refactor executable byte for byte. Overview
  inspected: 14,784 non-background pixels, bounds (1,1)–(794,598).
  Touching Terrain.cpp rebuilt one object and relinked ten executables in
  4.32 s (GCC/Ninja/RelWithDebInfo, two jobs). No dependencies added.
- Foliage implementation now lives in `src/rendering/foliage` and
  `shaders/foliage`, configured by `src/config/FoliageConfig.h`. The existing
  README already confirms SimonDev Quick_Grass as the reference. Adapted its
  six/one-segment blades, palette and light response with the MIT notice in
  `external/quick-grass`. Terrain roots are sampled from rendered triangles;
  same-LOD overlap is seeded independently of camera rejection. Wind uses
  simulation time; patches clear on reload. Three-metre surface preview is
  saved as the new foliage replay and inspected. CPU/GPU/full-capture tests
  and updated gallery are complete: all 41 CTest tests passed (70 s), all 16
  images refreshed and hashes checked. Tests/documentation now honor the user
  density limit of 4096 and working scene camera at 2 m. No new dependencies.
- Resumed after workspace permissions were restored. Configurable slope range
  is implemented in ScenarioConfig, TerrainSurface and the terrain shader,
  with config/CPU/GPU regressions and README/config documentation. All 39
  CTest tests passed in one run (93 s); all 15 gallery images refreshed and
  hashes checked; terrain-detail visually inspected. Overview render: 14,784
  non-background pixels, bounding box (1,1)–(794,598), 3,471 planet pixels.
  Water retains its separate smooth shader; sharp two-color reflection test passes.
  Found the likely foliage reference: https://github.com/simondevyoutube/Quick_Grass;
  asked the user to confirm while finishing the preceding row.
- Terrain material validation is complete. All 39 CTest checks passed across
  the full run and a focused rerun: the initial layout failure was the missing
  terrain image, now published; the overlay input timeout passed in isolation.
  All 15 gallery captures were regenerated and hashes checked. Inspected the
  terrain-detail image. Headless overview: 14,784 non-background pixels,
  bounding box (1,1)–(794,598), 3,466 planet pixels. No dependencies added.
- Next in table order: steep-mountain camera clearance, then the Renderer
  extraction and the newer entries below it. Preserve the
  user's new scene settings. The earlier sky-cut replay is still unavailable.

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

#set document(title: "PlanetSimulation: WebAssembly feasibility", author: "PlanetSimulation engineering journal")
#set page(paper: "a4", margin: (x: 17mm, y: 15mm), numbering: "1", footer: context align(right, text(size: 8pt, fill: rgb("617084"))[PlanetSimulation · WebAssembly feasibility · #counter(page).display("1") / 2]))
#set text(font: "Libertinus Serif", size: 10pt)
#set par(leading: .55em, justify: true)
#set heading(numbering: none)
#show heading.where(level: 1): set text(size: 18pt, fill: rgb("173c60"))
#show heading.where(level: 2): set text(size: 11.5pt, fill: rgb("173c60"))
#set block(spacing: .65em)
#let node(title, detail, tint: "eef4fa") = block(width: 100%, height: 18mm, inset: 5pt, radius: 3pt, fill: rgb(tint), stroke: .5pt + rgb("a8bbcd"))[
  #align(center)[#text(size: 9pt, weight: "bold")[#title] \ #text(size: 8.4pt)[#detail]]
]
#let ref(id, url, name) = link(url)[#id — #name]

= Can PlanetSimulation run in a browser?
#text(size: 8.5pt, fill: rgb("617084"))[Two-page feasibility memo · 2 October 2026 · native v0.0.1 at #raw("c464cd3")]

*Assessment:* feasible with a dedicated browser platform and graphics adapter.
The C++ simulation, noise, camera math and astronaut IK can largely be reused.
The current executable cannot be ported by changing the compiler alone: its
desktop OpenGL features, blocking frame loop and filesystem workflow need
adaptation. A WebGL 2 demonstration is the smaller first step; WebGPU is the
stronger candidate if preserving compute-based foliage and the planned GPU
terrain is the priority. This is a source audit and design assessment, not a
measured browser build or an FPS prediction.

== Compilation and delivery
#figure(
  grid(columns: (1fr, 9pt, 1fr, 9pt, 1fr, 9pt, 1fr), align: center + horizon,
    node([C++ + dependencies], [GLM, JSON, ozz, PNG]), [→],
    node([Emscripten toolchain], [emcmake → em++ → link]), [→],
    node([Browser bundle], [.wasm + JS + assets]), [→],
    node([HTTP(S) delivery], [HTML canvas + loader], tint: "eaf5ee")),
  caption: [Proposed build: compile native libraries for Wasm, then package shaders and scenes. GPU shaders are compiled by the browser, not into the C++ Wasm module.],
)

Emscripten supplies the CMake toolchain through `emcmake` [S1]. A new CMake
platform branch must replace the present required system OpenGL, GLEW and GLFW
packages; native static libraries cannot be linked into Wasm. Pin the SDK and
library versions, compile C++20 dependencies for that target, and explicitly
preserve the application's config exceptions. Package shaders/configs for the
virtual filesystem or fetch them before initialization. Ship optimized Wasm
with JavaScript glue and an HTML shell; use a local HTTP server for development.

#table(columns: (39mm, 1fr), inset: 5pt, stroke: .4pt + rgb("cbd5de"),
  table.header([*Current dependency*], [*Proposed browser equivalent / remaining work*]),
  [GLM + nlohmann JSON], [Reuse headers and CPU double-precision coordinates; verify unit tests under Wasm.],
  [ozz-animation runtime], [Cross-compile the pinned IK runtime. Its source provides a scalar SIMD reference path; validate gait before optional Wasm SIMD tuning.],
  [GLFW + GLEW], [Use an Emscripten GLFW canvas port [S2] or HTML5 bindings. Replace desktop GL loading and context hints with GLES/WebGL capability checks.],
  [libpng + shader files], [Cross-compile PNG or export through browser APIs; package GLSL libraries and preserve their inclusion order.],
  [std::async + diagnostics], [Choose worker-backed threads or incremental CPU jobs; replace OS telemetry and native capture/input automation.],
)

== The renderer needs more than a fallback switch
The desktop 4.3 foliage path uses compute shaders, SSBOs and indirect draws.
Core WebGL 2 lacks those APIs [S3]. Disabling compute still leaves
`samplerBuffer` terrain vertices, `usamplerBuffer` indices and the trail BVH
buffer texture in the 3.3 vertex path. Replace those with bounded 2-D data
textures and integer `texelFetch` addressing, or a revised vertex attribute
representation. Revalidate roots, LODs, trail deformation and transfer costs.
Convert `330 core` shaders to `300 es`, add precision declarations, and audit
formats, sampler limits and extensions [S3, S4]. Existing instanced draws,
shadow maps and fragment atmosphere passes provide useful reusable structure.

#pagebreak()

= Runtime, tradeoffs and a practical first port
#figure(
  grid(columns: (1fr, 11pt, 1fr, 11pt, 1fr), align: center + horizon,
    node([Wasm CPU state], [Orbits, terrain LOD, IK]), [⇄],
    node([JavaScript / browser], [Frame callbacks + input], tint: "eaf5ee"), [⇄],
    node([WebGL 2 / WebGPU], [GPU resources + canvas])),
  caption: [Proposed runtime boundary. Workers may prepare terrain; virtual files hold assets, while optional IndexedDB holds persistent state. Browser callbacks own frame scheduling.],
)

#table(columns: (30mm, 1fr), inset: 5pt, stroke: .4pt + rgb("cbd5de"),
  table.header([*Caveat*], [*Required adaptation and runtime dependency*]),
  [Frame loop / input], [Split `Interactive.cpp`'s blocking while loop into persistent state and one-frame callbacks via `emscripten_set_main_loop` / requestAnimationFrame [S5]. Canvas focus, resize, pointer lock and keyboard capture need browser handling.],
  [Terrain workers], [Threaded `std::async` needs a pthread build, SharedArrayBuffer and cross-origin isolation with COOP/COEP headers [S6]. Serve securely (localhost for development). Keep a separate single-thread build with incremental terrain work; avoid main-thread waits.],
  [Files / replay], [MEMFS is temporary; IDBFS persists through IndexedDB synchronization [S7]. Replace filesystem timestamp watching with explicit config reload/import, and PNG/JSON writes with downloads. Persistence remains versioned and quota-limited.],
  [HDR / readback], [Require `EXT_color_buffer_float` for RGBA16F/RG32F render targets [S8]. Current RG32F linear lookup also needs float-linear filtering or manual interpolation. Replace stencil/depth readback with colour-encoded diagnostics; use supported RGBA float readback for the highlight meter.],
  [Profiling / recovery], [NVML GPU utilization/VRAM is unavailable through the browser adapter. Track owned allocations; time passes only when timer extensions are available and results are non-disjoint [S9]. Rebuild resources after context loss.],
)

== Benefits and costs
*Benefits:* a shareable URL removes native installation for demos and lets the
same C++ simulation support browser and desktop frontends. JSON scenes and
replays remain useful exchange formats. Static hosting simplifies delivery;
the browser sandbox confines access to the host filesystem.

*Costs:* a second renderer/platform path needs continued testing. Asset download,
startup compilation, memory limits, browser/GPU variation and tab throttling
change the experience. Native llvmpipe timings do not predict browser FPS.
The existing grass queues alone reserve about 25.6 MB at 200,000 candidates;
terrain, water and HDR reflections add further allocations. Set measured
browser budgets; offline 20× foliage is not a suitable default mobile workload.

== Recommendation and acceptance gates
First isolate the window, frame lifecycle, assets and diagnostics; retain the
native tests. Then build a WebGL 2 slice with terrain, atmosphere, water,
astronaut and rewritten vertex-path foliage. Require correct shadows/trails,
config import/export, context recovery and responsive terrain updates. Measure
frame-time percentiles, peak owned memory and download/startup size in Chromium,
Firefox and Safari on actual target devices. Use tolerant image comparisons
across drivers, while checking deterministic simulation and same-backend replay.

If those budgets cannot support the required density, evaluate a separate
WebGPU renderer: compute/storage buffers match the current GPU design [S10],
but bindings, pipelines and GLSL-to-WGSL shaders must be rewritten. Emscripten
provides a C API route through Emdawnwebgpu [S11]. Probe capabilities at runtime;
neither route guarantees current desktop quality or performance without a port.

#text(size: 7.8pt)[
  *Primary documentation checked 2 October 2026.* Project-specific conclusions are inferred from the audited source; links describe APIs, not measured project performance. \
  #ref("S1", "https://emscripten.org/docs/compiling/Building-Projects.html", "CMake builds") ·
  #ref("S2", "https://github.com/pongasoft/emscripten-glfw/blob/master/docs/Usage.md", "GLFW canvas port") ·
  #ref("S3", "https://registry.khronos.org/webgl/specs/2.0.0/", "WebGL 2") ·
  #ref("S4", "https://emscripten.org/docs/porting/multimedia_and_graphics/OpenGL-support.html", "GL mapping") ·
  #ref("S5", "https://emscripten.org/docs/api_reference/emscripten.h.html#emscripten-set-main-loop", "frame callbacks") ·
  #ref("S6", "https://emscripten.org/docs/porting/pthreads.html", "threads") ·
  #ref("S7", "https://emscripten.org/docs/api_reference/Filesystem-API.html", "storage") ·
  #ref("S8", "https://registry.khronos.org/webgl/extensions/EXT_color_buffer_float/", "float targets") ·
  #ref("S9", "https://registry.khronos.org/webgl/extensions/EXT_disjoint_timer_query_webgl2/", "GPU timing") ·
  #ref("S10", "https://developer.chrome.com/docs/capabilities/web-apis/gpu-compute", "WebGPU compute") ·
  #ref("S11", "https://emscripten.org/docs/porting/multimedia_and_graphics/WebGPU-support.html", "WebGPU C API") ·
  #link("https://registry.khronos.org/webgl/extensions/OES_texture_float_linear/")[float filtering].
]

#context [#metadata(counter(page).final().first()) <memo-page-count>]

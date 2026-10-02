# Browser feasibility memo

Read the [two-page WebAssembly memo](wasm.pdf) or edit its [Typst source](wasm.typ).
It audits native v0.0.1 at `c464cd3`, using official Emscripten/Khronos and
maintainer documentation checked on 2 October 2026. The compilation and runtime
diagrams are drawn in Typst without external packages or raster assets.

The conclusion is an engineering assessment: much of the C++ core can be reused,
but both existing graphics paths need browser adaptation. WebGL 2 is a smaller
demonstration path; WebGPU better matches compute-driven foliage and future GPU
terrain at the cost of a new renderer. This task produces a feasibility memo;
it does not implement a Wasm target or establish browser performance.

Audited files include `CMakeLists.txt`, `src/app/Window.cpp`,
`src/rendering/runtime/Interactive.cpp`, `TerrainMeshes.cpp`, `Capture.cpp`,
`src/rendering/foliage/procedural/ProceduralGrass.cpp`, `GrassCompute.cpp`,
`src/rendering/foliage/trails/TrailUpload.cpp`, the foliage shaders,
`src/rendering/atmosphere/AtmosphereTransmittance.h`, `AtmosphereRenderer.h`,
`src/rendering/postprocessing/LensFlare.cpp` and the diagnostics headers.
The fetched ozz runtime's `simd_math_config.h` confirms a scalar reference path;
compatibility of the pinned dependency still requires an actual Wasm build.

To compile the standalone memo:

```sh
typst compile --root . docs/journal/portability/wasm.typ docs/journal/portability/wasm.pdf
typst compile --root . --ppi 110 docs/journal/portability/wasm.typ 'build/wasm-page-{p}-of-{t}.png'
```

Validated with Typst 0.15.1: the PDF compiles, its layout reports exactly two
pages, and both page previews were visually reviewed. Repository layout and
whitespace checks pass. [Validation metadata](validation/evidence.json) records
the PDF/source hashes, page count, audit revision and documentation links.
The native clean 52/52 test run and 22 verified gallery captures belong to the
preceding sand checkpoint; this documentation task does not change runtime
source, native binaries or scene settings.

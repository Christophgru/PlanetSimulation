We are developing PlanetSimulation, a C++20/OpenGL simulation engine.

The long-term goal is to simulate a solar system containing a sun and multiple
planets, with particular emphasis on numerical correctness and procedural
planet rendering.

Important architectural goals:

- Universe-scale positions must retain high precision over astronomical
  distances.
- Simulation/world coordinates should therefore use double precision.
- GPU rendering should use camera-relative/local float coordinates where
  appropriate.
- Every planet should have its own local coordinate system in addition to the
  global solar-system coordinate system.
- Rendering code and simulation logic should remain clearly separated.
- The renderer should initially target OpenGL and GLSL.
- The current rendering GPU may be relatively weak, so correctness and clean
  architecture are initially more important than maximum visual quality.

Future planned features include:

- orbital motion
- procedural spherical terrain
- multi-scale noise
- planetary LOD
- realistic atmospheric scattering
- axial tilt and seasons
- solar irradiance based on sun distance and incidence angle
- temperature and moisture modelling
- biome generation
- vegetation placement
- GPU-instanced grass
- grass movement using wind shaders
- seamless navigation between astronomical and planet-surface scales

Development principles:

- Do not prematurely implement all future systems.
- Prefer small, testable increments.
- Keep coordinate-system mathematics explicit and well tested.
- Avoid coupling physics/simulation state to OpenGL objects.
- Explain important architectural or mathematical decisions before making
  large changes.
- Do not replace working architecture merely to make code shorter.
- When uncertain about a design decision, discuss the tradeoffs first.

For now, inspect the repository and propose the smallest first milestone.
Do not modify any files yet.
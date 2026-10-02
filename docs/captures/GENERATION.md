# README image generation log

## Version 0.0.1 — 2026-10-01 (UTC)

All 17 README images were regenerated with the current renderer. Earlier capture dates and versions were not recorded.

Base source revision: `fdcfbbea42b61d4407c940deb570f8ac6283d789` plus the working-tree changes identified by the source fingerprint.
Build: RelWithDebInfo. Display: Mesa EGL pbuffer harness. OpenGL renderer: llvmpipe (LLVM 20.1.2, 256 bits).
The performance panel reports this capture environment, not hardware GPU performance.

| Image | Version | Last generated (UTC) |
| --- | --- | --- |
| [solar-view.png](../screenshots/solar-view.png) | 0.0.1 | 2026-10-01T21:54:23+00:00 |
| [surface-view.png](../screenshots/surface-view.png) | 0.0.1 | 2026-10-01T21:54:33+00:00 |
| [planet-orbit.png](../screenshots/planet-orbit.png) | 0.0.1 | 2026-10-01T21:54:37+00:00 |
| [moonlit-night.png](../screenshots/moonlit-night.png) | 0.0.1 | 2026-10-01T21:54:41+00:00 |
| [moonless-night.png](../screenshots/moonless-night.png) | 0.0.1 | 2026-10-01T21:54:45+00:00 |
| [refraction-extreme-on.png](../screenshots/refraction-extreme-on.png) | 0.0.1 | 2026-10-01T21:55:10+00:00 |
| [refraction-extreme-off.png](../screenshots/refraction-extreme-off.png) | 0.0.1 | 2026-10-01T21:55:25+00:00 |
| [terrain-detail.png](../screenshots/terrain-detail.png) | 0.0.1 | 2026-10-01T21:55:42+00:00 |
| [shoreline-detail.png](../screenshots/shoreline-detail.png) | 0.0.1 | 2026-10-01T21:55:49+00:00 |
| [grass-detail.png](../screenshots/grass-detail.png) | 0.0.1 | 2026-10-01T21:56:04+00:00 |
| [performance-overlay.png](../screenshots/performance-overlay.png) | 0.0.1 | 2026-10-01T21:58:30+00:00 |
| [atmosphere-day.png](../screenshots/atmosphere-day.png) | 0.0.1 | 2026-10-01T21:58:49+00:00 |
| [atmosphere-sunset.png](../screenshots/atmosphere-sunset.png) | 0.0.1 | 2026-10-01T21:59:02+00:00 |
| [atmosphere-mist.png](../screenshots/atmosphere-mist.png) | 0.0.1 | 2026-10-01T21:59:17+00:00 |
| [atmosphere-dust.png](../screenshots/atmosphere-dust.png) | 0.0.1 | 2026-10-01T21:59:30+00:00 |
| [twilight.png](../screenshots/twilight.png) | 0.0.1 | 2026-10-01T21:59:42+00:00 |
| [terrain-shadows.png](../screenshots/terrain-shadows.png) | 0.0.1 | 2026-10-01T21:59:47+00:00 |

Exact commands, image SHA-256 hashes and renderer details are in [generation.json](generation.json).
Available [replay sidecars](replay/) preserve resolved scenes and cameras.

The solar overview uses the frozen compact fixture; the surface and orbit gallery use the current working scene.
Night images preserve their airless lighting controls. Atmosphere, shadow and twilight images use regression fixtures.

To regenerate after building with `BUILD_TESTING=ON`:

~~~bash
cmake --build build --target PlanetSimulation terrain_shadow_render_tests
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a -s "-screen 0 1280x720x24" python3 scripts/generate_readme_images.py --build-dir build
~~~

Regenerate after changes to rendering, shaders or pictured scenarios; the version alone does not prove freshness.
Commit the images, sidecars and both generation records together.

# README image generation log

## Version 0.0.1 — 2026-09-29 (UTC)

All 16 README images were regenerated with the current renderer. Earlier capture dates and versions were not recorded.

Base source revision: `cfd7e41676c436ee6f6526501d1044e65f03429a` plus the working-tree changes identified by the source fingerprint.
Build: RelWithDebInfo. Display: Xvfb. OpenGL: Mesa llvmpipe (software rendering).
The performance panel reports this capture environment, not hardware GPU performance.

| Image | Version | Last generated (UTC) |
| --- | --- | --- |
| [solar-view.png](../screenshots/solar-view.png) | 0.0.1 | 2026-09-29T10:34:09+00:00 |
| [surface-view.png](../screenshots/surface-view.png) | 0.0.1 | 2026-09-29T10:34:16+00:00 |
| [planet-orbit.png](../screenshots/planet-orbit.png) | 0.0.1 | 2026-09-29T10:34:22+00:00 |
| [moonlit-night.png](../screenshots/moonlit-night.png) | 0.0.1 | 2026-09-29T10:34:27+00:00 |
| [moonless-night.png](../screenshots/moonless-night.png) | 0.0.1 | 2026-09-29T10:34:32+00:00 |
| [refraction-extreme-on.png](../screenshots/refraction-extreme-on.png) | 0.0.1 | 2026-09-29T10:34:51+00:00 |
| [refraction-extreme-off.png](../screenshots/refraction-extreme-off.png) | 0.0.1 | 2026-09-29T10:35:04+00:00 |
| [terrain-detail.png](../screenshots/terrain-detail.png) | 0.0.1 | 2026-09-29T10:35:12+00:00 |
| [grass-detail.png](../screenshots/grass-detail.png) | 0.0.1 | 2026-09-29T10:35:24+00:00 |
| [performance-overlay.png](../screenshots/performance-overlay.png) | 0.0.1 | 2026-09-29T10:36:45+00:00 |
| [atmosphere-day.png](../screenshots/atmosphere-day.png) | 0.0.1 | 2026-09-29T10:36:59+00:00 |
| [atmosphere-sunset.png](../screenshots/atmosphere-sunset.png) | 0.0.1 | 2026-09-29T10:37:12+00:00 |
| [atmosphere-mist.png](../screenshots/atmosphere-mist.png) | 0.0.1 | 2026-09-29T10:37:26+00:00 |
| [atmosphere-dust.png](../screenshots/atmosphere-dust.png) | 0.0.1 | 2026-09-29T10:37:38+00:00 |
| [twilight.png](../screenshots/twilight.png) | 0.0.1 | 2026-09-29T10:37:49+00:00 |
| [terrain-shadows.png](../screenshots/terrain-shadows.png) | 0.0.1 | 2026-09-29T10:37:52+00:00 |

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

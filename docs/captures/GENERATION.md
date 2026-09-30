# README image generation log

## Version 0.0.1 — 2026-09-30 (UTC)

All 17 README images were regenerated with the current renderer. Earlier capture dates and versions were not recorded.

Base source revision: `cd26486c30ed18b5ddb7245be6dfb76e3775a525` plus the working-tree changes identified by the source fingerprint.
Build: RelWithDebInfo. Display: Xvfb. OpenGL: Mesa llvmpipe (software rendering).
The performance panel reports this capture environment, not hardware GPU performance.

| Image | Version | Last generated (UTC) |
| --- | --- | --- |
| [solar-view.png](../screenshots/solar-view.png) | 0.0.1 | 2026-09-30T09:14:22+00:00 |
| [surface-view.png](../screenshots/surface-view.png) | 0.0.1 | 2026-09-30T09:14:28+00:00 |
| [planet-orbit.png](../screenshots/planet-orbit.png) | 0.0.1 | 2026-09-30T09:14:31+00:00 |
| [moonlit-night.png](../screenshots/moonlit-night.png) | 0.0.1 | 2026-09-30T09:14:35+00:00 |
| [moonless-night.png](../screenshots/moonless-night.png) | 0.0.1 | 2026-09-30T09:14:38+00:00 |
| [refraction-extreme-on.png](../screenshots/refraction-extreme-on.png) | 0.0.1 | 2026-09-30T09:14:55+00:00 |
| [refraction-extreme-off.png](../screenshots/refraction-extreme-off.png) | 0.0.1 | 2026-09-30T09:15:05+00:00 |
| [terrain-detail.png](../screenshots/terrain-detail.png) | 0.0.1 | 2026-09-30T09:15:15+00:00 |
| [shoreline-detail.png](../screenshots/shoreline-detail.png) | 0.0.1 | 2026-09-30T09:15:21+00:00 |
| [grass-detail.png](../screenshots/grass-detail.png) | 0.0.1 | 2026-09-30T09:15:32+00:00 |
| [performance-overlay.png](../screenshots/performance-overlay.png) | 0.0.1 | 2026-09-30T09:16:46+00:00 |
| [atmosphere-day.png](../screenshots/atmosphere-day.png) | 0.0.1 | 2026-09-30T09:16:58+00:00 |
| [atmosphere-sunset.png](../screenshots/atmosphere-sunset.png) | 0.0.1 | 2026-09-30T09:17:09+00:00 |
| [atmosphere-mist.png](../screenshots/atmosphere-mist.png) | 0.0.1 | 2026-09-30T09:17:20+00:00 |
| [atmosphere-dust.png](../screenshots/atmosphere-dust.png) | 0.0.1 | 2026-09-30T09:17:32+00:00 |
| [twilight.png](../screenshots/twilight.png) | 0.0.1 | 2026-09-30T09:17:42+00:00 |
| [terrain-shadows.png](../screenshots/terrain-shadows.png) | 0.0.1 | 2026-09-30T09:17:44+00:00 |

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

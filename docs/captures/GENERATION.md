# README image generation log

## Version 0.0.1 — 2026-10-02 (UTC)

All 21 README images were regenerated with the current renderer. Earlier capture dates and versions were not recorded.

Base source revision: `50c38157872fd85242b67dc7a265d63f7d0879dc` plus the working-tree changes identified by the source fingerprint.
Build: RelWithDebInfo. Display: GL window/Xvfb. OpenGL renderer: name of display: :99.
The performance panel reports this capture environment, not hardware GPU performance.

| Image | Version | Last generated (UTC) |
| --- | --- | --- |
| [solar-view.png](../screenshots/solar-view.png) | 0.0.1 | 2026-10-02T10:42:32+00:00 |
| [surface-view.png](../screenshots/surface-view.png) | 0.0.1 | 2026-10-02T10:42:46+00:00 |
| [planet-orbit.png](../screenshots/planet-orbit.png) | 0.0.1 | 2026-10-02T10:42:50+00:00 |
| [moonlit-night.png](../screenshots/moonlit-night.png) | 0.0.1 | 2026-10-02T10:42:55+00:00 |
| [moonless-night.png](../screenshots/moonless-night.png) | 0.0.1 | 2026-10-02T10:42:59+00:00 |
| [refraction-extreme-on.png](../screenshots/refraction-extreme-on.png) | 0.0.1 | 2026-10-02T10:43:23+00:00 |
| [refraction-extreme-off.png](../screenshots/refraction-extreme-off.png) | 0.0.1 | 2026-10-02T10:43:39+00:00 |
| [terrain-detail.png](../screenshots/terrain-detail.png) | 0.0.1 | 2026-10-02T10:43:48+00:00 |
| [shoreline-detail.png](../screenshots/shoreline-detail.png) | 0.0.1 | 2026-10-02T10:43:56+00:00 |
| [grass-detail.png](../screenshots/grass-detail.png) | 0.0.1 | 2026-10-02T10:44:10+00:00 |
| [astronaut.png](../screenshots/astronaut.png) | 0.0.1 | 2026-10-02T10:44:15+00:00 |
| [astronaut-front.png](../screenshots/astronaut-front.png) | 0.0.1 | 2026-10-02T10:44:20+00:00 |
| [astronaut-jetpack.png](../screenshots/astronaut-jetpack.png) | 0.0.1 | 2026-10-02T10:44:26+00:00 |
| [offline-render.png](../screenshots/offline-render.png) | 0.0.1 | 2026-10-02T10:44:29+00:00 |
| [performance-overlay.png](../screenshots/performance-overlay.png) | 0.0.1 | 2026-10-02T10:48:36+00:00 |
| [atmosphere-day.png](../screenshots/atmosphere-day.png) | 0.0.1 | 2026-10-02T10:48:52+00:00 |
| [atmosphere-sunset.png](../screenshots/atmosphere-sunset.png) | 0.0.1 | 2026-10-02T10:49:07+00:00 |
| [atmosphere-mist.png](../screenshots/atmosphere-mist.png) | 0.0.1 | 2026-10-02T10:49:23+00:00 |
| [atmosphere-dust.png](../screenshots/atmosphere-dust.png) | 0.0.1 | 2026-10-02T10:49:38+00:00 |
| [twilight.png](../screenshots/twilight.png) | 0.0.1 | 2026-10-02T10:49:51+00:00 |
| [terrain-shadows.png](../screenshots/terrain-shadows.png) | 0.0.1 | 2026-10-02T10:49:55+00:00 |

Exact commands, image SHA-256 hashes and renderer details are in [generation.json](generation.json).
Available [replay sidecars](replay/) preserve resolved scenes and cameras.

The solar overview uses the frozen compact fixture; the surface and orbit gallery use the current working scene.
Night images preserve their airless lighting controls. Atmosphere, shadow and twilight images use regression fixtures.

Astronaut images preserve camera 4, planted-foot or airborne pose, and bubble phase from the character regression fixture.

The offline image preserves the controlled 20x grass radius, high-detail bodies and partially visible Sun flare.

To regenerate after building with `BUILD_TESTING=ON`:

~~~bash
cmake --build build --target PlanetSimulation terrain_shadow_render_tests
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a -s "-screen 0 1280x720x24" python3 scripts/generate_readme_images.py --build-dir build
~~~

Regenerate after changes to rendering, shaders or pictured scenarios; the version alone does not prove freshness.
Commit the images, sidecars and both generation records together.

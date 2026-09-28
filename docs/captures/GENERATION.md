# README image generation log

## Version 0.0.1 — 2026-09-28 (UTC)

All 12 README images were regenerated with the current renderer. Earlier capture dates and versions were not recorded.

Base source revision: `bfee9ce89391a1dbeb434373c391a7ed8670425d` plus the working-tree changes identified by the source fingerprint.
Build: RelWithDebInfo. Display: Xvfb. OpenGL: Mesa llvmpipe (software rendering).
The performance panel reports this capture environment, not hardware GPU performance.

| Image | Version | Last generated (UTC) |
| --- | --- | --- |
| [solar-view.png](../screenshots/solar-view.png) | 0.0.1 | 2026-09-28T10:32:56+00:00 |
| [surface-view.png](../screenshots/surface-view.png) | 0.0.1 | 2026-09-28T10:32:57+00:00 |
| [planet-orbit.png](../screenshots/planet-orbit.png) | 0.0.1 | 2026-09-28T10:32:58+00:00 |
| [moonlit-night.png](../screenshots/moonlit-night.png) | 0.0.1 | 2026-09-28T10:32:59+00:00 |
| [moonless-night.png](../screenshots/moonless-night.png) | 0.0.1 | 2026-09-28T10:33:00+00:00 |
| [performance-overlay.png](../screenshots/performance-overlay.png) | 0.0.1 | 2026-09-28T10:33:16+00:00 |
| [atmosphere-day.png](../screenshots/atmosphere-day.png) | 0.0.1 | 2026-09-28T10:33:20+00:00 |
| [atmosphere-sunset.png](../screenshots/atmosphere-sunset.png) | 0.0.1 | 2026-09-28T10:33:24+00:00 |
| [atmosphere-mist.png](../screenshots/atmosphere-mist.png) | 0.0.1 | 2026-09-28T10:33:28+00:00 |
| [atmosphere-dust.png](../screenshots/atmosphere-dust.png) | 0.0.1 | 2026-09-28T10:33:32+00:00 |
| [twilight.png](../screenshots/twilight.png) | 0.0.1 | 2026-09-28T10:33:34+00:00 |
| [terrain-shadows.png](../screenshots/terrain-shadows.png) | 0.0.1 | 2026-09-28T10:33:35+00:00 |

Exact commands, image SHA-256 hashes and renderer details are in [generation.json](generation.json).
Available [replay sidecars](replay/) preserve resolved scenes and cameras.

The solar overview uses the frozen compact fixture; the surface and orbit gallery use the current working scene.
Night images preserve their airless lighting controls. Atmosphere, shadow and twilight images use regression fixtures.

To regenerate after building with `BUILD_TESTING=ON`:

```bash
cmake --build build --target PlanetSimulation terrain_shadow_render_tests
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a -s "-screen 0 1280x720x24" python3 scripts/generate_readme_images.py --build-dir build
```

Regenerate after changes to rendering, shaders or pictured scenarios; the version alone does not prove freshness.
Commit the images, sidecars and both generation records together.

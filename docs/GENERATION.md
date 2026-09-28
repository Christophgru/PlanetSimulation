# README image generation log

## Version 0.0.1 — 2026-09-28 (UTC)

All 12 README images were regenerated with the current renderer. Earlier capture dates and versions were not recorded.

Base source revision: `54a24e3748fb0e4dcb5603a0185a72776660d04b` plus the working-tree version, PNG writer and documentation changes.
Build: RelWithDebInfo. Display: Xvfb. OpenGL: Mesa llvmpipe (software rendering).
The performance panel reports this capture environment, not hardware GPU performance.

| Image | Version | Last generated (UTC) |
| --- | --- | --- |
| [solar-view.png](solar-view.png) | 0.0.1 | 2026-09-28T10:07:15+00:00 |
| [surface-view.png](surface-view.png) | 0.0.1 | 2026-09-28T10:07:16+00:00 |
| [planet-orbit.png](planet-orbit.png) | 0.0.1 | 2026-09-28T10:07:17+00:00 |
| [moonlit-night.png](moonlit-night.png) | 0.0.1 | 2026-09-28T10:07:18+00:00 |
| [moonless-night.png](moonless-night.png) | 0.0.1 | 2026-09-28T10:07:19+00:00 |
| [performance-overlay.png](performance-overlay.png) | 0.0.1 | 2026-09-28T10:07:31+00:00 |
| [atmosphere-day.png](atmosphere-day.png) | 0.0.1 | 2026-09-28T10:07:33+00:00 |
| [atmosphere-sunset.png](atmosphere-sunset.png) | 0.0.1 | 2026-09-28T10:07:36+00:00 |
| [atmosphere-mist.png](atmosphere-mist.png) | 0.0.1 | 2026-09-28T10:07:38+00:00 |
| [atmosphere-dust.png](atmosphere-dust.png) | 0.0.1 | 2026-09-28T10:07:41+00:00 |
| [twilight.png](twilight.png) | 0.0.1 | 2026-09-28T10:07:44+00:00 |
| [terrain-shadows.png](terrain-shadows.png) | 0.0.1 | 2026-09-28T10:07:45+00:00 |

Exact commands, image SHA-256 hashes and renderer details are in [generation.json](generation.json).
Available `.png.json` sidecars preserve resolved scenes and cameras.

The solar overview uses the frozen compact fixture; the surface and orbit gallery use the current working scene.
Night images preserve their airless lighting controls. Atmosphere, shadow and twilight images use regression fixtures.

To regenerate after building with `BUILD_TESTING=ON`:

```bash
cmake --build build --target PlanetSimulation terrain_shadow_render_tests
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a -s "-screen 0 1280x720x24" python3 scripts/generate_readme_images.py --build-dir build
```

Regenerate after changes to rendering, shaders or pictured scenarios; the version alone does not prove freshness.
Commit the images, sidecars and both generation records together.

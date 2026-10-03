# README image generation log

## Dense offline production start — 2026-10-03 (UTC)

The offline image (`offline-render.png`) is freshly rendered at native 1920×1080 using the production initial camera, atmosphere, water and lighting, with an explicit dense foliage study profile and stronger solar flare. Its replay is byte-identical. The other 21 PNGs and per-image provenance are unchanged. The complete gallery contains 22 images.

Offline source fingerprint: `86222331df9de34d31db648fcdc296f5cc7f781a526a10030e150a2dc19e59e8`. Per-image fingerprints and base revisions override gallery defaults in [generation.json](generation.json).

Build: GCC 12.2 RelWithDebInfo, Mesa llvmpipe/Xvfb, two rendering workers. The performance panel retains its original measurement.

| Image | Last generated (UTC) |
| --- | --- |
| [solar-view.png](../screenshots/solar-view.png) | 2026-10-02T16:59:08+00:00 |
| [surface-view.png](../screenshots/surface-view.png) | 2026-10-02T16:59:27+00:00 |
| [planet-orbit.png](../screenshots/planet-orbit.png) | 2026-10-02T16:59:33+00:00 |
| [moonlit-night.png](../screenshots/moonlit-night.png) | 2026-10-02T16:59:40+00:00 |
| [moonless-night.png](../screenshots/moonless-night.png) | 2026-10-02T16:59:46+00:00 |
| [refraction-extreme-on.png](../screenshots/refraction-extreme-on.png) | 2026-10-02T17:00:25+00:00 |
| [refraction-extreme-off.png](../screenshots/refraction-extreme-off.png) | 2026-10-02T17:00:48+00:00 |
| [terrain-detail.png](../screenshots/terrain-detail.png) | 2026-10-02T17:01:11+00:00 |
| [shoreline-detail.png](../screenshots/shoreline-detail.png) | 2026-10-02T17:01:22+00:00 |
| [grass-detail.png](../screenshots/grass-detail.png) | 2026-10-02T17:01:43+00:00 |
| [astronaut.png](../screenshots/astronaut.png) | 2026-10-02T19:50:10+00:00 |
| [astronaut-front.png](../screenshots/astronaut-front.png) | 2026-10-02T19:50:10+00:00 |
| [astronaut-jetpack.png](../screenshots/astronaut-jetpack.png) | 2026-10-03T08:21:41+00:00 |
| [astronaut-trail.png](../screenshots/astronaut-trail.png) | 2026-10-02T17:02:18+00:00 |
| [offline-render.png](../screenshots/offline-render.png) | 2026-10-03T11:09:26+00:00 |
| [performance-overlay.png](../screenshots/performance-overlay.png) | 2026-10-02T17:07:54+00:00 |
| [atmosphere-day.png](../screenshots/atmosphere-day.png) | 2026-10-02T17:08:16+00:00 |
| [atmosphere-sunset.png](../screenshots/atmosphere-sunset.png) | 2026-10-02T17:08:36+00:00 |
| [atmosphere-mist.png](../screenshots/atmosphere-mist.png) | 2026-10-02T17:08:57+00:00 |
| [atmosphere-dust.png](../screenshots/atmosphere-dust.png) | 2026-10-02T17:09:17+00:00 |
| [twilight.png](../screenshots/twilight.png) | 2026-10-02T17:09:35+00:00 |
| [terrain-shadows.png](../screenshots/terrain-shadows.png) | 2026-10-02T17:09:44+00:00 |

Commands, PNG hashes, source provenance and renderer details are in [generation.json](generation.json). Resolved [replay sidecars](replay/) retain scenes, cameras and complete character/effect state.

[Offline showcase study](../journal/benchmarks/offline-showcase.md) retains the dense/production, flare-off, exact replay and narrow-view allocation comparisons. Narrow views retain the same candidate queues and terrain meshes; the study records the prerequisites for useful column assembly.

Other images retain their earlier checkpoints: [exhaust](../journal/character/exhaust.md), [space-flight](../journal/character/space-flight.md) and [standing](../journal/character/standing.md).

For a full fresh generation, use `LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a python3 scripts/generate_readme_images.py --build-dir build-resume`. Commit images, sidecars and generation records together.

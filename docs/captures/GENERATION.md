# README image generation log

## Exhaust lifetime and wind — 2026-10-03 (UTC)

The jetpack view (`astronaut-jetpack.png`) was freshly rendered with persistent transparent exhaust and shared grass wind, then verified against its exact replay. The other 21 images retain their captures and provenance: two standing views from the standing checkpoint and 19 from the earlier gallery generation. The complete gallery contains 22 images.

New exhaust/wind source fingerprint: `b5c21157f38dc33e8c5d6fa949c99c5b936950f447427087965c00432a1ec6a8`. Per-image fingerprints and base revisions override the gallery defaults in [generation.json](generation.json).

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
| [offline-render.png](../screenshots/offline-render.png) | 2026-10-02T17:02:22+00:00 |
| [performance-overlay.png](../screenshots/performance-overlay.png) | 2026-10-02T17:07:54+00:00 |
| [atmosphere-day.png](../screenshots/atmosphere-day.png) | 2026-10-02T17:08:16+00:00 |
| [atmosphere-sunset.png](../screenshots/atmosphere-sunset.png) | 2026-10-02T17:08:36+00:00 |
| [atmosphere-mist.png](../screenshots/atmosphere-mist.png) | 2026-10-02T17:08:57+00:00 |
| [atmosphere-dust.png](../screenshots/atmosphere-dust.png) | 2026-10-02T17:09:17+00:00 |
| [twilight.png](../screenshots/twilight.png) | 2026-10-02T17:09:35+00:00 |
| [terrain-shadows.png](../screenshots/terrain-shadows.png) | 2026-10-02T17:09:44+00:00 |

Commands, PNG hashes, source provenance and renderer details are in [generation.json](generation.json). Resolved [replay sidecars](replay/) retain scenes, cameras and complete character/effect state.

[Exhaust study](../journal/character/exhaust.md) retains burn/release/retirement and downward plume views with CPU/GPU/replay validation. Earlier [space-flight](../journal/character/space-flight.md) and [standing](../journal/character/standing.md) studies keep their original checkpoint provenance.

For a full fresh generation, use `LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a python3 scripts/generate_readme_images.py --build-dir build-resume`. Commit images, sidecars and generation records together.

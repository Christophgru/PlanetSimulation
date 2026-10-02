# README image generation log

## Standing update — 2026-10-02 (UTC)

The two standing examples (`astronaut.png`, `astronaut-front.png`) were freshly posed and rendered. The other 20 images retain their previous captures and provenance; the complete gallery contains 22 images.

New standing source fingerprint: `d8218b326f4898f0bab98354ebad9aeea8afe36abff7b74424dccf50d4bd81e7`. Per-image source fingerprints and base revisions override the gallery defaults in [generation.json](generation.json). The default fingerprint still identifies the older capture generation.

Build: GCC 12.2 RelWithDebInfo, Mesa llvmpipe/Xvfb. The performance panel remains its original software-renderer measurement.

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
| [astronaut-jetpack.png](../screenshots/astronaut-jetpack.png) | 2026-10-02T17:02:06+00:00 |
| [astronaut-trail.png](../screenshots/astronaut-trail.png) | 2026-10-02T17:02:18+00:00 |
| [offline-render.png](../screenshots/offline-render.png) | 2026-10-02T17:02:22+00:00 |
| [performance-overlay.png](../screenshots/performance-overlay.png) | 2026-10-02T17:07:54+00:00 |
| [atmosphere-day.png](../screenshots/atmosphere-day.png) | 2026-10-02T17:08:16+00:00 |
| [atmosphere-sunset.png](../screenshots/atmosphere-sunset.png) | 2026-10-02T17:08:36+00:00 |
| [atmosphere-mist.png](../screenshots/atmosphere-mist.png) | 2026-10-02T17:08:57+00:00 |
| [atmosphere-dust.png](../screenshots/atmosphere-dust.png) | 2026-10-02T17:09:17+00:00 |
| [twilight.png](../screenshots/twilight.png) | 2026-10-02T17:09:35+00:00 |
| [terrain-shadows.png](../screenshots/terrain-shadows.png) | 2026-10-02T17:09:44+00:00 |

Commands, image SHA-256 hashes, source provenance and renderer details are retained in [generation.json](generation.json). Resolved [replay sidecars](replay/) retain scenes and cameras.

The new poses store their body offset; an old front-view sidecar also replayed byte-identically with the updated application. [Standing study](../journal/character/standing.md) retains the comparison and validation.

For a full fresh generation after building, use `LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a python3 scripts/generate_readme_images.py --build-dir build-resume`. Commit images, sidecars and both generation records together.

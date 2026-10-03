# Astronaut model search — 2026-10-02

The later [downloaded-candidate study](assets.md) prepares three public
creator downloads with walking deformation checks and local renders in
`USER_IO/astronaut_vis`. They are alternatives to this Sketchfab shortlist;
the earlier catalog counts below remain metadata from that search.

The recommended first candidate is [Astronaut character stylized rigged free
model by Muko_Art](https://sketchfab.com/3d-models/astronaut-character-stylized-rigged-free-model-c8daa753952e454eb3c6195446751e88).
Its preview has a slim grey suit, blue accents and a dark visor, close to the
requested Ava Turing silhouette. The creator labels it rigged; Sketchfab's
public model metadata reports 11,718 triangles, 6,062 vertices, one animation
and an available download. The asset is CC BY 4.0, rather than MIT.

| Candidate | Geometry | Animation evidence | Assessment |
| --- | ---: | --- | --- |
| [Muko_Art: stylized astronaut](https://sketchfab.com/3d-models/astronaut-character-stylized-rigged-free-model-c8daa753952e454eb3c6195446751e88) | 11,718 triangles | Rigged listing; one uploaded clip | Best initial balance of appearance and runtime cost; CC BY 4.0. Clip type and skeleton still need inspection. |
| [Antropik: astronaut](https://sketchfab.com/3d-models/astronaut-482bf87662fd4b378bcb3a2931d59ca3) | 19,584 triangles | Five uploaded clips reported by the public API | Useful candidate when existing animations matter; conventional NASA suit, 4K textures, CC BY 4.0. Clip names and skeletal rig remain unverified. |
| [Šimon Ustal: Low-poly Falling Astronaut](https://sketchfab.com/3d-models/low-poly-falling-astronaut-3december-df27a4d72cc74c5080bb95289f5778ca) | 9,030 triangles | Creator explicitly describes a rig and animation; one uploaded clip | Lightweight, angular alternative with a falling pose; CC BY 4.0. No evidence of walk/run clips. |
| [assetfactory: Astronaut rigged and animated](https://sketchfab.com/3d-models/astronaut-rigged-and-animated-f45039084a674080b42f5e171fd4f04c) | 12,656 triangles | Creator explicitly describes a full rig and idle clip | Conventional bulky suit; Free Standard license needs review before distributing its source assets. |
| [dark_igorek: To the Stars](https://sketchfab.com/3d-models/to-the-stars-astronaut-9c55bc97e009454cb768e3c9c84dbac4) | 124,946 triangles | Rigged-character tag; zero uploaded clips | More detailed female character; heavier and rig quality unverified; CC BY 4.0. |
| [Vu.: Astronaut Cartoon Rigged Animation Pack](https://sketchfab.com/3d-models/astronaut-cartoon-rigged-animation-pack-92123692b9af4a4385af5aa0fd81dc18) | About 16,800 triangles | Creator advertises 31 clips | Strong advertised animation coverage, but the listing offers no public model download. |

The supplied [Ava Turing reference](https://sketchfab.com/3d-models/ava-turing-the-turing-test-3dea4809201645a69e97214c048bd856)
is a visual reference only: its public metadata reports `isDownloadable: false`,
zero uploaded animations and no license grant. It is attributed to the game
*The Turing Test*. No model from this search has been installed in the renderer.

## Verification and acquisition

Sketchfab HTML pages returned HTTP 403 to the direct fetcher. Indexed creator
listings and the official public
[`/v3/models/{uid}` data API](https://sketchfab.com/developers/data-api/v3)
provided the comparisons above; official preview thumbnails were inspected.
Counts are catalog metadata, not measurements of downloaded files. A tag or a
single clip does not establish that a skeleton retargets well.
The selected public API fields and inspection timestamp are preserved in
[model-catalog.json](model-catalog.json).
An additional public catalog search filtered downloadable animated astronaut
models; Antropik's original game model stood out for its five clips and modest
geometry count. Reuploaded copies with unclear provenance and character packs
whose previews did not match the requested suit were excluded.

The official download endpoints for the first three candidates returned HTTP
401 without authentication. [Sketchfab's download documentation](https://sketchfab.com/developers/download-api)
requires an authenticated account and offers glTF/GLB assets. Obtain the model
through the normal download flow before assessing its skin weights, skeleton,
joint limits, neutral pose, materials and clip names. No authenticated download
or protected viewer-asset extraction was attempted.

For this project, inspect a downloaded glTF/GLB for hip, knee, ankle and foot
joints, usable elbow/shoulder chains, normalized weights and a neutral A/T pose.
Check retargeted idle, walk, sprint, jump and jetpack poses at 6/12 m/s; retain
planet-local stance locking and apply the existing ozz two-bone IK after the
base animation. Clips alone cannot prevent foot sliding on uneven terrain.
The current procedural renderer has no glTF skinning/material importer, so
replacing it requires that importer and retargeting work as well as the asset.

[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) permits sharing and
modification with attribution, the license link and an indication of changes.
Keep the model under its own asset license; the application's code license
does not replace it. A prospective credit for the recommended model is:

> Astronaut character stylized rigged free model by Muko_Art, provided through
> Sketchfab, licensed CC BY 4.0. Modified suit colors, arm patch and animation
> retargeting. Include the model and license links above.

This is a proposed credit for a future integration; no such asset modifications
have yet been made. No MIT-licensed download was found among these candidates.

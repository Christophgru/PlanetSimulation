# Detailed astronaut mesh studies — 2026-10-09

The rejected AstroDev and Polygonal Mind preview assets and their preparation
scripts were removed from the active tree. Their historical study is retained
in commit `a316d1a`; a local backup stays in ignored
`build-f5/astronaut-next/rejected-low-poly`. The runtime procedural astronaut
is still used while a suitable animated replacement is selected.

[The new comparison](../../../USER_IO/astronaut_vis/README.md) contains five
actual NASA mesh studies, each rendered in front, rear and close-up views with
Blender 4.5.4 LTS / Cycles, 64 samples and denoising on the RTX 3070 Ti. A shared
studio and 2 m display height make geometry and backpack shapes comparable.
Original downloads are pinned by repository revision, length and SHA256 and
stay local. No model Python or executable content is run. Published artifacts
are PNGs, compact receipts, provenance and regeneration scripts.

| Mesh | Imported triangles | Rig / clips | Selection finding |
|:--|--:|:--|:--|
| Z2 | 30,904 | None | Most relevant NASA prototype silhouette; original texture detail is retained, but the integrated rear shell needs planning for a custom jetpack. |
| EMU | 343,455 | None | Much denser suit geometry, but pack/hand attachments have visible roughness; count alone does not establish suitability. |
| Mark III | 69,202 | None | Articulated-looking suit structure; animation still requires an actual skeleton and weights. |
| Advanced Crew Escape Suit | 88,872 | None | Detailed cloth alternative; a launch/escape suit rather than a preferred planetary EVA design. |
| Gemini | 114,067 | None | Historical style comparison only; not a modern candidate. Source includes 396 repeated-index degenerate triangles discarded on import. |

These five files do not satisfy the complete request for modern high-detail,
well-animating astronauts. The existing TODO remains **p**, without additional
rows. Official previews and credits for dark_igorek's To the Stars, jgilhutton's
rigged multires EMU, Antropik's animated astronaut and soph's futuristic rig
are provided separately and explicitly labelled as listing images. Their
sign-in downloads are not acquired; advertised rigs and clips are not claimed
as tested. To the Stars is visually closest to the Ava direction; jgilhutton's
rig and sculpted multires mesh merit inspection, including its documented
finger/shoulder and multires compatibility issues. A paid BlenderKit option is
linked for comparison only, without purchase or a claimed license grant.

The first Debian Blender 3.4 preview pass exposed invalid specular calculations
and embedded-image loading failures. The final recipe imports original GLBs in
Blender 4.5.4. Some NASA exports also mark opaque cloth/hardware as fully
transmitting with IOR=1: that exact combination is reset to opaque in the
inspection scene, with affected materials listed in each receipt. Partial visor
transmission and original base colours/textures are retained. No geometry or
downloaded file is changed. NASA / Michael D. Carbajal and NASA / LaRC / Advanced
Concepts Lab credits, source pages and NASA media-use guidance are in the gallery.

Reproduction uses `scripts/character/fetch_realistic.py`,
`render_realistic.py`, `compare_realistic.py` and `check_realistic.py`.
The validator checks original file hashes, imported/source counts, zero skin
and clip claims, all 15 full-resolution PNGs, three contact sheets and published
artifact hashes; layout and whitespace checks also pass. Game source and
configuration are unchanged, so game build/tests are not repeated for this
asset inspection. Local rendering logs and downloaded Blender stay ignored.

Still required within the same astronaut item: acquire a preferred detailed rig,
verify licensing/embedded credits, inspect joint chains and weights, sample
walking/sprint/jump/flight and knee/shoulder/finger deformation, check animation
export and pack separation, and prepare grey/blue details with the German arm
flag before runtime integration. These are selection/integration requirements,
not separate invented TODOs.

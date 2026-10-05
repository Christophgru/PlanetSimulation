# T3c4c — native compute input and explicit opt-in

The public CLI now accepts `--terrain-backend compute` for the resident interactive
path. CPU stays the default. Interactive compute requires `gpu-v1` grass planning
and GPU placement for enabled foliage; captures retain legacy CPU grass planning.
Fresh unsupported GL requests fall back to CPU, while locked compute replays are
rejected. A legacy compute replay needs an explicit GPU-planner upgrade before
interactive use. No hardware speedup or physical VRAM reduction is claimed.

## Native loop and loading

The native test found a real loading-frame crash: the third-person mesh profiler
scope outlived `endFrame()`, so its destructor accessed the profiler's cleared
current frame. Loading presentation now follows scope destruction. Character
stepping and walking remain deferred until all initial consumers are ready.
Events and input callbacks continue during loading; orbital body motion can still
move world coordinates without changing the camera's body-local location.

Startup and reload snapshot validation share the resident planner rule. The
reload-specific factory remains a pipeline check without a startup bypass.
Enabled foliage lacking compute placement fails on a compute-capable context;
unavailable compute falls back to the CPU path before that requirement applies.

## Acceptance method

A private native executable uses the production CLI parser, renderer, GLFW window
and `Renderer::run()` interactive loop. X11 `xdotool` events select cameras and
hold actual keys. Test-only presentation interposition samples completed frames
and generation identities; GLEW hooks observe fence polls/server waits/readbacks,
and a `glFinish` hook checks explicit finishes. No product observer, test flags,
injected motion or substitute loop is added. The window is resized to 320 by 180
for software-renderer acceptance. Fault controls affect only this test executable.

The initial full run passed the other 62 groups but exposed a growing-file trace
read race during native space loading. A fragmented-write reproduction showed
transient zero bytes in reads while the final JSONL was valid. The reader now
commits only complete decoded rows, retries remaining bytes, requires consecutive
frame numbers and compares all observations with the finished trace after clean
shutdown. The final full run uses the corrected, frozen test inputs.

Five native processes cover:

- Delayed startup while camera 4 and W are pressed, then standing, paused walking
  at 6 m/s, Shift sprint at 12 m/s, trail accumulation and terrain/grass rebuilds.
- Jump, airborne W thrust without Space, Space thrust and emitted exhaust.
- Moving/drawing the old scene during delayed reload; R and file-watch reload,
  invalid config and GPU preparation recovery, supersession and body reorder,
  delayed/failed retirement with the latest complete scene still drawable.
- Saved outer-space and near-Moon starts, actual directional input and replay
  reload. Space keeps its inertial up and three gravity sources; Moon arrival
  binds destination contacts on the handoff frame and selects the Moon camera.
- CPU default and forced Mesa GL 3.3 fallback startup/walking. The shipping
  executable separately rejects locked compute on GL 3.3, interactive CPU grass
  planning, legacy interactive replay and enabled nonresident foliage.

Saved flight starts are derived from a public compute capture/replay, with only
initial pose coordinates changed. Native input drives subsequent motion. The
fixture has fixed spin, configured surface gravity and no atmosphere; it keeps water,
shadows and wind-driven grass. Atmosphere and exact capture replay remain covered
by the full regression. It is a correctness fixture rather than a hardware cost
benchmark.

Every sampled rendered compute frame must have matching land/grass/contact
keys and epochs, main/shadow/applicable reflection/water/grass revisions and
selected character contacts. Worker running/queued/ready counts stay at most one;
logical admission remains 512 MiB per set and 1 GiB aggregate. These exclude CPU
snapshots and driver physical memory. Generation-owned grass anchors follow the
normal rebuild policy while moving; captures retain exact saved anchors.

## Results

Five native processes and four shipping startup rejection contracts pass in
48.09 s of focused acceptance. All 63 CTest groups pass in one uninterrupted
frozen-input run (924.49 s); source, build/test inputs and eight executable
hashes match the final tree exactly. Median native walking/sprinting rates are
5.978/11.955 m/s on the configured triangle planes. All observed native
frames report zero positive-timeout/flush fence polls, server waits, bulk buffer
readbacks or `glFinish`. Native logical accounting peaks at 14,348,020 bytes,
excluding CPU snapshots and physical VRAM. Source checks retain separate explicit
capture wait adapters. Ten baseline compute/CPU/replay PNGs and 22 gallery PNG
hashes remain exact.

[Logs, complete compressed traces, selected frame landmarks and provenance](validation/evidence.json)
retain the acceptance. README and the rebuilt journal/PDF document the public
opt-in. The ordered [plan](../plan.md) now proceeds to T3c5 hardware frame-time and
memory acceptance; this environment exposes no GPU device and the default remains
CPU.

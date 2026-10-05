# T3c4a — asynchronous resident frame core

The resident frame path now prepares terrain through the persistent CPU worker,
submits complete GPU generations and publishes completed consumers at the frame
boundary. The public CLI remains capture-only until T3c4b asynchronous reload and
T3c4c native input acceptance pass. CPU remains the default.

## Ownership and frame behavior

`InteractiveTerrain.cpp` handles resident preparation independently of the explicit
capture adapter. Startup and local-mask changes enqueue CPU snapshots rather than
calling an exclusive build or capture fence wait. `readyIdentity()` inspects the
scheduler's single completed slot without consuming it; GPU staging or retirement
backpressure therefore prevents the next queued CPU build from starting. One
running, queued and ready CPU slot and one GPU preparation remain the bounds.

Each frame polls GPU readiness and old-resource retirement with flags and timeout
zero. Complete land/water/grass/contact publication refreshes renderer tracking,
rebinds selected ground contacts, invalidates shadows and clears frame reuse before
character updates and draws. Managed main/shadow/reflection/water/grass passes use
the same receipt validation as captures and skip independent grass preparation.
Grass-only publication keeps land, water and contacts while exchanging its patch
and planning receipt. Previewed chase anchors can lag subsequent movement within
the existing rebuild policy; capture's exact submission-time chase-eye rule is
not imposed on asynchronous frames.

Until every initial body's consumers are ready, the native frame loop defers
walking and character stepping, clears/presents the loading frame and continues
polling events. During replacement, the previous complete generation remains
usable. Incompatible scene/body/field/mode/serial completions are discarded. A
failed preparation retains live consumers and delays retry of unchanged input by
60 preparation frames; changed anchors/masks can request useful work immediately.
Failed retirement retains old ownership and admission, logs once until a successful
poll, and does not prevent drawing current consumers.

## Validation

Three renderer cases pass (20.148 s), and all eleven worker cases pass.
All 61 CTest groups pass in one uninterrupted frozen-input run (927.29 s),
including existing CPU native input, exact replay, GL 3.3 fallback and locked replay
rejection. Ten compute/CPU/replay PNG hashes match T3c3c3 exactly; all 22 gallery
hashes are retained. Source, build/test inputs and six binaries remain unchanged
through the full run. Three renderer tests use actual GL dispatches and
managed scene draws through the resident frame preparation branch, with a hidden
capture-created context switched to frame mode by a private test probe. This does
not enable the CLI or validate native input; those belong to T3c4c.

All three cases record zero positive-timeout/flush fence polls, server waits and
bulk buffer readbacks. Observed logical overlap is 8,408,696 bytes, below the
512 MiB per-set and 1 GiB aggregate admission limits. These exclude driver physical
VRAM and CPU scene/snapshot storage.

The cases cover initial all-body loading, programmatic 6/12 m/s walking across
rebuild thresholds, failed grass-only replacement, delayed GPU completion with a
bounded ready CPU slot, old-buffer retention/deletion across delayed retirement,
failed retirement polls, failed terrain fences, retry backoff and stale local-mask
rejection. Instrumentation observes fence polling and server waits and flags bulk
buffer readbacks; production source is audited for capture/worker waits and
`glFinish` on this path. Software GL validates correctness rather than hardware
frame cost or physical VRAM.

Raw [test logs, consumer traces, fingerprints and checks](validation/evidence.json)
retain the scope and reproduction commands.

See the [ordered continuation plan](plan.md): T3c4b implements asynchronous
complete scene reload, T3c4c validates native input before opening the CLI and
T3c5 retains hardware timing/memory acceptance.

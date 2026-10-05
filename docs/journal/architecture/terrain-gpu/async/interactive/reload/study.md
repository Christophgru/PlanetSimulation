# T3c4b — asynchronous complete scene reload

Resident interactive reload now prepares a complete replacement across frames while
motion and managed draws continue against the current scene. CPU remains the
default; startup and the public compute CLI stay capture-only until T3c4c native
input acceptance. The existing capture and CPU reload adapters are preserved.

## Request and worker ownership

A pending renderer owner retains an immutable config/replay/options snapshot,
future epoch, prepared scene and preallocated tracking. Every attempted reload
uses a new epoch, including invalid or abandoned attempts. A newer valid or invalid
request supersedes the previous one; failure leaves the live scene and epoch usable.
The reload-specific SceneSource factory validates snapshots without changing
render-test flags or opening the startup compute gate.

The persistent worker's nonblocking replacement lease drops obsolete queued and
ready work while an executing snapshot finishes normally and its result is
rejected. Abort restores the live epoch's use; commit advances it. A wrong commit
or an older abort cannot change a newer lease. There remains one running, queued
and ready CPU slot. Normal reload progress never invokes the exclusive capture
adapter or joins the worker.

Requests made during startup wait while initial preparation continues. A request
made during old-scene retirement retains only its latest snapshot. Failed last-use
polls retain both old ownership and the latest request. Once preparation can begin,
unfinished live GPU staging is cancelled and the scene-replacement lease freezes
live generation publication. Character/camera/orbit updates and draws continue
against the existing complete consumers.

## Per-body preparation and exchange

One CPU body is built and submitted into the existing off-live scene owner at a
time. GPU readiness and retirement use zero flags and timeout. Completed mesh and
sparse contacts remain off live; full CPU topology outputs are not collected for
all bodies. After all bodies are ready, a restored prospective character preview
uses those contacts to derive the replacement chase eye. Grass-only preparation
then installs final anchors before exchange; saved body-local replay anchors keep
their exact values. An exactly matching provisional anchor needs no second pass.
Otherwise, the extra grass pass remains part of hardware cost acceptance.

The shared commit boundary validates clip/speed/clock and creates the scene's
last-use fence before exchange. It swaps scene and tracking/options/replay, advances
the worker epoch, rebinds cameras/contacts, resets effects/input and invalidates
shadow/reflection/frame/orbit caches. It also clears frame staging and retry state.
The native loop resets transition/telemetry/elapsed clocks and reports success
only after commit. Old resources remain owned until a zero-timeout retirement
poll succeeds, with existing 512 MiB per-set and 1 GiB aggregate logical admission.
These are resource accounting limits, not physical VRAM estimates.

## Validation

Four actual GL renderer cases pass (26.511 s); all fourteen worker cases pass
(0.099 s), including three new controlled future-lease cases. All 62 CTest groups
pass in one uninterrupted frozen-input run (884.40 s). Source/build-test inputs
and seven binaries are unchanged through the run. Ten baseline compute/CPU/replay
PNG hashes and all 22 gallery hashes remain exact. Captured logical accounting
peaks at 16,300,856 bytes, excluding CPU snapshots and driver physical memory.
[Raw logs, consumer traces and fingerprints](validation/evidence.json) retain the
commands and acceptance scope.

Renderer cases cover old-scene 6 m/s movement and matching managed draws while
GPU completion is delayed, immutable snapshots despite subsequent disk edits,
body reorder/count/field changes, supersession while retirement is busy,
invalid latest config/replay, GPU preparation and final exchange fence failure,
same-field supersession, exact saved-replay recovery, requests during startup and
optional-camera/empty-scene recovery. Worker cases hold executing work behind a
controlled gate and verify obsolete executing/queued/ready ownership without
blocking replacement calls.

All four cases observe zero positive-timeout/flush fence polls, server waits or
bulk buffer readbacks. The tests use a hidden capture-created context switched to
frame mode through a private probe. They exercise real resident preparation and
managed draws; native GLFW input acceptance remains T3c4c. Software GL does not
establish hardware frame cost or physical VRAM use.

Resume the [native input/public opt-in checkpoint](../plan.md); hardware acceptance
remains T3c5. The [design notes](../reload-plan.md) record the ownership decisions.

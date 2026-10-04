# Complete generation transactions (T3c3a) — 2026-10-04

This checkpoint implements the transaction and retirement owner from the
[asynchronous plan](../plan.md), following [GPU preparation](../gpu/study.md).
The renderer has not been connected to this owner yet. Its capture terrain and
grass installation still occurs separately; frame-boundary contact/shadow/frame
integration is T3c3b and transactional scene reload is T3c3c. Interactive compute
remains gated and CPU remains the default.

## Ownership and transfer

`TerrainPublication` owns one pending preparation globally and a fixed body table.
Each body can hold one published receipt and one preparing or retiring spare.
A staged owner retains matching land/water buffers, sparse contacts, grass
metadata/allocation/draw resources and the publication receipt. Terrain and grass
planning eyes remain distinct: terrain uses the worker anchor, while grass can
use the chase-camera or saved replay anchor.

Every GPU submission and resource allocation precedes readiness. Once fences and
scalar summary validation complete, terrain handles and contacts move into private
meshes. The live meshes, grass patch and receipt remain unchanged. Disabled grass
is a ready empty patch carrying the terrain generation; disabled water is an
explicit empty mesh and clears previously enabled water at publication.

Publication checks epoch, body name/index, field/schema, backend, resident mode,
local mask and serial against current inputs. A useful completed anchor may still
publish after camera motion. It checks the original live mesh addresses, handles,
revisions and contact identities, checks the live grass key/revision/anchor, and
validates the prepared grass destination.
The caller must advance the scene epoch for any settings/configuration change.

A last-use fence is allocated before any live consumer changes. Then no-throw
swaps transfer land/water/contacts, grass and the complete receipt. Each mesh
revision advances exactly once. All previous consumers move together into the
retiring owner. Fence failure discards only the new resources. Preparation and
polling failures also release only staging resources; obsolete ready work cannot
replace the current generation.

The receipt exposes installed terrain/water keys, revisions, request identity,
anchors, foliage settings and face statistics. Resident grass exposes its actual generation and
revision, including the disabled state. T3c3b must use the receipt at a renderer
frame boundary, rebind contacts, invalidate frame caches and regenerate shadows
before dependent draws. The owner does not itself refresh renderer caches.

## Capacity and lifetime

Admission computes the complete land/water/grass reservation before GPU dispatch.
The default per-set ceiling is 512 MiB and the aggregate ceiling is 1 GiB. The
aggregate ledger includes published, pending and retiring sets. Per-body admission
stops while that body's old set is retiring, and a pending preparation prevents
another body from starting GPU work. Another body can prepare while the first
body retires, subject to aggregate admission.

Retirement polls `glClientWaitSync` with zero flags and timeout. No new spare for
that body is admitted until the last-use fence signals; old buffer textures,
terrain handles and contacts stay owned until then. A retirement poll failure
retains the old resources. Context shutdown can release owners normally through
OpenGL deletion semantics. Live meshes, grass and the current context must outlive
the transaction owner.

The reservation is conservative logical buffer payload, not measured physical
VRAM, free-memory telemetry or total process memory. It excludes shared shaders,
small bounded field caches, CPU snapshots and dynamic trails. The existing
available-memory/density policy and hardware acceptance remain later tasks.

## Validation

Native cases exercise atomic transfer, six injected fence failures (land, water,
metadata, allocation, draw resources, retirement), staged-buffer cleanup, delayed
preparation/retirement, stale identities, bounded body switches, overlap admission,
externally changed live destinations and enabled-to-disabled consumer replacement.
Tests issue actual old-generation draws before retirement and verify that both
old terrain buffers remain alive during delayed retirement and are deleted only
after completion. Loader probes are scoped and restored before real completion.

All **29 native compute cases pass (51.154 s)**, including eight new transaction
cases. All six fence failures retain the old mesh/grass/contact identities and
release every generated staging buffer. Ten stale identity variants are rejected;
useful older camera anchors can publish, and six repeated body replacements keep
the logical ledger bounded. All **59 CTest groups pass in one uninterrupted final-binary run (756.95 s)**,
including worker/lifecycle/reload, actor, flight, grass, atmosphere, native input
and adaptive quality. All ten compute/CPU/replay PNG hashes match T3c2 exactly,
and all 22 gallery hashes are retained.

This checkpoint does not claim renderer publication, transactional reload,
asynchronous interactive compute or hardware performance acceptance.

[Evidence](validation/evidence.json), [full regression](validation/tests.log),
[29 native cases](validation/native.log) and
[capture results](validation/capture-results.json) retain source/input/binary and
artifact provenance. README, journal/PDF, layout and local links are updated.
Resume at T3c3b; renderer cache/contact integration and scene reload transactions
remain pending.

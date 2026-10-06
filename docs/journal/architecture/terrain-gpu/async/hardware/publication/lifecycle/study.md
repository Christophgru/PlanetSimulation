# T3c5b2b2b — capture, synchronous reload, contact handoff and CPU grass

This checkpoint completes publication lifecycle coverage after ordinary requests
(T3c5b2b1) and asynchronous resident reloads (T3c5b2b2a). CPU terrain stays default;
physical-memory diagnostics and matched hardware performance acceptance remain
T3c5b2c and T3c5c.

## Capture and synchronous reload

Capture startup and replacements now admit terrain attempts, propagate their IDs
through the CPU adapter and GPU field/expansion/grass work, and arm matching live
generations. CPU, legacy compute with CPU grass planning and resident compute
with GPU planning share the first returned complete scene-draw endpoint. The
capture cache does not invent a new consumption event. The endpoint observes
terrain consumer submission before lens flare, overlay and PNG serialization;
it does not measure finished GPU execution or file saving. Capture exceptions
close still-unconsumed capture attempts in the live epoch as preparation failures.
Previously consumed receipts retain their successful result.

Synchronous reloads start a root before configuration loading and resource
retirement, with one child for each body's CPU/GPU/off-live grass preparation.
Invalid configuration and preparation/final-exchange failure close the family
without successful latency. Final live receipts arm only after the complete scene
exchange and contact binding, and the root waits for every child's matching draw.
A second exchange before the first draw leaves the previous root/children obsolete.
The synchronous protocol preserves its proposed `live_epoch + 1`: failed attempts
can reuse that epoch, while diagnostic attempt IDs remain unique. This differs
from the monotonically increasing attempted epochs of asynchronous resident
reload. Do not join failed synchronous transactions by epoch alone.

Synchronous reload can run between capture frames. A stack-owned GPU timing
binding preserves its child attempt identity even then, leaving the GPU work
CSV's frame column empty for off-live work. It restores any containing frame
binding afterward. Join these events by attempt, epoch, serial and generation,
not by the last capture frame. This enables the existing optional timestamp
scopes; their bounded query pool and nonblocking collection remain unchanged.

Replay capture can reprepare a body's geometry after scene exchange but before
its first draw. The committed child then ends as `replaced_before_draw`; after
the returned scene draw the root reports `trace_incomplete`, without successful
latency. Equivalent grass preparation leaves a committed child consumable.
These roots measure consumption of their original complete exchanged generation;
the replacement terrain receives its own successful receipt.

## Actual destination contact binding

A `handoff` receipt starts when the actual astronaut changes its reference body.
Preview/prospective calls admit no receipt. It records the origin and destination,
installed terrain serial and generation/revision keys. `contact_bound_ms` observes
completion of the live destination binding; success requires the complete draw
with matching destination geometry and selected character contact revision.
A surface/orbit draw without the character cannot close it. Handoff intervals
cover binding through consumption, not the flight's travel time. They coexist
with terrain requests, since they measure different events. Changing only the
grass anchor does not invalidate a contact binding; replacing its geometry or a
later binding to the same destination closes an unconsumed receipt as replaced.
Replay can require a new actual binding after each reload; those are real events,
while preview binding remains excluded.

## CPU grass-only admission

The CPU grass path starts an attempt only after its existing unchanged/disabled
checks admit a rebuild of the currently installed mesh. It starts before costly
planning and upload setup. Initial and replacement-mesh grass remains part of
the terrain request interval, rather than being mislabeled grass-only. A pure
grass rebuild retains terrain serial and geometry revisions, has an independent
attempt ID, CPU start/end offsets and its actual prepared anchor. Exceptions
close it as preparation failure. Success requires a draw using that anchor;
unchanged grass and cached presentation admit nothing.

The grass owner borrows stable renderer members (epoch, serial/mask vectors and
water meshes) only when tracing is enabled. It retains no topology or new worker.
Off-live reload grass owners are unbound and stay inside their root child scope.
Legacy GPU metadata/placement carries the originating pending terrain or pure
grass attempt and full generation keys. The off-live legacy reload grass scope
binds its body's epoch and serial explicitly; later placement retains those keys
even when no new grass plan is admitted. CPU terrain requests can precede the
new grass anchor, so they
match geometry/revisions without requiring the previous anchor; pure grass
requests match their actual anchor. Handoffs match bound geometry independently
of grass movement. Resident terrain still requires its exact committed anchor.

## CSV and ownership

The existing bounded 64-record pool is retained. The CSV appends `capture_mode`,
`origin_body` and `contact_bound_ms`; mode is captured at admission, even if later
frames use another mode. `handoff` is a distinct kind. Successful publication
latency is populated only after complete consumption; failure, obsolete,
replaced, shutdown and overflow paths never invent successful latency. Unknown
origin/binding offsets and unobserved phases remain empty. Root, child, grass and
handoff intervals overlap and must not be summed into a frame cost. No new GL
wait, readback, query or asynchronous owner is added by the publication recorder.
Disabled tracing adds no trace clock, record or file.

## Validation

One final frozen-input run passes ten relevant CTest groups (366.53 s),
including all 23 controlled timing cases, three capture backend cases, actual CPU
grass rebuild/failure, Moon recovery, synchronous/asynchronous reloads, worker and
compute cases, traced capture/replay, native-loop timing and existing native input
contracts. This is scoped validation, not a new full-suite result. All 41
application/test executable hashes and source/build-test fingerprints remain
frozen after validation; 22 gallery and ten historical baseline PNG hashes match.

Six actual Quadro OpenGL 4.3 checks pass (36.99 s summed case time). Software and
Quadro each retain nine capture reload roots and twelve children: three invalid,
three undrawn-obsolete and three published roots. Their capture work joins 57 GPU
events to attempts/generations, including 30 off-frame events; all are ready with
no dropped or missing samples. Each also checks one successful and one failed
CPU grass-only attempt, one actual Earth-to-Moon handoff, exact fresh Moon replay,
and seven injected reload fence failures followed by successful recovery. Moon
replay's two exchanged roots become trace-incomplete when capture replaces a
child before consumption; the replacement terrain succeeds independently.

Preflight and rejected checks are retained: between-frame legacy grass initially
lacked epoch/serial keys, and placement overwrote its stored generation. The final
scope binds the keys, retains full patch identity and checks GPU field/topology
through later draws. Review also corrected committed-child replacement closure.
These fixtures establish diagnostic correctness, not production frame cost.

Controlled recorder cases cover admission-scoped mode, CPU terrain spanning
new grass preparation, pure grass anchor matching, handoff/contact draw gating,
geometry replacement, stack-scope failure closure and capture exceptions.
A controlled GPU case checks off-frame identity and containing-frame restoration.
Renderer checks cover captures and undrawn/repeated synchronous reload on all
three backends, actual CPU grass-only rebuild and unchanged suppression, actual
Moon handoff versus preview, failure retention and exact replay. Raw validation,
renderer/device identity, frozen input/executable hashes and journal review are
retained in `validation/`. These correctness checks do not establish hardware
throughput or physical-memory cost.

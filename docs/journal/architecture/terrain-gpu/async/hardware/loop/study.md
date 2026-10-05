# T3c5b2a — complete native-loop wall receipts

Interactive `--performance-trace path.csv` now also writes
`path.csv.native-loop.csv`. The wall interval starts before `glfwPollEvents`
and ends after `FrameProfiler::endFrame` and the enclosing CPU trace scope.
It includes frame query collection, config watching/reload progress, input,
CPU preparation/submission, drawing, driver/presentation delays and the
minimized-window event wait. This closes the native wall-time part of the
[hardware plan](../plan.md).

T3c5b2 is split into three ownership scopes: this native-loop timer (T3c5b2a),
request-to-complete-publication latency/outcomes (T3c5b2b), and device-verified
physical-memory sampling (T3c5b2c). The latter two and matched hardware acceptance
T3c5c remain pending. CPU terrain is still default.

## Scope and columns

| Column | Meaning |
| --- | --- |
| `frame` | Candidate frame number shared with the frame and GPU work CSVs. |
| `wall_ms` | Full native-loop body wall time, including polling, profiler collection, loading presentation or minimized wait. |
| `event_poll_ms` | Wall interval around the event-poll call and its CPU trace scope; includes callbacks and any event processing delay. |
| `presentation_ms` | Wall interval around window buffer swaps, including the early loading path. |
| `event_wait_ms` | Wall interval around the existing minimized-window `glfwWaitEventsTimeout(0.05)` call. |
| `outcome` | `rendered`, `loading`, `minimized`, `exception`, or `unpresented`. |

The existing `frame_ms` and GPU markers retain their boundaries. Native loop
receipts are written in loop order; frame/work receipts can resolve later and
must be joined by frame number rather than row order. An exception before
`beginFrame` can produce a native exception receipt without a frame CSV row.
The ordinary completed-loop paths retain one native receipt per frame.

The phase intervals are contained in `wall_ms`; do not add them to it.
CPU wall, thread-CPU, GPU render-stage sums and GPU marker spans overlap and must
not be summed as a total cost. Presentation includes a VSync or compositor wait
when the driver imposes one. Minimized event waits are intentional idle time,
not a render-cost regression. Loading frames measure an incomplete scene and
must be kept separate from usable scene measurements.

The scope ends before writing its own CSV receipt. That buffered write, the
next while-condition check, startup before the loop, and final teardown lie
outside the measurement. GPU collection and other frame-CSV writes inside the
loop are included. Captures have their existing timer and do not create a
native-loop file; offscreen capture frame time is not native interactive wall
cost. Explicit trace file opening can fail before the native loop starts.

## Lifetime and validation

A stack-owned scope records every iteration, including early loading continues,
minimized waits and exception unwinding. It retains only one frame's scalar
measurements, adds no GL calls/query objects, worker, future or fence, and never
forces GPU completion. It uses `steady_clock`. Disabled tracing does not open
an output or read that clock. No product observer or injected controls are added.

Two deterministic-clock cases check full wall versus polling/submission/swap
phases, early return, minimized waits, exception closure and disabled tracing.
The native integration test runs the unchanged production GLFW loop on both CPU
and resident compute terrain. Its private executable interposes event polling
and swapping with known test delays, and zero-size framebuffer observations to
exercise minimized/resume behavior under Xvfb without a window manager. These
fault controls belong to the test executable only. Each finished native/frame
receipt is joined to an independent observation of the active frame number;
measured wall covers `frame_ms` and the contained phase intervals. The compute
case holds readiness to exercise loading and its early third-person continue.

All four relevant CTest groups pass in one final-input run (89.70 s):
GPU/work/frame/native timing (nine controlled cases, including the two new
clock cases), performance capture/replay, the new two-backend native loop
integration, and the existing five native input scenarios/four startup rejection
contracts. This is scoped validation; the preceding T3c5b1 64-group full run
remains historical evidence, and the complete 65-group suite was not rerun here.

The new software and Quadro OpenGL 4.3.0 NVIDIA 580.178.04 cases each join
64 receipts: CPU has eight rendered/seven minimized frames; compute has seven
rendered, 35 loading and seven minimized frames. Both audits observe zero
positive-timeout/flush fence polls, server waits, bulk buffer reads and explicit
finishes. Private test delays and small 10,000-triangle/128-candidate fixtures
make these scope/correctness checks, not frame-cost experiments. Actual context
renderer/version is retained; the device list is an access inventory, not UUID
verification for a future physical-memory sampler.

The first existing performance-capture preflight reached its old 60-second
CTest deadline. A standalone recheck passed. Its bounded suite allowance is now
120 seconds and the final CTest run passes with unchanged capture assertions.
The interrupted preflight and passing recheck are retained rather than discarded.
The source/build-input and all 41 executable fingerprints are checked after
validation; the 22 gallery hashes and prior ten baseline images are retained.
The journal PDF was rebuilt and affected pages reviewed.

Validation results and frozen provenance are retained in `validation/`.
These tests establish timer scope and bounded ownership, not production FPS or
CPU/compute speedup. Continue with T3c5b2b publication lifecycle receipts and
T3c5b2c physical-memory diagnostics before interpreting matched hardware costs.

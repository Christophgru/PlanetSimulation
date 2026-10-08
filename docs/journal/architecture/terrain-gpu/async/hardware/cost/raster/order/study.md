# T3c5c2b4 — private blade-order controls

Update, 2026-10-08: graphics access is restored. The bounded GPU runtime fix
and passing stationary production pairs are in [the F2 correction](correction.md).
The blocked attempts below remain historical evidence.

Declared on 2026-10-07 after `8efaec8`, before collecting the new controls.
CPU descriptors are distance-ordered within slot levels; resident descriptors
are triangle-ordered. The previous wind/raster study does not prove this causes
the full-draw regression. CPU stays default and all earlier failures remain.

A separate private probe will inspect both actual Earth main/reflection
indirect queues at fixed wind 12 s, retaining the production native camera,
plans, geometry, budgets, generation identities and shader inputs. Compare
native, ascending camera-depth and descending camera-depth permutations within
each detailed/quad queue. Native controls perform the same read/upload roundtrip
as permutations. Save complete 64-byte Blade records and indirect commands.
No blade is added, removed, modified or moved between queues. Sorting has a
stable original-index tie break. Camera depth uses the actual model/view matrix.

For each draw, query generated primitives, passing depth/stencil samples and
elapsed GPU time around only the forwarded draw. Read final framebuffer color
and depth after the complete frame. All query/read/sort/upload work is explicit,
blocking, and excluded from total-frame and migration acceptance. Passing
samples are not fragment shader invocation counts. Samples and diagnostic GPU
time describe order sensitivity, not independent overdraw or throughput proof.

Qualify semantics on a small software fixture first. Require exact Blade
multisets and command equality across permutations, exact depth equality, and
report every color discrepancy (including quantized-depth ties). Verify main
and reflection identity, shader time, unrelated draws, restored buffer bindings,
native generation/pose/anchor stability and all existing native audit fields.
Do not silently filter a differing image or root population. Then use the same
probe on production Quadro CPU/compute planning, at least three captured frames
per order/view. Keep raw captures and qualification/preflight failures.

If ordering changes cost while preserving contents/depth, use that controlled
evidence to design a bounded GPU correction with no runtime CPU queue readback.
Qualify numerical/contact/root/coverage/replay parity and repeat affected
full-workload native pairs before accepting cost. Diagnosis, correction and
verification remain one TODO outcome; this private control check alone cannot
complete T3c5c2b4.

Qualification refinement: the initial coarse software fixture passes exact
permutation/depth/color checks (50.23 s) but its quad queue is empty. Retain it
as a preflight and reduce only the fixture planet radius from 1 km to 100 m
to require nonempty detailed and quad main queues. Production inputs stay
unchanged. NVIDIA native context creation currently fails; direct opens of
both NVIDIA devices and control/modeset/UVM nodes return EPERM, while NVML
inventory remains accessible. Keep failed GLFW/GLX/EGL evidence separately;
do not replace production Quadro diagnosis with software cost acceptance.

## Qualified software controls and current hardware blocker

The refined final scoped CTest passes in **35.27 s**, retaining 18 snapshots:
CPU and resident planning, three frames each in native/near/far order, with
both geometry queues populated in main and reflection. The 100 m flat fixture
keeps 10,000 terrain triangles, a 10,000-blade cap and a 320×180 viewport.
Actual retained queue counts are:

| Planning backend | Main detailed | Main quad | Reflection detailed | Reflection quad |
| --- | ---: | ---: | ---: | ---: |
| cpu | 772 | 396 | 11 | 166 |
| compute | 772 | 396 | 11 | 166 |

Every captured permutation uses exactly the original 64-byte Blade multiset,
unchanged indirect commands and shader matrices, with exact opaque depth
before/after grass and **zero displayed RGB pixel differences**. Actual C++
float depth keys and source indices let the independent validator reconstruct
stable ascending/descending permutations exactly, including ties. Generated
primitive counts match 12 per detailed blade and two per quad. Passing-sample
and draw-query distributions remain in raw receipts; software elapsed-query
values are not physical-GPU or native total-cost acceptance.

All captured root/camera/generation/workload joins are retained. The native
camera and actual shader model/view/projection matrices remain exact. Repeated
contact evaluation changes a double root coordinate by at most
1.4210854715202004e-14 m (one ULP); float roots remain exact. An initial archive
assertion incorrectly required exact double roots, and its failure is retained.
The corrected validator compares exact draw inputs and reports this drift;
it does not relax the production native timing gate or claim an exact-double
motion acceptance result. Native worker/publication/wait/readback audits pass.
The probe's own blocking queue/pixel reads, query reads and roundtrip uploads
are separately counted and excluded. Every frame explicitly sets full-draw
timing eligibility and order timing/migration acceptance to false; captured
frames join their own inspection receipt exactly. The final CPU/compute traces retain all
57/1,253 native observations, including frames outside the 18 snapshots.

The initial coarse 1 km fixture passes in 50.23 s but has an empty quad queue.
Its complete output and original driver stay archived as a preflight. The
first refined direct run also passes with 18 snapshots. A further 18-snapshot
preflight predates the explicit per-frame exclusion flags; its complete output,
log, source variants and provenance stay separate. Final CTest uses fresh
output automatically when an earlier qualification directory is occupied;
production requests reject occupied outputs. Exact coarse source recovery
reproduces its recorded build/test tree fingerprint. The final source/input
and all 46 executable hashes are frozen; shipping/shaders and all 45 previous
executables and driver fixtures remain unchanged. No full-suite rerun or
runtime optimization is claimed.

The Quadro fixture fails before producing native frames: GLFW cannot create
a window. NVIDIA-offloaded `glxinfo` fails `X_GLXCreateNewContext` with
`BadValue`; EGL initialization fails with `EGL_NOT_INITIALIZED` (`0x3001`).
Direct read/write opens return `EPERM` for both GPU nodes, NVIDIA control,
modeset and UVM nodes. Missing DRM nodes were derived from sysfs and tested;
their opens also fail, and the temporary nodes were removed. NVML inventory
and telemetry still report both GPUs. This is graphics-device access failure,
not a passing production measurement. Failure logs and
`validation/device-access.json` retain the exact outcomes. Device access must
be restored outside these repository changes before native Quadro diagnosis.

## Archive and resume

Independent decoded checks reconstruct all **72 final/preflight snapshots**,
exact permutations, primitive/count/depth/color outcomes, main/reflection
matrices and native trace joins. The archive byte-verifies **1,675 payloads**:
110,730,278 original bytes become 32,018,882 stored bytes (71.1% saved).
All raw frames, images, commands/queries, full trace companions, earlier
fixtures, failures and exact helper sources remain. Blade arrays, depth keys
and permutations are grouped by queue to meet the repository folder limits;
compression changes storage only. The validator's path adapter decodes those
original bytes before invoking the retained qualification math.

```sh
python3 -B docs/journal/architecture/terrain-gpu/async/hardware/cost/raster/order/validate.py
```

The private software control prerequisite is tested. T3c5c2b4 remains open:
physical-GPU qualification and production diagnosis are blocked by
the device-access blocker. Once access is restored, run the unchanged private
probe first on the small Quadro fixture and then production CPU/compute input,
using new output folders and the verified Quadro UUID:

```sh
__NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia \
  PYTHONDONTWRITEBYTECODE=1 xvfb-run -a python3 -B \
  tests/app/terrain/native/raster/order/test_order.py \
  --probe build-resume/tests/terrain_order_probe \
  --output-dir build-resume/order-quadro-fixture-next \
  --expected-uuid GPU-2cefee61-6b3b-a670-c390-c7449dad3f79
```

For the subsequent production run, add `--production` and choose another
fresh directory. Preserve every discrepancy and separately verify actual
queue populations, matrices and draw-order sensitivity before attributing
cost to ordering. Only then select a bounded GPU correction and run
full-workload repeats. The previous stationary failures and 1.05 gate
remain; CPU stays default. No production-GPU diagnosis, cause, correction,
overdraw count, timing acceptance or migration acceptance follows from these
software qualification results.

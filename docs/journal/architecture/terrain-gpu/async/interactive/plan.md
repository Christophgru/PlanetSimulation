# T3c4 — interactive compute acceptance

CPU remains the default. Keep the CLI capture gate until the following ordered
checkpoints pass; completing the frame core alone does not enable interactive
compute for users.

1. **T3c4a — asynchronous frame core.** Route resident interactive preparation
   through the persistent CPU worker and complete GPU transaction. Never invoke
   capture waits, even for initial bodies or local-mask changes. Leave completions
   in the bounded worker slot while GPU capacity/retirement is busy. Poll readiness
   and retirement with zero timeout, reject incompatible results, publish before
   contact/draw use, and retain old consumers on failure. Bound retries. Defer
   grounded movement and scene draws during initial complete-body loading. Validate
   with real GL dispatch/draws and controlled delayed/failed fences; CLI stays gated.
2. **T3c4b — asynchronous complete scene reload.** Follow the
   [reload design notes](reload-plan.md). Stage replacement config/replay,
   CPU body requests and GPU consumers across frames while the current scene stays
   usable. Poll rather than execute exclusive capture adapters. Complete scene
   exchange, camera/contact/input/tracking rebinding and fenced retirement retain
   T3c3 invariants. Supersede pending reloads without worker joins; failed config,
   allocation and stale work leave the live scene usable.
3. **T3c4c — native input and public opt-in.** Exercise actual standing, walking,
   sprinting, jump/flight/trails, body switching and valid/invalid/repeated reload
   under native GLFW input. Record matching consumer identities and zero normal
   frame waits, including startup and reload progress. Retain CPU default, GL 3.3
   fallback and locked replay rejection. Then accept explicit interactive compute
   using resident GPU grass; legacy capture planners remain supported.

Grass planning anchors are generation-owned and may lag a moving camera within
the normal rebuild policy. Submission previews use prospective contacts; publication
rebinds the committed contacts before the current character step. Interactive
movement does not require capture's exact submission-time chase-eye equality.
Hardware frame percentiles and physical memory remain T3c5; llvmpipe validates
correctness only.

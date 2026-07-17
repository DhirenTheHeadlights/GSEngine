# Render-graph cross-queue batching — plan

Fix the cross-queue synchronization model in the render graph so a
graphics → compute → graphics pass chain (the RT shadow AS build) is
schedulable without deadlock or races, on both DX12 and Vulkan.

## Problem

Spawning an object adds a BLAS build (`rt_shadow: BLAS build verts=63`) to the
compute queue. The next frame, every queue head-blocks: DRED shows 45 command
lists all at `completed 0/N`, hung at `BEGIN_COMMAND_LIST` (10 draw/clear,
1 RTAS, 8 dispatch-only, 26 barrier/query-only — 2-3 frames of pile-up on both
queues). Watchdog fires in `engine::render`, TDR removes the device
(`DXGI_ERROR_DEVICE_HUNG`), and the app crash-cascades (`create_buffer FAILED
removed=0x887a0006` → SEH). This is the long-standing "RT cross-queue AS hang";
it predates DX12 and reproduces there now that cross-queue waits are real.

## Root cause — proven from code

The pass graph itself is sound: full per-pass DAG (explicit `.after`, chains,
read/write overlap), topo-sorted, with cycle diagnostics
(`RenderGraph.cpp:561-763`). The defects are all in how that DAG is lowered to
queue submissions (`RenderGraph.cpp:784-1242`):

1. **Frame-coarse sync.** One submission per queue per frame;
   cross-queue dependencies collapse into a queue×queue boolean matrix
   (`queue_waits_on`, line 784). A ping-pong chain
   (graphics geometry → compute AS build → graphics forward sampling the TLAS)
   is inexpressible at that granularity: the matrix goes mutual.
2. **Assert-only defense, compiled out.** The only guard is
   `assert(!(queue_waits_on[a][b] && queue_waits_on[b][a]))` (line 830) whose
   message already names the fix ("split a submission"). RelWithDebInfo strips
   it, so the broken topology ships instead of failing loudly.
3. **compute→graphics waits silently dropped.** `this_frame_signal_values` is
   only populated for non-graphics queues (loop at 1183 skips graphics;
   graphics never signals a graph timeline). The `> 0` guard at 1212 then
   drops every compute→graphics wait. Today the AS build (compute) reads
   graphics-produced geometry with **no synchronization at all** — a race that
   ships every frame the mutual matrix occurs.
4. **Producer-order fragility.** Aux submissions are built in queue-index
   order and read `this_frame_signal_values` of other queues mid-construction;
   a wait on a queue built later in the loop sees 0 and is dropped.
5. **Zero observability.** No queue-level Wait/Signal is logged anywhere
   (`Dx12/Queue.cppm:40-79`), D3D12 objects are unnamed so DRED prints
   `queue='?' list='?'`, and the device-removed dump has no
   fence completed-vs-waited values. This bug class costs a full session per
   occurrence purely for lack of a dump.

### RESOLVED 2026-07-16 by the Phase 0 ring — the spawn crash is NOT a sync deadlock

First instrumented repro (log 16:16): the queue-op ring shows every wait
satisfiable. Steady state per frame: compute {wait own timeline V, execute 67
lists, signal V+1} then graphics {wait image_available, wait compute timeline
V+1, wait transient timeline (stuck value 32 = long-satisfied), execute 42
lists, signal render_finished + in-flight}. On the spawn frame graphics
executes **46** lists (the BLAS/TLAS work joins the GRAPHICS queue — the
rt_shadow pass never sets `.on(compute)`; compute stays at 67), then
`swapchain::present`'s CPU wait on render_finished blocked ~12 s → TDR
`DEVICE_HUNG` → all fences read `UINT64_MAX` → the queued frame-begin waits
fell through → next frame's `create_buffer` failed on the removed device →
SEH cascade. No cross-queue edge exists on the spawn path at all (the
cycle/dropped-wait warnings correctly stayed silent), so the device hung
**executing** the spawn-frame graphics submission.

Prime suspect: `BuildRaytracingAccelerationStructure` consuming instance data
in the wrong state — validation flags the exact list
(`'gse.worker graphics f0 #7'`: instance buffer in `UNORDERED_ACCESS`,
build requires `NON_PIXEL_SHADER_RESOURCE`). The missing transition also
means no flush/visibility for the compute-written instance descs, so the
first real instance (spawn) can hand the build bogus BLAS addresses.
The DRED walker's old 128-node cap could hide the partially-complete node
("all completed" was unreliable) — cap raised to 65536 with walked/incomplete
counts printed.

Consequence for this plan: Phases 1-2 remain justified by the proven graph
defects (frame-coarse matrix, dropped edges — latent, and live for any pass
that does use `.on(compute)`), but they are NOT the spawn-crash fix. The
immediate fix is the AS-input state/visibility transition in the DX12
backend (instance + BLAS geometry buffers → `NON_PIXEL_SHADER_RESOURCE`
before build), which was previously listed as a non-goal.

### Superseded framing — the exact first-blocked wait (settled by Phase 0)

The emitted wait topology (graphics→compute only, compute self-serialized)
cannot literally cycle by itself, yet both queues sat at 0/N for >2s before
TDR. One more mechanism is involved. Ranked candidates:

- transient-upload timeline: graphics waits `pending_value`
  (`Context.cpp:89-95`) while the upload work is recorded *inside* the same
  graphics submission (`run_post_frame` → `graphics_end`) — a same-queue
  wait-before-signal self-deadlock on the frame that uploads the spawned mesh;
- stale aux/graphics interleave on an early-out frame
  (`m_pending_graphics_buffers` is overwritten, extra waits accumulate);
- binary-semaphore emulation edge in the new VRAM-buffer upload path.

Phase 0 instrumentation converts the next repro into a definitive answer.
Phase 1 removes the entire class regardless of which candidate fired first.

## Fix

### Phase 0 — diagnostics (small, permanent) — IMPLEMENTED 2026-07-16, unbuilt

- `SetName` on both queues, the per-frame primary lists, and every
  worker/transient list at creation (`gse.worker graphics f1 #3` style) so the
  DRED dump's `queue='?' list='?'` fields populate. Pass-tag naming needs a
  device-seam op — folded into Phase 1 instead.
- Queue-op ring (256 entries, `Dx12/Device.cppm`): every queue
  Wait/Signal/ExecuteCommandLists plus every CPU-side fence signal and
  blocking wait (`swapchain::present`'s wait, `wait_for_fence`, `wait_idle`)
  with fence pointer + value, recorded before the call so a hang's in-flight
  op is the newest entry. Dumped by `dump_dred_once` alongside a sync-point
  registry (every created fence/semaphore: timeline flag, CPU-side expected
  value, `GetCompletedValue`, `<< LAGGING` marker) — fence naming is
  unnecessary given the registry. The registry is kept outside `m_mutex`
  (device-removed dumps fire from paths that hold it).
- The mutual-wait assert is now an always-on once-per-pair `log::error`
  naming both queues and this plan.
- Added beyond plan: once-per-pair `log::error` when a cross-queue wait is
  DROPPED at emission (producer has work but no timeline value — defect 3/4),
  so the silent under-sync is loud in every build.
- Found while implementing, for the open-question ranking:
  `dx12::swapchain::present` CPU-blocks on the `render_finished` fence before
  `Present` (`Swapchain.cppm`) — the likely main-thread block point in the
  watchdog stack, and a full CPU⇄GPU serialization per frame to revisit
  after Phase 1.

### Phase 1 — per-batch topological scheduling (backend-agnostic)

Replace the queue×queue matrix with batch-granular scheduling derived from the
existing topo order (`sorted`):

- **Batching rule.** Walk `sorted`; a batch is a maximal run of passes on the
  same `effective_queue`. Cut the consumer queue's open batch whenever a pass
  has a cross-queue dependency edge to a batch not already covered by the open
  batch's waits.
- **Emission.** Batches are emitted in global topo order. Each batch carries:
  its queue, its command lists (per-pass transition lists stay glued to their
  pass), waits = exact `(producer timeline, value)` pairs for producer batches,
  signal = own queue timeline at `++signal_counter`. The batch DAG refines the
  acyclic pass DAG and every wait references an earlier-emitted batch, so the
  wait graph is acyclic **by construction** — the assert and
  `this_frame_signal_values` are deleted, and defects 1-4 disappear.
- **Graphics timeline.** Intermediate graphics batches signal
  `m_queue_states[graphics].timeline` (exists today, never signaled), making
  compute→graphics waits expressible. The **final** graphics batch remains the
  presenter submission exactly as today: submitted by `frame::end` with
  `image_available` / transient waits prepended, `render_finished` +
  in-flight-fence signaled.
- **`frame::end` seam.** Accept the ordered batch list; relax the
  "no graphics in aux submissions" assert (`Frame.cpp:241`) to "only non-final
  graphics batches"; keep the existing last-batch-per-queue in-flight fence
  logic (`last_for_queue`).
- **Profiling.** Profile-begin list rides the first batch of its queue,
  end/stat-resolve the last.
- Wait stages stay `all_commands` initially — correctness first, stage
  narrowing later.

### Phase 2 — transient uploads for non-graphics consumers

`Context.cpp` adds transient-upload waits only to graphics. The first batch of
every queue that consumes transient-uploaded resources must wait that queue's
transient pending value too — this closes the spawn-frame race where the
compute AS build reads a mesh that is still uploading. (Modeling uploads as
graph passes on a transfer queue is future work, not this plan.)

### Phase 3 — verification

- DX12: spawn repro ×N with no hang, DRED silent, RT shadows visually correct,
  watchdog quiet.
- Vulkan: same scene, no regression — the scheduling change is in shared graph
  code driving VK timeline semaphores.
- Builds/tests only with explicit go-ahead (standing rule).

## Non-goals

- The meshes-not-rendering issue from the previous run (separate open item).
- The AS instance-data resource-state warnings
  (`UNORDERED_ACCESS` vs `NON_PIXEL_SHADER_RESOURCE`, log lines 29199/29207) —
  pre-existing barrier/state bug, belongs with the bindless-barriers plan.
- Per-pass submission granularity, fence pooling, moving BLAS builds off the
  graph, async-compute performance tuning.

## Risks

- The presenter seam (final graphics batch vs `frame::end`) is the
  highest-touch part; regressions show as present/pacing bugs.
- Pathological interleaves increase batch count → more
  `ExecuteCommandLists`/`vkQueueSubmit` calls; acceptable, log when a frame
  exceeds a threshold.
- DX12 cross-queue resource-state ownership (compute queues cannot hold
  graphics-only states): existing transition logic must be re-validated under
  the new interleaving.
- Profile-slot placement across split batches.

## Touch list

| File | Change |
| --- | --- |
| `Gpu/Graph/RenderGraph.cpp` | rewrite lowering region (~784-1242): batching, emission; delete matrix/assert |
| `Gpu/Graph/RenderGraph.cppm` | pending-batch storage replacing aux/pending-graphics triple |
| `Gpu/Device/Frame.cpp/.cppm` | `end()` takes ordered batches; final-graphics seam; fence logic |
| `Gpu/Context.cpp` | per-queue transient waits (Phase 2) |
| `Dx12/Device.cpp`, `Dx12/Queue.cppm` | Phase 0 naming, op ring, removed-dump fence values |
| `Vulkan/*` | no change expected; re-test |

Estimate: Phase 0 ~1 h; Phase 1 is the bulk (~1-2 sessions); Phase 2 ~1 h;
Phase 3 is the repro loop.

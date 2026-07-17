# DX12 backend — status, investigation notes, and machine handoff

Written 2026-07-16 on branch `dx12-backend` (last commit at time of writing:
`a451a5b9`). Purpose: everything needed to continue this effort from a fresh
machine — current state, what the spawn-crash investigation proved, how to
read the new diagnostics, and setup gotchas that do not live in git.

Companion docs:

- `docs/render_graph_cross_queue_batching_plan.md` — the render-graph
  cross-queue defects (proven, currently latent) and the phased fix plan;
  Phase 0 (diagnostics) is implemented and verified, Phases 1+ not started.
- `docs/render_graph_bindless_barriers_plan.md` — bindless barrier plan.

## Current state

Working on DX12: adapter selection, VRAM buffers, bindless
(resource/sampler heaps), graphics+compute queues with real timeline-fence
sync, swapchain, timestamps, VBD compute on the compute queue, meshlet
pipeline, RT shadow BLAS/TLAS path up to the build itself. The steady-state
two-queue frame loop is verified healthy by the queue-op ring (see below).

Two open problems, in priority order:

1. **Spawning an object hangs the GPU** (TDR `DEVICE_HUNG` -> device
   removed -> SEH cascade). Root-caused far enough to name a prime suspect
   and a fix; see next section. THIS IS THE CURRENT WORK ITEM.
2. **Mesh rendering wrongness** observed in an earlier run (before the
   crash cut evaluation short) — suspected OIT alpha / `SV_Target`
   semantics on the weighted-blended path. Unevaluated; revisit after the
   spawn hang.

## The spawn-crash investigation — arc and verdict

1. First DX12 hang: cross-queue waits were fake (each `sync_point` waited a
   self-incremented counter — always already signaled). Fixed with real
   per-queue timeline values derived in the render graph and correct
   binary-semaphore emulation in `dx12::queue::submit`. The fix held: no
   more value-0 waits, engine runs indefinitely until spawn.
2. Spawn then hung both queues. Initial theory: the render graph's
   frame-coarse cross-queue sync (mutual `queue_waits_on` matrix, silently
   dropped compute->graphics waits) closes a wait cycle once the spawned
   object adds RT AS work. Those graph defects are REAL (see the batching
   plan doc) but turned out latent here.
3. Phase 0 diagnostics landed (commit `a451a5b9`): named queues and all
   command lists, a 256-entry queue-op ring, a sync-point registry dumped
   on device removal, DRED walker fixes.
4. The first instrumented repro overturned the deadlock theory:
   - Steady state per frame: compute {wait own timeline V, execute 67
     lists, signal V+1, signal in-flight}; graphics {wait image_available,
     wait compute timeline V+1, wait transient timeline (long-satisfied),
     execute 42 lists, signal render_finished + in-flight}; then CPU waits
     (present, frame-begin) all satisfied.
   - Spawn frame: graphics executes **46** lists (the +4 = BLAS/TLAS build
     and transitions — the AS work joins the **graphics** queue; the
     rt_shadow pass never sets `.on(compute)`, only VBD uses compute).
     Compute stays at 67. **No cross-queue edge exists on the spawn path**;
     the new cycle/dropped-wait warnings correctly stayed silent.
   - `swapchain::present`'s CPU wait on `render_finished` then blocked
     ~12 s (watchdog dumps) -> TDR removed the device -> every fence reads
     `UINT64_MAX` -> the queued frame-begin waits fell through -> next
     frame's `create_buffer` failed on the removed device -> SEH.
   - Every wait in the ring history was satisfiable => the device hung
     **executing** the spawn-frame graphics submission, not waiting.
5. **Prime suspect**: `BuildRaytracingAccelerationStructure` consuming
   instance data in the wrong state. The debug layer names the exact list
   (`'gse.worker graphics f0 #7'`): instance buffer in
   `UNORDERED_ACCESS`, build requires `NON_PIXEL_SHADER_RESOURCE`. The
   missing transition also means no flush/visibility for the
   compute-written instance descriptors, so the first REAL instance
   (spawn) can hand the build stale/garbage BLAS GPU VAs — a classic
   execution hang. Pre-spawn builds have nothing to dereference, which is
   why only spawn dies. The same warning class fires for BLAS
   vertex/index inputs.

## Next actions (ordered)

1. **AS-input state/visibility fix** in
   `dx12::commands::build_acceleration_structures`
   (`Engine/Engine/Source/Dx12/Commands.cppm`): transition the instance
   buffer and BLAS vertex/index buffers to
   `NON_PIXEL_SHADER_RESOURCE` before the build (resolve resources from
   GPU addresses via the device's buffer-by-address map + tracked states),
   and back/UAV-barrier as appropriate after. Then re-run the spawn repro.
2. If it still hangs: the DRED walker now walks up to 65536 nodes and
   prints walked/incomplete counts (the old 128-node cap could hide the
   hung node — the "all breadcrumb nodes completed" line in the last log
   is NOT trustworthy). The hung list will be named.
3. Remove `swapchain::present`'s CPU `wait_fence` on `render_finished`
   (`Engine/Engine/Source/Dx12/Swapchain.cppm`) — it fully serializes CPU
   against GPU every frame. Bring-up crutch; replace with proper
   frames-in-flight pacing once stable.
4. Phases 1-2 of the cross-queue batching plan (per-batch scheduling in
   the render graph). Justified by the proven-but-latent defects and by
   the Vulkan "RT cross-queue AS hang" note; not urgent for DX12 spawn.
5. Mesh-rendering wrongness (OIT alpha lead).

## Reading the diagnostics (added in Phase 0)

- **Queue-op ring**: `dump_dred_once` prints the last 256 queue ops
  (`[seq] <queue> <kind> fence=<ptr> value=<v> lists=<n>`). Ops are
  recorded BEFORE the API call, so on a hang the newest entry is the
  in-flight op. Kinds: `wait`/`signal`/`execute` (queue-side),
  `cpu_signal`/`cpu_wait` (CPU-side: swapchain acquire signal, present
  wait, frame-begin `wait_for_fence`, `wait_idle`).
- **Sync-point registry**: every created fence/semaphore with timeline
  flag, CPU-side expected value, and `GetCompletedValue`, with a
  `<< LAGGING` marker when completed < expected.
- **Post-TDR caveat**: after device removal `GetCompletedValue` returns
  `UINT64_MAX` for everything, so LAGGING markers are meaningless in a
  post-removal dump — the ring HISTORY is the reliable signal. Any
  CPU wait recorded after the stall "returned" only because the removed
  device satisfies all waits.
- **DRED**: breadcrumb nodes now carry real queue/list names
  (`gse.graphics_queue`, `gse.worker compute f1 #3`,
  `gse.frame_primary graphics f0`, `gse.transient graphics p2 #0`).
- Useful fence identities from the 2026-07-16 log (values will differ per
  run but the SHAPE repeats): compute graph timeline waited/signaled by
  compute each frame and waited by graphics; three per-image
  render_finished fences signaled by graphics then CPU-waited by present;
  per-slot image_available fences CPU-signaled at acquire; per-queue
  in-flight fences signaled at submit end and CPU-waited at frame begin.

## New-machine setup (things git does NOT carry)

1. **Slang port pin (CRITICAL for correct rendering)**: this branch needs
   shader-slang **2026.12** (for `DescriptorHandle` bindless /
   `spvDescriptorHeapEXT`), but the pin lives in the vcpkg submodule's
   WORKING TREE only (`ports/shader-slang/{portfile.cmake,vcpkg.json}` —
   uncommitted, submodule checks out 2025.14.3 by default). A fresh clone
   silently builds the old Slang: shaders compile but
   `__slang_resource_heap` never maps (`findCapability=0`) and everything
   renders garbage. The pinned files are carried in this repo at
   `Engine/External/pinned-ports/shader-slang/`. On a new machine, after
   submodule init:

       cp Engine/External/pinned-ports/shader-slang/* \
          Engine/External/vcpkg/ports/shader-slang/
       # then reconfigure so vcpkg rebuilds shader-slang 2026.12

2. **Toolchain**: builds use a custom gcc-trunk (C++26 modules +
   reflection) at `~/.gcc-trunk/current`, built with mcfgthread. Shell
   PATH must include `~/.gcc-trunk/current/bin` BEFORE building (otherwise
   every module .ddi scan fails silently — cc1plus can't load) and before
   RUNNING the binary (needs the gcc runtime DLLs; bare exe exits 127).
3. **Slang session gotcha** (already in code, do not "optimize"): one
   Slang ISession per compile is load-bearing — caching a session across
   compiles crashes.
4. `settings.ini` overrides code defaults (backend selection etc.).
5. Repro for the current bug: run with the DX12 backend, spawn an object
   (dev spawn binding). Crash log lands in `Engine/Resources/Misc/log.txt`
   (gitignored).

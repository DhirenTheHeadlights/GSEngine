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

Two open problems, in priority order (re-prioritized 2026-07-22 per user):

1. **Orientation + textures.**
   - **Orientation — NOT A BUG; my 2026-07-22 "fix" was the regression,
     REVERTED 2026-07-23.** The original DX12 viewport used negative
     height (`cmd_set_viewport`: `y+h`, `-h`) — the standard idiom to
     match Vulkan's clip convention on D3D12 — and rendered right-side
     up. I misread a BLANK screen (which was actually the F5 crash / TDR
     presenting nothing) as a viewport-clipping bug and flipped the
     viewport to positive height; THAT is what turned the image upside
     down. I then compounded it with a projection m11 flip. Both changes
     are now fully reverted: `cmd_set_viewport` is back to negative
     height, `CameraSystem.cpp` is byte-for-byte original, and the
     `ndc_y_direction` scaffolding was removed. LESSON: negative viewport
     height DOES rasterize fine here; a blank screen on this branch is
     the crash, not the viewport — do not touch viewport/projection for
     orientation. (`gpu_backend_kind` -> `backend_kind` rename was kept —
     independent cleanup, redundant `gpu_` prefix inside ns `gse::gpu`.)
   - **No textures — under investigation, UNRESOLVED.** Ruled out at code
     level: upload path (`host_upload_image_layers` -> `upload_texture`
     is correct, ends COMMON, promotes on sample); SRV format
     (`srv_format_of` returns the typed format for color, only depth goes
     typeless->float); heap mechanism + root signature (BOTH
     `CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED` and
     `SAMPLER_HEAP_DIRECTLY_INDEXED` set — and bindless BUFFERS work
     since meshlet geometry renders, proving `ResourceDescriptorHeap[]`
     access + index math are sound); descriptor index math (pools'
     base_index/base_offset internally consistent between SRV write and
     the returned slot). Remaining suspects, need a build + frame capture
     (RenderDoc/PIX) to narrow: (a) sampler descriptor content /
     `SamplerDescriptorHeap[]` indexing — the one thing textures use that
     buffers don't; (b) material->texture-index propagation and whether
     assets go through `register_texture` (combined, index range
     [0,1024)) vs `create_image`+`write_sampled_image` (image pool,
     [1024,...)) and whether the shader reads a combined or separate
     texture+sampler handle.
2. **GPU hangs on a compute DISPATCH** (TDR `DEVICE_HUNG` -> device
   removed -> SEH cascade). 2026-07-24 log (DRED W-name fix now working)
   names it: the ONE partially-executed list is
   `queue='gse.compute_queue' list='gse.worker compute f1 #19'`, ops
   `begin / beginevent / resolvequery / resourcebarrier` done then
   **op[4] DISPATCH HUNG** (a single profiled dispatch: timestamp
   RESOLVEQUERYDATA + PIX BEGIN/ENDEVENT bracket it). 253 of 620
   breadcrumb nodes incomplete = all queued behind this one op. DRED
   page-fault output is empty (`PageFaultVA==0`) -> NOT a bad pointer /
   freed resource; the shader is **looping on data**. Context: VBD
   physics is live (a `leg_controller ... fallen=true` humanoid) and RT
   shadow BLAS/TLAS is active; the hang is on ~the first full render
   frame after the cold-shader-compile watchdog stalls (those early
   `engine::update` stalls at :42-:45 are the Slang cold compile, a
   SEPARATE expected event, not this hang).
   - **`#19` is a pool-ordinal, not a pass name** (CommandPools.cppm
     names lists `gse.worker <q> f<N> #<pool-index>` at allocation). The
     real pass label rides the PIX `begin_debug_event` marker, which DRED
     doesn't surface. DIAGNOSTIC ADDED 2026-07-24 (UNBUILT): dx12
     `begin_debug_event` now also `set_object_name(list, label)` when
     `m_validation_enabled` -> the list's DRED name becomes the actual
     pass label (e.g. a VBD solve pass vs `rt_shadow` tlas_update). Next
     hang log names the pass definitively. Gated on validation, zero cost
     in release.
   - **NAMED 2026-07-24 (2nd run, label-in-name diagnostic worked):** the
     hung pass is `gse::vbd::vbd_broad_phase_stage` (compute), op[4]
     DISPATCH. ROOT-CAUSE TRACED to the `collision_broad_phase.slang`
     grid linked-list walk (`while (entry_ptr != 0 && entry_ptr >=
     grid_table_size+1) { entry_ptr = grid_data[entry_ptr+1]; }`)
     following a CYCLIC list. Proof it's this loop and not the others:
     the binary search can't spin (OOB would page-fault, none seen); the
     triple-nested cell loop would only run away on garbage AABBs, but
     `collision_grid_build` has the IDENTICAL cell loop over the same
     AABBs and did NOT hang -> AABBs bounded -> by elimination it's the
     list walk, and a non-terminating walk = a cycle = corrupt grid.
   - **host_zero fix (2026-07-24) did NOT fix it** (rebuilt+repro'd, same
     broad_phase hang) — ruled out first-frame uninitialized grid;
     `collision_reset` re-clears the grid each frame so host_zero only
     covered frame 1. host_zero kept anyway (correct/consistent).
   - **REAL ROOT CAUSE (2026-07-24): bindless storage buffers written by
     compute shaders are bound as read-only SRVs on DX12.** `buffer_desc`
     (GpuBackend/Buffer.cppm) defaults `writable=false`; dx12
     `create_buffer` (Device.cpp ~1341) creates an SRV when
     `!writable`, a UAV only when `writable`. `f.grid_buffer` set neither
     `writable` nor `byte_address` -> SRV -> the shader's
     `InterlockedAdd`/`InterlockedExchange` on `grid_data` fail/duplicate
     -> two threads get the same slot -> a node's `next` points to itself
     -> `collision_broad_phase`'s list walk cycles -> hang. Vulkan is
     immune (SSBOs are always read-write). Reads through the SRV work,
     which is why bindless meshlet geometry renders. The GPU_UPLOAD heap
     already has `ALLOW_UNORDERED_ACCESS`, so a UAV is legal here.
     **FIX (2026-07-24, current): surgical grid-only, via `create_buffer`
     (NOT the reverted derivation).** grid_buffer gets `.stride =
     sizeof(std::uint32_t)`, `.writable = true` -> a structured UAV
     stride 4 (the grid is `ssbo_readwrite<uint>` = `RWStructuredBuffer<uint>`
     in Slang, NOT byte-address — the earlier byte_address idea was wrong).
     The grid is compute-ONLY (no graphics read), so making it a UAV has
     none of the compute-write/graphics-read conflict that sank the
     derivation. `collision_reset`+`grid_build` now write it correctly ->
     `broad_phase` walks a well-formed list -> no hang, no rendering impact.
     Other VBD buffers stay SRVs (their writes still fail -> physics stays
     wrong / character falls or freezes), but none of them drive an
     unbounded loop the way the grid does, so no re-hang expected. If a
     different VBD pass hangs on rebuild, it needs the same `.stride +
     .writable` treatment (the log will name it).
   - **UPDATE: grid-only was NOT enough — broad_phase re-hung 2026-07-24
     (rebuilt). The grid descriptor I made == what the derivation made,
     yet the derivation build did NOT hang broad_phase -> the derivation
     fixed it via the OTHER VBD buffers being correct too (broad_phase
     reads body/static_bodies/env/etc., all of which had stride=whole-buffer
     = 1 element = broken array access; the loop bounds read garbage).
     Root: EVERY VBD bindless buffer was created with no `.stride` and no
     `.writable` -> a 1-element structured SRV -> all array reads/writes
     wrong; the grid was just the first domino.**
   - **FULL FIX (2026-07-24, UNBUILT): all ~30 VBD bindless buffers get
     `.stride = sizeof(element)` and, for the RW (ssbo_readwrite) ones,
     `.writable = true`, added INLINE on each direct
     `ctx.device->create_buffer({...})` call in GpuSolver.cpp
     `create_buffers` (NO helper/wrapper — user rejected routing all
     creation through a helper). This produces exactly the descriptors the
     reverted derivation did for VBD, but scoped to VBD via `create_buffer`
     -> does NOT touch the graphics/instance descriptors the derivation
     stomped, so rendering stays intact. VERIFIED nothing outside VBD reads
     a VBD buffer's own bindless slot (PhysicsTransformRenderer reads the
     snapshot via its own `write_storage_buffer` view, not the VBD slot).
     Split: 22 RW buffers (stride+writable), 9 RO buffers (stride only,
     SRV); 2 non-bindless (physics_snapshot, grounded_readback) untouched.
     Expected: broad_phase + all VBD passes stop hanging, physics runs,
     rendering unaffected.
   - **SYSTEMIC — this is bigger than the grid.** VBD sets `.writable`
     on ZERO of its ~20 buffers; engine-wide only 2 files (GeometryCollector,
     LightCullingRenderer) ever set it. So EVERY compute-written bindless
     buffer that isn't in those two files is a read-only SRV on DX12 ->
     its writes silently fail. The grid is just the first to *hang* (its
     failure mode is an infinite loop); the rest (body_buffer,
     collision_pairs, contact_*, solve_state, ...) silently corrupt ->
     physics can't actually work on DX12 until they're all writable.
     Proper fix options: (a) set `.writable=true` (+ byte_address/stride
     per access type) on every compute-written bindless buffer, or (b)
     infer writable from the shader binding (`ssbo_readwrite`) in the
     codegen/create path, or (c) default bindless STORAGE buffers to UAV
     on DX12 (matches Vulkan SSBO semantics; UAVs are readable).
   - **DERIVATION IMPLEMENTED (option b) 2026-07-24, UNBUILT.** The binding
     annotation is now authoritative for the DX12 bindless descriptor.
     Chain: `shaders::descriptor_is_raw_of<T>` + `descriptor_stride_of<T>`
     (ShaderCodegen.cppm, mirrors `descriptor_access_of`) ->
     `gpu::descriptor_is_raw_v`/`descriptor_stride_v` (PipelineBuilder.cppm)
     -> `recording_context::register_one_bindless` (RecordingContext.cppm)
     calls new `device::ensure_bindless_storage_view(slot_index, writable,
     raw, stride)` for every buffer binding — NO `active_backend` branch in
     the frontend; the method is polymorphic (Vulkan backend = no-op, DX12
     = real), so backend identity does not leak into the record layer.
     **REVERTED 2026-07-24 (built + repro'd): the derivation FIXED the
     crash but BROKE all rendering (sky/atmosphere/scene -> clear color).**
     Root of the regression: `ensure_bindless_storage_view` is a THIRD
     authority over each heap slot, conflicting with `create_buffer` and
     `write_storage_buffer` for buffers that are compute-WRITTEN and
     graphics-READ (e.g. instance/transform buffers `write_storage_buffer`
     sets up as raw UAVs). The derivation's sticky UAV + structured form
     overrode those working descriptors -> graphics read garbage (valid
     descriptor, wrong form/state -> no validation error). The derivation
     LOGIC is correct (matches Slang codegen) but it can't be authoritative
     while two other paths also write descriptors, and the
     compute-write/graphics-read buffer needs one descriptor that serves
     both — not solvable at the descriptor layer alone (needs render-graph
     state cooperation). Hook neutralized in `register_one_bindless`
     (the `ensure_bindless_storage_view` seam + `descriptor_is_raw_v`/
     `descriptor_stride_v` left as dead code, not yet removed). New device seam method (mirrors `write_storage_buffer`
     across gpu Device.cppm/.cpp, DeviceVulkanBackend=no-op,
     DeviceDx12Backend->dx12::device). dx12 impl creates the matching view
     (raw/structured x SRV/UAV, stride=sizeof(element) for ssbo) at the
     slot's heap location, cached per-slot in `m_storage_view_sigs` and
     UPGRADE-ONLY on UAV (a buffer read in one shader + written in another
     stays a UAV so the writer never loses its UAV; UAVs are readable).
     Mapping proven from codegen: `ssbo_readonly`->StructuredBuffer(SRV),
     `ssbo_readwrite`->RWStructuredBuffer(UAV),
     `byte_address`/`rw_byte_address`->raw. The grid `byte_address` hack
     was REVERTED (grid_data is `ssbo_readwrite` = structured, not raw);
     the derivation makes it a structured UAV stride=4. This fixes the
     hang AND every silently-corrupt compute-written VBD buffer at once,
     and produces IDENTICAL descriptors for the buffers that already work
     (geometry sets stride=sizeof(element) manually). Read-only buffers
     that never bind read_write keep an SRV. **Remaining: build.** Watch:
     10-file cross-partition change; the reflection dispatch
     (define_aggregate over vulkan_device_backend) must pick up the new
     method on both backends — if it fails, check the two backend
     signatures match exactly.
   - **SEPARATE CONFIRMED BUG (not this hang, not yet fixed):**
     `dx12::commands::fill_buffer` (Commands.cppm) and
     `device::record_buffer_fill_u32` (Device.cpp) are BOTH no-op `{}`
     stubs. VBD clears `frozen_jacobian`/`solve_state`/`solve_deltas` via
     `rec.fill_buffer` each frame (GpuSolver.cpp ~1118) -> silently does
     nothing on DX12, so those buffers are never cleared. Not the
     broad_phase hang (uncleared solve buffers -> NaN would hang
     grid_build first, which didn't), but a real latent corruption; fix
     `fill_buffer` (ClearUnorderedAccessViewUint or a copy-from-zero) as
     a follow-up.

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
5. **Prime suspect (superseded — see 6)**: `BuildRaytracingAccelerationStructure`
   consuming instance data in the wrong state. The debug layer names the
   exact list (`'gse.worker graphics f0 #7'`): instance buffer in
   `UNORDERED_ACCESS`, build requires `NON_PIXEL_SHADER_RESOURCE`. The
   missing transition also means no flush/visibility for the
   compute-written instance descriptors, so the first REAL instance
   (spawn) can hand the build stale/garbage BLAS GPU VAs — a classic
   execution hang. Pre-spawn builds have nothing to dereference, which is
   why only spawn dies. The same warning class fires for BLAS
   vertex/index inputs.
6. **2026-07-21 repro (F5 obj load, fix v1 in the binary): still hangs,
   verdict shifted.** Fix v1 (tracked-state transition in
   `build_acceleration_structures`) never fired: the TLAS instance
   buffer's UAV state comes from **implicit promotion** by the
   `tlas_update` bindless-UAV dispatch (RtShadowRenderer), and bindless
   access is invisible to `m_buffer_states` — the map honestly said
   COMMON, the skip rule skipped, 45 instance-data warnings persisted
   (resource prints as 'Unnamed': `create_tlas` passed an empty tag; it
   IS registered in the address map). New evidence:
   - DRED: the only partially-executed list hung on a **DISPATCH** (ops:
     begin / event / resolve / barrier done / **DISPATCH hung**); the
     TLAS-build list (node[583]) completed 0/13 — it never started. The
     GPU hung in a compute-shader dispatch, not the AS build.
   - Ring: last graphics execute = 46 lists (spawn signature; steady 42);
     the last two compute frames dropped to 187 lists (steady 247).
   - New warning class from load (:16.5) to hang (:20.07), every frame:
     the VBD warm-start copy on `gse.worker compute fN #16` —
     `vbd.contact` read as copy-source while promoted-UAV,
     `vbd.warm_start` written as copy-dest while promoted-NON_PIXEL.
     `rec.copy_buffer` sites carry only memory-scope barriers (null UAV
     on DX12) — no states anywhere.
   - DRED breadcrumb names all regressed to `queue='?' list='?'` (616
     nodes; names worked 2026-07-16).
   Working theory: obj load starts VBD + RT instance work; a VBD compute
   dispatch hung (garbage via the state-corrupted warm-start copy is the
   lead, unproven); graphics' frame-coarse wait on the compute timeline
   propagated the stall to everything; TDR. The AS instance-state defect
   is real but was not the op executing at the hang.
7. **2026-07-22 repro (fix v2 in the binary): state warnings GONE — fix
   v2 verified (zero BuildRTAS, zero CopyBufferRegion warnings) — but the
   hang is unchanged.** Same signature: exactly one partially-executed
   list hung on DISPATCH right after its RESOURCEBARRIER (node[365], 4/7
   ops: begin/event/resolve/barrier done -> DISPATCH hung); graphics
   steady at 42 lists, compute at 67; present blocked ~19 s -> TDR ->
   SEH. The DRED page-fault dump (already implemented in the walker)
   printed nothing -> PageFaultVA == 0 -> the dispatch is not memory
   faulting; it loops. So the hang is data/shader-driven, not a barrier
   or residency problem. Separately root-caused this run: the no-draw
   problem (viewport Y-flip, see Current state) and the DRED '?' names
   (A-vs-W debug-name fields).

## Next actions (ordered)

1. **Build + run with the 2026-07-23 fixes** (projection Y-flip for dx12
   in `camera::compute_projection_matrix` + jitter-Y match; viewport is a
   plain positive pass-through; DRED W-name fallback). Expected: geometry
   renders RIGHT-SIDE UP. Then:
   - **Textures**: capture a frame (RenderDoc/PIX). Check whether a
     textured draw's `SamplerDescriptorHeap[]`/`ResourceDescriptorHeap[]`
     indices resolve to the written SRV+sampler, and whether the sampler
     descriptor is well-formed. This is the fastest way to split suspects
     (a) sampler vs (b) material-index propagation. See problem 1 above.
   - Evaluate remaining rendering correctness (old OIT-alpha /
     `SV_Target` lead).
   - If the F5 hang reproduces, the DRED dump now NAMES the hung list —
     map `gse.worker <queue> fN #K` to its pass, then read that shader
     for a data-dependent loop (no page fault + clean states = the
     dispatch spins on data; candidates: first-ever tlas_update run,
     meshlet culling on first real geometry, VBD with fresh bodies).
   (2026-07-21's bindless-promotion state fixes are verified effective —
   zero state warnings in the 07-22 log.)
2. Remove `swapchain::present`'s CPU `wait_fence` on `render_finished`
   (`Engine/Engine/Source/Dx12/Swapchain.cppm`) — it fully serializes CPU
   against GPU every frame. Bring-up crutch; replace with proper
   frames-in-flight pacing once stable.
3. Phases 1-2 of the cross-queue batching plan (per-batch scheduling in
   the render graph). Justified by the proven-but-latent defects and by
   the Vulkan "RT cross-queue AS hang" note; not urgent for DX12 spawn.
4. Mesh-rendering wrongness (OIT alpha lead), once geometry is visible.

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

# Render Graph Multi-Queue Support + VBD Solver Migration

This document tracks two coupled pieces of work:

1. **Backend feature: per-pass queue selection in `render_graph`.** Required first.
2. **VBD solver migration: move `gpu_solver::dispatch_compute` into the graph as compute-queue passes.** Unblocked by (1).

The motivation for both is the same: the VBD solver is the only major GPU system not in the render graph. It has its own `gpu::compute_queue`, manual semaphore plumbing, manual timing/profile aggregation, and manual barrier insertion — all rebuilding what the graph already provides. We want the solver inside the graph. But a naive migration drops it onto the graph's single queue, losing async-compute overlap with graphics rendering — a real per-frame regression on modern GPUs. So the graph has to grow multi-queue support first.

---

## Part 1 — Render graph: per-pass queue selection

### Goal

Extend `render_graph` so each pass declares which Vulkan queue it runs on (graphics / compute / transfer), with the graph auto-inserting timeline-semaphore waits for cross-queue dependencies. Today the graph is single-queue: anything migrated into it forfeits async-compute parallelism.

### Why

- **Unblock VBD solver migration** (Part 2) without giving up async compute.
- **Enable future async workloads** (light culling, mesh skinning, particle sim) to opt into the compute queue with one line: `.on(queue_type::compute)`. No per-system semaphore plumbing.
- **Remove the special-case ceiling** where async-compute work must live outside the graph.

### Desired end state

- `gpu::pass_builder` gains a queue selector. Recommendation: `.on(gpu::queue_type::compute)` / `.on(gpu::queue_type::graphics)`. Default is graphics.
- `render_pass_descriptor` carries a `gpu::queue_type queue` field.
- `render_graph::execute` partitions passes by queue, builds per-queue command buffers, submits each to its target queue.
- Cross-queue dependencies (pass A on queue X writes a resource read by pass B on queue Y) auto-insert a timeline-semaphore signal on X and a wait on Y, plus the required queue-family ownership transfer barriers if the queues are different families.
- Per-pass GPU timestamps continue to work on both queues — each queue gets its own timestamp pool, but [`profile::ingest_gpu_sample`](Engine/Engine/Source/Diag/ProfileAggregator.cppm:149) and the `trace::gpu_virtual_tid` timeline stay unified (one virtual GPU lane or one lane per queue — design choice; recommend single unified lane with per-pass queue tag).
- Per-frame fence rotation tracks completion across all queues used in the frame.
- All existing callsites of `gpu::pass<X>(ctx)` continue working unchanged — graphics queue stays the default. Verify the ~10 existing renderer callsites pass through with zero modification.

### Constraints

- **No regressions for existing single-queue users.** CullCompute, SkinCompute, LightCulling, PhysicsTransform, etc., must behave identically.
- **Auto-timing must work per-queue.** The block at [RenderGraph.cppm:977-987](Engine/Engine/Source/Gpu/Vulkan/RenderGraph.cppm:977) ingests GPU timestamps + async trace markers + profile samples for every pass. That logic has to fire correctly for compute-queue passes too, with their own clock reference.
- **`profile::ingest_gpu_sample` should produce one unified timeline.** Decide: single GPU virtual TID for all queues (passes carry a queue tag), or separate TIDs per queue (cleaner display, slight aggregator changes).
- **Cross-queue dependency machinery is invisible to pass authors.** They keep declaring `.reads()` / `.writes()` / `.after<>()`. The graph derives semaphores from access patterns same as it derives barriers today.

### Design decisions to make

1. **Timeline vs binary semaphores.** Strongly recommend timeline (Vulkan 1.2 core, already a baseline assumption in this codebase). One timeline semaphore per queue, each pass increments, dependents wait on a specific value. Binary semaphores need 1-to-1 producer/consumer matching and don't compose well across multiple cross-queue edges.
2. **Queue selection API.** Three options:
   - Builder method: `co_await gpu::pass<X>(ctx).on(queue_type::compute)...` — recommended; explicit, opt-in, local.
   - Annotation on owner type: `struct [[= queue::compute]] system { ... };` — declarative but spreads queue info between the type and the use site.
   - Inferred from resource usage — too magical, skip.
3. **Queue-family ownership transfers.** Resources used cross-queue between different queue families need explicit acquire/release barriers (Vulkan spec). Options:
   - Assume all queues are graphics-family (compute-capable graphics queue, no transfer needed). Simpler, common on modern GPUs.
   - Handle separate compute family with auto-inserted transfer barriers. More general but more code.
   - Check what queue families the device exposes (likely already in [Device.cppm:295](Engine/Engine/Source/Gpu/Vulkan/Device.cpp:295) area) and pick based on what's actually available.
4. **Cross-queue R-W inference.** Today the graph derives edges in [RenderGraph.cppm:1117-1143](Engine/Engine/Source/Gpu/Vulkan/RenderGraph.cppm:1117) from write-then-read overlap across passes. With multi-queue, the same edge-producing logic should output a **semaphore** instead of a pipeline barrier when the producer and consumer are on different queues. Same-queue edges keep producing pipeline barriers. The data structure tracking edges needs to distinguish the two cases.
5. **Frame-in-flight fencing.** Each queue's submit returns a fence (or timeline value). The frame-end fence wait needs to wait on all queues that submitted work this frame. Probably extend the existing fence wait to a fence-array wait.
6. **`recording_context` per queue.** Currently one `recording_context` wraps one command buffer. Multi-queue means multiple command buffers per frame — one per queue. The recorder coroutine for each pass needs to record into the *right* command buffer. Probably: the graph executor sets the active recording context per pass based on the pass's queue selection, and `recording_context` itself doesn't need to know about queues.

### Files to read first

- [Engine/Engine/Source/Gpu/Vulkan/RenderGraph.cppm](Engine/Engine/Source/Gpu/Vulkan/RenderGraph.cppm) — the engine. ~1100 lines. Pass partitioning, fence handling, timestamp ingestion, edge derivation all live here.
- [Engine/Engine/Source/Gpu/RenderPass.cppm](Engine/Engine/Source/Gpu/RenderPass.cppm) — `pass_builder` API.
- [Engine/Engine/Source/Gpu/Resources/Compute.cppm](Engine/Engine/Source/Gpu/Resources/Compute.cppm) — existing `gpu::compute_queue` (single-queue Vulkan wrapper). Reference for queue handling primitives.
- [Engine/Engine/Source/Diag/ProfileAggregator.cppm](Engine/Engine/Source/Diag/ProfileAggregator.cppm) — `ingest_gpu_sample` consumer. Verify multi-queue timestamps flow correctly.
- [Engine/Engine/Source/Gpu/Vulkan/Device.cpp](Engine/Engine/Source/Gpu/Vulkan/Device.cpp) (search for `queueFamily`) — queue family discovery, see what's already exposed.

### How to know it's working

- Every existing scene runs identically. No visual or perf regression for the ~10 existing single-queue renderers.
- A trivial test: a new compute pass `gpu::pass<X>(ctx).on(queue_type::compute).writes(buf)` followed by a graphics pass that reads `buf` automatically waits on the compute work. Verify in RenderDoc: a queue-submit + semaphore wait shows up between them, not a pipeline barrier.
- The profiler timeline shows compute-queue work running concurrently with graphics-queue work where dependencies don't force serialization.
- No semaphore leaks across frames (timeline values monotonically advance per queue, fences/wait values matched).
- Stress test with the eventual VBD migration (Part 2) — many compute passes, many cross-queue handoffs to the renderer.

### Out of scope for Part 1

- The VBD migration itself. That's Part 2.
- Migrating any other existing system to compute queue. Just unblock the capability.
- Transfer queue support (separate from compute). Add the `transfer` enum variant for future, but only graphics + compute need to actually work.

---

## Part 2 — VBD solver: migrate `dispatch_compute` into the graph

### Prerequisite

Part 1 must be done. Without per-queue passes, this migration regresses perf because the solver currently has dedicated-compute-queue async overlap with rendering.

### Goal

Replace the standalone `gpu::compute_queue` in `gpu_solver` with graph-managed compute-queue passes. Eliminate manual semaphore plumbing, manual timing, and manual barrier insertion. The solver becomes a sequence of compute-queue passes (~14 logical stages per substep) that the graph schedules alongside everything else.

### What gets deleted

In [Engine/Engine/Source/Physics/VBD/GpuSolver.cppm](Engine/Engine/Source/Physics/VBD/GpuSolver.cppm):
- `gpu::compute_queue queue` field on `per_frame_data`.
- `bool first_submit = true;` field on `per_frame_data` (was for compute_queue's first-submit logic).
- `m_compute.solve_ms` field.
- `namespace timing_slot { ... }` block.
- `compute_semaphore()` accessor method.
- `solve_time()` accessor method.

In [Engine/Engine/Source/Physics/VBD/GpuSolver.cpp](Engine/Engine/Source/Physics/VBD/GpuSolver.cpp):
- All `f.queue.begin(...)`, `f.queue.submit()`, `f.queue.is_complete()`, `f.queue.wait()`, `f.queue.semaphore_state()` calls.
- All `f.queue.begin_timing()`, `f.queue.mark_timing(...)`, `f.queue.end_timing()`, `f.queue.read_timing(...)` calls.
- The `ingest_stage` lambda and its `find_or_generate_id` / `profile::ingest_gpu_sample` calls.
- All explicit `f.queue.barrier(scope)` and `f.queue.barriers(scopes)` calls inside `dispatch_compute` (graph derives these from `.reads()`/`.writes()`).
- All `f.queue.copy_buffer(...)` at end of dispatch — replaced by `rec.copy_buffer(...)` inside a final pass.
- The destructor's `f.queue.wait()` loop (graph handles per-frame fencing).
- `gpu::compute_queue::create(...)` in `initialize_compute`.

In [Engine/Engine/Source/Physics/System.cppm](Engine/Engine/Source/Physics/System.cppm):
- `gpu_solver_frame_info::semaphore` field.
- `gpu_solver_stats::solve_time` field.

In [Engine/Engine/Source/Physics/System.cpp](Engine/Engine/Source/Physics/System.cpp):
- The `ready_to_dispatch()` check at the dispatch site — graph handles per-frame sync.
- `.solve_time = d.gpu_solver.solve_time(),` from the `gpu_solver_stats` push.
- `.semaphore = d.gpu_solver.compute_semaphore(),` from the `gpu_solver_frame_info` push.

In [Engine/Engine/Source/Graphics/Renderers/PhysicsTransformRenderer.cpp](Engine/Engine/Source/Graphics/Renderers/PhysicsTransformRenderer.cpp):
- The `if (!info.semaphore.has_signaled()) co_return;` gate (replaced by `.after<>()` graph dependency).

### What gets added

Stage tag types in `GpuSolver.cpp`, one per logical stage (file-scope structs, used as `gpu::pass<vbd_predict_stage>(ctx)`):

```cpp
struct vbd_collision_reset_stage {};
struct vbd_grid_build_stage {};
struct vbd_broad_phase_stage {};
struct vbd_prepare_indirect_stage {};
struct vbd_narrow_phase_stage {};
struct vbd_prepare_contact_indirect_stage {};
struct vbd_build_adjacency_stage {};
struct vbd_predict_stage {};
struct vbd_freeze_jacobians_stage {};
struct vbd_solve_iterations_stage {};   // wraps the full inner iteration loop
struct vbd_derive_velocities_stage {};
struct vbd_apply_restitution_stage {};  // per-color sequential inside one pass
struct vbd_post_stabilize_stage {};     // conditional on cfg.post_stabilize
struct vbd_finalize_stage {};
struct vbd_readback_copy_stage {};      // final buffer copies to readback/snapshot
```

`dispatch_compute` signature change:
```cpp
auto dispatch_compute(frame_context& ctx) -> async::task<>;
```

Each stage becomes a block like:
```cpp
{
    auto rec = co_await gpu::pass<vbd_predict_stage>(ctx)
        .on(gpu::queue_type::compute)
        .reads(/* buffers read */)
        .writes(/* buffers written */);
    rec.bind(m_compute.predict_pipeline);
    rec.bind_descriptors(m_compute.predict_pipeline, f.descriptors);
    rec.push(m_compute.predict_pipeline, make_pc(...));
    rec.dispatch(body_workgroups, 1, 1);
}
```

Inner loops (per-iteration, per-color) stay inside a single pass — the `solve_iterations` pass body contains the iteration `for` loop with `rec.barrier(...)` between sub-dispatches as today. Same for `apply_restitution`'s per-color loop.

### Resource declarations per stage

Bulky but mechanical. Per-stage reads/writes (all in compute_shader stage):

| Stage | Reads | Writes |
|---|---|---|
| collision_reset | — | collision_state, color_data, grid_data, indirect_dispatch |
| grid_build | body | grid_data |
| broad_phase | body, grid_data | collision_pairs |
| prepare_indirect | collision_pairs | indirect_dispatch |
| narrow_phase | body, collision_pairs, warm_starts | contact, collision_state |
| prepare_contact_indirect | collision_state | indirect_dispatch |
| build_adjacency | contact, joint, body, collision_state | contact_offsets, contact_counts, contact_adjacency, joint_offsets, joint_counts, joint_adjacency, color_data, indirect_dispatch, collision_state |
| predict | motor, motor_map, contact, contact_offsets, contact_counts, contact_adjacency, joint, joint_offsets, joint_counts, joint_adjacency, body | body |
| freeze_jacobians | contact, body | frozen_jacobian |
| solve_iterations | body, contact, joint, motor, motor_map, color_data, contact_offsets, contact_counts, contact_adjacency, frozen_jacobian, indirect_dispatch | body, contact, joint, solve_state, solve_deltas, collision_state |
| derive_velocities | body | body, collision_state |
| apply_restitution | contact, contact_offsets, contact_counts, contact_adjacency, color_data | body |
| post_stabilize | (same as solve_iterations) | (same as solve_iterations) |
| finalize | body | body |
| readback_copy | body, contact, collision_state, joint | readback_buffer, physics_snapshot_buffer (+ other body_buffer for the other-slot copy) |

A small helper to compress the boilerplate is recommended:
```cpp
template <typename Tag>
auto begin_compute_stage(frame_context& ctx,
    std::initializer_list<gpu::resource_usage> reads,
    std::initializer_list<gpu::resource_usage> writes)
    -> /* awaitable */;
```

### Resource dependency note

The graph's W→R edge inference at [RenderGraph.cppm:1117-1143](Engine/Engine/Source/Gpu/Vulkan/RenderGraph.cppm:1117) handles intra-frame, intra-system ordering between stages. Read-modify-write stages must declare *both* `.reads()` and `.writes()` for the same buffer so that subsequent stages reading the same buffer get a dependency edge to them. The graph's `else if` order resolution prevents cycles when two passes both read-modify-write the same buffer (stable by registration order).

Across substep iterations of the same stage type, the same resource-edge mechanism chains substep N+1's stage to substep N's stage. The `type_to_index` map at [RenderGraph.cppm:1091](Engine/Engine/Source/Gpu/Vulkan/RenderGraph.cppm:1091) only stores one index per type (last-registered wins), so don't rely on `.after<TagType>()` for intra-substep ordering — use resource declarations.

External consumers (PhysicsTransformRenderer) using `.after<vbd_readback_copy_stage>()` correctly resolve to the *last* substep's copy pass, which is the right thing.

### Caller side changes

[System.cpp:1428](Engine/Engine/Source/Physics/System.cpp:1428):
```cpp
// Before:
if (d.gpu_solver.pending_dispatch() && d.gpu_solver.ready_to_dispatch()) {
    d.gpu_solver.commit_upload();
    d.gpu_solver.dispatch_compute();
}

// After:
if (d.gpu_solver.pending_dispatch()) {
    d.gpu_solver.commit_upload();
    co_await d.gpu_solver.dispatch_compute(ctx);
}
```

[System.cpp:1437-1448](Engine/Engine/Source/Physics/System.cpp:1437): drop `.solve_time` and `.semaphore` from the channel pushes.

[PhysicsTransformRenderer.cpp:115-117](Engine/Engine/Source/Graphics/Renderers/PhysicsTransformRenderer.cpp:115): drop the `info.semaphore.has_signaled()` gate.

[PhysicsTransformRenderer.cpp:135](Engine/Engine/Source/Graphics/Renderers/PhysicsTransformRenderer.cpp:135): add `.after<vbd::vbd_readback_copy_stage>()` to the pass builder.

### Readback timing

Today's `stage_readback` uses `f.queue.is_complete()` to gate reading the other slot's GPU-written readback buffer. Under graph, the per-frame fence rotation guarantees that, by the time we're recording frame N, frame N-(frames_in_flight) is GPU-complete.

Two reasonable approaches:
1. **Track the dispatch frame index per slot.** Allow readback when `current_frame - slot.dispatch_frame >= frames_in_flight`.
2. **Always readback the slot not being written this frame.** With 2-slot double buffering and 2 frames-in-flight, that slot was last written ≥1 frame ago and is guaranteed complete on entry to the next frame's CPU work for that slot.

Option 2 is simpler and matches the engine's existing per-frame buffer rotation pattern. Verify by tracing on the engine's `frames_in_flight` constant.

### What about `prepare_joint`?

CPU helper, no GPU work, no changes. Stays where it is in `GpuSolver.cpp`.

### What this enables / sets up next

- Every GPU system is now in the graph. No special cases.
- Async-compute opt-in is one line per pass.
- ~200-300 lines of manual plumbing (timing, semaphores, barriers, queue submit/wait) deleted from `GpuSolver.cpp`.
- Solver passes show up in the unified profiler timeline alongside everything else, on the compute-queue lane.

### Risk areas

- **Resource declarations are easy to get wrong.** A buffer that's read but not declared = race condition. Recommend a careful audit pass after migration: for each stage, cross-check that every buffer the shader binds is declared in `.reads()` or `.writes()`. Possible to derive these from the descriptor bindings if a future cleanup wants to remove the boilerplate entirely.
- **Readback latency might shift.** The current `is_complete()` gate is conservative; the graph-based gating could potentially be 1 frame earlier or later depending on how it's implemented. Validate by running the GPU-vs-CPU solver comparison mode and watching for drift.
- **Substep loop with many passes.** Each substep produces ~14 pass records. With 2 substeps that's 28 passes from VBD alone. Verify the graph handles this volume without scheduling cost spike (profile pass-planning time).
- **Async compute semaphore correctness.** The cross-queue edge between VBD's `vbd_readback_copy_stage` (compute queue) and PhysicsTransformRenderer's pass (graphics queue) is the critical handoff. Validate the timeline semaphore wait correctly orders the renderer's body-buffer read after the solver's writes. RenderDoc or Nsight Graphics will show this clearly.

### How to know it's working

- Bouncing scene behaves identically to pre-migration (no perf regression, no behavior change).
- Profiler timeline shows VBD compute work on the compute-queue lane, overlapping with graphics queue work from the previous frame.
- Solver-vs-CPU comparison mode reports same magnitudes of divergence as today (no new sources of error).
- `GpuSolver.cpp` is ~200-300 lines shorter.
- Grep for `compute_queue`, `is_complete`, `mark_timing`, `solve_ms`, `gpu_solver_frame_info::semaphore` in the physics path returns nothing.

---

## Sequencing

1. Land Part 1 (multi-queue render graph). Self-contained engine work. Add a trivial test pass to validate cross-queue dependencies work in RenderDoc.
2. Land Part 2 (VBD migration). Mechanical refactor against the new capability. One PR, all-at-once (no half-state where some VBD stages are on `compute_queue` and others on `rec` — they share buffers).

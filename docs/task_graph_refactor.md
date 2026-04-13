# Task Graph Refactor: Two-Tier Execution

## Current State (2026-04-13)

**Update tier: COMPLETE.** All systems migrated to `update(update_context&, state&) -> void`. Old update pipeline deleted (`work_queue`, `queued_work`, `build_work_batches`, `chunk<T>`, `chunk_traits`, `update_phase`, `has_update` concept).

**Frame tier: NOT STARTED.** Render pipeline still uses old `begin_frame_phase` / `prepare_render_phase` / `render_phase` / `end_frame_phase` with insertion-order execution. Infrastructure is built (`frame_context`, `graph_frame`, `after<T>()`, `notify_ready<T>()`) but no system uses it yet.

**GUI: retained-mode redesign in progress** (separate agent). Immediate-mode `begin_frame`/`end_frame` bracket stays in old render pipeline until retained mode is ready.

---

## Architecture

### Two-Tier Model

```
+-- UPDATE TIER -----------------------------------------------+
|  All systems run update(update_context&, state&) -> void      |
|  Sequential collection, parallel execution via task graph     |
|  Component access via read<T>/write<T> access tokens          |
|  This is the ONLY place state is mutated                      |
+---------------------------------------------------------------+
                        |
          state readable by render (semaphore barrier)
                        |
+-- FRAME TIER (planned) --------------------------------------+
|  Systems run frame(frame_context&, const state&) -> task<>    |
|  Parallel coroutines with co_await ctx.after<T>()             |
|  Replaces begin_frame/prepare_render/render/end_frame         |
+---------------------------------------------------------------+
```

### System Interface

```cpp
struct my_system {
    static auto initialize(init_context&, state&) -> void;
    static auto shutdown(shutdown_context&, state&) -> void;
    static auto update(update_context&, state&) -> void;
    static auto frame(frame_context&, const state&) -> async::task<>;
};
```

`update` returns `void`. System methods run sequentially (collection phase). Component work declared via `ctx.schedule([](read<A>, write<B>) { ... })` runs in parallel with conflict serialization (execution phase).

`frame` returns `async::task<>`. Coroutines earn their keep here — parallel execution with `co_await ctx.after<T>()` for dependency ordering.

### Update Tier Execution

**Phase 1 — Collection (sequential):** Each system's `update(ctx, state)` runs one at a time. Inline code executes immediately. `ctx.schedule(lambda)` defers the lambda + its reads/writes.

**Phase 2 — Execution (parallel):** Deferred lambdas submitted to task graph with read/write type sets. `build_edges()` creates conflict edges. Non-conflicting work runs in parallel on TBB via `task::group` (work-stealing, no deadlock). Conflicting work serializes.

### Update/Render Overlap

Update and render run as parallel TBB tasks. A `std::binary_semaphore` ensures render does not read system state until update completes. The acquire is placed after the GPU fence wait in `begin_frame`.

### Thread Safety

- **Component access**: `read<T>`/`write<T>` access tokens in schedule lambdas. Task graph serializes conflicts via `build_edges()`.
- **System state**: each system writes only its own `state&`. Cross-system reads via `state_of<T>()` see previous frame's finalized state (sequential execution within collection phase).
- **Channels**: `channel_writer::push()` is mutex-protected. `read_channel<T>()` reads from immutable frame-start snapshot.
- **Deferred ops**: `mpsc_ring_buffer` — lock-free multi-producer, single-consumer drain after graph execution.
- **Escaped pointers**: eliminated. `bind_int_map_request` uses value semantics with `int_map_sync`/`int_map_loaded` channels.

---

## What's Built

### New Files
| File | Partition | Purpose |
|------|-----------|---------|
| `Concurrency/AsyncTask.cppm` | `:async_task` | Lazy coroutine `async::task<T>`, `when_all`, `sync_wait` |
| `Concurrency/FrameArena.cppm` | `:frame_arena` | Lock-free bump allocator for coroutine frames |
| `Concurrency/TaskGraph.cppm` | `:task_graph` | DAG executor with `build_edges`, `notify_state_ready`/`wait_state_ready` |
| `Ecs/AccessToken.cppm` | `:access_token` | `access<T, M>`, `read<T>`, `write<T>` replacing `chunk<T>` |
| `Ecs/UpdateContext.cppm` | `:update_context` | Update tier context with `schedule()`, `defer_add()`, channels |
| `Ecs/FrameContext.cppm` | `:frame_context` | Frame tier context with `after<T>()`, `notify_ready<T>()` |

### Modified Files
| File | Changes |
|------|---------|
| `Ecs/SystemNode.cppm` | Added `graph_update`/`graph_frame` virtuals + concepts. Removed old `update(update_phase&)` virtual. |
| `Concurrency/Scheduler.cppm` | `update()` calls `run_graph_update()` only (old pipeline deleted). Semaphore between update/render. `wrap_work()` free function for task graph coroutine wrapping. |
| `Ecs/PhaseContext.cppm` | Deleted `chunk<T>`, `queued_work`, `work_queue`, `update_phase`, `has_update`. Kept render-phase types. |
| `Meta/LambdaTraits.cppm` | Deleted `chunk_traits`. `access_traits` is the only extraction mechanism. |
| `Ecs/Registry.cppm` | Added `acquire_read<T>()`/`acquire_write<T>()`. |
| `Concurrency/MpscRingBuffer.cppm` | Added `push(T&&)` move overload. |
| `Runtime/Api/CoreApi.cppm` | Kept parallel update/render posting (semaphore provides safety). |

### Migrated Systems (update tier)
All systems now use `update(update_context&, state&) -> void`:

| System | Key changes |
|--------|-------------|
| Camera | `chunk<follow_component>` → `read<follow_component>` |
| Animation | `chunk<T>` → `write<T>` for 3 component types |
| Audio | Trivial rename |
| SaveSystem | `do_update` takes `update_context&`. `bind_int_map_request` uses value semantics. |
| Input | `end_frame` deleted. Flip at end of update. |
| Actions | `end_frame` deleted. Flip at end of update. `int_map_sync`/`int_map_loaded` channels. |
| Network | `phase.registry.reg` → `ctx.reg`. `phase.try_state_of` → `ctx.try_state_of`. |
| Renderer | `phase.get<gpu::context>()` → `ctx.get<gpu::context>()`. begin_frame/end_frame stay in old render pipeline. |
| GeometryCollector | `chunk<T>` → `write<T>`/`read<T>` for 4 types. `prepare_render` stays in old render pipeline. |
| Physics | `chunk<T>` → `write<T>` in schedule + all helper functions. begin_frame/render stay in old pipeline. |
| PhysicsDebug | `phase.registry.view<T>()` → `ctx.reg.linked_objects_read<T>()`. |
| UiRenderer | `phase.read_channel<T>()` → `ctx.read_channel<T>()`. |
| GUI | Update migrated. `begin_frame`/`end_frame` stay in old render pipeline (immediate-mode bracket). Retained-mode redesign in progress. |

### Deleted Legacy Code
- `chunk<T>` class + all implementations (~70 lines)
- `chunk_traits` + specialization + helpers
- `queued_work` struct + `conflicts_with()`
- `work_queue` class + `schedule()` + `make_executor()` + `invoke_with_chunks()` + `make_chunk()`
- `update_phase` struct + all method implementations
- `has_update` concept (old, checking `update_phase`)
- `build_work_batches()` in scheduler
- `parallel_for` + work queue drain in `scheduler::update()`
- Old `update(update_phase&)` virtual + override + implementation in system_node

---

## Remaining Work

### Frame Tier Migration (replaces old render pipeline)
Each render-phase system migrates from `begin_frame`/`prepare_render`/`render`/`end_frame` to a single `frame(frame_context&, const state&) -> async::task<>` with explicit `co_await ctx.after<T>()` dependencies.

**Planned frame tier DAG:**
```
geometry_collector::frame (upload)
    |
    +-- skin_compute ----------------------------------------+
    +-- cull_compute --> depth_prepass --> light_culling -----+
    +-- rt_shadow -------------------------------------------+
                                                             |
                                                    forward_renderer
                                                         |
                                               physics_debug --> ui
```

**GPU lifecycle becomes scheduler infrastructure:**
- `gpu::context::begin_frame()` called by scheduler before frame tier
- `gpu::context::end_frame()` called by scheduler after frame tier
- Renderer system's `begin_frame`/`end_frame` deleted

**Physics render_state elimination:**
- GPU readback moves to `physics::update()` (check fence, read back)
- GPU dispatch moves to `physics::frame()` (const state, dispatch through mapped handles)
- `render_state` template parameter deleted from `system_node`

### GUI Retained Mode (in progress, separate agent)
- Immediate-mode drawing API (`start`/`begin`/`end`/`slider`) replaced by channel-based menu definitions
- `begin_frame`/`end_frame` eliminated — single `update()` produces all draw commands
- `mutable` fields removed from `system_state`

### Final Cleanup (after frame tier + GUI)
- Delete `begin_frame_phase`, `prepare_render_phase`, `render_phase`, `end_frame_phase`
- Delete `initialize_phase`, `shutdown_phase` (migrate to `init_context`, `shutdown_context`)
- Delete old render virtuals from `system_node_base`
- Delete `RenderState` template parameter + `render_state_storage`
- Delete `registry_access` wrapper (replaced by direct `registry&`)

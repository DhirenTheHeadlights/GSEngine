# Task Graph Refactor: Two-Tier Execution

## Current State (2026-04-13)

**Update tier: COMPLETE.** All systems migrated to `update(update_context&, state&) -> void`. Old update pipeline deleted (`work_queue`, `queued_work`, `build_work_batches`, `chunk<T>`, `chunk_traits`, `update_phase`, `has_update` concept).

**Frame tier: COMPLETE.** All systems migrated to `frame(frame_context&, resources&, const state&) -> async::task<>`. Old render pipeline deleted. GPU lifecycle moved to `engine::render()`. Frame coroutines run via `async::sync_wait(async::when_all(...))`.

**Resources/state split: COMPLETE.** Tier-exclusive ownership model:
- `resources` — immutable after init, `const&` in both tiers
- `update_data` — mutable only during update, no frame access
- `frame_data` — mutable only during frame, no update access
- `state` — trivially copyable, snapshotted after update, frame reads snapshot
Cross-tier communication via channels. `RenderState` template parameter eliminated. Binary semaphore removed.

**Update/render overlap: ENABLED.** CoreApi runs update synchronously, posts render to worker thread. Update N+1 starts while frame N is still running. No shared mutable data between tiers — type-enforced via tier-exclusive structs.

**GUI: retained-mode redesign COMPLETE.** `begin_frame`/`end_frame` deleted. Channel-based `gui::panel()` API. All `mutable` removed from `system_state`. Single `update()` pass.

---

## Architecture

### Two-Tier Model

```
+-- UPDATE TIER ---------------------------------------------------------------+
|  update(update_context&, const resources&, update_data&, state&) -> void     |
|  Sequential collection, parallel execution via task graph                     |
|  Component access via read<T>/write<T> access tokens                         |
|  Writes: state, update_data. Reads: const resources, channel snapshots       |
+-------------------------------------------------------------------------------+
                        |
          state snapshot (trivially-copyable memcpy after update)
                        |
+-- FRAME TIER ----------------------------------------------------------------+
|  frame(frame_context&, const resources&, frame_data&, const state&) -> task<>|
|  Parallel coroutines with co_await ctx.after<T>()                             |
|  Writes: frame_data. Reads: const resources, const state snapshot, channels  |
+-------------------------------------------------------------------------------+
```

Update N+1 overlaps with frame N. No shared mutable data between tiers.

### System Interface

```cpp
struct my_system {
    struct resources { ... };    // immutable after init, const in both tiers
    struct update_data { ... };  // optional, update-exclusive mutable
    struct frame_data { ... };   // optional, frame-exclusive mutable

    static auto initialize(initialize_phase&, resources&, update_data&, frame_data&, state&) -> void;
    static auto update(update_context&, const resources&, update_data&, state&) -> void;
    static auto frame(frame_context&, const resources&, frame_data&, const state&) -> async::task<>;
};
```

All struct types detected via concepts (`has_resources<S>`, `has_update_data<S>`, `has_frame_data<S>`). Systems only define what they need — dispatch adapts automatically.

### Update/Render Overlap

CoreApi runs update synchronously on the main thread, then posts render to a worker thread. The next update starts immediately without waiting for render to finish:

```
Main thread: [update 0]→post→[update 1]→post→[update 2]→...
Worker:      ─────────────[frame 0]─────[frame 1]─────[frame 2]
```

Previous frame completion is awaited at the START of the next iteration (before channel snapshot). No semaphore — posting order provides happens-before via TBB.

### Thread Safety

- **Tier exclusivity**: `update_data` only accessible during update, `frame_data` only during frame. `resources` const in both. `state` snapshotted — frame reads frozen copy. Enforced by system_node dispatch (type-level).
- **Component access**: `read<T>`/`write<T>` access tokens in schedule lambdas. Task graph serializes conflicts via `build_edges()`.
- **Cross-system reads**: `state_of<T>()` returns const live state (update) or const snapshot (frame). `resources_of<T>()` returns const resources (both tiers).
- **Cross-tier communication**: channels only. `channel_writer::push()` is mutex-protected. `read_channel<T>()` reads from immutable snapshot taken at frame boundary.
- **Frame arena**: atomic bump allocator. Both tiers allocate concurrently via `fetch_add`. Reset at end of render.
- **Deferred ops**: `mpsc_ring_buffer` — lock-free multi-producer, single-consumer drain after graph execution.

### Frame Tier DAG

```
geometry_collector::frame (upload, notify_ready)
    |
    +-- skin_compute ----------------------------------------+
    +-- cull_compute --> depth_prepass --> light_culling -----+
    +-- rt_shadow -------------------------------------------+
                                                             |
                                                    forward_renderer
                                                         |
                                               physics_debug --> ui --> capture
```

CPU ordering via `co_await ctx.after<T>()`. GPU ordering via render graph `.after<T>()`.

---

## Deleted Legacy Code
- `chunk<T>`, `chunk_traits`, `queued_work`, `work_queue`, `update_phase`, `has_update`, `build_work_batches`, old `update(update_phase&)` virtual
- `begin_frame_phase`, `prepare_render_phase`, `render_phase`, `end_frame_phase` structs + all concepts
- Old render virtuals from `system_node_base` (begin_frame, prepare_render, render, end_frame)
- `RenderState` template parameter + `render_state_storage`
- `physics::render_state`, `rt_shadow::render_state`
- `renderer::state::frame_begun` + `is_frame_begun()` + `RendererApi::frame_begun()`
- `std::binary_semaphore m_update_complete`
- All `const_cast` in system frame methods

## Remaining Work
None. Refactor complete.

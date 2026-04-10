# Task Graph Refactor: Two-Tier Execution with Const State Snapshots

## Context

GSEngine's current concurrency model relies on **phase-rigid temporal separation**: the frame is split into 7 fixed phases (`initialize`, `update`, `begin_frame`, `prepare_render`, `render`, `end_frame`, `shutdown`). Thread safety comes from structural guarantees (work queue conflict analysis on `std::type_index`, sequential render).

### Problems

**The phase zoo is patchwork.** Each phase was added to solve a specific ordering need:
- `prepare_render_phase` — GeometryCollector needs to upload GPU buffers after `begin_frame` but before `render`
- `render_state` template param — Physics needs mutable GPU state during render
- `begin_frame` returning `bool` — Renderer needs to gate the entire pipeline
- 15+ `mutable` fields on GUI state — immediate-mode UI doesn't fit the retained-mode phase model

**Render ordering is implicit and fragile.** The render pipeline is a DAG:
```
geometry_collector::prepare_render (upload)
    |
    +-- skin_compute -----------------------------------------+
    +-- cull_compute --> depth_prepass --> light_culling ------+
    +-- rt_shadow ------------------------------------------- +
                                                              |
                                                     forward_renderer
                                                          |
                                                physics_debug --> ui
```
But execution order comes entirely from `add_system()` call order in `Engine.cppm`. Reorder those calls and the renderer silently breaks.

**"State" is a grab-bag.** A system's `state` struct mixes:
| What | Lifetime | Mutability |
|------|----------|------------|
| Game logic (physics solver, camera orientation) | Persistent | Mutable in update only |
| Per-frame transient data (render queue, UI draw commands) | Single frame | Produced in update, consumed in frame |
| GPU resource handles (buffers, descriptors, pipelines) | Init to shutdown | Immutable after init |
| Frame lifecycle flags (`frame_begun`) | Single frame | Mechanical, not logic |

These are forced into one struct, requiring `mutable`, `render_state`, and extra phases.

**No DAG composition.** Work queue is flat (one level of batching). Systems can't express multi-step pipelines or inter-system dependencies beyond phase ordering.

### Non-update mutations audit

Every mutation that currently happens outside of `update`:

| System | Phase | What it mutates | Category |
|--------|-------|-----------------|----------|
| Physics | begin_frame | `state.completed_readback`, `state.gpu_stats` | GPU readback from prev frame |
| Renderer | begin_frame | `state.frame_begun = true` | Frame lifecycle flag |
| Renderer | end_frame | `state.frame_begun = false` | Frame lifecycle flag |
| GUI | begin_frame | `fstate`, `sprite_commands`, `hot_widget_id`, layouts | Frame state reset |
| GUI | end_frame | `menu_bar_state`, `tooltip`, `sprite_commands` sort | UI finalization |
| GeometryCollector | prepare_render | GPU buffer memcpy via mapped pointers | GPU-side write through const handle |
| CullCompute | render | GPU buffer writes via mapped pointers | GPU-side write through const handle |
| LightCulling | render | GPU buffer writes via mapped pointers | GPU-side write through const handle |
| SkinCompute | render | GPU buffer writes via mapped pointers | GPU-side write through const handle |
| Input | end_frame | `states.flip()` | Double-buffer swap |
| Actions | end_frame | `states.flip()` | Double-buffer swap |
| RT Shadow | render | `render_state` TLAS rebuild | GPU-side write through const handle |

Every single one of these either (a) belongs in update, (b) is a GPU-side write that doesn't mutate the state struct, or (c) is a mechanical frame-boundary operation the scheduler should handle.

---

## Design Overview

Replace the 7-phase model with a **two-tier execution model**:

```
+-- UPDATE TIER -----------------------------------------------+
|  All systems run update(update_context&, state&)              |
|  DAG execution via task graph                                 |
|  Component access via compile-time access tokens              |
|  This is the ONLY place state is mutated                      |
|                                                               |
|  GPU readback, double-buffer flips, GUI logic -- all here.    |
|  If it mutates state, it's update.                            |
+---------------------------------------------------------------+
                        |
                state snapshot (const)
                        |
+-- FRAME TIER ------------------------------------------------+
|  Systems run frame(frame_context&, const state&)              |
|  DAG execution -- dependencies declared explicitly            |
|  GPU buffer writes through mapped pointers are fine           |
|  (writing to GPU memory != mutating the state struct)         |
|  State is CONST -- no exceptions                              |
+---------------------------------------------------------------+
```

### Key Principles

1. **State is const after update.** No `mutable` hacks, no `render_state`, no phase proliferation. If something needs to mutate state, it belongs in update.

2. **Explicit dependencies, not insertion order.** Frame-tier systems declare `co_await ctx.after<other::state>()` to express ordering. The task graph resolves it. Reordering `add_system()` calls cannot break rendering.

3. **Transient frame data != state.** Render queues, UI draw commands, debug vertices are produced in update and live in the const state snapshot. Frame-tier systems read them.

4. **GPU frame lifecycle is scheduler infrastructure.** `begin_frame()` / `end_frame()` are called by the scheduler directly on the GPU context, not through a system method.

### System Interface

```cpp
struct my_system {
    static auto initialize(init_context&, state&) -> void;
    static auto shutdown(shutdown_context&, state&) -> void;
    static auto update(update_context&, state&) -> async::task<>;
    static auto frame(frame_context&, const state&) -> async::task<>;
};
```

Four methods. Down from seven. Only `update` gets mutable state.

Systems that don't need a particular method simply omit it (detected via concepts, same as today).

---

## New Files

All new files are module partitions of `gse.utility`, placed under `Engine/Engine/Source/Core/Utility/`.

### 1. `Concurrency/AsyncTask.cppm`
**Partition:** `:async_task`

Lazy coroutine type driven by the task graph executor.

```cpp
export module gse.utility:async_task;
import std;
import :non_copyable;

export namespace gse::async {
    template <typename T = void>
    class task : non_copyable {
    public:
        struct promise_type {
            auto initial_suspend() noexcept -> std::suspend_always;
            auto final_suspend() noexcept -> final_awaiter;
            auto get_return_object() -> task;
            auto unhandled_exception() -> void;
            auto return_void() -> void requires std::is_void_v<T>;
            auto return_value(T value) -> void requires (!std::is_void_v<T>);

            static auto operator new(std::size_t size) -> void*;
            static auto operator delete(void*) noexcept -> void;

            std::coroutine_handle<> m_continuation;
            std::optional<T> m_result;
            std::exception_ptr m_exception;
        };

        auto operator co_await() const noexcept -> awaiter;
        auto start() -> void;
        auto done() const -> bool;
        auto result() -> T requires (!std::is_void_v<T>);

        task(task&& other) noexcept;
        ~task();

    private:
        std::coroutine_handle<promise_type> m_handle;
    };

    struct final_awaiter {
        auto await_ready() const noexcept -> bool { return false; }
        template <typename P>
        auto await_suspend(std::coroutine_handle<P> h) noexcept -> std::coroutine_handle<>;
        auto await_resume() noexcept -> void {}
    };

    auto when_all(task<>... tasks) -> task<>;

    template <typename... Ts>
    auto when_all(task<Ts>... tasks) -> task<std::tuple<Ts...>>;

    template <typename T>
    auto sync_wait(task<T> t) -> T;
    auto sync_wait(task<> t) -> void;
}
```

**Implementation notes:**
- `initial_suspend` returns `suspend_always` (lazy). Graph executor calls `start()`.
- `final_suspend` returns `final_awaiter` which resumes the continuation (the coroutine that `co_await`'d this task). If no continuation, returns `noop_coroutine()`.
- `when_all`: allocates a shared `std::atomic<std::size_t>` counter initialized to N. Each task's completion decrements. Last one to decrement resumes the awaiting coroutine. All tasks posted to TBB arena.
- `sync_wait`: creates a `std::binary_semaphore`, sets the task's continuation to release it, starts the task, then `acquire()`s.
- **No sender framework.** `then()`, `bulk()`, `just()`, `schedule()` are intentionally omitted. If P2300 lands in C++26, adopt that instead of maintaining a custom sender library.
- **Frame-arena allocator:** `promise_type::operator new` allocates from a per-frame bump allocator. `operator delete` is a no-op. Arena is bulk-reset at frame boundary by the scheduler. This mitigates MSVC's poor coroutine heap elision (HALO).

### 2. `Concurrency/FrameArena.cppm`
**Partition:** `:frame_arena`

Per-frame bump allocator for coroutine frames.

```cpp
export module gse.utility:frame_arena;
import std;

export namespace gse::async {
    class frame_arena {
    public:
        explicit frame_arena(std::size_t capacity);

        auto allocate(std::size_t size, std::size_t align = alignof(std::max_align_t)) -> void*;
        auto reset() -> void;

        static auto current() -> frame_arena&;
        static auto set_current(frame_arena& arena) -> void;

    private:
        std::vector<std::byte> m_buffer;
        std::atomic<std::size_t> m_offset{0};
    };
}
```

**Implementation notes:**
- `allocate` is lock-free: `m_offset.fetch_add(aligned_size)`. Falls back to `::operator new` if arena is exhausted.
- `current()` uses `thread_local` storage. The scheduler sets it before executing each tier.
- Capacity: start with 1MB, profile and adjust.
- `reset()` sets `m_offset = 0`. Called by scheduler between frames.

### 3. `Ecs/AccessToken.cppm`
**Partition:** `:access_token`

RAII tokens for component access with compile-time read/write tracking.

```cpp
export module gse.utility:access_token;
import std;
import :concepts;
import :registry;

export namespace gse {
    enum class access_mode { read, write };

    template <is_component T, access_mode M = access_mode::read>
    class access {
    public:
        using value_type = std::conditional_t<M == access_mode::read, const T, T>;
        using span_type = std::span<value_type>;

        access(access&& other) noexcept;
        ~access() = default;

        auto begin() const -> value_type*;
        auto end() const -> value_type*;
        auto size() const -> std::size_t;
        auto empty() const -> bool;
        auto operator[](std::size_t i) -> value_type&;
        auto data() -> value_type*;
        auto find(id owner) -> value_type*;

    private:
        friend class registry;
        explicit access(span_type span);

        span_type m_span;
        mutable std::optional<std::unordered_map<id, value_type*>> m_lookup;
        auto build_lookup() -> void;
    };

    template <is_component T> using read  = access<T, access_mode::read>;
    template <is_component T> using write = access<T, access_mode::write>;
}
```

**Key difference from chunk:** No runtime mutex. The task graph's compile-time read/write analysis prevents conflicting access from running concurrently. The DAG already solved the synchronization problem — locks would be redundant overhead.

Debug builds: `registry::acquire_read/write` maintains a `thread_local` set of held access types. Asserts on conflicting concurrent access.

### 4. `Concurrency/TaskGraph.cppm`
**Partition:** `:task_graph`

Per-frame DAG executor that replaces `build_work_batches()`.

```cpp
export module gse.utility:task_graph;
import std;
import :async_task;
import :id;
import :trace;

export namespace gse {
    class task_graph {
    public:
        using node_id = std::size_t;

        auto submit(id name, async::task<> coroutine,
                    std::vector<std::type_index> reads = {},
                    std::vector<std::type_index> writes = {}) -> node_id;

        auto execute() -> void;
        auto clear() -> void;

        auto notify_state_ready(std::type_index state_type) -> void;
        auto wait_state_ready(std::type_index state_type) -> async::task<>;

    private:
        struct graph_node {
            id name;
            async::task<> coroutine;
            std::vector<std::type_index> reads;
            std::vector<std::type_index> writes;
            std::vector<node_id> depends_on;
            std::atomic<bool> completed = false;
        };

        auto build_edges() -> void;

        std::vector<graph_node> m_nodes;

        std::unordered_map<std::type_index, bool> m_state_ready;
        std::unordered_map<std::type_index, std::vector<std::coroutine_handle<>>> m_state_waiters;
        std::mutex m_state_mutex;
    };
}
```

**Edge construction in `build_edges()`:**
For each pair of nodes (A submitted before B):
- A.writes intersect B.writes -> edge A->B (write-write)
- A.writes intersect B.reads -> edge A->B (write-read)
- A.reads intersect B.writes -> edge A->B (read-write)
- A.reads intersect B.reads -> no edge (parallel reads OK)

**Execution of `execute()`:**
1. `build_edges()` from read/write type sets
2. Compute in-degree for each node
3. Enqueue zero-in-degree nodes into ready queue
4. While ready queue not empty:
   a. Pop node, start its coroutine on TBB arena via `task::post`
   b. On coroutine completion: mark done, decrement dependents' in-degrees
   c. Enqueue any dependent hitting zero in-degree
5. Wait for all nodes (atomic counter or `sync_wait`)

**`wait_state_ready` / `notify_state_ready`:**
- Frame-tier systems call `co_await ctx.after<T>()` which calls `wait_state_ready(typeid(T))`
- If the owning system's frame work already called `notify_state_ready`, resumes immediately
- Otherwise suspends the coroutine and registers it as a waiter
- When `notify_state_ready` fires, all waiters are resumed via TBB arena
- **Static cycle detection:** At `add_system()` time, the scheduler collects declared `after<T>` dependencies and asserts acyclicity. Deadlocks detected at startup, not at runtime.

### 5. `Ecs/UpdateContext.cppm`
**Partition:** `:update_context`

Mutable-state context for the update tier.

```cpp
export module gse.utility:update_context;
import std;
import :async_task;
import :access_token;
import :registry;
import :phase_context;
import :task_graph;

export namespace gse {
    struct update_context : phase_gpu_access {
        registry& reg;
        const state_snapshot_provider& snapshots;
        channel_writer& channels;
        const channel_reader_provider& channel_reader;
        task_graph& graph;

        template <is_component T>
        auto read() -> gse::read<T>;

        template <is_component T>
        auto write() -> gse::write<T>;

        template <typename State>
        auto state_of() const -> const State&;

        template <typename State>
        auto try_state_of() const -> const State*;

        template <typename T>
        auto read_channel() const -> channel_read_guard<T>;

        template <typename F>
        auto schedule(
            F&& action,
            std::source_location loc = std::source_location::current()
        ) -> void;

        template <is_component T, typename... Args>
        auto defer_add(id entity, Args&&... args) -> void;

        template <is_component T>
        auto defer_remove(id entity) -> void;

        auto defer_activate(id entity) -> void;
    };
}
```

`schedule()` uses `access_traits` to extract reads/writes from the lambda's parameters (same compile-time introspection as the current `chunk_traits`), then submits to the task graph with the extracted type sets.

### 6. `Ecs/FrameContext.cppm`
**Partition:** `:frame_context`

Const-state context for the frame tier.

```cpp
export module gse.utility:frame_context;
import std;
import :async_task;
import :phase_context;
import :task_graph;

export namespace gse {
    struct frame_context : phase_gpu_access {
        const state_snapshot_provider& snapshots;
        const channel_reader_provider& channel_reader;
        task_graph& graph;

        template <typename State>
        auto state_of() const -> const State&;

        template <typename State>
        auto try_state_of() const -> const State*;

        template <typename T>
        auto read_channel() const -> channel_read_guard<T>;

        template <typename State>
        auto after() -> async::task<>;
    };
}
```

`after<State>()` suspends until the system owning `State` has completed its frame work and called `notify_state_ready`. This is how frame-tier systems express ordering. The dependency is explicit and verifiable at startup.

---

## Files to Modify

### 7. `Ecs/Registry.cppm`

Add access token acquisition methods:

```cpp
template <is_component T>
auto acquire_read() -> read<T>;

template <is_component T>
auto acquire_write() -> write<T>;
```

These return `access` tokens wrapping spans over the component arrays. No mutex. Debug builds assert no conflicting concurrent access.

### 8. `Ecs/SystemNode.cppm`

Replace 7 virtual methods with 4:

```cpp
class system_node_base {
public:
    virtual ~system_node_base() = default;
    virtual auto initialize(init_context&) -> void = 0;
    virtual auto shutdown(shutdown_context&) -> void = 0;
    virtual auto update(update_context&) -> async::task<> = 0;
    virtual auto frame(frame_context&) -> async::task<> = 0;
    virtual auto state_ptr() -> void* = 0;
    virtual auto state_ptr() const -> const void* = 0;
    virtual auto state_type() const -> std::type_index = 0;
    virtual auto trace_id() const -> id = 0;
};
```

**`system_node<S, State>` adapter (no more `RenderState` param):**
- If `S::update` returns `async::task<>`: calls it directly
- If `S::update` returns `void`: wraps in a trivially-completed coroutine (backward compat during migration)
- Same pattern for `frame`
- If `S` has no `frame` method: returns an already-completed task

**New concepts:**
```cpp
template <typename S, typename State>
concept has_update = requires(update_context& ctx, State& s) {
    { S::update(ctx, s) } -> std::same_as<async::task<>>;
};

template <typename S, typename State>
concept has_update_sync = requires(update_context& ctx, State& s) {
    { S::update(ctx, s) } -> std::same_as<void>;
};

template <typename S, typename State>
concept has_frame = requires(frame_context& ctx, const State& s) {
    { S::frame(ctx, s) } -> std::same_as<async::task<>>;
};

template <typename S, typename State>
concept has_initialize = requires(init_context& ctx, State& s) {
    { S::initialize(ctx, s) } -> std::same_as<void>;
};

template <typename S, typename State>
concept has_shutdown = requires(shutdown_context& ctx, State& s) {
    { S::shutdown(ctx, s) } -> std::same_as<void>;
};
```

### 9. `Concurrency/Scheduler.cppm`

Replace phase loop with two-tier execution:

```cpp
auto scheduler::run() -> void {
    drain_deferred();

    update_context u_ctx{
        .reg = *m_registry,
        .snapshots = *this,
        .channels = make_channel_writer(),
        .channel_reader = *this,
        .graph = m_update_graph
    };
    u_ctx.gpu_ctx = m_gpu_ctx;

    for (auto& node : m_nodes) {
        auto coro = node->update(u_ctx);
        m_update_graph.submit(node->trace_id(), std::move(coro));
    }

    m_update_graph.execute();
    m_update_graph.clear();
    snapshot_all_channels();

    if (!gpu_begin_frame()) return;

    frame_context f_ctx{
        .snapshots = *this,
        .channel_reader = *this,
        .graph = m_frame_graph
    };
    f_ctx.gpu_ctx = m_gpu_ctx;

    for (auto& node : m_nodes) {
        auto coro = node->frame(f_ctx);
        m_frame_graph.submit(node->trace_id(), std::move(coro));
    }

    m_frame_graph.execute();
    m_frame_graph.clear();

    gpu_end_frame();
    m_arena.reset();
}
```

**`gpu_begin_frame()` / `gpu_end_frame()`:**
Scheduler calls these directly on the GPU context. This was previously the Renderer system's `begin_frame`/`end_frame` responsibility. Moving it to the scheduler means the frame gate is infrastructure, not a system concern.

**Removals:**
- `build_work_batches()` — replaced by task graph edge construction
- `work_queue`, `queued_work` — replaced by access-token task graph
- Separate `render()` method — merged into `run()`

**New members:**
```cpp
task_graph m_update_graph;
task_graph m_frame_graph;
async::frame_arena m_arena;
```

### 10. `Ecs/PhaseContext.cppm`

**Removals:**
- `update_phase`, `begin_frame_phase`, `prepare_render_phase`, `render_phase`, `end_frame_phase`, `shutdown_phase`, `initialize_phase`
- `work_queue`, `queued_work`
- `chunk<T>`
- All `has_*` concepts referencing old phase types

**Keep:**
- `state_snapshot_provider` — used by both contexts
- `channel_writer`, `channel_reader_provider` — used by both contexts
- `phase_gpu_access` — used by both contexts
- `registry_access` — used by init/shutdown contexts

### 11. `Meta/LambdaTraits.cppm`

Rename `chunk_traits` to `access_traits`:

```cpp
template <typename T>
struct access_traits {
    static constexpr bool is_access = false;
    static constexpr bool is_const_element = false;
    using element_type = void;
};

template <is_component T, access_mode M>
struct access_traits<access<T, M>> {
    static constexpr bool is_access = true;
    static constexpr bool is_const_element = (M == access_mode::read);
    using element_type = T;
};

template <typename T>
constexpr bool is_access_v = access_traits<std::remove_cvref_t<T>>::is_access;

template <typename T>
constexpr bool is_read_access_v = access_traits<std::remove_cvref_t<T>>::is_const_element;

template <typename T>
using access_element_t = access_traits<std::remove_cvref_t<T>>::element_type;
```

`param_info::reads()` / `writes()` updated to use `access_traits` instead of `chunk_traits`. Same compile-time mechanism.

### 12. `Engine/Import/Utility.cppm`

Add new partition exports:
```cpp
export import :async_task;
export import :frame_arena;
export import :access_token;
export import :update_context;
export import :frame_context;
export import :task_graph;
```

---

## Before / After Examples

### Camera System
Simplest case. Update only, no frame work.

**BEFORE:**
```cpp
struct system {
    static auto initialize(initialize_phase& phase, state& s) -> void;
    static auto update(update_phase& phase, state& s) -> void;
};

auto system::update(update_phase& phase, state& s) -> void {
    if (auto focus = phase.read_channel<ui_focus_update>(); !focus.empty()) {
        s.ui_has_focus = focus.back().has_focus;
    }

    if (auto vp = phase.read_channel<viewport_update>(); !vp.empty()) {
        s.viewport = vp.back().size;
    }

    auto* input = phase.try_state_of<input::system_state>();
    if (input && !s.ui_has_focus) {
        auto delta = input->read().mouse_delta;
        s.yaw += delta.x() * s.mouse_sensitivity;
        s.pitch = std::clamp(s.pitch + delta.y() * s.mouse_sensitivity, -89.f, 89.f);
    }

    auto new_orientation = from_euler(degrees(s.pitch), degrees(s.yaw), degrees(0.f));
    auto dt = system_clock::delta();

    phase.schedule([&s, new_orientation, dt](chunk<follow_component> cameras) {
        for (const auto& cam : cameras) {
            if (cam.priority > s.best_priority) {
                s.best_priority = cam.priority;
                s.target_position = cam.target_position;
            }
        }
        s.orientation = slerp(s.orientation, new_orientation, dt * s.blend_speed);
        s.view_matrix = look_at(s.position, s.position + rotate_vector(s.orientation, vec3_forward));
        s.projection_matrix = perspective(s.fov, s.viewport.x() / s.viewport.y(), s.near, s.far);
    });
}
```

**AFTER:**
```cpp
struct system {
    static auto initialize(init_context& ctx, state& s) -> void;
    static auto update(update_context& ctx, state& s) -> async::task<>;
};

auto system::update(update_context& ctx, state& s) -> async::task<> {
    if (auto focus = ctx.read_channel<ui_focus_update>(); !focus.empty()) {
        s.ui_has_focus = focus.back().has_focus;
    }

    if (auto vp = ctx.read_channel<viewport_update>(); !vp.empty()) {
        s.viewport = vp.back().size;
    }

    auto* input = ctx.try_state_of<input::system_state>();
    if (input && !s.ui_has_focus) {
        auto delta = input->read().mouse_delta;
        s.yaw += delta.x() * s.mouse_sensitivity;
        s.pitch = std::clamp(s.pitch + delta.y() * s.mouse_sensitivity, -89.f, 89.f);
    }

    auto new_orientation = from_euler(degrees(s.pitch), degrees(s.yaw), degrees(0.f));
    auto dt = system_clock::delta();

    ctx.schedule([&s, new_orientation, dt](read<follow_component> cameras) {
        for (const auto& cam : cameras) {
            if (cam.priority > s.best_priority) {
                s.best_priority = cam.priority;
                s.target_position = cam.target_position;
            }
        }
        s.orientation = slerp(s.orientation, new_orientation, dt * s.blend_speed);
        s.view_matrix = look_at(s.position, s.position + rotate_vector(s.orientation, vec3_forward));
        s.projection_matrix = perspective(s.fov, s.viewport.x() / s.viewport.y(), s.near, s.far);
    });

    co_return;
}
```

**What changed:** `update_phase` -> `update_context`, `chunk<T>` -> `read<T>`, returns `async::task<>` with `co_return`. No `frame()` needed.

---

### Input System
Double-buffer flip moves to update. `end_frame` eliminated.

**BEFORE:**
```cpp
struct system {
    static auto update(update_phase& phase, system_state& s) -> void;
    static auto end_frame(end_frame_phase& phase, system_state& s) -> void;
};

auto system::update(update_phase& phase, system_state& s) -> void {
    std::lock_guard lock(s.mutex);
    auto& w = s.states.write();
    for (auto& ev : s.queue) { /* process into w */ }
    s.queue.clear();
}

auto system::end_frame(end_frame_phase&, system_state& s) -> void {
    s.states.flip();
}
```

**AFTER:**
```cpp
struct system {
    static auto update(update_context& ctx, system_state& s) -> async::task<>;
};

auto system::update(update_context& ctx, system_state& s) -> async::task<> {
    s.states.flip();

    std::lock_guard lock(s.mutex);
    auto& w = s.states.write();
    for (auto& ev : s.queue) { /* process into w */ }
    s.queue.clear();

    co_return;
}
```

**What changed:** Flip moves to start of update. Snapshot taken after update completes, so all systems reading input get the freshly processed state. `end_frame` deleted entirely.

---

### Physics System
GPU readback moves to update. `render_state` eliminated.

**BEFORE:**
```cpp
struct system {
    static auto update(update_phase& phase, state& s) -> void;
    static auto begin_frame(begin_frame_phase& phase, state& s, render_state& rs) -> bool;
    static auto render(render_phase& phase, const state& s, render_state& rs) -> void;
};

auto system::begin_frame(begin_frame_phase& phase, state& s, render_state& rs) -> bool {
    if (rs.completed) {
        s.completed_readback = std::move(rs.completed);
        rs.completed.reset();
        s.gpu_stats = { rs.gpu_solver.iteration_count(), /* ... */ };
    }
    return true;
}

auto system::update(update_phase& phase, state& s) -> void {
    auto dt = system_clock::delta();
    s.accumulator += dt;

    phase.schedule([&](chunk<motion_component> motion, chunk<collision_component> collision) {
        update_vbd_gpu(steps, s, motion, collision);
        for (auto& mc : motion) {
            mc.render_position = lerp(mc.prev_position, mc.position, alpha);
        }
    });
}

auto system::render(render_phase& phase, const state& s, render_state& rs) -> void {
    auto& gpu = phase.get<gpu::context>();
    rs.gpu_solver.dispatch(gpu, s.solver_data);
    if (rs.in_flight && rs.in_flight->fence_signaled()) {
        rs.completed = read_back(rs.in_flight->buffer);
        rs.in_flight.reset();
    }
}
```

**AFTER:**
```cpp
struct system {
    static auto update(update_context& ctx, state& s) -> async::task<>;
    static auto frame(frame_context& ctx, const state& s) -> async::task<>;
};

auto system::update(update_context& ctx, state& s) -> async::task<> {
    if (s.pending_readback && s.pending_readback->fence_signaled()) {
        s.completed_readback = read_back(s.pending_readback->buffer);
        s.pending_readback.reset();
        s.gpu_stats = { s.gpu_solver.iteration_count(), /* ... */ };
    }

    auto dt = system_clock::delta();
    s.accumulator += dt;

    ctx.schedule([&](write<motion_component> motion, write<collision_component> collision) {
        update_vbd_gpu(steps, s, motion, collision);
        for (auto& mc : motion) {
            mc.render_position = lerp(mc.prev_position, mc.position, alpha);
        }
    });

    co_return;
}

auto system::frame(frame_context& ctx, const state& s) -> async::task<> {
    if (!s.use_gpu_solver) co_return;

    auto& gpu = ctx.get<gpu::context>();
    s.gpu_solver.dispatch(gpu, s.solver_data);

    co_return;
}
```

**What changed:** `render_state` eliminated. `gpu_solver` and `pending_readback` move into `state`. GPU readback check (was `begin_frame`) moves to start of `update` — it's just "incorporate previous frame's results." `frame()` only dispatches GPU work through const handles.

---

### Geometry Collector
`prepare_render` becomes `frame` with `notify_state_ready`.

**BEFORE:**
```cpp
struct system {
    static auto initialize(initialize_phase& phase, state& s) -> void;
    static auto update(update_phase& phase, state& s) -> void;
    static auto prepare_render(prepare_render_phase& phase, state& s) -> void;
};

auto system::update(update_phase& phase, state& s) -> void {
    phase.schedule([&](
        chunk<render_component> render,
        chunk<const physics::motion_component> motion,
        chunk<const physics::collision_component> collision,
        chunk<const animation_component> anim
    ) {
        s.render_queue.clear();
        for (auto& rc : render) { /* build render queue */ }
    });
}

auto system::prepare_render(prepare_render_phase& phase, state& s) -> void {
    auto frame_index = /* ... */;
    gse::memcpy(s.instance_buffer[frame_index].mapped(), s.render_queue.data(), /* ... */);
    gse::memcpy(s.normal_indirect_commands_buffer[frame_index].mapped(), /* ... */);
    gse::memcpy(s.skinned_indirect_commands_buffer[frame_index].mapped(), /* ... */);
    gse::memcpy(s.local_pose_buffer[frame_index].mapped(), /* ... */);
}
```

**AFTER:**
```cpp
struct system {
    static auto initialize(init_context& ctx, state& s) -> void;
    static auto update(update_context& ctx, state& s) -> async::task<>;
    static auto frame(frame_context& ctx, const state& s) -> async::task<>;
};

auto system::update(update_context& ctx, state& s) -> async::task<> {
    ctx.schedule([&](
        write<render_component> render,
        read<physics::motion_component> motion,
        read<physics::collision_component> collision,
        read<animation_component> anim
    ) {
        s.render_queue.clear();
        for (auto& rc : render) { /* build render queue */ }
    });

    co_return;
}

auto system::frame(frame_context& ctx, const state& s) -> async::task<> {
    auto frame_index = /* ... */;
    gse::memcpy(s.instance_buffer[frame_index].mapped(), s.render_queue.data(), /* ... */);
    gse::memcpy(s.normal_indirect_commands_buffer[frame_index].mapped(), /* ... */);
    gse::memcpy(s.skinned_indirect_commands_buffer[frame_index].mapped(), /* ... */);
    gse::memcpy(s.local_pose_buffer[frame_index].mapped(), /* ... */);

    ctx.graph.notify_state_ready(std::type_index(typeid(state)));
    co_return;
}
```

**What changed:** `prepare_render` -> `frame`. State is const — `memcpy` writes through the mapped pointer (GPU-side write), not a state mutation. Calls `notify_state_ready` so downstream systems (cull, skin, depth) unblock.

---

### Cull Compute Renderer
`render` becomes `frame` with explicit dependency.

**BEFORE:**
```cpp
struct system {
    static auto render(render_phase& phase, const state& s) -> void;
};

auto system::render(render_phase& phase, const state& s) -> void {
    auto& geo = phase.state_of<geometry_collector::state>();
    auto& cam = phase.state_of<camera::state>();
    auto& gpu = phase.get<gpu::context>();
    /* dispatch compute using geo.instance_buffer */
}
```

**AFTER:**
```cpp
struct system {
    static auto frame(frame_context& ctx, const state& s) -> async::task<>;
};

auto system::frame(frame_context& ctx, const state& s) -> async::task<> {
    co_await ctx.after<geometry_collector::state>();

    auto& geo = ctx.state_of<geometry_collector::state>();
    auto& cam = ctx.state_of<camera::state>();
    auto& gpu = ctx.get<gpu::context>();
    /* dispatch compute using geo.instance_buffer */
}
```

**What changed:** Dependency on geometry_collector is explicit via `co_await ctx.after<>()`. No longer relies on `add_system()` call order in `Engine.cppm`. If someone reorders system registration, this still works.

---

### Forward Renderer
Multiple explicit dependencies.

**BEFORE:**
```cpp
struct system {
    static auto render(render_phase& phase, const state& s) -> void;
};

auto system::render(render_phase& phase, const state& s) -> void {
    auto& geo = phase.state_of<geometry_collector::state>();
    auto& lc = phase.state_of<light_culling::state>();
    auto& rt = phase.state_of<rt_shadow::state>();
    auto& gpu = phase.get<gpu::context>();
    /* forward shading pass */
}
```

**AFTER:**
```cpp
struct system {
    static auto frame(frame_context& ctx, const state& s) -> async::task<>;
};

auto system::frame(frame_context& ctx, const state& s) -> async::task<> {
    co_await async::when_all(
        ctx.after<light_culling::state>(),
        ctx.after<rt_shadow::state>(),
        ctx.after<depth_prepass::state>()
    );

    auto& geo = ctx.state_of<geometry_collector::state>();
    auto& lc = ctx.state_of<light_culling::state>();
    auto& rt = ctx.state_of<rt_shadow::state>();
    auto& gpu = ctx.get<gpu::context>();
    /* forward shading pass */
}
```

**What changed:** `when_all` on multiple dependencies — forward renderer waits for light culling, RT shadows, and depth prepass to all complete before proceeding. These three can run in parallel with each other.

---

### GUI System (Option A — all logic in update)

**BEFORE (3 phase methods, 15+ mutable fields):**
```cpp
struct system {
    static auto initialize(initialize_phase&, system_state&) -> void;
    static auto update(update_phase&, system_state&) -> void;
    static auto begin_frame(begin_frame_phase&, system_state&) -> bool;
    static auto render(render_phase&, const system_state&) -> void;
    static auto end_frame(end_frame_phase&, system_state&) -> void;
    static auto shutdown(shutdown_phase&, system_state&) -> void;
};

auto system::begin_frame(begin_frame_phase& phase, system_state& s) -> bool {
    s.fstate = {};
    s.sprite_commands.clear();
    s.text_commands.clear();
    s.input_layers_data.begin_frame();
    s.hot_widget_id = {};
    /* ... layout recalculation, 40+ lines ... */
    return true;
}

auto system::update(update_phase& phase, system_state& s) -> void {
    /* ... input handling, widget interactions ... */
}

auto system::end_frame(end_frame_phase& phase, system_state& s) -> void {
    menu_bar::update(s.menu_bar_state, /* ... */);
    settings_panel::update(/* ... */);
    /* ... tooltip logic, draw command sorting, 100+ lines ... */
    sort_by_layer(s.sprite_commands);
    sort_by_layer(s.text_commands);
}

auto system::render(render_phase&, const system_state&) -> void {}
```

**AFTER (2 methods, no mutable fields):**
```cpp
struct system {
    static auto initialize(init_context&, system_state&) -> void;
    static auto update(update_context&, system_state&) -> async::task<>;
    static auto shutdown(shutdown_context&, system_state&) -> void;
};

auto system::update(update_context& ctx, system_state& s) -> async::task<> {
    s.fstate = {};
    s.sprite_commands.clear();
    s.text_commands.clear();
    s.input_layers_data.begin_frame();
    s.hot_widget_id = {};
    /* ... former begin_frame layout logic ... */

    auto* input = ctx.try_state_of<input::system_state>();
    /* ... former update logic -- input, widget interactions ... */

    menu_bar::update(s.menu_bar_state, /* ... */);
    settings_panel::update(/* ... */);
    /* ... former end_frame logic -- tooltips, sorting ... */
    sort_by_layer(s.sprite_commands);
    sort_by_layer(s.text_commands);

    co_return;
}
```

**What changed:** `begin_frame` + `update` + `end_frame` collapse into one `update`. `sprite_commands` and `text_commands` are now part of the const state snapshot that `ui_renderer::frame()` reads. No `mutable` needed. The no-op `render()` is deleted.

---

### Renderer Base System
`begin_frame`/`end_frame` become scheduler infrastructure.

**BEFORE:**
```cpp
struct system {
    static auto initialize(const initialize_phase&, state&) -> void;
    static auto update(const update_phase&, state&) -> void;
    static auto begin_frame(const begin_frame_phase&, state&) -> bool;
    static auto end_frame(const end_frame_phase&, state&) -> void;
    static auto shutdown(const shutdown_phase&, state&) -> void;
};

auto system::begin_frame(const begin_frame_phase&, state& s) -> bool {
    auto& ctx = /* ... */;
    auto result = ctx.begin_frame();
    s.frame_begun = result.has_value();
    return s.frame_begun;
}

auto system::end_frame(const end_frame_phase&, state& s) -> void {
    auto& ctx = /* ... */;
    ctx.end_frame();
    ctx.finalize_reloads();
    s.frame_begun = false;
}
```

**AFTER:**
```cpp
struct system {
    static auto initialize(init_context&, state&) -> void;
    static auto update(update_context&, state&) -> async::task<>;
    static auto shutdown(shutdown_context&, state&) -> void;
};

auto system::update(update_context& ctx, state& s) -> async::task<> {
    auto& gpu = ctx.get<gpu::context>();
    gpu.poll_assets();
    gpu.process_queue();
    gpu.update_window();
    gpu.finalize_reloads();

    co_return;
}
```

**What changed:** `begin_frame`/`end_frame` deleted entirely. The scheduler calls `gpu::context::begin_frame()` and `gpu::context::end_frame()` directly as frame tier infrastructure. `frame_begun` flag is no longer part of system state. Asset polling and reload finalization move to update.

---

## Migration Order

### Step 1: `async::task<T>` + `when_all` + `sync_wait` + `frame_arena`
**Files:** `AsyncTask.cppm`, `FrameArena.cppm`
**Test:** Create tasks, verify completion, exception propagation, `when_all` parallelism, arena allocation and reset.
**Dependencies:** None

### Step 2: Access tokens + registry changes
**Files:** `AccessToken.cppm`, `Registry.cppm`
**Test:** `read<T>` returns valid span. `write<T>` returns mutable span. Debug assertions catch conflicts.
**Dependencies:** None

### Step 3: `task_graph`
**File:** `TaskGraph.cppm`
**Test:** Submit coroutines with conflicting read/write sets. Verify parallel reads, serialized writes. `state_ready` suspend/resume.
**Dependencies:** Step 1

### Step 4: `update_context` + `frame_context` + `LambdaTraits` changes
**Files:** `UpdateContext.cppm`, `FrameContext.cppm`, `LambdaTraits.cppm`
**Dependencies:** Steps 1, 2, 3

### Step 5: `system_node` + `scheduler` changes (with backward compat shim)
**Files:** `SystemNode.cppm`, `Scheduler.cppm`
**Shim:** Systems returning `void` auto-wrapped in completed task. Allows incremental migration.
**Test:** Engine boots and runs a frame with new scheduler loop.
**Dependencies:** Step 4

### Step 6: Trivial system migrations
**Systems:** Camera, Animation, Audio, SaveSystem
**Change:** Phase type renames, `chunk` -> `read`/`write`, add `co_return`. No structural changes.
**Dependencies:** Step 5

### Step 7: Double-buffer systems
**Systems:** Input, Actions
**Change:** Move flip to start of update. Delete `end_frame`.
**Dependencies:** Step 5

### Step 8: GUI restructure
**System:** GUI
**Change:** Merge `begin_frame` + `update` + `end_frame` into one `update`. Remove all `mutable` from state. Delete no-op `render`.
**Dependencies:** Step 5

### Step 9: Renderer base system
**System:** Renderer
**Change:** Move `begin_frame`/`end_frame` to scheduler infrastructure. Keep asset polling in update.
**Dependencies:** Step 5

### Step 10: Physics system
**System:** Physics
**Change:** Move GPU readback to update. Eliminate `render_state`. `gpu_solver` and `pending_readback` move into `state`. Add `frame()` for GPU dispatch.
**Dependencies:** Step 5

### Step 11: Geometry Collector + downstream renderers
**Systems:** GeometryCollector, SkinCompute, CullCompute, DepthPrepass, LightCulling, RtShadow, ForwardRenderer, PhysicsDebug, UiRenderer
**Change:** `prepare_render`/`render` -> `frame` with explicit `after<>` dependencies. Verify the explicit DAG matches the old implicit ordering.
**Dependencies:** Steps 9, 10

### Step 12: Remove legacy code
- Delete all old phase types, `work_queue`, `queued_work`, `build_work_batches()`, `chunk<T>`, `chunk_traits`
- Delete `RenderState` template parameter and `render_state_storage` from `system_node`
- Delete `prepare_render` virtual from `system_node_base`
- Remove backward compat shim (void -> task wrapper)
- Clean up `PhaseContext.cppm` to only shared infrastructure

---

## Verification

### Runtime
1. Engine boots, window opens, renders correctly
2. Physics simulation: objects fall, collide, GPU solver produces correct results
3. Camera: follows entities, mouse look, blending between controllers
4. Skeletal animations play
5. No Vulkan validation layer errors

### Concurrency
- Debug assertions on access token conflicts fire when DAG edges are wrong
- No data races (Application Verifier / TSan if available)
- No deadlocks (static cycle detection catches `after<>` cycles at startup)

### Performance
- Compare frame times before/after
- Trace output shows overlapping execution in update tier (camera + physics + animation can overlap)
- Trace output shows frame tier parallelism where no `after<>` edges exist (rt_shadow parallel with cull_compute -> depth_prepass -> light_culling)

### Regression
- All existing behavior preserved: entity lifecycle, channel messaging, deferred operations, hot reload, GPU readback
- System registration order in `Engine.cppm` can be shuffled without breaking rendering (explicit `after<>` handles ordering)

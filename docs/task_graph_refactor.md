# Path B: Task Graph with Access Tokens, Coroutines, and Parallel Render

## Context

GSEngine's current concurrency model relies on **phase-rigid temporal separation**: the frame is split into fixed phases (update, render, end_frame), and thread safety comes from structural guarantees (double-buffered registry, work queue conflict analysis on `std::type_index`, sequential render). This works but creates limitations:

- No way to compose async work with results flowing between steps
- Work queue is flat (one level of batching per frame, no DAGs)
- Systems can't express multi-step pipelines (broad phase -> narrow phase -> solve)
- No incentive to create new jobs outside the scheduler's rigid phase model
- Opening async work (asset loading, GPU readback, network) requires manual synchronization

This refactor replaces the phase-rigid model with a **task graph** backed by **per-component access tokens**, **coroutine-based system methods**, and **parallel command buffer recording**. Inspired by std::execution (P2300) concepts: senders as composable lazy work descriptions, schedulers as execution context handles, structured concurrency.

---

## Files to Create (New Modules)

All new files are module partitions of `gse.utility`, placed under `Engine/Engine/Source/Core/Utility/`.

### 1. `Concurrency/AsyncTask.cppm`
**Partition:** `:async_task`

The coroutine type that all system methods return. A lazy coroutine whose frame is driven by the task graph executor.

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
}
```

**Implementation notes:**
- `initial_suspend` returns `suspend_always` (lazy). The graph executor calls `start()` -> `m_handle.resume()`.
- `final_suspend` returns `final_awaiter` which resumes the continuation (the coroutine that `co_await`'d this task). If no continuation, returns `noop_coroutine()`.
- `task<void>` specialization: `promise_type` uses `return_void()`, no `m_result` storage.
- Exception propagation: `unhandled_exception()` stores `std::current_exception()`. Rethrown when `result()` is called or when the awaiting coroutine resumes.
- TBB integration: when the executor resumes a coroutine, it does so inside `arena->execute(...)` so TBB's work-stealing is active.

### 2. `Concurrency/Sender.cppm`
**Partition:** `:sender`

Minimal sender/receiver framework for composable async. These wrap around `task<T>` for interop.

```cpp
export module gse.utility:sender;
import std;
import :async_task;
import :task;
import :trace;

export namespace gse::async {
    auto schedule() -> task<>;

    template <typename... Ts>
    auto just(Ts&&... vals) -> task<std::tuple<Ts...>>;

    template <typename T>
    auto just(T&& val) -> task<T>;

    template <typename T, typename F>
    auto then(task<T> input, F&& func) -> task<std::invoke_result_t<F, T>>;

    template <typename... Ts>
    auto when_all(task<Ts>... tasks) -> task<std::tuple<Ts...>>;

    template <typename A, typename B>
    auto when_all(task<A> a, task<B> b) -> task<std::tuple<A, B>>;

    auto when_all(task<>... tasks) -> task<>;

    template <typename T, typename F>
    auto bulk(task<T> input, std::size_t count, F&& func) -> task<T>;

    template <typename T>
    auto sync_wait(task<T> t) -> T;
    auto sync_wait(task<> t) -> void;
}
```

**Implementation notes:**
- `when_all` implementation: allocates a shared atomic counter initialized to N. Each task's final_awaiter decrements. The last one to decrement resumes the awaiting coroutine. All tasks are started via `task::post()` into the TBB arena.
- `sync_wait` implementation: creates a `std::binary_semaphore`, sets the task's continuation to release it, starts the task, then `acquire()`s the semaphore. Used at frame scope boundaries.
- `then` is syntactic sugar: `then(t, f)` is equivalent to `[](auto t, auto f) -> task<R> { auto v = co_await t; co_return f(v); }(std::move(t), std::forward<F>(f))`.
- `bulk` uses `task::parallel_for` internally. The input task's result is passed through unchanged after all bulk iterations complete.

### 3. `Ecs/AccessToken.cppm`
**Partition:** `:access_token`

RAII access tokens that acquire per-component locks from the registry.

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
        ~access();

        auto begin() const -> value_type*;
        auto end() const -> value_type*;
        auto size() const -> std::size_t;
        auto empty() const -> bool;
        auto operator[](std::size_t i) -> value_type&;
        auto data() -> value_type*;

        auto find(id owner) -> value_type*;

    private:
        friend class registry;
        access(span_type span, std::shared_mutex& mutex, access_mode mode);

        span_type m_span;
        std::shared_mutex* m_mutex;
        bool m_owns_lock = false;
        std::variant<std::shared_lock<std::shared_mutex>,
                     std::unique_lock<std::shared_mutex>> m_lock;

        mutable std::optional<std::unordered_map<id, value_type*>> m_lookup;
        auto build_lookup() -> void;
    };

    template <is_component T> using read  = access<T, access_mode::read>;
    template <is_component T> using write = access<T, access_mode::write>;

    struct entity_range {
        std::size_t offset = 0;
        std::size_t count = 0;

        auto overlaps(const entity_range& other) const -> bool;
    };

    template <is_component T>
    class partitioned_write {
    public:
        using value_type = T;

        partitioned_write(partitioned_write&& other) noexcept;
        ~partitioned_write();

        auto begin() -> T*;
        auto end() -> T*;
        auto size() const -> std::size_t;
        auto range() const -> entity_range;

    private:
        friend class registry;
        partitioned_write(std::span<T> span, entity_range range, range_lock_table& table);

        std::span<T> m_span;
        entity_range m_range;
        range_lock_table* m_table;
    };

    class range_lock_table {
    public:
        auto try_lock(entity_range range) -> bool;
        auto unlock(entity_range range) -> void;
        auto wait_and_lock(entity_range range) -> void;

    private:
        struct locked_range {
            entity_range range;
            std::thread::id owner;
        };
        std::vector<locked_range> m_locked;
        std::mutex m_mutex;
        std::condition_variable m_cv;
    };
}
```

**Implementation notes for access token construction:**
- `registry::acquire_read<T>()` -> takes `shared_lock` on `component_slot::type_mutex`, returns `access<T, read>` with span over read buffer.
- `registry::acquire_write<T>()` -> takes `unique_lock` on `component_slot::type_mutex`, returns `access<T, write>` with span over write buffer.
- Locks are held for the lifetime of the access token (RAII).
- Move constructor transfers lock ownership. Destructor releases.
- `range_lock_table`: stores a `vector<locked_range>`. `try_lock` scans for overlaps, fails if any found. `wait_and_lock` uses `condition_variable` to block until the conflicting range is released. `unlock` erases the range and notifies all waiters.
- The `entity_range` is in terms of the component array's contiguous indices, not entity IDs. Systems that want range-partitioned access call `phase.partition<T>(n)` which splits the total count into n chunks.

### 4. `Ecs/GraphPhase.cppm`
**Partition:** `:graph_phase`

Replaces all old phase types with a unified phase that submits work to the task graph.

```cpp
export module gse.utility:graph_phase;
import std;
import :async_task;
import :access_token;
import :registry;
import :phase_context;
import :channel_base;
import :task_graph;

export namespace gse {
    class task_graph;

    struct graph_phase : phase_gpu_access {
        registry& reg;
        const state_snapshot_provider& snapshots;
        const channel_reader_provider& channel_reader;
        channel_writer& channels;
        task_graph& graph;

        template <is_component T>
        auto read() -> async::task<gse::read<T>>;

        template <is_component T>
        auto write() -> async::task<gse::write<T>>;

        template <is_component T>
        auto partition(std::size_t partition_count)
            -> async::task<std::vector<partitioned_write<T>>>;

        template <typename F>
        auto submit(id name, F&& action) -> async::task</*return type of F*/>;

        template <typename F>
        auto submit_detached(id name, F&& action) -> void;

        template <typename State>
        auto state_ready() -> async::task<const State*>;

        template <typename State>
        auto try_state_of() const -> const State*;

        template <typename State>
        auto state_of() const -> const State&;

        template <typename T>
        auto read_channel() const -> channel_read_guard<T>;

        auto record_parallel(
            std::move_only_function<void(gpu::recording_context&)> fn
        ) -> async::task<>;

        template <is_component T, typename... Args>
        auto defer_add(id entity, Args&&... args) -> void;

        template <is_component T>
        auto defer_remove(id entity) -> void;

        auto defer_activate(id entity) -> void;
    };
}
```

**Implementation notes:**

`read<T>()` implementation:
```
1. Check if type_mutex for T is available for shared lock
2. If available: acquire shared_lock, build access<T, read> from registry read buffer, return
3. If blocked (exclusive lock held): suspend coroutine, register wakeup callback on the mutex
4. When lock released: callback enqueues coroutine resume on TBB arena
5. Coroutine resumes, acquires shared_lock, returns access token
```

`submit(name, action)` implementation:
```
1. Inspect F's parameter types using lambda_traits (extended to recognize read<T>/write<T>)
2. For each read<T> param: register a read dependency on T in the graph
3. For each write<T> param: register a write dependency on T in the graph
4. Create a graph node that:
   a. Acquires all access tokens (using when_all for parallel acquisition)
   b. Calls action(tokens...)
   c. Returns the result
5. Return the task representing this node
```

`state_ready<State>()` implementation:
```
1. Check if the system with State has completed its update coroutine this frame
2. If yes: return pointer to state
3. If no: suspend, register wakeup on the state's completion event
4. Resume when state system finishes, return pointer
```

`record_parallel()` implementation:
```
1. Allocate a secondary command buffer from a thread-local pool
2. Create a recording_context wrapping the secondary buffer
3. Call the user's lambda with the recording_context
4. Register the secondary buffer with the render graph for later execution
5. The render graph's execute() merges secondaries via vkCmdExecuteCommands
```

### 5. `Concurrency/TaskGraph.cppm`
**Partition:** `:task_graph`

The per-frame DAG executor that replaces `scheduler::update()` + `build_work_batches()`.

```cpp
export module gse.utility:task_graph;
import std;
import :async_task;
import :access_token;
import :task;
import :trace;
import :id;

export namespace gse {
    class task_graph {
    public:
        using node_id = std::size_t;

        auto submit(id name, async::task<> coroutine) -> node_id;

        auto barrier() -> void;

        auto execute() -> void;

        auto clear() -> void;

        auto notify_state_ready(std::type_index state_type) -> void;

        auto wait_state_ready(std::type_index state_type) -> async::task<>;

    private:
        struct graph_node {
            id name;
            async::task<> coroutine;
            std::vector<node_id> depends_on;
            std::atomic<bool> completed = false;
        };

        std::vector<graph_node> m_nodes;
        std::size_t m_barrier_point = 0;

        std::unordered_map<std::type_index, bool> m_state_ready;
        std::unordered_map<std::type_index, std::vector<std::coroutine_handle<>>> m_state_waiters;
        std::mutex m_state_mutex;
    };
}
```

**Implementation of `execute()`:**
```
1. Build adjacency list from m_nodes[i].depends_on
2. Compute in-degree for each node
3. Enqueue all zero-in-degree nodes into a ready queue
4. While ready queue not empty:
   a. Pop node from ready queue
   b. Start its coroutine on TBB arena via task::post
   c. When coroutine completes (hits co_return or final_suspend):
      - Mark node completed
      - Decrement in-degree of all dependents
      - Enqueue any dependent that hits zero in-degree
5. Wait for all nodes to complete (task::wait_idle or atomic counter)
```

Notes:
- Coroutines that suspend (e.g., waiting for a lock in `phase.read<T>()`) don't block a TBB worker. The worker picks up other ready nodes.
- When a lock becomes available, the coroutine is re-enqueued via `task::post`.
- Barrier nodes: `barrier()` adds edges from all current nodes to the next node submitted.

---

## Files to Modify

### 6. `Engine/Engine/Source/Core/Utility/Ecs/Registry.cppm`

Add per-component-type `shared_mutex` and access token acquisition methods.

**Changes:**
```cpp
struct component_slot {
    std::unique_ptr<component_link_base> link;
    std::shared_mutex type_mutex;
    range_lock_table range_locks;
};

// Replace:
std::unordered_map<std::type_index, std::unique_ptr<component_link_base>> m_component_links;
// With:
std::unordered_map<std::type_index, component_slot> m_components;

// Add acquisition methods:
template <is_component T>
auto acquire_read() -> read<T>;

template <is_component T>
auto acquire_write() -> write<T>;

template <is_component T>
auto acquire_partitioned_write(std::size_t partition_count) -> std::vector<partitioned_write<T>>;
```

Every existing method that accesses `m_component_links[idx]` changes to `m_components[idx].link`. Mechanical refactor.

**Debug assertion:** In `frame_sync::on_end` callback (where buffers flip), assert that no access tokens are outstanding.

### 7. `Engine/Engine/Source/Core/Utility/Ecs/SystemNode.cppm`

Change system lifecycle methods to return `async::task<>`.

**Changes:**
```cpp
// system_node_base virtual methods change to:
virtual auto update(graph_phase&) -> async::task<> = 0;
virtual auto render(graph_phase&) -> async::task<> = 0;
virtual auto begin_frame(graph_phase&) -> async::task<bool> = 0;
virtual auto end_frame(graph_phase&) -> async::task<> = 0;
virtual auto initialize(graph_phase&) -> void = 0;
virtual auto shutdown(graph_phase&) -> void = 0;
```

All systems must use `async::task<>` return types. No backward compatibility shim.

**Concept changes:**
```cpp
template <typename S, typename State>
concept has_update = requires(graph_phase& p, State& s) {
    { S::update(p, s) } -> std::same_as<async::task<>>;
};

template <typename S, typename State>
concept has_render = requires(graph_phase& p, const State& s) {
    { S::render(p, s) } -> std::same_as<async::task<>>;
};

template <typename S, typename State>
concept has_begin_frame = requires(graph_phase& p, State& s) {
    { S::begin_frame(p, s) } -> std::same_as<async::task<bool>>;
};

template <typename S, typename State>
concept has_end_frame = requires(graph_phase& p, State& s) {
    { S::end_frame(p, s) } -> std::same_as<async::task<>>;
};

// With-render-state variants follow the same pattern
template <typename S, typename State, typename RenderState>
concept has_render_with_state = requires(graph_phase& p, const State& s, RenderState& rs) {
    { S::render(p, s, rs) } -> std::same_as<async::task<>>;
};

template <typename S, typename State, typename RenderState>
concept has_begin_frame_with_state = requires(graph_phase& p, State& s, RenderState& rs) {
    { S::begin_frame(p, s, rs) } -> std::same_as<async::task<bool>>;
};

template <typename S, typename State, typename RenderState>
concept has_end_frame_with_state = requires(graph_phase& p, State& s, RenderState& rs) {
    { S::end_frame(p, s, rs) } -> std::same_as<async::task<>>;
};

template <typename S, typename State, typename RenderState>
concept has_initialize_with_state = requires(graph_phase& p, State& s, RenderState& rs) {
    { S::initialize(p, s, rs) } -> std::same_as<void>;
};
```

### 8. `Engine/Engine/Source/Core/Utility/Concurrency/Scheduler.cppm`

Replace the phase-based execution loop with task graph submission.

**Changes to `scheduler::update()`:**
```cpp
// AFTER:
auto scheduler::update() -> void {
    drain_deferred();
    auto writer = make_channel_writer();
    graph_phase phase{
        .reg = *m_registry,
        .snapshots = *this,
        .channel_reader = *this,
        .channels = writer,
        .graph = m_task_graph
    };
    phase.gpu_ctx = m_gpu_ctx;

    for (auto& node : m_nodes) {
        auto coro = node->update(phase);
        m_task_graph.submit(node->trace_id(), std::move(coro));
    }

    m_task_graph.barrier();
    m_task_graph.execute();
    m_task_graph.clear();
}
```

**Changes to `scheduler::render()`:**
```cpp
// Submit begin_frame, render, end_frame as graph nodes
// begin_frame is a gate (if it returns false, skip render nodes)
// render nodes can run in parallel
// end_frame depends on all render nodes
```

**Removals:**
- `build_work_batches()` — replaced by task graph dependency resolution
- `work_queue` usage — replaced by `graph_phase::submit()`

**New member:**
```cpp
task_graph m_task_graph;
```

### 9. `Engine/Engine/Source/Core/Utility/Ecs/PhaseContext.cppm`

Delete old phase types and related infrastructure.

**Removals:**
- `update_phase`, `render_phase`, `begin_frame_phase`, `end_frame_phase`, `shutdown_phase`, `initialize_phase` structs
- `work_queue` class and `queued_work` struct
- `chunk<T>` class
- All `has_*` concepts that reference old phase types

**Keep:**
- `state_snapshot_provider` — still used by `graph_phase`
- `channel_writer` — still used by `graph_phase`
- `channel_reader_provider` — still used by `graph_phase`
- `registry_access` — may still be useful for initialize/shutdown
- `phase_gpu_access` — still used by `graph_phase`

### 10. `Engine/Engine/Source/Core/Utility/Meta/LambdaTraits.cppm`

Extend to recognize `access<T, M>` parameter types.

**Additions:**
```cpp
template <typename T>
struct access_traits {
    static constexpr bool is_access = false;
    static constexpr access_mode mode = access_mode::read;
    using element_type = void;
};

template <is_component T, access_mode M>
struct access_traits<access<T, M>> {
    static constexpr bool is_access = true;
    static constexpr access_mode mode = M;
    using element_type = T;
};

template <typename T>
constexpr bool is_access_v = access_traits<std::remove_cvref_t<T>>::is_access;

template <typename T>
constexpr bool is_read_access_v = access_traits<std::remove_cvref_t<T>>::mode == access_mode::read;

template <typename T>
using access_element_t = access_traits<std::remove_cvref_t<T>>::element_type;
```

**Removals:**
- `chunk_traits`, `is_chunk_v`, `is_read_chunk_v`, `chunk_element_t`
- Chunk-based `param_info::read_types()` / `write_types()` — replaced with access-based versions

### 11. `Engine/Engine/Source/Platform/Vulkan/RenderGraph.cppm`

Add secondary command buffer support for parallel recording.

**Changes to `render_graph`:**
```cpp
struct secondary_recording {
    vk::CommandBuffer buffer;
    std::type_index pass_type;
    std::move_only_function<void(recording_context&)> record_fn;
};

// New members:
std::vector<secondary_recording> m_secondary_recordings;
std::mutex m_secondary_mutex;

// New method:
auto record_secondary(
    std::type_index pass_type,
    std::move_only_function<void(recording_context&)> fn
) -> void;
```

**Changes to `execute()`:**
```
In the pass execution loop, after topological sort:
1. Identify passes that can run in parallel (no edges between them)
2. For parallel-eligible passes:
   a. Allocate secondary command buffer per pass
   b. Set VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT
   c. Create VkCommandBufferInheritanceInfo with the rendering info
   d. Record each pass to its secondary buffer (can run on TBB workers)
   e. In the primary buffer: vkCmdExecuteCommands with all secondaries
3. For sequential passes: record directly to primary (unchanged)
```

**Secondary command buffer pool (new, in GpuFrame or GpuDevice):**
```cpp
auto allocate_secondary_buffer() -> vk::CommandBuffer;
auto reset_secondary_pool() -> void;
```

### 12. `Engine/Engine/Import/Utility.cppm`

Add new partition exports:
```cpp
export import :async_task;
export import :sender;
export import :access_token;
export import :graph_phase;
export import :task_graph;
```

Remove old partition exports once legacy code is deleted:
- `:phase_context` contents reduced (old phase types removed, keep snapshot providers)

---

## Systems to Migrate

All systems switch from old phase types to `graph_phase` + `async::task<>`. Migration order based on complexity (simplest first):

### Phase 1: Camera System
**File:** `Engine/Engine/Source/Graphics/3D/Camera/CameraSystem.cppm`
- 1 `phase.schedule` call
- Only reads `follow_component` (never writes)
- Change: `update(update_phase&, state&) -> void` to `update(graph_phase&, state&) -> async::task<>`
- Replace `phase.schedule(lambda)` with `co_await phase.read<follow_component>()` + inline logic

### Phase 2: Animation System
**File:** `Engine/Engine/Source/Graphics/3D/Animations/Animation.cppm`
- 1 `phase.schedule` call (line 151)
- Writes `animation_component`, `controller_component`, `clip_component`
- Similar coroutine conversion

### Phase 3: Geometry Collector
**File:** `Engine/Engine/Source/Graphics/Renderers/GeometryCollector.cppm`
- 1 `phase.schedule` call (line 301)
- Writes `render_component`, reads `motion_component`, `collision_component`, `animation_component`
- Uses `when_all` for parallel token acquisition
- Splits gather (component access) from upload (GPU staging) into separate graph nodes

### Phase 4: Physics System
**File:** `Engine/Engine/Source/Physics/System.cppm`
- 2 `phase.schedule` calls (GPU + CPU solver paths, lines 633, 652)
- Most complex: solver reads, interpolation writes
- Split into solver node (read) + interpolation node (write)
- Render method: minimal changes (GPU context management)

### Phase 5: Renderer + Forward + RT Shadow + Light Culling + Depth Prepass
**Files:**
- `Engine/Engine/Source/Graphics/Renderers/Renderer.cppm`
- `Engine/Engine/Source/Graphics/Renderers/ForwardRenderer.cppm`
- `Engine/Engine/Source/Graphics/Renderers/RtShadowRenderer.cppm`
- `Engine/Engine/Source/Graphics/Renderers/LightCullingRenderer.cppm`
- `Engine/Engine/Source/Graphics/Renderers/DepthPrepassRenderer.cppm`

These use `render_phase` + `ctx.graph().add_pass()`. Change to `graph_phase` + `record_parallel()` for passes that can be recorded on separate threads.

### Phase 6: All remaining systems
Any system with `initialize`, `update`, `begin_frame`, `render`, `end_frame`, or `shutdown` methods must update signatures to use `graph_phase` and return `async::task<>` (or `async::task<bool>` for `begin_frame`). `initialize` and `shutdown` remain `void` since they run sequentially at startup/teardown.

---

## Implementation Order

### Step 1: `async::task<T>` coroutine type
**File:** `Concurrency/AsyncTask.cppm`
**Verify:** Create a `task<int>`, start it, verify it completes with the correct value. Test `co_await` between two tasks. Test exception propagation.
**Dependencies:** None (standalone)

### Step 2: `async::when_all`, `async::sync_wait`
**File:** `Concurrency/Sender.cppm`
**Verify:** `when_all(task_a, task_b)` completes when both are done. `sync_wait` blocks correctly.
**Dependencies:** Step 1

### Step 3: Access tokens + registry changes
**Files:** `Ecs/AccessToken.cppm`, `Ecs/Registry.cppm`
**Verify:** Acquire `read<T>` from two threads simultaneously (should succeed). Acquire `write<T>` while `read<T>` is held (should block). Release `read<T>`, write should unblock. Test `range_lock_table` with overlapping and non-overlapping ranges.
**Dependencies:** None (standalone)

### Step 4: `graph_phase` + `task_graph`
**Files:** `Ecs/GraphPhase.cppm`, `Concurrency/TaskGraph.cppm`
**Verify:** Submit two coroutines that read the same component (should run in parallel). Submit one that writes and one that reads (should serialize). Verify `state_ready` suspends and resumes correctly.
**Dependencies:** Steps 1, 2, 3

### Step 5: `system_node` changes + `scheduler` changes
**Files:** `SystemNode.cppm`, `Scheduler.cppm`
**Verify:** Engine boots and runs a frame with the new scheduler loop.
**Dependencies:** Step 4

### Step 6: Migrate camera system (simplest)
**File:** `CameraSystem.cppm`
**Verify:** Camera follows entities, blends between controllers, mouse look works.
**Dependencies:** Step 5

### Step 7: Migrate animation system
**File:** `Animation.cppm`
**Verify:** Skeletal animations play correctly.
**Dependencies:** Step 5

### Step 8: Migrate geometry collector
**File:** `GeometryCollector.cppm`
**Verify:** Meshes render correctly, batching works, skinned meshes animate.
**Dependencies:** Step 5

### Step 9: Migrate physics system
**File:** `Physics/System.cppm`
**Verify:** Physics simulation runs, GPU solver path works, interpolation is smooth.
**Dependencies:** Step 5

### Step 10: Secondary command buffer support
**File:** `RenderGraph.cppm`, GPU frame management files
**Verify:** Forward renderer and RT shadow record in parallel. No validation errors.
**Dependencies:** None (can be done in parallel with steps 6-9)

### Step 11: Migrate render systems
**Files:** All renderer `.cppm` files
**Verify:** Full render pipeline works. Compare frame output before/after.
**Dependencies:** Steps 9, 10

### Step 12: Remove legacy code
- Delete `work_queue`, `queued_work`, `build_work_batches()`
- Delete `chunk<T>`
- Delete `update_phase`, `render_phase`, `begin_frame_phase`, `end_frame_phase`, `shutdown_phase`, `initialize_phase`
- Delete `lambda_traits` chunk detection
- Remove old `system_node_base` virtual methods
- Clean up `PhaseContext.cppm` to only contain shared infrastructure (snapshot providers, channel types)
**Dependencies:** All systems migrated

---

## Verification

### Runtime
1. Launch the engine, verify window opens and renders
2. Verify physics simulation runs (objects fall, collide)
3. Verify camera follows entities and mouse look works
4. Verify skeletal animations play
5. Verify no Vulkan validation layer errors (parallel command buffer recording)

### Concurrency validation
- Run with ThreadSanitizer (if available on MSVC) or Application Verifier
- Verify no data races on component access
- Verify no deadlocks (access tokens always acquired in consistent order via `when_all`)

### Performance
- Compare frame times before/after
- Trace (existing trace system) should show overlapping execution of:
  - Camera read + physics read (during solver step)
  - Geometry upload + other systems
  - Render pass recording (if secondary buffers implemented)

### Regression
- All existing behavior preserved: entity lifecycle, channel messaging, deferred operations, hot reload, GPU readback

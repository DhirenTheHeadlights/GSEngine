# Variant 2 — Scheduler unification (rolling topo-ordered run loop)

Replaces the `init phase → update phase → render phase` separation with a unified topo-ordered tick loop. Each system has a single coroutine `run` that handles both init and update; `frame` stays as a separate static method (different concurrency semantics — runs in parallel with the *next* tick's update). The scheduler resumes each system's coroutine at the right time each tick, in topo order. Hot-add is first-class; failures are asserts.

## Goal

After this lands:

- **No `scheduler::initialize()` call.** Engine.cpp registers systems and immediately enters the tick loop. The first few ticks resolve init across systems via coroutine resumption.
- **Every system is a single coroutine** that lives across ticks. Init code runs before the first `co_await ctx.next_tick()`; update code runs in the loop body after.
- **Async init is a primitive.** Any system can `co_await ctx.load<asset>(...)` or `co_await ctx.wait_for<X::system>()` during init without bootstrap orchestration.
- **Hot-add from anywhere.** `ctx.add_system<X>()` from update body, frame body, or external code queues a new system; the next tick incorporates it via topo-sort.
- **Render output gates on "renderers initialized"** — first frames render nothing or a loading state until renderer pipelines are built.

## Convention decisions

| Question | Choice | Rationale |
|----------|--------|-----------|
| Init/update structure | Single `run` coroutine; loops if it has update work, `co_return`s if it doesn't | Architectural purity — no special init phase. Init-only systems just don't have the loop |
| First-tick semantics | Engine ticks at full speed from t=0; render gated until ready | Matches the rolling-init model; loading-state UX handled by render gate |
| Hot-add | First-class from any context | Required for runtime mod loading, dynamic scene composition |
| Init failure | Assert | Matches current behavior; future graceful-degradation is opt-in per system |

### `run` coroutine shapes — three valid forms

A system's `run` body falls into one of three shapes depending on what work it has:

**Shape A — init only (no per-tick work):**
```cpp
auto save::system::run(run_context& ctx, settings& cfg, state& s) -> async::task<> {
    s.path = config::resource_path / "Misc/settings.ini";
    register_persistence_hooks(ctx, cfg, s);
    co_return;   // no update loop — system is done
}
```
The coroutine completes after init. The scheduler stops resuming it; its state slot stays alive for cross-system reads. Storage owned by `system_node_data<S>` is destroyed on shutdown like any other system.

**Shape B — init + always-update loop:**
```cpp
auto camera::system::run(run_context& ctx, state& s, const input::system::state& input) -> async::task<> {
    s.view_matrix = identity();
    s.projection_matrix = perspective(...);

    while (true) {
        co_await ctx.next_tick();
        // per-tick update work
    }
}
```
Standard case for systems with always-on update logic.

**Shape C — init + conditional-update loop:**
```cpp
auto profiler::system::run(run_context& ctx, settings& cfg, state& s) -> async::task<> {
    init_overlay(s);

    while (cfg.enabled) {
        co_await ctx.next_tick();
        sample_frame(s);
    }
    // exited loop: coroutine completes; scheduler stops resuming
}
```
Or with re-enterability via `continue`:
```cpp
auto debug_overlay::system::run(run_context& ctx, settings& cfg, state& s) -> async::task<> {
    while (true) {
        co_await ctx.next_tick();
        if (!cfg.enabled) {
            continue;   // skip this tick's work but stay alive for re-enable
        }
        update_overlay(s);
    }
}
```

**Choosing between `while (true)` + `continue` vs `while (flag)` + co_return:**
- `while (true)` + `continue`: system stays alive across the entire engine lifetime; can re-enable later. Best default — coroutine frame is small and the resume cost is negligible.
- `while (flag)` + co_return: system terminates when flag flips. Once terminated, can't be re-enabled (coroutine frame is destroyed). Use only for genuinely one-shot lifecycle (e.g., a per-level-load system that runs through a boot sequence and is gone).

Recommendation: prefer `while (true)` + `continue` unless the system is intentionally one-shot. Coroutine frames are small; the re-enterability is worth the trivial cost.

## The new system shape

### Today

```cpp
export namespace gse::camera {
    struct system {
        struct state { /* ... */ };

        static auto initialize(
            init_context& phase,
            state& s
        ) -> void;

        static auto update(
            update_context& ctx,
            state& s,
            const input::system::state& input_state
        ) -> async::task<>;
    };
}
```

### After variant 2

```cpp
export namespace gse::camera {
    struct system {
        struct state { /* ... */ };

        static auto run(
            run_context& ctx,
            state& s,
            const input::system::state& input_state
        ) -> async::task<>;
    };
}

auto gse::camera::system::run(run_context& ctx, state& s, const input::system::state& input_state) -> async::task<> {
    s.view_matrix = identity();
    s.projection_matrix = perspective(degrees(70.0f), 16.0f / 9.0f, meters(0.1f), meters(1000.0f));

    while (true) {
        co_await ctx.next_tick();

        const time dt = system_clock::dt();
        const auto& current_input = input::system::current_state(input_state);

        if (!s.ui_focus) {
            const auto delta = current_input.mouse_delta();
            s.yaw -= degrees(delta.x() * s.mouse_sensitivity);
            s.pitch -= degrees(delta.y() * s.mouse_sensitivity);
            s.pitch = std::clamp(s.pitch, degrees(-89.0f), degrees(89.0f));
        }

        s.view_matrix = compute_view_matrix(s.current);
        s.projection_matrix = compute_projection_matrix(s.current, s.viewport);
    }
}
```

What changes:

- `initialize` and `update` collapse into a single `run` coroutine.
- Code before the `while (true) { co_await ctx.next_tick(); ... }` loop is the init phase.
- Code inside the loop body (after `next_tick()`) is the per-tick update.
- Returns `async::task<>` — but the body never `co_return`s. The coroutine lives for the lifetime of the system.

### Renderers — `frame` stays separate

```cpp
export namespace gse::renderer::skin_compute {
    struct system {
        struct resources {
            resource::handle<shader> shader_handle;
            gpu::pipeline pipeline;
            per_frame_resource<gpu::descriptor_region> descriptors;
        };

        static auto run(
            run_context& ctx,
            const gpu::context::state& gpu_s,
            const geometry_collector::system::resources& gc,
            resources& r
        ) -> async::task<>;

        static auto frame(
            frame_context& ctx,
            const gpu::context::state& gpu_s,
            const resources& r,
            const geometry_collector::system::state& gc_s,
            const geometry_collector::system::resources& gc_r
        ) -> async::task<>;
    };
}

auto gse::renderer::skin_compute::system::run(
    run_context& ctx,
    const gpu::context::state& gpu_s,
    const geometry_collector::system::resources& gc,
    resources& r
) -> async::task<> {
    r.shader_handle = co_await ctx.load<shader>("Shaders/Compute/skin_compute");
    assert(r.shader_handle->is_compute(), "Skin compute shader is not loaded as a compute shader");

    r.pipeline = gpu::create_compute_pipeline(*gpu_s.device, *gpu_s.shader_registry, *gpu_s.bindless_textures, r.shader_handle, "push_constants");

    constexpr std::size_t skin_buffer_size = geometry_collector::system::resources::max_skin_matrices * sizeof(mat4f);
    constexpr std::size_t local_pose_size = geometry_collector::system::resources::max_skin_matrices * sizeof(mat4f);

    for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
        r.descriptors[i] = gpu::allocate_descriptors(*gpu_s.shader_registry, gpu_s.device->descriptor_heap(), r.shader_handle);

        gpu::descriptor_writer(*gpu_s.shader_registry, gpu::context::device_handle(gpu_s), r.shader_handle, r.descriptors[i])
            .buffer("skeletonData", gc.skeleton_buffer, 0, geometry_collector::system::resources::max_joints * gc.joint_stride)
            .buffer("localPoses", gc.local_pose_buffer[i], 0, local_pose_size)
            .buffer("skinMatrices", gc.skin_buffer[i], 0, skin_buffer_size)
            .commit();
    }

    while (true) {
        co_await ctx.next_tick();
        // skin_compute has no per-tick update work; it only contributes to frame.
    }
}
```

`run` handles init (loads shader, builds pipeline, sets up descriptors) then idles in the tick loop (this renderer doesn't have update work). `frame` is unchanged from today.

### Why `frame` doesn't merge into `run`

`update` and `frame` have different concurrency: frame N executes concurrently with update N+1. Merging them serialized within a system would cost throughput. Keeping `frame` separate preserves the current parallelism. Conceptually:

- `run` = "this system's life-thread" — init, then the per-tick update loop.
- `frame` = "this system's contribution to GPU frame N" — separate task spawned by the dispatcher, runs in parallel with the next tick.

## The new dispatch model

### `run_context` — what the system sees

```cpp
export namespace gse {
    class run_context {
    public:
        run_context(
            scheduler& sched,
            registry& reg,
            state_registry& states,
            resource_registry& resources_store,
            channel_registry& channels_store,
            channel_writer& channels,
            task_graph& graph
        );

        // Suspend until the scheduler advances this system to its next tick.
        [[nodiscard]] auto next_tick(
        ) -> async::task<>;

        // Async asset load. Returns a handle once loaded + finalized.
        template <typename Asset>
        [[nodiscard]] auto load(
            std::string_view path
        ) -> async::task<resource::handle<Asset>>;

        // Hot-add a system. Queued; takes effect at the start of the next tick.
        template <typename S, typename... Args>
        auto add_system(
            Args&&... args
        ) -> void;

        // Channel + component access (same as today's update_context).
        template <typename T>
        auto read_channel(
        ) const -> channel_read_guard<T>;

        // Component / entity access (same as today).
        // ...

    private:
        // Per-system tick-resume slot, used by next_tick().
        async::manual_event m_tick_event;
        // ...
    };
}
```

### Scheduler internals — the per-tick loop

```cpp
auto gse::scheduler::tick() -> void {
    drain_hot_add_queue();         // 1
    sort_topo_if_dirty();          // 2
    advance_systems();             // 3
    drive_frame_phase();           // 4
}

auto gse::scheduler::drain_hot_add_queue() -> void {
    std::vector<system_node> queued;
    {
        std::lock_guard lock(m_hot_add_mutex);
        queued.swap(m_hot_add_queue);
    }
    for (auto& node : queued) {
        m_nodes.push_back(std::move(node));
        m_topo_dirty = true;
    }
}

auto gse::scheduler::sort_topo_if_dirty() -> void {
    if (!m_topo_dirty) {
        return;
    }
    m_topo_order = topo_sort_all_nodes();   // includes both initialized and pending
    m_topo_dirty = false;
}

auto gse::scheduler::advance_systems() -> void {
    graph.clear();
    std::vector<async::task<>> advances;
    advances.reserve(m_nodes.size());
    for (const std::size_t idx : m_topo_order) {
        advances.push_back(advance_one_system(m_nodes[idx]));
    }
    async::sync_wait(async::when_all(std::move(advances)));
}

auto gse::scheduler::advance_one_system(system_node& node) -> async::task<> {
    // Wait on the cross-system deps declared in run's signature.
    // Same task_graph + after_id mechanism as today's update phase.
    for (const id& dep : node.run_state_deps) {
        co_await graph.wait_state_ready(dep);
    }

    if (!node.run_handle) {
        // First tick: spawn the run coroutine.
        // run() executes synchronously up to the first co_await
        // (typically next_tick(), but could be next_tick() or some async load).
        node.run_handle = node.invoke_run_fn(node.data.get());
    }
    else if (!node.run_handle.done()) {
        // Coroutine is alive. Signal "your tick" and wait for it
        // to suspend at next next_tick() (or co_return / hit an asset await).
        node.tick_event.set();
        co_await node.tick_done_event.wait();
    }
    // else: coroutine completed (Shape A or Shape C w/ co_return).
    // Do nothing — its state slot stays valid for cross-system reads.

    // Notify dependents that this system has advanced this tick.
    graph.notify_state_ready(node.state_id);
    if (node.legacy_state_id.exists()) {
        graph.notify_state_ready(node.legacy_state_id);
    }
    if (node.resources_id.exists()) {
        graph.notify_state_ready(node.resources_id);
    }
    if (node.settings_id.exists()) {
        graph.notify_state_ready(node.settings_id);
    }
}

auto gse::scheduler::drive_frame_phase() -> void {
    if (!m_frame_phase_enabled) {
        return;  // no GPU / no rendering yet
    }
    // Spawn frame() coroutines for every system whose run coroutine has
    // either reached the update loop OR completed. Both states mean
    // "this system's state is valid and ready to be read in frame()."
    std::vector<async::task<>> tasks;
    for (auto& node : m_nodes) {
        if (!node.has_frame) {
            continue;
        }
        if (node.is_in_update_loop || (node.run_handle && node.run_handle.done())) {
            tasks.push_back(node.invoke_frame_fn(m_frame_ctx, node.data.get()));
        }
    }
    async::sync_wait(async::when_all(std::move(tasks)));
}
```

Key invariants:

- A system is "fully initialized" when its `run` coroutine has executed past the first `co_await ctx.next_tick()`. The dispatcher tracks this via a `is_in_update_loop` flag set when next_tick suspends.
- Until that point, `frame()` does not run for that system.
- Cross-system param resolution still works because topo-sort guarantees deps are spawned before consumers, but the dispatcher additionally waits for the dep's `is_in_update_loop` before invoking the consumer's `next_tick`.

### `next_tick()` mechanics

```cpp
auto gse::run_context::next_tick() -> async::task<> {
    // Mark "this system has reached the update loop" the first time we hit
    // next_tick. The scheduler reads this flag to gate frame() and consumers.
    if (!m_in_update_loop) {
        m_in_update_loop = true;
        m_node_ptr->is_in_update_loop = true;
    }
    // Wait for the scheduler's "tick advance" signal for this system.
    co_await m_tick_event.wait();
    m_tick_event.reset();
}
```

The scheduler calls `m_tick_event.set()` on each system once per tick (from `advance_systems`). The system's coroutine resumes, runs its tick body, then suspends again at the next `next_tick()`.

### `load<Asset>()` — async asset loading

```cpp
template <typename Asset>
auto gse::run_context::load(std::string_view path) -> async::task<resource::handle<Asset>> {
    // Push a load request to the asset registry's input channel.
    auto handle = co_await m_channels.push_with_reply<asset::load_request<Asset>>(
        { .path = std::string(path) }
    );

    // The asset registry's run coroutine has been waiting on this channel.
    // It schedules the load, waits for completion, then signals back.
    co_return handle;
}
```

This collapses `asset::registry::get` + `instantly_load` into a single async call. The asset registry's own `run` coroutine drives the loading on its own thread / pump:

```cpp
auto gse::asset::registry::run(run_context& ctx, const gpu::context::state& gpu_s, state& s) -> async::task<> {
    s.async_submit = make_async_submit(ctx, gpu_s);
    s.sync_submit = make_sync_submit(gpu_s);
    s.gpu_waiter = [&gpu_s] { gpu_s.device->wait_idle(); };

    add_loader<shader>(s);
    add_loader<texture>(s);
    add_loader<model>(s);
    // ...

    while (true) {
        co_await ctx.next_tick();

        for (auto&& req : ctx.read_channel<load_request<shader>>()) {
            auto handle = queue<shader>(s, req.path);
            instantly_load(s, handle);
            req.reply.set_value(handle);
        }
        // ...repeat for other asset types (or use type-erased channel)
    }
}
```

Note this folds Engine.cpp's between-batches wiring directly into the asset registry's init. `gpu::context::state` is now a declared cross-system param of `asset::registry::run` — topo-sort orders gpu first.

### Hot-add queue

```cpp
template <typename S, typename... Args>
auto gse::scheduler::add_system(Args&&... args) -> void {
    auto node = make_system_node<S>(std::forward<Args>(args)...);
    {
        std::lock_guard lock(m_hot_add_mutex);
        m_hot_add_queue.push_back(std::move(node));
    }
    // Picked up at the start of the next tick by drain_hot_add_queue().
}
```

`add_system` from any thread / context is safe via the mutex. The new system's `run` coroutine starts on the next tick after registration.

### Removed: `scheduler::initialize()` / `scheduler::update()` distinction

```cpp
// Before:
class scheduler {
public:
    auto initialize() -> void;          // run init phase
    auto update() -> void;              // run update phase
    auto render(bool frame_ok, ...)
        -> void;                        // run frame phase
};

// After:
class scheduler {
public:
    auto tick(bool frame_ok) -> void;   // single per-tick entry point
};
```

`Engine.cpp::game_loop` becomes:

```cpp
auto gse::engine::tick() -> void {
    system_clock::update();
    m_scheduler.tick(m_render_enabled);
    m_world.update();
}
```

No more two-phase init. Game loop is one call.

## Migration path

The migration is in 5 phases. Each phase is a coherent landing point — the engine compiles and runs after each, with progressively more systems on the new model. A "compatibility pump" lets old-style systems coexist with new-style ones during the transition.

### Phase V2-1 — Build the run-coroutine machinery in parallel (~3 hr)

Add the new types alongside the existing ones:

- `run_context` class with `next_tick()`, `load<Asset>()`, `add_system<S>()`.
- `system_node::run_handle` field (coroutine handle for the run coroutine).
- `system_node::is_in_update_loop` flag.
- `scheduler::tick()` method that runs the new model OR delegates to the old `initialize()`/`update()`/`render()` for unmigrated systems.
- `make_system_node<S>` checks for `S::run` (new) vs `S::initialize`/`S::update` (old) via a concept and dispatches to the right path.

```cpp
template <typename S>
concept names_run = requires { &S::run; };

template <typename S>
auto make_system_node(...) -> system_node {
    system_node node;
    if constexpr (names_run<S>) {
        node.invoke_run_fn = &invoke_run_for<S>;
    }
    else {
        node.invoke_initialize_fn = &invoke_initialize_for<S>;
        node.invoke_update_fn = &invoke_update_for<S>;
    }
    // ...
}
```

After this phase, both system models work. No system has migrated yet.

### Phase V2-2 — Migrate the asset registry first (~2 hr)

Asset registry is the critical path: every other system depends on it for asset loading, so it must support `co_await ctx.load<...>` before any consumer migrates.

```cpp
auto gse::asset::registry::run(
    run_context& ctx,
    const gpu::context::state& gpu_s,
    state& s
) -> async::task<> {
    // (the wiring code from Engine.cpp:50-86 moves here)
    install_callbacks(s, gpu_s, ctx);
    register_default_loaders(s);

    while (true) {
        co_await ctx.next_tick();
        process_pending_loads(s);
        process_finalizations(s);
    }
}
```

Engine.cpp simplifies: the wiring block disappears, gpu::context becomes a regular topo-sort dep.

### Phase V2-3 — Migrate engine systems (~6 hr)

Convert `initialize` + `update` to `run` for: input, actions, network, save, window, gpu::context, physics, camera, audio, animation, gui, renderer aggregator. Each is a one-system migration following the camera example above.

Order by least-dependents-first to minimize cross-system breakage during the migration. Compatibility pump in scheduler::tick handles the mixed state.

### Phase V2-4 — Migrate renderers (~5 hr)

The 11 renderer systems. Each becomes a `run` coroutine that loads its shader async, builds its pipeline, then idles in the tick loop (most renderers do their per-tick work in `frame`, not `update`).

```cpp
auto gse::renderer::forward::system::run(
    run_context& ctx,
    const gpu::context::state& gpu_s,
    const rt_shadow::system::state& rt_state,
    const light_culling::system::resources& lc_r,
    settings& cfg,
    resources& r,
    frame_data& fd
) -> async::task<> {
    gse::settings::register_panel(ctx, "Graphics", cfg);

    r.shader_handle = co_await ctx.load<shader>("Shaders/Standard3D/meshlet_geometry");
    r.skinned_shader = co_await ctx.load<shader>("Shaders/Standard3D/skinned_geometry_pass");
    r.blank_texture = co_await ctx.load_or_queue<texture>("blank", vec4f(1, 1, 1, 1));

    build_camera_ubo(r, gpu_s);
    build_lights_buffer(r, gpu_s);
    build_material_palette_buffer(r, gpu_s);
    build_descriptors(r, gpu_s, rt_state, lc_r);
    build_pipelines(r, gpu_s);

    while (true) {
        co_await ctx.next_tick();
        // forward has no per-tick update — all work is in frame()
    }
}
```

### Phase V2-5 — Drop the old machinery + Engine.cpp migration (~3 hr)

Once every system is migrated:

- Remove `initialize()` / `update()` / `render()` from scheduler — replace with `tick()`.
- Remove the compatibility pump.
- Remove `init_context` / `update_context` types — `run_context` replaces both.
- `frame_context` stays (still used by `frame()`).
- `Engine.cpp::start()` drops the two-phase init pattern; just registers and ticks.
- `Engine.cpp::render()` becomes a thin gate that runs the frame phase only when GPU is ready and renderers are initialized.

```cpp
auto gse::start(setup_fn setup, flags flags, config cfg) -> int {
    engine e(flags, cfg);
    setup(e);
    while (!e.should_quit()) {
        e.tick();
    }
    e.shutdown();
    return 0;
}
```

## Per-area changes

### `Engine/Engine/Source/Ecs/`

- `Scheduler.{cppm,cpp}` — major rewrite. New `tick()`, `advance_systems()`, `drain_hot_add_queue()`, hot-add mutex + queue, removal of `initialize` / `update` / `render` methods. ~250 LoC change.
- `SystemDispatch.cppm` — add `invoke_run_for<S>`, `register_state_dep_tags<^^S::run, S>`, `extract_run_state_deps<S>`. Old `invoke_initialize_for<S>` / `invoke_update_for<S>` deleted in phase V2-5. ~100 LoC change.
- `SystemNode.cppm` — add `run_handle`, `is_in_update_loop`, `invoke_run_fn`. Drop `invoke_initialize_fn`, `invoke_update_fn`, `init_state_deps`, `update_state_deps` in V2-5. ~30 LoC change.
- New: `RunContext.cppm` — the `run_context` class.
- `PhaseContext.cppm` — `init_context` removed in V2-5; only `shutdown_context` remains.
- `TaskContext.cppm` / `UpdateContext.cppm` — removed in V2-5.
- `FrameContext.cppm` — unchanged. Still used by `frame()`.

### `Engine/Engine/Source/Runtime/`

- `Engine.{cppm,cpp}` — drops the two-phase init pattern. `start()` becomes a single tick loop. `render()` becomes a frame-phase gate (or merges into `tick()`).

### Per-system files

- Every `.cppm` system declaration: remove `initialize` / `update`, add `run`.
- Every `.cpp` system implementation: rewrite the body as a coroutine. Load assets via `co_await ctx.load<X>(...)`. Per-tick work goes inside `while (true) { co_await ctx.next_tick(); ... }`.

Estimate: ~21 systems × 30 min average = ~10 hr (most are mechanical; a few have weird control flow that's tricky to translate to a coroutine loop).

### Game-side systems (Game/)

- `gs::client_ui_system` and other game systems migrate same way.
- Game systems currently use `update`, no `initialize`. Become `run` with no init code, just the loop body.

## Testing strategy

- **Phase V2-1**: existing test suite still passes (no system migrated).
- **Phase V2-2**: asset loading still works for all systems (asset registry is the only migrated system; others use compat).
- **Phase V2-3**: smoke test each engine system after migration. Run the game; verify input, audio, network all work.
- **Phase V2-4**: visual test — render output looks correct. First few frames may be black during init; verify they become correct.
- **Phase V2-5**: full test pass + perf comparison vs pre-variant-2 baseline.

## Risks and mitigations

| Risk | Mitigation |
|------|-----------|
| Compatibility pump in scheduler is buggy / regressions during transition | Each phase commits separately; can bisect |
| Coroutine frame size growth (locals captured in coroutine state) | Profile heap allocations after V2-5; if needed, move large locals to `state` struct |
| Hot-add race conditions (mid-tick add_system from update body) | Mutex on hot_add_queue; drained at tick start, not mid-tick |
| Render output during early frames looks broken | Loading-screen UX; gate Engine::render until renderers in update loop |
| Cross-system param refs become dangling if the producer system is removed via hot-remove | Don't support hot-remove in v1; only hot-add |
| Asset load failures during init now propagate through coroutines instead of synchronous calls | `co_await ctx.load<X>(...)` asserts on failure (matches today). For graceful degradation, add `try_load<X>` returning `task<optional<handle>>` |
| Coroutine spec edge cases on clang-p2996 (the toolchain) | The renderer body split (`Phase 8`) already validated cross-TU coroutine elaboration is safe |
| Init time regression (parallel init becomes serial in early ticks) | Multiple systems' init bodies run their synchronous prefix in topo order in tick 0; only ones with `co_await load<>` actually defer |

## Estimated total cost

| Phase | Effort | Risk |
|-------|--------|------|
| V2-1: Build run-coroutine machinery in parallel | 3 hr | Low |
| V2-2: Asset registry migration | 2 hr | Low-Med |
| V2-3: Engine systems migration | 6 hr | Med |
| V2-4: Renderer migration | 5 hr | Med |
| V2-5: Drop old machinery + Engine.cpp migration | 3 hr | Low |
| **Total** | **~19 hr** | **Med** |

Add ~3 hr buffer for unexpected systems with weird control flow. Realistic: **~22 hr**.

## What survives, what dies

### Survives

- `system_node` data lifecycle (`make_system_node<S>` + `system_node_data<S>`).
- Per-system `state` / `resources` / `settings` / `frame_data` storage structs.
- `frame_context`, `frame()` method, frame graph, render-pass dispatch.
- Channels and channel-write semantics.
- `S::shutdown()` static method (still called in shutdown order).
- Per-system `S::settings` reflection for the settings panel.
- Cross-system param convention (declared in run/frame signatures, dispatcher resolves).
- The dispatcher's reflection chain: `^^S::run` parameter walking, `is_state_dep_v`, `extract_*_state_deps`, dual-registration window.
- `task_graph` / `wait_state_ready` / `notify_state_ready` (still load-bearing for per-tick cross-system synchronization).

### Cleanup checklist (after V2-5 lands — line-by-line items to delete)

The full migration produces a substantial deletion footprint. Walk this list once V2-5 is merged to confirm nothing's left dangling:

**Types to delete entirely:**
- [ ] `init_context` struct + all members in `Engine/Engine/Source/Ecs/PhaseContext.cppm` (only `shutdown_context` remains in that file).
- [ ] `update_context` class in `Engine/Engine/Source/Ecs/UpdateContext.cppm` — delete the whole file.
- [ ] `task_context` struct in `Engine/Engine/Source/Ecs/TaskContext.cppm` — delete the whole file (its `read_channel` / `after_id` / `notify_ready_by_id` move into `run_context`).
- [ ] `init_context` from the dispatcher's `is_state_dep_v` exclusion list (the special case is no longer needed since `init_context` no longer exists).

**Methods to delete on `scheduler`:**
- [ ] `scheduler::initialize()` (Scheduler.cpp + Scheduler.cppm).
- [ ] `scheduler::update()`.
- [ ] `scheduler::render(bool, std::function<void()>)`.
- [ ] `scheduler::topo_sort_pending_inits()` — replaced by `topo_sort_all_nodes()` which doesn't filter on `initialized`.
- [ ] `scheduler::run_graph_update()` (the body that orchestrated update tasks).
- [ ] `scheduler::run_graph_frame()` (whatever it's called now — the body that orchestrated frame tasks; subsumed into `drive_frame_phase`).
- [ ] `m_initialized` bool field.
- [ ] `m_dep_graph_checked` bool field (closed-graph check is per-tick now, not lazy).
- [ ] `m_update_graph` task_graph (replaced by `m_tick_graph`).
- [ ] `check_closed_dep_graph()` is kept but called from `tick()` rather than `update()`; delete the `m_dep_graph_checked` flag.

**Methods / functions in dispatcher (`SystemDispatch.cppm`):**
- [ ] `invoke_initialize_for<S>` template + the function pointer slot.
- [ ] `invoke_update_for<S>` template + the function pointer slot.
- [ ] `extract_init_state_deps<S>()`.
- [ ] `extract_update_state_deps<S>()`.
- [ ] `compute_state_dep_count<^^S::initialize, S>()` and `compute_state_dep_count<^^S::update, S>()` instantiations (the templates themselves remain, parameterized over MemberFn).
- [ ] `noop_initialize`, `noop_update_for<S>`, `noop_dispatchers::noop_update_for<S>` — replaced by a single `noop_run_for<S>` (or just the absence of `invoke_run_fn`).
- [ ] `resolve_initialize_arg<Arg, S>` and `resolve_update_arg<Arg, S>` templates — replaced by `resolve_run_arg<Arg, S>`. Note: `resolve_frame_arg` stays; it's used by `frame()`.
- [ ] `names_initialize<S>` and `names_update<S>` concepts in `SystemNode.cppm`.
- [ ] `names_run<S>` concept replaces them.
- [ ] `shutdown_takes_resources_state<S>`, `shutdown_takes_state<S>`, `shutdown_takes_phase_only<S>` concepts — kept (shutdown is unchanged).

**Fields on `system_node` (`SystemNode.cppm`):**
- [ ] `auto (*invoke_initialize_fn)(init_context&, void*) -> void` field.
- [ ] `auto (*invoke_update_fn)(update_context&, void*) -> async::task<>` field.
- [ ] `init_state_deps` `std::vector<id>` field.
- [ ] `update_state_deps` `std::vector<id>` field.
- [ ] `bool initialized` flag (replaced by `run_handle.done()` / `is_in_update_loop`).
- [ ] **Add**: `auto (*invoke_run_fn)(run_context&, void*) -> std::coroutine_handle<>` (or whatever the run launch returns).
- [ ] **Add**: `std::coroutine_handle<> run_handle`.
- [ ] **Add**: `std::vector<id> run_state_deps`.
- [ ] **Add**: `bool is_in_update_loop`.
- [ ] **Add**: `async::manual_event tick_event`.
- [ ] **Add**: `async::manual_event tick_done_event`.

**On every system's `.cppm` + `.cpp` (~21 systems):**
- [ ] Delete `static auto initialize(init_context&, ...) -> void;` declaration.
- [ ] Delete `static auto update(update_context&, ...) -> async::task<>;` declaration.
- [ ] Delete the corresponding `.cpp` body for both.
- [ ] **Add** `static auto run(run_context&, ...) -> async::task<>;` declaration + `.cpp` body.

**`Engine.cpp` cleanup:**
- [ ] The whole "between init #1 and init #2" wiring block at lines 50-86 — every line moves into `asset::registry::system::run`'s init prefix.
- [ ] The two `m_scheduler.initialize()` calls (lines 50, 106 + the conditional one in user-callback at ~110).
- [ ] `engine::shutdown()`'s `m_scheduler.clear()` after `m_scheduler.shutdown()` — `clear()` is gone (or kept as a no-op for symmetry; either way audit it).
- [ ] `engine::render(bool frame_ok, fn)` becomes `engine::tick()`'s frame phase. The `bool frame_ok` parameter is computed inside tick() not passed in.
- [ ] `engine::try_state_of<X>` and `engine::resources_of<X>` (in Engine.cppm) — kept; bootstrap-only access path. Friend-restrict if you want.

**`Engine.cppm` (the public interface):**
- [ ] `engine::start(setup_fn, flags, config)` — body simplifies; delete the `setup` callback's special handling for re-init since there's no init phase to re-trigger.

**Compatibility pump from V2-1 (deleted in V2-5):**
- [ ] The branch in `make_system_node<S>` that dispatched to old-vs-new based on `names_run<S>` vs `names_initialize<S>`.
- [ ] The branch in `scheduler::tick` that ran old-style update for unmigrated systems.
- [ ] `if constexpr (names_initialize<S>) { ... } else if constexpr (names_run<S>) { ... }` patterns throughout the dispatcher.

**Reproducer / scratch:**
- [ ] `tmp/p2996-eof-ice/` — the original-ICE reproducer. Keep if you want a regression smoke test; delete if confident the structural fix is permanent.

**Files to fully delete:**
- [ ] `Engine/Engine/Source/Ecs/UpdateContext.cppm`.
- [ ] `Engine/Engine/Source/Ecs/TaskContext.cppm`.
- [ ] (Possibly) `Engine/Engine/Source/Ecs/PhaseContext.cppm` if `shutdown_context` moves to a more natural home.

**Greppable smoke tests after V2-5:**
- `grep -rn "init_context" Engine/` → should return only `shutdown_context` matches (or zero if PhaseContext.cppm deleted).
- `grep -rn "update_context" Engine/` → zero matches.
- `grep -rn "::initialize(" Engine/Engine/Source/` → only system shutdowns and external-API initialize calls (e.g., GLFW init).
- `grep -rn "::update(" Engine/Engine/Source/` → only non-system update methods (e.g., on `world`, on data structures).
- `grep -rn "scheduler::initialize\|scheduler::update\|scheduler::render" Engine/` → only `scheduler::tick` and `scheduler::shutdown` matches.
- `grep -rn "names_initialize\|names_update" Engine/Engine/Source/Ecs/` → zero matches.

## Open future work (post-V2)

- **Hot-remove**: opposite of hot-add, supporting `ctx.remove_system<X>()`. Requires reference-counting cross-system param dependencies. Not v1.
- **Per-system thread affinity**: `S::run` could declare it wants to run on a specific thread (e.g., GPU-bound systems on the GPU submit thread).
- **Save/restore mid-coroutine**: serializing a system's coroutine state. Probably never; would need custom coroutine machinery.
- **Graceful asset failure**: `try_load<X>` returning `task<optional<handle>>`. Lets games handle missing-asset gracefully.

---

If you green-light this, I'd start with **V2-1** as a single PR — the parallel coroutine machinery alongside the existing dispatcher, no system migrated yet, both code paths exercised. That's the lowest-risk landing point and validates the hot-add queue + run-coroutine resumption work cleanly before any system has to migrate.

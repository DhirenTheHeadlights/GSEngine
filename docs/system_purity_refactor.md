# System Purity Refactor

Enforce the architectural rule that ECS state is immutable across systems and all cross-system mutation flows through channels. The current codebase has accumulated escape hatches — mutable cross-system state access, instance methods on state structs, raw cross-system pointers cached on state. This refactor closes them.

## The rules

1. **`state` is POD data.** Default-constructible aggregate. No methods, no inheritance, no virtual functions.
2. **State is owned by exactly one system.** That system mutates it freely.
3. **Cross-system state access is `const` only.** Reading another system's state is fine; writing is not.
4. **Cross-system mutation goes through channels.** Producer pushes an event; consumer (the owning system) drains and applies.
5. **Per-frame scratch goes in `update_data` / `frame_data`** (already-owned by the writing system) or in published channel events.
6. **No cached cross-system pointers on state.** If you need another system's state in a function, take it as a parameter or look it up via cross-system const read at use time.

The dispatcher enforces rule 3 at compile time: cross-system params resolve to `const T&` only; declaring a mutable cross-system param is a `static_assert` failure.

## Why this matters

The previous gpu-as-system landing exposed the failure mode. To make renderers work, I added a mutable cross-system fallback in the dispatcher and let renderers do `gpu_s.render_graph->add_pass(...)` — directly mutating gpu's owned state from arbitrary other systems. That defeats the architecture: ordering is implicit, races are silent, ownership is fictional. Channels exist precisely to express "I want to mutate state X" as a typed event the owner can serialize.

## Scope of work — 9 phases

Phases 1–7 ship the spine of the architecture (mutable-fallback removal, gpu-state decomposition, channel-ification, render-graph migration, asset registry as a system, lifetime cleanup). Phase 8 makes cross-system reads structurally enforceable via reflection on `initialize`/`update`/`frame` signatures plus topo-sorted init order — landed except for the CI lint guards. Phase 9 unifies the cross-system identifier so every reference uses `id_of<S>` rather than `id_of<S::state>` — landed via `compute_state_dep_id<T>()` parent-of reflection. See sections at the end of this doc for 8 and 9.

### Phase 1 — Revert the mutable cross-system fallback

`Engine/Engine/Source/Ecs/SystemDispatch.cppm`:
- Drop `direct_state_ref_mut<T>`.
- `resolve_update_arg` / `resolve_frame_arg`: fallback returns `const U&` always.
- `resolve_initialize_arg`: same. Drop the `if constexpr (std::is_const_v<...>)` branch.
- Add a `static_assert(std::is_const_v<std::remove_reference_t<Arg>>, "cross-system state must be const; use channels for mutation")` so the failure message points at the right thing.

This is the spine. After it lands, every illegal cross-system mutation in the codebase fails to compile and we follow the failures.

**Estimate: 30 min.**

### Phase 2 — Decompose `gpu::context::state`

Today's state holds: `device`, `shader_registry`, `swapchain`, `frame`, `render_graph`, `bindless_textures`, `frame_scheduler`, `window_state*`, `assets*`, `ui_focus`, `command_queue`, `pending_finalizations`. Sort each into the right home.

**Stays on state (immutable post-init infrastructure):**
- `device` (`unique_ptr<gpu::device>`) — set up in initialize, never reseated.
- `shader_registry` — same.
- `swapchain` — same.
- `frame` — same.
- `bindless_textures` — same.
- `frame_scheduler` — owned, used by gpu's own update for command submission scheduling.

These are owned via `unique_ptr`; cross-system const readers can't reseat them, only call methods on the underlying objects. Audit those method calls — anything not logically const needs to be marked const where appropriate, or moved to gpu's own update phase. The biggest such method is `device->create_buffer` — that's mutating the allocator. Either mark allocator's mutating methods on a `mutable` member (the mutex pattern, justified for thread-safe primitives) or move buffer creation into gpu's update via a channel request (see phase 3).

**Drop from state:**
- `window_state*` — replaced by `(gpu::state&, window::state&)` parameters on every static fn that needs the window. `gpu::context::initialize` takes both, stashes nothing.
- `assets*` — drops with phase 5 (asset registry becomes a system). Until then, asset loaders take `gpu::context::state&` and `asset::registry&` as separate args.
- `ui_focus` — duplicates `window::state::ui_focus`. Drop. Read from window state where needed.

**Move to channels (phase 3):**
- `command_queue` — becomes `channel<gpu::command_request>`.
- `pending_finalizations` — becomes `channel<gpu::pending_finalization>`.

**Move to phase 4 (render graph migration):**
- `render_graph` — biggest decision. Channel-published passes; gpu builds and executes graph each frame. See phase 4.

**Drop methods on state**:
- `take_pending_finalizations()` (asset::context virtual override).
- `gpu_queue_size()`, `mark_pending_for_finalization()`, `wait_idle()`, `process_gpu_queue()` (the shims I added).

State becomes pure aggregate. Inheritance from `asset::context` goes away when `asset::registry` becomes a system in phase 5.

**Estimate: 2 hr.**

### Phase 3 — Channel-ify command_queue and pending_finalizations

`gpu::command_request` carries a closure: `std::function<void(gpu::context::state&)>` (or `move_only_function`). Pushed by anyone (asset loaders, etc.). Drained by `gpu::context::update` which runs each command on its own state.

`gpu::pending_finalization { id resource_type; id resource_id; }`. Pushed by loaders. Drained by `asset::registry::update` (phase 5) — until then, gpu's update drains and forwards to the asset registry held by Engine.

Drop:
- `gpu::context::queue_gpu_command` static fn.
- `gpu::context::mark_pending_for_finalization` static fn.
- `gpu::context::process_gpu_queue` static fn.
- `gpu::context::take_pending_finalizations` (and the `asset::context` virtual base entirely).

Update callers (~10 places — texture, model, font, skeleton, clip_asset, audio_clip, skinned_model, vbd::gpu_solver) to push channel events instead of calling the static fns.

**Estimate: 1.5 hr.**

### Phase 4 — Render graph channel migration

The hard one. Today's pattern:

```cpp
auto pass = gpu_s.render_graph->add_pass<state>();
pass.color_output(...);
pass.depth_output_load();
pass.after<other_system::state>();
auto& rec = co_await pass.record();
rec.bind(...);
rec.draw(...);
```

`add_pass` and the recording closure mutate `gpu_s`. With const cross-system reads, this fails.

**New shape:**

```cpp
struct render_pass_request {
    id pass_kind;
    std::vector<color_attachment> color_outputs;
    std::optional<depth_attachment> depth_output;
    std::vector<id> after_deps;
    move_only_function<async::task<>(recording_context&)> record;
};

// Renderer's frame phase:
ctx.channels.push(render_pass_request{
    .pass_kind = id_of<my_state>(),
    .color_outputs = { color_clear{...} },
    .depth_output = depth_load{},
    .after_deps = { id_of<rt_shadow::system::state>(), ... },
    .record = [&r, frame_index, /* captures */](recording_context& rec) -> async::task<> {
        rec.bind(r.pipeline);
        // ...
        co_return;
    },
});
```

`gpu::context::frame()` drains the `render_pass_request` channel, builds passes in dependency order (topological sort by `after_deps`), executes record functions, runs the graph.

**Subtleties:**
- Move-only closures must work in `task::concurrent_queue` and channels (they already do — `move_only_function` is supported).
- The `pass.color_output(gpu::color_clear{...})` DSL flattens to a value type. Capture every shape currently used.
- `pass.track(...)` (resource lifetime tracking) becomes a vector field on the descriptor.
- `pass.reads(...)` (storage/indirect dependency declarations) becomes a vector field.
- `pass.after<X>()` is just a vector of dep ids.
- The captures in record functions today reference `r` (resources) by ref — those refs must outlive the channel pump. Since resources are owned by the same system that pushed the request, and channel processing happens within a frame, this is safe — but document it.

The render graph builder lives in gpu and is the only place that touches the graph object. The graph object never appears in any other system's signature.

**Renderer files affected**: every `_renderer.cppm/.cpp` in `Graphics/Renderers/` plus `Gui.cpp`. ~14 files. Each frame phase rewrites pass building. Bodies become flatter (no graph access, just push a request).

**Estimate: 4–6 hr.** Highest risk. Build will be broken throughout this phase.

### Phase 5 — Promote `asset::registry` to a system

Same shape as `window` and `gpu::context`:

```cpp
struct asset::registry {
    struct state {
        // resources map, loaders, file watcher, pending queues — all the existing fields
    };
    static auto initialize(const init_context&, state&) -> void;
    static auto update(update_context&, state&) -> async::task<>;
    static auto shutdown(shutdown_context&, state&) -> void;

    // existing public ops as static fns:
    template <typename T, typename Ctx> static auto add_loader(state&, Ctx&) -> ...;
    template <typename T> static auto get(const state&, std::string_view) -> resource::handle<T>;
    // etc.
};
```

Engine.cpp drops `m_assets`. The registry is registered as a system; engine looks it up via `m_scheduler.state<asset::registry::state>()` only if it needs to call something during init. Phase 5 also drops `set_asset_registry` and the `void* assets_ptr` plumbing on phase contexts. `phase.assets()` and `ctx.assets()` go away entirely — replaced by cross-system const reads of `asset::registry::state`.

**Estimate: 2 hr.** Mechanical sweep.

### Phase 6 — Audit state methods

Walk every `state` struct in the codebase. Any instance method gets moved to its system as a static fn. State is POD.

Already-clean systems (input, actions, network, save, physics, renderer, forward, etc.) likely don't have method-on-state issues since they were authored after the rule. Audit anyway.

**Estimate: 1 hr.**

### Phase 7 — Drop the `resource_context` concept

`Engine/Engine/Source/Assets/ResourceLoader.cppm` declares a generic `resource_context` concept that requires instance methods. After phase 3 those methods are gone. The concept should also go — loaders parameterize on `gpu::context::state` directly. The abstraction was never used for anything but `gpu::context`.

**Estimate: 30 min.**

## Total estimate

~12 hours of focused work.

## Recommended order

1. Phase 1 first. Standalone. Build breaks at every illegal cross-system mutation, which gives a punch list.
2. Phases 2 + 3 together. They overlap: phase 2 says "drop the queues from state," phase 3 says "they become channels." Land as one commit.
3. Phase 5. Standalone-ish; depends on phase 2 dropping `assets*` from gpu state.
4. Phases 6 + 7. Cleanup.
5. Phase 4 last. Highest risk, most invasive. Does not depend on others. Could land as a separate dedicated session.

Phases 1+2+3+5+6+7 (~7 hr) get the architecture mostly clean. Phase 4 (~5 hr) closes the last hole — the render graph escape — but is best done with focused attention on the pass DSL design.

## Open design points

- **Buffer/image creation cross-system**: today renderers do `gpu::buffer::create(gpu_s.device->allocator(), ...)` synchronously in their init. Allocator mutates internally. Two ways:
  - Mark the allocator's mutating ops as logically-const + thread-safe (the mutex pattern, like `task::concurrent_queue`). Justified at the primitive level; not state-level mutation.
  - Push a `gpu::create_buffer_request` and have gpu's update fulfill it asynchronously. Adds a frame of latency, complicates init (renderer's init can't have a buffer until next frame).

  Lean (a). The allocator is a thread-safe primitive; const-correctness there is reasonable.

- **Render graph topological order from channel events**: current code uses static `pass.after<X>()` calls to declare deps. With channel events, the dep is just an `id`. Same shape, just type-erased via id_of. The graph builder sorts. No semantic change.

- **Recording closures and lifetime**: closures capture renderer state by reference. State lives for the system's lifetime — channel events processed in the same frame as their push are safe. Document.

- **Channel timing for command_request**: pushed during update, drained during update? Or during frame? The channel snapshot model (one-frame delay) means a command pushed in frame N is drained in frame N+1. For texture upload and similar GPU work, that's fine. For mid-frame work (render passes), needs same-frame delivery, which is why render_pass_request goes through a different mechanism — gpu's frame phase reads them after collecting from all renderers' frame phases. This requires the scheduler to order gpu's frame phase last.

## Definition of done

- The `static_assert` in Phase 1 holds across the entire codebase.
- `gpu::context::state` has no methods, no inheritance, no cross-system pointers.
- Searching the codebase for `gpu_s.render_graph->add_pass` returns zero results outside `gpu::context::frame()`.
- Every renderer's frame phase pushes `render_pass_request` events and nothing else.
- `asset::registry` is a system. `phase.assets()` does not exist.
- The `resource_context` concept is gone.
- Build is green; existing scenes render identically.

---

## Status (2026-05-05)

All 7 phases complete on `forward-plus-renderer-refactor`. Build green, scene renders.

### Phase 1 — Mutable cross-system fallback removed

Done. `SystemDispatch.cppm` no longer offers `direct_state_ref_mut<T>`; cross-system params resolve to `const T&` only. Compile-time enforcement via `static_assert` on the resolver paths.

### Phase 2 — `gpu::context::state` decomposed

Done. State is a pure aggregate of `unique_ptr<...>` infrastructure (`device`, `shader_registry`, `swapchain`, `frame`, `render_graph`, `bindless_textures`) plus `frame_scheduler`. `window_state*`, `assets*`, `ui_focus`, `command_queue`, `pending_finalizations` all dropped. No methods on state. `device->allocator()` mutating ops kept inline at the primitive level (justified — thread-safe primitive).

### Phase 3 — `command_queue` / `pending_finalizations` channelled

Done. `gpu::command_request { std::function<void(gpu::context::state&)> work; }` channel pumped by `gpu::context::update`. `pending_finalization { id resource_type; id resource_id; }` channel pumped by `asset::registry::update`. The static fns `queue_gpu_command`, `mark_pending_for_finalization`, `process_gpu_queue`, and `take_pending_finalizations` are gone, along with the `asset::context` virtual base.

### Phase 4 — Render graph channelled

Done — but landed in compromised form. Renderer frame bodies push `gpu::render_pass_request` events via a custom awaitable that suspends the coroutine, hands the descriptor + handle to the engine via the channel, and resumes with a `vulkan::recording_context&`. The `gpu::context::frame()` drains the channel each frame and feeds the descriptors to `vulkan::render_graph::execute`.

The intended ergonomic was a chained builder DSL:
```cpp
auto& rec = co_await gpu::pass(ctx, trace_id<state>())
    .color(...).depth(...).after<X, Y>().reads(...).tracks(...);
```

What shipped is the descriptor-literal form because the chained builder ICEs clang-p2996 cross-TU. See `docs/compiler_compromises.md` → "Render pass channel API forced into descriptor-literal form" for the bisecting trail and the structural finding (only `async::task<T>` survives as the cross-TU await target). What renderers actually write:
```cpp
auto& rec = co_await gpu::request_pass(ctx, {
    .pass_kind = trace_id<state>(),
    .color = gpu::clear_color(...),
    .after_deps = gpu::after<X, Y>(),
    .reads = gpu::reads(...),
    .writes = gpu::writes(...),
    .tracked_buffers = gpu::tracks(...),
});
```

`gpu_s.render_graph->add_pass` returns zero hits outside `gpu::context::frame()`. The graph builder is fully internal to gpu. Files migrated (11 call sites): `CullComputeRenderer`, `DepthPrepassRenderer`, `PhysicsTransformRenderer`, `SkinComputeRenderer`, `LightCullingRenderer`, `ForwardRenderer` (×2), `PhysicsDebugRenderer`, `UiRenderer`, `CaptureRenderer`, `RtShadowRenderer`.

### Phase 5 — `asset::registry` promoted to a system

Done. `asset::registry::state` holds the resource map, loaders, file watcher, and pending queues. `initialize` / `update` / `shutdown` are static fns. All consumers (Audio API, every renderer, etc.) read assets via `phase.sched.state<asset::registry::state>()` (init phase) or `co_await ctx.state_of<asset::registry::state>()` (frame phase). `phase.assets()` and the `void* assets_ptr` plumbing are gone.

### Phase 6 — State methods audited

Done as part of phases 2/5. Every `state` struct in the codebase is a POD aggregate.

### Phase 7 — `resource_context` concept dropped

Done. `Engine/Engine/Source/Assets/ResourceLoader.cppm` is now just `loader_base` and `loader_t<T>` abstracts; the templated `resource_context` concept is gone. Loaders parameterise on `gpu::context::state` directly.

## Known limitations / follow-ups

- **Asset registry escape hatch.** `scheduler::state<asset::registry::state>()` is still used inside ~14 renderer init bodies for `asset::registry::get<X>` + `instantly_load`. This is the remaining mutation-on-cross-system-state escape from Phase 1's const-only rule. Path-decision pending — see "Step 6 decision" below.
- **Renderer params still use the state type, not the system type.** Renderer cross-system params are typed `const X::system::state&` / `const X::system::resources&` (renderers need to read state members; the system type itself has only static methods). However, the **identifier** the dispatcher uses for ordering is now uniformly `id_of<X::system>()` — `compute_state_dep_id<T>()` does parent-of reflection to map `X::system::state` → `id_of<X::system>()` at the lookup site. Dual-registration is gone.
- **`task_context::states` / `task_context::resources_store` are still public** — the typed escape-hatch methods (`state_of<>`, `try_state_of<>`, etc.) are deleted, but the underlying member access isn't yet friend-restricted. Polish-only; the dispatcher is the only remaining caller.

### Renderer body split (this branch)

Every renderer system's `initialize` / `frame` body has been moved out of its `.cppm` into a companion `.cpp` module-implementation unit:

- `SkinComputeRenderer.{cppm,cpp}`, `CullComputeRenderer.{cppm,cpp}`, `ForwardRenderer.{cppm,cpp}`, `LightCullingRenderer.{cppm,cpp}`, `PhysicsTransformRenderer.{cppm,cpp}`, `DepthPrepassRenderer.{cppm,cpp}`.
- The `.cppm` exports declarations only; the `.cpp` holds bodies.

This breaks the "cross-TU instantiation of `ensure_system<S>` reaches into `S::frame`'s coroutine body and any custom awaitable it constructs" chain that was repeatedly ICEing clang-p2996 (`<eof>` parser-at-end-of-file, `Sema::SubstType` recursion). When `invoke_frame_for<S>` is instantiated in a caller TU now, `S::frame` is just a forward declaration there; clang doesn't have to elaborate the body. Reflection on `^^S::frame` parameters still works (only the declaration is needed for `parameters_of`).

**Convention to keep:** any new renderer (or, more broadly, any system) should put its `frame` / `initialize` / `update` bodies in a `.cpp` next to the `.cppm`. Inline bodies in the `.cppm` are a regression risk for the same ICE.

---

## Phase 8 — Declared cross-system reads + init topo-sort (LANDED, except CI lint)

**Goal:** make cross-system state reads structurally enforceable. Move every cross-system read into the system's signature, delete the `state_of`/`try_state_of`/`resources_of`/`try_resources_of` escape hatches from `init_context` / `task_context` / `frame_context`, and topo-sort init order by declared deps so the manual `ensure_system` ordering convention in `Renderer.cpp` becomes unnecessary.

**Why:** the rt_shadow → forward null-deref bug fixed earlier in this branch was a symptom — forward's init read `phase.try_state_of<rt_shadow::system::state>()` before rt_shadow was initialized. That whole class of bug is now structurally impossible.

### How the ICE was resolved

The cross-TU `ensure_system<S>` ICE was the load-bearing blocker. Diagnosis (`tmp/p2996-eof-ice/` reproducer + `compiler_compromises.md`): clang-p2996 exploded its `Sema::SubstType` recursion when the dispatcher chain instantiated `invoke_frame_for<S>` cross-TU and that body had to elaborate `S::frame`'s coroutine body, which in turn referenced a fresh user-defined awaitable. `async::task<T>` survived the same chain because it was reified everywhere; bespoke awaitables didn't.

**Fix**: move every renderer's `frame` / `initialize` body from the `.cppm` into a companion `.cpp` module-implementation unit. Caller TUs now see only forward declarations of `S::frame`; the dispatcher's `invoke_frame_for<S>` instantiates fine because elaborating a *call* doesn't require the body. Reflection on `^^S::frame` only needs the declaration. Six renderer .cppms (`SkinComputeRenderer`, `CullComputeRenderer`, `ForwardRenderer`, `LightCullingRenderer`, `PhysicsTransformRenderer`, `DepthPrepassRenderer`) were split.

Validation: chained `pass_builder` DSL was restored ([RenderPass.cppm](../Engine/Engine/Source/Gpu/RenderPass.cppm)) — exactly the design pattern that previously ICEd three different ways. It now compiles cleanly.

### What landed in this branch

`Engine/Engine/Source/Ecs/SystemDispatch.cppm`:
- `dep_pointee_t<T>` alias and pointer-aware `is_state_dep_v` / `state_dep_id_v` (so `const S*` params become optional cross-system deps that resolve to `nullptr` if the producer isn't registered).
- `init_context` excluded from `is_state_dep_v`.
- `extract_init_state_deps<S>()` populated via `compute_state_dep_ids<^^S::initialize, S>()` — symmetric with the update/frame variants.
- `resolve_initialize_arg` / `resolve_update_arg` / `resolve_frame_arg` extended with pointer-type and reference-type cross-system branches plus resources-store fallback.
- `direct_state_ref<T>` falls back to resources_store when state lookup misses.
- `state_of_t<S>` falls back to `S` itself when the system has no `state` member; stateless systems use their own type as identity.
- New `register_state_dep_tags<MemberFn, S>()` runtime helper — registers `trace_id<dep_pointee_t<T>>()` for every reflected dep parameter so the closed-graph violation formatter never sees an unregistered tag.

`Engine/Engine/Source/Ecs/SystemNode.cppm`:
- `init_state_deps` field on `system_node`.
- `state_id` is now uniformly `id_of<S>()` — the system type, never the state member. Dual-registration scaffold (`legacy_state_id` field) is gone.

`Engine/Engine/Source/Ecs/Scheduler.cpp` + `.cppm`:
- `topo_sort_pending_inits()` — Kahn's algorithm over uninitialized nodes, ordered by `init_state_deps`. Indexes state IDs, legacy state IDs, and resources IDs.
- `scheduler::initialize()` runs an iterative loop: topo-sort pending → init that batch → repeat.
- `ensure_system<S>` / `add_system<S>` use `state_of_t<S>&` for return type so stateless systems substitute correctly.
- **Single canonical registration**: state registers at `id_of<S>()` only. Renderer params typed `const X::system::state&` resolve correctly because the dispatcher's `compute_state_dep_id<T>()` uses `std::meta::parent_of(^^T)` to find the owning system class — so `X::system::state` maps to `id_of<X::system>()` at lookup time. Resources and settings keep their own `id_of<X::system::resources>()` / `id_of<X::system::settings>()` (no parent-of redirect).
- `check_closed_dep_graph()` deferred to first call of `scheduler::update()` rather than each `initialize()` — the engine has a two-phase init pattern (`Engine.cpp` calls `initialize()` twice, once after foundation systems and once after game systems), and the closure check needs the full system list to be valid.
- `scheduler::shutdown()` rewritten to interleave `invoke_shutdown_fn` + `m_nodes.pop_back()` in a loop — each system's data (including GPU resources) is fully destroyed before the next system shuts down. Fixes a 104-allocation Vulkan leak that surfaced when `gpu::context::shutdown` reset the device while later renderer nodes still held buffers/pipelines.

`Engine/Engine/Source/Ecs/PhaseContext.cppm` + `TaskContext.cppm` + `FrameContext.cppm`:
- `state_of` / `try_state_of` / `resources_of` / `try_resources_of` / typed `after<>` / `notify_ready<>` are **deleted** from all three contexts. Only `read_channel`, `after_id`, `notify_ready_by_id` remain (the latter two are dispatcher-internal). Member fields (`states`, `resources_store`, etc.) are still public — friend-restricting them is a polish task.

`Engine/Engine/Source/Graphics/Renderers/Renderer.cpp`:
- All leaf renderers (`geometry_collector`, `skin_compute`, `cull_compute`, `physics_transform`, `depth_prepass`, `rt_shadow`, `light_culling`, `forward`, `physics_debug`, `ui`, `capture`) registered explicitly in the parent aggregator's `initialize`. Renderer init bodies no longer call `phase.sched.ensure_system<X>()` for ordering — topo-sort handles it via declared deps. The old manual ordering hack (`ensure_system<rt_shadow>` + `ensure_system<light_culling>` before `add_system<forward>`) is gone.

Per-system migrations from `phase.try_state_of<X>` / `co_await ctx.state_of<X>` to declared cross-system params:

| System | Phase | Added param(s) |
|--------|-------|----------------|
| skin_compute | init, frame | `gc&` (gc::resources), `gc_r&` |
| cull_compute | init | `gc_r&` |
| physics_transform | frame | `gc_r&` |
| depth_prepass | frame | `gc_r&`, `cam_state&` |
| forward | frame | `cam_state&`, `gc_r&`, `lc_r&` |
| light_culling | frame | `cam_state&` |
| rt_shadow | frame | `gc_r&` |
| geometry_collector | update | `cam_state&` |
| physics_debug | update, frame | `ps&`, `phys_cfg&`, `cam_state&` |
| camera | update | `input_state&` |
| capture | update | `actions_state&` |
| renderer (aggregator) | update | `actions_state&` |
| gui | update | `gse::input::system::state&` (fully-qualified to dodge `gui::input` widget shadowing) |
| network | update | `actions_state&`, `cam_state&` |
| physics | init, frame | `gpu_s*` (optional pointer for headless builds) |
| input | update | `win*` (optional pointer) |
| client_ui (game) | update | `pds&`, `pd_cfg&` |

### What's still open

1. **CI lint / grep guard**: forbid `phase.state_of<` / `phase.try_state_of<` / `phase.resources_of<` / `phase.try_resources_of<` / `co_await ctx.state_of<` / `co_await ctx.resources_of<` outside the dispatcher.
2. **Forbid inline `.cppm` bodies for system phase functions.** Grep-guard for `auto.*::system::(initialize|frame|update|shutdown).*\) -> .* \{` inside `.cppm` files. Prevents regression to the cross-TU ICE pattern.
3. **Friend-restrict `task_context::states` / `task_context::resources_store`.** Polish — the typed escape methods are gone, only the field access remains.
4. **Decide on `scheduler::state<X>()` and `phase.sched.state<X>()`.** Currently used for asset::registry mutations. See "Step 6 decision" below.

### Step 6 decision — `scheduler::state<X>()` and asset-registry mutation

22 callsites of `phase.sched.state<X>()` / `m_scheduler.state<X>()` remain after Phase 8 step 5:
- 5 in `Engine.cpp` bootstrap (wiring up async submitters at startup) — Engine isn't a system, this is fundamentally outside the dispatch model.
- ~14 in renderer/system init bodies doing `auto& assets = phase.sched.state<asset::registry::state>();` followed by `asset::registry::get<X>(assets, path)` + `instantly_load(...)` — this is the architectural escape; it mutates cross-system state during init.
- 2 stragglers (`gpu::context::initialize` reading `window::state`, `physics::system::initialize` for asset loading).

Four candidate paths, ordered by cost:

**Path 1 — Status quo + CI lint.** Keep `scheduler::state<X>()` public; CI greps for it in any TU under `Engine/Source/Graphics/Renderers/` and rejects (Engine bootstrap exempt). Convention-enforced, not compile-enforced. ~10 min. Doesn't fix the underlying mutation; just discourages new uses.

**Path 2 — Friend-restrict.** `scheduler::state<X>()` becomes private; friend `gse::engine` and the asset-loading helpers. Renderers calling it become compile errors. ~30 min. Hides the escape hatch behind a friend list but doesn't resolve why asset loading needs it.

**Path 3 — Migrate asset ops to const-cross-system params (recommended).** Declare `const asset::registry::state& assets` as an init param on every renderer that loads assets. Mark `asset::registry::get` / `instantly_load` / `queue` / `compile_all` as taking `const&` (internal mutability via the mutex they already have). Renderers stop calling `scheduler::state<X>()` entirely; only Engine.cpp bootstrap retains it. Asset registry stops being an architectural escape — it's a normal cross-system dep like any other system. ~1-2 hr.

**Path 4 — Channel-ify asset loading.** Async load via `asset::load_request<X>` channel; renderers `co_await` for ready handles. The audit (every renderer init does `get<shader>` → `instantly_load` → immediately query layout / build pipeline) makes the synchronous dependency chain too tight to channel-ify directly. Four sub-flavors:

| Sub-flavor | Effort | Risk | Notes |
|------------|--------|------|-------|
| 4a — `pre_initialize` + `initialize` phases | ~6 hr | Med | New dispatcher phase, asset registry update runs between phases |
| 4b — coroutine `initialize` (pumped) | ~6 hr | Low-Med | Init coroutines that suspend awaiting loads; init loop pumps asset registry's update synchronously between iterations. Removes deadlock that the original 4b had |
| 4c — async-service rename | nil | nil | Lipstick on current model. Skip |
| 4d — pre-load manifest at bootstrap | ~5 hr | Med | Engine.cpp lists every asset; renderers receive loaded handles as params. Ergonomic cost: per-asset signature param explosion forever |

**Recommendation — Path 3.** Asset registry's mutations are already mutex-protected internally; marking the public API const is just honesty. Buys 95% of Path 4's architectural benefit at 20% of the cost. Path 4b is the right pick *only* if you also want async init as a primitive for future use cases beyond assets — that's a real architectural unlock (~3-4 extra hr beyond Path 3) but Path 3 is fine for now.

### Footnote — discuss culling the API files entirely

The existing `Engine/Engine/Source/Runtime/Api/*Api.cppm` files (AudioApi, CameraApi, GuiApi, InputApi, ActionsApi, AnimationApi, AnimationGraphApi, NetworkApi, PhysicsApi, RendererApi, CoreApi) are convenience facades that reach into `engine_instance->state_of<S>()` to produce read getters and wrap channel writes. They serve three jobs:

1. Bootstrap from non-system code (`Main.cpp`, `WorldLoader.cppm`, scene setup callbacks).
2. Channel writes (audio::play, network::send, etc.) — already correct under the architecture.
3. Read access from inside game systems (`gs::client_system::update` calling `gse::network::connection_state()` etc.) — this is an architectural escape that bypasses the declared-dep convention even after Phase 8 lands.

After Phase 8, (3) becomes the only loophole left. Options:
- **Keep API files**, lint `gse::state_of(` out of any TU that defines a system.
- **Cull the API files** — every reader becomes either a declared cross-system param (when called from a system) or a direct `state_of<S>()` (when called from bootstrap). Setters stay as channel pushes but maybe move to per-system free functions (`audio::system::play`) instead of an API namespace.
- **Make API readers async** — `gse::audio::master_volume()` becomes `co_await ...`, going through a request-response channel. Heavy ergonomic cost.

**Open for discussion** — needs a decision before Phase 8 fully closes the loophole.

---

## Phase 9 — Use the system type itself as the identifier (`S::system` instead of `S::system::state`)

**Goal:** every cross-system reference — `gpu::after<X::system::state>()`, `trace_id<state>()` inside a system body, `phase.try_state_of<X::system::state>()`, `co_await ctx.state_of<X::system::state>()` — uses the **system type** as the identity, not the state member.

```cpp
gpu::after<rt_shadow::system, light_culling::system, depth_prepass::system>()
trace_id<system>()
phase.try_state_of<rt_shadow::system>()
co_await ctx.state_of<camera::system>()
```

**Why:**
- Stateless systems (skin_compute, forward, depth_prepass, physics_transform — half the renderers) shouldn't need to declare an empty `struct state {}` solely as a type-key for ordering. The system type is already a unique nominal identity; reuse it.
- Removing the `state` indirection unifies stateful and stateless systems behind one identifier convention. Today's mix (`X::system::state` for stateful, `X::system` for stateless after the partial migration in this branch) is a sharp edge in renderer code.
- `S::state` becomes a pure data carrier — when a system has mutable per-frame state, the state struct exists; when it doesn't, no boilerplate.

**Convention decision:** System type is always the identity. Stateful systems still nest `struct state { ... }` inside `system` for storage, but that name is *only* meaningful inside the dispatcher's `system_node_data<S>` and the system's own member functions. No external code references `X::system::state`; the canonical identifier for any system is `X::system`.

The alternative (folding `state` members into `S` itself) was considered and rejected — it conflates "policy struct with static functions" and "owned mutable storage", which is the precise distinction the dispatcher's split between `system_node_data<S>::state` and `S::initialize/frame/update` is trading on.

### What's already in tree (this branch)

- `state_of_t<S>` in `SystemDispatch.cppm` falls back to `S` itself when `S::state` isn't named, so the dispatcher registers stateless systems under `id_of<S>()`.
- `ensure_system<S>` / `add_system<S>` return `state_of_t<S>&` (not `typename S::state&`) — substitution-correct for both stateful and stateless cases.
- skin_compute, forward, depth_prepass, physics_transform: empty `state` structs deleted. Callers updated to `gpu::after<X::system>()`. `trace_id<state>()` inside their frame bodies → `trace_id<system>()`.
- Renderer body split (companion `.cpp` per renderer) ensures the dispatcher template chain compiles in one TU per system, so the additional `if constexpr` work this phase needs in the resolvers won't reignite the ICE.

The four stateful renderers (cull_compute, light_culling, rt_shadow, geometry_collector) and the engine systems (camera, audio, network, physics, gui, animation, asset registry) still expose `S::system::state` as their identity. That mixed convention is what Phase 9 closes.

### What's left

1. **Resolver: route cross-system params through the system type.** Update `resolve_*_arg<Arg, S>` so `Arg = const X::system&` is treated as a cross-system const-state read against `id_of<X::system>()`. Today the resolver matches `Arg` against `state_of_t<S>` (own state) or pointer/reference-to-arbitrary types looked up by `id_of<dep_pointee_t<Arg>>()`. With the `state_of_t` fallback already in place, the existing logic does the right thing for stateless `X` — the work is making it also work for stateful `X` by registering the storage under `id_of<X::system>()` *as well as* `id_of<X::system::state>()` during the migration window, then dropping the latter.
2. **Migrate the stateful systems' identity.** Pick a system, change `gpu::after<X::system::state>()` / `trace_id<state>()` / `co_await ctx.state_of<X::system::state>()` / `phase.try_state_of<X::system::state>()` callsites to use `X::system`, update the dispatcher's registration to key on `id_of<X::system>()`, and verify ordering still works. Repeat per system. Stateful systems retain `struct state { ... }` as a nested member type for storage, but its name is no longer load-bearing.
3. **Cross-system param signatures.** `S::frame(..., const Y::system::state& y_state)` → `S::frame(..., const Y::system::state& y_state)` keeps working as a renderer-internal concession (renderer body reads state members), but the *identifier* the dispatcher uses for ordering is `id_of<Y::system>()`. Two options for cleanup:
   - Keep `const Y::system::state&` in the signature — the dispatcher's `is_state_dep_v` uses `dep_pointee_t<Arg>` which strips to `Y::system::state`, and the resolver looks up by `id_of<Y::system::state>()`. To keep this working, register stateful state under both ids during migration. Drop `id_of<state>` once nobody depends on it.
   - Move state reads into the renderer body via `co_await ctx.state_of<Y::system>()` returning `const Y::system::state&` (typed via `state_of_t`). Lets you drop the cross-system param from the signature entirely. More invasive.
4. **Update the API files** (`Engine/Engine/Source/Runtime/Api/*Api.cppm`) — their `state_of<S>()` calls become `state_of<S::system>()`. Same change in the API readers' implementations.
5. **Lint guard.** Once migration is complete, grep-fail any `::system::state` reference outside of the system's own `.cppm` definition (where it's still allowed as the storage member type). Catches regressions and signals the convention to anyone adding a new system.

**Estimate:** ~3 hr — mostly mechanical greps. Biggest risk is `co_await ctx.state_of<X::system::state>()` callsites where the renderer body actually reads state members; those switch to `state_of<X::system>()` (returning `const state_of_t<X::system>&` = `const X::system::state&` for stateful X) — no caller-visible behavior change, just an identifier rename.

**Order of work:** finish Phase 8 first (declared cross-system reads + topo-sort) so the migration in step 2 can happen with the topo-sort actively keeping ordering correct as identifiers move. Reverse order is fine if Phase 8 stalls — Phase 9 doesn't structurally require Phase 8 — but Phase 8 makes the verification cheaper.

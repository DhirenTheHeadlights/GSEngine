# Coroutine Scheduler Unification

## Overview

Unify the update and frame tiers onto a single coroutine-based execution model, eliminate the lambda-based scheduling APIs, and fix an input-loss race in the process. The update tier currently uses `graph_update() -> void` dispatched in parallel via `task::group`, with no cross-system ordering primitive. The frame tier already uses `graph_frame() -> async::task<>` with `co_await ctx.after<State>()` for dependency ordering. This refactor brings the update tier up to the same model, then goes further — replacing `ctx.schedule(lambda)` with coroutine-signature acquisition and `pass.record(lambda)` with `co_await pass.record()`.

## Motivating Bug

Mouse clicks and key presses intermittently fail to register. The user must press repeatedly before input takes effect.

**Root cause.** `scheduler::run_graph_update` (`Engine/Engine/Source/Core/Utility/Concurrency/Scheduler.cppm:311`) posts every system node into a `task::group` with no ordering:

```cpp
task::group group(find_or_generate_id("scheduler::parallel_updates"));
for (const auto& node : m_nodes) {
    group.post([&, node = node.get()] {
        node->graph_update(u_ctx);
    }, node->trace_id());
}
group.wait();
```

`input::system::update` (`Engine/Engine/Source/Platform/GLFW/Input.cppm:164`) and `actions::system::update` (`Engine/Engine/Source/Platform/GLFW/Actions.cppm:680`) run concurrently. Actions reads `input_state->current_state()` (the read side of a `double_buffer<state>`) while input is mid-frame: calling `begin_frame()` to clear transient sets, replaying events into the write side, then `flip()`-ing the buffer.

Two failure modes:

1. **Stale read**: actions runs before input's `flip()` — sees last frame's state. Transient flags (`keys_pressed_this_frame`, `mouse_buttons_pressed_this_frame`) that only live for one frame get missed.
2. **Torn read**: `double_buffer<T>` has only two slots (`Engine/Engine/Source/Core/Utility/Concurrency/NBuffer.cppm`). After `flip()`, the reader's buffer becomes the next write target. If actions is still reading when input begins the next frame's write, values tear.

Mouse position is unaffected because it is persistent state, not transient per-frame state. Clicks, key presses, and key releases all use transient tracking that is cleared every frame by `begin_frame()`.

## Target Architecture

### Unified Execution Model

Both tiers use the same shape:

```
system function returns async::task<>
  co_await ctx.after<OtherSystemState>()    // cross-system ordering
  co_await ctx.acquire<write<T>, read<U>>() // component access
  co_await pass.record()                    // render pass body (frame tier)
  trace::scope_guard _{"phase-name"}        // RAII sub-span tracing
```

One mental model, one coroutine vocabulary, no lambdas for scheduling or recording.

### Context Hierarchy

```
task_context (base)
  - snapshots, channels, channel_reader, resources, graph
  - state_of<>, try_state_of<>, read_channel<>, resources_of<>, try_resources_of<>
  - after<State>, notify_ready<State>

update_context : task_context
  - registry& reg (mutable, hidden behind defer_*)
  - defer_add, defer_remove, defer_activate
  - acquire<read<T>, write<U>, ...>

frame_context : task_context
  - const registry& reg
  - (no component acquisition — reads go through snapshots)
```

### Tracing

Awaiter hooks fire `trace::begin_async` / `trace::end_async` around every `co_await` point. `task::group::post` already wraps each posted coroutine in `trace::scope(node_trace_id)` (`Engine/Engine/Source/Core/Utility/Concurrency/Task.cppm:202`). RAII `trace::scope_guard` added for user-defined sub-spans inside coroutine bodies. No per-site markup needed at typical call sites.

---

## Phase 1: Update Tier Coroutine Conversion

**Goal.** Convert `graph_update` to `async::task<>`, add `after<>` / `notify_ready<>` to `update_context`, make `actions::system::update` wait on `input::system_state`. Fixes the input-loss bug.

### 1.1 Task Graph State Notification

The frame tier uses `task_graph::wait_state_ready` / `notify_state_ready` (`Engine/Engine/Source/Core/Utility/Ecs/FrameContext.cppm:76-83`). Verify these are already general-purpose (they should be — they key on `std::type_index`, not frame-specific).

### 1.2 update_context Additions

In `Engine/Engine/Source/Core/Utility/Ecs/UpdateContext.cppm`:

- Add `template <typename State> auto after() -> async::task<>`
- Add `template <typename State> auto notify_ready() -> void`

Implementations mirror `frame_context::after` and `frame_context::notify_ready` verbatim — both delegate to `graph.wait_state_ready` / `graph.notify_state_ready`.

### 1.3 system_node_base Signature Change

In `Engine/Engine/Source/Core/Utility/Ecs/SystemNode.cppm`:

```cpp
virtual auto graph_update(update_context&) -> async::task<> = 0;
```

Update `system_node<S, State>::graph_update` to return `async::task<>`. After dispatching `S::update`, call `ctx.notify_ready<State>()` so downstream awaiters resume.

### 1.4 Scheduler Run Loop

Replace `run_graph_update`'s `task::group` loop with the same shape as `run_graph_frame`:

```cpp
std::vector<async::task<>> tasks;
tasks.reserve(m_nodes.size());
for (const auto& node : m_nodes) {
    tasks.push_back(node->graph_update(u_ctx));
}
async::sync_wait(async::when_all(std::move(tasks)));
```

The deferred-ops drain and `collected_work` submission after the coroutine wait remain as today.

### 1.5 System Signature Migration

Every existing `static auto update(update_context&, ...) -> void` becomes `-> async::task<>`. Bodies stay the same except for adding `co_return;` at the end (or converting an early `return;` to `co_return;`).

### 1.6 Actions Dependency

In `Engine/Engine/Source/Platform/GLFW/Actions.cppm:680`:

```cpp
static auto update(update_context& ctx, system_state& s) -> async::task<> {
    co_await ctx.after<input::system_state>();
    // rest of the existing body
    co_return;
}
```

Physics, rendering, audio, and everything else that does not depend on input continues to run in parallel with input.

### 1.7 Verification

Input-loss bug fixed. Trace `physics::system` span and `actions::system` span: physics should still overlap input, actions should start after input's span ends.

---

## Phase 2: task_context Base

**Goal.** Eliminate duplication between `update_context` and `frame_context`.

### 2.1 Extract task_context

New file `Engine/Engine/Source/Core/Utility/Ecs/TaskContext.cppm`:

```cpp
struct task_context : phase_gpu_access {
    const state_snapshot_provider& snapshots;
    channel_writer& channels;
    const channel_reader_provider& channel_reader;
    const resources_provider& resources;
    task_graph& graph;

    template <typename State> auto state_of() const -> const State&;
    template <typename State> auto try_state_of() const -> const State*;
    template <typename T>     auto read_channel() const -> channel_read_guard<T>;
    template <typename R>     auto resources_of() const -> const R&;
    template <typename R>     auto try_resources_of() const -> const R*;
    template <typename State> auto after() -> async::task<>;
    template <typename State> auto notify_ready() -> void;
};
```

### 2.2 Inherit

```cpp
struct frame_context : task_context {
    const registry& reg;
};

struct update_context : task_context {
    registry& reg;
    std::vector<scheduled_work>& work;
    std::mutex& work_mutex;
    mpsc_ring_buffer<std::move_only_function<void()>, 64>& deferred_ops;
    // acquisition + deferral API (see Phase 4)
};
```

Delete the duplicated templates from both subclasses.

### 2.3 Scheduler Construction

`scheduler::run_graph_update` and `run_graph_frame` construct their contexts by initializing the `task_context` base with the shared fields and the subclass fields with the tier-specific ones.

---

## Phase 3: Coroutine-Signature Component Acquisition

**Goal.** Replace `ctx.schedule(lambda)` with direct coroutine acquisition. Delete the scheduled-work plumbing.

### 3.1 Acquisition Primitive

Add to `task_context` (or `update_context` only, since frame tier uses snapshots):

```cpp
template <typename... Accesses>
auto acquire() -> acquisition_awaitable<Accesses...>;
```

Where `Accesses` is a pack of `read<T>` / `write<T>`. The awaitable suspends until all requested components are available (no active writers for reads, no active readers/writers for writes), then resumes with a `std::tuple<Accesses...>` of span-owning handles. Handles release on destruction.

### 3.2 Acquisition Ordering

Multi-acquisition is atomic — all requested accesses granted together or none. No single-component acquire API exists, which prevents lock-ordering deadlocks by construction. Any system needing phase-separated access re-acquires in sequence via scoped blocks (the RAII handles release at block exit).

### 3.3 Signature Acquisition

`SystemNode` already uses `lambda_traits` to auto-wire system function signatures (snapshots, resources, frame_data). Extend to `read<T>` / `write<T>` parameters on `update`:

```cpp
auto update(
    update_context& ctx,
    system_state& s,
    write<motion_component> motion,
    write<collision_component> collision
) -> async::task<> {
    co_await ctx.after<input::system_state>();
    for (auto& m : motion) { ... }
    co_return;
}
```

The dispatch wrapper (`SystemNode.cppm:420-460`) acquires the declared components before invoking the coroutine, so the body runs with access already held. Identical semantics to today's `schedule(lambda)` with the lambda params, but no lambda.

### 3.4 Delete scheduled_work

- `scheduled_work` struct (`UpdateContext.cppm:17-22`)
- `work`, `work_mutex` fields on `update_context`
- `collected_work`, `collected_work_mutex` locals in `scheduler::run_graph_update` (`Scheduler.cppm:313-314`)
- `update_context::schedule` method
- The post-coroutine block in `run_graph_update` at `Scheduler.cppm:339-350` that submits collected work to `m_update_graph` and executes it
- `m_update_graph` field on `scheduler` if nothing else uses it (check)

### 3.5 Per-System Migration

Every `ctx.schedule([captures](read<T>, write<U>) { body; })` becomes either:

- Signature acquisition: move the lambda params to the system's `update` signature, inline the body.
- Mid-body acquisition (rare): `auto [a, b] = co_await ctx.acquire<read<T>, write<U>>(); body;`

Migrate in any order — the `schedule` API can coexist with the new primitive during transition, deleted at the end.

---

## Phase 4: Coroutine Trace Hooks

**Goal.** Every `co_await` point and every RAII sub-span captured in the profiler without per-site markup.

### 4.1 Awaiter Hooks

In `Engine/Engine/Source/Core/Utility/Concurrency/AsyncTask.cppm`, extend the awaiter types returned by `ctx.after<>`, `ctx.acquire<>`, and (Phase 6) `pass.record()` with trace calls:

```cpp
auto await_suspend(std::coroutine_handle<> h) -> void {
    m_trace_span = trace::begin_async(awaiter_trace_id<Awaiter>());
    // existing suspension logic
}

auto await_resume() -> result_type {
    trace::end_async(m_trace_span);
    // existing resume logic
}
```

`awaiter_trace_id` derives a stable `trace::id` from the template parameters (e.g. `"after<input::system_state>"`, `"acquire<write<motion_component>>"`).

### 4.2 scope_guard

Add to `Engine/Engine/Import/Trace.cppm` a RAII wrapper around `begin_block` / `end_block`:

```cpp
class scope_guard {
public:
    explicit scope_guard(trace::id id);
    ~scope_guard();
    scope_guard(const scope_guard&) = delete;
    scope_guard& operator=(const scope_guard&) = delete;
private:
    trace::id m_id;
};
```

Usage inside coroutine bodies for user-defined sub-spans:

```cpp
auto _ = trace::scope_guard(find_or_generate_id("physics::integrate"));
integrate(motion, collision);
```

### 4.3 Verification

Profile output distinguishes time spent suspended (`after<T>`, `acquire<T>`) from time spent executing. `profile.txt` shows nested spans per coroutine matching today's `trace::scope(lambda)` nesting at `Physics/System.cppm:1052`.

---

## Phase 5: Render Record Coroutine

**Goal.** Replace `pass.record(lambda)` with `co_await pass.record()` returning a `recording_context&`. Renderer bodies go inline.

### 5.1 Awaitable Record

In `Engine/Engine/Source/Platform/Vulkan/RenderGraph.cppm`, replace:

```cpp
auto record(std::move_only_function<void(recording_context&)> fn) -> void;
```

with:

```cpp
auto record() -> record_awaitable;
```

`record_awaitable::await_suspend` stores the coroutine handle on the pass. `render_graph::execute` resumes the handle on a worker thread (matching today's `task::group::post` at `RenderGraph.cppm:1446`) when the pass is scheduled for recording, with its secondary command buffer wrapped in a `recording_context` returned from `await_resume`.

### 5.2 Renderer Conversion

Each renderer `frame()` coroutine replaces:

```cpp
.record([captures...](const recording_context& rec) {
    rec.set_viewport(ext);
    rec.set_scissor(ext);
    // draw calls
});
```

with:

```cpp
auto& rec = co_await pass.record();
rec.set_viewport(ext);
rec.set_scissor(ext);
// draw calls
```

Captures become coroutine-frame locals. Lifetime of captured buffers (e.g. `meshlet_writer`, `skinned_writer` in `ForwardRenderer.cppm:384-470`) now spans the whole coroutine instead of the record lambda — acceptable, since those buffers are needed for the recording anyway.

### 5.3 One Pass Per Coroutine

Enforce 1 pass per `frame()` coroutine. Renderers that declare multiple passes split into separate system functions. Avoids the resume-ordering complexity of multiple awaits interleaving with other passes being recorded in parallel.

### 5.4 Delete record Lambda Path

After migration:

- Delete `record(std::move_only_function<void(recording_context&)>)` overload
- Delete `pass_data::record_fn` field if replaced by coroutine handle storage
- Trace hooks from Phase 4 now cover record spans automatically

---

## Phase 6: API Hardening

**Goal.** Make misuse un-spellable. Every sharp edge still reachable today either disappears or becomes a compile error.

### 6.1 Private `reg` Behind Safe Wrappers (Absorbs Old Phase 3)

Make `m_reg` a private member of both `update_context` and `frame_context`. Users never see `registry` directly. Each context exposes only the wrapper methods that are known-safe for its tier.

`frame_context` wrappers (all const, all read-through-snapshot or const-registry):

```cpp
class frame_context : public task_context {
public:
    template <typename T> auto linked_objects() const -> std::span<const T>;
    template <typename T> auto try_linked_object(id owner) const -> const T*;
    // ... any other const read paths actually needed by renderers
private:
    const registry& m_reg;
};
```

`update_context` wrappers (read-only views + deferred mutation):

```cpp
class update_context : public task_context {
public:
    template <is_component T> auto linked_objects() const -> std::span<const T>;
    template <is_component T> auto try_linked_object(id owner) const -> const T*;

    template <is_component T, typename... Args>
    auto defer_add(id entity, Args&&... args) -> void;
    template <is_component T>
    auto defer_remove(id entity) -> void;
    auto defer_activate(id entity) -> void;
private:
    registry& m_reg;
    mpsc_ring_buffer<std::move_only_function<void()>, 64>& m_deferred_ops;
};
```

No public accessor returns `registry&`. No public method returns a mutable `std::span<T>` into component storage. The scheduler (friend) constructs the contexts with the private references; everybody else goes through the curated API.

Audit existing call sites that reach into `ctx.reg.*`:
- `ForwardRenderer.cppm:253-255` (`reg.linked_objects_read<directional_light_component>`) — becomes `ctx.linked_objects<directional_light_component>()`.
- `PhysicsDebugRenderer.cppm:366-388`, `LightCullingRenderer.cppm:197-199` — same rewrite.

Any remaining caller that was relying on mutable registry access is exactly the misuse this phase targets. Route it through `defer_*` or promote the access to a coroutine-signature `read<T>` / `write<T>`.

Delete the following in the same change — previously listed as Phase 3, now subsumed:

- `update_context::read()` / `update_context::write()` methods at `UpdateContext.cppm:117-125`. Zero call sites (verified by grep — all `read<T>`/`write<T>` appearances are lambda parameter types, not method calls).
- `registry::acquire_read` / `registry::acquire_write` at `Registry.cppm:558-566`. The only non-dead path through them was via the deleted context methods; with `m_reg` private, no user code can reach them anyway.

### 6.2 Signature-Declared State Dependencies

Today's pattern is a two-step trap — declare the ordering, then fetch the state, and nothing links the two:

```cpp
co_await ctx.after<input::system_state>();
const auto& in = ctx.state_of<input::system_state>();
```

Extend the `SystemNode` dispatch wrapper (`SystemNode.cppm:420-460`) to auto-wire `const T&` parameters where `T` is another system's state. The wrapper inserts `co_await ctx.after<T>()` and `state_of<T>()` before invoking the coroutine body:

```cpp
auto update(
    update_context& ctx,
    system_state& s,
    const input::system_state& in
) -> async::task<> {
    // body; `in` already valid, ordering already satisfied
    co_return;
}
```

Cross-system dependencies become visible at the top of every system function. Forgetting the await is impossible — the state is physically unreachable without declaring it in the signature.

Delete `state_of<T>()` / `try_state_of<T>()` from `task_context` for the update tier, or keep them as `co_await`-style fused primitives (see 7.3). Frame tier keeps `state_of<T>()` for reading snapshots — frame-tier snapshots are already frozen at tier boundary, so the race doesn't apply.

### 6.3 Fused `co_await ctx.state<T>()` (Mid-Body Escape Hatch)

For dynamic/conditional cross-system reads that don't fit a static signature declaration:

```cpp
template <typename State>
auto update_context::state() -> async::task<const State&>;
```

Returns a coroutine that does `after<T>` + read in one call. Replaces the bare `state_of<T>()` on `update_context` so a caller can't accidentally skip the ordering step.

### 6.4 Access Tokens Scheduler-Constructible Only

`gse::read<T>` and `gse::write<T>` become move-only, non-default-constructible, private constructors. An `access_token` tag is required to construct — its own constructor is private and friended only by `scheduler` and `system_node`.

```cpp
template <typename T>
class read {
public:
    // ... read-only span-like surface ...
    read(read&&) noexcept = default;
    auto operator=(read&&) noexcept -> read& = default;
    read(const read&) = delete;
    auto operator=(const read&) -> read& = delete;
private:
    friend class scheduler;
    template <typename S, typename State> friend class system_node;
    read(access_token, std::span<const T>);
    std::span<const T> m_data;
};
```

After this phase, the only way to obtain a `read<T>` / `write<T>` is through coroutine-signature declaration or `co_await ctx.acquire<>()`. No public spellable alternative exists.

### 6.5 Cycle Detection on Dependency Graph

Section 6.2 gives the scheduler static visibility into every system's cross-system reads. At `scheduler::initialize` (after all `add_system` calls), walk the declared dependency graph and assert no cycles. If `A` declares `const B::state&` and `B` declares `const A::state&`, fail fast with both names in the error rather than deadlocking on first frame.

Strengthened variant: at `scheduler::add_system` itself, a `static_assert` walks the already-registered template types against the new one. Compile-time detection for the whole registered set. Requires the dependency list to be extractable at constant-evaluation time from each system's signature, which is feasible via `lambda_traits`-style inspection.

### 6.6 Move-Only Pass-Scoped `recording_context`

`co_await pass.record()` returns `recording_context` by value, not by reference:

```cpp
class recording_context {
public:
    recording_context(recording_context&&) noexcept = default;
    auto operator=(recording_context&&) noexcept -> recording_context& = default;
    recording_context(const recording_context&) = delete;
    auto operator=(const recording_context&) -> recording_context& = delete;
    ~recording_context();  // submits the secondary
    // ... bind, draw, dispatch, etc.
private:
    friend class render_pass;
    recording_context(access_token, secondary_command_buffer);
    secondary_command_buffer m_cb;
};
```

Can't escape the coroutine frame, can't be copied into a lambda that outlives the pass, destructor submits the secondary even on exception. Eliminates the "recorded a command after the pass ended" bug class.

### 6.7 (Optional, Deferred) Typed Channel Publish/Subscribe Sets

Not in the critical path. Land only if channel misuse shows up as an actual bug:

```cpp
struct actions::system {
    using channels_published = std::tuple<button_channel, axis1_channel, axis2_channel, save::int_map_sync>;
    using channels_subscribed = std::tuple<>;
};
```

`ctx.channels.push<T>()` gated on `T` appearing in the caller's `channels_published` via a concept. Same for `read_channel<T>`.

**Cost:** boilerplate per system. **Payoff:** channel graph visible in type space. Hold in reserve.

---

## Optional: Triple Buffer Input State

**Goal.** Belt-and-suspenders protection against torn reads even after Phase 1 eliminates the ordering race.

In `Engine/Engine/Source/Platform/GLFW/Input.cppm:77`:

```cpp
triple_buffer<state> states;
```

`n_buffer<T, 3>` already handles the writer-avoids-reader-slot case correctly (`NBuffer.cppm:53-59` has an `N > 2` branch that skips the reader's index). Zero code change beyond the type alias.

Only matters if a future non-coroutine consumer reads the state concurrently with input. Safe to defer until that situation arises.

---

## Deleted Code Summary

Once all phases land:

- `scheduled_work` struct (`UpdateContext.cppm:17-22`)
- `update_context::schedule` method
- `update_context::read` / `update_context::write` methods
- `update_context::work`, `work_mutex` fields
- `scheduler::run_graph_update` post-coroutine submission block (`Scheduler.cppm:339-350`)
- `task::group`-based parallel dispatch in `run_graph_update`
- `registry::acquire_read` / `registry::acquire_write` (unsafe escape hatches)
- Duplicated context templates (moved to `task_context` base)
- `pass_builder::record(std::move_only_function<...>)` lambda overload
- Per-renderer capture lists for record lambdas
- Public `reg` member on both contexts (hidden behind safe wrappers)
- Bare `state_of<T>()` on `update_context` (replaced by signature injection or fused `co_await ctx.state<T>()`)
- Public constructors on `gse::read<T>` / `gse::write<T>` (replaced with access-token-gated construction)

## Sequencing

Ship phases in order. Each phase is independently valuable and independently testable:

| Phase | Value | Risk | Blocks |
|-------|-------|------|--------|
| 1 | Fixes input bug | Low | Nothing |
| 2 | Deduplication | Very low | Nothing |
| 3 | Eliminates lambda scheduling | Medium | Phase 1 |
| 4 | Profiling fidelity | Low | Phase 1 |
| 5 | Eliminates record lambda | Medium | Phases 1, 3, 4 |
| 6 | API hardening (misuse un-spellable) | Medium | Phases 1, 3 |

Phase 1 is the critical path for the bug. Phase 2 can land in parallel or interleaved with 1. Phase 4 is recommended before Phase 3 so coroutine-body profiling is already in place when the migration removes today's per-lambda trace spans. Phase 6 lands after Phase 3 because it hardens APIs introduced or retained in 1/3 (`acquire`, context fields, access tokens); 6.1 (private `reg` + wrappers, which absorbs the old Phase 3's registry cleanup) can land anytime after Phase 2 since it is independent of the coroutine work.

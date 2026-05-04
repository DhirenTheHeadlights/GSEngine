# Settings as ECS State

Promote settings from "a sub-struct hanging off `state`" to a first-class ECS concept the scheduler manages, with channel-based mutation and reaction.

## Motivation

Today:

- Each system has a `settings` sub-struct member of `state`. Phase functions read `s.settings.x`.
- Save holds raw `void*` to live struct fields. Lifetime is implicit, fragile if a state moves or gets destroyed mid-frame.
- Classes like `window`, `gpu::context`, `vulkan::device` register *themselves* with save (`save::register_struct(save_state, "Window", *this)`), entangling persistence machinery with constructor-time concerns and pre-init phases.
- The settings panel mutates the live ref via `gui::slider<T>::draw(... T& value ...)` — direct write through a chain of references that crosses system boundaries.

This works but the coupling is brittle. Settings end up as data the engine cares about, but it's stored and read like an ad-hoc field in random structs.

Goal: settings is a top-level scheduler concept, owned centrally, mutated through channels, and projected to consumers (systems + save + panel) as read-only views.

## Design

### Storage

Each system declares a settings type:

```cpp
struct physics::system {
    using settings = physics::settings;   // type alias, optional — no alias means "no settings"
    static auto initialize(...);
    static auto update(...);
};
```

The scheduler stores one instance of `physics::settings` next to where it stores `physics::state`, `physics::system::update_data`, `physics::system::frame_data`. Same lifetime, same registry, same `id_of<T>` lookup.

This is the same pattern that already exists for `update_data` / `frame_data` / `resources` — settings becomes the seventh storage class.

### Phase parameter resolution

Phase functions can declare `settings&` (or a const ref) as a parameter. The resolver matches it the same way it matches `state&` / `update_data&` today.

```cpp
// Read-only — most update / frame phases
static auto update(
    update_context& ctx,
    const physics::settings& cfg,
    state& s,
    update_data& ud
) -> async::task<>;

// Mutable — typically only the bootstrap initialize, or a custom settings-applier system
static auto initialize(
    const init_context& phase,
    physics::settings& cfg,   // mutable so init can populate defaults if needed
    state& s
) -> void;
```

**Convention:** `const settings&` for read access, `settings&` only when the system genuinely owns the value. Most systems will only take const refs.

Cross-system reads use the existing `ctx.state_of<T>()` shape, extended to settings:

```cpp
const auto* phys_cfg = ctx.try_state_of<physics::settings>();
if (phys_cfg && phys_cfg->use_gpu_solver) { ... }
```

### Mutation: channel-based, not direct writes — **CONFIRMED**

The gui slider widget keeps its current `T& value` API for direct in-place editing of *non-settings* values (debug overlays, ad-hoc tweakable fields), but for **settings specifically** the panel publishes mutation requests instead of writing through a ref. No live ref crosses a system boundary for settings mutations.

Two channels per settings type:

```cpp
namespace gse::settings {
    template <typename T>
    struct change_request {
        // Apply this delta to the current settings.
        // Implemented as a closure for flexibility; could be plain field+value pair.
        std::function<void(T&)> apply;
    };

    template <typename T>
    struct changed {
        T old_value;
        T new_value;
    };
}
```

A settings-applier system runs once per frame, reads `change_request<T>` channel, applies each one to the central `T` instance, publishes `changed<T>{old, new}` for downstream consumers, and writes the new value into the save snapshot.

The settings panel doesn't hold any ref into a system's state. It walks the panel registry, draws widgets bound to a *local copy* of each setting (read fresh from `state_of<T>()` each frame), and on user interaction publishes a `change_request<T>`. The panel never writes a real settings value; the settings system does, on its terms, in the right phase.

The slider widget for settings becomes a different overload (or a wrapper):

```cpp
namespace gse::gui {
    template <typename T>
    struct settings_slider {
        struct params {
            std::string_view name;
            T value;                 // current value, read-only
            T min, max;
            std::function<void(T)> on_change;   // emits change_request through caller's channel
        };
        // ...
    };
}
```

Or simpler — the existing `slider<T>` keeps its API, but the settings panel constructs a *throwaway* local copy of the field, hands its address to the slider, and after the call diffs against the previous value to detect a change and publish the request:

```cpp
// inside draw_struct_thunk for a setting field:
F local = ref;             // read live value
F before = local;
b.draw<gui::slider<F>>({ .name = label, .value = local, .min = ..., .max = ... });
if (local != before) {
    push_settings_request<TopLevel>([new_value = local](TopLevel& cfg) {
        cfg.[:m:] = new_value;
    });
}
```

Where `ref` is now the *settings registry's* copy, not a pointer threaded through anyone else's state. The diff-and-emit pattern keeps the slider widget's interface unchanged and isolates settings to a well-defined channel pathway.

### Reaction: `settings_changed<T>` events — **CONFIRMED**

Subscribe model. For systems that need to *do something* when a setting changes (window applying fullscreen, GPU enabling validation layers), they read `changed<T>` events from the channel:

```cpp
auto window_system::update(
    update_context& ctx,
    const window_settings& cfg,
    window& w
) -> async::task<> {
    for (const auto& [old_v, new_v] : ctx.read_channel<settings::changed<window_settings>>()) {
        if (new_v.fullscreen != old_v.fullscreen) {
            w.apply_fullscreen(new_v.fullscreen);
        }
        if (new_v.monitor.value != old_v.monitor.value) {
            w.apply_monitor(new_v.monitor.value);
        }
    }
    co_return;
}
```

No more "diff against last-frame's value in update". The settings-applier system already computed the diff; everyone else just reads the change event.

**Restart-required** settings: a `restart_required` flag on the change event, set by the settings-applier when it sees an annotated field. Systems that subscribe choose to honor the change immediately or stash it pending a restart.

### Save: snapshot-based, no live pointers — **CONFIRMED**

Save no longer holds `void*` into other structs. Instead, save subscribes to `settings::changed<T>` (for every registered T) and keeps an internal **snapshot map** of `category → field → string`. On save-to-file, it emits the snapshot. On load-from-file, it parses and publishes initial `change_request<T>` for each loaded value, which the settings-applier handles like any other mutation. Save's lifetime is fully decoupled from any other system's data.

```cpp
namespace gse::save {
    struct state {
        // No more void* + write_thunk + read_thunk.
        std::unordered_map<std::string,
            std::unordered_map<std::string, std::string>> snapshot;
        std::filesystem::path auto_save_path;
        bool auto_save = false;
    };
}
```

Systems still register their settings type at scheduler `add_system` time (so save knows the schema), but no pointer is exchanged. The settings-applier publishes "current value of T as INI strings" each time T changes; save updates its snapshot.

This means save's lifetime is fully decoupled from live system state. State structs can move, get destroyed, get recreated — save's snapshot doesn't care, it's bytes the settings-applier handed over.

### Auto-install on system add

When `add_system<S, State>(reg)` runs and detects `S::settings`, the scheduler automatically:

1. Allocates a `settings_storage<S>`.
2. Pushes `save::register_settings_type{T_info, category}` so save records the schema.
3. Pushes `settings::register_panel<T>` so the gui panel can render it.
4. Posts initial-state to load (if loading from file at this moment).

Category derived from a `static constexpr std::string_view category` member on the settings struct, or from `gse::meta::group_name<T>()` reading a `[[=group]]`-style annotation.

User code stops calling `gse::settings::install(phase, "Cat", s.settings)`. It just declares `using settings = ...;` and the scheduler does the rest.

### Window/Context/Device migration

These classes today own settings as private members and self-register. The new model:

1. **Extract a real settings struct** (`window_settings`, `graphics_settings`, `vulkan_settings`).
2. **Class no longer owns the settings.** Constructor takes `const settings&` (or a settings ref scoped to the system that drives it).
3. **The system owning that class is the one with `using settings = ...;`** — e.g. the GPU system declares `vulkan_settings` and passes it to the device when creating it.
4. **Window's reaction logic moves into a system** (or a method called from a system) that reads `settings::changed<window_settings>`.

Net: the constructor-time pre-init quirk (`save::register_struct(save_state, "Window", *this)` called before any phase exists) goes away. Settings registration is uniformly an `add_system` consequence.

## Scope of work, ordered

### Phase 1 — ECS foundation (engine-side, do once)

- Add `settings_storage<S>` to `system_node`. Lifecycle parallel to `update_data_storage<S>`.
- Add `S::settings` type alias detection.
- Resolver: extend `resolve_initialize_arg` / `resolve_update_arg` / `resolve_frame_arg` / `resolve_shutdown_arg` to recognize `S::settings` and `const S::settings`.
- Extend `init_context::state_of<T>` / `try_state_of<T>` etc. to find settings (transparently, since they live in the same registry).

Touches: `gse.ecs:phase_context`, `gse.ecs:scheduler`, `gse.ecs:system_dispatch`, `gse.ecs:system_node`.

Estimated: ~200 LOC, 1-2 hours.

### Phase 2 — Settings-applier system

- New `gse::settings::system` that reads `change_request<T>` for each registered T, applies via the closure, pushes `changed<T>`.
- New channels: `change_request<T>` (input), `changed<T>` (output, broadcast).
- Settings-applier holds the *only* mutable ref to each settings instance during the apply phase.

Touches: new `gse.settings:applier` partition or fold into the existing `gse.graphics:settings`.

Estimated: ~80 LOC.

### Phase 3 — Save channel-ification

- Replace `save::persisted` (which holds `void*` + `write_fn` + `read_fn`) with `save::snapshot` (just the doc) plus a `save::schema_entry { string category; vector<field_info> fields; }` map.
- Save subscribes to `settings::changed<T>` for every registered type and updates the snapshot.
- Save-to-file iterates the snapshot. Load-from-file parses, then for each (category, field, value) emits a `settings::change_request<T>` (resolved via the schema map).
- Drop `register_struct(state&, ...)` overload — the live-state path is no longer needed. Channel-based registration only.
- `read_one<T>(path, cat, name, fallback)` keeps working for early-read (Vulkan validation flag) — it just parses the file directly without consulting save's runtime state.

Touches: `gse.save:save_system`. Big rewrite, ~150 LOC delta.

### Phase 4 — Panel: read-only + diff-and-emit

- `draw_struct_thunk<T>` reads from a panel-local copy of T, draws widgets against the copy, diffs after each draw call, and publishes `change_request<T>` for any deltas.
- The panel registry still walks settings types, but it stores `(category, T_info, getter)` where the getter copies the current value out of the settings registry. No live ptr.
- Diff comparison needs `T` to be equality-comparable. For simple settings structs this is automatic via aggregate `=` defaulting; verify each settings struct supports it.

Touches: `gse.graphics:settings`. ~50 LOC delta.

### Phase 5 — Auto-install on add_system

- When the scheduler's `add_system<S, State>(reg)` runs and detects `S::settings`, auto-publish:
  - `save::register_settings_type{T_info, category}` (schema)
  - `settings::register_panel<T>{category, T_info}` (panel entry)
- Category source: `S::settings::category` static constexpr member.
- Drop manual `gse::settings::install(phase, "Cat", s.settings)` calls from every system's `initialize`.

Touches: `gse.ecs:scheduler`. ~30 LOC.

### Phase 6 — Per-system migration (mechanical)

For each system that has settings today:

- Move `physics::state::settings` → top-level `physics::settings { ... }`.
- Drop the `settings settings;` member from `state`.
- Add `using settings = physics::settings;` to `physics::system`.
- Add `static constexpr std::string_view category = "Physics";` to settings struct.
- Update phase function signatures: take `const physics::settings&` where settings is read.
- Sed `s.settings.x` → `cfg.x` (or whatever the param is named).
- Drop the `gse::settings::install(phase, "Physics", s.settings)` call from `initialize`.
- Cross-system reads: `ps->settings.use_gpu_solver` → `ctx.try_state_of<physics::settings>()->use_gpu_solver`.

Systems: Physics, Renderer, ForwardRenderer, PhysicsDebugRenderer, CaptureRenderer, Gui (system_state's `settings` sub-struct).

Estimated: ~30-50 LOC per system, ~5 systems, mostly mechanical.

### Phase 7 — Window/Context/Device redesign

- Extract `window_settings`, `graphics_settings`, `vulkan_settings` as real structs.
- Remove the `m_fullscreen`, `m_validation_layers_enabled`, etc. private members from those classes.
- Add a *system* that owns those settings types (probably `gpu::system` and `os::window_system`) — register them via `using settings = ...;`.
- The class itself takes `const settings&` in its constructor and stores a const ref (or copies on construction).
- Settings-driven side effects (apply fullscreen, set validation layers, etc.) move into the system's update phase, reading `settings::changed<window_settings>` events.
- `vulkan::instance::create()` early-read of validation layer setting still uses `save::read_one<bool>(path, "Graphics", "validation_layers_enabled", true)` — file-based, no runtime state needed at that point.

Touches: Window.cppm (significant), Context.cppm (significant), Device.cppm (significant), Engine.cpp (wiring).

Estimated: ~300 LOC delta total, the most involved migration.

### Phase 8 — Cleanup

- Drop dead code: old `register_struct(state&, ...)` path, `gse::settings::install` (or thin it to a one-liner that pushes the channels), the `m_val` was-protected comment in `quantity` once we confirm we're not regressing the structural property.
- Sweep for any leftover ref-based settings access.

## Open design points

Marked for resolution during implementation:

1. **Granularity of `change_request<T>`.** Closure-based (`std::function<void(T&)>`) vs field-pointer-based (`{ member_ptr, new_value }`). Closures are flexible and work for any field; field-pointers are cheaper and serializable. Lean closure for v1; revisit if it becomes a hotspot.

2. **`changed<T>` granularity.** Whole-struct old/new vs per-field events. Whole-struct is simpler, requires a copyable T (most settings already are). Per-field is more efficient for systems that only care about one field but adds complexity to the applier.

3. **Settings-applier phase ordering.** Has to run before all consumers each frame so they see a consistent snapshot. Probably first thing after channel pump, before any system's `update` phase.

4. **Initial load semantics.** When save loads from disk on `do_initialize`, it publishes `change_request<T>` for every persisted value. The settings-applier needs to run during the init phase too (or save needs to apply directly the first time). Probably: init-phase load goes direct (no channel round-trip), runtime mutations go through the channel.

5. **Restart-required propagation.** Should `changed<T>` carry per-field restart flags, or should the panel filter at draw time and the applier just always apply? Probably: applier always applies in-memory; UI shows "* restart required" badge by reading the `[[=restart_required]]` annotation on the field; save flushes to file as normal; the field just doesn't *take effect* in subsystems that hard-cache their initialization parameters until restart.

## Total estimate

~7-10 hours of focused work split across the 8 phases. Phases 1+2+3+5 are the engine work that has to land together (~4 hours). Phases 4+6 are mostly mechanical (~2 hours). Phase 7 is the longest single piece of work (~3 hours) but can land independently as long as Phases 1-5 are stable.

## Risks

- **Equality comparison on settings types** — required for diff detection in the panel. Most are POD-aggregate with default `==`, fine. `choice<T>` has a `vector<string>`, equality is well-defined but expensive; consider exposing a per-field diff in `draw_struct_thunk` instead of a whole-struct compare.
- **Channel ordering during init.** The first frame loads from disk and publishes change_requests; if the applier runs before save's `do_initialize` completes, the requests fire into a void. Need to ensure applier runs after save in init phase, or have save apply synchronously during init.
- **Cross-module dependencies.** Settings-applier needs to know all registered settings types at compile time? No — channels are type-erased at the registry level, registration is per-T at runtime. Should compose cleanly.
- **GUI slider widgets writing through `T&`.** The diff-and-emit pattern works but means the panel's local copy gets mutated, then we detect, then we revert (since the actual value is owned by the settings registry, not the local copy). Subtle but correct: each frame the local is re-seeded from the registry, so no permanent divergence. Need to verify no slider state (drag-in-progress) breaks across re-seeds.

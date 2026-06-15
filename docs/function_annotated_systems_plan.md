# Function-Annotated Systems Refactor Plan

## Status

Planning draft.

This document sketches a larger ECS/scheduler refactor where system behavior moves from static member functions on system structs to annotated free functions attached to explicit state types.

The main motivation is not syntax. The motivation is to make the scheduler, settings UI, save system, and debug tooling consume compact runtime descriptors instead of repeatedly instantiating large type-specific reflection paths in high-fan-in code.

## Current Shape

Today a system is usually a type like this:

```cpp
struct foo_system {
	struct [[= gse::settings::category<"Foo">{}]] data {
		[[= gse::settings::describe<"Enable foo.">{}]]
		bool enabled = true;
	};

	static auto run(
		gse::context& ctx,
		data& d
	) -> gse::async::task<>;
};
```

`make_system_node<S>` owns most of the bridge:

- It creates `system_node_data<S>`.
- It reflects `S::run`, `S::init`, `S::frame`, `S::shutdown`, or phased `S::run`.
- It extracts dependencies from function signatures.
- It builds settings records from `S::data`.
- It creates typed dispatch thunks for run/init/frame/shutdown/settings.

This is powerful, but it also means every registered system tends to instantiate a wide template surface. The settings GUI experiment showed that moving normal fields to runtime metadata helps, but custom settings pages still need a type-specific hook.

## Goals

- Keep state explicit as normal aggregate structs.
- Move behavior to annotated free functions.
- Keep function signatures as the source of scheduler dependencies.
- Keep adding a game system local to game code, with no engine-source table edits.
- Preserve custom settings pages.
- Make settings/UI/scheduler fan-in code consume runtime descriptors.
- Instantiate custom GUI thunks only for states that actually declare a custom settings page.
- Allow gradual migration: old static-system structs and new function-annotated systems can coexist.

## Non-Goals

- Do not replace component storage in this pass.
- Do not rewrite the channel system.
- Do not remove `context`, `access<T>`, `structural<T>`, `shared_view<T>`, or external-resource injection.
- Do not build one giant compile-time scene type that every importer pays for.
- Do not require game code to edit engine source when adding a system or custom settings page.

## Proposed User Model

State is a data type:

```cpp
struct [[= gse::system_state<"Crosshair">{}]] crosshair_state {
	[[= gse::settings::describe<"Show the crosshair while in-game.">{}, = gse::shared]]
	bool show = true;

	[[= gse::settings::describe<"Length of each arm in pixels.">{}, = gse::settings::range<0, 30>{}, = gse::shared]]
	int arm_length = 8;
};
```

Behavior is one or more free functions attached to that state:

```cpp
[[= gse::system_frame<^^crosshair_state>{}]]
auto draw_crosshair(
	crosshair_state& state,
	gse::context& ctx
) -> gse::async::task<>;
```

Custom settings pages are also free functions:

```cpp
[[= gse::settings::page_for<^^crosshair_state>{}]]
auto draw_crosshair_settings(
	gse::gui::builder& b,
	gse::settings::panel_state& ps,
	const crosshair_state& live,
	crosshair_state& pending,
	gse::channel_writer& channels
) -> void;
```

The state type becomes the stable identity. Functions attach behavior to the state through annotations.

## Runtime Descriptor Boundary

The refactor should introduce a small descriptor layer that high-fan-in systems can consume:

```cpp
struct system_state_descriptor {
	gse::id state_id;
	gse::id state_type_id;
	std::string name;
	void* state_ptr = nullptr;
	const void* snapshot_ptr = nullptr;
	gse::settings::register_settings_type settings;
};

struct system_phase_descriptor {
	gse::id phase_id;
	gse::id state_id;
	gse::scheduler_phase phase;
	auto (*invoke)(gse::context&, void*) -> gse::async::task<> = nullptr;
	std::vector<gse::id> required_state_deps;
	std::vector<gse::id> optional_state_deps;
	std::vector<gse::id> component_reads;
	std::vector<gse::id> component_writes;
};
```

Names are illustrative. The key boundary is that GUI/settings/scheduler code should mostly see descriptors and function pointers, not the original `State` and `Fn` template parameters.

## Discovery Options

### Option A: Explicit Game Manifest First

The lowest-risk first step is an explicit manifest owned by game code:

```cpp
using game_systems = gse::system_manifest<
	^^crosshair_state,
	^^draw_crosshair,
	^^draw_crosshair_settings
>;
```

This still avoids engine-source edits. It also avoids betting the first migration on compiler support for namespace-wide scans across modules.

### Option B: Namespace Scan Later

Once the descriptor path is stable, add a namespace scan that finds annotated state and function declarations under a chosen namespace.

This is ergonomically better, but it is also the part most likely to stress GCC reflection and module visibility. It should be a second step, not the foundation of the first prototype.

## Scheduler Semantics

Each annotated phase function should be classified using the same rules as current run signatures:

- `context&` resolves to the active run context.
- `state&` resolves to the attached state object.
- `access<T, read>` contributes a component read.
- `access<T, write>` contributes a component write.
- `structural<T>` contributes structural write authority.
- `shared_view<T>` contributes a required state dependency.
- `std::optional<shared_view<T>>` contributes an optional state dependency.
- `const T&` and `const T*` continue to represent external resources.

The scheduler can choose one of two granularities:

- State-node granularity: one scheduler node per state, with multiple phase descriptors invoked in a fixed local order.
- Function-node granularity: one scheduler node per annotated phase function, all sharing the same state object.

Function-node granularity is more expressive and may expose more parallelism. State-node granularity is closer to the existing engine. The first prototype should use state-node granularity unless there is a clear reason to split scheduling more aggressively.

## Settings And Custom Pages

Normal settings fields should keep using runtime metadata:

- Field key.
- Field description.
- Widget kind.
- Range/options metadata.
- Format thunk.
- Push-change thunk.

Custom pages need one optional typed thunk per state that declares `page_for<^^state>`:

```cpp
using draw_settings_page_thunk = void (*)(
	void* gui_builder,
	void* panel_state,
	void* channel_writer,
	const gse::settings::register_settings_type* entry
);
```

The thunk can rebuild a typed pending object:

```cpp
state pending = live;
apply_pending_fields(entry, panel_state, pending);
draw_custom_page(b, ps, live, pending, channels);
```

To support this, `settings_field` needs an additional type-erased operation:

```cpp
using apply_settings_field_to_object_thunk = bool (*)(
	void* object,
	std::string_view value
);
```

That keeps custom settings pages typed where they need to be typed, while the normal panel stays runtime-driven. Systems without custom pages pay no custom-page thunk cost.

## Migration Plan

### Phase 0: Freeze Baselines

- Keep the current static-system path working.
- Record `Engine.cpp` syntax/profile numbers for the current metadata experiment.
- Record clean profile build timing.
- Preserve the known custom-page case as a smoke test.

### Phase 1: Add Annotation Types And Descriptor IR

- Add `system_state`, `system_init`, `system_run`, `system_frame`, `system_shutdown` annotations.
- Add `settings::page_for`.
- Add descriptor structs for state and phase metadata.
- Add reflection helpers that turn `^^state` and `^^function` into descriptors.
- Do not wire scheduler behavior yet.

### Phase 2: Build A Compatibility Adapter

- Add an adapter that registers a state plus annotated functions into the existing `scheduler`.
- Use an explicit game-owned manifest for the prototype.
- Keep old `scheduler::add_system<S>` untouched.
- Verify one simple no-settings system can run through the new path.

### Phase 3: Move Settings To State Descriptors

- Extend `settings_field` with `apply_to_object`.
- Let `build_settings_record<State>` work independently of a system wrapper type.
- Add optional custom page thunk generation for `page_for<^^state>`.
- Convert the crosshair settings page first, because it proves custom pages still work.

### Phase 4: Convert A Small Cluster

Convert a few low-risk systems:

- One simple system with no settings.
- One system with generic settings only.
- One system with a custom settings page.
- One system with `shared_view` or optional state dependency.

Measure after each conversion. Do not convert large renderer or locomotion systems until the profile says the shape is winning.

### Phase 5: Broaden Or Stop

If the measurements improve:

- Convert more game systems.
- Convert engine systems that are high-churn or settings-heavy.
- Consider namespace-scan discovery.

If the measurements regress:

- Keep the runtime settings metadata work.
- Keep the custom-page thunk improvement.
- Drop or postpone the larger free-function migration.

## Compile-Time Expectations

This should help if:

- The settings panel imports descriptor APIs instead of instantiating `draw_fields<S>` for every system.
- Custom settings thunks are generated only for custom-page states.
- `make_system_node` stops being the single place that instantiates scheduler, settings, snapshot, and GUI behavior for every system.
- The game manifest or namespace scan is compiled in a low-fan-in implementation unit.

This will not help if:

- Every annotated function feeds one exported mega-template.
- The complete system list becomes a high-fan-in exported type.
- GUI modules import game modules to see custom pages directly.
- Namespace scanning forces GCC to deserialize most game declarations in common engine imports.

## Measurement Gates

After each phase, compare:

```text
cmake --build --preset x64-mingw-gcc-Profile --target GoonSquad
cmake --build --preset x64-mingw-gcc-Profile --target GoonSquad --clean-first
```

Also rerun the `Engine.cpp` syntax/profile probes:

- Full body.
- Baseline import only.
- `make_system_node` or equivalent descriptor build for selected systems.
- `build_settings_record` for selected states.

Success for the prototype:

- Custom settings page still renders.
- No engine-source edit is needed to add a game system with settings.
- `Engine.cpp` full-body probe is no worse than the runtime-metadata path.
- Settings-related template time trends toward the provider-table result without reintroducing a provider table.
- Clean profile build does not regress.

## Risks

- GCC reflection over free functions may have different costs than reflection over static members.
- Namespace scans across modules may be expensive or unreliable.
- Custom page thunks can accidentally pull GUI imports into places that should only know about descriptors.
- Function annotations can make ownership less obvious if the state/function naming convention is weak.
- Existing constructor-argument support for system data needs a replacement story.
- Function-node scheduling may change ordering if introduced too early.

## Open Questions

- Should the first implementation use explicit manifests permanently, or only until namespace scans are proven cheap?
- Should each annotated function become a scheduler node, or should a state remain the node?
- How should constructor arguments for state be expressed?
- Should settings category live on `system_state`, on the state struct, or remain `settings::category`?
- How should custom settings pages access pending values for nested structs once nested field metadata is supported?
- How should `init`, `frame`, and `shutdown` be ordered relative to multiple run phase functions attached to the same state?

## Recommended First Prototype

Prototype the smallest useful slice:

1. Add annotation types and descriptors.
2. Add explicit manifest support.
3. Convert one tiny no-settings system.
4. Convert the crosshair settings state and custom page.
5. Measure before touching renderer or locomotion systems.

The prototype is only worth keeping if it preserves the authoring ergonomics and moves compile time in the right direction.

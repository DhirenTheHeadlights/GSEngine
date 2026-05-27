# World-as-System + Data-Oriented Scene Composition

Two refactors that compose. Together they kill the last "engine has special non-system code" carve-outs in the per-tick path and convert scene authoring from a god-callback into pure data.

End state:

```cpp
// engine.cpp
auto gse::engine::update() -> void {
    system_clock::update();
    m_scheduler.update();   // one line. no drains, no world tick, no special-cased anything.
}

// floor_setup.cppm
auto floor_setup(scene& s) -> void {
    s.build("Floor")
        .with<physics::motion_component>({...})
        .with<physics::collision_component>({...})
        .with<primitive_box_spec>({.material = {.base_color = grey}, .size = floor_size});

    auto a = s.build("Anchor").with<...>(...).identify();
    auto b = s.build("Hanging").with<...>(...).identify();

    s.build("AB Joint")
        .with<joint_spec>({.entity_a = a, .entity_b = b, .config = fixed_joint{...}});
}
```

`setup_fn` is `void(*)(scene&)`. Forever. No channel writer, no asset state, no engine plumbing leaks into authoring.

## Why

Two coupled smells.

**Smell 1: scene setup is a god-callback.** Every time the engine adds a new dependency that authoring needs (channels, assets, audio, …), the `setup_fn` signature grows and ripples through every scene file. Today's signature is `void(*)(scene&, channel_writer&, asset::state&)`. Adding audio = another param, another cascade. The signature is a concrete record of authoring's ever-expanding coupling to engine internals.

**Smell 2: world is a special case in the engine update loop.** `engine::update` does `m_scheduler.update()` then `drain_lifecycle_channels()` then `m_world.update()`. World is not a system; it has its own update path, its own state, its own backdoor reads (`world::state_of<T>` template member, since killed). Server has to hold a `world*` pointer set up at bootstrap because it can't declare world state as a dep like a normal system would. The engine has a sibling special-case in `engine::render` (drains `gpu::render_pass_request`, calls `m_world.render()`, GPU begin/end frame) — that's a separate cleanup and not in scope here.

Both smells share a root: things that should be ECS state + system code aren't, so the engine grows special pathways to host them. World becoming a system removes the per-tick special-case for game state. Pure-data scene setup removes the synchronous engine-reach-in inside authoring callbacks.

## End-state inventory

After both refactors land:

| Today | After |
|---|---|
| `class world` (~150 LOC, owned by engine) | `world_system::state` + `run` body |
| `engine::drain_lifecycle_channels()` | drains live in `world_system::run` |
| `engine::world()`, `engine::add_scene`, `engine::direct`, `engine::triggers` | callers reach `world_system::state` directly via declared dep or post-`add_system` ref |
| `server_system::state::world_ptr` plumbing | declared dep `const world_system::state&` |
| `world::state_of<T>()` backdoor template | dead conceptually; replaced by declared deps |
| `setup_fn(scene&, channel_writer&, asset::state&)` | `setup_fn(scene&)` |
| `procedural_model::box/sphere` runtime mesh-gen + per-(size, material) cache | static `unit_box.gmodel`, `sphere_lo/mid/hi.gmodel`, transform handles size, per-instance tint handles color |
| `physics::joint_request` channel + `physics::join()` family | `joint_spec` component + drain in `physics::system::run` |
| `engine::update` as 3 sequential phases | one `m_scheduler.update()` call |
| Asset state cascade through entity_builders + 5 scene files + ~12 inner build helpers | gone; `gse.assets` import drops from `Scene.cppm` and all game scene files |

Scenes become serializable data — `[[= networked]]` tags on spec components let the server author scenes too.

## Two refactors, two tracks

Track A and Track B are orthogonal and can be sequenced either way. They compose well: Track A removes engine-side game-state plumbing; Track B removes authoring-side engine-state plumbing. Both end in fewer special pathways.

Track A — world-as-system — is mechanically smaller but touches engine bootstrap (registry ownership, system add order). Track B — pure-data scene composition — is mechanically larger but each phase is independent and can land as no-ops first.

Recommended order: Track B first (Phase 0–4 land as no-op infrastructure, Phase 5–8 cuts over authoring), then Track A (registry move + world_system + bootstrap reorder). Track A is easier on top of Track B because by then `setup_fn` is `void(scene&)` and there's no asset/channel plumbing for world to thread through scene activation.

---

## Track A — World as a system

### Shape

```cpp
// Engine/Engine/Source/Runtime/WorldSystem.cppm  (new)
struct world_system {
    struct state {
        std::unordered_map<id, std::unique_ptr<scene>> scenes;
        std::vector<trigger> triggers;
        std::optional<id> active_scene;
        bool networked = false;
        bool authoritative = true;
        std::optional<id> client_id;
        id local_controlled_entity{};
        id local_controller_id{};

        // PC bookkeeping (was world::m_pc_*)
        std::unordered_set<id> pc_processed;
        std::unordered_map<id, id> pc_controller_to_local_player;
        bool pc_local_player_created = false;
    };

    static auto run(
        run_context& ctx,
        state& s,
        const actions::system::state& as
    ) -> async::task<>;

    static auto shutdown(
        shutdown_context& phase,
        state& s
    ) -> void;
};
```

`run` does what `world::update` + `engine::drain_lifecycle_channels` did:
1. Drain `set_networked_request`, `set_authoritative_request`, `set_local_controller_id_request`, `activate_scene_request`, `deactivate_active_scene_request`.
2. Trigger evaluation in non-networked mode.
3. Player-controller spawn logic (folded in from `world_update_player_controllers`).

`shutdown` deactivates any active scene and clears the scene map (was `world::shutdown`).

### Registry ownership move

Today: `world m_world` owns `gse::registry m_registry`. `engine::initialize` calls `m_scheduler.set_registry(m_world.registry())`.

After: registry moves to engine.

```cpp
// Engine.cppm
class engine : public identifiable {
private:
    flags<engine_flag> m_flags;
    scheduler m_scheduler;
    registry m_registry;          // hoisted from world
    save::registry m_save;
};
```

`engine::initialize` sets `m_scheduler.set_registry(m_registry)` and constructs `world_system` such that scenes get the engine's registry passed to their ctor.

Scene constructor stays `scene(registry&, std::string_view)` — same signature, just gets the engine's registry instead of the world's.

### Bootstrap reorder

`world_system` needs to be added before `app_setup` runs, because `app_setup` populates `world_state.scenes` and `world_state.triggers` directly via the `state&` returned from `add_system`.

```cpp
auto gse::engine::initialize(const setup_fn& app_setup) -> void {
    trace::start({...});

    m_scheduler.set_registry(m_registry);
    m_save.set_auto_save(true, ...);
    m_scheduler.register_external_resource<save::registry>(&m_save);

    add_system<input::system>();
    add_system<actions::system>();
    add_system<network::system>();
    auto& world_state = add_system<world_system>(m_registry);  // early — app_setup populates

    if (m_flags.test(engine_flag::render)) {
        // ... rendering systems ...
    }
    else {
        // ... server-only path ...
    }

    m_scheduler.initialize();

    if (app_setup) {
        app_setup(*this);
        m_scheduler.initialize();
    }
}
```

`add_system<world_system>(m_registry)` passes the registry to the system's state constructor (or via a state initializer if `add_system` doesn't currently support ctor args — minor scheduler tweak).

### Director shape

`gse::director` becomes a thin wrapper around `world_system::state&`:

```cpp
class director {
public:
    explicit director(world_system::state& state) : m_state(state) {}

    auto when(const trigger& t) -> director& {
        m_state.triggers.push_back(t);
        return *this;
    }

private:
    world_system::state& m_state;
};
```

Bootstrap usage:

```cpp
auto gs::world_loader_setup(engine& e, world_system::state& world) -> void {
    auto channels = e.make_channel_writer();
    g_scene_keys.default_scene = actions::add<"Load Default Scene">(channels, key::f1);
    // ...

    director(world)
        .when({
            .scene_id = world.scenes.emplace(
                generate_id("Default Scene"),
                std::make_unique<scene>(e.registry(), "Default Scene")
            ).first->first,
            .condition = [](const evaluation_context& ctx) { ... },
        });
}
```

A slight ergonomic regression vs `e.add_scene("...")` returning a `scene*` — caller manages the id/lifecycle directly. Probably worth a small `world_system::add_scene(state&, name, setup_fn) -> id` helper that mints the id, constructs the scene, drops it in the map, returns the id. Same one-liner ergonomics, just through a free function instead of an engine method.

### Lifecycle drain timing

`world_system::run` declares `read<actions::system::state>` (already needed for trigger evaluation). It does not need additional ordering for the lifecycle drains — those are same-frame channel reads.

Renderer or anything that reads `world_system::state` (e.g., server) declares it as a const dep, scheduler orders correctly.

### Server-side cleanup

`server_system::state::world_ptr` deletes. `server::update` signature changes from `(world&, channel_writer&, const actions::system::state&)` to `(const world_system::state&, channel_writer&, const actions::system::state&)`. Server reads `world_state.scenes` and `world_state.triggers` directly. All entity manipulation continues via `ctx.registry()` (the engine's registry).

### What dies in Track A

- `Engine/Engine/Source/Runtime/World.cppm` — most of it. `class world` shell goes away (~150 LOC). `evaluation_context`, `trigger`, `director` stay (move to a `world_system_types.cppm` partition or stay in `WorldSystem.cppm`).
- `engine::drain_lifecycle_channels()` (~25 LOC).
- `engine::world()` accessor (~5 LOC).
- `engine::add_scene`, `engine::direct`, `engine::triggers` (~15 LOC, replaced by free helpers or direct state mutation).
- `world::state_of<T>` template member — already dead.

### What gets added in Track A

- `world_system` class (~80 LOC).
- `director(state&)` helper (~10 LOC).
- `world_system::add_scene(state&, ...)` helper (~8 LOC).
- Bootstrap reorder + registry hoist (~20 LOC delta).

**Net: ~−90 LOC. Conceptually big.** Engine update becomes one line; world participates in the system graph like everything else.

### Track A risks

- **`add_system<S>(args...)` ctor support**: confirm the scheduler can pass constructor args to system state. If not, populate state post-`add_system` via the returned `state&`.
- **PC spawn registry mutation under acquire**: `world_update_player_controllers` mutates the registry without locks today. Inside `world_system::run` it should `co_await ctx.acquire<write<player_controller>>()` etc. Adds correctness but small refactor.
- **`world::shutdown` migrating to system shutdown hook**: the existing `shutdown_context` flow handles this. `world_system::shutdown(phase, state)` deactivates and clears.
- **Scene activation timing under same-frame channel drain**: today, `engine::drain_lifecycle_channels` runs *between* `m_scheduler.update()` and `m_world.update()`. Inside `world_system::run`, drains happen at the top of `run`, then trigger eval, then PC spawn. Renderer runs after world_system because it declares `read<world_system::state>`. Same effective order as today.

---

## Track B — Pure-data scene composition

### Architecture

Setup is description; systems are realization. Scene authors write spec components. Resolver systems (with the engine state they need declared as deps) convert specs to render-ready state.

```cpp
// Setup
auto floor_setup(scene& s) -> void {
    s.build("Floor").with<primitive_box_spec>({.material = ..., .size = ...});
}

// Resolver (a regular system)
auto primitive_resolver::run(run_context& ctx, const primitives::state& prims, asset::state& assets) -> async::task<>;
```

The resolver attaches the unit-primitive handle to `render_component`, copies tint into per-instance color, marks the spec resolved.

### Component design

#### Spec components (graphics-side)

Module partition `gse.graphics:primitive_specs` (new):

```cpp
export namespace gse {
    struct material_spec {
        vec3f base_color = vec3f(1.0f);
        float roughness = 0.5f;
        float metallic = 0.0f;
        std::optional<std::string> diffuse_texture_name;
        std::optional<std::string> normal_texture_name;
        std::optional<std::string> specular_texture_name;
    };

    enum class sphere_lod : std::uint8_t { lo, mid, hi };

    struct primitive_box_spec : component_tag {
        [[= networked]] material_spec material;
        [[= networked]] vec3<length> size = vec3<length>{ meters(1.f) };
        bool resolved = false;

        id m_owner_id;
        auto owner_id() const -> id { return m_owner_id; }
    };

    struct primitive_sphere_spec : component_tag {
        [[= networked]] material_spec material;
        [[= networked]] sphere_lod lod = sphere_lod::mid;
        bool resolved = false;

        id m_owner_id;
        auto owner_id() const -> id { return m_owner_id; }
    };
}
```

`resolved = false` marks "needs work this tick"; resolver flips to `true` after attaching handle. User explicitly chose to leave specs in place after resolution (serialization, hot-reload, "what kind of primitive was this").

Hot reload: when a spec is mutated (e.g., from a hot-reloaded scene file), `mark_component_updated<primitive_*_spec>` resets `resolved = false`; resolver re-runs.

#### Joint spec component (physics-side)

Module partition `gse.physics:joint_spec` (new):

```cpp
export namespace gse::physics {
    struct joint_spec : component_tag {
        [[= networked]] id entity_a;
        [[= networked]] id entity_b;
        [[= networked]] joint_definition def;
        bool resolved = false;

        id m_owner_id;
        auto owner_id() const -> id { return m_owner_id; }
    };
}
```

`physics::system::run` iterates unresolved `joint_spec` components, calls `create_joint(state, def)`, marks resolved. The `joint_request` channel and `physics::join()` overloads delete.

### Renderer prerequisite — per-instance tint

Currently `render_queue_entry::color` is hardcoded to `vec3f(1.0f)` in `model_instance::sync_structure` ([Model.cppm:205](../Engine/Engine/Source/Graphics/3D/Vulkan/Model.cppm)). Wire it through.

```cpp
// RenderComponent.cppm — add:
[[= networked]] std::array<vec3f, max_models> tints{};   // default-init = zero; convert to white at use site, or default to vec3f(1.0f)

// Model.cppm — model_instance::sync_structure:
//   read render_component.tints[index_in_models_array] and stamp into entry.color
```

`model_instance` doesn't currently know its index in `render_component.models[]`. Either:
- Pass the index through `model_instance` ctor and store it.
- Have `geometry_collector` apply tint at `render_queue_entry` construction time, looking up render_component + model index from the entity.

Latter is cleaner (model_instance stays per-handle, geometry_collector handles per-entity overlay).

Confirm forward shader multiplies vertex/instance color into base_color sample. If not, add it. (Quick shader audit during Phase 0.)

For Valorant-style content (multi-mesh authored .gmodels), tinting is rarely needed — characters carry their own baked materials. The tint exists to recover the "100 random-color crates" use case from procedural caching. Full per-instance material override (textures, roughness) is out of scope; add it only when content requires it.

### Primitives: codegen + asset pipeline

ProceduralModels.cppm goes away. In its place:

1. **Codegen tool** (`Engine/Tools/PrimitiveGen/`): a one-shot CLI that uses the existing `procedural_model::box` / `sphere` mesh-build code (lifted into the tool, not the engine) to write `.gmodel` files. Writes:
   - `Resources/Models/Primitives/unit_box.gmodel` (size 1×1×1, uv=size mapping, white default material).
   - `Resources/Models/Primitives/sphere_lo.gmodel` (8×6).
   - `Resources/Models/Primitives/sphere_mid.gmodel` (24×16).
   - `Resources/Models/Primitives/sphere_hi.gmodel` (48×32).

2. **Asset compiler** ingests them on next compile. The `.gmdl` magic is `0x474D444C`, version 4 ([Model.cppm:65-70](../Engine/Engine/Source/Graphics/3D/Vulkan/Model.cppm)).

3. **`primitives::system`** (engine-side, added in `engine::initialize`):

```cpp
struct primitives {
    struct state {
        resource::handle<model> unit_box;
        resource::handle<model> sphere_lo;
        resource::handle<model> sphere_mid;
        resource::handle<model> sphere_hi;
        bool ready = false;
    };

    static auto run(run_context& ctx, state& s, asset::state& assets) -> async::task<> {
        if (!s.ready) {
            s.unit_box   = asset::get<model>(assets, "Primitives/unit_box");
            s.sphere_lo  = asset::get<model>(assets, "Primitives/sphere_lo");
            s.sphere_mid = asset::get<model>(assets, "Primitives/sphere_mid");
            s.sphere_hi  = asset::get<model>(assets, "Primitives/sphere_hi");
            s.ready = true;
        }
        while (true) co_await ctx.next_tick();
    }
};
```

Sync resolution at first tick; subsequent ticks no-op. The handles are valid for the rest of the engine's lifetime.

### `primitive_resolver::system`

```cpp
struct primitive_resolver {
    struct state {};   // marker, used for ordering deps

    static auto run(
        run_context& ctx,
        state& /*self*/,
        const primitives::state& prims,
        asset::state& assets
    ) -> async::task<> {
        while (true) {
            {
                auto [boxes, spheres, renders] = co_await ctx.acquire<
                    write<primitive_box_spec>,
                    write<primitive_sphere_spec>,
                    write<render_component>
                >();

                for (auto& spec : boxes) {
                    if (spec.resolved) continue;
                    materialize(spec, prims.unit_box, renders, ctx, assets);
                    spec.resolved = true;
                }
                for (auto& spec : spheres) {
                    if (spec.resolved) continue;
                    auto handle = (spec.lod == sphere_lod::lo) ? prims.sphere_lo
                                : (spec.lod == sphere_lod::hi) ? prims.sphere_hi
                                : prims.sphere_mid;
                    materialize(spec, handle, renders, ctx, assets);
                    spec.resolved = true;
                }
            }

            co_await ctx.next_tick();
        }
    }

private:
    template <typename Spec>
    static auto materialize(
        const Spec& spec,
        const resource::handle<model>& primitive,
        write<render_component>& renders,
        run_context& ctx,
        asset::state& assets
    ) -> void;
};
```

`materialize` ensures `render_component` exists on the entity (creates if missing), appends `primitive` to `render.models`, copies `material_spec.base_color` into `render.tints[idx]`, increments `model_count`, and (Phase 5) loads textures by name and stuffs them into per-instance material overrides if/when that exists.

Renderer / geometry_collector declares `read<primitive_resolver::state>` to force ordering: resolver runs first, renderer reads the populated `render_component`. No 1-frame popping.

### Texture-by-name in `material_spec`

Phase 5 work, defer-able. For Phase 1–4 with the existing test scenes, drop the textured-sphere case (replace `diffuse_texture` use in `build_sphere` with a flat color material — visual regression on one prototype scene). Adding full texture-by-name support requires:
- Per-instance material override in `render_component` (parallel `materials[max_models]` array, or a `material_override` component).
- Resolver loads textures via `asset::get<texture>(assets, *name)` and stuffs handles into the override.
- Renderer respects per-instance override over per-mesh-of-model material.

Probably ~150 LOC and a renderer change. Worth it when you actually need swappable textures per-entity (Valorant team colors etc.). Out of Phase 1–4 scope.

### Scene/world integration

After Track A:
- `setup_fn(scene&)` runs inside `world_system::run` when `activate_scene_request` is drained.
- `scene::set_active(bool)` no longer takes channel_writer or asset state — pure scene-internal mutation (entity creation, init callbacks).
- The scene's setup creates entities with spec components; resolvers (running in their own systems on subsequent ticks) realize them.

Without Track A: `world::set_active` callers stop passing channels/assets. `engine::drain_lifecycle_channels` still drives activation but doesn't need to fetch asset state.

---

## Phased plan

11 phases. Phase 0 is a renderer prereq. Phases 1–5 add Track B infrastructure as no-ops. Phase 6 is the Track B cutover. Phases 7–9 are Track A. Phase 10 is cleanup.

Each phase is independently shippable and testable.

### Phase 0 — Renderer per-instance tint

- Add `std::array<vec3f, max_models> tints{}` to `render_component`. Default to `vec3f(1.0f)` per element.
- Wire `tints[i]` into `geometry_collector` so each `render_queue_entry.color` carries the per-instance tint.
- Audit forward shader: confirm it multiplies vertex/instance color into the per-pixel base_color sample. If not, add the multiply.

**Files**: `RenderComponent.cppm`, `GeometryCollector.cpp`, possibly `Forward.slang`.
**LOC**: ~40.
**Risk**: shader regressions. Run test scenes, compare visual.

### Phase 1 — Generate primitive .gmodel files

- New tool `Engine/Tools/PrimitiveGen/Main.cpp` lifts the geometry math from `ProceduralModels.cppm` and emits `unit_box.gmodel`, `sphere_lo.gmodel`, `sphere_mid.gmodel`, `sphere_hi.gmodel` at `Resources/Models/Primitives/`.
- Run once. Files commit to repo (small, deterministic, regenerable).
- Asset compiler picks them up automatically; verify they load via `asset::get<model>(state, "Primitives/unit_box")` in a smoke test.

**Files**: new `Engine/Tools/PrimitiveGen/`, new `Resources/Models/Primitives/*.gmodel`.
**LOC**: ~120 (mostly lifted).
**Risk**: gmodel format mismatch — verify magic + version match Model.cppm.

### Phase 2 — `primitives::system`

- New module partition `gse.graphics:primitives` (or `gse.runtime:primitives` if you prefer engine-runtime ownership).
- `engine::initialize` adds it (only when `engine_flag::render` is set; server doesn't need primitives).

**Files**: new `Engine/Engine/Source/Graphics/3D/Primitives.cppm`, `Engine.cpp`.
**LOC**: ~50.
**Risk**: minimal. No callers yet.

### Phase 3 — Spec components

- New module partition `gse.graphics:primitive_specs`.
- `primitive_box_spec`, `primitive_sphere_spec`, `material_spec`, `sphere_lod`. All `[[= networked]]` for replication.
- New module partition `gse.physics:joint_spec`.
- `joint_spec` with `[[= networked]]` fields for entity_a, entity_b, joint_definition.

**Files**: new `Engine/Engine/Source/Graphics/3D/PrimitiveSpecs.cppm`, new `Engine/Engine/Source/Physics/JointSpec.cppm`.
**LOC**: ~70.
**Risk**: zero. Empty additions.

### Phase 4 — `primitive_resolver::system`

- Reads `primitives::state` + `asset::state` (write — texture loads). Writes `primitive_box_spec`, `primitive_sphere_spec`, `render_component`.
- Materialization function shared between box and sphere paths.
- Renderer (geometry_collector) declares `read<primitive_resolver::state>` to enforce ordering.

**Files**: new `Engine/Engine/Source/Graphics/3D/PrimitiveResolver.cppm`, `Engine.cpp` (add_system), `GeometryCollector.cppm` (add dep).
**LOC**: ~120.
**Risk**: ordering/timing. If resolver runs after renderer in tick N, you see 1-frame popping for newly-activated scenes. Verify the dep enforces ordering.

### Phase 5 — Joint resolver in `physics::system`

- `physics::system::run` adds an iteration over `joint_spec` components after its existing channel drains.
- Marks resolved via `joint_spec::resolved = true`.
- `physics::joint_request` channel + `physics::join()` overloads stay during Phase 5 (still callable from old code).

**Files**: `Physics/System.cppm`, `Physics/System.cpp`.
**LOC**: ~25.
**Risk**: minimal. Old path still works.

### Phase 6 — Cutover

The big one. Migrates entity_builders + all 5 game scenes + joint callers.

- `entity_builders.cppm`:
  - `build_box` / `build_sphere` / `build_static_box` lose `asset::state&` param. Body emits `primitive_box_spec` / `primitive_sphere_spec` instead of `render_component`.
  - `build_sphere_light` composes `build_sphere` + `point_light_component`.
  - Texture sphere case: simplest is `build_sphere` no longer accepts a texture name; the existing sun-textured sphere becomes flat-colored. Defer full texture support to a follow-up if needed.

- All 5 scene files (`MainTestScene.cppm`, `SecondTestScene.cppm`, `SkyboxScene.cppm`, `PhysicsStressTestScene.cppm`, `PhysicsJointTestScene.cppm`) and inner build helpers:
  - `setup_fn` signature → `void(scene&)`.
  - Drop `channel_writer&` and `asset::state&` params + all forwards.
  - `physics::join(channels, a, b, cfg)` → `s.build("Joint").with<joint_spec>({a, b, cfg})`.

- `arena::create` similar — drop `asset::state&`.

- `Scene.cppm`: `setup_fn = void(*)(scene&)`. `set_active(bool)` no extra params. Drop `import gse.assets` from Scene.cppm.

- `World.cppm` (or `world_system` if Track A landed first): set_active call sites stop fetching channel/asset state.

- `engine::drain_lifecycle_channels` (still alive pre-Track A) stops needing to fetch asset state for activation.

**Files**: `Scene.cppm`, `World.cppm`, all 5 game scene files, `EntityBuilders.cppm`, `Arena.cppm`, `FreeCamera.cppm`, `Engine.cpp`.
**LOC**: ~−250 churn (mostly deletions of param plumbing).
**Risk**: medium. Many touch points; mechanical but invites typos. Each scene file should compile independently.

### Phase 7 — Delete `ProceduralModels.cppm`

- Confirm no callers remain (`gse::procedural_model::box`, `sphere` should be 0 hits in `Game/` and `Engine/Engine/Source/`).
- Delete the file.
- Remove `:procedural_models` partition from `gse.examples`.
- The codegen tool (Phase 1) keeps a private copy of the geometry math; engine no longer carries it.

**Files**: delete `ProceduralModels.cppm`, edit `Engine/Engine/Import/Examples.cppm`.
**LOC**: ~−180.
**Risk**: zero if Phase 6 verified.

### Phase 8 — Hoist registry to engine; introduce `world_system::state`

Track A starts here.

- Move `gse::registry m_registry` from `world` to `engine`.
- Scene ctor unchanged (still `(registry&, name)`); engine passes its own registry now.
- Introduce `world_system` module + state struct. Don't migrate logic yet — just the struct, registered via `add_system<world_system>()`.
- `engine::initialize` calls `add_system<world_system>(...)` early.

**Files**: `Engine.cppm`, `Engine.cpp`, new `WorldSystem.cppm`, `World.cppm` (registry removal).
**LOC**: ~30.
**Risk**: bootstrap order. Confirm registry is wired before any system that uses it; `m_scheduler.set_registry(m_registry)` must happen before `add_system<world_system>` (which probably reads it).

### Phase 9 — Migrate world logic into `world_system::run`

- Move `world::update` body into `world_system::run`. Drains lifecycle channels at top, runs trigger eval, calls `world_update_player_controllers` (folded as a private function).
- Move `world::shutdown` into `world_system::shutdown(phase, state)`.
- `engine::drain_lifecycle_channels` deletes.
- `m_world` member deletes from engine. `engine::update` simplifies to one line.
- `engine::add_scene` / `direct` / `triggers` / `world()` accessors delete. Bootstrap (`world_loader_setup`) takes `world_system::state&` directly.
- Server: `server_system::state::world_ptr` deletes; declared dep on `const world_system::state&` replaces it.

**Files**: `WorldSystem.cppm` (logic), `Engine.cpp` (drain removal), `WorldLoader.cppm` (signature), `Server/Server/Include/Application.cppm` (state field), `Server/Server/Include/Server.cppm` (signature).
**LOC**: ~−120 net (engine + world plumbing dies; world_system body grows).
**Risk**: medium. Many cross-cutting changes; bootstrap reorder, server-side dep change. Each landing should keep the test scenes runnable.

### Phase 10 — Cleanup

- Drop `import gse.assets` from `Scene.cppm` (no longer needed after Phase 6).
- Revert the `gse.assets` umbrella export in `Engine/Engine/Import/Engine.cppm` if no Game files reference asset state directly.
- Verification grep sweep:
  - `m_world` → 0 hits in engine.
  - `world::set_active` / `world::activate` / `world::deactivate` → 0 hits.
  - `setup_fn` taking >1 arg → 0 hits.
  - `procedural_model` → 0 hits.
  - `joint_request` channel → 0 hits.
  - `physics::join(` → 0 hits in game/engine.

---

## Risks & open design questions

### `add_system<S>(args...)` constructor support

Phase 8 needs `add_system<world_system>(m_registry)` to forward `m_registry` to the system's state ctor. Today's scheduler `add_system<S>(Args&&...)` calls `make_system_node<S>(args...)`. Need to confirm those args reach `state` construction. If not, populate state post-add via the returned `state&`:

```cpp
auto& world_state = e.add_system<world_system>();
world_state.registry_ref = &e.registry();
```

### Resolver ordering enforcement

Renderer must run after `primitive_resolver` within the same tick to avoid popping. State-dep declaration on `primitive_resolver::state` (an empty marker) forces it. Verify in Phase 4 with a 100-entity scene activation; confirm no popping.

### Spec replication semantics

`primitive_box_spec` is `[[= networked]]`. On clients, a replicated entity with this spec runs through the same resolver pipeline locally. Two consequences:
- Client resolver also needs `primitives::state` populated. Ensure clients run primitives::system too.
- The `resolved` flag is part of state — should it replicate? Probably not (each side resolves independently). Mark non-networked.

### Renderer texture override scope

Phase 5+ for full per-instance material override (textures) requires touching the renderer. If you decide to drop the textured-sphere test case entirely, you can defer this indefinitely. Track B is complete-as-described without it.

### Hot-reload behavior

When a scene file hot-reloads, the new setup runs against a fresh scene activation. Existing entities are destroyed first (via `set_active(false)`). New entities created with new specs. Resolver picks them up. Same flow as cold start. Confirm no state leaks.

### Server doesn't run resolvers

Server-side, `engine_flag::render` is off. `primitives::system` and `primitive_resolver` are not added (or are added but no-op because no GPU). Server scenes have entities with spec components but no `render_component`. Physics/network/world state all work — physics doesn't read render data.

Confirm physics replication doesn't try to sync `render_component` across the wire if it's empty on server. (Probably handled by the existing replication system; spec components ARE replicated which is what matters for clients.)

### Joint timing

Today `physics::join(channels, ...)` pushes to a same-frame channel; physics drains it the same tick the joint is requested, then create_joint runs that tick. With `joint_spec` component, the spec lives until physics::system::run iterates it (next tick). One-tick delay for joint creation. Probably fine — joints are scene-init, not per-frame. Confirm joint setup doesn't break in test scenes.

### Multi-model entities

`render_component.models[max_models = 16]` and `model_count` already support multi-model. For complex authored content (Valorant character with multiple meshes), one .gmodel handle covers it (model class has a `std::vector<mesh>` internally). For "compose two unit primitives on one entity," spec components are unique per type so a single entity can have one `primitive_box_spec` AND one `primitive_sphere_spec`; resolver appends to `models` for each. Edge case but supported.

For "two unit boxes on one entity" — not directly supported via specs (component instances are unique per type). Spawn child entities or accept it.

### Bootstrap order chicken-and-egg

If `primitive_resolver` declares `read<primitives::state>` and `primitives::state` requires `asset::state` to be populated, the dep chain is:

```
asset::registry::run → primitives::run → primitive_resolver::run → renderer
```

Verify this resolves cleanly during scheduler initialize. `primitives::run` populates handles on first tick; subsequent ticks see them ready.

---

## Validation

### Phase 0
- All test scenes render identically to before. Run StressTest scene and verify random box colors look unchanged.

### Phases 1–4 (no-op infra)
- Each phase compiles standalone. Test scenes still use procedural_model path, render unchanged.

### Phase 5 (joint resolver)
- Add a `joint_spec`-using test (e.g., one new joint in StressTest scene as `joint_spec`). Verify it works alongside existing channel-based joints.

### Phase 6 (cutover)
- All 5 test scenes load and render. Box colors look right (per-instance tint working). Joint test scene's joints all create. Sphere LODs visible (mid by default).
- Server-side: spawn the server with a default scene, verify trigger transitions work, no asset/render-related errors in logs.

### Phase 7 (delete procedural)
- Build clean. Test scenes still work.

### Phases 8–9 (Track A)
- Engine update is one line. Server still works (declared dep on world_system::state). Scene transitions work.

### Phase 10 (cleanup)
- Verification greps return zero. `setup_fn(scene&)` is the only signature.

---

## Total scope

- **LOC delta**: roughly +400 (specs, resolver, primitives system, codegen tool, world_system, renderer tint), −600 (procedural models, asset/channel cascade through builders/scenes, world class, engine drain, joint_request channel + physics::join, world::state_of). Net **~−200 LOC**.
- **Conceptual delta**: setup_fn is `void(scene&)` forever. `engine::update` is one line. Engine's only special non-system per-tick code is `engine::render` (out of scope here; future cleanup).
- **Time estimate**: ~12–16 hours of focused work spread across the 11 phases. Phase 6 is the biggest single chunk (~3 hours). Phases 8+9 together are ~4 hours.

---

## Out of scope (future cleanup)

- **`engine::render` as a system**: the GPU frame begin/drain/end loop is the second sibling of the world special-case. Same shape, bigger refactor (touches scheduler render-phase ordering). Worth doing eventually; not in this doc.
- **Full per-instance material override**: textures, roughness, metallic per-entity. Solves the Valorant team-color case. Add when content needs it.
- **Save registry as a system**: `m_save` is currently an external resource on the engine. Could become a system. Minor; defer.
- **Scene serialization**: spec components being `[[= networked]]` makes scenes transmittable; full disk serialization (yaml/binary) is its own project but the data shape is right.

# ID System Reflection Reform

Scope of work to replace runtime ID/type-identity machinery with compile-time reflection where the inputs are statically known. Companion to `reflection_opportunities.md`; this doc focuses specifically on the ID/typeid/type_index axis.

## The Core Insight

Two reflection primitives unlock everything below. They live in a new `gse.core:id_reflect` partition:

```cpp
template <typename T>
consteval auto id_of() -> id;                                 // hash of display_string_of(^^T)

template <fixed_string Tag>
consteval auto id_of() -> id;                                 // hash of Tag

template <typename T>
consteval auto type_tag() -> std::string_view;                // display_string_of(^^T) → static_string
```

`id_of<T>()` and `id_of<"literal">()` produce the same `uuid` the runtime registry would, but at compile time, with no mutex, no map lookup, no FNV-1a loop, no typeid call. They use the existing FNV-1a constants from `generate_id` so wire compatibility with the runtime registry is exact.

The registry stays — for *user-string* IDs (asset paths, scene names, runtime user input). Per-type and string-literal IDs stop touching it.

---

## Priority Summary

| Rank | Target | Sites | Hot-path impact | Notes |
|------|--------|------:|------------------|-------|
| **S** | [`find_or_generate_id("literal")` → `id_of<"literal">()`](#s1-string-literal-trace-ids) | ~80 | Per-frame mutex+map kills | One-line per call site |
| **S** | [`std::type_index` map keys → `id_of<T>()`](#s2-type_index-keyed-maps) | 7 systems | Per-frame typeid+map → cache hit | Removes `<typeindex>` dep in many TUs |
| **A** | [`typeid(T).name()` string surgery](#a1-typeid-name-mangling) | 3 sites | Compile-time tag baking | Replaces MSVC "struct " parsing |
| **A** | [`pass_tag` lambda in RenderGraph](#a2-pass_tag-lambda) | 1 site, 12-line lambda | Per-pass formatting | Becomes `consteval pass_tag<T>()` |
| **A** | [Virtual `state_type()` / `resources_type()`](#a3-virtual-type-identity-recovery) | system_node | Vtable call → constant | Erase to id at registration |
| **B** | [SaveSystem `typeid(bool/int/float)` dispatch](#b1-savesystem-primitive-type-dispatch) | 4 cases | Switch on type-tag | Tied to C2 SaveSystem rewrite |
| **B** | [TaskContext `find_or_generate_id(format("after<{}>", typeid(State).name()))`](#b2-taskcontext-after-tag) | 1 site | Frame-hot tag rebuild | Becomes `id_of<"after", State>()` |
| **B** | [UpdateContext mutex-key + label format](#b3-updatecontext-access-labels) | 2 sites | Per-access typeid+format | Same primitive |
| **C** | [`asset_compiler<T>` registry → annotation scan](#c1-asset_compiler-registry) | asset pipeline | Startup cost | Crosses into reshapes |

**Running total:** 100+ runtime ID derivations per frame eliminated. Several `unordered_map<type_index, X>` lookups per system tick collapse to constant-time direct indexing or compile-time-keyed hash.

---

## Shared Primitives

New partition `gse.core:id_reflect`, exported by `gse.core`:

```cpp
export namespace gse {
    template <std::size_t N>
    struct fixed_string {
        char data[N]{};
        consteval fixed_string(const char (&s)[N]) { std::ranges::copy(s, data); }
        consteval operator std::string_view() const { return { data, N - 1 }; }
    };

    consteval auto stable_id(std::string_view tag) -> uuid {
        uuid h = 0xcbf29ce484222325ull;
        for (unsigned char c : tag) {
            h ^= c;
            h *= 1099511628211ull;
        }
        return h;
    }

    template <typename T>
    consteval auto id_of() -> id {
        return generate_temp_id(stable_id(std::meta::display_string_of(^^T)));
    }

    template <fixed_string Tag>
    consteval auto id_of() -> id {
        return generate_temp_id(stable_id(Tag));
    }

    template <typename T>
    consteval auto type_tag() -> std::string_view {
        return std::meta::display_string_of(^^T);
    }
}
```

The `stable_id` body is character-identical to the runtime `generate_id`'s loop, so a `consteval id_of<"foo">()` and a runtime `find_or_generate_id("foo")` return the same `uuid`. They can interoperate during the migration — one site at a time can flip without breaking the others.

**The registry's role shrinks but does not vanish.** For runtime tags (asset paths, scene IDs, user input) it stays as-is. The compile-time path skips the registry entirely; if you ever need the *string* back from a compile-time-derived ID, register it once at startup or use `type_tag<T>()` directly.

---

## Rank S

### S1. String-literal trace IDs

**Files:** `Scheduler.cppm` (~30 sites), `Runtime/Engine.cppm` (~15), `GeometryCollector.cppm` (~10), `Diag/Trace.cppm` (~10), `AsyncTask.cppm` (~5), `TaskGraph.cppm` (~3), plus scattered renderer/game sites. **~80 total.**

**Pattern:**

```cpp
trace::scope(find_or_generate_id("scheduler::update"), [&] { ... });
```

`find_or_generate_id` takes the read lock, hits the `tag_to_uuid` hash map, and on cold cache returns to the caller; on cache miss takes the write lock, runs FNV-1a over the string, inserts. Every single one of these calls fires every frame.

**Replacement:**

```cpp
trace::scope(id_of<"scheduler::update">(), [&] { ... });
```

`id_of<...>()` is a `consteval` constant — compiles to an immediate `uuid` literal. Zero runtime cost.

**Risk:** trivial. The on-disk format and trace UI take a `uuid`; both the old and new path produce the same value (FNV-1a of the same string).

**Migration:** mechanical, can be a sed/grep pass per file. Recommend doing it in batches by directory so reviewer can verify each batch.

### S2. `type_index`-keyed maps

**Files:**
- `Concurrency/RwMutex.cppm:79-83,179` — `unordered_map<type_index, unique_ptr<rw_mutex>>` keyed by `typeid(element_t)`
- `Concurrency/TaskGraph.cppm:76,179,197,212` — `flat_map<type_index, unique_ptr<state_slot>>` for ECS state-ready signaling
- `Concurrency/FrameScheduler.cppm:21,91` — `unordered_map<type_index, int>` for state ordinal lookup
- `Ecs/Scheduler.cppm:506,517,522` — channel ensure-by-type
- `Ecs/PhaseContext.cppm:40,80,94,160-245` — snapshot/channel/resources lookup, ~6 sites
- `Gpu/GpuContext.cppm:234,237,291,428,433,458` — resource loader registry + pending GPU resources
- `Gpu/Device/RenderGraph.cppm:356,361,1404` — per-pass type tracking
- `Assets/AssetPipeline.cppm:90,243` — type-routed bake dispatch
- `Save/SaveSystem.cppm:59,100,208,552,643,1091-1153` — property type tracking (mostly subsumed by C2 in `reflection_opportunities.md`)

**Pattern:**

```cpp
auto& mutex = registry.mutex_for(std::type_index(typeid(element_t)));        // UpdateContext.cppm:130
m_states[type_index] = std::make_unique<state_slot>(...);                    // TaskGraph.cppm
auto* loader = m_resource_loaders[type_index].get();                         // GpuContext.cppm
```

Every call constructs a `type_index` (cheap but non-zero), hashes it (typeid name mangling string), then looks up. The map is keyed on a runtime object that exists only because the lookup path discarded the static `T` along the way.

**Replacement:** key on `uuid` instead of `type_index`:

```cpp
flat_map<uuid, std::unique_ptr<state_slot>> m_states;

auto& mutex = registry.mutex_for(id_of<element_t>());
m_states[id_of<State>()] = std::make_unique<state_slot>(...);
```

`id_of<T>()` is a compile-time constant; no runtime hash of a mangled name, just a direct map probe with a precomputed key.

**Bonus:** with all keys converted, the `<typeindex>` include disappears from many TUs. `typeid` itself stays available for emergency runtime polymorphism cases (none currently used).

**Risk:** low. Wire up `id_of<T>()` first, then site-by-site swap the map's key type and the lookup expression. No on-disk format involved. Stable across DLL boundaries (typeid identity is famously not).

**Migration:** per-system, in this order:
1. `RwMutex` — smallest, isolated.
2. `FrameScheduler` — slightly bigger, similar shape.
3. `TaskGraph`, `Scheduler`, `PhaseContext` — share the state-type pattern.
4. `GpuContext`, `RenderGraph` — bigger touch, leans into A2.
5. `AssetPipeline` — ties into C1.

---

## Rank A

### A1. `typeid(T).name()` mangling

**Files:**
- `Ecs/SystemNode.cppm:503-515` — strips "struct "/"class "/"gse::" prefixes to make a system tag
- `Ecs/SystemNode.cppm:551-563` — same surgery for `frame_wall:` tag
- `Ecs/UpdateContext.cppm:154` — `std::format("{}<{}>", tag, typeid(...).name())`
- `Ecs/SystemNode.cppm:537,596` — `typeid(S).name()` in assert messages
- `Ecs/TaskContext.cppm:76` — `format("after<{}>", typeid(State).name())` (also S2/B2 above)

**Pattern:**

```cpp
std::string_view raw = typeid(S).name();
for (const std::string_view prefix : { "struct ", "class " }) {
    if (raw.starts_with(prefix)) {
        raw.remove_prefix(prefix.size());
    }
}
if (constexpr std::string_view ns = "gse::"; raw.starts_with(ns)) {
    raw.remove_prefix(ns.size());
}
return find_or_generate_id(raw);
```

This exists because `typeid::name()` returns implementation-defined mangled output — MSVC injects "struct "/"class " prefixes, no namespace stripping. Cross-compiler builds need to massage it.

**Replacement:**

```cpp
return id_of<S>();                                              // tag = display_string_of(^^S)
```

`std::meta::display_string_of(^^S)` returns canonical "namespace::type" form on every compiler. No prefix stripping needed.

If a stripped tag ("S" instead of "gse::S") is desired for display, build it from `identifier_of(^^S)` plus selective `parent_of` walks — also compile-time.

**Risk:** low. Behavior change for the *displayable string* — assertion messages and trace UI labels will now read "gse::physics::system_state" instead of "system_state". Acceptable upgrade (more informative, deterministic across compilers).

### A2. `pass_tag` lambda

**File:** `Gpu/Device/RenderGraph.cppm:1203` (12 lines).

The exact pattern from this conversation. Strips "struct "/"class "/"gse::renderer::"/"::state", splits on `[` for module suffix, formats as `gpu:name[suffix]`.

**Replacement:** template helper:

```cpp
template <typename PassType>
consteval auto pass_tag() -> std::string_view {
    constexpr auto state_tag = std::meta::display_string_of(^^PassType);
    constexpr auto parent_tag = std::meta::identifier_of(std::meta::parent_of(^^PassType));
    return /* compile-time concat: "gpu:" + parent_tag */;
}
```

Then the lambda's call sites change from `pass_tag(ti)` to `pass_tag<PassType>()`. The `std::type_index` parameter goes away; the `register_pass<PassType>` template already has the type at the call site.

**Risk:** trivial. Behavior change limited to display-only strings.

**Bonus:** the `[Module]` suffix is MSVC's mangling artifact for module-attached entities; `display_string_of` doesn't include it on clang-p2996. Cleaner cross-compiler labels.

### A3. Virtual `state_type()` / `resources_type()` recovery

**File:** `Ecs/SystemNode.cppm:62-65,177-180,628-637`

**Pattern:** `system_node_base` declares two pure virtuals:

```cpp
virtual auto state_type() const -> std::type_index = 0;
virtual auto resources_type() const -> std::type_index = 0;
```

The derived `system_node<S, State>` overrides each with `return typeid(State);`. They exist solely so the scheduler can look up which state slot a given system needs at runtime — the scheduler holds `vector<unique_ptr<system_node_base>>` and erases `S, State` at registration.

**Replacement:** capture the IDs at registration time, not at lookup time.

```cpp
struct system_node_base {
    uuid state_id;
    uuid resources_id;
    virtual auto run(/* ctx */) -> void = 0;
};

template <typename S, typename State>
struct system_node : system_node_base {
    system_node()
        : system_node_base{
            .state_id = id_of<State>().number(),
            .resources_id = /* if has_resources<S> */ id_of<typename S::resources>().number()
                                                  /* else */ uuid{},
        } {}
};
```

Two virtuals deleted. Scheduler reads the IDs as fields, no vtable hop on the hot path.

**Risk:** low. Field-init pattern, no behavior change.

---

## Rank B

### B1. SaveSystem primitive-type dispatch

**File:** `Save/SaveSystem.cppm:1091-1116`

**Pattern:**

```cpp
if (reg.type == typeid(bool)) {
    /* bool branch */
}
else if (reg.type == typeid(float)) {
    /* float branch */
}
else if (reg.type == typeid(int)) {
    /* int branch */
}
```

Triggered per setting on TOML load/UI render. Each branch test is a `type_index` equality (string compare on typeid name).

**Replacement:** subsumed by reflection_opportunities.md C2 (SaveSystem rewrite). Until that lands, the immediate fix is the same `id_of<bool>()` / `id_of<float>()` swap as S2 — `reg.type` becomes a `uuid`, comparisons are integer equality.

**Risk:** low standalone, but planned to be deleted entirely by C2 — only worth doing now if C2 is far off.

### B2. TaskContext `after<T>` tag

**File:** `Ecs/TaskContext.cppm:76`

```cpp
static const id tid = find_or_generate_id(std::format("after<{}>", typeid(State).name()));
```

Combines two anti-patterns: `typeid::name()` for the type tag + `find_or_generate_id` for the registry lookup. Static-init helps amortize but the `format` still runs once per `State` and the registry mutex is held during init.

**Replacement:** compose at compile time.

```cpp
constexpr auto tag = std::format(
    "after<{}>",
    std::meta::display_string_of(^^State)
);
constexpr id tid = id_of<State, "after">();   // overload or composer
```

**Risk:** trivial.

### B3. UpdateContext access labels

**File:** `Ecs/UpdateContext.cppm:154,170,177-178`

**Pattern:**

```cpp
return std::format("{}<{}>", tag, typeid(access_element_t<Access>).name());
// ...
return find_or_generate_id(label);
// ...
const std::array<std::type_index, count> type_indices = {
    std::type_index(typeid(access_element_t<Accesses>))...
};
```

Same shape as B2 — runtime format of a static template parameter list, then runtime hash. All the inputs are statically known.

**Replacement:** `id_of<access_element_t<Access>...>()` variadic composer; `type_indices` becomes `std::array<uuid, count>{ id_of<...>().number()... }` — values known at compile time.

**Risk:** low.

---

## Rank C

### C1. `asset_compiler<T>` registry

**File:** `Assets/AssetPipeline.cppm`

`asset_compiler<T>` is keyed by type via `std::type_index`. Each asset bake hits the map by typeid. Crosses into the broader Reshape territory (annotation-scanned compile-time registry of compilers); doable as a polish pass after the core ID work lands.

---

## Rollout Order

Maximizes payoff-per-day, minimizes rework:

1. **Bootstrap `gse.core:id_reflect`** — `fixed_string`, `stable_id`, `id_of<T>()`, `id_of<"literal">()`, `type_tag<T>()`. No behavior change.
2. **S1 (string-literal trace IDs)** — mechanical, ~80 sites, immediately measurable in profiler. Validates `id_of<"literal">()`.
3. **A1 (`typeid::name` surgery in SystemNode)** — 3 small sites, deletes ~25 lines of platform-specific string parsing.
4. **A2 (RenderGraph `pass_tag`)** — single-site, deletes the 12-line lambda the user originally flagged. Confirms the `display_string_of` + `parent_of` recipe.
5. **S2 batch 1 (RwMutex, FrameScheduler)** — smallest type-index→uuid migrations, prove the pattern.
6. **S2 batch 2 (TaskGraph, Scheduler, PhaseContext)** — medium, share state-type shape.
7. **A3 (system_node virtuals)** — ties into S2 batch 2.
8. **B2, B3 (TaskContext + UpdateContext format/tag)** — small, mop-up.
9. **S2 batch 3 (GpuContext, RenderGraph)** — biggest touch, leans on prior validation.
10. **B1 (SaveSystem typeid dispatch)** — only worth doing standalone if C2 (full SaveSystem rewrite) is deferred.
11. **C1 (asset_compiler registry)** — schedule as polish; overlaps with Part II reshapes.

Skip the registry-side cleanup until all the consumers are migrated. The runtime registry stays for user-string IDs. Removing the type-keyed plumbing from the registry itself is the last step.

---

## Risks and Caveats

- **Wire compatibility.** `stable_id(string_view)` must match `generate_id`'s FNV-1a constants byte-for-byte so the two paths interop during the migration. The doc above uses the same `0xcbf29ce484222325` / `1099511628211` constants — verified.
- **Display strings change.** Anywhere logs/traces/assertions surface the *old* tag string ("system_state" vs "gse::physics::system_state"), the new path produces the canonical reflected name. Net upgrade but worth acknowledging in the trace UI's first frame after rollout.
- **Cross-compiler `display_string_of` shape.** P2996's `display_string_of` is canonical *per implementation*. Locking on this clang-p2996 fork is fine; if you ever ship a non-clang build the IDs would shift. Acceptable since the engine is single-compiler.
- **Templates get long names.** `std::vector<gse::physics::motion_component>` produces a long display string. FNV-1a still hashes to 64 bits cleanly; only matters if you log the raw tag.
- **`typeid` itself stays available.** This reform doesn't touch `typeid` — keeps it for the rare runtime polymorphism case (none currently exist in the engine; document it as the escape hatch).

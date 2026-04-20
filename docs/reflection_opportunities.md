# P2996 Reflection Opportunities

## Overview

This document catalogs opportunities to apply C++26 static reflection (P2996, via the Bloomberg clang-p2996 fork) across GSEngine. It is split into two parts:

- **Part I — Tactical Opportunities.** Existing code, ranked S/A/B/C, each a concrete refactor that deletes boilerplate or prevents a drift bug.
- **Part II — Architectural Reshapes.** How new systems should be structured going forward. Design philosophy, not a migration plan.

The two parts are read differently. Part I is a punch list; Part II is an orientation for new work.

# Part I: Tactical Opportunities

Reflection features relied on throughout:

- `^^T` / `^^expr` — reflect on a type, namespace, member, enumerator, or constant
- `std::meta::nonstatic_data_members_of`, `enumerators_of`, `members_of`
- `std::meta::annotations_of` + `[[= value]]` — carry compile-time metadata on declarations
- `std::meta::define_aggregate` + `consteval {}` injection blocks — synthesize types/declarations at compile time
- `template for (constexpr auto m : ...)` — expansion statements over reflection ranges
- Splicers `[: r :]` — paste a reflected entity into a program

## Priority Summary

| Rank | Target | Lines eliminated | Risk | Unlocks new capability? |
|------|--------|-----------------:|------|-------------------------|
| **S** | [Unit system declarations](#s1-unit-system-quantity-declarations) | ~600 | low | Yes — tag recovery after arithmetic |
| **A** | [Vulkan enum `to_vk` family](#a1-vulkan-enum-to_vk-converters) | ~140 | trivial | No |
| **A** | [Generic `hash_combine`](#a2-generic-reflected-hash_combine) | ~30 now + future | trivial | No |
| **A** | [Reflected Archive serialization](#a3-reflected-binary-archive) | scales with components | low | Prevents future drift |
| **B** | [`_net` / `_data` struct projection](#b1-networkedlocal-struct-projection) | N × component fields | low | Yes — single source of truth |
| **B** | [Vulkan bitfield flag converters](#b2-vulkan-bitfield-flag-converters) | ~30 | trivial | No |
| **B** | [Theme palette](#b3-gui-theme-palette) | ~40 | trivial | No |
| **B** | [Generic `std::formatter<T>`](#b4-generic-reflected-stdformatter) | 30-file reach | medium | Yes — print any struct |
| **B** | [Pipeline state member-pointer mapping](#b5-vulkan-pipeline-state-struct-filling) | ~35 | medium | Compile-time binding safety |
| **B** | [Animation condition dispatch](#b6-animation-condition-variant-dispatch) | ~40 | low | Yes — enum/value lockstep |
| **C** | [ECS `component_storage` vtable](#c1-ecs-component_storage-vtable-elision) | perf + boilerplate | medium-high | Yes — O(1) direct storage |
| **C** | [`SaveSystem::property_base`](#c2-savesystem-property_base-hierarchy) | hundreds | high churn | Yes — compile-time settings |
| **C** | [Shape-type dispatch](#c3-physics-shape-type-dispatch) | ~25 per site | low | No |
| **C** | [Audio request dispatch](#c4-audio-request-loop-fusion) | ~40 | low | No |
| **A** | [Network message system](#a4-network-message-ceremony) | ~150 + 4 files future | low | Yes — eliminates ID-collision bug class |
| **A** | [Push-constant binding](#a5-shader-push-constant-binding) | ~120 runtime lookups eliminated | low | Yes — compile-time binding + C++↔Slang single source |
| **C** | [Network component registry](#c5-network-component-registry) | ~50 | low | Yes — can't register in one place |
| **C** | [`ActionsApi` channel overloads](#c6-actionsapi-channel-overloads) | ~15 | low | No |
| **C** | [`CoreApi` forwarding functions](#c7-coreapi-forwarding-stubs) | ~12 | low | No |
| **C** | [Generic `enum_to_string`](#c8-generic-enum_to_string) | small but pervasive | trivial | Yes — config/UI for any enum |

**Running total:** ~1250 lines + 120 hot-path runtime lookups eliminated across ~17 sites, plus several capabilities that are unavailable without reflection — including a real bug class (network message ID collisions) that reflection makes unrepresentable, and a C++↔Slang single-source-of-truth for push-constant layouts.

---

## Shared Primitives

Before any of the per-site refactors, introduce `gse.meta` in `Engine/Engine/Source/Core/Utility/Meta.cppm`, re-exported from `gse.utility`. It holds the annotation tag types and reflection utilities everything else depends on.

Planned contents:

- Annotation tag types: `struct archive_skip{}; struct format_skip{}; struct networked{}; struct theme_color{};`
- `template <typename T> constexpr auto hash_combine(const T&) -> std::size_t;`
- `template <typename Vk, typename E> constexpr auto to_vk(E) -> Vk;` + bitflag variant
- `template <typename E> constexpr auto enum_to_string(E) -> std::string_view;` + `from_string`
- `template <typename T, typename Tag> using project_by_annotation = /* reflected type */;`
- Reflected `std::formatter<T>` base CRTP / specialization template
- Annotation helper `template <auto V> concept has_annotation = ...;`

---

## Rank S

### S1. Unit system quantity declarations

**File:** `Engine/Engine/Source/Math/Units/Units.cppm` (779 lines, ~85% repetition)

**Current boilerplate (per quantity, repeated ~35 times):**

```cpp
struct gap_tag {};
template <>
struct internal::quantity_tag_traits<gap_tag> {
    using parent_tag = displacement_tag;
    using unit_tag = length_tag;
    using relative_tag = displacement_tag;
    static constexpr auto semantic_kind = quantity_semantic_kind::relative;
};

template <typename T = float, auto U = meters>
using gap_t = internal::quantity<T, internal::dimi<1, 0, 0, 0>, gap_tag, decltype(U)>;
using gap = gap_t<>;

template <>
struct internal::quantity_traits<gap_tag> {
    template <typename T, auto U = meters>
    using type = gap_t<T, U>;
};
```

That's ~18 lines × ~35 quantities = **~600 lines** where only the tag name, dimension, parent, and default unit actually differ.

**Reflection replacement:**

```cpp
[[= quantity_spec{
    .name = "gap",
    .dim = {1, 0, 0, 0},
    .parent = ^^displacement_tag,
    .unit = ^^length_tag,
    .relative = ^^displacement_tag,
    .kind = quantity_semantic_kind::relative,
    .default_unit = ^^meters
}]]
struct gap_info;
```

One `consteval { emit_quantities(); }` block scans the namespace, reads each `quantity_spec` annotation, and injects the four declarations via `queue_injection` / `define_aggregate`. ~600 lines → ~40 lines.

**Unlocked capabilities reflection enables here that templates cannot:**

1. **Tag recovery after arithmetic.** `force / mass` currently returns a synthetic-tag `quantity<...dimi<1,-2,0,0>>`; the named `acceleration` is lost. Reflect over all registered specs, match on `.dim`, and splice the found tag back into the result type.
2. **Auto-populated `units` tuple.** Only `time_tag` has a `units` tuple today (`Units.cppm:334`). A reflected namespace scan can assemble each quantity's unit list from every `internal::unit<tag, ratio, name>` constant of the right tag, used by UI dropdowns and formatters.
3. **Reflected `std::formatter<quantity>`** with format spec like `{:km/h}` — the spec-parser reflects over the tag's units, finds the one whose `name` matches, applies the ratio conversion. One formatter for all 35 quantities.
4. **Generic `to_toml` / `from_toml`** for any quantity. Save system no longer needs a `constraint_type::range` per quantity type.

**Risk:** Low. Change is isolated to `Units.cppm` + `Quantity.cppm`; call sites stay identical because aliases and traits have the same public names.

---

## Rank A

### A1. Vulkan enum `to_vk` converters

**File:** `Engine/Engine/Source/Platform/Vulkan/VulkanEnums.cppm:94-226`

**Current boilerplate:** 10 switches of the same shape, e.g.

```cpp
auto gse::vulkan::to_vk(const gpu::compare_op op) -> vk::CompareOp {
    switch (op) {
        case gpu::compare_op::never:            return vk::CompareOp::eNever;
        case gpu::compare_op::less:             return vk::CompareOp::eLess;
        case gpu::compare_op::equal:            return vk::CompareOp::eEqual;
        case gpu::compare_op::less_or_equal:    return vk::CompareOp::eLessOrEqual;
        case gpu::compare_op::greater:          return vk::CompareOp::eGreater;
        case gpu::compare_op::not_equal:        return vk::CompareOp::eNotEqual;
        case gpu::compare_op::greater_or_equal: return vk::CompareOp::eGreaterOrEqual;
        case gpu::compare_op::always:           return vk::CompareOp::eAlways;
    }
    return vk::CompareOp::eAlways;
}
```

**Reflection replacement:** annotate each enumerator with its Vulkan counterpart; write `to_vk` **once**.

```cpp
enum class compare_op {
    [[= vk::CompareOp::eNever]]
    never,
    [[= vk::CompareOp::eLess]]
    less,
    [[= vk::CompareOp::eEqual]]
    equal,
    [[= vk::CompareOp::eLessOrEqual]]
    less_or_equal,
    [[= vk::CompareOp::eGreater]]
    greater,
    [[= vk::CompareOp::eNotEqual]]
    not_equal,
    [[= vk::CompareOp::eGreaterOrEqual]]
    greater_or_equal,
    [[= vk::CompareOp::eAlways]]
    always,
};

template <typename Vk, typename E> requires std::is_enum_v<E>
constexpr auto to_vk(E e) -> Vk {
    template for (constexpr auto v : std::meta::enumerators_of(^^E)) {
        if ([:v:] == e) {
            constexpr auto anns = std::meta::annotations_of(v, ^^Vk);
            static_assert(anns.size() == 1, "missing [[= Vk::... ]] on enumerator");
            return [:anns[0]:];
        }
    }
    std::unreachable();
}
```

**Covered enums:** `cull_mode`, `compare_op`, `polygon_mode`, `topology`, `sampler_filter`, `sampler_address_mode`, `border_color`, `image_layout`, `image_format`, `vertex_format`, `descriptor_type`. `blend_preset` (line 228) uses the same pattern with a struct-valued annotation.

**Lines killed:** ~140. **Risk:** trivial — the enum *is* the mapping table now; forgetting an annotation is a compile error.

### A2. Generic reflected `hash_combine`

**Files:**

- `Engine/Engine/Source/Physics/VBD/ContactCache.cppm:74-83` (10 hand-unrolled XOR-combine lines)
- `Engine/Engine/Source/Physics/System.cppm:262-264`
- `Engine/Engine/Source/Graphics/3D/Animations/Animation.cppm:50-52` (`pose_cache_key_hash`)
- `Engine/Engine/Import/Network.cppm:112` (`address`)
- `Server/Server/Include/Server.cppm:18-21` (another `address` hash)

Every site reimplements `h ^= std::hash<F>{}(k.field) + 0x9e3779b9 + (h << 6) + (h >> 2)` field by field.

**Reflection replacement** in `gse::meta`:

```cpp
template <typename T>
constexpr auto hash_combine(const T& v) -> std::size_t {
    std::size_t h = 0;
    template for (constexpr auto m : std::meta::nonstatic_data_members_of(^^T))
        h ^= std::hash<[: std::meta::type_of(m) :]>{}(v.[:m:])
             + 0x9e3779b9u + (h << 6) + (h >> 2);
    return h;
}
```

Each `std::hash<...>` specialization collapses to one line. All POD key structs can then use `= default` on `operator==`.

**Lines killed:** ~30 across 5 files, plus every future key struct stays one line.

### A3. Reflected binary archive

**File:** `Engine/Engine/Source/Core/Utility/Archive.cppm:210-213, 309-312`

**Current pattern:** ADL-dispatched hook for any non-trivially-copyable type:

```cpp
template <typename T> requires (!std::is_trivially_copyable_v<T>)
auto operator&(const T& value) -> binary_writer& {
    serialize(*this, const_cast<T&>(value));
    return *this;
}
```

Every such `T` requires a hand-written `serialize(binary_writer&, T&)` that lists every field.

**Blast radius today:** a grep for `auto serialize(binary_writer` returns **zero matches**. The hook is wired but unpopulated. This is the cheapest possible moment to land the reflected version, before the boilerplate proliferates.

**Reflection replacement:**

```cpp
template <typename T> requires (!std::is_trivially_copyable_v<T> && !has_user_serialize<T>)
auto operator&(const T& v) -> binary_writer& {
    template for (constexpr auto m : std::meta::nonstatic_data_members_of(^^T)) {
        if constexpr (!std::meta::has_annotation<m, archive_skip>())
            *this & v.[:m:];
    }
    return *this;
}
```

Opt-out per field:

```cpp
[[= archive_skip]]
std::string m_debug_name;
```

Keep the user-hook escape for exotic cases.

### A4. Network message ceremony

**Files:** `Engine/Engine/Source/Network/Message/*.cppm` — `PingPong.cppm`, `InputFrame.cppm`, `ServerInfo.cppm`, `NotifySceneChange.cppm`, plus the `Message.cppm` harness itself.

**Real bug this exposes right now:** message IDs are hand-picked `std::uint16_t` literals and **two pairs collide**:

| Message | ID | File:line |
|---------|----|-----------|
| `server_info_response` | `0x0006` | `ServerInfo.cppm:58` |
| `input_frame_header`   | `0x0006` | `InputFrame.cppm:33` |
| `server_info_request`  | `0x0005` | `ServerInfo.cppm:47` |
| `notify_scene_change`  | `0x0005` | `NotifySceneChange.cppm:30` |

Any `message_switch` dispatching on the wire ID will hit the wrong handler. Reflection makes this bug class impossible.

**Current ceremony per message type:**

```cpp
// PingPong.cppm — pattern repeated for every message
export namespace gse::network {
    struct ping {
        std::uint32_t sequence{};
    };

    constexpr auto message_id(std::type_identity<ping>) -> std::uint16_t;
    auto encode(const ping& msg, bitstream& stream) -> void;
    auto decode(bitstream& stream, std::type_identity<ping>) -> ping;
}

constexpr auto gse::network::message_id(std::type_identity<ping>) -> std::uint16_t {
    return 0x0003;
}
auto gse::network::encode(const ping& msg, bitstream& stream) -> void {
    stream.write(msg.sequence);
}
auto gse::network::decode(bitstream& stream, std::type_identity<ping>) -> ping {
    return { .sequence = stream.read<std::uint32_t>() };
}
```

That's **6 declarations + 3 definitions per message**, times 6+ message types. Every `encode` / `decode` is just the Archive pattern (write/read each field) with a different name. And the IDs are a manually-maintained namespace begging for collisions.

**Reflection replacement:**

```cpp
export namespace gse::network {
    [[= network_message]]
    struct ping { std::uint32_t sequence{}; };

    [[= network_message]]
    struct pong { std::uint32_t sequence{}; };

    [[= network_message]]
    struct notify_scene_change { id scene_id{}; };
    // ... each message file collapses to 1-3 lines
}
```

Three free helpers replace all the per-type code:

```cpp
template <typename T> requires has_annotation<^^T, network_message>
consteval auto message_id() -> std::uint16_t {
    return stable_code(std::meta::display_string_of(^^T));   // reuse existing stable_code from RegistrySync
}

template <typename T> requires has_annotation<^^T, network_message>
auto encode(bitstream& s, const T& msg) -> void {
    template for (constexpr auto m : std::meta::nonstatic_data_members_of(^^T))
        s.write(msg.[:m:]);
}

template <typename T> requires has_annotation<^^T, network_message>
auto decode(bitstream& s, std::type_identity<T>) -> T {
    T msg{};
    template for (constexpr auto m : std::meta::nonstatic_data_members_of(^^T))
        msg.[:m:] = s.read<[: std::meta::type_of(m) :]>();
    return msg;
}
```

**Second, equally important win — `message_switch` collapses.** The 11-overload `message_switch` class at `Message.cppm:53-118` (lvalue `&` / rvalue `&&` variants of `if_is<T>` / `else_if_is<T>` / `otherwise`) is ~90 lines of runtime pseudo-switch built out of ADL-dispatched `try_decode` calls. Reflect over a namespace-scoped list of `[[= network_message]]`-annotated types and generate a real compile-time dispatch:

```cpp
template <typename Handlers>
auto dispatch_message(bitstream& s, std::uint16_t id, Handlers&& h) -> bool {
    bool handled = false;
    template for (constexpr auto t : network_message_types()) {
        using T = typename [: t :];
        if (!handled && id == message_id<T>()) {
            if constexpr (requires { h(std::declval<T>()); }) {
                T msg = decode(s, std::type_identity<T>{});
                h(msg);
                handled = true;
            }
        }
    }
    return handled;
}
```

Call sites stay readable:

```cpp
dispatch_message(stream, id, overloaded{
    [](const ping& p)              { respond_pong(p.sequence); },
    [](const server_info_request&) { send_server_info(); },
    [](const notify_scene_change& m) { load_scene(m.scene_id); },
});
```

**Scope of what this kills:**

- 6 per-message files shrink to struct-definition only (~30 lines × 6 = 180 lines of ceremony → ~20 lines total)
- `message_switch` class and its 11 overloads (~90 lines) → one reflected dispatch helper (~15 lines)
- Hand-picked IDs replaced by `stable_code(type_name)`; collisions become compile errors via `static_assert(no_id_collisions<messages>())`
- The ADL `encode` / `decode` free functions (Message.cppm:17-27 primary templates plus every per-type specialization) → one reflected definition

**Ties into existing infrastructure:** `stable_code` already exists in `RegistrySync.cppm:117-122`. The component-registry `is_network_message` tag pattern from opportunity C5 is the same machinery — this generalizes it to every message, not just component upserts/removes.

**Risk:** low. Wire format stays byte-compatible (`stable_code` of each struct name is stable across compilations and matches what the component registry already does). The migration is strict subtraction — delete per-message ADL overloads one file at a time and verify the reflected path handles them.

### A5. Shader push-constant binding

**Files (call sites):** `Graphics/Renderers/{ForwardRenderer,DepthPrepassRenderer,CullComputeRenderer,SkinComputeRenderer,RtShadowRenderer,PhysicsTransformRenderer,UiRenderer,CaptureRenderer}.cppm`, `Physics/VBD/GpuSolver.cppm` — **~120 `pc.set("name", value)` call sites across 9 files**.

**File (implementation):** `Engine/Engine/Source/Platform/Vulkan/GpuPushConstants.cppm:37-43`.

**Scope boundary — what this is not.** Shader reflection (the Slang compiler inspecting shader source at build time to emit `.glayout` descriptor binding info, `ShaderLayoutCompiler.cppm:36-72`) is not something C++ reflection can replace. That data lives in binary files produced by an external toolchain. Keep the compiler, keep the `.glayout` format, keep `shader_layout::load`.

**What it is.** Every `pc.set` call does runtime work for compile-time-known information:

```cpp
template <typename T>
auto cached_push_constants::set(std::string_view name, const T& value) -> void {
    const auto it = members.find(std::string(name));                         // hash lookup
    assert(it != members.end(), ..., "Push constant member '{}' not found"); // runtime
    assert(sizeof(T) <= it->second.size, ..., "size mismatch");              // runtime
    gse::memcpy(data.data() + it->second.offset, value);
}
```

Call sites like `ForwardRenderer.cppm:422-429` are 8 consecutive `pc.set("...", ...)` calls every frame, each an `unordered_map<std::string, push_constant_member>` lookup.

**Reflection replacement — two layers.**

**Layer 1 — compile-time binding.** Mirror each shader's push-constant block as an annotated C++ struct; `set` becomes zero-cost:

```cpp
namespace gse::shaders::forward {
    [[= push_constants_for("standard3d.slang")]]
    struct push_constants {
        std::uint32_t meshlet_offset;
        std::uint32_t meshlet_count;
        std::uint32_t first_instance;
        std::uint32_t num_lights;
        vec2u screen_size;
        std::uint32_t shadow_quality;
        std::uint32_t ao_quality;
        std::uint32_t reflection_quality;
    };
}

template <typename Struct>
auto cached_push_constants::set(const Struct& block) -> void {
    template for (constexpr auto m : std::meta::nonstatic_data_members_of(^^Struct)) {
        const auto it = members.find(std::meta::identifier_of(m));
        assert(it != members.end() && sizeof([: std::meta::type_of(m) :]) == it->second.size, ...);
        std::memcpy(data.data() + it->second.offset, &block.[:m:], sizeof(block.[:m:]));
    }
}
```

Usage collapses from 8 lines to one:

```cpp
pc.set(shaders::forward::push_constants{
    .meshlet_offset = 0,
    .meshlet_count = ml_count,
    .first_instance = batch.first_instance,
    .num_lights = num_lights_i,
    .screen_size = ext,
    .shadow_quality = shadow_quality_i,
    .ao_quality = ao_quality_i,
    .reflection_quality = reflection_quality_i,
});
```

Asserts run once per distinct `Struct` type (cacheable); the per-field copy is a direct `memcpy` at a known offset. Fields are checked against the `.glayout` at first use — mismatch becomes a clear one-time error rather than a silent `value_or` miss.

**Layer 2 — single source of truth via build-time Slang codegen.** Today the push-constant layout exists implicitly in both the Slang source and every `pc.set` call. With the annotated C++ struct, a ~30-line build step walks `nonstatic_data_members_of` and emits a Slang header:

```
// gen/push_constants_forward.slang  (generated; do not edit)
struct PushConstants {
    uint   meshlet_offset;
    uint   meshlet_count;
    uint   first_instance;
    uint   num_lights;
    uint2  screen_size;
    uint   shadow_quality;
    uint   ao_quality;
    uint   reflection_quality;
};
```

Shaders `#include` the generated file. C++ and Slang sides can no longer drift — adding a field on either side is a compile error on the other. The CLAUDE.md invariant ("unit types are layout-compatible with float, can pass directly to GPU push constants") is what lets this work cleanly; quantity types in push-constant blocks emit as `float` on the Slang side automatically.

**Lines killed:** 120 runtime lookups at call sites collapse to one `pc.set(struct_literal)` per renderer. Cumulative reduction across the 9 renderer files is substantial, and the hot-path cost drops from hashmap-per-field to memcpy-per-field.

**Risk:** low. Rollout is file-by-file — each renderer switches to a named struct, call sites change mechanically, old `pc.set(string_view, T)` overload stays until the last caller is migrated and then deletes cleanly.

**Ties in:** uses the same `std::meta::identifier_of` + `offset_of` primitives as A3 (reflected Archive) and the generic `std::formatter`. The enum annotation switch in `ShaderLayoutCompiler.cppm:36-72` (`to_vk_descriptor_type`) is another site for the A1 pattern — not a new opportunity, just another customer of it.

---

## Rank B

### B1. Networked/local struct projection

**File:** `Engine/Engine/Source/Physics/MotionComponent.cppm:13-50`

**Current pattern:** two structs with 9 shared fields, kept in sync by hand:

```cpp
struct motion_component_net {
    vec3<current_position> current_position;
    vec3<velocity> current_velocity;
    mass mass = kilograms(1.f);
    quat orientation = quat(1.f, 0.f, 0.f, 0.f);
    vec3<angular_velocity> angular_velocity;
    inertia moment_of_inertia = kilograms_meters_squared(1.f);
    bool affected_by_gravity = true;
    bool airborne = true;
    bool position_locked = false;
};

struct motion_component_data {
    // 9 fields duplicated verbatim + extras:
    float restitution = 0.3f;
    bool sleeping = false;
    bool update_orientation = true;
    vec3<velocity> velocity_drive_target;
    bool velocity_drive_active = false;
    vec3<impulse> pending_impulse;
};
```

**Reflection replacement:**

```cpp
struct motion_component_data {
    [[= networked]]
    vec3<current_position> current_position;

    [[= networked]]
    vec3<velocity> current_velocity;

    [[= networked]]
    mass mass = kilograms(1.f);

    [[= networked]]
    quat orientation = quat(1.f, 0.f, 0.f, 0.f);

    [[= networked]]
    vec3<angular_velocity> angular_velocity;

    [[= networked]]
    inertia moment_of_inertia = kilograms_meters_squared(1.f);

    [[= networked]]
    bool affected_by_gravity = true;

    [[= networked]]
    bool airborne = true;

    [[= networked]]
    bool position_locked = false;

    float restitution = 0.3f;
    bool sleeping = false;
    bool update_orientation = true;
    vec3<velocity> velocity_drive_target;
    bool velocity_drive_active = false;
    vec3<impulse> pending_impulse;
};

using motion_component_net = project_by_annotation<motion_component_data, networked>;
```

`project_by_annotation` uses `define_aggregate` over `nonstatic_data_members_of` filtered by `annotations_of`. Same pattern applies to every component with a `_net` twin.

**Risk:** low. Net-data layout must stay trivially copyable (asserted by existing `static_assert` in `RegistrySync.cppm:168`). Reflection preserves declaration order, so the projection is deterministic.

### B2. Vulkan bitfield flag converters

**Files:**

- `VulkanEnums.cppm:70-91` (`buffer_usage` → `BufferUsageFlags`)
- `VulkanEnums.cppm:204-213` (`stage_flags` → `ShaderStageFlags`)
- `GpuFactory.cppm:130-137` (image usage)

**Current pattern:**

```cpp
if (flags.test(vertex))   result |= vk::ShaderStageFlagBits::eVertex;
if (flags.test(fragment)) result |= vk::ShaderStageFlagBits::eFragment;
if (flags.test(compute))  result |= vk::ShaderStageFlagBits::eCompute;
if (flags.test(task))     result |= vk::ShaderStageFlagBits::eTaskEXT;
if (flags.test(mesh))     result |= vk::ShaderStageFlagBits::eMeshEXT;
```

**Reflection replacement:** annotate enumerators, reflect to generate the OR-fold. Compound rules (e.g. `uniform|storage|indirect → +eShaderDeviceAddress` at `VulkanEnums.cppm:88-90`) expressible as a second `[[= auto_flag_rule]]` annotation on the flag set type.

### B3. GUI theme palette

**File:** `Engine/Engine/Source/Graphics/2D/Gui/Styles.cppm:88-210+`

**Current pattern:** four theme functions (`dark()`, `darker()`, `light()`, `high_contrast()`), each initializing ~30 fields. Sizing fields (`padding = 10.f`, `title_bar_height = 30.f`, `font_size = 16.f`, …) are identical across all four themes — ~40 lines of pure copy-paste.

**Reflection replacement:** annotate the color fields, split the struct into "varies per theme" vs "shared defaults":

```cpp
struct style {
    [[= theme_color]]
    vec4f color_title_bar = {...};

    [[= theme_color]]
    vec4f color_text = {...};
    // ...

    float padding = 10.f;             // no annotation → same across themes
    float title_bar_height = 30.f;
    float font_size = 16.f;
    // ...
};
```

Each theme table lists only its color overrides. `from_theme(t)` reflects over `theme_color`-annotated fields and patches them over a default-constructed `style{}`.

### B4. Generic reflected `std::formatter`

**Scope:** 105 call sites of `std::format` / `fmt::format` across 30 files. Every custom struct currently needs a hand-written formatter specialization or manual field-by-field format strings.

**Replacement** in `gse::meta`:

```cpp
template <typename T> requires (std::is_class_v<T> && !has_user_formatter<T>)
struct std::formatter<T, char> : std::formatter<std::string, char> {
    auto format(const T& v, auto& ctx) const {
        std::string out = "{ ";
        template for (constexpr auto m : std::meta::nonstatic_data_members_of(^^T)) {
            if constexpr (!has_annotation<m, format_skip>())
                out += std::format("{}={} ", std::meta::identifier_of(m), v.[:m:]);
        }
        out += "}";
        return std::formatter<std::string, char>::format(out, ctx);
    }
}
```

Assertions and every debug print get automatic pretty-printing for any struct. `[[= format_skip]]` on noisy fields (keys, byte buffers, caches).

**Risk:** medium. Need to ensure it doesn't conflict with existing specializations (GLM types, quantities); SFINAE / `requires`-guard on `!has_user_formatter<T>`.

### B5. Vulkan pipeline state struct filling

**File:** `Engine/Engine/Source/Platform/Vulkan/GpuFactory.cppm:251-328`

**Current pattern:**

```cpp
const vk::PipelineRasterizationStateCreateInfo rasterizer{
    .depthClampEnable = vk::False,
    .rasterizerDiscardEnable = vk::False,
    .polygonMode = vulkan::to_vk(desc.rasterization.polygon),
    .cullMode = vulkan::to_vk(desc.rasterization.cull),
    .frontFace = vk::FrontFace::eCounterClockwise,
    .depthBiasEnable = desc.rasterization.depth_bias ? vk::True : vk::False,
    .depthBiasConstantFactor = desc.rasterization.depth_bias_constant,
    .depthBiasClamp = desc.rasterization.depth_bias_clamp,
    .depthBiasSlopeFactor = desc.rasterization.depth_bias_slope,
    .lineWidth = desc.rasterization.line_width
};
```

**Reflection replacement:** annotate engine-side struct fields with member-pointer-to-Vulkan-field, reflect to generate the Vk struct initializer:

```cpp
struct rasterization_state {
    [[= &vk::PipelineRasterizationStateCreateInfo::polygonMode]]
    gpu::polygon_mode polygon;

    [[= &vk::PipelineRasterizationStateCreateInfo::cullMode]]
    gpu::cull_mode cull;
    // ...
};
```

Makes it impossible to bind a field to the wrong Vulkan slot.

### B6. Animation condition variant dispatch

**File:** `Engine/Engine/Source/Graphics/3D/Animations/Animation.cppm`

**Current pattern:** `switch` on `transition_condition_type` with cases for `bool_equals`, `float_greater`, `float_less`, `trigger` — each case casts and checks a `std::variant`. The enum and the associated value types are in lockstep; adding a new kind means touching both.

**Reflection replacement:**

```cpp
enum class condition_kind {
    [[= value_t<bool>]]
    bool_equals,
    [[= value_t<float>]]
    float_greater,
    [[= value_t<float>]]
    float_less,
    [[= value_t<std::monostate>]]
    trigger,
};
```

The evaluator reflects over `enumerators_of(^^condition_kind)`, pulls each one's annotated value type, and generates dispatch + `std::get`.

---

## Rank C

### C1. ECS `component_storage` vtable elision

**File:** `Engine/Engine/Source/Core/Utility/Ecs/Registry.cppm:52-115`

**Current pattern:** `component_storage_base` with two pure virtuals (`activate_pending`, `remove_owner`) exists only so `registry` can store heterogeneous storages in an `unordered_map<type_index, unique_ptr<base>>`. Runtime type-erasure reinventing reflection.

**Reflection replacement:**

1. Reflect over a canonical list of component types (generated from `[[= component]]` annotations on struct definitions).
2. Replace the type-indexed map with a compile-time static array, indexed by `info_to_ordinal<^^T>()`.
3. Drop `component_storage_base`, drop the virtuals, drop the `unique_ptr` indirection.

**Benefit beyond line count:** O(1) direct-array storage access replaces unordered_map lookup on every component access. This is the biggest runtime performance win on the list.

**Risk:** medium-high. Storage is hot code; needs careful perf verification and probably a parallel rollout.

### C2. SaveSystem `property_base` hierarchy

**File:** `Engine/Engine/Source/Core/Utility/SaveSystem.cppm:59-150+`

**Current pattern:** `property_base` with 15+ virtuals, `register_property` struct with `void*` + `type_index` + `std::function` factory, multiple `make_property_registration` overloads, enum constraint metadata built by hand. An entire runtime reinvention of reflection.

**Reflection replacement:** annotate settings struct fields directly:

```cpp
struct graphics_settings {
    [[= setting("Graphics", "shadow map resolution", range{256, 4096, 256})]]
    std::uint32_t shadow_map_resolution = 1024;

    [[= setting("Graphics", "anti-aliasing", enumeration{})]]
    aa_mode anti_aliasing = aa_mode::taa;
};
```

Reflect over the struct to build: TOML reader/writer, ImGui renderer, dirty tracking, reset-to-default — no `void*`, no `type_index`, no virtual dispatch. Range/enumeration constraints come from annotation values.

**Risk:** high churn. SaveSystem has many clients; plan for a parallel migration per category.

**Dependency note — can we drop tomlplusplus?**

Reflection replaces the *glue* between structs and tomlplusplus, not tomlplusplus itself. Text parsing and emitting are not things reflection can do — something still has to turn `"AO Quality" = 2` into structured data. Scope of tomlplusplus usage today:

- 26 `toml::*` calls across `Save.cppm` and `SaveSystem.cppm`
- 2 data files totalling 100 lines (`settings.toml` 66, `gui_layout.toml` 34)
- Only basic TOML features used: section headers, strings, ints, floats, bools, flat arrays, arrays of tables
- Existing integration already has a quote-substitution hack at `Save.cppm:96,118`

After this refactor, the tomlplusplus surface area in engine code shrinks to ~5 call sites (`toml::parse`, `toml::table`, `operator<<`, `toml::parse_error`). At that point, dropping the dep is a ~150-LoC hand-rolled parser project — feasible but not driven by reflection capability. **Recommendation: keep tomlplusplus.** Revisit only if compile time or binary size becomes a motivating concern.

### C3. Physics shape-type dispatch

**File:** `Engine/Engine/Source/Graphics/Renderers/PhysicsDebugRenderer.cppm:314-326`

**Current pattern:**

```cpp
switch (coll.shape) {
    case physics::shape_type::sphere:  build_sphere_lines_for_collider(...); break;
    case physics::shape_type::capsule: build_capsule_lines_for_collider(...); break;
    default:                           build_obb_lines_for_collider(...); break;
}
```

**Reflection replacement:** annotate enumerators with the builder function pointer; dispatch through a reflected table. Reusable at every other shape-type switch (collision narrow-phase, etc.).

### C4. Audio request loop fusion

**File:** `Engine/Engine/Source/Audio/Audio.cppm:315-350`

Six `for (auto& req : ctx.read_channel<Req>())` blocks, each with a tiny handler. Reflect over a `[[= audio_request]]`-tagged list and fuse into one generic dispatcher.

### C5. Network component registry

**File:** `Engine/Engine/Source/Network/Message/RegistrySync.cppm:104-143`

**Current pattern:** a `networked_types` tuple listing component types AND four `component_name<T>()` specializations, all kept in sync by hand.

```cpp
constexpr auto networked_types = std::tuple<
    std::type_identity<physics::motion_component>,
    std::type_identity<physics::collision_component>,
    std::type_identity<render_component>,
    std::type_identity<player_controller>
>{};

template <> consteval auto component_name<gse::physics::motion_component>() -> std::string_view {
    return "gse.physics.motion_component";
}
// × 4
```

**Reflection replacement:** annotate each component with `[[= networked_component("gse.physics.motion_component")]]`; reflect over the namespace to build both the tuple and the name mapping. Adding a new networked component becomes a one-line annotation on its definition.

### C6. `ActionsApi` channel overloads

**File:** `Engine/Engine/Source/Core/Runtime/Api/ActionsApi.cppm:26-52`

Three overloads of `bind_*_channel` / `register_channel` for `button_channel`, `axis1_channel`, `axis2_channel` — identical structure. Reflect over a `[[= input_channel]]`-tagged list of channel types to generate the overload set.

### C7. `CoreApi` forwarding stubs

**Files:** `Engine/Engine/Source/Core/Runtime/Api/*.cppm`

**Current pattern:** every public function is essentially `state_of<X::system_state>().current_state().some_method(args...)`.

**Reflection replacement:** a `forward_to<&input::system_state::key_pressed>` helper lets each API file collapse to a list of one-liner re-exports using function-pointer reflection.

### C8. Generic `enum_to_string`

Every `enum class` used in serialization, logging, or UI dropdowns (e.g. `shape_type` at `CollisionComponent.cppm:14`, GUI styles, animation enums, `theme` at `Styles.cppm:12`) gets auto-string-conversion:

```cpp
template <typename E> requires std::is_enum_v<E>
constexpr auto enum_to_string(E e) -> std::string_view {
    template for (constexpr auto v : std::meta::enumerators_of(^^E))
        if ([:v:] == e) return std::meta::identifier_of(v);
    return "<unknown>";
}
```

Pairs with `from_string` for config deserialization.

---

## Recommended Rollout Order

The rollout order maximizes payoff-per-day of engineering while minimizing risk and rework:

1. **Bootstrap `gse::meta`** (primitives module). Empty shell with annotation tags and the concept helpers. No behavior change.
2. **A2 `hash_combine`** — smallest change, immediate benefit, validates the primitives module.
3. **C8 `enum_to_string`** — also tiny, used by several later steps.
4. **A1 Vulkan enum annotations** — big, isolated, low-risk. Good confidence builder for the annotation pattern.
5. **S1 Unit system** — biggest single win, contained to `Units.cppm`. Needs the `consteval` injection pattern; once working, reusable everywhere.
6. **A3 Reflected Archive** — land before any hand-written `serialize` functions appear. Immediately reusable by A4.
7. **A4 Network message ceremony** — eliminates the ID-collision bugs currently on wire and collapses 6 per-message files. Leans on A3's reflected encode/decode.
8. **A5 Push-constant binding** — layer 1 first (compile-time `set(struct)`), then layer 2 (Slang codegen) once the pattern has proven stable.
9. **B1 `_net` / `_data` projection**, then **C5 network registry** — collapses physics/network boilerplate together.
10. **B3 Theme palette**, **B2 Vk bitfield flags**, **B6 animation conditions** — straightforward follow-ups once the pattern is proven.
11. **B4 Generic `std::formatter`** — land when the other annotation patterns are stable, since it touches 30 files.
12. **B5 Pipeline state annotations**, **C3 shape dispatch**, **C4 audio dispatch**, **C6 actions**, **C7 core API** — polish passes.
13. **C1 ECS storage vtable** — schedule as a dedicated perf refactor with benchmarks.
14. **C2 SaveSystem property_base** — the big rewrite; do last when everything else has validated the patterns.

---

# Part II: Architectural Reshapes

The opportunities above are **tactical** — existing code you've written that reflection deletes. This section is different: it's about **how new systems should be structured going forward**, given reflection as a primitive. Read this as design philosophy, not as a migration plan.

## The Framing Insight

Several existing systems are *half-way to reflection*: they carry runtime machinery that looks like a reflection system because it **is** one, just built by hand without the compile-time bridge. Examples already in the codebase:

- `scheduled_work` in `UpdateContext.cppm:16-21` — a `std::vector<std::type_index>` reads/writes list per work item, populated at runtime when a system calls `ctx.read<T>()` / `ctx.write<T>()`.
- `render_graph` resource tracking in `RenderGraph.cppm:58-96` — a `resource_usage` vector per pass, populated by `sampled(img, stage)` / `storage(buf, stage)` / `attachment(img, stage)` call chains.
- `cached_push_constants` in `GpuPushConstants.cppm:18-34` — a `unordered_map<string, push_constant_member>` populated from `.glayout` reflection data, then queried by name at every `set` call.
- `SaveSystem::property_base` in `SaveSystem.cppm:91-150` — a virtual hierarchy holding `void*` + `type_index` + factory `std::function`, existing solely to learn at runtime what fields a struct has.

Each of these is a domain-specific reinvention of "walk struct fields and do something per field." Reflection doesn't make them smaller — it **completes** them. The architectural complexity that exists solely to substitute for reflection collapses.

The nine reshapes below are what new systems should look like once reflection is available.

## Reshape 1 — ECS Systems as Declarative Annotated Structs

**Today:** scheduler builds the read/write graph from runtime `ctx.read<T>()` / `ctx.write<T>()` calls. Dependencies aren't known until after a system has executed.

**Reflected shape:**

```cpp
struct physics_integrate {
    [[= writes]]
    query<motion_component> motion;

    [[= reads]]
    query<collision_component> colliders;

    [[= reads]]
    resource<time_delta> dt;

    auto run() -> void;
};

gse::scheduler::register_system<physics_integrate>();
```

Scheduler reflects the system's fields at registration time and builds the full read/write graph **statically**. The `std::vector<std::type_index> reads/writes` in `scheduled_work` becomes a compile-time list.

**Unlocked capabilities:**

- Write-write conflicts between systems become compile errors, not runtime races.
- Parallel batch scheduling can be computed at program start from static dependency data, not discovered per-frame.
- Systems become hot-swappable / hot-reloadable because their contract is entirely in the struct definition.
- A "dry run" graph visualization (which system writes what, in what order) falls out by walking the registered system types — no separate tooling.

## Reshape 2 — Render Graph Passes as Declarative Annotated Structs

**Today** (`RenderGraph.cppm:64-96`): pass builder takes a chain of `sampled(img, stage)`, `storage_read(buf, stage)`, `attachment(img, stage)` helper calls. Pass ordering and barrier placement derive from these manually-wired usage lists.

**Reflected shape:**

```cpp
struct forward_pass {
    [[= sampled<fragment>]]
    const image& shadow_map;

    [[= storage_read<fragment>]]
    const gpu_buffer& materials;

    [[= color_attachment]]
    image& color_target;

    [[= depth_attachment<read>]]
    image& depth_from_prepass;

    auto record(recording_context& cmd) -> void;
};
```

Barriers, layout transitions, and pass ordering derive from the annotated struct fields. No manual `.reads(...).writes(...)` chains. The `resource_usage` helper functions at `RenderGraph.cppm:64-95` evaporate because their input is now a reflected struct, not a builder call sequence.

**Unlocked capabilities:**

- Impossible to forget a barrier — a pass that reads without an annotation simply can't compile against the reflected builder.
- Render graph visualization (DOT / Graphviz output showing pass DAG, resource lifetime, barrier placement) falls out automatically by reflecting over registered passes.
- Pass reordering / culling becomes an optimization the engine can apply, because the full resource usage is a compile-time property of each pass type.

This is structurally how Frostbite FrameGraph and the Unreal Nanite path look. They're hand-written there because no reflection; with P2996 you get it as a property of how passes are declared.

## Reshape 3 — Per-Field Replication Rules

**Today:** `motion_component_net` is a layout-compatible subset of `motion_component_data`. The `_net` struct can only express "include or exclude." Delta encoding, interpolation, priority, rollback are all hand-written in `Replication.cppm`.

**Reflected shape:**

```cpp
struct motion_component_data {
    [[= replicated, interpolate]]
    vec3<current_position> current_position;

    [[= replicated, delta_encoded]]
    vec3<velocity> current_velocity;

    [[= replicated, priority<high>]]
    mass mass;

    [[= replicated, rewindable]]
    quat orientation;

    [[= replicated, interest<radius<50>>]]
    vec3<angular_velocity> angular_velocity;

    float restitution;             // unannotated → server-local, never sent
    vec3<impulse> pending_impulse; // same
};
```

One struct. Each field independently declares its replication rules: wire inclusion, delta/full encoding, receive-side interpolation, client-side prediction/rollback participation, bandwidth priority, interest management radius.

**Unlocked capabilities:**

- The entire concept of a "_net struct" disappears. `motion_component_net`, `render_component_net`, `player_controller_net` — all become unnecessary.
- Replication behaviors are far richer than the current inclusion model: interpolation, rollback, priority, and interest management all become field-level annotations rather than struct-level write-once code.
- Adding a new replicated field is one-line: annotate it. No sync edit to a network-projection struct, no change to the replication system itself.
- Network-specific code becomes a single reflected walker in `Replication.cppm` that handles every component; adding a new replicated component adds zero code to that module.

## Reshape 4 — One Command / Dispatch Mechanism

**Today:** five separate implementations of the same pattern —

- Audio requests (`Audio.cppm` channels, six `for (auto& req : ctx.read_channel<Req>())` loops)
- Network messages (`Message.cppm` `message_switch` + per-type ADL encode/decode)
- Action events (`ActionsApi.cppm` channel registration)
- Deferred ECS ops (`update_context::defer_add` / `defer_remove`)
- No undo-redo today, but it'd be a sixth

Each is a "typed struct → queue → dispatcher" mechanism, implemented differently.

**Reflected shape:**

```cpp
[[= command<"audio">]]
struct play_sound { clip_id clip; float volume; };

[[= command<"network">]]
struct ping { std::uint32_t seq; };

[[= command<"ecs">]]
struct spawn_entity { id parent; scene_ref tmpl; };

[[= command<"undo">]]
struct transform_set { id target; vec3f old_pos; };

gse::command_queue<"audio">().enqueue(play_sound{...});
gse::command_queue<"network">().dispatch(stream, handlers);
```

One templated `command_queue<Domain>` and one reflected dispatcher. Serialization (via A3), dispatch (via reflected type-list scan), and logging are all shared.

**Unlocked capabilities:**

- Record / replay becomes uniform across every domain. Commands are first-class reflectable types, so replay infrastructure is written once.
- Deterministic networking becomes straightforward: the wire format is reflected from the struct, and commands can be logged server-side for drift detection.
- Editor command history (for undo-redo) becomes an automatic side-effect of any `[[= command<"editor">]]` struct.
- `message_switch`'s 11 overloads, the audio-request loop fusion, and the future undo system collapse into the same 15-line reflected dispatcher.

## Reshape 5 — Editor as a Free Service Over Any Annotated Type

**Today:** `SaveSystem::property_base` + `register_property` + `void*` + `type_index` + `std::function` factories — roughly 300 lines of runtime machinery whose entire purpose is "learn at runtime what fields this struct has so the editor can render them."

**Reflected shape:**

```cpp
struct graphics_settings {
    [[= inspector, slider<1, 4>]]
    std::uint32_t shadow_quality = 2;

    [[= inspector, dropdown<aa_mode>]]
    aa_mode anti_aliasing = aa_mode::taa;

    [[= inspector]]
    bool bloom = true;

    [[= inspector, range<0.f, 2.f, 0.05f>]]
    float exposure = 1.f;
};

render_editor_panel<graphics_settings>(settings);   // generic, reflected
```

Any annotated struct gets a property inspector, TOML serialization, auto-diff / auto-undo, and dirty tracking. No per-struct registration, no `property_base` hierarchy.

**Unlocked capabilities:**

- The editor's inspector, debug overlays, and profiler UI all become the same code path — "walk the annotated struct, render each field with an appropriate widget."
- Live tweaking of any game struct in a debug build becomes trivial: add `[[= debug_inspect]]` to the struct, and it gets a runtime inspector at a known hotkey.
- Scene graph panels, per-entity component inspectors, material editors, audio bank editors — all instances of the same reflection walker. The editor's total size shrinks rather than growing with each new inspectable type.

## Reshape 6 — Input as Typed Struct Fields

**Today:** actions looked up by string tags in `InputApi.cppm`; the tag-based system tries to be compile-time but stops short because strings can't be reflected.

**Reflected shape:**

```cpp
struct player_controls {
    [[= bind<"W", "Up", gamepad_left_stick_up>]]
    button forward;

    [[= bind<"Space", gamepad_a>]]
    button jump;

    [[= bind<mouse_delta>]]
    vec2f look;

    [[= bind<"LShift">, repeat_hold]]
    button sprint;

    [[= bind<gamepad_left_trigger>, analog_threshold<0.1f>]]
    float brake;
};

if (controls.jump.pressed) player.do_jump();
```

Input system maintains one `player_controls` per player; each frame, reflection walks its fields and pulls the bound input state into each member. Gameplay code does direct field access.

**Unlocked capabilities:**

- Zero string lookups in the input hot path. Zero hashmap hits per action query.
- Rebinding becomes data-driven cleanly: annotations supply defaults, runtime config can override per field.
- Input recording / replay is automatic: `player_controls` is a reflectable struct, so it serializes via A3.
- Multiple input schemas (player vs editor vs debug camera) coexist as distinct annotated struct types. No shared global action namespace.

## Reshape 7 — Scenes / Prefabs as Annotated Component Streams

**Today:** scene loading is bespoke per scene (see `Game/Game/Source/World1/MainTestScene.cppm` et al.) — each scene's setup is hand-written entity construction.

**Reflected shape:** a scene file is a list of entities; each entity lists its components; each component deserializes via the reflected Archive (A3). Loader reflects over all `[[= component]]`-annotated types to dispatch parsing:

```
entity "player_01" {
    transform          { position = [0, 5, 0], rotation = identity }
    motion_component   { mass = 70kg, restitution = 0.3 }
    player_controls    { }
    render_component   { mesh = "player.glb", material = "standard" }
}
```

**Unlocked capabilities:**

- Scene loader is one piece of code; every current and future component type works without change.
- Prefabs, templates, and save games are the same mechanism.
- Scene diffs become natural (textual diff of annotated component streams).
- Hot-reload of a scene while the game runs is feasible because the data model is reflectable.

## Reshape 8 — GUI: Widget Primitives + Reflected Composition

Your GUI is an especially clean fit because it already has all the **widget primitives** (`toggle`, `slider`, `input`, `dropdown`, `tree`, `section`, `scroll`, etc.) but is missing the **composition layer** that turns a struct into a sequence of widget calls. Reflection *is* that composition layer.

### 8a — `draw_inspector<T>` as a universal widget

**Today:** every editable UI — `SettingsPanel` (`SettingsPanel.cppm`, ~500 lines hand-walking `save::state`), entity inspectors, debug overlays — is bespoke. Each one individually calls `b.draw<toggle>({...})` or `b.draw<slider>({...})` for every field.

**Reflected shape:**

```cpp
template <typename T>
auto draw_inspector(builder& b, std::string_view name, T& value) -> void {
    if      constexpr (std::same_as<T, bool>)    b.draw<toggle>({name, value});
    else if constexpr (std::is_enum_v<T>)        b.draw<enum_dropdown<T>>({name, value});
    else if constexpr (is_arithmetic<T>)         b.draw<input<T>>({name, value});
    else if constexpr (internal::is_quantity<T>) b.draw<quantity_slider<T>>({name, value});
    else if constexpr (std::is_class_v<T>) {
        b.draw<section>({name});
        template for (constexpr auto m : std::meta::nonstatic_data_members_of(^^T))
            draw_inspector(b, std::meta::identifier_of(m), value.[:m:]);
    }
}
```

Any annotated struct becomes live-editable with one call. `motion_component`, `graphics_settings`, random debug state — no per-type glue.

### 8b — Annotation-driven widget selection

The generic picker above is a starting point; annotations give per-field control:

```cpp
struct player_settings {
    [[= slider<0.f, 10.f>]]
    float walk_speed = 5.f;

    [[= slider<0.f, 10.f, step<0.5f>>]]
    float run_speed = 8.f;

    [[= dropdown]]
    movement_mode mode;

    [[= toggle]]
    bool allow_sprint;

    [[= input<masked>]]
    std::string password;

    [[= hidden]]
    id internal_id;

    [[= section("Advanced"), collapsed]]
    struct { float friction; float air_control; } advanced;
};
```

Widget selection, range/step constraints, visibility, collapsible sections — all from annotations. **This is exactly what `SaveSystem::property_base` is trying to express at runtime** (`SaveSystem.cppm:91-150+`); annotations express it at compile time and the runtime hierarchy dissolves.

### 8c — Slider overloads collapse

`Slider.cppm:14-57` has four `draw::slider` overloads (arithmetic, quantity, `vec<T,N>`, `vec<quantity,N>`) that all marshal into arrays and conditionally append a unit suffix. Combined with S1 (reflected quantity `units` tuple), one template handles all four:

```cpp
template <typename T>
auto slider(draw_context& ctx, std::string_view name, T& v, T min, T max, ...) -> void {
    constexpr auto n = gse::meta::component_count<T>();       // 1 or N
    constexpr auto suffix = gse::meta::unit_suffix<T>();      // "" or " (m/s)"
    // one body
}
```

The `if constexpr (internal::is_quantity<T>)` branches inside `slider_box` (lines 193-201, 219-224) also evaporate once B4's reflected `std::formatter` handles quantities directly.

### 8d — Widget-wrapper shells auto-generated

Every widget repeats this shape (`Slider.cppm:60-86`, `Input.cppm:23-31`, etc.):

```cpp
struct toggle {
    using result = void;
    struct params { std::string_view name; bool& value; };
    static auto draw(draw_context& ctx, params p, id& hot, id& active, id& focus) -> void {
        draw::toggle(ctx, std::string(p.name), p.value, hot, focus);
    }
};
```

A reflected helper generates the `struct widget_name` shell from the freestanding `draw::widget_name` function signature. Adding a new widget becomes a single function definition.

### 8e — Menus as annotated structs

**Today** (`Builder.cppm:57-61`): `menu_content{ .menu = "main", .priority = 0, .build = [](builder& b) { /* manual calls */ } }`.

**Reflected:**

```cpp
[[= menu("main", priority<0>)]]
struct main_menu {
    [[= button, on_click<&game::new_game>]]
    std::monostate new_game;

    [[= button, on_click<&game::load_game>]]
    std::monostate load_game;

    [[= separator]]
    std::monostate _sep;

    [[= submenu<settings_menu>]]
    std::monostate settings;

    [[= button, on_click<&game::quit>]]
    std::monostate quit;
};
```

Menu system scans all `[[= menu]]`-annotated structs at startup and assembles the tree. Priorities, submenus, and shortcut keys come from annotations.

### 8f — `SettingsPanel` evaporates

`SettingsPanel.cppm` is ~500 lines hand-walking `save::state` through `property_base` virtuals, with a `pending_value` variant, `pending_change` list, manual `find_pending`, restart-required popup logic. With reflection + Reshape 5:

```cpp
auto update(state& s, context& ctx, ui_rect rect, bool open) -> bool {
    return draw_inspector(ctx.builder, "Settings", g_settings);
}
```

The rest is gone. Pending/dirty tracking, TOML write-back, restart-required handling — all annotation-driven properties of the settings struct itself (`[[= requires_restart]]`, `[[= commit_on_apply]]`).

### 8g — Layout via annotations

```cpp
struct entity_inspector_view {
    [[= header]]
    std::string name;

    [[= section("Transform")]]
    transform_component transform;

    [[= section("Physics"), collapsed]]
    motion_component motion;

    [[= section("Rendering")]]
    render_component render;

    [[= section("Debug"), debug_only]]
    debug_flags flags;
};
```

Section headers, collapse state, debug-only visibility come from annotations instead of manual `section()` / `header()` calls.

### 8h — Free debug inspector for every engine type

Because `draw_inspector<T>` works on **any** struct, a single debug overlay bound to a hotkey can render any game struct by name: `motion_component_data` on the selected entity, `render_graph` pass state, audio voice allocations, active tasks in the scheduler. All live-viewable with zero per-type UI code. The ergonomic leap is from "coding a UI" to "having a UI for free."

### Summary

The GUI today is a **widget library with hand-written composition**. After reflection, it is a widget library with **reflection-driven composition**. The widgets themselves change little; what changes is the call site — "hand the struct to the inspector" replaces "call one widget per field." Opportunities C2, B4, S1, and Reshape 5 all converge here, because a property inspector is fundamentally "walk fields, format each, make it editable."

---

## Reshape 9 — Resource Pipeline: Close the Loop

The resource pipeline is already well-designed: trait-driven (`asset_compiler<R>`), concept-guarded (`is_resource<R, C>`), per-type loaders, clean separation of compile from load. It is not the train wreck that `SaveSystem::property_base` is. The reflection wins here are about **closing the loop** — unifying the four places a resource type is defined today into one annotated class — and about **eliminating the type-erasure layer** the pipeline uses to hold heterogeneous compilers.

### The core problem today

One resource type is currently declared in four places:

1. The resource class itself (`class texture { ... load() ... unload() ... }`).
2. The `asset_compiler<texture>` trait specialization (in a separate file).
3. The `resource::loader<texture, render_context>` instantiation at startup.
4. The `pipeline.register_type<texture, render_context>(&tex_loader)` call.

Miss any one and the asset silently never loads. No error. That's a recurring bug class waiting to happen.

### 9a — Auto-registration via annotated resource classes

**Reflected shape:**

```cpp
[[= resource<
    .source_ext = {".png", ".jpg", ".exr"},
    .baked_ext = ".gtex",
    .source_dir = "Textures",
    .baked_dir = "Textures",
    .bake = &texture::bake_from_source,
    .dependencies = &texture::list_dependencies
>]]
class texture {
    explicit texture(const std::filesystem::path&);
    auto load(render_context&) -> void;
    auto unload() -> void;

    static auto bake_from_source(const path& src, const path& dst) -> bool;
    static auto list_dependencies(const path& src) -> std::vector<path>;
};
```

At engine bootstrap, one `consteval` scan over every `[[= resource]]`-annotated type registers its loader and its compiler automatically. **New resource = one annotated class.** The four-places-out-of-sync bug class is structurally eliminated. The `asset_compiler<R>` trait specialization (`AssetCompiler.cppm:6-33`) evaporates — its contents move into the annotation.

### 9b — `compiler_entry` std::function chain collapses

**Today** (`AssetPipeline.cppm:84-95, 136-163`): each registered type builds a `compiler_entry` with **seven** `std::function`-typed fields (`compile_fn`, `needs_recompile_fn`, `dependencies_fn`, `reload_fn`, `queue_fn`, plus captured closures). Each wraps an `asset_compiler<R>::some_static()` in a lambda. Pure type-erasure bridging from templated traits to a heterogeneous container.

**Reflected shape:** since the complete set of resource types is known at compile time from annotations, the pipeline holds a `constexpr` dispatch table indexed by reflected ordinal — not a `std::vector<compiler_entry>`. The seven `std::function` fields evaporate. Calls become direct. `find_compiler(source)` becomes a compile-time switch on file extension rather than a linear scan through lambdas. The `std::type_index` field and the `typeid(R)` compare in `asset_pipeline::compile<Resource>()` (line 238) go with them.

### 9c — Bake and load bodies use reflected Archive (A3)

Every compiler today writes bake output via `binary_writer`, hand-listing fields. `shader_layout::load` (`ShaderLayout.cppm:79-126`) reads back via `ar & m_name; ar & m_sets;` with explicit `serialize(ar, shader_layout_binding&)` ADL hooks (lines 62-70). Every compiler has its own version of this.

With A3's reflected archive, each body collapses to **one line per direction**:

```cpp
auto shader_layout::load(const vk::raii::Device& device) -> void {
    std::ifstream in(m_path, std::ios::binary);
    binary_reader ar(in, magic, version, m_path.string());
    if (!ar.valid()) return;
    ar & *this;                   // reflected — walks all non-trivial fields
    build_vk_layouts(device);     // GPU-specific work stays explicit
}
```

Every bake/load pipeline shrinks proportionally. Schema evolution becomes "change the struct"; the magic/version pair stays as the only manual bump.

### 9d — Resource-to-resource dependencies from annotated fields

**Today** `asset_compiler<R>::dependencies(src)` returns `std::vector<path>` — hand-parsed per compiler (Slang scanning `#include`, glTF parsing texture references). That covers *build-time* deps (source-file parsing) but doesn't express **runtime resource references**. A `skinned_model` depends on a `skeleton` and a set of `material`s; those are currently handled by the resource's `load()` method fetching other handles.

**Reflected shape:**

```cpp
[[= resource]]
class skinned_model {
    [[= resource_ref]]
    handle<skeleton> rig;

    [[= resource_ref]]
    std::vector<handle<material>> materials;

    [[= resource_ref<optional>]]
    handle<animation_clip> default_clip;

    // ...
};
```

The pipeline reflects `resource_ref` annotations to build a dependency graph **before** `load()` runs. Correct load order is enforced. Missing-dependency failures become loud, not silent. Bonus: the asset browser gets a free "find references to this asset" feature by walking the reflected reference graph in reverse.

### 9e — `loader_base` virtuals evaporate

**Today** (`ResourceLoader.cppm:53-63`): `loader_base` has 7 pure virtuals (`flush`, `update_state`, `mark_for_gpu_finalization`, `finalize_state`, `queue_reload_by_path`, `queue_by_path`, `finalize_reloads`). The base class exists solely so `asset_pipeline` can store loaders in a homogeneous collection via `compiler_entry::reload_fn` capturing a `loader<R,C>*`.

Once registration is a compile-time reflection scan (9a), the pipeline doesn't need homogeneous storage — it holds a `std::tuple<loader<font,C>, loader<texture,C>, loader<shader,C>, ...>` indexed by reflected ordinal. The entire `loader_base` abstract class can be deleted. All virtual calls become direct calls; the fetch path inlines cleanly.

### What stays the same

Reflection does not need to touch these, and forcing it to would make them worse:

- **`resource_slot<R>` + `handle<R>`** — already clean, per-type, concrete.
- **`gpu_work_token` / `reload_token`** — RAII state-machine wrappers; reflection does not improve them.
- **`n_buffer<...> resource` in `resource_slot`** — the lock-free double-buffering is orthogonal to reflection.
- **File watcher** (`asset_pipeline::enable_hot_reload`) — wire-up stays identical; `[[= hot_reloadable]]` as a per-resource opt-out is cosmetic.

### The shape of the win

The resource pipeline is a case where reflection is about **closure**, not destruction. The system is already correct; reflection makes it impossible to use incorrectly. Rough impact:

- **Per resource type:** ~30-50 lines of glue deleted (trait specialization, registration call, manual `ar & field_1 & field_2 ...` in load/unload).
- **Per bake pipeline:** one-line reflected serialization replaces ~20 lines of hand-written `binary_writer` field walks.
- **Core:** `loader_base` deleted (~30 lines), `compiler_entry::*_fn` std::function fields deleted (~15 lines + runtime cost), `type_index` dispatch deleted.
- **Bugs prevented:** "I added a resource type and forgot to register it" becomes structurally impossible.

Ties into existing opportunities: A3 (reflected Archive) is the enabling primitive for 9c; the same annotation-scan machinery that powers Reshape 1 (ECS), Reshape 2 (render graph), and Reshape 8 (GUI) powers 9a. This is the same shape reshape appearing in yet another subsystem — evidence that the core primitive really is universal.

---

## The Unifying Primitive

All nine reshapes reduce to the same compile-time operation: **walk the annotated fields of a struct (or the annotated types in a namespace), classify each by annotation, and plug it into a domain-specific handler.** That primitive is ~50 lines of shared infrastructure in `gse::meta`:

```cpp
namespace gse::meta {
    template <auto Tag, typename T>
    consteval auto annotated_members() -> std::vector<std::meta::info>;

    template <auto Tag, typename T, typename Visit>
    constexpr auto for_each_annotated(T& value, Visit&& v) -> void;

    template <typename T>
    consteval auto all_members() -> std::vector<std::meta::info>;

    template <typename T, typename Annotation>
    consteval auto has_annotation() -> bool;
}
```

Each reshape is then ~100 lines of domain-specific code on top of this primitive.

**The reason these systems are complex today is that each one reinvents this primitive at runtime in a domain-specific way.** Once the compile-time primitive exists, the runtime reinventions evaporate and the systems collapse to their essential domain logic.

## Migration Posture

Part II is not a migration plan — existing systems already work. The right posture is:

1. **New systems default to the reshape.** Whenever you're writing a genuinely new subsystem (a future editor panel, a new replicated component kind, a new command domain), write it in the reflected shape from day one. The primitives from Part I will already be in place.
2. **Reshape old systems only when they're being touched anyway.** When a tactical opportunity from Part I lands in a system's neighborhood, consider whether the surrounding system should adopt the reshape at the same time. Example: when A5 (push-constant binding) goes in, ask whether the render graph passes that *use* those push constants should adopt Reshape 2 concurrently.
3. **Don't reshape stable systems just to reshape them.** The ECS scheduler, render graph, and input system all work. Reshape them when a motivating change arrives — a new feature, a perf ceiling, a bug pattern — not as pure cleanup. Tactical wins from Part I are the pure-cleanup bucket; architectural reshapes deserve a real reason.

---

## Non-Opportunities (Explicitly Considered, Rejected)

Flagged during exploration but not worth the reflection effort:

- **Math vector / matrix / quaternion ops** — `Math/Vector/`, `Math/Matrix/`, `Math/Quaternion/`. Template generics already handle the component-wise operations cleanly; reflection would add complexity without shrinking code.
- **Key code mapping** — `Platform/GLFW/Keys.cppm` uses `= GLFW_KEY_X` trick for identity mapping; no dispatch needed. String conversion for key names is covered by C8.
- **Dimension arithmetic in `Dimension.cppm`** — `std::ratio`-based template metaprogramming is already the right tool.
- **Primitives (`Circle.cppm`, `Rectangle.cppm`, `Segment.cppm`)** — small, focused types with no visible repetition.

## References

- P2996 working paper: *Reflection for C++26*
- Bloomberg clang-p2996 fork: `https://github.com/bloomberg/clang-p2996`
- `Engine/Engine/Import/Log.cppm` — existing assert/format surface that the reflected `std::formatter` must compose with
- `docs/STYLEGUIDE.md` — naming and style conventions to preserve across refactors

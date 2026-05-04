# Content pipeline refactor — scope

## Motivation

The engine's content pipeline today spans two parallel sets of machinery —
the asset pipeline (textures, fonts, models, shader artifacts) and the
shader-source pipeline (Slang authored by hand, descriptor layouts hand-kept
in sync with C++ on the binding side). Both have the same root problem:
information that exists once in C++ is restated by hand somewhere else, and
the framework that bridges them does so by type erasure (asset side) or by
hand-mirrored declarations (shader side).

C++26 reflection collapses both. The asset side becomes a closed type pack
with a single `[[= asset_format{...}]]` annotation per baked struct; the
shader side becomes C++-authored structs and resource bindings codegen-ed
into Slang. Two refactors, separable execution, shared primitives.

**Asset side.** The asset pipeline currently spans five modules and ~1,600
LOC for what is, in the abstract, a small job: take a source file, turn it
into a baked binary, load the baked binary at runtime, reload it when
source changes. The size comes from type erasure.
[`asset_pipeline`](../Engine/Engine/Source/Assets/AssetPipeline.cppm) holds
a `vector<compiler_entry>` where each entry stores six `std::function`
callbacks.
[`asset_registry`](../Engine/Engine/Source/Assets/AssetRegistry.cppm) holds
an `unordered_map<id, unique_ptr<loader_base>>`.
[`loader_base`/`loader_t<T>`](../Engine/Engine/Source/Assets/ResourceLoader.cppm)
is a four-method virtual base whose only job is to let the registry call
methods without knowing `T`. With C++26 reflection and a compile-time type
pack, every layer of erasure becomes a `template for` over `Ts...`.

**Shader side.** Today every shader has two coupled declarations. The
Slang source declares push constants, descriptor bindings, shared structs,
and entry points. The C++ renderer binds resources and writes push
constants by name. The two are kept in sync by hand — adding a field means
editing both sides, and the `pc.set("x", v)` path does a hash lookup per
member at runtime to validate what's already known at compile time.
After this refactor, **C++ structs are the single source of truth for
every shader-side declaration a renderer cares about, and entry points
are composed from C++ by selecting library functions.** The hand-authored
part of `.slang` files collapses to pure function libraries — functions
with typed parameters, no entry points, no bindings, no structs.

The two halves are independent in execution. Each can land before the
other. They intersect at one boundary: shaders, today an asset type, exit
the asset pipeline entirely when shader phase 4 (S4) ships.

## Scope

### Asset pipeline — in scope

- `[[= asset_format{...}]]` annotation on baked struct types carries every
  constant the framework needs.
- `bake()` ADL hook as the only per-type customization. Replaces
  `compile_one`.
- Reflected serialization for `compile_one` — the
  [Archive reflection path](../Engine/Engine/Source/Containers/Archive.cppm)
  already does this; we feed it.
- Compile-time `asset_system<Ctx, Ts...>` replacing
  registry + pipeline + virtual loader hierarchy.
- Compositional type pack — each subsystem exports `asset_types`; the game
  flattens at the `asset_system` instantiation site.
- Templated tokens replacing `loader_base*`-bound `gpu_work_token` and
  `reload_token`.

### Shader pipeline — in scope

- **Struct codegen.** Shared GPU structs authored once in C++, emitted into
  generated `.slang` headers.
- **Resource codegen.** Push constants, UBOs, SSBOs, samplers, RT
  acceleration structures — all declared via annotated C++ with a reflected
  binding map.
- **Entry-point forwarders.** Compute / vertex / fragment / mesh /
  amplification `main` functions generated as thin shims that invoke a
  hand-authored library function.
- **Permutation control.** Generic instantiation and specialization
  constants selected by C++ composition, not by `#ifdef` forests.
- **Coexistence.** Legacy hand-authored shaders keep working during
  rollout.

### Out of scope (both halves)

- Replacing the physical baked-file format (`.gtx`, `.gfont`, `.gmdl`,
  `.gskel` keep their current bytes; magic+version constants survive,
  relocated to annotations).
- The `bake()` body for any asset type. Image decode, MSDF generation,
  glTF/FBX import — all unchanged.
- Hot reload semantics on the asset side. The watcher still polls source
  and baked directories; reloads still bump a per-resource version.
- Replacing Slang the language. Function libraries stay hand-authored in
  Slang.
- Replacing the Slang compiler or `.glayout` binary format. The compile
  pipeline still ends at Slang → SPIR-V.
- Shader graph / node editor UI.
- Runtime hot-recompilation of C++-side structs. Those still require a C++
  rebuild. Slang-side changes (library function bodies, new permutations)
  remain hot-reloadable.
- Replacing the render-graph API. Pipelines still get created with
  pipeline state, push-constant ranges, layouts.
- Plugin/DLL injection of new asset types at runtime. The type pack is a
  closed set, sealed at the `asset_system` instantiation site.

### What stays the same

Asset side: everything observable above the asset-system façade —
`handle<T>`, version-stamped slots, the `state` enum, async load, GPU
finalization tokens, hot reload bumping versions, the
[`gse::asset::context`](../Engine/Engine/Source/Assets/AssetRegistry.cppm)
virtual interface, the
[`is_resource<R, C>` and `resource_context<C>`](../Engine/Engine/Source/Assets/ResourceLoader.cppm)
concepts, each `T::load(ctx)` / `T::unload()` body.

Shader side: the render-graph API, the `VkPipeline` cache shape,
`ShaderRegistry::cache()` semantics during rollout. Migrations happen
shader-by-shader; both old and new paths share the same pipeline-creation
machinery.

## Reflection primitives used throughout

Both refactors lean on the same C++26 reflection feature set, available
via the Bloomberg clang-p2996 fork (already enabled in this build).

- `^^T` / `^^expr` — reflect on a type, namespace, member, enumerator, or
  constant.
- `[: r :]` — splicer, paste a reflected entity back into a program.
- `std::meta::nonstatic_data_members_of`, `enumerators_of`, `members_of`,
  `parameters_of`, `annotations_of` — introspect entities.
- `std::meta::identifier_of`, `display_string_of`, `type_of`, `dealias`,
  `template_arguments_of`, `parent_of` — extract names and traits.
- `std::meta::define_aggregate` + `consteval {}` injection blocks —
  synthesize types/declarations at compile time.
- `template for (constexpr auto m : ...)` — expansion statement over a
  `define_static_array(reflection-range)`.
- Value annotations: `[[= some_struct{ .field = value }]]` — carry
  compile-time metadata on declarations.

Established patterns already in the codebase:

- [`Archive.cppm:225-238`](../Engine/Engine/Source/Containers/Archive.cppm) —
  reflected struct serialization with `[[archive_skip]]` honoring.
- [`Annotations.cppm:51`](../Engine/Engine/Source/Meta/Annotations.cppm) —
  `has_annotation<Tag>` consteval helper.
- [`Enum.cppm:25-43`](../Engine/Engine/Source/Meta/Enum.cppm) —
  bidirectional enum↔string conversion.
- [`ID.cppm:641+`](../Engine/Engine/Source/Core/ID.cppm) — recursive type
  name builder using `dealias` + `display_string_of` +
  `template_arguments_of`.

The asset and shader refactors model new patterns after these.

---

# Asset pipeline (track A)

Five phases, A1–A5. Each leaves the build green and the engine runnable.
Phases A1–A3 yield real wins on their own and are an acceptable stopping
point if A4–A5 (the collapse) get deferred.

| Phase | What it produces | Depends on |
|-------|------------------|------------|
| A1 | `asset_format` annotation, `format_of<T>`, `raw_blob_owned`, `load_baked<T>` | Existing reflected Archive |
| A2 | Migrate `texture`, `font` to baked-struct + `bake()` | A1 |
| A3 | Migrate `model` (and interim `shader`) to baked-struct + `bake()` | A2 |
| A4 | `type_pack`, `gse::assets::append`, per-module exports | A3 |
| A5 | `asset_system<Ctx, Ts...>` replaces registry + pipeline | A4 |

## A1 — annotation primitives

Goal: introduce the contract types and reflection helpers that every
subsequent phase uses. No existing per-type code changes yet.

### A1a. `asset_format`

`Engine/Engine/Source/Assets/AssetFormat.cppm`:

```cpp
export module gse.assets:asset_format;

import std;

export namespace gse {

    struct asset_format {
        std::span<const std::string_view> source_exts;
        std::string_view source_dir;
        std::string_view baked_ext;
        std::string_view baked_dir;
        std::uint32_t magic;
        std::uint32_t version;
        bool meta_sidecar = false;
    };

    template <typename T>
    consteval auto format_of() -> asset_format {
        template for (constexpr auto ann : std::define_static_array(std::meta::annotations_of(^^T))) {
            if constexpr (std::meta::type_of(ann) == ^^asset_format) {
                return [:ann:];
            }
        }
        static_assert(false, "type missing [[= asset_format{...}]] annotation");
    }

    template <typename T>
    concept has_asset_format = requires { format_of<T>(); };
}
```

`source_exts` is `std::span<const std::string_view>` — fall back to
`static_vector<std::string_view, 8>` if compile errors surface.

### A1b. `raw_blob_owned`

[Archive.cppm:14](../Engine/Engine/Source/Containers/Archive.cppm) defines
`raw_blob<T>` as a *reference* to a `std::vector<T>&`. Baked-struct fields
need a self-owning sibling so the struct can carry the data instead of
borrowing. Lives in the same module as Archive (sibling export):

```cpp
export namespace gse {

    template <typename T>
    struct raw_blob_owned {
        std::vector<T> storage;
    };

    template <typename T>
    auto serialize(binary_writer& ar, raw_blob_owned<T>& v) -> void {
        ar & raw_blob<T>{ v.storage };
    }

    template <typename T>
    auto serialize(binary_reader& ar, raw_blob_owned<T>& v) -> void {
        ar & raw_blob<T>{ v.storage };
    }
}
```

The user-`serialize` hook at
[Archive.cppm:227](../Engine/Engine/Source/Containers/Archive.cppm) picks
these up automatically — no other Archive change needed.

### A1c. `load_baked<T>`

A symmetric reader so call sites
([Texture.cpp:34](../Engine/Engine/Source/Graphics/2D/Texture.cpp),
[Font.cpp:73](../Engine/Engine/Source/Graphics/2D/Font.cpp),
[Model.cppm:82](../Engine/Engine/Source/Graphics/3D/Vulkan/Model.cppm))
stop inlining magic/version constants:

```cpp
export namespace gse {

    template <typename T> requires has_asset_format<T>
    auto load_baked(
        const std::filesystem::path& path,
        T& out
    ) -> bool {
        std::ifstream in(path, std::ios::binary);
        if (!in.is_open()) {
            return false;
        }

        constexpr auto fmt = format_of<T>();
        binary_reader ar(in, fmt.magic, fmt.version, path.string());
        if (!ar.valid()) {
            return false;
        }

        ar & out;
        return true;
    }
}
```

After A2, `Texture::load`'s body becomes ~5 lines of struct→runtime
translation, with no file I/O or magic constants visible.

**Exit criterion.** `asset_format`, `format_of`, `raw_blob_owned`,
`load_baked` exported and unit-tested. No existing compiler is migrated
yet. Build is green; rendering is unchanged.

## A2 — migrate `texture` and `font`

### A2a. `texture_baked`

A new struct adjacent to `texture`:

```cpp
struct [[= asset_format{
    .source_exts  = std::array{ ".png"sv, ".jpg"sv, ".jpeg"sv, ".tga"sv, ".bmp"sv },
    .source_dir   = "",
    .baked_ext    = ".gtx",
    .baked_dir    = "Textures",
    .magic        = 0x47544558,
    .version      = 1,
    .meta_sidecar = true
}]] texture_baked {
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t channels;
    texture::profile profile;
    raw_blob_owned<std::byte> pixels;
};

auto bake(const std::filesystem::path& src, texture_baked& out) -> bool;
```

`bake()` body is the existing `compile_one` minus the file I/O scaffolding.
The `.meta` sidecar parsing
([TextureCompiler.cppm:48-63](../Engine/Engine/Source/Graphics/2D/TextureCompiler.cppm))
moves into a private helper `read_texture_profile()` and is invoked from
`bake()`.

`Texture::load`'s body
([Texture.cpp:34](../Engine/Engine/Source/Graphics/2D/Texture.cpp)) becomes:

```cpp
texture_baked baked{};
if (!load_baked(m_image_data.path, baked)) {
    return;
}
m_image_data.size     = vec2u{ baked.width, baked.height };
m_image_data.channels = baked.channels;
m_profile             = baked.profile;
m_image_data.pixels   = std::move(baked.pixels.storage);
```

Magic + version exist exactly once (in the annotation) instead of three
times.

### A2b. `font_baked`

Same pattern, larger payload:

```cpp
struct [[= asset_format{
    .source_exts = std::array{ ".ttf"sv, ".otf"sv },
    .source_dir  = "Fonts",
    .baked_ext   = ".gfont",
    .baked_dir   = "Fonts",
    .magic       = 0x47464E54,
    .version     = 2
}]] font_baked {
    std::string source_path_relative;
    float ascender;
    float descender;
    std::uint32_t atlas_width;
    std::uint32_t atlas_height;
    std::uint32_t channels;
    raw_blob_owned<std::byte> rgba;
    std::unordered_map<char, glyph> glyphs;
};
```

The 169-line `compile_one` at
[FontCompiler.cppm:39](../Engine/Engine/Source/Graphics/2D/FontCompiler.cppm)
becomes a `bake()` whose tail goes from 12 lines of `binary_writer`
chaining to 9 lines of struct field assignment.

### A2c. Compatibility layer

`asset_compiler<T>` keeps existing per-type specializations during
migration. We add a generic primary template that activates only when
`has_asset_format<T>`:

```cpp
template <typename T> requires has_asset_format<T>
struct asset_compiler {
    static constexpr auto fmt = format_of<T>();

    static auto source_extensions() -> std::vector<std::string> {
        return { fmt.source_exts.begin(), fmt.source_exts.end() };
    }
    static auto baked_extension() -> std::string { return std::string{ fmt.baked_ext }; }
    static auto source_directory() -> std::string { return std::string{ fmt.source_dir }; }
    static auto baked_directory() -> std::string { return std::string{ fmt.baked_dir }; }

    static auto compile_one(
        const std::filesystem::path& src,
        const std::filesystem::path& dst
    ) -> bool {
        T baked{};
        if (!gse::bake(src, baked)) {
            return false;
        }
        std::filesystem::create_directories(dst.parent_path());
        std::ofstream out(dst, std::ios::binary);
        if (!out.is_open()) {
            return false;
        }
        binary_writer ar(out, fmt.magic, fmt.version);
        ar & baked;
        log::println(log::category::assets, "{} compiled: {}",
            std::meta::display_string_of(^^T), dst.filename().string());
        return true;
    }

    static auto needs_recompile(
        const std::filesystem::path& src,
        const std::filesystem::path& dst
    ) -> bool;

    static auto dependencies(
        const std::filesystem::path& src
    ) -> std::vector<std::filesystem::path>;
};
```

Existing specializations of `asset_compiler<texture>` and
`asset_compiler<font>` are deleted once `texture_baked` / `font_baked`
exist. The registry keeps working — same `has_asset_compiler<T>` concept,
same `add_loader<T>`, same registration flow.

`needs_recompile` becomes generic:

```cpp
template <typename T> requires has_asset_format<T>
auto gse::asset_compiler<T>::needs_recompile(
    const std::filesystem::path& src,
    const std::filesystem::path& dst
) -> bool {
    if (!std::filesystem::exists(dst)) {
        return true;
    }
    const auto dst_time = std::filesystem::last_write_time(dst);
    if (std::filesystem::last_write_time(src) > dst_time) {
        return true;
    }
    if constexpr (fmt.meta_sidecar) {
        const auto meta = src.parent_path() / (src.stem().string() + ".meta");
        if (std::filesystem::exists(meta) && std::filesystem::last_write_time(meta) > dst_time) {
            return true;
        }
    }
    return false;
}
```

Texture's bespoke `needs_recompile` and `dependencies` collapse into the
`meta_sidecar = true` flag.

**Exit criterion.** Both `texture` and `font` source their compile path
from the generic primary template. Their old `template<>` specializations
are deleted. Baked file bytes are byte-identical to pre-A2 (verified by
hash compare on `Resources/Baked/Textures` and `Resources/Baked/Fonts`).
Hot reload still works.

## A3 — migrate `model` (and interim `shader`)

### A3a. Model

The same machinery, slightly different shape because the model file
already includes a custom `serialize(ar, model_baked&)` routine. A3 is
mechanical: define `model_baked` with `[[= asset_format{...}]]`, leave
the `serialize` overload unchanged, replace `compile_one` with the
generic path, replace the
[`binary_reader(in, 0x474D444C, 4, ...)`](../Engine/Engine/Source/Graphics/3D/Vulkan/Model.cppm)
call site with `load_baked<model_baked>`.

### A3b. Shader (interim, throwaway)

Migrate `shader` to `[[= asset_format{...}]]` as part of A3, accepting
that the work is throwaway when shader phase S4 retires shader from the
asset pipeline entirely. The throwaway is small (~30 LOC) and the
cleanliness of having every asset type on the same path during overlap
outweighs the cost.

The 4 `binary_reader(stream, magic, version, path)` call sites today
(Texture, Font, Model, Shader) collapse to `load_baked<T>` after A2 +
A3. Shader's call site disappears entirely when S4 ships.

**Exit criterion.** `model` and `shader` load through `load_baked`. All
four hand-inlined `binary_reader(stream, magic, version, ...)`
constructors are gone.

## A4 — type pack and compositional `append`

Goal: enable the eventual `asset_system<Ctx, Ts...>` collapse by giving
each subsystem a way to declare which asset types it owns. Per-module
type packs replace per-type runtime registration.

### A4a. `type_pack` in containers

`Engine/Engine/Source/Containers/TypePack.cppm`:

```cpp
export module gse.containers:type_pack;

import std;

export namespace gse {

    template <typename... Ts>
    struct type_pack {
        static constexpr std::size_t size = sizeof...(Ts);

        template <template <typename...> typename Tmpl, typename... Prefix>
        using apply = Tmpl<Prefix..., Ts...>;
    };
}
```

### A4b. Compositional `append`

`gse.assets` exports a flatten-and-append meta-function:

```cpp
export module gse.assets:append;

import gse.containers;

namespace gse::detail {

    template <typename... Args> struct flatten;

    template <typename Pack, typename... Rest>
    struct flatten<Pack, Rest...> {
        using head = typename flatten<Pack>::type;
        using tail = typename flatten<Rest...>::type;
        using type = /* concatenate head and tail */;
    };

    template <typename... Ts>
    struct flatten<type_pack<Ts...>> { using type = type_pack<Ts...>; };

    template <typename T> requires (!std::derived_from</* type_pack tag */>)
    struct flatten<T> { using type = type_pack<T>; };

    template <> struct flatten<> { using type = type_pack<>; };
}

export namespace gse::assets {
    template <typename... Args>
    using append = typename detail::flatten<Args...>::type;
}
```

(Sketch — actual concat helper elided. The point is: `append<...>` accepts
an arbitrary mix of bare types and existing type packs, flattens into one
pack.)

Each subsystem appends its slice; the game composes:

```cpp
// Engine/Engine/Source/Graphics/AssetTypes.cppm — sub-partition of gse.graphics
export module gse.graphics:asset_types;
import :texture;
import :font;
import :model;
import gse.containers;

export namespace gse::graphics {
    using asset_types = type_pack<texture_baked, font_baked, model_baked>;
}

// Engine/Engine/Source/Audio/AssetTypes.cppm  (when audio gets baked assets)
export module gse.audio:asset_types;
export namespace gse::audio {
    using asset_types = type_pack<sound_baked>;
}

// In game code
import gse.graphics;
import gse.audio;

using game_assets = gse::assets::append<
    gse::graphics::asset_types,
    gse::audio::asset_types,
    world_chunk_baked
>;
```

Each subsystem's "init phase" corresponds to:

1. **Compile-time:** declare its `asset_types` typedef. This is what
   "appends" the types into the closed set.
2. **Run-time:** during the subsystem's init function, call into the
   asset system to compile/queue assets for its slice. Example:
   `sys.compile<gse::graphics::asset_types>()` or
   `sys.queue<texture_baked>(...)`.

### A4c. Why not runtime registration

`gse::assets::append<T...>()` as a *runtime call* is incompatible with
A5's collapse — a `tuple<loader<Ts, Ctx>...>` cannot grow at runtime. We
could fall back to type-erased `unordered_map<id, loader_base>` to
support runtime append, but that's the current registry. Defeats the
refactor.

The compile-time `using` approach gets 95% of the ergonomic feel
(per-subsystem ownership, flat list at the game level) for 0% of the
cost (no virtual dispatch, no runtime lookup, no `std::function`
wrapping). Worth the constraint.

**Exit criterion.** Per-module `asset_types` typedefs exist.
`game_assets` composed at game init. Existing `asset_registry` still in
use; new pack is parallel infrastructure not yet wired up.

## A5 — `asset_system<Ctx, Ts...>` collapse

Goal: replace `asset_registry` + `asset_pipeline` + `loader_base` /
`loader_t<T>` virtual hierarchy with one statically-instantiated class.

### A5a. `asset_system`

`Engine/Engine/Source/Assets/AssetSystem.cppm`:

```cpp
export module gse.assets:asset_system;

import std;
import gse.core;
import gse.fs;
import gse.log;
import gse.config;
import gse.containers;

import :asset_format;
import :asset_compiler;
import :resource_loader;
import :resource_handle;
import :append;

export namespace gse::asset {

    class context {
    public:
        virtual ~context() = default;
        virtual auto take_pending_finalizations() -> std::vector<std::pair<id, id>> = 0;
    };

    struct compile_result {
        std::size_t success_count = 0;
        std::size_t failure_count = 0;
        std::size_t skipped_count = 0;

        auto operator+=(const compile_result& o) -> compile_result&;
    };

    template <typename Ctx, typename... Ts>
    class system final : public non_copyable {
    public:
        system(
            Ctx& ctx,
            context& acx
        );

        template <typename T>
        auto loader(
            this auto&& self
        ) -> auto& {
            return std::get<resource::loader<T, Ctx>>(self.m_loaders);
        }

        template <typename T>
        auto get(id rid) -> resource::handle<T> {
            return loader<T>().get(rid);
        }

        template <typename T>
        auto get(const std::string& filename) -> resource::handle<T> {
            return loader<T>().get(filename);
        }

        template <typename T, typename... Args>
        auto queue(const std::string& name, Args&&... args) -> resource::handle<T> {
            return loader<T>().enqueue(name, std::make_unique<T>(name, std::forward<Args>(args)...));
        }

        template <typename T>
        auto compile() -> compile_result;

        template <typename Pack> requires /* Pack is type_pack */
        auto compile() -> compile_result;

        auto compile_all() -> compile_result;

        auto process_resource_queue() -> void;
        auto finalize_pending_loads() -> void;
        auto finalize_reloads() -> void;

        auto enable_hot_reload() -> void;
        auto disable_hot_reload() -> void;
        auto poll_assets() -> void;
        auto shutdown() -> void;

    private:
        Ctx* m_ctx;
        context* m_acx;
        std::tuple<resource::loader<Ts, Ctx>...> m_loaders;
        file_watcher m_watcher;
        bool m_hot_reload_enabled = false;

        auto on_source_change(const std::filesystem::path& src) -> void;
        auto on_baked_change(const std::filesystem::path& baked) -> void;

        template <typename T>
        auto compute_baked_path(const std::filesystem::path& src) const -> std::filesystem::path;
    };

    template <typename Ctx, typename Pack>
    using system_for = typename Pack::template apply<system, Ctx>;
}
```

The per-subsystem `compile<Pack>()` overload lets each init phase ask
the asset system to bake its slice:

```cpp
auto graphics_init(asset::system_for<gpu::context, game_assets>& sys) -> void {
    sys.compile<gse::graphics::asset_types>();
}
```

### A5b. Generic `compile<T>`

```cpp
template <typename Ctx, typename... Ts>
template <typename T>
auto gse::asset::system<Ctx, Ts...>::compile() -> compile_result {
    constexpr auto fmt = format_of<T>();
    compile_result result{};

    const auto source_root = config::resource_path / fmt.source_dir;
    const auto baked_root  = config::baked_resource_path / fmt.baked_dir;

    if (!std::filesystem::exists(source_root)) {
        return result;
    }

    std::filesystem::create_directories(baked_root);

    for (const auto& entry : std::filesystem::recursive_directory_iterator(source_root)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const auto ext = entry.path().extension().string();
        if (std::ranges::find(fmt.source_exts, ext) == fmt.source_exts.end()) {
            continue;
        }

        const auto dst = compute_baked_path<T>(entry.path());
        if (!asset_compiler<T>::needs_recompile(entry.path(), dst)) {
            ++result.skipped_count;
            continue;
        }

        if (asset_compiler<T>::compile_one(entry.path(), dst)) {
            ++result.success_count;
        }
        else {
            ++result.failure_count;
        }
    }

    return result;
}

template <typename Ctx, typename... Ts>
auto gse::asset::system<Ctx, Ts...>::compile_all() -> compile_result {
    compile_result total{};
    (total += compile<Ts>(), ...);
    return total;
}
```

`compile_all` is one fold expression. No `compiler_entry`, no
`std::function`, no runtime dispatch table.

### A5c. Hot reload via `template for`

```cpp
template <typename Ctx, typename... Ts>
auto gse::asset::system<Ctx, Ts...>::on_baked_change(const std::filesystem::path& baked) -> void {
    const auto ext = baked.extension().string();
    template for (constexpr auto Tinfo : type_pack<Ts...>{}) {
        using R = [: Tinfo :];
        constexpr auto fmt = format_of<R>();
        if (ext == fmt.baked_ext) {
            loader<R>().queue_reload_by_path(baked);
            return;
        }
    }
}

template <typename Ctx, typename... Ts>
auto gse::asset::system<Ctx, Ts...>::on_source_change(const std::filesystem::path& src) -> void {
    const auto ext = src.extension().string();
    template for (constexpr auto Tinfo : type_pack<Ts...>{}) {
        using R = [: Tinfo :];
        constexpr auto fmt = format_of<R>();
        if (std::ranges::find(fmt.source_exts, ext) != fmt.source_exts.end()) {
            const auto dst = compute_baked_path<R>(src);
            if (asset_compiler<R>::compile_one(src, dst)) {
                loader<R>().queue_reload_by_path(dst);
            }
            return;
        }
    }
}
```

The pipeline's "find compiler by extension" runtime loop becomes a
compile-time-unrolled extension comparison.

### A5d. Templated tokens

[ResourceLoader.cppm:34](../Engine/Engine/Source/Assets/ResourceLoader.cppm)
defines `gpu_work_token` and `reload_token` taking `loader_base*`. After
A5, no virtual base exists — they take any loader by template:

```cpp
template <typename Loader>
class gpu_work_token final : public non_copyable {
public:
    gpu_work_token(
        Loader* loader,
        id resource_id,
        std::size_t queue_size_before
    ) : m_loader(loader), m_id(resource_id), m_queue_size_before(queue_size_before) {
        m_loader->update_state(m_id, state::loading);
    }

    ~gpu_work_token() override {
        if (std::uncaught_exceptions()) {
            m_loader->update_state(m_id, state::failed);
            return;
        }
        m_loader->finalize_state(m_id, m_queue_size_before);
    }

private:
    Loader* m_loader;
    id m_id;
    std::size_t m_queue_size_before;
};

template <typename L> gpu_work_token(L*, id, std::size_t) -> gpu_work_token<L>;
```

CTAD keeps existing call sites unchanged
([ResourceLoader.cppm:322, 544](../Engine/Engine/Source/Assets/ResourceLoader.cppm)).

### A5e. `loader<T, Ctx>` loses inheritance

Today: `loader<T, Ctx>` inherits `loader_t<T>` inherits `loader_base`.
After A5: `loader<T, Ctx>` inherits `non_copyable` only. Every member
loses `override`. Bodies unchanged. `loader_base` and `loader_t<T>` are
deleted.

### A5f. Game-side surface

**Before:**

```cpp
asset_registry registry{ asset_ctx };
auto* tex_loader   = registry.add_loader<texture>(rendering_ctx);
auto* font_loader  = registry.add_loader<font>(rendering_ctx);
auto* model_loader = registry.add_loader<model>(rendering_ctx);
registry.compile_all();
auto handle = registry.queue<texture>("hero_idle");
```

**After:**

```cpp
gse::asset::system_for<gpu::context, game_assets> sys{ rendering_ctx, asset_ctx };
sys.compile_all();
auto handle = sys.queue<texture_baked>("hero_idle");
```

**Exit criterion.** `asset_registry`, `asset_pipeline`, `loader_base`,
`loader_t<T>`, `compiler_entry` deleted. All callers updated.

LOC delta:

| Module | Before | After |
|---|---|---|
| `AssetCompiler.cppm` | 59 | ~120 |
| `AssetPipeline.cppm` | 444 | **0** |
| `AssetRegistry.cppm` | 302 | **0** |
| `ResourceLoader.cppm` | 597 | ~440 |
| `AssetSystem.cppm` (new) | — | ~220 |
| `AssetTypes.cppm` (per-subsystem, new) | — | ~40 total |
| `TypePack.cppm` (new) | — | ~30 |
| `AssetFormat.cppm` (new) | — | ~70 |
| Per-type compiler files (texture/font/model) | ~412 | ~190 |
| **Net** | ~1,814 | ~1,110 |

About a 40% reduction in pipeline-related code, with the deleted code
being the type-erased dispatch glue.

---

# Shader pipeline (track S)

Five phases, S1–S5. Each produces something usable on its own and leaves
older shaders unchanged. Nothing in phase S(N) commits you to phase
S(N+1).

| Phase | What it produces | Depends on |
|-------|------------------|------------|
| S1 | C++ struct → `.slang` struct codegen | P2996 reflection |
| S2 | Resource binding codegen (UBO/SSBO/push) | S1 |
| S3 | Compute entry-point forwarder + `shader_builder` API | S2 |
| S4 | Graphics forwarders (vertex/fragment/mesh) + varyings | S3 |
| S5 | Permutation DSL (generics, specialization constants) | S4 |

## S1 — struct codegen

Goal: every GPU struct authored once in C++, emitted as matching Slang.
After this phase, a rename in C++ is a compile error in Slang (or vice
versa). Nothing else in the shader pipeline changes yet.

### S1a. Annotation + registry

```cpp
namespace gse::shaders {

struct shader_struct {};

template <std::meta::info E, std::meta::info... Rest>
consteval auto register_shader_types() -> void;

}
```

A struct becomes a shader struct by annotation:

```cpp
namespace gse::shaders::forward {

[[= shader_struct]]
struct light {
    vec3<length>   position;
    float          radius;
    vec3<unitless> color;
    float          intensity;
    vec3<unitless> direction;
    float          cut_off;
    vec3<length>   world_position;
    float          outer_cut_off;
    std::uint32_t  light_type;
    float          source_radius;
    float          ambient_strength;
    float          constant;
    float          linear;
    float          quadratic;
};

[[= shader_struct]]
struct material_data {
    vec3<unitless> base_color;
    float          metallic;
    float          roughness;
    std::uint32_t  _pad[3];
};

}
```

### S1b. Type map (units strip at the boundary)

The core trait. `vec3<length>` and `vec3<unitless>` both emit `float3`;
quantity wrappers collapse to their storage scalar. Units stay in C++,
stripped when crossing the GPU boundary.

```cpp
namespace gse::shaders::detail {

template <typename T> struct slang_type;

template <> struct slang_type<float>       { static constexpr std::string_view name = "float"; };
template <> struct slang_type<std::int32_t>{ static constexpr std::string_view name = "int"; };
template <> struct slang_type<std::uint32_t>{ static constexpr std::string_view name = "uint"; };

template <gse::quantity_like Q>
struct slang_type<Q> : slang_type<typename Q::rep> {};

template <gse::quantity_like Q> struct slang_type<gse::vec2<Q>> { static constexpr std::string_view name = "float2"; };
template <gse::quantity_like Q> struct slang_type<gse::vec3<Q>> { static constexpr std::string_view name = "float3"; };
template <gse::quantity_like Q> struct slang_type<gse::vec4<Q>> { static constexpr std::string_view name = "float4"; };
template <>                     struct slang_type<gse::unitless::vec2> { static constexpr std::string_view name = "float2"; };
template <>                     struct slang_type<gse::unitless::vec3> { static constexpr std::string_view name = "float3"; };
template <>                     struct slang_type<gse::unitless::vec4> { static constexpr std::string_view name = "float4"; };
template <>                     struct slang_type<gse::quat>   { static constexpr std::string_view name = "float4"; };
template <>                     struct slang_type<gse::mat3>   { static constexpr std::string_view name = "float3x3"; };
template <>                     struct slang_type<gse::mat4>   { static constexpr std::string_view name = "float4x4"; };

template <typename T, std::size_t N> struct slang_type<std::array<T, N>> {
    static auto emit(std::string_view ident) -> std::string;
};

}
```

### S1c. The walk

Iterate nonstatic data members, pull name + type, delegate to the type
map:

```cpp
template <typename T>
consteval auto emit_slang_struct() -> std::string {
    std::string out = std::format("struct {} {{\n", std::meta::identifier_of(^^T));
    template for (constexpr auto M : std::meta::nonstatic_data_members_of(^^T)) {
        using F = [: std::meta::type_of(M) :];
        constexpr auto ident = std::meta::identifier_of(M);
        if constexpr (requires { detail::slang_type<F>::emit(ident); })
            out += std::format("    {};\n", detail::slang_type<F>::emit(ident));
        else
            out += std::format("    {} {};\n", detail::slang_type<F>::name, ident);
    }
    out += "};\n";
    return out;
}
```

A registry scan (one `consteval {}` block per shader namespace)
concatenates emissions, writes the result to
`build/generated/shader_types/<namespace>.slang`. The file exists on disk
so Slang LSP picks it up — authoring experience is unchanged.

### S1d. Layout

Scalar layout (`VK_EXT_scalar_block_layout`, core in Vulkan 1.2). Already
listed in `vulkan_extensions.md`; enable it if it isn't on already. With
scalar layout, C++ and Slang agree without padding tricks — `vec3` is 12
bytes both sides, arrays pack tight, struct end isn't auto-padded. Slang
emits scalar buffer layout by default for `StructuredBuffer`; UBO
push-constants go scalar via `[[vk::layout(scalar)]]` on the block.

Canary: after each struct emission, static-assert the round-trip size.

```cpp
template <typename T>
consteval auto slang_scalar_size() -> std::size_t;

static_assert(sizeof(gse::shaders::forward::light) == slang_scalar_size<gse::shaders::forward::light>());
```

If the C++ layout ever drifts (someone adds a `bool` field which is 1
byte in C++ but 4 in Slang), the assertion fires at compile time with
the exact struct name.

### S1e. Build integration

Generated `.slang` headers live in `Engine/Resources/Shaders/generated/`,
regenerated by a small CMake custom target before shader compile.
Gitignored. Shader files always live alongside other shaders, the
generated/ directory is clearly marked, and Slang LSP picks them up
without any LSP config changes.

**Exit criterion.** All shared structs in `common.slang` are migrated to
annotated C++ counterparts. The `common.slang` file contains only
functions and generated imports. No renderer touches a hand-written
struct.

## S2 — resource binding codegen

Goal: descriptor sets, push constants, UBOs, SSBOs, samplers, RT
acceleration structures — all authored in C++, emitted to a matching
Slang layout file. Compile-time `pc.set(struct)` lands as the first
slice (was reflection_opportunities.md § A5 Layer 1).

### S2a. Binding metadata

Each resource kind gets a tag type carrying binding + name. Annotations
attach them to C++ declarations:

```cpp
namespace gse::shaders::forward {

struct binding { std::uint32_t set; std::uint32_t slot; };

[[= binding{0, 0}]] struct camera_ubo {
    mat4 view;
    mat4 proj;
    mat4 inv_view;
};

[[= binding{0, 1}, = ssbo_readonly]]   struct lights_buffer    { using element = light; };
[[= binding{0, 2}, = rt_tlas]]         struct scene_tlas       {};
[[= binding{0, 3}, = ssbo_readonly]]   struct light_index_list { using element = std::uint32_t; };
[[= binding{0, 4}, = ssbo_readonly]]   struct tile_light_table { using element = vec2u; };
[[= binding{0, 5}, = ssbo_readonly]]   struct material_palette { using element = material_data; };

[[= binding{1, 0}, = ssbo_readonly]]   struct vertices_buffer     { using element = vertex; };
[[= binding{1, 1}, = ssbo_readonly]]   struct meshlets_buffer     { using element = meshlet_descriptor; };
[[= binding{1, 6}, = sampler2d]]       struct diffuse_sampler     {};

[[= push_constant]]
struct forward_push {
    std::uint32_t meshlet_offset;
    std::uint32_t meshlet_count;
    std::uint32_t first_instance;
    std::int32_t  num_lights;
    vec2u         screen_size;
    std::int32_t  shadow_quality;
    std::int32_t  ao_quality;
    std::int32_t  reflection_quality;
};

}
```

### S2b. Layout emission

A reflected namespace walk finds every declaration annotated with
`binding`, `push_constant`, etc., and emits the matching Slang:

```
// build/generated/layouts/forward.slang
import generated.shader_types.forward;

[[vk::binding(0, 0)]]
cbuffer camera_ubo {
    float4x4 view;
    float4x4 proj;
    float4x4 inv_view;
};

[[vk::binding(0, 1)]] StructuredBuffer<light>           lights_buffer;
[[vk::binding(0, 2)]] RaytracingAccelerationStructure   scene_tlas;
[[vk::binding(0, 3)]] StructuredBuffer<uint>            light_index_list;
[[vk::binding(0, 4)]] StructuredBuffer<uint2>           tile_light_table;
[[vk::binding(0, 5)]] StructuredBuffer<material_data>   material_palette;

[[vk::binding(1, 0)]] StructuredBuffer<vertex>             vertices_buffer;
[[vk::binding(1, 1)]] StructuredBuffer<meshlet_descriptor> meshlets_buffer;
[[vk::binding(1, 6)]] Sampler2D                            diffuse_sampler;

[[vk::push_constant]] ConstantBuffer<forward_push> push;
```

The hand-authored `Layouts/forward_3d.slang` is deleted. The generated
file takes its place; shaders `import generated.layouts.forward;` exactly
as before.

### S2c. C++-side plumbing

Compile-time `pc.set(struct)` reflects members and looks up layout
offsets from the shader's `.glayout` (which `ShaderLayoutCompiler` still
produces from the generated Slang during overlap). The same `set<T>`
template extends to UBO writes: reflect members, look up offsets, memcpy
at known offsets. The runtime hash lookup in
[`GpuPushConstants.cppm:37-43`](../Engine/Engine/Source/Gpu/GpuPushConstants.cppm)
becomes a compile-time offset table built once per `T`.

Typed bindless handles (from `bindless_renderer_plan.md` Phase 3) drop
into this phase naturally — `bindless<material_data>` is just a tagged
u32, emitted to Slang as `uint`.

**Exit criterion.** Every renderer in `Graphics/Renderers/` sources its
bindings from an annotated C++ struct. No `.glayout` lookup happens at
hot-path frame time. The `Layouts/` directory contains only generated
files. **The asset-pipeline's `compile<ShaderLayout>()` entry point at
[AssetRegistry.cppm:88](../Engine/Engine/Source/Assets/AssetRegistry.cppm)
is deleted as part of this phase** — layouts are no longer on-disk
artifacts.

## S3 — compute entry-point forwarder

Goal: compute shaders are written as library functions; the engine
generates `main` from C++. First user: VBD physics (7 compute shaders,
all uniform in shape). Biggest payoff for the GPU physics pipeline —
permutations currently expressed as duplicated `.slang` files become one
function with different dispatches.

### S3a. Hand-authored library side

```slang
// Engine/Resources/Shaders/VBDPhysics/vbd_solve_color.slang
import generated.shader_types.vbd;
import generated.layouts.vbd;
import vbd_shared;

public void solve_color(
    uint3 dtid,
    vbd_solve_push push,
    StructuredBuffer<vbd_particle> particles,
    RWStructuredBuffer<vbd_constraint> constraints
) {
    // body unchanged from today's shader
}
```

No `[shader(...)]`, no `[numthreads(...)]`, no `main`. Pure function.

### S3b. C++ builder

```cpp
namespace gse::shaders {

class shader_builder {
public:
    template <std::size_t X, std::size_t Y, std::size_t Z>
    auto compute() -> shader_builder&;

    template <typename T>         auto push_constant() -> shader_builder&;
    template <auto Binding>       auto bind(std::string_view name) -> shader_builder&;
    auto import_module(std::string_view mod) -> shader_builder&;

    template <auto LibFn>
    auto dispatch_to() -> shader_builder&;

    auto compile() -> gpu::shader_module;
};

}
```

Usage in `Physics/VBD/GpuSolver.cppm`:

```cpp
auto module = shader_builder{}
    .compute<64, 1, 1>()
    .push_constant<vbd_solve_push>()
    .bind<vbd::particles>("particles")
    .bind<vbd::constraints>("constraints")
    .import_module("vbd_solve_color")
    .dispatch_to<&vbd::solve_color>()
    .compile();
```

### S3c. Generated wrapper

The builder writes a synthetic `.slang` file and feeds it to Slang:

```slang
// build/generated/shaders/vbd_solve_color_cs.slang
import generated.shader_types.vbd;
import generated.layouts.vbd;
import vbd_solve_color;

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID) {
    solve_color(dtid, push, particles, constraints);
}
```

Four mechanical lines. If the hand-authored `solve_color` signature
drifts from the builder's binding set, Slang issues a clean compile
error pointing at the generated file. The error is unambiguous because
the wrapper is trivial.

### S3d. Reflected forwarder call

`dispatch_to<&LibFn>()` uses reflection on the library function's
parameter types to emit the call in the right order. The builder can
cross-check each parameter against the resources it's been told about
and fail at C++ compile time if they don't match.

```cpp
template <auto LibFn>
auto shader_builder::dispatch_to() -> shader_builder& {
    template for (constexpr auto P : std::meta::parameters_of(^^LibFn)) {
        using T = [: std::meta::type_of(P) :];
        if constexpr (std::is_same_v<T, uint3>) continue;
        static_assert(has_binding_for<T>(m_bindings),
            "library function parameter has no matching bind<>() call");
    }
    m_entry_target = ^^LibFn;
    return *this;
}
```

**Exit criterion.** Every compute shader in `Compute/` and `VBDPhysics/`
is authored as a library function + C++ builder invocation. The
`Compute/` and `VBDPhysics/` directories contain only function libraries;
no entry points. No more hand-written `[numthreads(...)]` or
`[shader("compute")]` in project source.

## S4 — graphics forwarders

Goal: vertex / fragment / mesh / amplification stages generated by the
builder. Adds two wrinkles over S3 — vertex inputs and interstage
varyings — both resolved by reflection on annotated structs.

**S4 is also the phase where shader exits the asset pipeline entirely.**
After S4, there are no `.gshader`/`.glayout` baked files,
`ShaderCompiler.cppm` and `ShaderLayoutCompiler.cppm` are deleted, and
`shader_baked` is removed from `gse::graphics::asset_types` if track A
has progressed to A4+.

### S4a. Vertex inputs

```cpp
namespace gse::shaders::forward {

[[= vertex_input]]
struct vertex {
    [[= semantic{"POSITION"}]]   vec3<length>   position;
    [[= semantic{"NORMAL"}]]     vec3<unitless> normal;
    [[= semantic{"TEXCOORD0"}]]  vec2<unitless> tex_coord;
};

}
```

Emits:

```slang
struct vertex_in {
    float3 position  : POSITION;
    float3 normal    : NORMAL;
    float2 tex_coord : TEXCOORD0;
};
```

### S4b. Varyings (interstage)

The same mechanism. Annotate a struct, mark semantics, builder glues it
between vertex and fragment forwarders.

```cpp
[[= varying]]
struct forward_varying {
    [[= semantic{"SV_Position"}, = precise]]  vec4<unitless> clip_pos;
    [[= semantic{"TEXCOORD0"}]]               vec3<length>   world_pos;
    [[= semantic{"TEXCOORD1"}]]               vec3<unitless> world_normal;
    [[= semantic{"TEXCOORD5"}, = nointerpolation]] std::uint32_t instance_idx;
};
```

Hand-authored vertex function:

```slang
public forward_varying vs_transform(vertex_in v, instance_data inst, float4x4 view, float4x4 proj);
```

Hand-authored fragment function:

```slang
public float4 shade_pixel(forward_varying i, /* ... */);
```

Generated wrapper:

```slang
[shader("vertex")]
forward_varying main(vertex_in v) {
    return vs_transform(v, instance_data[push.first_instance], view, proj);
}

[shader("fragment")]
float4 main(forward_varying i) : SV_Target0 {
    return shade_pixel(i, /* resources... */);
}
```

### S4c. Mesh + amplification

Meshlet shaders use `OutputVertices`, `OutputIndices`,
`SetMeshOutputCounts`, `DispatchMesh`, payloads. All buildable —
`mesh_stage<VertexCount, TriCount>()` replaces `compute<...>()`, payload
struct is another annotated C++ type.

The meshlet path is where this phase earns the most.
`meshlet_geometry.slang` today is 485 lines mixing stage declarations,
resource bindings, PBR math, RT shadow math, three `get_*_config()`
switch statements for quality tiers, and the entry points. After S4,
that file becomes a namespace of functions; the C++ side composes the
stages and picks quality via permutation.

### S4d. Shader exits the asset pipeline

After S4, the cleanup on the asset side:

- Remove `shader_baked` from `gse::graphics::asset_types` (if track A is
  at A4+).
- Delete `ShaderCompiler.cppm` (~971 LOC) and `ShaderLayoutCompiler.cppm`.
- Delete `Engine/Resources/Baked/Shaders/` directory and the `.gshader` /
  `.glayout` files.
- Hot reload migrates from `file_watcher` over baked files to the
  shader_builder watching its inputs (library `.slang` files only).

**Exit criterion.** All graphics shaders in `Standard3D/` and
`Standard2D/` source their vertex inputs and varyings from annotated
C++. No `[shader(...)]` stage attributes in project `.slang` sources.
The file count in `Engine/Resources/Shaders/` drops — multiple stages
per shader often collapse into one library namespace. `ShaderCompiler`
is deleted.

## S5 — permutation DSL

Goal: the `#ifdef` / `switch(quality)` patterns visible in
`meshlet_geometry.slang` become C++ composition. Today, quality tiers
live in Slang `switch` statements at fragment-shader entry
(`get_shadow_config(shadow_quality)` etc.). Post-S5, the builder picks a
different library function per tier at pipeline-create time.

### S5a. Generic library functions

Slang supports generics. Write one shading function parameterized over
shadow quality; the builder instantiates.

```slang
public interface IShadowSampler {
    float trace(float3 origin, float3 n, float3 dir, float t_max, float r, float2 pix, float view_dist);
}

public struct shadow_quality_4 : IShadowSampler { /* ... */ }
public struct shadow_quality_2 : IShadowSampler { /* ... */ }
public struct shadow_quality_off : IShadowSampler { float trace(...) { return 1.0; } }

public float4 shade_pixel_impl<S : IShadowSampler>(forward_varying i, /* ... */);
```

Builder picks at C++ side:

```cpp
switch (render_state.shadow_quality) {
    case 4:   b.specialize<&forward::shade_pixel_impl, forward::shadow_quality_4>(); break;
    case 2:   b.specialize<&forward::shade_pixel_impl, forward::shadow_quality_2>(); break;
    default:  b.specialize<&forward::shade_pixel_impl, forward::shadow_quality_off>(); break;
}
```

### S5b. Specialization constants

For continuous parameters (counts, thresholds), Slang specialization
constants are set via the builder at pipeline-create time. C++ handles
the u32 binding; no string-keyed runtime dispatch.

### S5c. Pipeline cache keying

Each unique combination (bindings × imports × specializations) is a
cache key. Hash via reflection over the builder state — no hand-rolled
hashmap.

**Exit criterion.** No quality-tier `switch` statements in shader
source; permutations visible in C++ renderer code. Every shader program
has a canonical hash derived from its composition, used as the pipeline
cache key.

## Escape hatch — raw entry points

Some shaders don't fit the forwarder shape. A hand-tuned TAA resolve, a
one-off blit, a tiny debug overlay. Forcing them through the builder
adds noise without saving anything. The builder keeps one escape:

```cpp
auto module = shader_builder{}
    .compute<8, 8, 1>()
    .push_constant<blit_push>()
    .bind<blit::src>("src")
    .bind<blit::dst>("dst")
    .raw_entry_point(R"(
        [shader("compute")]
        [numthreads(8, 8, 1)]
        void main(uint3 id : SV_DispatchThreadID) {
            dst[id.xy] = src.Load(int3(id.xy, 0));
        }
    )")
    .compile();
```

Bindings still flow from C++; the body is user-written. The escape is
distinguishable from the forwarder path — `raw_entry_point` explicitly
opts out of the reflected check that parameter names match bindings.

---

# Where the tracks intersect

Asset and shader refactors are independent in execution. Both can be
worked in parallel; neither blocks the other. They touch at exactly two
points:

**1. The asset pipeline today owns shader compilation.**
[`AssetRegistry::compile<ShaderLayout>()`](../Engine/Engine/Source/Assets/AssetRegistry.cppm)
exists, `ShaderCompiler.cppm` is the largest per-type compiler in the
codebase, and `Resources/Baked/Shaders/` holds `.gshader` artifacts that
go through the same `binary_reader` / hot-reload machinery as textures.

**2. The shader refactor removes shader from the asset pipeline.**
After S2, layouts stop being on-disk artifacts and `compile<ShaderLayout>`
is deleted. After S4, shaders themselves are produced at pipeline-create
time by `shader_builder` rather than baked from disk; `ShaderCompiler`,
`ShaderLayoutCompiler`, `.gshader`, `.glayout`, and `Resources/Baked/Shaders/`
are all deleted.

Sequencing implications:

- **A1–A3 can land before any of S.** Texture/font/model migration is
  independent; shader is migrated as throwaway in A3.
- **S2 deletes `compile<ShaderLayout>` from the asset pipeline.** This is
  cleanup that doesn't require A4+ to have happened.
- **S4 retires shader from the asset pipeline.** If A4 has already
  happened, this means removing `shader_baked` from
  `gse::graphics::asset_types`. If A4 hasn't yet happened, the legacy
  registry's `add_loader<shader>` call is removed.
- **A4–A5 can happen before, after, or interleaved with S.** The two
  refactors don't deadlock each other.

Recommended interleaving: **A1 → A2 → A3 → S1 → S2 → A4 → A5 → S3 → S4 →
S5.** This front-loads the LOC reduction (A) before the
larger-architectural work (S), and lets S4's "shader exits the pipeline"
cleanup land in a world where the pipeline is already collapsed (more
satisfying delete).

But any ordering that respects per-track phase dependencies works.

---

# Migration order (combined timeline)

Each step keeps the build green and the engine runnable.

1. **A1.** Land `asset_format`, `format_of`, `raw_blob_owned`,
   `load_baked`. Nothing else changes.
2. **A2 — `texture`.** Smallest body, exercises `meta_sidecar` path.
3. **A2 — `font`.** Exercises `unordered_map` + heterogeneous layout.
4. **A3 — `model`.** Exercises custom `serialize` overload.
5. **A3 — `shader` (interim).** Throwaway annotation work; deleted in S4.
6. **S1.** Migrate every shared struct in `common.slang` to annotated C++,
   slice per-shader-family (forward3D first, then VBD, then compute, then
   2D). `common.slang` keeps only functions and generated imports.
7. **S2 — `forward` family bindings.** Annotated C++ → generated
   `forward.slang` layout. UiRenderer, CaptureRenderer, PhysicsDebugRenderer
   migrate too — layout-light, fast wins.
8. **S2 cleanup.** Delete `compile<ShaderLayout>` from `AssetRegistry`.
   No `.glayout` lookup happens at hot-path frame time.
9. **A4.** Add `type_pack`, `gse::assets::append`, per-subsystem
   `asset_types` typedefs. Old registry still primary.
10. **A5a.** Build `asset::system<Ctx, Ts...>` alongside `asset_registry`.
    Both work; pick one at instantiation site.
11. **A5b — switch one game scene.** Verify end-to-end: compile, queue,
    get, hot reload, finalize.
12. **A5c — switch all game scenes.** Delete `asset_registry`,
    `asset_pipeline`, `loader_base`, `loader_t<T>`, `compiler_entry`.
13. **A5d — token + loader cleanup.** Templated tokens, drop `override`,
    delete dead virtual methods.
14. **S3.** VBD compute shaders — uniform shape, biggest payoff. Then
    other compute shaders.
15. **S4 — graphics.** Forward3D / meshlet path is the largest single
    migration. Standard3D / Standard2D follow.
16. **S4 cleanup.** Remove `shader_baked` from `gse::graphics::asset_types`.
    Delete `ShaderCompiler`, `ShaderLayoutCompiler`, `Resources/Baked/Shaders/`.
17. **S5.** Replace quality-tier switches with permutation specialization.

---

# Costs and mitigations

**Compile time (asset side).** `system<Ctx, Ts...>` is one large
template; instantiating it forces every loader and `bake()` to be
visible. Mitigation: define members out-of-line in a single
`AssetSystem.cpp` that explicitly instantiates `system_for<gpu::context,
game_assets>`. Game code includes only the export interface and pays
nothing extra. Non-issue at ~5 types; split into multiple `asset_system`
instances by domain if it grows past ~20.

**Compile time (shader side).** S1's codegen runs every build.
Mitigation: dirty-check keyed on the C++ struct's layout hash
(reflection-computed) avoids regenerating unchanged files. S3+'s wrapper
generation is per-pipeline-create, cached by builder hash — not part of
the C++ build.

**Type-list locality (asset side).** Currently any module can
`add_loader<MyType>` to self-register. After A4, types must appear in
some `asset_types` typedef. Mitigation: per-subsystem typedef colocated
with the asset type's source file; game-level
`using game_assets = append<...>` is one line. The cost is real but small.

**Closed set (asset side).** Plugin/DLL cannot inject a new asset type at
runtime. None of the current code does this. If ever needed, add a small
type-erased side channel later.

**P2996 maturity.** S1 uses reflection to emit strings — well-supported.
S5's specialization dispatch may need `define_aggregate` /
`queue_injection`, which are on the less-tested edges. Fallback:
string-only codegen throughout, with C++ helpers picking functions by
name rather than reflected pointer. Slower to debug but no functionality
loss.

**Error message quality (shader side).** Slang errors point at generated
files. Mitigation: always write the wrapper to disk (not just
in-memory), emit Slang `#line`-like directives pointing back into the
library function for the callable portion, and keep the forwarder small
enough that "error is in the forwarder" essentially never happens.

**Slang LSP and generated files.** Generated `.slang` needs to be on
disk before the user opens a shader that imports it. For first-time
clones, the custom target runs at configure time, not build time. Same
pattern used by generated C++ headers (`config.h` style); well-understood.

**Scalar layout correctness under Slang.** Slang emits scalar layout for
`StructuredBuffer` by default; `cbuffer` and push-constants need the
explicit `[[vk::layout(scalar)]]` attribute. Validate in S1 with a test
program that writes/reads every struct type and compares bytes.

**RenderDoc step-through.** With forwarders, RenderDoc sees the
generated wrapper plus the imported function libraries. Since stepping
happens in the function body (which is hand-written), the debugging
experience is essentially unchanged. The forwarder itself is four lines
of glue; stepping past it is fast.

**Pipeline cache invalidation.** Any change to an emitted struct or
binding invalidates every shader that used it — by design. The existing
shader cache already keys on source hash; nothing new to build, but the
blast radius is larger when C++ struct changes cause more pipelines to
rebuild.

**Annotation portability.** Both refactors use P2996 value annotations
(`[[= asset_format{...}]]`, `[[= shader_struct]]`, `[[= binding{0,0}]]`).
Already in use elsewhere in the codebase. Risk of
`std::span<const std::string_view>` in annotation values being awkward —
fallback is `static_vector<std::string_view, 8>`.

**Annotation discoverability.** Annotations are at type declarations,
not at use sites. New contributors won't see them unless they look at
the type. Mitigation: each baked struct lives in a file named
`<type>Baked.cppm` (or as a partition next to `<type>.cppm`); each
shader resource group lives in a single namespace per family. Once seen
on one type the pattern generalizes.

---

# Relationship to other documents

- `reflection_opportunities.md` § A3 ("Reflected Archive serialization")
  is **done**. This refactor extends that pattern from per-resource
  serialize bodies to the entire compile/load pipeline.
- `reflection_opportunities.md` § A5 ("Shader push-constant binding") is
  subsumed by S2.
- `reflection_opportunities.md` § B5 ("Pipeline state member-pointer
  mapping") is independent — Vulkan pipeline state struct fill, not
  asset compile or shader codegen. Doesn't interact.
- `bindless_renderer_plan.md` Layer 4 ("reflection-powered ergonomics") is
  subsumed by track S in its entirety:
  - **4a `slang_mirror<T>`** → S1 (struct codegen).
  - **4b typed `bindless<T>`** → S2 (binding annotations + compile-time
    `pc.set(struct)`); `bindless<material_data>` is a tagged u32 emitted as
    `uint`.
  - **4c `bindless_buffer<T>`** → declaration half is S2 (annotated C++ struct
    + `[[= ssbo_readonly]]`); allocator half stays in bindless Layer 1b.
  - **4d compile-time layout cross-check** → S1d's `slang_scalar_size<T>`
    canary.
  - **4e RAII handle** is the only Layer 4 item that stays in the bindless
    plan — track S doesn't own resource lifetime.
  After S1+S2, bindless Phase 3 collapses to a renaming pass on existing
  renderers. Bindless Phase 1+2 (Vulkan plumbing) and Phase 4 (GPU-driven
  rendering) are independent of track S.
- `coroutine_scheduler_unification.md` and
  `transient_gpu_work_redesign.md` are independent — both touch
  scheduling/GPU but not the content pipeline.

---

# Decision points to resolve before kickoff

| # | Question | Resolution |
|---|----------|------------|
| 1 | `source_exts` storage in annotations | `std::span<const std::string_view>`; fall back to `static_vector` on first compile error |
| 2 | `asset_types` placement per subsystem | Sub-partition (`gse.graphics:asset_types`), re-exported by parent module |
| 3 | `gse::assets::append<Args...>` shape | Permissive variadic — flattens both packs and bare types |
| 4 | Shader handling during A/S overlap | Migrate shader to `[[= asset_format]]` in A3 as throwaway; deleted in S4 |
| 5 | `raw_blob_owned` location | Sibling export in same module as `Archive.cppm` |
| 6 | Generated shader file location | `Engine/Resources/Shaders/generated/`, in-tree, gitignored |
| 7 | Codegen invocation | Dedicated codegen tool as CMake custom target — runs once per C++ change, not per engine launch |
| 8 | Annotation placement (shader side) | Inline at struct declaration; reflection on the namespace finds them, no macro registry |
| 9 | S1 scope | Per-shader-family slice (forward3D first, then VBD, then compute, then 2D); not a single big-bang migration |
| 10 | S1 boundary | Only structs that C++ also writes to. Slang-only helper structs stay hand-authored until S4 needs them as payloads |

All ten resolved. Refactor is ready to start at A1.

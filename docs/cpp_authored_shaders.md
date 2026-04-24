# C++-authored shaders — scope

## Motivation

Today every shader has two coupled declarations. The Slang source declares push
constants, descriptor bindings, shared structs, and entry points. The C++
renderer binds resources and writes push constants by name. The two are kept in
sync by hand — adding a field means editing both sides, and the `pc.set("x", v)`
path does a hash lookup per member at runtime to validate what's already known
at compile time.

A5 in `reflection_opportunities.md` solves one slice: compile-time
push-constant binding, with a thin Slang codegen step emitting the block struct.
This doc is the full-scope version of that idea. **C++ structs are the single
source of truth for every shader-side declaration a renderer cares about, and
entry points are composed from C++ by selecting library functions.** The
hand-authored part of `.slang` files collapses to pure function libraries —
functions with typed parameters, no entry points, no bindings, no structs.

Put differently: after this lands, writing a new shader stops being "author a
`.slang` file with resources + entry point + body" and becomes "declare the
resources in C++, write the body as a library function, and tell the builder
to dispatch to it." The wrapper `.slang` the engine hands to Slang is generated
every run from those pieces.

## Scope

In scope:

- **Struct codegen.** Shared GPU structs (Light, Vertex, InstanceData,
  MaterialData, MeshletDescriptor, ...) authored once in C++, emitted into
  generated `.slang` headers.
- **Resource codegen.** Push constants, UBOs, SSBOs, samplers, RT acceleration
  structures — all declared via annotated C++ with a reflected binding map.
- **Entry-point forwarders.** Compute / vertex / fragment / mesh / amplification
  `main` functions generated as thin shims that invoke a hand-authored library
  function.
- **Permutation control.** Generic instantiation and specialization constants
  selected by C++ composition, not by `#ifdef` forests.
- **Coexistence.** Legacy hand-authored shaders keep working during rollout.

Out of scope:

- Replacing Slang the language. Function libraries stay hand-authored in Slang.
- Replacing the Slang compiler or `.glayout` binary format. The compile pipeline
  still ends at Slang → SPIR-V; this machinery sits in front of it.
- Shader graph / node editor UI. Could build on this later; not here.
- Runtime hot-recompilation of C++-side structs. Those still require a C++
  rebuild. Slang-side changes (library function bodies, new permutations)
  remain hot-reloadable.
- Replacing the render-graph API. Pipelines still get created with pipeline
  state, push-constant ranges, layouts — this project just changes where the
  metadata originates.

## Architecture

Five phases. Each produces something usable on its own and leaves older
shaders unchanged. Nothing in phase N commits you to phase N+1.

| Phase | What it produces | Depends on |
|-------|-------------------|------------|
| 1 | C++ struct → `.slang` struct codegen | P2996 reflection (already enabled) |
| 2 | Resource binding codegen (UBO/SSBO/push) | Phase 1 |
| 3 | Compute entry-point forwarder + `shader_builder` API | Phase 2 |
| 4 | Graphics forwarders (vertex/fragment/mesh) + varyings | Phase 3 |
| 5 | Permutation DSL (generics, specialization constants) | Phase 4 |

### Phase 1 — struct codegen

Goal: every GPU struct authored once in C++, emitted as matching Slang.
After this phase, a rename in C++ is a compile error in Slang (or vice versa).
Nothing else in the shader pipeline changes yet.

#### 1a. Annotation + registry

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

#### 1b. Type map (units strip at the boundary)

The core trait. `vec3<length>` and `vec3<unitless>` both emit `float3`; quantity
wrappers collapse to their storage scalar. Units stay in C++, stripped when
crossing the GPU boundary.

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

#### 1c. The walk

Iterate nonstatic data members, pull name + type, delegate to the type map:

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

A registry scan (one `consteval {}` block per shader namespace) concatenates
emissions, writes the result to `build/generated/shader_types/<namespace>.slang`.
The file exists on disk so Slang LSP picks it up — authoring experience is
unchanged.

#### 1d. Layout

Scalar layout (`VK_EXT_scalar_block_layout`, core in Vulkan 1.2). Already
listed in `vulkan_extensions.md`; enable it if it isn't on already. With scalar
layout, C++ and Slang agree without padding tricks — `vec3` is 12 bytes both
sides, arrays pack tight, struct end isn't auto-padded. Slang emits scalar
buffer layout by default for `StructuredBuffer`; UBO push-constants go scalar
via `[[vk::layout(scalar)]]` on the block.

Canary: after each struct emission, static-assert the round-trip size.

```cpp
template <typename T>
consteval auto slang_scalar_size() -> std::size_t;

static_assert(sizeof(gse::shaders::forward::light) == slang_scalar_size<gse::shaders::forward::light>());
```

If the C++ layout ever drifts (someone adds a `bool` field which is 1 byte in
C++ but 4 in Slang), the assertion fires at compile time with the exact struct
name.

#### 1e. Build integration

Option A — pre-build step: a small tool runs `consteval` code, writes files,
added as a CMake custom target that runs before shader compile.

Option B — in-tree: `.slang` headers live in `Engine/Resources/Shaders/generated/`,
regenerated by the same custom target, gitignored.

Either works. I'd pick B — shader files always live alongside other shaders, the
generated/ directory is clearly marked, and Slang LSP picks them up without any
LSP config changes.

**Exit criterion.** All shared structs in `common.slang` are migrated to
annotated C++ counterparts. The `common.slang` file contains only functions
and generated imports. No renderer touches a hand-written struct.

### Phase 2 — resource binding codegen

Goal: descriptor sets, push constants, UBOs, SSBOs, samplers, RT acceleration
structures — all authored in C++, emitted to a matching Slang layout file.
A5's Layer 1 (compile-time `pc.set(struct)`) is a strict subset of this phase
and lands as the first slice.

#### 2a. Binding metadata

Each resource kind gets a tag type carrying binding + name. Annotations attach
them to C++ declarations:

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

#### 2b. Layout emission

A reflected namespace walk finds every declaration annotated with `binding`,
`push_constant`, etc., and emits the matching Slang:

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

The hand-authored `Layouts/forward_3d.slang` is deleted. The generated file
takes its place; shaders `import generated.layouts.forward;` exactly as before.

#### 2c. C++-side plumbing

A5 Layer 1 already sketches compile-time `pc.set(struct)`. The same `set<T>`
template extends to UBO writes: reflect members, look up layout offsets from
the shader's `.glayout` (which `ShaderLayoutCompiler` still produces from the
generated Slang), memcpy at known offsets. The runtime hash lookup in
`GpuPushConstants.cppm:37-43` becomes a compile-time offset table built once
per `T`.

Typed bindless handles (from `bindless_renderer_plan.md` Phase 3) drop into
this phase naturally — `bindless<material_data>` is just a tagged u32, emitted
to Slang as `uint`.

**Exit criterion.** Every renderer in `Graphics/Renderers/` sources its
bindings from an annotated C++ struct. No `.glayout` lookup happens at hot-path
frame time. The `Layouts/` directory contains only generated files.

### Phase 3 — compute entry-point forwarder

Goal: compute shaders are written as library functions; the engine generates
`main` from C++. First user: VBD physics (7 compute shaders, all uniform in
shape). Biggest payoff for the GPU physics pipeline — permutations currently
expressed as duplicated `.slang` files become one function with different
dispatches.

#### 3a. Hand-authored library side

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

#### 3b. C++ builder

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

#### 3c. Generated wrapper

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

Four mechanical lines. If the hand-authored `solve_color` signature drifts from
the builder's binding set, Slang issues a clean compile error pointing at the
generated file. The error is unambiguous because the wrapper is trivial.

#### 3d. Reflected forwarder call

`dispatch_to<&LibFn>()` uses reflection on the library function's parameter
types to emit the call in the right order. The builder can cross-check each
parameter against the resources it's been told about and fail at C++ compile
time if they don't match.

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

**Exit criterion.** Every compute shader in `Compute/` and `VBDPhysics/` is
authored as a library function + C++ builder invocation. The `Compute/` and
`VBDPhysics/` directories contain only function libraries; no entry points.
No more hand-written `[numthreads(...)]` or `[shader("compute")]` in project
source.

### Phase 4 — graphics forwarders

Goal: vertex / fragment / mesh / amplification stages generated by the builder.
Adds two wrinkles over Phase 3 — vertex inputs and interstage varyings — both
resolved by reflection on annotated structs.

#### 4a. Vertex inputs

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

#### 4b. Varyings (interstage)

The same mechanism. Annotate a struct, mark semantics, builder glues it between
vertex and fragment forwarders.

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

#### 4c. Mesh + amplification

Meshlet shaders use `OutputVertices`, `OutputIndices`, `SetMeshOutputCounts`,
`DispatchMesh`, payloads. All buildable — `mesh_stage<VertexCount, TriCount>()`
replaces `compute<...>()`, payload struct is another annotated C++ type.

The meshlet path is where this phase earns the most. `meshlet_geometry.slang`
today is 485 lines mixing stage declarations, resource bindings, PBR math, RT
shadow math, three `get_*_config()` switch statements for quality tiers, and
the entry points. After Phase 4, that file becomes a namespace of functions;
the C++ side composes the stages and picks quality via permutation.

**Exit criterion.** All graphics shaders in `Standard3D/` and `Standard2D/`
source their vertex inputs and varyings from annotated C++. No `[shader(...)]`
stage attributes in project `.slang` sources. The file count in
`Engine/Resources/Shaders/` drops — multiple stages per shader often collapse
into one library namespace.

### Phase 5 — permutation DSL

Goal: the `#ifdef` / `switch(quality)` patterns visible in `meshlet_geometry.slang`
become C++ composition. Today, quality tiers live in Slang `switch` statements
at fragment-shader entry (`get_shadow_config(shadow_quality)` etc.). Post-Phase 5,
the builder picks a different library function per tier at pipeline-create time.

#### 5a. Generic library functions

Slang supports generics. Write one shading function parameterized over shadow
quality; the builder instantiates.

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

#### 5b. Specialization constants

For continuous parameters (counts, thresholds), Slang specialization constants
are set via the builder at pipeline-create time. C++ handles the u32 binding;
no string-keyed runtime dispatch.

#### 5c. Pipeline cache keying

Each unique combination (bindings × imports × specializations) is a cache key.
Hash via reflection over the builder state — no hand-rolled hashmap.

**Exit criterion.** No quality-tier `switch` statements in shader source;
permutations visible in C++ renderer code. Every shader program has a
canonical hash derived from its composition, used as the pipeline cache key.

## Escape hatch — raw entry points

Some shaders don't fit the forwarder shape. A hand-tuned TAA resolve, a
one-off blit, a tiny debug overlay. Forcing them through the builder adds
noise without saving anything. The builder keeps one escape:

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
distinguishable from the forwarder path — `raw_entry_point` explicitly opts
out of the reflected check that parameter names match bindings.

## Interop with current shaders

During the rollout, hand-authored shaders keep compiling. The builder path
and the legacy `ShaderRegistry::cache()` path share the same pipeline-creation
machinery; both end at Slang → SPIR-V → `VkPipeline`. Migrations happen
shader-by-shader. A phase N migration does not block a phase N+1 migration on
a different shader.

Concretely, after Phase 2 lands:

- UiRenderer, CaptureRenderer, PhysicsDebugRenderer — layout-light, migrate first
- VBD compute shaders — uniform shape, Phase 3 target batch
- Forward3D / meshlet path — Phase 4 target, largest single migration
- PostProcess — probably the final holdouts, several are genuine one-offs
  (gaussian blur, rgba-to-nv12) that use the escape hatch

## Risks and open questions

- **P2996 `consteval` injection maturity.** Phase 1 uses reflection to emit
  strings, which is well-supported. Phase 5's specialization dispatch may need
  `define_aggregate` / `queue_injection`, which are on the less-tested edges.
  Fallback: string-only codegen throughout, with C++ helpers picking functions
  by name rather than reflected pointer. Slower to debug but no functionality
  loss.
- **Error message quality.** Slang errors point at generated files. Mitigation:
  always write the wrapper to disk (not just in-memory), emit Slang
  `#line`-like directives pointing back into the library function for the
  callable portion, and keep the forwarder small enough that "error is in the
  forwarder" essentially never happens.
- **Build-time cost.** Phase 1's codegen runs every build. A dirty-check keyed
  on the C++ struct's layout hash (reflection-computed) avoids regenerating
  unchanged files. Phase 3+'s wrapper generation is per-pipeline-create, cached
  by builder hash — not part of the C++ build.
- **Slang LSP and generated files.** Generated `.slang` needs to be on disk
  before the user opens a shader that imports it. For first-time clones, the
  custom target runs at configure time, not build time. Same pattern used by
  generated C++ headers (`config.h` style); well-understood.
- **Scalar layout correctness under Slang.** Slang emits scalar layout for
  `StructuredBuffer` by default; `cbuffer` and push-constants need the
  explicit `[[vk::layout(scalar)]]` attribute. Validate in Phase 1 with a
  test program that writes/reads every struct type and compares bytes.
- **RenderDoc step-through.** With forwarders, RenderDoc sees the generated
  wrapper plus the imported function libraries. Since stepping happens in the
  function body (which is hand-written), the debugging experience is
  essentially unchanged. The forwarder itself is four lines of glue; stepping
  past it is fast.
- **Pipeline cache invalidation.** Any change to an emitted struct or binding
  invalidates every shader that used it — by design. The existing shader cache
  already keys on source hash; nothing new to build, but the blast radius is
  larger when C++ struct changes cause more pipelines to rebuild.

## Decision points to resolve before Phase 1

1. **Generated-file location.** `Engine/Resources/Shaders/generated/`
   (in-tree, gitignored) or `build/generated/shaders/` (out-of-tree)?
   **Recommendation:** in-tree, gitignored. Slang LSP picks them up with no
   config; developers can inspect what was emitted; output lives alongside
   hand-written shaders.
2. **Codegen invocation.** CMake custom target with a dedicated tiny C++
   program (preferred — fast, deterministic) vs. a `consteval` block inside
   the engine that writes files during engine startup (simpler but runs every
   launch)? **Recommendation:** dedicated codegen tool. Rebuild cost is paid
   once per C++ change, not once per engine launch.
3. **Annotation placement.** Inline at struct declaration (as in the sketches
   above) vs. separate registration site (`GSE_SHADER_STRUCT(light)` macro
   that emits `[[= shader_struct]]` + push into a registry)? **Recommendation:**
   inline. Reflection on the namespace finds them without a macro; one fewer
   place to keep in sync.
4. **Scope of Phase 1.** Migrate every shared struct in `common.slang` in one
   go, or slice per-shader-family (forward3D first, then VBD, then compute,
   then 2D)? **Recommendation:** slice. `common.slang` has 10+ structs across
   subsystems; one family at a time means smaller PRs and cleaner rollbacks.
5. **Phase 1 boundary.** Is Phase 1 "every struct in every shader" or "every
   struct that C++ also writes to"? **Recommendation:** the latter. Slang-only
   helper structs (e.g. `MeshletPayload`) that no C++ code touches stay
   hand-authored until Phase 4 needs them as payloads.

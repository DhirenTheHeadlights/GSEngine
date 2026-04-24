# Bindless renderers — scope

## Motivation

Current renderers bind descriptors per draw. For each material / texture / mesh a
renderer issues `bind_descriptors(...)` with a fresh region. Benefits of moving to
bindless:

- **Draw-call fusion.** One `cmdDrawIndirect` can cover thousands of objects with
  different textures/materials, since all descriptors are visible to every draw.
- **GPU-driven culling.** Compute shaders can build indirect draw streams without the
  CPU knowing which material any given draw uses.
- **Simpler renderers.** No more per-frame descriptor writes for every object; the
  writer becomes a cold-path registration concern, not a hot-path frame concern.
- **Material authoring decoupling.** Adding a new material type no longer means
  touching every renderer that draws it.

## What's already in place

- Device features (just landed in P2.3):
  `runtimeDescriptorArray`, `descriptorBindingPartiallyBound`,
  `descriptorBindingVariableDescriptorCount`, `descriptorBindingSampledImageUpdateAfterBind`,
  `descriptorBindingStorageImageUpdateAfterBind`,
  `descriptorBindingStorageBufferUpdateAfterBind`,
  `shaderSampledImageArrayNonUniformIndexing`.
- Descriptor set type enum already has `descriptor_set_type::bind_less = 2` — no consumer yet.
- `descriptor_heap` supports persistent (not-transient) allocations, which is what bindless sets need.

What's missing: everything above that baseline.

## Architecture

Three distinct layers. Build them bottom-up.

### Layer 1 — Infrastructure (Vulkan-level, no reflection)

The boring but load-bearing part. Reflection does not help here.

#### 1a. Binding-flag propagation in shader layout

`ShaderRegistry::cache()` currently creates every `DescriptorSetLayout` with only
`eDescriptorBufferEXT`. For the `bind_less` set specifically:

- Layout flag: add `eUpdateAfterBindPool`.
- Per-binding flags via pNext `DescriptorSetLayoutBindingFlagsCreateInfo`:
  `PartiallyBound | UpdateAfterBind | VariableDescriptorCount` on every binding in
  the bindless set.

Gate it on "is this a bindless set?" — the other two set types (persistent, push)
keep their current behavior.

#### 1b. Bindless resource service (one per resource kind)

```cpp
namespace gse::gpu {
    struct bindless_texture_slot {
        std::uint32_t index = invalid_index;
        explicit operator bool() const { return index != invalid_index; }
    };

    class bindless_texture_set final : public non_copyable {
    public:
        explicit bindless_texture_set(context& ctx, std::uint32_t capacity = 4096);

        auto allocate(const vulkan::image_resource& img,
                      const sampler& samp) -> bindless_texture_slot;
        auto release(bindless_texture_slot slot) -> void;

        [[nodiscard]] auto layout() const -> vk::DescriptorSetLayout;
        [[nodiscard]] auto region() const -> const vulkan::descriptor_region&;
    };
}
```

Internally: simple free-list allocator over `[0, capacity)`, writes a combined-image-sampler
descriptor into the persistent region at `slot.index * descriptor_size`. Released slots
don't need clearing — `PartiallyBound` means unwritten descriptors are legal as long as
the shader doesn't sample them.

Parallel services for storage images, storage buffers when needed. Same template, different
descriptor type. One-line changes.

#### 1c. Context integration

```cpp
class context {
    // ...
    [[nodiscard]] auto bindless_textures() -> bindless_texture_set&;
private:
    std::unique_ptr<bindless_texture_set> m_bindless_textures;
};
```

Created in the ctor after the descriptor heap exists. Bound by the renderer (or the
pass_builder — TBD based on render graph integration).

#### 1d. Pipeline layout integration

Every pipeline that wants bindless textures adds the bindless set layout at set index 2.
`ShaderRegistry::cache()` already creates a slot for `bind_less` — just needs to actually
populate it when the shader uses the bindless array.

### Layer 2 — Usage pattern (shader-side + renderer-side)

Once Layer 1 lands, this is mechanical.

#### Shader declaration

```slang
// set 2 = bind_less
[[vk::binding(0, 2)]] Sampler2D g_textures[];

// Use:
float4 color = g_textures[NonUniformResourceIndex(push.tex_idx)].Sample(uv);
```

#### Renderer flow

```cpp
// Initialization (cold path, once per texture asset):
auto slot = ctx.bindless_textures().allocate(texture.native(), sampler);
cache.insert({ texture_id, slot });

// Record (hot path, per draw):
auto pc = r.shader->cache_push_block("push_constants");
pc.set("tex_idx", slot.index);
rec.push(r.pipeline, pc);
rec.draw(...);
```

No more per-draw descriptor writes for textures. The bindless set is bound once per
frame (or once at pipeline bind; `UpdateAfterBind` means it can even be modified while
bound).

### Layer 3 — GPU-driven rendering (what bindless actually unlocks)

Once textures are bindless, buffers are the next. A typical modern renderer pattern:

- One giant `StructuredBuffer<MaterialData>` in bindless set
- One giant `StructuredBuffer<InstanceData>` in bindless set
- Compute culling builds `DrawIndexedIndirectCommand[]` with `(instance_idx, material_idx)`
  packed in the draw's `firstInstance` or similar
- Single `drawIndexedIndirect` covers everything

This is the goal. Layer 1+2 are prerequisites.

### Layer 4 — Reflection-powered ergonomics (P2996 specific)

This is the "what does reflection buy us" layer. Lives on top of Layer 1, not instead of it.

#### 4a. `slang_mirror<T>` — generate Slang structs from C++

Declare once:

```cpp
struct material_gpu {
    vec3f albedo;
    float roughness;
    float metallic;
    std::uint32_t normal_map_idx;
    std::uint32_t mra_map_idx;
};

// At build time, reflection emits:
//   struct MaterialGpu { float3 albedo; float roughness; float metallic;
//                        uint normal_map_idx; uint mra_map_idx; };
// into a generated .slang file alongside the shader.
```

One source of truth for layout. Adding/removing fields in C++ automatically updates Slang.
Eliminates the #1 source of "mysterious rendering corruption" bugs in bindless systems.

This is a `consteval` code generator that iterates `std::meta::data_members_of(^T)`,
emits Slang syntax, and writes the `.slang` include. Runs as part of the shader build
step; output checked in or regenerated.

#### 4b. Typed bindless handles

```cpp
template <typename T>
struct bindless {
    std::uint32_t index = invalid_index;
    explicit operator bool() const { return index != invalid_index; }
};

// Distinct types:
bindless<texture>      tex_slot;
bindless<material_gpu> mat_slot;
bindless<light_gpu>    light_slot;

// Can't accidentally pass a material index where a texture is expected.
```

Push-constant packing uses reflection to serialize struct → bytes, so the shader-side
receives `push_constants { bindless<texture> albedo; bindless<material_gpu> mat; }`
and the CPU binding matches automatically.

#### 4c. Automatic descriptor-write for struct arrays

A `bindless_buffer<T>` template:

```cpp
template <typename T>
class bindless_buffer {
public:
    auto add(const T& value) -> bindless<T>;       // uploads + returns slot
    auto update(bindless<T> slot, const T& value) -> void;
    auto free(bindless<T> slot) -> void;
};
```

Internal impl reflects on `T` to:
- Compute GPU layout (scalar/std140/std430 padding)
- Generate per-field memcpy or gpu::memcpy calls
- Static-assert that `sizeof(T)` matches the reflected layout

#### 4d. Compile-time layout cross-check

At build time, reflect on every C++ struct used in bindless buffers, emit a `static_assert`
that `sizeof(T)` and per-field offsets match what Slang computes. Layout drift surfaces at
compile time, not as rendering artifacts.

#### 4e. Resource lifetime (RAII)

```cpp
class bindless_texture_handle {
    bindless<texture> m_slot;
    bindless_texture_set* m_owner;
public:
    ~bindless_texture_handle() { if (m_owner) m_owner->release(m_slot); }
    // move-only
};
```

Reflection doesn't strictly need to be involved, but the ergonomics of "reflect on a
resource type, generate the appropriate RAII wrapper" matches what clang-p2996 enables.

## Execution order

### Phase 1 — foundation (no reflection needed)

Goal: one renderer converted, proves the pattern works.

1. **Binding-flag propagation in ShaderRegistry.** Add `DescriptorSetLayoutBindingFlagsCreateInfo`
   pNext chaining + `eUpdateAfterBindPool` for bindless-typed sets. Gate on the set type.
2. **`bindless_texture_set` service.** Free-list allocator + descriptor writes. Standalone,
   unit-testable.
3. **`gpu::context::bindless_textures()` accessor + ctor wiring.** One liner at the ctor,
   one member.
4. **Shader layout authoring.** Pick a simple shader (sprite in UiRenderer), add
   `[[vk::binding(0, 2)]] Sampler2D g_textures[];`, change sampling to use
   `NonUniformResourceIndex(pc.tex_idx)`.
5. **Convert UiRenderer.** Replace per-frame `writer.image("spriteTexture", ...)` calls
   with `ctx.bindless_textures().allocate(...)` at load time, push `.index` per draw.
   Keep the old persistent-set path working until the bindless path is validated.

Exit criterion: UiRenderer draws correctly with the bindless path; old writer.image call
for spriteTexture removed.

### Phase 2 — replicate the pattern

Goal: everywhere a renderer binds a texture, it uses bindless instead.

6. **Convert ForwardRenderer material textures.** Biggest win; materials are currently
   bound per-draw.
7. **Convert DepthPrepass, CullCompute, etc.** where applicable.
8. **Sampler factor-out.** Most renderers only need 2–4 samplers; samplers can share a
   bindless sampler array too (or just live alongside in the same bindless_texture_set
   as combined-image-samplers — which is what 1b already assumes).

Exit criterion: no renderer calls `writer.image()` for texture binding in hot paths.

### Phase 3 — reflection-powered ergonomics

Goal: CPU/GPU layout sync becomes a compile-time concern.

9. **`slang_mirror<T>` code generator.** `consteval` function that emits Slang struct text
   for a given C++ type. Hook into the Slang build step (the `.slang.in` → `.slang`
   generation, or a pre-compile step).
10. **Typed `bindless<T>` handle.** Replace raw `u32 tex_idx` with `bindless<texture>` etc.
    Shader side: same u32; C++ side: type-safe.
11. **`bindless_buffer<T>`.** Template over POD types. First user: material palette in
    ForwardRenderer. Remove `material_palette_buffers` hand-managed by the renderer.
12. **Compile-time layout assertions.** `static_assert(gpu::layout<T>::matches_slang<T>)`
    wherever a bindless_buffer is instantiated. Layout drift becomes a compile error.

Exit criterion: adding a new material type means adding a C++ struct and one line, not
N lines across renderer + shader.

### Phase 4 — GPU-driven rendering (the actual payoff)

Goal: one `drawIndexedIndirect` covers everything.

13. **Instance buffer bindless.** `StructuredBuffer<InstanceData>` at bindless index.
14. **Compute-driven culling writes draw streams with material/texture indices packed.**
15. **Renderer becomes a pass that binds three things (bindless set, indirect buffer,
    pipeline) and issues one draw.**

## What to leave alone

- **Vertex / index buffers.** They're already bound differently from descriptors
  (via `bindVertexBuffers` / `bindIndexBuffer2KHR`). Leave them. Later, mesh shader
  path can bindless those too, but that's a separate project.
- **Push constants.** Still the right transport for per-draw indices. Don't move those
  to bindless.
- **Per-frame UBOs (camera, light data).** Small, hot, fit fine in the persistent
  descriptor set. Not every resource wants to be bindless.

## Risks and open questions

- **Driver / validation layer maturity around `UpdateAfterBind` + `descriptor_buffer`
  extension.** Both are mature separately; less testing together. Expect to hit at
  least one validation warning in Phase 1 that clarifies a layout flag requirement.
- **Bindless slot lifetime.** First slice uses explicit `release()`. RAII wrapper comes
  in Phase 3. For hot-reloaded textures: reuse the same slot (update-after-bind makes
  this legal) — requires the texture loader to know its bindless slot. TBD in Phase 2.
- **Capacity sizing.** 4096 sampled images = 4096 × descriptor_size bytes in the
  persistent region. At 64 bytes/descriptor that's 256 KB — fine. Larger capacities
  (16K+) should work but verify against `maxDescriptorSetUpdateAfterBindSampledImages`.
- **Multi-frame-in-flight safety.** `UpdateAfterBind` relaxes the "no writes while bound"
  rule, but the *slot* lifetime still matters — if frame N reads slot X and the slot gets
  recycled in frame N+1 before N completes, reads get torn. Solution: free list delays
  release by `max_frames_in_flight` (retire-queue style). Implement in Phase 1 or defer
  until a concrete bug appears.
- **Slang-side `runtime-sized array` support.** Slang compiles `Sampler2D g[]` to the
  right SPIR-V capability (`SampledImageArrayNonUniformIndexingEXT`). Verified in
  P2.3 — feature is enabled. First-slice shader test will confirm the emission.

## Decision points to resolve before Phase 1

1. Is bindless set bound **once per command buffer** (early in the frame) or **per
   pipeline bind** (inline with `bind_pipeline`)? Once per command buffer is simpler;
   inline is more flexible if different renderers want different bindless sets.
   **Recommendation: once per command buffer.** Keep it simple.
2. Release strategy: **explicit `release()`** (Phase 1) or **retire queue** (freeing
   delayed by frames-in-flight)? Retire queue prevents use-after-free. Small cost,
   clear benefit. **Recommendation: retire queue from day 1.**
3. Capacity: start at 4096 sampled images; bump when a project hits the limit.

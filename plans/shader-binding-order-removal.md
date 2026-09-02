# Shader binding order removal + pre-bindless sweep

Goal: stop hand-numbering shader bindings. `shaders::binding<Set, Slot>` is a pre-bindless
descriptor-set artifact whose only surviving job is to be a sort key, and the numbers are a global,
unenforced namespace that has now produced the same misleading compile error three times. Replace the
key with position in the `type_pack`, delete the descriptor-set layer it was feeding, and clear out
the other idioms from the same era that outlived their reason.

## Current state (verified against source, 2026-08-31)

`shaders::binding<Set, Slot>` (`Gpu/Shader/ShaderCodegen.cppm:122`) has exactly three consumers:

1. **`sorted_pack_bindings`** (`ShaderCodegen.cppm:766`) sorts a pack by `(set, slot)`; `emit_pack_bindings`
   then emits `public uniform uint <name>_idx;` per binding in that order into the generated Slang.
   Slang gathers those free-standing uniforms into `globalParams`.
2. **`binding_args_aggregate`** (`GpuRecord/PipelineBuilder.cppm:56`) sorts the *same* pack by the *same*
   key inside a `consteval` block and `define_aggregate`s the C++ struct that `recording_context::push_data`
   memcpy's raw as that uniform block. Bindings with `descriptor_count > 1` are skipped (they emit no
   uniform field).
3. **`build_family_sets` → `family_set` → `binding_use`** (`ShaderCodegen.cppm:640`, `PipelineBuilder.cpp:819`
   and `:914`) builds descriptor-set layouts.

**Consumer 3 is dead.** `shader_program_create_info::bindings` (`GpuBackend/ShaderProgram.cppm:31`) is
written by both `build_compute_program` and `build_graphics_program` and never read. `binding_use`
appears 4 times in the entire repo — the type declaration (`GpuBackend/Pipeline.cppm:84`), the field,
and the two construction sites. Neither backend touches it: `grep -i "descriptorpool|descriptorsetlayout|bindDescriptorSets|updateDescriptorSets|writeDescriptorSet"`
over `Source/Vulkan`, `Source/Dx12`, `Source/GpuBackend`, `Source/GpuRecord` returns nothing. The
translation function `to_vk(gpu::descriptor_type)` (`Vulkan/Types.cppm:479`) has no call sites.

What actually reaches Vulkan is `build_bindless_mappings` (`Vulkan/BindlessMapping.cppm:225`), which
**reflects the SPIR-V** for `globalParams` and maps whatever set/binding *Slang* assigned to
`ePushData` at `pushDataOffset = push_offset_start`. Our numbers never leave the C++ side. DX12 goes
through `.Handle idx` in push data with no set/slot concept at all.

So consumers 1 and 2 are the whole story, and their only requirement is **that they agree with each
other**. The values are arbitrary. `set` is not a descriptor set — it is a sort tier: `shaders::meshlet::*`
sits at set 1 purely so it orders after everything in set 0, and `shaders::bindless::*` at set 2 so it
orders last.

### What this costs today

- **Collisions do not report as collisions.** Two bindings tying on the sort key make the aggregate's
  field order stop matching any hand-written designator sequence, so you get
  `designator order for field 'X' does not match declaration order` — which reads as a mistyped
  initialiser list and sends you to the wrong file. Recorded twice in
  `gpu-bindless-render-graph-gotchas`, most recently 2026-08-29.
- **Slots must be reserved cross-file.** `atmosphere::atmosphere_ubo` is `<0,7>` and appears in both
  `forward_bindings` and `raymarch_bindings`, so 7 is burned in every pipeline touching either — and
  it is not declared in the `.cpp` you would be reading while picking a slot.
- **Designators must be written in `(set, slot)` order, not pack order.** Two orderings for one list.
- **There is already a live duplicate.** `standard_3d::instance_data_buffer` (`SharedShaders.cppm:141`)
  and `meshlet::meshlet_bounds_buffer` (`SharedShaders.cppm:128`) are both `<1,4>`. Harmless only
  because `standard_3d::shader_binding_types` (`SharedShaders.cppm:147`) and
  `standard_3d::instance_data_buffer` are **both dead** — neither is referenced anywhere.

Scope of the annotation: 141 sites across 23 files (21 renderers + `SharedShaders.cppm` +
`Physics/VBD/GpuSolver.cpp`). Only **9** of the 141 carry `binding<>` as their sole annotation; the
other 132 already declare their kind via `ssbo_readonly` / `texture2d` / `sampler_state` / etc.

## Phase 1 — key both sorts on pack position

Replace `(set, slot)` with index in the `type_pack`. Declaration order is unique by construction,
local to one pipeline, and already the order the designators get written in.

Both generators are per-entry, so per-pipeline agreement is all that is needed; a binding type shared
between two pipelines landing at different offsets in each is fine and already effectively the case.
Every entry in the repo passes a single pack (`gpu::bindings<one_pack>`), and `entry_bindings_pack_t`
only reads template argument 0 anyway, so the variadic `bindings<Packs...>` path is not exercised.

- `ShaderCodegen.cppm:766` `sorted_pack_bindings` collapses to the identity over the pack (keep the
  name or fold it into `emit_pack_bindings`).
- `PipelineBuilder.cppm:56` `binding_args_aggregate`'s `consteval` block iterates the pack in order,
  still skipping `descriptor_count_v > 1`.

Safe by construction elsewhere: `register_one_bindless` (`RecordingContext.cppm:381`) already resolves
aggregate members by **name** via `bindless_member_for`, not by position, and iterates the pack in pack
order. Name uniqueness within a pack is already required — `define_aggregate` would reject two members
with the same identifier — so nothing new is assumed.

### The one real design question

`is_shader_binding` is currently defined as "has a `binding<>` annotation" (`ShaderCodegen.cppm:145`
via `find_binding_type`). Removing the annotation removes the discriminator, and it is load-bearing:
`emit_one_binding` / `emit_one_user_type` use it to separate resource bindings from shader structs
and enums.

Recommended: a parameterless tag, e.g. `shaders::uniform`, on the 9 bare declarations:
`atmosphere_ubo` (`AtmosphereRenderer.cppm:55`), `cloud_ubo` and `cloud_shadow_ubo`
(`CloudRenderer.cppm:64`, `:83`), `cull::frustum_ubo` (`CullComputeRenderer.cpp:39`),
`light_culling::culling_params` (`LightCullingRenderer.cpp:40`), and the four `camera_ubo`
redeclarations (`DepthPrepassRenderer.cpp:27`, `ForwardRenderer.cpp:39`, `OitRenderer.cpp:31`,
`SharedShaders.cppm:136`). Every other
binding is already identified by its existing resource tag, so `is_shader_binding` becomes "has any
resource tag" — a disjunction of the tags `descriptor_type_of` already switches on. `find_binding_type`
and the `binding` template are then deleted.

Rejected alternative: infer "binding" from membership in the `gpu::bindings<>` pack. It reads cleaner
at declaration sites but `is_shader_binding` is used as a template *constraint* (`emit_slang_binding`,
`descriptor_type_of`, `descriptor_count_of`, `descriptor_access_of`), so the type would have to know
independent of any pack.

### Secondary cleanup enabled here

While the bare declarations are being touched: two spellings of the same thing exist. Most bindings
say `using element = X;`, but `atmosphere_ubo` (`AtmosphereRenderer.cppm:55`), `cloud_ubo` and
`cloud_shadow_ubo` (`CloudRenderer.cppm:64`, `:83`) declare their members inline, which
`emit_slang_binding`'s final `else` branch handles as a separate code path duplicating the `element`
branch verbatim. Converting those to `using element =` deletes the duplicated branch. Worth doing in
the same pass, not worth a separate one.

Naming note, deliberately **not** in scope: `_ubo` is a lie — the codegen emits
`StructuredBuffer<T>` read at `[0]` and `descriptor_type_of` returns `storage_buffer`, because a real
uniform buffer through the heap hangs the GPU (`gpu-bindless-uniform-buffer-hang`). Renaming ~30
symbols is a mechanically separate, higher-churn change; flagging it rather than bundling it.

## Phase 2 — delete the dead descriptor-set layer

Once Phase 1 lands, nothing computes set/slot, so the whole layer goes:

- `shaders::family_binding`, `shaders::family_set`, `build_family_sets`, `build_combined_family_sets`
  (`ShaderCodegen.cppm:183-205`, `:640-702`)
- `compute_entry_pod::build_family_sets_fn` / `graphics_entry_pod::build_family_sets_fn`
  (`PipelineBuilder.cppm:184`, `:244`) and their two assignment sites (`:452`, `:676`)
- the `pack_bindings` construction in `build_compute_program` / `build_graphics_program`
  (`PipelineBuilder.cpp:819-830`, `:914-925`) and the two `assert(pod.build_family_sets_fn, ...)`
- `gpu::binding_use` (`GpuBackend/Pipeline.cppm:84`) and `shader_program_create_info::bindings`
  (`GpuBackend/ShaderProgram.cppm:31`)
- `gpu::descriptor_binding_desc` (`GpuBackend/Pipeline.cppm:76`) — only referenced by `family_binding`
- `to_vk(gpu::descriptor_type)` (`Vulkan/Types.cppm:479`) — no callers

`descriptor_type` and `descriptor_access` themselves **stay**. They are live and load-bearing:
`binding_access_contribution` and `register_one_bindless` (`RecordingContext.cppm:352-410`) use them to
derive access flags and image layout transitions for the auto-barrier, and `BindlessMapping.cppm`
uses `descriptor_type` for its own SPIR-V reflection. `descriptor_type::uniform_buffer` becomes
unreachable from our codegen but is still produced by that reflection path, so the enumerator stays.

`descriptor_count_of` survives too, but its only remaining job is "is this the 2048-entry texture
table" (the `count > 1` skip in `binding_args_aggregate` and `register_one_bindless`). Worth
considering collapsing to a named predicate — `is_bindless_table<T>` — rather than leaving a count
that no longer describes a descriptor count. Low priority; note it, decide during execution.

The per-binding `stage_flags all_stages` in `append_family_binding` (`ShaderCodegen.cppm:653`) dies
with the function. Real stage information already comes from the call site
(`register_bindless_usage(args, bound_shader_stages())`).

## Phase 3 — the push-data budget guard

Not strictly binding-order work, but it is the same subsystem and the same class of silent failure,
and the missing piece is now trivially available.

`descriptor_heap_properties::max_push_data_size` (`GpuBackend/Bindless.cppm:33`) is queried from
`maxPushDataSize` (`Vulkan/Device.cpp:2117`) and **never read**. Meanwhile `push_data(pc, 0)` +
`push_data(args, sizeof(pc))` (`RecordingContext.cppm:444`) can overflow the device limit with no
diagnostic — the VBD solver already did this once (176 B push + 88 B indices = 264 > 256) and it
surfaced as a device-lost, costing days.

Add the check in `build_compute_program` / `build_graphics_program`, where both halves are known:
`pod.push_constant_size + sizeof(binding_args<Pack>) <= dev.descriptor_heap_props().max_push_data_size`.
It has to be a runtime assert rather than a `static_assert` because the limit is device-queried, but
it fires at pipeline build, which is early and loud.

Four sibling fields in the same struct are also write-only mirrors of VK limits with no reader:
`max_sampler_heap_size`, `max_resource_heap_size`, `max_embedded_samplers`, `sparse_descriptor_heaps`.
Not pre-bindless — bindless-era plumbing that never got wired. Leave them; note them.

## Phase 4 — other survivors from the pre-bindless era

Each needs individual verification, so this is a list of candidates, not a sweep.

**`buffer_flag::uniform` (`GpuBackend/Buffer.cppm:13`) — delete.** Zero users. The only two references
are `to_vk` (`Vulkan/Types.cppm:677`) and an assert whose entire purpose is to forbid combining it
with `bindless` (`Gpu/Device/Device.cpp:751`, message: *"a uniform-usage bindless buffer writes a
descriptor no shader can read (garbage matrices -> NaN -> GPU hang). Drop buffer_flag::uniform."*).
A flag kept alive solely by a guard against using it. Deleting it deletes the guard, the translation,
and the `eUniformBuffer` branch in `pick_memory_properties` (`Vulkan/Device.cpp:2415`) — which is worth
its own look, since it prefers a different memory-property fallback chain (`eHostVisible` third) than
the storage branch (`eDeviceLocal` third), so anything that had reached it would silently land in host
memory.

**`recording_context::sample_image` (`RecordingContext.cppm:157`) — audit its 10 call sites.** It does
exactly what `register_one_bindless` now does for a sampled-image binding: `note_touched` +
`transition_image_for_binding(sampled, ...)` (`RecordingContext.cpp:207`). Where the image also has its
own `texture2d` binding in the same pass's args, the call is redundant — confirmed for
`ForwardRenderer.cpp:338` (`gi_state.irradiance_atlas` is passed as `.gi_atlas = gi_atlas_slot` at
`:403`, and `gi_atlas` is a `texture2d` binding). Where the image is reached through
`bindless::textures[idx]` with the index in push constants (`CaptureRenderer.cpp:438`, the UI and
meshlet material paths), the graph genuinely cannot see it and the call is **load-bearing** — keep it.
One subtlety at the Forward site: the call sits before an early `co_return` on
`normal_batches.empty()`, so deleting it changes behaviour in the no-draw case. Verify per site; do
not bulk-delete.

**`shaders::standard_3d::shader_binding_types` and `standard_3d::instance_data_buffer`
(`SharedShaders.cppm:141`, `:147`) — delete.** Dead. Nothing references either. Their removal also
retires the `<1,4>` duplicate noted above.

**Three independent redeclarations of the instance buffer.** `forward::instance_data_buffer` `<1,5>`,
`depth_prepass::instance_data_buffer` `<1,5>`, `oit::instance_data_buffer` `<1,5>` — the same
`ssbo_readonly` of `common::instance_data`, declared three times with hand-coordinated slots so they
would not collide with the shared meshlet bindings. Once slots are gone, the coordination reason is
gone and these collapse to one shared declaration next to `meshlet::*`. Judgment call on whether that
is one binding or three that happen to match; check `depth_prepass`'s `<1,6>` `prev_vertices_buffer`
first, since it suggests the packs are not actually identical.

## Verification

No unit surface here — this is a codegen ABI change where a mismatch between the two generators
produces a GPU fault, not a compile error. So:

1. The static guards that already exist keep working and are the first line:
   `binding_args_layout_is_flat<Pack>()` (`PipelineBuilder.cppm:58`) still asserts every arg field is
   4 bytes, and any designator/field-order mismatch after the change is a hard compile error at every
   `dispatch<Entry>` / `push_bindings<Entry>` call site — 132 bindings across 22 renderers means the
   compiler checks the whole surface.
2. Convert one renderer first: `BloomRenderer` — 7 bindings over two packs
   (`downsample_bindings`, `upsample_bindings`), no shared binding types, no cross-file slot
   dependencies, and both packs are already written in slot order so the designators should not move
   at all. Confirm it renders before touching anything shared.
3. `SharedShaders.cppm` last, since `bindless::textures`, `meshlet::*` and `atmosphere_ubo` are the
   cross-pipeline ones.
4. Full-scene visual check plus the GPU VBD parity gate (`gpu-vbd-parity-harness`), because
   `GpuSolver.cpp` is in the binding set and it is the shader closest to the push-data ceiling.

## Order

Phase 1 and 2 are one landing — Phase 2 is only deletable once Phase 1 removes the last producer, and
leaving the dead layer half-wired between commits is worse than a single larger change. Phase 3 is
independent and could land first (it is small and would have caught a real historical bug). Phase 4 is
independent of all of it and can be picked off individually.

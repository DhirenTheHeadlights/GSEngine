Extension Adoption + Boilerplate Removal Plan
==============================================

Audit of what the engine can delete or simplify by turning on extensions
the GPU (Intel Arc 140V, Vulkan 1.4.341) already supports. See
`docs/vulkan_extensions.md` for the device's full capability list.

Goal: shrink the GPU module by ~800–1000 lines, collapse the image-layout
state machine, replace pipeline-state objects with shader objects, drop
the staging-buffer path for load-once textures, and clean up several
swapchain/descriptor papercuts.

Execution order (suggested)
---------------------------

1. `VK_KHR_unified_image_layouts` — biggest surface, unlocks §3/§4/§6.
2. `VK_EXT_host_image_copy` — collapses `Resources/Image.cpp`.
3. `VK_EXT_mutable_descriptor_type` — small, isolated.
4. `VK_KHR_swapchain_maintenance1` + swapchain wrapper cleanup.
5. `VK_EXT_shader_object` — biggest blast radius; subsumes EDS3.
6. Render-graph hash-map rewrite + bloom barrier fix (independent).

---

1. VK_KHR_unified_image_layouts  — DONE
---------------------------------------

All images live in `GENERAL`; only the swapchain transitions to/from
`PRESENT_SRC_KHR`. The whole layout state machine collapses.

**Extension status**: enabled conditionally in `Vulkan/Device.cpp`
(detect → feature query → log → enable). Drivers that expose
`VK_KHR_unified_image_layouts` (e.g. NVIDIA RTX 50 series) get the
zero-cost-GENERAL perf guarantee. Drivers that don't (e.g. Intel Arc
140V driver 101.8629, snapshot in `docs/vulkan_extensions.md`) fall
back to standard GENERAL semantics — still legal everywhere in Vulkan,
just without the formal "free" promise. The engine code is identical
either way.

### Delete

- `Engine/Engine/Source/Gpu/Vulkan/Types.cppm:212–223` — drop
  `shader_read_only`, `color_attachment`, `depth_stencil_attachment`,
  `depth_attachment`, `transfer_src`, `transfer_dst`. `image_layout`
  shrinks to `{ undefined, general, present_src, video_encode_src }`.
- `Vulkan/Types.cppm:231` — `image_desc::ready_layout` (unread).
- `Vulkan/Image.cppm:54, 58–60, 66, 199–200` — `m_current_layout`,
  `set_layout()`, `layout()` getter.
- `Vulkan/Commands.cppm:37–38, 477` — `image_barrier::old_layout`,
  `image_barrier::new_layout`, and their `to_vk` call. Image barriers
  carry only stages/access/aspects.
- `RenderGraph.cppm:62–64` — `framebuffer_image_desc::{steady_layout,
  steady_stages, steady_access}`.
- `RenderGraph.cppm:1225–1241` — `pre_frame_transitions()` whole-sale.
- `Vulkan/Device.cpp:10–48` and `Vulkan/Device.cppm:45–48` —
  `transition_image_layout()` whole-sale.
- `TransientPool.cpp:452, 460` — `req.desc.layout` write +
  `transient_image_allocation::layout` field.
- `RenderGraph.cppm:2118–2119` — drop layout fields from the
  barrier-coalesce comparator (more barriers merge for free).

### Simplify

- `Resources/Image.cpp:8–84, 86–330` — `transition_image_async`,
  `transition_image_to`, and the four `upload_image_*` functions lose
  ~6 transition pairs each. Most of these vanish entirely once §3 lands.
- `Device/Frame.cpp:143–157` — acquire barrier becomes
  `PRESENT_SRC → GENERAL`.
- `Device/Frame.cpp:203–217` — present barrier becomes
  `GENERAL → PRESENT_SRC`. Only surviving layout transitions in the
  engine.
- `RenderGraph.cppm:753–924` — swapchain capture/blit: 4 explicit
  layout pairs collapse to GENERAL with one PRESENT_SRC bookend.
- `Vulkan/Swapchain.cpp:211` — `depth_image.set_layout(undefined)`.

### Must stay

- The `PRESENT_SRC ↔ GENERAL` pair around acquire/present.
- Barrier *stages* and *access* masks — those are sync, not layout.

---

2. VK_EXT_shader_object  — DONE
-------------------------------

Graphics + compute pipelines became `VkShaderEXT` objects bound via
`vkCmdBindShadersEXT`. Mesh shaders also use `VkShaderEXT` (task/mesh
stages). Ray-tracing pipelines stay on `VkPipeline` (RT is its own
type system, not unified).

**Extension status**: hard-required (NVIDIA RTX 50 / AMD only).
`Vulkan/Device.cpp` asserts on absence of `VK_EXT_shader_object`,
`VK_EXT_extended_dynamic_state3`, and `VK_EXT_vertex_input_dynamic_state`.
Intel Arc 140V driver doesn't expose shader_object — engine will fail
device-create on Intel until they ship it.

### New types
- `vulkan::shader_object` (`Vulkan/ShaderObject.cppm`) — wraps a single
  `vk::raii::ShaderEXT`. `create_linked()` is the multi-stage path
  used for full programs.
- `vulkan::shader_program` (`Vulkan/ShaderProgram.cppm`) — owns a
  `pipeline_layout` + N `shader_object`s + dynamic-pipeline-state +
  active-bindings metadata. Replaces `vulkan::pipeline` for
  graphics/compute/mesh.
- `gpu::dynamic_pipeline_state` (in ShaderProgram.cppm) — captures all
  the per-draw state that used to be baked into `VkPipeline`: topology,
  polygon mode, cull/front-face, depth state, blend state, vertex input.

### New commands API
- `bind_shaders(stages, handles)` / `unbind_shaders(stages)` — wrap
  `vkCmdBindShadersEXT`.
- ~22 dynamic state setters: `set_topology`, `set_polygon_mode`,
  `set_cull_mode`, `set_front_face`, `set_depth_test_enable`,
  `set_depth_write_enable`, `set_depth_compare_op`,
  `set_depth_bias_enable/set_depth_bias`, `set_depth_clamp_enable`,
  `set_depth_bounds_test_enable`, `set_stencil_test_enable`,
  `set_rasterizer_discard_enable`, `set_primitive_restart_enable`,
  `set_rasterization_samples`, `set_sample_mask`,
  `set_alpha_to_coverage_enable`, `set_alpha_to_one_enable`,
  `set_logic_op_enable`, `set_color_blend_enable/equation/write_mask`,
  `set_blend_constants`, `set_vertex_input`, `set_line_width`.
- `set_viewport`/`set_scissor` swapped from `setViewport`/`setScissor`
  to `setViewportWithCount`/`setScissorWithCount` (shader_object
  requires the WithCount dynamic state).
- `vulkan::pipeline_state_cache` lives on `recording_context`
  (non-mutable, per-pass). `apply_dynamic_state()` skips redundant
  emissions within a pass; cache invalidates naturally between passes
  because each pass gets a fresh `recording_context`.

### Builder rewrite
- `build_compute_program()` and `build_graphics_program()` in
  `Resources/PipelineBuilder.cppm` replaced `build_compute_pipeline()`
  / `build_graphics_pipeline()`. Same POD-tag input
  (`compute_entry_pod`, `graphics_entry_pod`), `shader_program`
  output instead of `pipeline`.
- `gpu::pass(...).pipeline(shader_program&)` is the call site (no
  source changes for renderers beyond `gpu::pipeline` →
  `gpu::shader_program` and `build_*_pipeline` → `build_*_program`).
- 28 renderer files migrated.

### Dead code deleted (Phase 6)
- `vulkan::pipeline` class (`Vulkan/Pipeline.cppm` + `.cpp` — whole
  files gone).
- `vulkan::shader_module` (`Vulkan/ShaderModule.cppm` — whole file
  gone, shader_object compiles SPIRV directly).
- `build_compute_pipeline`, `build_graphics_pipeline`,
  `build_compute_pipeline_impl`, `create_compute_pipeline_from_spirv`.
- `recording_context` overloads taking `const gpu::pipeline&`:
  `push<T>`, `bind`, `bind_descriptors`, `commit`.
- `commands::bind_pipeline`.
- `shader_stage_create_info`, `graphics_pipeline_create_info`,
  `compute_pipeline_create_info`.
- Module exports: `:vulkan_pipeline`, `:vulkan_shader_module` dropped
  from `Gpu.cppm`; `pipeline`/`shader_module` aliases dropped from
  `Aliases.cppm`.

### Must stay
- `Vulkan/PipelineLayout.cppm` — shader objects still bind through
  `VkPipelineLayout`.
- `Resources/Pipeline.cppm` partition — contains
  `typed_push_constants<T>`, not the deleted class. Kept as-is.

---

3. VK_EXT_host_image_copy  — DONE
---------------------------------

For load-once textures, the staging-buffer + transfer-queue path
collapses to two host-side calls: `vkTransitionImageLayoutEXT` (move
to GENERAL) then `vkCopyMemoryToImageEXT` (write pixels). Fully
synchronous, returns a ready `sync_token` immediately.

**Extension status**: enabled conditionally in `Vulkan/Device.cpp`
(detect → feature query → log → enable). Both target drivers
(Intel Arc 140V, NVIDIA RTX 50 series) expose the extension. Drivers
without it fall back to the staging-buffer path automatically.

**Wired up:**
- `Vulkan/Types.cppm` — added `image_flag::host_transfer` (maps to
  `VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT`).
- `Vulkan/Image.cppm` — `basic_image::create` auto-adds `host_transfer`
  to any image that already has `transfer_dst` usage when the device
  supports the feature. Existing callers stay unchanged.
- `Resources/Image.cpp` — each of the four upload helpers branches on
  `dev.vulkan_device().host_image_copy_enabled()`:
  - **Fast path**: `host_transition_to_general` then `host_copy_layers`.
    No staging buffer, no transient queue, no fence, no semaphore.
  - **Fallback path**: existing staging-buffer + transient submit code.
- `transition_image_async` / `transition_image_to` deliberately do NOT
  branch — they take raw handles or arbitrary images that may not have
  `host_transfer` usage (e.g. atmosphere/bloom storage images).

### Stayed

- `Resources/Buffer.cpp` — buffer uploads still go through staging.
- GPU→GPU mip generation, equirect→cube, blit chains.
- Transient queue infrastructure (still used by buffer uploads, render
  passes, compute scratch).

---

4. VK_KHR_dynamic_rendering_local_read — SKIPPED (no fit)
---------------------------------------------------------

The original audit listed three candidates in `AtmosphereRenderer.cpp`
(lines 436/458/472). Rescoped after shader_object work: those are all
**compute→compute** dispatches (`transmittance` → `multiscatter` →
`sky_view`/`ap_volume`). LocalRead is for **graphics passes that sample
a color/depth target written earlier in the same render pass** —
classic deferred lighting reading a G-buffer. Compute shaders can't
sample render-pass attachments at all.

Engine has zero real LocalRead candidates today:
- Forward shading is single-pass, no intra-pass resample of the
  framebuffer.
- Atmosphere does compute LUT generation + one final
  `sky_raster_pass` that only samples *pre-computed* LUTs, not a
  current-pass attachment.
- Bloom is a compute pyramid (the proper fix landed in §4 below as
  a single-pass chain with `compute_to_compute` barriers).

**Revisit when/if deferred lighting ships.** With G-buffers it
becomes a natural fit; until then the extension would buy nothing.

### Bloom barrier explosion (bug-grade)  — DONE

- `Graphics/Renderers/BloomRenderer.cpp` — downsample loop was opening
  a fresh `co_await gpu::pass<downsample_pass>()` per mip. Collapsed
  to a single pass with `rec.barrier(compute_to_compute)` between
  dispatches. Each removed pass also removed an O(prior_passes) scan
  in `append_prev_pass_barriers`. Net: ~6 fewer passes per frame on
  bloom + much less barrier-bookkeeping.
- Also extended `barrier_scope::compute_to_compute` to include
  `shader_sampled_read` in `dst_access` (bloom samples the previous
  mip as `combined_image_sampler`, not as a storage image).

### Render-graph cleanups  — DONE

- `RenderGraph.cppm::append_prev_pass_barriers` was O(prior_passes ×
  prev_accesses × cur_accesses). Replaced with per-queue hash maps:
  `latest_writes[queue][resource_ptr]` and
  `reads_since_write[queue][resource_ptr]`. Per-pass cost drops to
  O(cur_reads + cur_writes). Latest-writer-only is sufficient because
  writes are ordered (latest dominates earlier ones).
- `RenderGraph.cppm::append_barrier_for_resource` now early-outs on
  pure read→read barriers (no write on either side, same stages).
  These are no-ops in Vulkan — saves coalesce work.

### Not applicable

- `VK_EXT_attachment_feedback_loop_layout` — confirmed nothing samples
  a texture it's simultaneously writing to.

---

5. VK_EXT_mutable_descriptor_type  — DONE
-----------------------------------------

**Extension status**: enabled conditionally in `Vulkan/Device.cpp`
(detect → feature query → log → enable). Both target drivers support
it. The actual mutable-binding usage stays for a future commit when a
specific binding can benefit (e.g. bindless heap holding multiple
types); the extension is on so that future work is unblocked.

**Done now: per-type descriptor sizes collapsed.**
- `Vulkan/Types.cppm` — dropped the 7 per-type descriptor-size fields
  from `descriptor_buffer_properties` (`uniform_buffer`,
  `storage_buffer`, `sampled_image`, `sampler`,
  `combined_image_sampler`, `storage_image`, `input_attachment`,
  `acceleration_structure`). Only `offset_alignment`,
  `push_descriptors_supported`, `bufferless_push_descriptors` remain.
- `Vulkan/Device.cpp::query_descriptor_buffer_properties` — dropped
  the per-type query+log calls.
- `Vulkan/Bindless.cppm` — `m_descriptor_size` is now derived from
  `(layout_size - binding_offset) / capacity` instead of pulling
  `props.combined_image_sampler_descriptor_size`.
- `Resources/Descriptors.cppm::build_push_writer_from_family` —
  replaced the `descriptor_size_for(dt)` switch with a sorted-by-offset
  gap computation: per-binding size = `(next_offset - this_offset) / count`,
  with `layout_size` as the upper bound for the last binding. Works
  identically for non-mutable bindings and is the correct approach for
  future mutable bindings.
- `Resources/Descriptors.cppm::descriptor_writer::commit` — same
  treatment applied to the persistent-set write path.

### Deferred

- `VkMutableDescriptorTypeCreateInfoEXT` plumbing in
  `Vulkan/DescriptorSetLayout.cpp` — waits for a concrete consumer.
- Merging `m_storage_image_infos` + `m_combined_sampler_infos` in
  `Resources/Descriptors.cppm` — waits for a consumer too.
- Bindless heap collapse — waits for a binding that needs more than
  one descriptor type.

---

6. VK_EXT_swapchain_maintenance1  — DONE
----------------------------------------

**Extension status**: enabled conditionally in `Vulkan/Device.cpp`.
NVIDIA RTX 50 series supports it; Intel Arc 140V doesn't (driver
falls back to the existing recreate-on-resize path).

### Dead code dropped
- `gpu::swap_chain::set_config()` — unused, gone.
- `gpu::swap_chain::generation()` + `m_generation` member — unused, gone.

### Depth image relocated
- `vulkan::swap_chain` no longer owns the depth image. Members
  `m_depth_image`, `depth()` accessor, `clear_depth()`, the constructor
  param, and the depth-creation block inside `create()` all dropped.
- `gpu::swap_chain` now owns the depth image directly. New file-scope
  helper `create_swapchain_depth(device&, extent)` builds it via
  `gpu::image::create()`.
- `gpu::swap_chain::clear_depth_image()` dropped — depth dies with the
  wrapper naturally.
- `Context.cpp` shutdown no longer needs the manual
  `swapchain->clear_depth_image()` call before `swapchain.reset()`.
- 5 callers in `RenderGraph.cppm` and `Frame.cpp` still call
  `swapchain->depth_image()` — same API, same return type, just owned
  one level higher.

### Present mode switching without recreate (`VK_EXT_swapchain_maintenance1`)
- `vulkan::swap_chain::create` pNexts `VkSwapchainPresentModesCreateInfoEXT`
  listing every surface-supported mode (when feature on).
- `vulkan::swap_chain::set_present_mode()` updates the tracked mode.
- `present_info` got a `present_modes` span. `build_vk_present_info`
  pNexts `VkSwapchainPresentModeInfoEXT` per present when populated.
- `frame::recreate_resources` detects "size unchanged, only mode
  changed" → calls `set_present_mode` and returns early (no recreate,
  no `wait_idle`, no sync recreation).

### Deferred destroy + release fences
- `vulkan::swap_chain` got `std::vector<fence> m_release_fences`
  (one per image, created start-signaled when maintenance1 is on).
- New methods: `release_fence(image_index)`, `wait_release_fences(dev)`,
  `reset_release_fence(dev, image_index)`.
- `create()` now takes an optional `old_swapchain` handle and passes
  it via `create_info.oldSwapchain`.
- `present_info` got a `release_fences` span. `build_vk_present_info`
  pNexts `VkSwapchainPresentFenceInfoEXT` per present when populated.
- `gpu::swap_chain::recreate` branches on the feature:
  - **Maintenance1 path**: wait on the swapchain's release fences (not
    `wait_idle`), then `create` the new one with `old_swapchain = old`,
    move-assign. Old swapchain destroyed during the move.
  - **Fallback path**: existing `reset_swapchain()` + create flow.

### Release-fence wait at acquire
- `frame::begin`, after `acquireNextImage` returns image N: if
  maintenance1 is on, wait on `release_fence(N)`. Ensures the
  presentation engine has released the image before we re-use it.

### Stayed (deferred)
- "True deferred destroy" with a retiring list of old swap_chains —
  current impl still synchronously waits on release fences before
  destroying the old swapchain. The retiring list would let the
  destroy happen lazily on a background tick.
- Decoupling `max_frames_in_flight` from `image_count` — would need
  reworking the frame/sync model.
- Pass-through wrapper deletion (Item 2B from the audit) — wrapper
  kept; only the dead `set_config`/`generation` were removed.

---

7. VK_EXT_extended_dynamic_state3  — SUBSUMED BY §2
---------------------------------------------------

Hard-required by §2 (shader_object's dynamic state needs EDS3 setters).
Enabled in `Vulkan/Device.cpp` alongside shader_object. Nothing
separate to do here.

---

8. VK_KHR_maintenance7/8/9 (papercuts)
--------------------------------------

- **MT7** — `Vulkan/Device.cpp:234–244` can batch the `getFeatures2<...>()`
  chains into one query call.
- **MT8** — adopt `VkMemoryBarrier2`-only forms where stages currently
  exist only to satisfy validation.
- **MT9** — drop empty descriptor set 0 if unused; check
  `Vulkan/DescriptorHeap.cppm:100–105`.

---

9. General dead/duplicate code (extension-independent)
------------------------------------------------------

- `Vulkan/Device.cpp:87–91` — `wait_for_fence()` is a 5-line wrapper
  around `waitForFences()`. Inline or kill.
- `Vulkan/Bindless.cppm:93` — `ePartiallyBound` is hardcoded; if it's
  universally true, hoist to the layout factory.
- `Resources/Descriptors.cppm:139–141` — bucketed maps that could be one
  (see §5).
- Per the `enum_formatter` memory note — grep for any hand-rolled
  `to_string(enum)` helpers in the GPU module; reflective
  `std::formatter<E>` should cover them all.

---

Total: roughly 800–1000 lines deletable, plus the layout enum collapse
and several files reduced to thin pass-throughs.

---

10. VK_KHR_present_id + VK_KHR_present_wait  — DONE
---------------------------------------------------

Added after the original audit closed out, scoped as the next
latency-tuning win:

- Per-present monotonic `present_id` populated into
  `VkPresentInfoKHR.pNext → VkPresentIdKHR` (Frame.cpp end_frame +
  Queues.cppm `build_vk_present_info`).
- `begin_frame` waits on the present_id from two frames ago via
  `vkWaitForPresentKHR` (per-frame-slot ring of size
  `max_frames_in_flight`). Wait happens *after* the existing
  in-flight-fence wait, *before* the next acquire.
- Frame ring reset to zeros on swapchain recreate and on
  OOD/suboptimal present so we never wait on a stale ID owned by a
  destroyed swapchain.
- Both extensions enabled conditionally with feature
  `presentWait` requiring `presentId`. Drivers without the
  extensions keep the old fence-only wait path.

Expected: ~1–2ms latency reduction on variable loads. The fence wait
is still required (releases per-frame resources); present wait
adds the actual presentation-timing signal on top.

---

11. Subgroup size control (Vulkan 1.3 core feature)  — DONE
-----------------------------------------------------------

Enabled `subgroupSizeControl` + `computeFullSubgroups` on
`vulkan13_features` (no separate extension — both are core 1.3).
Added per-shader hints reachable from the `gpu::compute_entry<...>`
template:

- `gpu::full_subgroups` — sets
  `VK_SHADER_CREATE_REQUIRE_FULL_SUBGROUPS_BIT_EXT` on the
  `VkShaderCreateInfoEXT`. Tells the compiler the workgroup size is a
  multiple of the subgroup size, unlocking optimizations. Only safe
  when the workgroup size is divisible by every possible subgroup
  size on target hardware (32 on NVIDIA, 32 or 64 on AMD).
- `gpu::required_subgroup_size<N>` — pNext-chains
  `VkShaderRequiredSubgroupSizeCreateInfoEXT` to lock the subgroup
  size. Use sparingly; only meaningful on AMD (wave32 vs wave64) and
  only if profiling shows occupancy is a bottleneck.

Plumbing path: marker → `compute_entry_pod.{required_subgroup_size,
require_full_subgroups}` → `build_compute_program` →
`shader_object_create_info` → `build_vk_shader_create_info` (chains
the pNext when `required_subgroup_size` is set; OR's in the flag
when `require_full_subgroups` is true) → `vkCreateShadersEXT`.

Initial application: `BloomRenderer.cpp` `downsample_entry` and
`upsample_entry` are 8×8 (64 invocations) workgroups — divisible by
both 32 and 64, so `full_subgroups` is safe and lets the SPIR-V
compiler avoid runtime "is the last subgroup partial?" branches.
Expected: small but real (~few %) on bloom passes. Tune other shaders
with the same marker as profiling identifies them.

---

12. VK_EXT_descriptor_heap — IN PROGRESS (Phase A foundation landed)
--------------------------------------------------------------------

Brand-new extension (spec_version=1) that replaces descriptor sets +
pipeline layouts with a D3D12-style "two-heap" model: one resource
heap (images, buffers, AS), one sampler heap. Shaders access via
either explicit `ResourceDescriptorHeap[idx]` syntax or via per-shader
binding-to-heap mappings.

**Driver support verified**: RTX 5090 driver exposes the extension
(`vulkaninfo` reports `descriptorHeap = true`, spec_version=1).
AMD/Intel driver support not yet verified.

### Properties on RTX 5090 (recorded for tuning)

- `samplerHeapAlignment` / `resourceHeapAlignment`: 32 / 32 bytes
- `maxSamplerHeapSize` / `maxResourceHeapSize`: 128 KB / 32 MB
- `minSamplerHeapReservedRange` (no embedded): 512 B
- `minResourceHeapReservedRange`: ~94 KB (`0x17A00`)
- Descriptor sizes: sampler 32, image 32, buffer 16 (bytes)
- `maxPushDataSize`: 256 B
- `maxDescriptorHeapEmbeddedSamplers`: 2032
- `sparseDescriptorHeaps`: true
- `descriptorHeapCaptureReplay`: **false** — RenderDoc/Nsight captures
  may not work cleanly with the new heaps. Real debugging concern.

### Architecture decisions

1. **Coexist with old descriptor_buffer system.** The new heap is
   built alongside the existing `descriptor_heap` (`VK_EXT_descriptor_
   buffer` wrapper). No renderer changes during Phase A. Old system
   still serves all existing draws; new heap waits for migration in
   Phase D.

2. **Use partitioned single resource heap.** One `bindless_resource_
   heap` (4 MB default, ~128 K image slots + ~16 K buffer slots) and
   one `bindless_sampler_heap` (16 KB default, 512 slots). The
   resource heap is internally partitioned: images first (32-byte
   stride), buffers second (16-byte stride). Per-type slot allocators
   return logical indices; engine translates to byte offsets when
   writing descriptors.

3. **Use `HEAP_WITH_PUSH_INDEX` mapping mode** (planned for Phase B).
   Shaders keep their existing `[[vk::binding(N,M)]] Texture2D tex;`
   declarations. Engine auto-generates a per-shader mapping struct
   that translates `(set, binding)` → "read uint32 from push data at
   offset X, that's the heap slot". This avoids rewriting every
   shader to use explicit `ResourceDescriptorHeap[idx]` syntax.

4. **Heaps owned by `gpu::context::data`.** Created conditionally when
   `descriptor_heap_enabled()` is true. Both heaps reset per-frame
   (deferred-release pattern, mirrors `bindless_texture_set`).

5. **Driver-reserved range at start of each heap.** Engine slot 0
   maps to byte offset `reserved_size`, not 0. Hidden inside the
   allocator API — callers just see slot indices starting at 0.

### Phase A — DONE

Files added/changed:
- `Engine/Engine/Source/Gpu/Vulkan/BindlessHeap.cppm` (new)
  - `descriptor_heap_properties` + `query_descriptor_heap_properties()`
  - `bindless_resource_heap` (4 MB, image+buffer slot allocators,
    deferred release, `write_sampled_image` / `write_storage_image`
    / `write_storage_buffer` / `write_uniform_buffer` APIs taking
    `VkImageViewCreateInfo&` directly)
  - `bindless_sampler_heap` (16 KB, takes `VkSamplerCreateInfo` at
    allocate time)
  - `bindless_heaps` aggregator that owns both + provides `bind(cmd)`
- `Vulkan/Device.cpp` + `Vulkan/Device.cppm`: extension probe → query
  feature → conditional enable → `descriptor_heap_enabled()` accessor
- `Vulkan/Commands.cppm`: `bind_resource_heap`, `bind_sampler_heap`,
  `push_data` wrappers around `vkCmdBindResourceHeapEXT` /
  `vkCmdBindSamplerHeapEXT` / `vkCmdPushDataEXT`
- `Context.cpp` + `Context.cppm`: own `std::unique_ptr<vulkan::bindless_
  heaps>` in `gpu::context::data`; create in `run`, reset in
  `shutdown`, tick in `begin_frame`

### Remaining work (Phases B–E)

**Phase B — Push-constant ABI for resource indices.** Define how a
renderer passes its index struct: dedicated `gpu::push_indices<S>`
spec on `compute_entry` / `graphics_entry`. Codegen emits a Slang
struct + a `[[vk::push_constant]]` block. CPU side builds the struct
each draw and calls `cmd.push_data(offset, span(&s, 1))`.

**Phase C — Per-shader `VkShaderDescriptorSetAndBindingMappingInfoEXT`
generation.** Walk each shader's spirv reflection, produce one
`VkDescriptorSetAndBindingMappingEXT` per `(set, binding)` with
`source = HEAP_WITH_PUSH_INDEX`, `heapOffset = image_range_offset OR
buffer_range_offset`, `pushOffset = struct field offset`,
`heapIndexStride = image_descriptor_size OR buffer_descriptor_size`.
Chain into `VkShaderCreateInfoEXT::pNext`. Add a new field to
`shader_object_create_info` for the mapping list.

**Phase D — Migrate renderers one at a time.** Order:
1. `BloomRenderer` (compute, 2 descriptors, simplest)
2. `TonemapRenderer`, `SdfGridRenderer`, `WorldTextRenderer` (simple)
3. `AtmosphereRenderer` (medium, multi-pass)
4. `ForwardRenderer` (most bindings)
5. `UiRenderer`, `PhysicsDebugRenderer`, `CloudRenderer`, etc.

Each renderer: allocate bindless slots on resource creation; build
the per-draw `PushIndices` struct; call `cmd.push_data` + dispatch.

**Phase E — Delete dead descriptor machinery.** `descriptor_writer`,
`descriptor_region`, `allocate_descriptors`, per-shader
`family_layout` set machinery, transient descriptor sub-buffers.
Keep `descriptor_buffer` itself for the *bindless_texture_set* OR
migrate it to the new heap too (decide based on dev experience).

### Open risks

- **No RenderDoc support.** Debugging will rely on log lines +
  Aftermath GPU dumps. Make sure assertions are loud.
- **spec_version=1**. Driver bugs likely. Plan to file repros against
  NVIDIA if behavior diverges from spec.
- **`VkImageDescriptorInfoEXT` wants `VkImageViewCreateInfo*`, not a
  `VkImageView` handle.** Current `vulkan::basic_image` does not
  store its `VkImageViewCreateInfo`. Phase D will either: store the
  create-info on `basic_image`, or have callers reconstruct it at
  bindless registration time. Decided in Phase D, not Phase A.
- **AMD support not verified.** Run `vulkaninfo` on an AMD machine
  before declaring victory. Worst case: keep the old descriptor_buffer
  path as a fallback for non-supporting drivers.

### Overnight progress checkpoint (Phase A complete)

What landed in this session:
- Driver probe + `descriptor_heap_enabled()` accessor on
  `vulkan::device`
- `vulkan::bindless_resource_heap` + `vulkan::bindless_sampler_heap`
  + `vulkan::bindless_heaps` aggregator (BindlessHeap.cppm, new)
- `vulkan::commands::{bind_resource_heap, bind_sampler_heap,
  push_data}` wrappers
- `shader_object_create_info::bindless_mappings` field; pNext-chain
  wiring in `build_vk_shader_create_info`
- `gpu::context::data::bindless_heaps` (created conditionally,
  ticked per-frame, reset on shutdown)
- `:bindless_heap` partition registered in `Gpu.cppm`

What's left (Phase B onward) before any renderer can use bindless:
1. **Slang codegen for push-index struct.** A `gpu::push_indices<S>`
   spec on `compute_entry` / `graphics_entry`; codegen emits a
   `[[vk::push_constant]] struct PushIndices { ... }` and the
   renderer fills it per draw.
2. **Per-shader mapping generation.** Walk the spirv reflection
   `used_bindings(...)` output. For each `(set, binding)`, synthesize
   a `vk::DescriptorSetAndBindingMappingEXT` with `source =
   HEAP_WITH_PUSH_INDEX`, `heapOffset = heap.image_range_offset()`
   or `heap.buffer_range_offset()`, `pushOffset` from the struct
   field offset, `heapIndexStride = image_stride()` or
   `buffer_stride()`. Plumb into `shader_program_create_info` →
   `shader_object_create_info::bindless_mappings`.
3. **Image create-info plumbing.** `vk::ImageDescriptorInfoEXT` needs
   the full `VkImageViewCreateInfo`, not a handle. Decide between:
   (a) cache the create info on `basic_image`, or
   (b) require callers to pass it to `bindless_heap.write_*` at
   registration time.
4. **Renderer migration starting with Bloom.** Allocate slots in
   `system::run`, write descriptors, build `PushIndices` per draw,
   call `cmd.push_data` before each `cmd.dispatch`. Resource heap
   must be bound at the start of each command buffer (likely in
   `render_graph` primary cmd record path).
5. **Delete old descriptor_writer / region machinery once all
   renderers migrated.**

Risks not yet validated:
- Direct `vk::Device::writeResourceDescriptorsEXT` call relies on
  dynamic dispatcher being initialized. Existing
  `descriptor_heap::descriptor()` uses the same pattern, so this
  should work — first build will confirm.
- Designated-initializer for the `ResourceDescriptorDataEXT` union
  (with `VULKAN_HPP_NO_CONSTRUCTORS`). Should work in C++20+ aggregate
  init; first build will confirm.
- Driver bugs at spec_version=1. No way to know without trying.

### Driver bug discovered — feature enable currently GATED OFF

**Symptom**: Enabling `VkPhysicalDeviceDescriptorHeapFeaturesEXT::
descriptorHeap = VK_TRUE` on NVIDIA RTX 5090 (driver as of 2026-05-24)
causes `vkCreateImage` to crash with integer division by zero inside
`nvoglv64.dll`, even when no descriptor-heap APIs are ever called. The
mere presence of the feature flag poisons the driver's image-creation
path.

Stack:
```
nvoglv64.dll!00007ff906dc06fb()     <- DIV/0
nvoglv64.dll!00007ff906dc1e87()
nvoglv64.dll!00007ff906ddb833()
...
vkCreateImage
gse::vulkan::device::create_image
```

**Mitigation (current)**: `Device.cpp` introduces
`descriptor_heap_advertised` (extension + feature both reported by
driver) but hardcodes `descriptor_heap_supported = false` so neither
the extension nor the feature is enabled on the device. The log
records that the driver advertises the feature but we've gated it off.

All scaffolding (heap classes, command wrappers,
`shader_object_create_info::bindless_mappings`, `bindless_heaps`
ownership in `gpu::context::data`) remains built. Re-enable by
deleting the `= false` line and using `descriptor_heap_advertised`
directly once the driver bug is fixed (or if a newer NVIDIA driver
ships before then).

**Next steps deferred** until driver bug is fixed:
- Phases B/C/D/E all blocked.
- Track via NVIDIA developer support, or check vulkaninfo on a
  new driver release every couple of weeks.

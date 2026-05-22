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

1. VK_KHR_unified_image_layouts
-------------------------------

All images live in `GENERAL`; only the swapchain transitions to/from
`PRESENT_SRC_KHR`. The whole layout state machine collapses.

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

2. VK_EXT_shader_object
-----------------------

Graphics + compute pipelines become `VkShaderEXT` objects bound via
`vkCmdBindShadersEXT`. Mesh + RT pipelines stay on `VkPipeline`.

### Delete

- `Vulkan/Pipeline.cppm:15–32` — `graphics_pipeline_create_info`.
- `Vulkan/Pipeline.cppm:34–40` — `compute_pipeline_create_info`.
- `Vulkan/Pipeline.cpp:44–232` — `pipeline::create_graphics()` body.
- `Vulkan/Pipeline.cpp:234–277` — `pipeline::create_compute()`.
- `Vulkan/Pipeline.cpp:65–114` — input assembly / rasterization /
  depth-stencil / viewport state builders.
- `Vulkan/Pipeline.cpp:124–158` — blend attachment state builders.
- `Vulkan/Pipeline.cpp:167–193` — vertex input state builders.
- `Vulkan/Pipeline.cpp:205–222, 261–267` — `VkGraphicsPipelineCreateInfo`
  / `VkComputePipelineCreateInfo` assembly.
- `Vulkan/Pipeline.cppm:97` — `m_pipeline` on the graphics/compute
  branch (keep on mesh/RT).
- `Resources/PipelineBuilder.cppm:96, 240–528, 481–482` — `blend_preset`
  enum + the entire graphics POD template specs.

### Reshape

- `Resources/PipelineBuilder.cppm:63–68, 114–119` —
  `build_graphics_pipeline()` / `build_compute_pipeline()` return
  `VkShaderEXT[]` instead of `VkPipeline`.
- `Vulkan/Commands.cppm:131–134` — `bind_pipeline()` stays for mesh/RT;
  add `bind_shaders()` for graphics/compute. Plus
  `set_vertex_input`, `set_rasterization_state`, `set_depth_test_enable`,
  `set_depth_compare_op`, `set_blend_constants`, `set_primitive_topology`,
  etc.

### Must stay

- `Vulkan/PipelineLayout.cppm` — shader objects bind through a
  `VkPipelineLayout`.
- `Vulkan/ShaderModule.cppm` — SPIR-V compile path.
- Vertex-attribute reflection — moves from pipeline-creation to
  `vkCmdSetVertexInputEXT` setup.

---

3. VK_EXT_host_image_copy
-------------------------

For load-once textures (BRDF LUT, environment cubes, sampled assets)
add `VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT` and call
`vkCopyMemoryToImageEXT`. No staging buffer, no transfer queue, no
layout transitions.

### Delete

- `Resources/Image.cpp:86–139` — `upload_image_2d_async()` body.
- `Resources/Image.cpp:141–204` — `upload_image_layers_async()` body.
- `Resources/Image.cpp:206–260` — `upload_image_2d()` body.
- `Resources/Image.cpp:262–330` — `upload_image_layers()` body.
- Inside each: staging-buffer alloc, `copy_buffer_to_image`, two
  transitions, `.retain(std::move(staging))`, transient submit.

### Shrink

- `Device/Task.cppm:48–52, 110–118, 69, 167–170` — `.retain()` /
  pending-retains path; image uploads stop feeding it.
- `Device/TransientQueue.cppm` — load fraction drops; infrastructure
  stays for buffer uploads + render passes + compute scratch.

### Must stay

- `Resources/Buffer.cpp:7–72` — buffer uploads need staging.
- GPU→GPU mip generation, equirect→cube, blit chains.

---

4. VK_KHR_dynamic_rendering_local_read + render-graph cleanups
--------------------------------------------------------------

### LocalRead candidates

- `Graphics/Renderers/AtmosphereRenderer.cpp:436` — transmittance →
  multiscatter (compute→compute, same-region).
- `AtmosphereRenderer.cpp:458` — multiscatter+transmittance → sky_view.
- `AtmosphereRenderer.cpp:472` — multiscatter+transmittance → AP volume
  (currently not chained via `.after()` — separate barrier streams).

### Bloom barrier explosion (bug-grade)

- `Graphics/Renderers/BloomRenderer.cpp:230–264` — downsample loop opens
  a fresh `co_await gpu::pass<downsample_pass>()` per mip.
  `append_prev_pass_barriers` (`RenderGraph.cppm:1943–2008`) is O(n²)
  over prior passes per mip. Fix by either:
  - Switching to `.record(lambda)` chaining (see memory note
    `pass — .record vs co_await`), or
  - Grouping consecutive mip levels into one pass with internal barriers.

### Render-graph cleanups (extension-independent)

- `RenderGraph.cppm:1955–2007` — `append_prev_pass_barriers` does
  O(passes × writes × reads) pointer compares. Build a hash map keyed by
  `resource.ptr` of prior writes; O(1) lookup.
- `RenderGraph.cppm:1891–1905` — once layouts are unified, skip emit
  when stages/access also match.

### Not applicable

- `VK_EXT_attachment_feedback_loop_layout` — confirmed nothing samples
  a texture it's simultaneously writing to.

---

5. VK_EXT_mutable_descriptor_type
---------------------------------

### Delete

- `Vulkan/Types.cppm:38–50` — seven per-type descriptor-size fields
  (`uniform_buffer_descriptor_size`, `storage_buffer_descriptor_size`,
  `sampled_image_descriptor_size`, `sampler_descriptor_size`,
  `combined_image_sampler_descriptor_size`,
  `storage_image_descriptor_size`,
  `acceleration_structure_descriptor_size`) collapse to one
  `mutable_descriptor_size` queried via `vkGetDescriptorSetLayoutSizeEXT`.
- `Resources/Descriptors.cppm:183–201` — the `descriptor_size_for(dt)`
  switch dies with them.
- `Vulkan/Bindless.cppm:61, 119` — `m_descriptor_size` no longer pulls
  from the per-type combined_image_sampler size.

### Reshape

- `Vulkan/DescriptorSetLayout.cpp:10–30` — add `eMutableDescriptorTypeEXT`
  + `VkMutableDescriptorTypeCreateInfoEXT` to the pNext chain. Follow
  the partial-binding pattern at `Bindless.cppm:95–98`.
- `Resources/Descriptors.cppm:139–141` — `m_storage_image_infos` and
  `m_combined_sampler_infos` can merge into one
  `unordered_map<(binding,type), descriptor_image_info>`.

### Must stay

- `descriptor_binding_desc::type` (`Types.cppm:318–324`) — still needed
  for layout creation; becomes the *set of allowed types* per binding.
- Samplers (standalone) and acceleration structures — spec forbids them
  in mutable bindings. `Descriptors.cppm:354–408` AS bucket and sampler
  bucket stay separate.
- `bindless_texture_set` slot allocator — stays a single free-list;
  only the write path gains per-call type dispatch.

---

6. VK_KHR_swapchain_maintenance1
--------------------------------

### Delete

- `Device/Swapchain.cppm:98–141` — 44 lines of pass-through wrappers
  (`depth_image()`, `extent()`, `format()`, etc.) over
  `vulkan::swap_chain`. Inline or alias.
- `Device/Swapchain.cppm:106–108` — `clear_depth_image()`. Depth
  belongs to RenderPass, not the swapchain.
- `Device/Swapchain.cppm:121–133` — `set_config()` + `reset_swapchain()`
  + `m_generation` bump. Replace with a single `recreate()` or deferred
  in-place mode change via `VkSwapchainPresentModesCreateInfoEXT`.
- `Vulkan/Swapchain.cppm:283–285` — explicit `reset_swapchain()`
  nullification. `VkSwapchainPresentFenceInfoEXT` lets the driver defer
  the destroy.

### Reshape

- `Vulkan/Sync.cppm:56–58` — `m_image_available[frame_index]` +
  `m_render_finished[image_index]` dual arrays collapse to one per-image
  release fence array under `VkSwapchainPresentFenceInfoEXT`.
- `Device/Frame.cpp:74–75` — fence wait switches from per-frame to
  per-image, dropping the worst-case stall.

---

7. VK_EXT_extended_dynamic_state3
---------------------------------

Mostly subsumed by §2 (shader_object's dynamic state is a superset). If
shader_object is skipped, EDS3 still buys:

- `Vulkan/Pipeline.cpp:124–158` — blend presets →
  `vkCmdSetColorBlendEnableEXT` + `vkCmdSetColorBlendEquationEXT`.
- `Vulkan/Pipeline.cpp:65–76` — rasterizer (polygon mode, cull, depth
  bias) → dynamic.
- `Vulkan/Pipeline.cpp:88–95` — `rasterizationSamples` →
  `vkCmdSetRasterizationSamplesEXT`.
- `Resources/PipelineBuilder.cppm:96, 481–482` — collapse N×blend_preset
  variants to one pipeline.

With shader_object on, delete this entire section.

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

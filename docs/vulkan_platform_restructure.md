# Vulkan Platform Wrapper Restructure

## Motivation

`Engine/Engine/Source/Platform/Vulkan/` originally had 24 tangled modules with several structural problems:

- `GpuDevice.cppm` (45KB) was a god object holding instance, device, allocator, queues, command pool, worker pools, descriptor heap, transient-work graveyard, shader cache, shader layout map, and feature flags.
- `GpuFactory.cppm` (27KB) was a factory in name only — a 15-partition import hub mixing enum-to-`vk::*` conversions, creation logic, and upload helpers.
- Multiple enum conversion helpers were byte-for-byte duplicated across files.
- The core wrapper types (`buffer`, `image`, `pipeline`, `descriptor_region`) and the consumers that used them (`render_graph`, `gpu_context`, `gpu_frame`, `gpu_compute`) formed a tangle of mutual imports.
- `resource_manager` was a vestigial class owned by context, with every public method forwarded through context.

Steps 1 through 4a have been completed. This document tracks what's landed and what's left.

---

## Architectural principles

These emerged from the refactor and govern remaining work:

**1. One-way dependency graph.** Wrappers → platform infra → leaf types. Nothing flows back up. The wrapper layer is strictly additive convenience; platform internals never speak wrapper types.

**2. Native handle types live in leaf partitions.** `vulkan::buffer_resource`, `vulkan::image_resource`, `vulkan::descriptor_region`, `vulkan::pipeline_handle` — these small structs carry `vk::*` handles and basic metadata. Render graph, compute queue, and other platform internals take these by reference; wrappers convert to them implicitly.

**3. Wrappers are thin and user-facing.** Each wrapper owns a native handle type (`gpu::buffer` wraps `vulkan::buffer_resource`, etc.) and exposes user conveniences (layout getters, RAII, etc.). They provide `operator const vulkan::X&() const` so callers pass wrappers to internal APIs without writing `.native()`.

**4. When a helper has no callers, delete it.** This rule has surfaced repeatedly:
- `to_vk_format` duplicates → one shared partition.
- `upload_image_3d` / `upload_mip_mapped_image` — zero external callers → deleted.
- `gpu::transition_image_layout` free function → one caller that couldn't see it due to a cycle → deleted, inlined.
- `resource_manager` class → all methods forwarded to context → folded entirely into context.
- `device::transition_image_layout` member → redundant with free-function variant → deleted.

Before adding any helper, grep for callers. Before keeping any existing helper during a refactor, grep again.

**5. Hard requirements are asserted at bootstrap, not exposed as flags.** Mesh shaders, ray tracing (acceleration structure + ray query), and descriptor buffer are required by the renderer; if the GPU doesn't support them the engine fails loudly at init. Only genuinely optional features (`video_encode`, device-fault diagnostics, robustness2) carry bools. No silent fallbacks that produce broken visuals.

---

## Progress log

### Step 1 — Enum conversions extracted ✅

- New partition `VulkanEnums.cppm` holds all `gpu::X → vk::Y` conversions as `to_vk` overloads.
- Deleted duplicates: `to_vk_stage_flags`, `to_vk_layout`, `to_vk_descriptor_type` (enum form).
- 7 consumers added `import :vulkan_enums;`; 13 overloads centralized.
- `ShaderLayoutCompiler::to_vk_descriptor_type` (takes Slang reflection, not enum) was NOT a duplicate — left in place.

### Step 2 — ShaderRegistry extracted ✅

- New partition `ShaderRegistry.cppm` owns `shader_cache_entry`, `cache(const shader&)`, `load_layouts(path)`, and the two maps.
- `GpuDevice` lost ~128 lines (cache + layouts + entry struct + related imports).
- `gpu::context` owns `std::unique_ptr<shader_registry>`. `context::compile()` invokes `load_layouts`.
- Pipeline and descriptor factory functions switched from `device&` to `context&`.
- `descriptor_writer` class rewritten: captures `const vulkan::shader_cache_entry*` at construction — no more `device*` member.
- 12 renderer files updated.

### Step 3 — Transient-work graveyard relocated ✅ (scope expanded)

Core:
- Graveyard + `add_transient_work` + `cleanup_finished` moved from `device` to `frame`.
- `create_sync_objects` became a private static on `frame`.
- `frame::transition_image_layout` added, delegates to uploader's low-level barrier primitive.

Scope that expanded:
- **Uploader split.** `vulkan::uploader` reduced to pure low-level primitives (`transition_image_layout(cmd,...)`, `upload_to_buffer`). Four high-level context-taking upload functions moved into GpuFactory; two unused ones (`upload_image_3d`, `upload_mip_mapped_image`) deleted.
- **Context-taking signatures widened** for `create_image`, `upload_to_buffers`, `upload_image_*`, `build_blas`.
- **Depth image layout tracking fixed.** `swap_chain::depth_image()` and `render_graph::depth_image()` return mutable `image&` via deducing-this; `const_cast` gymnastics removed.
- **`resource_manager` folded into `context`.** Pure layering win — it was owned by context, duplicated device access, and every method was already a forwarder. ~200 lines relocated. `resource::loader<T, context>` replaced `loader<T, resource_manager>`. `GpuResourceManager.cppm` deleted.

### Step 4a — DeviceBootstrap extracted ✅

- New partition `DeviceBootstrap.cppm` holds all one-shot init: `create_instance`, `create_device_and_queues`, `create_command_objects`, `find_queue_families`, `query_descriptor_buffer_properties`, `pick_surface_format`, dispatch loader, debug messenger.
- `bootstrap_device(win, save)` orchestrator returns `bootstrap_result`; `device::create` is now a 2-line wrapper.
- `device` constructor collapsed from 12 parameters to one (`bootstrap_result&&`).
- `max_frames_in_flight` migrated to `:vulkan_runtime` (its natural home).
- `GpuDevice.cppm` shrunk from 891 lines to 278 lines (−69%).

### Step 4.5 — Hard-requirement assertions ✅

Instead of exposing fallback bools for capabilities the renderer actually requires:

- **Asserted at bootstrap:** mesh shaders, ray tracing (acceleration structure + ray query), descriptor buffer. Clear assertion messages on unsupported GPUs.
- **Simplified pNext chain:** these features unconditionally chained; conditional branches removed.
- **Extension list:** `VK_EXT_mesh_shader`, `VK_EXT_descriptor_buffer`, `VK_KHR_deferred_host_operations`, `VK_KHR_acceleration_structure`, `VK_KHR_ray_query` always enabled.
- **Dead-flag cleanup:** `bootstrap_result::mesh_shaders_enabled`, `ray_tracing_enabled` deleted (no readers). `device::ray_tracing_enabled()` accessor deleted (one caller eliminated by removing its dead code path). `descriptor_buffer_properties::supported` field deleted.
- **`RtShadowRenderer`:** removed the "skip if no RT" fallback; the renderer's `initialized` bool became dead state and was also deleted.

Genuinely optional features still carry flags: `video_encode` (capture degrades to screenshots-only), `device_fault` (diagnostic), `robustness2` (hardening). Those stay.

---

## Current dependency shape

After Steps 1–4.5, the remaining cycle tangle is concentrated in how wrappers and platform internals reference each other's types:

```
render_graph      ─── uses wrapper types in its API ───→  buffer, pipeline, image, descriptor
gpu_frame         ─── uses image for transition + compute_semaphore_state ──→  gpu_image, gpu_compute
gpu_compute       ─── uses buffer in dispatch_indirect/buffer_copy ──→  gpu_buffer
gpu_swapchain     ─── holds gpu::image for depth ──→  gpu_image
vulkan_runtime    ─── swap_chain_config holds gpu::image ──→  gpu_image
gpu_device        ─── legacy import, likely unused ──→  gpu_image

gpu_context       ─── imports all of the above ───
```

Every wrapper is transitively importable from `gpu_context`, which means wrappers cannot import `gpu_context` themselves without cycles. This is why `GpuFactory.cppm` still exists — factory functions need `context&` but can't live with their wrappers.

Step 5 breaks this.

---

## Step 5 — One-way dependency + factory dispersal

### Principle

Make the dependency graph strictly one-way: **wrappers → platform infra → leaf types**. No wrapper is imported by any platform internal; platform internals operate on native handle types from leaf partitions.

With this, every wrapper can freely import `gpu_context`, and factory functions disperse to their owning wrapper files.

### The move

Push wrapper types out of internal APIs. Where internals need to reference a resource, they take the corresponding platform-level handle type, which is already a leaf:

| Wrapper (user API) | Handle type used by internals (leaf) | Location |
|---|---|---|
| `gpu::buffer` | `vulkan::buffer_resource` | `:vulkan_allocator` (exists) |
| `gpu::image` | `vulkan::image_resource` | `:vulkan_allocator` (exists) |
| `gpu::descriptor_region` | `vulkan::descriptor_region` | `:descriptor_heap` (exists) |
| `gpu::pipeline` | `vulkan::pipeline_handle` | `:vulkan_handles` (new leaf, ~10 lines: `vk::Pipeline`, `vk::PipelineLayout`, `bind_point`) |

Internals (`render_graph`, `gpu_compute`, `gpu_swapchain`, etc.) take the handle types in their APIs. Wrappers provide implicit conversion:

```cpp
class buffer {
    vulkan::buffer_resource m_resource;
public:
    operator const vulkan::buffer_resource&() const { return m_resource; }
};
```

Call sites stay identical — `rec.bind_vertex(my_buffer)` still compiles because `const buffer&` implicitly binds to `const buffer_resource&`.

### Seven cycles dissolve

1. **`render_graph → wrappers`.** render_graph's API switches to `buffer_resource`, `image_resource`, `vulkan::descriptor_region`, `pipeline_handle`. Drops all 4 wrapper imports.
2. **`gpu_frame → gpu_image`.** `frame::transition_image_layout` inlined at its one caller (context ctor). `gpu_image` import dropped.
3. **`gpu_frame → gpu_compute`.** `compute_semaphore_state` moves to new leaf `:gpu_sync`. Both frame and compute import the leaf.
4. **`gpu_compute → gpu_buffer`.** compute_queue's API (`dispatch_indirect`, `buffer_copy`) switches to `buffer_resource`.
5. **`vulkan_runtime → gpu_image`.** `swap_chain_config::depth_image` becomes `image_resource` (already a platform type). Runtime stops importing gpu_image.
6. **`gpu_swapchain → gpu_image`.** `swap_chain` holds `image_resource` for depth; `swap_chain::depth_image()` returns `image_resource&`.
7. **`gpu_device → gpu_image`.** Legacy; verify unused and drop.

### Cascading consequence

`descriptor_writer`'s methods (`image(name, img, sampler, layout)` etc.) are high-level but they write raw descriptors, so they also take handle types:

```cpp
auto image(std::string_view name, const vulkan::image_resource& img, vk::Sampler samp, image_layout layout) -> descriptor_writer&;
```

Keeps consistency: **any "op" method takes handle types; wrappers convert on the way in.**

### Architectural decisions (signed off)

- **Q1 — `pipeline_handle` home:** new leaf `:vulkan_handles`. Clean semantics.
- **Q2 — Implicit conversion operators on wrappers:** yes, required. That's what keeps call sites free of `.native()`.
- **Q3 — `swap_chain::depth_image()` returns `image_resource&`:** yes. Propagates to a few render-graph and descriptor-writer calls; mechanical.
- **Q4 — Call-site impact on renderers:** near-zero. Same syntax (`rec.bind(x)`, `writer.image(n, x, s)`); types under the hood change.
- **Q5 — `compute_semaphore_state` home:** new leaf `:gpu_sync`. 15-line file, no new bloat.

### Final tier structure after Step 5

```
Tier 0 (leaf):
  :gpu_types                   enums
  :descriptor_buffer_types     descriptor-buffer props
  :vulkan_allocator            buffer_resource, image_resource
  :descriptor_heap             vulkan::descriptor_region
  :vulkan_handles        NEW   pipeline_handle
  :gpu_sync              NEW   compute_semaphore_state

Tier 1 (platform infra, no wrapper imports):
  :vulkan_runtime              configs (depth_image → image_resource)
  :device_bootstrap
  :gpu_device                  (drops :gpu_image)
  :gpu_swapchain               (holds image_resource for depth)
  :gpu_frame                   (drops :gpu_image, :gpu_compute)
  :gpu_compute                 (API takes buffer_resource, pipeline_handle)
  :render_graph                (API takes handle types everywhere)
  :shader_registry
  :gpu_context                 (imports Tier 0 + 1, no wrappers)

Tier 2 (wrappers):
  :gpu_buffer                  operator const buffer_resource&; factories here
  :gpu_image                   operator const image_resource&; factories here
  :gpu_pipeline                operator pipeline_handle; factories here
  :gpu_descriptor              operator const vulkan::descriptor_region&; factory here
  :gpu_descriptor_writer       (existing home; absorbs writer factories)

Deleted:
  :gpu_factory                 all contents dispersed
```

### Factory dispersal table

| Factory function | New home |
|---|---|
| `create_buffer(device&, desc)` | `:gpu_buffer` |
| `upload_to_buffers(context&, span<buffer_upload>)` | `:gpu_buffer` |
| `create_image(context&, desc)` | `:gpu_image` |
| `upload_image_2d(context&, image&, ...)` | `:gpu_image` |
| `upload_image_layers(context&, image&, ...)` | `:gpu_image` |
| `create_sampler(device&, desc)` | `:gpu_pipeline` |
| `create_graphics_pipeline(context&, shader&, desc)` | `:gpu_pipeline` |
| `create_compute_pipeline(context&, shader&, ...)` | `:gpu_pipeline` |
| `allocate_descriptors(context&, shader&)` | `:gpu_descriptor` |
| `create_descriptor_writer(context&, handle<shader>, region&)` | **delete** (trivial ctor wrapper) |
| `create_push_writer(context&, handle<shader>)` | **delete** (trivial ctor wrapper) |
| `create_compute_queue(device&)` | `:gpu_compute` |
| `build_blas(context&, desc)` | stays in `:acceleration_structure` (already there) |

### Order of operations within Step 5

Each sub-step should leave the build working:

1. **Build the leaves.** Create `:vulkan_handles` and `:gpu_sync`. No consumers yet.
2. **Drop unused imports.** Remove `:gpu_image` from `gpu_device` if verified unused (cycle 7).
3. **Inline frame transition (cycle 2).** Remove `frame::transition_image_layout`, inline at context ctor. Drop `:gpu_image` from `gpu_frame`.
4. **Move `compute_semaphore_state` (cycle 3).** Move to `:gpu_sync`. Drop `:gpu_compute` from `gpu_frame`.
5. **Switch compute_queue API to handle types (cycle 4).** `buffer_copy`, `dispatch_indirect` etc. take `buffer_resource`. Drop `:gpu_buffer` from `gpu_compute`.
6. **Depth image to resource (cycles 5+6).** `swap_chain_config` holds `image_resource`. `swap_chain::depth_image()` returns `image_resource&`. Drop `:gpu_image` from both `vulkan_runtime` and `gpu_swapchain`.
7. **Render graph API to handle types (cycle 1).** All `bind`/`track`/`reads`/`writes`/recording methods take handle types. Drop wrapper imports. This is the biggest sub-step — many signatures, many call sites.
8. **Descriptor writer API to handle types.** Consequence of swap_chain returning resources; keeps the idiom consistent.
9. **Add implicit conversion operators** on `gpu::buffer`, `gpu::image`, `gpu::pipeline`, `gpu::descriptor_region`.
10. **Disperse factory functions** to their wrapper files. Delete trivial writer-factories. Delete `GpuFactory.cppm`. Drop `:gpu_factory` from `Platform.cppm`.

### Verification

- `grep "import :gpu_buffer\|import :gpu_pipeline\|import :gpu_image\|import :gpu_descriptor" Platform/Vulkan/` returns only wrapper-tier files (excluding `gpu_descriptor_writer` which is wrapper-tier itself).
- Every wrapper file has `import :gpu_context;` (or is `gpu_context` itself).
- `GpuFactory.cppm` does not exist.
- Physics stress test scene renders identically.

---

## Step 6 — Collapse the three descriptor files

Currently `GpuDescriptor.cppm`, `GpuDescriptorWriter.cppm`, and `DescriptorHeap.cppm` split the descriptor concept across three partitions with no clear primary. Collapse into a single `Descriptors.cppm`.

- `descriptor_region` wrapper, `descriptor_writer` class, heap allocator — one file.
- Low priority cosmetically, but bundles well with Part 2 Step 8 (`maintenance6` `*2` APIs) and Step 9 (descriptor-indexing flags) — the collapsed file ships with the new APIs from day one rather than being rewritten after.

Do this only after Step 5 (which changes descriptor_writer's method signatures).

---

## Step 7 — Move shader compilers to `Shader/` subdir

`Shader.cppm`, `ShaderLayout.cppm`, `ShaderCompiler.cppm`, `ShaderLayoutCompiler.cppm` — 66KB combined. Compilers are asset-pipeline code; runtime shader metadata is different. Move into `Platform/Vulkan/Shader/`.

While moving, thread a `feature_caps` view into the compiler so SPIR-V capability emission (cooperative matrix, atomic float, barycentrics, nonuniform, partially-bound) is driven by device features rather than hard-coded. Pairs with Part 2 Step 13.

---

## Things to leave alone

- Thin-wrapper pattern for `GpuBuffer`/`GpuImage`/`GpuPipeline`/`GpuPushConstants` (except for the handle-type conversion ops added in Step 5).
- `GpuCompute::compute_queue` — boundary is good; only the API surface changes in Step 5.
- `AccelerationStructure`, `VideoEncoder` — feature-optional isolation works.
- `PersistentAllocator` — already a clean service.
- Render-graph internals (scheduling, barrier generation). Step 5 only touches the API surface. A deeper render-graph cleanup is a separate project.

Render-graph cleanup is **not** part of Step 5. Part 2 steps that touch the render graph (local-read, load_store_op_none) ride that separate track.

---

## Part 2 — Extension adoption

Baseline: `docs/vulkan_extensions.md`. The Intel Arc 140V device list is the support floor across NVIDIA/AMD/Intel for this project; treat every extension referenced below as available.

Strategy: Step 4.5 already made mesh shaders, ray tracing, and descriptor buffer hard requirements. Remaining extension adoption is about picking up incremental features that are genuinely optional or additive.

The original plan called for a declarative feature table before Part 2. With requirements asserted, only 3–4 genuinely optional features remain; a table would be overkill. Add new extensions directly to `create_device_and_queues` in `DeviceBootstrap.cppm`.

### Step P2.1 — VK_KHR_maintenance5

Adopt `PipelineCreateFlags2` (64-bit), `vkCmdBindIndexBuffer2` (index type per call, no reinterpret dance), and the relaxed `RenderingAttachmentInfo` semantics.

Broad but mechanical: one new `to_vk` overload for `pipeline_create_flags2`, then a sweep of every index-buffer bind and pipeline create. No behavioral changes.

### Step P2.2 — VK_KHR_maintenance6

Swap `vkCmdPushDescriptorSet` / `vkCmdPushConstants` / `vkCmdBindDescriptorSets` to the `*2` variants that take pNext-chained info structs. Allow `VK_NULL_HANDLE` set layouts in pipeline layouts — kills dummy-set plumbing where present.

Best bundled with Step 6 (descriptor-file collapse) — the collapsed `Descriptors.cppm` should ship with `*2` APIs from day one.

### Step P2.3 — Descriptor-indexing features

Flip in `PhysicalDeviceVulkan12Features`:
- `runtimeDescriptorArray`
- `descriptorBindingPartiallyBound`
- `descriptorBindingVariableDescriptorCount`
- `descriptorBindingSampledImageUpdateAfterBind`
- `descriptorBindingStorageImageUpdateAfterBind`
- `descriptorBindingStorageBufferUpdateAfterBind`
- `shaderSampledImageArrayNonUniformIndexing`

Shader-side: audit `ShaderLayoutCompiler` for `nonuniformEXT` emission and `UPDATE_AFTER_BIND_POOL` / `PARTIALLY_BOUND` binding-flag propagation. Pairs with Step 7 (compiler + feature caps).

Prerequisite for a real bindless path.

### Step P2.4 — VK_KHR_dynamic_rendering_local_read

Collapse forward-plus depth prepass + light-culling reads + shading passes into one `vkCmdBeginRendering` block using local-read barriers, replacing split renders + pipeline barriers.

Touches `RenderGraph.cppm`. Defer until the render-graph cleanup lands — do not bundle with Step 5.

### Step P2.5 — VK_EXT_host_image_copy

Replace the staging-buffer + transfer-queue upload path with `vkCopyMemoryToImage` / `vkTransitionImageLayout`. Call sites: texture loading in `Model.cppm`, the scene asset pipeline.

Shrinks `transient_work_graveyard` traffic — cleanest after Step 5 when image factories are colocated with the wrapper.

Out of scope: buffer uploads (no equivalent extension — still staging).

### Step P2.6 — VK_EXT_extended_dynamic_state3 + VK_EXT_vertex_input_dynamic_state

Move polygon mode, color blend state, sample count, depth clip, color write mask, and vertex input bindings into dynamic state. Collapses per-material PSO permutations; critical for skinned-vs-static mesh variants.

Touches the pipeline-creation surface (post-Step 5, that's `gpu_pipeline`). Deferrable: only worth it once PSO count hurts.

### Step P2.7 — Physics / compute shader features

Enable and plumb through the shader compiler:
- `shaderBufferFloat32Atomics` + `Atomics2` — replace CAS loops in VBD accumulation compute.
- `VK_KHR_cooperative_matrix` — if VBD inner loops use small dense matmul.
- `shaderMaximalReconvergence` + `shaderSubgroupRotate` — culling / light-binning kernels.

Scoped to the compute track; independent of renderer restructure.

### Step P2.8 — Diagnostics / memory wins

Each landable piecemeal:
- **VK_EXT_calibrated_timestamps** — plug into existing tracing so CPU/GPU timelines share a calibrated clock.
- **VK_EXT_pageable_device_local_memory + VK_EXT_memory_priority** — enable features; VMA picks both up automatically.
- **VK_EXT_load_store_op_none** — transient forward-plus attachments. Rides the render-graph track.

### Explicit skips

- **VK_EXT_shader_object** — Intel Arc exposes it only via the Khronos emulation layer.
- **VK_EXT_graphics_pipeline_library** — Step P2.6's dynamic state3 covers most variant-explosion wins with less complexity. Revisit only if shader-variant hitches show up in traces.
- **VK_KHR_ray_tracing_pipeline** — ray query covers the shadow use case; the pipeline flavor adds machinery we don't need.

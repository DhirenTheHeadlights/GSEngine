# RHI Refactor — Scope of Work

## Goal

Replace the current patchwork `gpu::`/`vulkan::` split with a proper RHI (Render Hardware Interface) that completely isolates Vulkan from the rest of the engine. After this refactor, no file outside `Engine/Source/Platform/Vulkan/` imports or mentions a `vk::` type. Renderers, compilers, and game code speak only `gpu::` types with semantic APIs.

## Current Problems

- `gpu::context` is monolithic: device ownership, frame management, uploads, hot reload, shader management all in one class
- Vulkan types leak through the public API: `device()` returns `vk::raii::Device&`, shaders take `vk::CommandBuffer`, renderers build `vk::ImageMemoryBarrier2` inline
- `gpu::buffer`/`gpu::pipeline` are thin wrappers with `.native()` escape hatches every renderer uses
- Separate compute context is unclear in purpose vs the main context
- Descriptor writes require manual `vk::DescriptorBufferInfo` / `vk::DescriptorImageInfo` construction in renderers

## Design Principles

1. **Semantic over mechanical** — `cmd.barrier(image, new_layout)` not `cmd.pipelineBarrier2(vk::DependencyInfo{...})`
2. **Each wrapper is its own type** — `gpu::cmd`, `gpu::image`, `gpu::buffer`, etc. are distinct opaque types, not typedefs
3. **No `.native()` escape hatches in public API** — Vulkan access is internal to the Platform/Vulkan/ implementation
4. **Split the monolith** — `gpu::context` breaks into focused, single-responsibility types
5. **Render graph integration** — wrappers compose naturally with the existing render graph

## New Type System

### Core Device Types

| Type | Responsibility | Replaces |
|---|---|---|
| `gpu::device` | Physical/logical device, queues, allocator, device properties | `gpu::context.device()`, `runtime.device_config()` |
| `gpu::swapchain` | Swapchain images, format, extent, recreation on resize | `runtime.swap_chain_config()` |
| `gpu::frame` | Per-frame state: command buffer, image index, sync primitives | `runtime.frame_context_config()` |

### Resource Types

| Type | Responsibility | Replaces |
|---|---|---|
| `gpu::buffer` | GPU buffer with usage flags, size, mapped pointer | `vulkan::buffer_resource` (existing, needs completion) |
| `gpu::image` | Image + view + format + current layout tracking | `vulkan::image_resource` + `vulkan::image_resource_info` |
| `gpu::sampler` | Sampler state | Existing (already decent) |
| `gpu::pipeline` | Pipeline + layout + bind point | Existing (needs pipeline_layout absorbed) |

### Command Types

| Type | Responsibility | Replaces |
|---|---|---|
| `gpu::cmd` | Semantic command recording | Raw `vk::CommandBuffer` usage in renderers |
| `gpu::barrier` | Describes a resource transition | Inline `vk::ImageMemoryBarrier2` / `vk::BufferMemoryBarrier2` |

### Descriptor Types

| Type | Responsibility | Replaces |
|---|---|---|
| `gpu::descriptor_region` | Allocated descriptor memory region | `vulkan::descriptor_region` |
| `gpu::descriptor_writer` | Fluent API for writing buffer/image/sampler descriptors | Manual `vk::DescriptorBufferInfo` + `vk::WriteDescriptorSet` |

## Semantic API Sketches

### gpu::cmd

```cpp
// Binding
cmd.bind(pipeline);                          // infers bind point from pipeline type
cmd.bind_vertex(buffer, offset);
cmd.bind_index(buffer, offset, index_type);
cmd.bind_descriptors(pipeline, set, region); // infers layout from pipeline

// Drawing
cmd.draw(vertex_count, instance_count, first_vertex, first_instance);
cmd.draw_indexed(index_count, instance_count, first_index, vertex_offset, first_instance);
cmd.draw_indexed_indirect(buffer, offset, count, stride);

// Compute
cmd.dispatch(x, y, z);
cmd.dispatch_indirect(buffer, offset);

// Barriers — stage/access inferred from old_layout -> new_layout
cmd.barrier(image, new_layout);
cmd.barrier(image, new_layout, mip_range, layer_range);  // subresource
cmd.barrier(buffer, src_stage, dst_stage);                // buffer memory barrier
cmd.execution_barrier(src_stage, dst_stage);              // pure execution dependency

// Push constants — integrates with shader reflection
cmd.push(pipeline, "block", "member", value, ...);

// Rendering (dynamic rendering)
cmd.begin_rendering(color_attachments, depth_attachment, extent);
cmd.end_rendering();

// Viewport/scissor
cmd.set_viewport(extent);
cmd.set_scissor(extent);
cmd.set_viewport(x, y, w, h, min_depth, max_depth);
cmd.set_scissor(x, y, w, h);
```

### gpu::image

```cpp
// Creation via factory
auto img = gpu::create_image(device, {
    .extent = {1024, 1024, 1},
    .format = gpu::image_format::d32_sfloat,
    .usage  = gpu::image_usage::depth_attachment | gpu::image_usage::sampled,
    .layers = 6,
    .mips   = 1,
});

// Queries
img.extent();
img.format();
img.layout();     // tracks current layout
img.view();       // default view (all mips/layers) — returns gpu::image_view
img.layer_view(i); // per-layer view
img.mip_view(i);   // per-mip view
```

### gpu::descriptor_writer

```cpp
gpu::descriptor_writer writer(shader, region);
writer.buffer("lights", light_buffer);
writer.image("shadow_maps", shadow_image, sampler);
writer.image_array("textures", texture_images, sampler);
writer.commit();
```

### gpu::device

```cpp
device.properties();       // limits, features, vendor info
device.queue(gpu::queue_type::graphics);
device.queue(gpu::queue_type::compute);
device.wait_idle();
```

## Context Split

```
BEFORE:
  gpu::context  (monolith)

AFTER:
  gpu::device           — device + queues + allocator (created once)
  gpu::swapchain        — swapchain management (recreated on resize)
  gpu::frame            — per-frame cmd + sync (N in flight)
  gpu::resource_manager — upload queue, shader cache, hot reload
  gpu::render_graph     — pass ordering, resource tracking (already exists)
```

Renderers receive `gpu::frame&` for recording and `gpu::device&` for resource creation. They never see the full set of services.

## Phases

### Phase 1: Core types + cmd wrapper ✓
- `gpu::buffer` (`GpuBuffer.cppm`) — wraps `vulkan::buffer_resource`, tracks size/mapped pointer
- `gpu::image` (`GpuImage.cppm`) — wraps `vulkan::image_resource` with layout tracking and format conversion
- `gpu::pipeline` (`GpuPipeline.cppm`) — wraps `vk::raii::Pipeline` + `PipelineLayout` + bind point
- `gpu::descriptor_set`, `gpu::sampler` — RAII wrappers in `GpuPipeline.cppm`
- `gpu::buffer_desc`, `gpu::image_desc`, `gpu::sampler_desc`, `gpu::graphics_pipeline_desc` — backend-agnostic descriptor structs in `GpuTypes.cppm`
- Factory functions in `GpuFactory.cppm` — `create_buffer`, `create_image`, `create_graphics_pipeline`, `create_compute_pipeline`, `create_sampler`, `allocate_descriptors`, `write_descriptors`, `upload_to_buffers`
- `recording_context` overloads accepting `gpu::pipeline`, `gpu::buffer`, `gpu::descriptor_region` in `RenderGraph.cppm`
- All renderers migrated to use abstract types for pipeline/descriptor/sampler creation

### Phase 2: Context split ✓
- `gpu::device` (`GpuDevice.cppm`) — wraps runtime for logical/physical device, queues, allocator, descriptor heap, transient work
- `gpu::swapchain` (`GpuSwapchain.cppm`) — wraps runtime for extent, format, depth image, image views, recreate callbacks
- `gpu::frame` (`GpuFrame.cppm`) — wraps runtime for current frame index, image index, command buffer, frame status; `frame_token`/`frame_status` types for begin_frame result
- Factory functions take `gpu::device&` as primary signature; `context&` forwarding overloads for backward compatibility
- Render graph constructor takes `gpu::device&, gpu::swapchain&, gpu::frame&` instead of `runtime&`
- `gpu::context` delegates device/queue/allocator/format/descriptor methods through the new types
- `context::begin_frame()` returns `std::expected<frame_token, frame_status>`
- **Remaining runtime usage in context:** `begin_frame`/`end_frame` (vulkan free functions take `runtime&`), `upload_image_2d`/`upload_image_layers` (uploader takes `runtime&`), `on_swap_chain_recreate` (const method quirk), shutdown depth_image cleanup

### Phase 3: Descriptor abstraction ✓
- `gpu::descriptor_writer` (`GpuDescriptorWriter.cppm`) — fluent name-based API for both persistent and push descriptor paths
  - Takes `resource::handle<shader>` — resolves binding names to indices via shader reflection
  - Persistent mode: accumulates `buffer()`/`image()`/`image_array()` calls, writes to `descriptor_region` on `commit()`
  - Push mode: created via `gpu::create_push_writer()`, `begin(frame_index)` allocates transient region, committed via `ctx.commit(writer.native_writer(), pipeline, set)`
  - `image_array()` supports both uniform sampler and per-element sampler overloads
- `meshlet_gpu_data::bind(gpu::descriptor_writer&)` — name-based overload using shader binding names
- Factory functions: `create_descriptor_writer()`, `create_push_writer()` in `GpuFactory.cppm`
- All renderers migrated: no `gpu::buffer_binding`/`gpu::image_binding` structs, no `gpu::write_descriptors()` calls, no `vulkan::descriptor_writer` or `shader::create_writer()` in renderer code
- Old `vulkan::descriptor_writer` index-based path retained internally for `gpu::descriptor_writer` push mode

### Phase 4: Renderer migration ✓
- All renderers use only `gpu::` types for pipeline/descriptor/sampler/buffer/image/barrier operations
- Zero `vk::` types in any exported `gse.graphics` or `gse.platform` interface
- `.native()` escape hatches remain only inside `Platform/Vulkan/` implementation files
- `vulkan::` types removed from all renderer and consumer `.cppm` files

### Phase 5: Resource system refactor ✓
- `ResourceHandle.cppm` (`:resource_handle`) — leaf partition: `resource::state`, `resource::resource_slot<T>`, `resource::handle<T>` with direct typed slot pointer
  - `handle<T>` holds `resource_slot<T>*` directly — zero virtual dispatch, zero `void*` casting for resolution
  - Consumers only need this partition to use `handle<shader>`, `handle<texture>`, etc.
- `ResourceLoader.cppm` (`:resource_loader`) — implementation partition:
  - `resource_context<C>` concept constrains the loader's context type (`gpu_queue_size`, `mark_pending_for_finalization`, `wait_idle`, `process_gpu_queue`)
  - `loader_base` slimmed — resolve/state/version virtuals removed (now bypassed by direct slot pointer)
  - `loader<T, C>` uses `id_mapped_collection<std::unique_ptr<resource_slot<T>>>` for heap-allocated stable-address slots
  - `loader::state_of(id)` — typed replacement for removed `loader_base::resource_state()` virtual
  - `instantly_load` rewritten to use before/after `gpu_queue_size()` pattern
- Circular module dependency between `resource::loader<T, context>` and `gpu::context` is broken — `loader` constrains via concept, never imports `context`

### Phase 6: Cleanup (in progress)
**Done:**
- `context&` forwarding overloads removed from `GpuFactory.cppm` — all callers migrated to `ctx.device_ref()`
- `gpu::buffer_binding` / `gpu::image_binding` structs obsoleted by `descriptor_writer` — retained in `GpuDescriptor.cppm` for backward compat, no renderer uses them
- `gpu::write_descriptors()` free function obsoleted by `descriptor_writer` — retained internally, no renderer uses it
- Dead `vulkan::descriptor_region` overload removed from `compute_queue`
- 5 vk-leaking swapchain methods deleted; `context::execute_and_detect_gpu_queue` removed

**Remaining:**
- Extract `gpu::resource_manager` from `gpu::context` — now unblocked by the concept-based loader split

**Done (remaining issue cleanup):**
- `cached_push_constants` and `push_constant_member` moved from `vulkan::` to `gpu::` namespace
- `compute_queue::push(pipeline, cached_push_constants)` added — replaces `shader->push(cmd, layout, ...)` pattern
- `GpuSolver.cppm` fully cleaned: zero `vk::` types, zero `.native()` calls
  - `shader->push()` replaced with `cache_push_block()` + individual `set()` + `compute_queue::push()`
  - `vk::DeviceSize` → `std::size_t`
  - Raw `cmd.copyBuffer()` → `compute_queue::copy_buffer()`
- **Final state: zero `vk::` types and zero `.native()` calls outside `Platform/Vulkan/`**

**Done (shader API-agnostic refactor):**
- `shader` class is now a pure data container — zero `vk::` types in its interface
- New gpu-agnostic types in `GpuTypes.cppm`: `shader_stage`, `stage_flag`/`stage_flags`, `descriptor_type`, `vertex_format`, `vertex_binding_desc`, `vertex_attribute_desc`, `descriptor_binding_desc`, `descriptor_set_type`
- Shader stores SPIR-V bytecode as `std::vector<std::uint32_t>` per stage, reflection data uses gpu enums
- `set_uniform`/`set_ssbo` methods use `buf.mapped()` directly — no `vulkan::allocation` dependency
- New data accessors: `spirv()`, `vertex_input_data()`, `layout_data()`, `push_constants()`, `layout_name()`
- All vk::-dependent methods (`shader_stages`, `compute_stage`, `vertex_input_state`, `layouts`, `push_constant_range`, `allocate_descriptors`, `create_writer`, `write_descriptors`, `push_descriptor`, `push_bytes`, `push`) removed from shader class
- `ShaderCompiler` serializes gpu-agnostic types — binary format changed (auto-recompiles via asset pipeline)
- `gpu::device` owns shader cache (`shader_cache_entry` with modules + layouts), lazily created per shader
- `gpu::device` owns shader layout loading (`load_shader_layouts()`)
- `GpuFactory` pipeline/descriptor creation uses device cache for modules, layouts, and conversions
- `GpuDescriptorWriter` uses shader data + device cache — no shader methods for descriptor operations
- Conversion helpers (`to_vk_stage_flags`, `to_vk_descriptor_type`, `to_vk_format`) live in GpuDevice.cppm as internal functions

**Done (dead code removal + renderer .native() elimination):**
- `gpu::buffer_binding`, `gpu::image_binding` structs deleted from `GpuDescriptor.cppm`
- `gpu::write_descriptors()` free function and its helpers (`to_vk_buffer_infos`, `to_vk_image_infos`, `to_vk_image_array_infos`) deleted from `GpuFactory.cppm`
- `meshlet_gpu_data::bind(vulkan::descriptor_writer&)` overload already removed — only name-based overload exists
- All Runtime.cppm config structs verified active — no dead types to remove
- Compute context kept as-is — focused single-purpose type with one consumer (GpuSolver)
- `shader::set_uniform()`, `set_uniform_block()`, `set_ssbo_element()`, `set_ssbo_struct()` — added `gpu::buffer&` overloads that forward to allocation internally
- All 6 graphics renderers migrated from `.native().allocation` to passing `gpu::buffer&` directly — zero `.native()` escape hatches remain in Graphics/
- `CullComputeRenderer` migrated from `buffer.native()` to passing `gpu::buffer&` directly to `track()`
- Asset compilers verified clean — no `vk::` types, no `.native()` calls

**Done (runtime elimination):**
- `vulkan::runtime` class deleted from `Runtime.cppm` — config structs (`instance_config`, `device_config`, `queue_config`, `command_config`, `sync_config`, `swap_chain_config`, `frame_context_config`, `queue_family`) retained as pure data types
- `gpu::device` now owns: `instance_config`, `device_config`, `allocator`, `queue_config`, `command_config`, `descriptor_heap`, `descriptor_buffer_properties`, transient work graveyard, device lost reporting — constructor takes all by rvalue reference
- `gpu::swapchain` now owns: `swap_chain_config`, recreate callbacks, generation counter — stores `gpu::device*` for recreation; exposes `image()`, `image_view()`, `format()`, `config()` accessors
- `gpu::frame` now owns: `sync_config`, `frame_context_config`, current frame index, frame-in-progress flag — stores `gpu::device*` and `gpu::swapchain*`; `begin()`/`end()` methods encapsulate all frame lifecycle logic previously in `vulkan::begin_frame()`/`vulkan::end_frame()`
- `gpu::context` constructs device/swapchain/frame directly from helper functions (`create_instance_and_surface`, `create_device_and_queues`, etc.) — no `runtime` intermediary; destruction order: frame → swapchain → device
- `create_runtime()`, `begin_frame()`, `end_frame()` free functions deleted from `Context.cppm`; helper functions (`create_instance_and_surface`, `create_device_and_queues`, `create_command_objects`, `create_sync_objects`, `create_swap_chain_resources`) exported for direct use
- `runtime_ref()` removed from all three wrapper types (`device`, `swapchain`, `frame`)
- Uploader functions (`upload_image_2d`, `upload_image_layers`, `upload_image_3d`, `upload_mip_mapped_image`) take `gpu::device&` instead of `runtime&`
- `device::surface_format()` cached at construction instead of reaching through `swap_chain_config`
- `RenderGraph` uses `swapchain::image()`, `swapchain::image_view()`, `swapchain::extent()` instead of `runtime_ref().swap_chain_config()`
- `transient_work_graveyard` moved from `sync_config` to `gpu::device` — `sync_config` is now purely synchronization primitives

**Done (GLFW isolation + file reorganization):**
- GLFW headers now only included in `GLFW/Window.cppm`, `GLFW/Input.cppm`, `GLFW/Keys.cppm` — zero GLFW outside the `GLFW/` folder
- `create_instance_and_surface()` split: renamed to `create_instance(std::span<const char* const>)`, surface created separately via `window.create_vulkan_surface()`
- `create_swap_chain_resources()` takes `vec2i framebuffer_size` instead of `GLFWwindow*`
- `gpu::frame::begin()`/`end()` take `gse::window&` instead of `GLFWwindow*` + `bool` + `bool` — queries `win.minimized()`, `win.frame_buffer_resized()`, `win.viewport()` internally
- `gse::window::vulkan_instance_extensions()` added — static method wrapping `glfwGetRequiredInstanceExtensions()`, keeps GLFW contained in `GLFW/` folder
- `Vulkan/Context.cppm` GLFW include removed — no longer depends on GLFW at all
- 10 vulkan-dependent `Gpu*.cppm` files moved from `Platform/` root to `Platform/Vulkan/`: GpuBuffer, GpuImage, GpuPipeline, GpuDescriptor, GpuDescriptorWriter, GpuCompute, GpuFactory, GpuDevice, GpuSwapchain, GpuFrame
- `Platform/` root now contains only API-agnostic files: GpuTypes, GpuContext (facade), App, AssetCompiler, AssetPipeline, PlayerController, ResourceHandle, ResourceLoader

**Done (Context.cppm dissolution):**
- `Vulkan/Context.cppm` deleted — all creation code distributed to owning types
- `gpu::device::create(window&, save::state&)` static factory encapsulates instance, device, queue, command, allocator, descriptor heap creation
- `gpu::swapchain::create(vec2i, device&)` static factory encapsulates swap chain resource creation; `swapchain::recreate(vec2i)` for swap chain recreation
- `gpu::frame::create(device&, swapchain&)` static factory encapsulates sync object creation
- `vulkan::find_queue_families` exported from GpuDevice.cppm for use by GpuSwapchain swap chain creation
- `vulkan::max_frames_in_flight` and `vulkan::device_creation_result` exported from GpuDevice.cppm
- `GpuContext` constructor reduced to three factory calls + render graph creation
- Dead `:vulkan_context` imports removed from Uploader.cppm, GpuFrame.cppm, GpuContext.cppm
- Dead `:vulkan_allocator`, `:descriptor_heap`, `:descriptor_buffer_types` imports removed from GpuContext.cppm

## Non-Goals

- Multi-backend support (D3D12, Metal) — this is about clean abstraction, not portability
- Changing the render graph architecture — it's good as-is
- Changing the shader reflection system — just wrap its inputs/outputs
- Changing the Slang shader compiler pipeline

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

### Phase 1: Core types + cmd wrapper
- Define `gpu::cmd`, `gpu::image`, `gpu::barrier` types
- Implement semantic command recording (bind, draw, barrier, push, rendering)
- Implement `gpu::image` with layout tracking and view management
- All new types live in `Platform/GpuTypes.cppm` or new `Platform/Gpu*.cppm` modules
- Vulkan implementations in `Platform/Vulkan/` — wrappers forward to vk:: calls

### Phase 2: Context split
- Break `gpu::context` into `gpu::device`, `gpu::swapchain`, `gpu::frame`, `gpu::resource_manager`
- Update render graph to pass `gpu::frame` to record lambdas
- Migrate factory functions to take `gpu::device&`

### Phase 3: Descriptor abstraction
- Implement `gpu::descriptor_writer` with fluent API
- Integrate with shader reflection (shader knows binding names and types)
- Migrate renderers from manual descriptor writes to writer API

### Phase 4: Renderer migration
- Migrate all renderers to use only `gpu::` types
- Remove all `vk::` includes from renderer modules
- Remove `.native()` escape hatches from public API
- Audit: no file outside `Platform/Vulkan/` should import vulkan headers

### Phase 5: Cleanup
- Remove dead `vulkan::` types that are now fully wrapped
- Merge or remove compute context into `gpu::device` queue model
- Update compilers (model, material, texture, etc.) to use `gpu::` types only

## Non-Goals

- Multi-backend support (D3D12, Metal) — this is about clean abstraction, not portability
- Changing the render graph architecture — it's good as-is
- Changing the shader reflection system — just wrap its inputs/outputs
- Changing the Slang shader compiler pipeline

# Transient GPU Work Redesign

## Status

| Phase | Status | Notes |
|-------|--------|-------|
| 0 — Audit current callers | TODO | Classify each `add_transient_work` site as fire-and-forget (A) or async-setup (B). |
| 1 — Frame-deferred resource bin | TODO | Extract a generic, type-erased keep-alive registry keyed on a timeline-semaphore value. |
| 2 — `frame_recorder` for use case A | TODO | Replace per-frame transition submits with appended commands on the frame's primary command buffer. |
| 3 — `gpu_task<T>` coroutine + awaiters for use case B | TODO | Build on `gse::async::task` infrastructure; staging lives in the coroutine frame. |
| 4 — Timeline semaphore poller | TODO | Per-queue monotonic timeline; poll at frame begin to resume ready coroutines and drain the resource bin. |
| 5 — Migrate callers | TODO | Sweep `GpuBuffer.cppm`, `GpuImage.cppm`, `AccelerationStructure.cppm`, `GpuContext.cppm`. |
| 6 — Delete `add_transient_work` | TODO | Final cleanup once all sites are migrated. |

---

## Motivation

`gse::gpu::frame::add_transient_work` (`Engine/Engine/Source/Gpu/Device/GpuFrame.cppm:90`) is the only escape hatch for "I need to submit some commands outside the render graph." It is doing too much, and what it does is forced into a single shape regardless of the caller's actual needs.

### What the current API does on every call

```cpp
auto gse::gpu::frame::add_transient_work(auto&& commands) -> void {
    const auto& vk_device = m_device->logical_device();

    const vk::CommandBufferAllocateInfo alloc_info{
        .commandPool = std::bit_cast<vk::CommandPool>(m_device->command_config().pool_handle()),
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1,
    };

    auto buffers = vk_device.allocateCommandBuffers(alloc_info);
    vk::raii::CommandBuffer command_buffer = std::move(buffers[0]);

    constexpr vk::CommandBufferBeginInfo begin_info{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    };

    command_buffer.begin(begin_info);

    std::vector<vulkan::buffer_resource> transient_buffers;
    if constexpr (std::is_void_v<std::invoke_result_t<decltype(commands), vk::raii::CommandBuffer&>>) {
        std::invoke(std::forward<decltype(commands)>(commands), command_buffer);
    } else {
        transient_buffers = std::invoke(std::forward<decltype(commands)>(commands), command_buffer);
    }

    command_buffer.end();

    vk::raii::Fence fence = vk_device.createFence({});

    ...
    m_device->queue_config().submit_graphics(submit_info, std::bit_cast<gpu::fence_handle>(*fence));
    ...

    m_graveyard[m_current_frame].push_back({
        .command_buffer = std::move(command_buffer),
        .fence = std::move(fence),
        .transient_buffers = std::move(transient_buffers),
    });
}
```

Each call: allocates a primary command buffer, begins it, runs the callback, ends it, creates a fence, submits to the **graphics** queue, and stuffs the (cmd, fence, staging) triple into a per-frame "graveyard" that is reaped when that frame index is revisited.

### Two genuinely different use cases hidden under one API

**Use case A — frame-piggyback work.** Per-frame transitions and bookkeeping that don't need their own command buffer or fence; they only need to be ordered correctly with the frame's main work.

```cpp
auto transition_depth = [this]() {
    auto& depth = m_render_graph->depth_image();
    m_frame->add_transient_work([&depth](const vk::raii::CommandBuffer& cmd) {
        vulkan::transition_image_layout(depth,
            std::bit_cast<gpu::command_buffer_handle>(*cmd),
            image_layout::general,
            ...
        );
    });
};
```
(`Engine/Engine/Source/Gpu/GpuContext.cppm:197`)

**Use case B — async resource setup.** Texture upload, BLAS build, buffer fill. These need their own submit, often have staging that must outlive the GPU work, and the caller frequently wants to do something *after* completion — free CPU data, mark an asset ready, chain another step.

```cpp
frame.add_transient_work([&alloc, &resource, extent, pixel_data, data_size](const vk::raii::CommandBuffer& cmd) -> std::vector<vulkan::buffer_resource> {
    auto staging = alloc.create_buffer(
        gpu::buffer_create_info{
            .size = data_size,
            .usage = gpu::buffer_flag::transfer_src,
        },
        pixel_data
    );

    vulkan::transition_image_layout(resource, ...);
    ...
    return { std::move(staging) };
});
```
(`Engine/Engine/Source/Gpu/GpuImage.cppm:225`)

### Concrete pain points

1. **Three concerns in one lambda.** Recording, submission policy, and lifetime tracking are tangled. Every site negotiates all three even when it only cares about one.
2. **The dual-return convention.** `if constexpr (std::is_void_v<...>)` switches between "void" and "return a vector of staging buffers." Anything that isn't a `buffer_resource` (an image, an arbitrary RAII handle) cannot be retained.
3. **One submit per call.** Use case A pays a full allocate-begin-end-submit-fence per transition, every frame, forever. Multiple transitions in one frame cannot be batched.
4. **Forced graphics queue.** No way to say "this is a copy, run it on the transfer queue" or "this is a build, run it on a dedicated build queue."
5. **No completion signal.** Callers cannot wait, cannot chain, cannot know when staging memory is free. The graveyard reclaim happens on the next visit to the same frame index, which is the wrong granularity for both use cases.
6. **Initial loads are awkward.** Loading all assets and *then* starting the loop currently means hand-rolling a fence wait at the call site.
7. **Fence-per-submit.** A fresh `vk::raii::Fence` per call burns kernel objects when one timeline semaphore per queue would do.

---

## Target Architecture

Three orthogonal pieces. Each maps to one concern from the current tangled API.

```
frame_recorder        — use case A (record into the frame's cmd buffer)
gpu_task<T>           — use case B (one-shot async setup, awaitable)
frame_resource_bin    — generic, type-erased keep-alive keyed on timeline values
```

### `frame_recorder` — fire-and-forget frame work

A handle that appends commands into the frame's existing primary command buffer at well-defined points (`pre_frame`, `post_frame`). No submit, no fence, no graveyard.

```cpp
export namespace gse::gpu {
    class frame_recorder {
    public:
        auto pre_frame(
            std::move_only_function<void(command_buffer&)> commands
        ) -> void;

        auto post_frame(
            std::move_only_function<void(command_buffer&)> commands
        ) -> void;
    };
}
```

Recorded callbacks fire once per call, in registration order, at the corresponding hook within `frame::begin` / `frame::end`. Persistent transitions register inside an `on_recreate` callback exactly the way `transition_depth` already does — but with no extra command buffer or fence.

### `gpu_task<T>` — async setup as a coroutine

A coroutine type that piggy-backs on the existing `gse::async::task` machinery (`Engine/Engine/Source/Concurrency/AsyncTask.cppm`). The coroutine frame holds staging and any other RAII resources naturally; awaiters drive recording, submission, and completion.

```cpp
async::gpu_task<> upload_texture(
    image& img,
    std::span<const std::byte> data
);
```

Inside the body, three awaiter shapes:

| Awaiter | Resumes when | Yields |
|---|---|---|
| `co_await ctx.staging(size)` | Allocator returns buffer | `buffer_resource` |
| `co_await ctx.begin_transient(queue)` | Command buffer allocated and begun | `command_buffer&` |
| `co_await ctx.submit(cmd)` | Timeline semaphore reaches submit value | `void` |

The submit awaiter is the one that suspends the CPU coroutine on GPU completion. It records the timeline value to wait for and parks the handle in the queue's wait-list; the per-queue poller resumes it (see [Timeline poller](#timeline-poller-and-resumption)).

### `frame_resource_bin` — type-erased keep-alive

A bin of `unique_ptr<retained_base>` slots, each tagged with a `(queue, timeline_value)`. Anything that satisfies `move_constructible` can be dropped in. A single drain call at frame begin walks the bin, asks each queue what timeline value it has reached, and erases entries whose tag is `<=` that value.

```cpp
export namespace gse::gpu {
    class frame_resource_bin {
    public:
        template <typename T>
        auto retain(
            queue_id queue,
            std::uint64_t until_value,
            T resource
        ) -> void;

        auto drain(
            std::span<const queue_progress> progress
        ) -> void;
    };
}
```

This replaces both the `transient_buffers` field on `transient_gpu_work` and the `m_graveyard` vector-of-vectors on `frame`. It does not care what kind of object it holds.

### Timeline poller and resumption

Each queue owns one `vk::raii::Semaphore` of type `eTimeline` and a monotonic `std::uint64_t m_next_value`. Every submit increments the value, signals it on the timeline, and registers `(handle, value)` with the queue's wait-list. At frame begin:

```cpp
auto gse::gpu::queue::poll() -> void {
    const std::uint64_t reached = m_timeline.getCounterValue();
    while (!m_waiters.empty() && m_waiters.top().value <= reached) {
        auto handle = m_waiters.top().handle;
        m_waiters.pop();
        handle.resume();
    }
    m_progress = reached;
}
```

`getCounterValue()` is a non-blocking host query — no kernel wait, no fence allocation. The bin drain consumes the same `m_progress` value.

For initial loads, a `sync_wait` analog blocks the calling thread on the host with `vkWaitSemaphores(timeout=infinite)` against the same timeline. The same coroutine works synchronously and asynchronously.

---

## Examples

Every example below follows the project style guide: snake_case, `m_` prefix for members, declarations inside the namespace with one parameter per line, definitions outside with parameters collapsed, designated initializers each on their own line, no padding, no one-line `if`/lambda bodies, no redundant qualification.

### Example 1 — `frame_recorder` declaration

```cpp
export module gse.gpu:frame_recorder;

import std;

import :command_buffer;
import gse.core;

export namespace gse::gpu {
    class frame_recorder final : public non_copyable {
    public:
        ~frame_recorder(
        );

        frame_recorder(
            frame_recorder&&
        ) noexcept = default;

        auto operator=(
            frame_recorder&&
        ) noexcept -> frame_recorder& = default;

        auto pre_frame(
            std::move_only_function<void(command_buffer&)> commands
        ) -> void;

        auto post_frame(
            std::move_only_function<void(command_buffer&)> commands
        ) -> void;

        auto run_pre_frame(
            command_buffer& cmd
        ) -> void;

        auto run_post_frame(
            command_buffer& cmd
        ) -> void;

    private:
        std::vector<std::move_only_function<void(command_buffer&)>> m_pre;
        std::vector<std::move_only_function<void(command_buffer&)>> m_post;
    };
}
```

### Example 2 — `frame_recorder` definitions

```cpp
gse::gpu::frame_recorder::~frame_recorder() = default;

auto gse::gpu::frame_recorder::pre_frame(std::move_only_function<void(command_buffer&)> commands) -> void {
    m_pre.push_back(std::move(commands));
}

auto gse::gpu::frame_recorder::post_frame(std::move_only_function<void(command_buffer&)> commands) -> void {
    m_post.push_back(std::move(commands));
}

auto gse::gpu::frame_recorder::run_pre_frame(command_buffer& cmd) -> void {
    for (auto& fn : m_pre) {
        fn(cmd);
    }
}

auto gse::gpu::frame_recorder::run_post_frame(command_buffer& cmd) -> void {
    for (auto& fn : m_post) {
        fn(cmd);
    }
}
```

### Example 3 — Migrating the depth transition (use case A)

Before (`Engine/Engine/Source/Gpu/GpuContext.cppm:197`):

```cpp
auto transition_depth = [this]() {
    auto& depth = m_render_graph->depth_image();
    m_frame->add_transient_work([&depth](const vk::raii::CommandBuffer& cmd) {
        vulkan::transition_image_layout(depth,
            std::bit_cast<gpu::command_buffer_handle>(*cmd),
            image_layout::general,
            gpu::image_aspect_flag::depth,
            gpu::pipeline_stage_flag::top_of_pipe,
            {},
            gpu::pipeline_stage_flag::early_fragment_tests | gpu::pipeline_stage_flag::late_fragment_tests,
            gpu::access_flag::depth_stencil_attachment_write | gpu::access_flag::depth_stencil_attachment_read
        );
    });
};
transition_depth();
m_swapchain->on_recreate([transition_depth]() {
    transition_depth();
});
```

After:

```cpp
auto register_depth_transition = [this] {
    auto& depth = m_render_graph->depth_image();
    m_frame->recorder().pre_frame([&depth](command_buffer& cmd) {
        vulkan::transition_image_layout(
            depth,
            cmd.handle(),
            image_layout::general,
            gpu::image_aspect_flag::depth,
            gpu::pipeline_stage_flag::top_of_pipe,
            {},
            gpu::pipeline_stage_flag::early_fragment_tests | gpu::pipeline_stage_flag::late_fragment_tests,
            gpu::access_flag::depth_stencil_attachment_write | gpu::access_flag::depth_stencil_attachment_read
        );
    });
};
register_depth_transition();
m_swapchain->on_recreate(register_depth_transition);
```

No fence per frame. No extra submit. The transition is just a few extra commands on the frame's existing primary command buffer.

### Example 4 — `gpu_task` declaration

```cpp
export module gse.gpu:gpu_task;

import std;

import :command_buffer;
import :gpu_context_fwd;
import :handles;
import :queue;
import gse.concurrency;
import gse.core;

export namespace gse::async {
    template <typename T = void>
    class gpu_task;

    struct gpu_promise_base {
        gpu::context* m_context = nullptr;
        std::coroutine_handle<> m_continuation{ std::noop_coroutine() };
        std::exception_ptr m_exception;
        std::atomic<bool> m_started{ false };

        static auto initial_suspend(
        ) noexcept -> std::suspend_always;

        static auto final_suspend(
        ) noexcept -> final_awaiter;

        auto unhandled_exception(
        ) -> void;
    };

    template <typename T>
    struct gpu_value_promise : gpu_promise_base {
        std::optional<T> m_result;

        auto get_return_object(
        ) -> gpu_task<T>;

        auto return_value(
            T value
        ) -> void;

        auto result(
        ) -> T;
    };

    struct gpu_void_promise : gpu_promise_base {
        auto get_return_object(
        ) -> gpu_task<>;

        static auto return_void(
        ) noexcept -> void;

        auto result(
        ) const -> void;
    };

    template <typename T>
    class gpu_task {
    public:
        using promise_type = std::conditional_t<std::is_void_v<T>, gpu_void_promise, gpu_value_promise<T>>;
        using handle_type = std::coroutine_handle<promise_type>;

        gpu_task(
        ) noexcept = default;

        explicit gpu_task(
            handle_type h
        ) noexcept;

        ~gpu_task(
        );

        gpu_task(
            gpu_task&& other
        ) noexcept;

        auto operator=(
            gpu_task&& other
        ) noexcept -> gpu_task&;

        gpu_task(
            const gpu_task&
        ) = delete;

        auto operator=(
            const gpu_task&
        ) -> gpu_task& = delete;

        auto start(
            gpu::context& ctx
        ) -> void;

        auto done(
        ) const -> bool;

        auto operator co_await(
        ) noexcept -> awaiter;

    private:
        handle_type m_handle{};
    };
}
```

### Example 5 — `submit_awaiter` declaration

```cpp
export namespace gse::gpu {
    struct submit_awaiter {
        context* m_context;
        queue_id m_queue;
        std::uint64_t m_timeline_value;
        vk::raii::CommandBuffer m_command_buffer{ nullptr };
        std::vector<retained_handle> m_retained;

        auto await_ready(
        ) const noexcept -> bool;

        auto await_suspend(
            std::coroutine_handle<> caller
        ) -> bool;

        auto await_resume(
        ) -> void;
    };
}
```

### Example 6 — `submit_awaiter` definitions

```cpp
auto gse::gpu::submit_awaiter::await_ready() const noexcept -> bool {
    return m_context->queue(m_queue).reached(m_timeline_value);
}

auto gse::gpu::submit_awaiter::await_suspend(std::coroutine_handle<> caller) -> bool {
    auto& q = m_context->queue(m_queue);

    const gpu::submit_info submit{
        .command_buffers = std::span(&m_command_buffer, 1),
        .signal_timeline = {
            .semaphore = q.timeline_handle(),
            .value = m_timeline_value,
        },
    };
    q.submit(submit);

    auto& bin = m_context->frame().resource_bin();
    for (auto& slot : m_retained) {
        bin.retain(m_queue, m_timeline_value, std::move(slot));
    }
    bin.retain(m_queue, m_timeline_value, std::move(m_command_buffer));

    return q.park(m_timeline_value, caller);
}

auto gse::gpu::submit_awaiter::await_resume() -> void {
}
```

### Example 7 — Texture upload as a coroutine (use case B)

Before (`Engine/Engine/Source/Gpu/GpuImage.cppm:225`):

```cpp
frame.add_transient_work([&alloc, &resource, extent, pixel_data, data_size](const vk::raii::CommandBuffer& cmd) -> std::vector<vulkan::buffer_resource> {
    auto staging = alloc.create_buffer(
        gpu::buffer_create_info{
            .size = data_size,
            .usage = gpu::buffer_flag::transfer_src,
        },
        pixel_data
    );

    vulkan::transition_image_layout(resource,
        std::bit_cast<gpu::command_buffer_handle>(*cmd),
        gpu::image_layout::transfer_dst,
        gpu::image_aspect_flag::color,
        gpu::pipeline_stage_flag::top_of_pipe,
        {},
        gpu::pipeline_stage_flag::transfer,
        gpu::access_flag::transfer_write
    );

    const gpu::buffer_image_copy_region region{
        .buffer_offset = 0,
        ...
    };
    ...
    return { std::move(staging) };
});
```

After:

```cpp
async::gpu_task<> gse::gpu::image::upload(context& ctx, std::span<const std::byte> pixel_data, extent_2d extent) {
    auto staging = co_await ctx.staging({
        .size = pixel_data.size_bytes(),
        .usage = gpu::buffer_flag::transfer_src,
    });
    gse::memcpy(staging.mapped(), pixel_data.data(), pixel_data.size_bytes());

    auto cmd = co_await ctx.begin_transient(queue_id::transfer);

    vulkan::transition_image_layout(
        m_resource,
        cmd.handle(),
        gpu::image_layout::transfer_dst,
        gpu::image_aspect_flag::color,
        gpu::pipeline_stage_flag::top_of_pipe,
        {},
        gpu::pipeline_stage_flag::transfer,
        gpu::access_flag::transfer_write
    );

    const gpu::buffer_image_copy_region region{
        .buffer_offset = 0,
        .buffer_row_length = 0,
        .buffer_image_height = 0,
        .image_subresource = {
            .aspect = gpu::image_aspect_flag::color,
            .mip_level = 0,
            .base_array_layer = 0,
            .layer_count = 1,
        },
        .image_offset = { 0, 0, 0 },
        .image_extent = { extent.width, extent.height, 1 },
    };
    cmd.copy_buffer_to_image(staging.handle(), m_resource.handle(), region);

    co_await ctx.submit(std::move(cmd), keep_alive(std::move(staging)));
}
```

`staging` lives in the coroutine frame until `submit` packages it into the resource bin — the dual-return-type hack disappears.

### Example 8 — Buffer batched uploads (use case B)

Before (`Engine/Engine/Source/Gpu/GpuBuffer.cppm:200`):

```cpp
frame.add_transient_work([&alloc, entries = std::move(entries)](const auto& cmd) {
    std::vector<vulkan::buffer_resource> transient;
    transient.reserve(entries.size());

    for (const auto& [dst, data, size, offset] : entries) {
        auto staging = alloc.create_buffer(gpu::buffer_create_info{
            .size = size,
            .usage = gpu::buffer_flag::transfer_src,
        }, data);

        vulkan::commands(std::bit_cast<gpu::command_buffer_handle>(*cmd)).copy_buffer(
            staging.handle(),
            std::bit_cast<vulkan::buffer_handle>(dst),
            gpu::buffer_copy_region{
                .src_offset = 0,
                .dst_offset = offset,
                .size = size,
            }
        );
        transient.push_back(std::move(staging));
    }
    return transient;
});
```

After:

```cpp
async::gpu_task<> gse::gpu::buffer::upload_many(context& ctx, std::vector<upload_entry> entries) {
    std::vector<vulkan::buffer_resource> stagings;
    stagings.reserve(entries.size());

    for (const auto& entry : entries) {
        auto staging = co_await ctx.staging({
            .size = entry.size,
            .usage = gpu::buffer_flag::transfer_src,
        });
        gse::memcpy(staging.mapped(), entry.data, entry.size);
        stagings.push_back(std::move(staging));
    }

    auto cmd = co_await ctx.begin_transient(queue_id::transfer);
    for (std::size_t i = 0; i < entries.size(); ++i) {
        cmd.copy_buffer(
            stagings[i].handle(),
            entries[i].dst,
            gpu::buffer_copy_region{
                .src_offset = 0,
                .dst_offset = entries[i].offset,
                .size = entries[i].size,
            }
        );
    }

    co_await ctx.submit(std::move(cmd), keep_alive(std::move(stagings)));
}
```

### Example 9 — BLAS build with chained barrier (use case B)

Before (`Engine/Engine/Source/Graphics/Raytracing/AccelerationStructure.cppm:232`): allocate scratch inside the lambda, build, return scratch in the staging vector. The "use the BLAS" step has to be a separate `add_transient_work` call with no completion ordering between them.

After:

```cpp
async::gpu_task<> gse::vulkan::build_blas(context& ctx, gpu::acceleration_structure_handle as_handle, gpu::geometry geometry, std::uint32_t prim_count) {
    auto& dev = ctx.device();

    const vk::DeviceSize scratch_size = acceleration_structure_scratch_size(dev, geometry, prim_count);
    const vk::DeviceSize scratch_alignment = acceleration_structure_scratch_alignment(dev);

    auto scratch = co_await ctx.staging({
        .size = scratch_size + scratch_alignment,
        .usage = gpu::buffer_flag::storage,
    });

    const auto scratch_addr = dev.logical_device().getBufferAddress({ .buffer = scratch.buffer });
    const gpu::device_address aligned_scratch = (scratch_addr + scratch_alignment - 1) & ~(scratch_alignment - 1);

    auto cmd = co_await ctx.begin_transient(queue_id::compute);

    const gpu::acceleration_structure_build_geometry_info build_info{
        .type = gpu::acceleration_structure_type::bottom_level,
        .flags = gpu::build_acceleration_structure_flag::prefer_fast_build,
        .mode = gpu::build_acceleration_structure_mode::build,
        .dst = as_handle,
        .geometries = std::span(&geometry, 1),
        .scratch_address = aligned_scratch,
    };

    const gpu::acceleration_structure_build_range_info range{
        .primitive_count = prim_count,
        .primitive_offset = 0,
        .first_vertex = 0,
        .transform_offset = 0,
    };

    cmd.build_acceleration_structures(std::span(&build_info, 1), std::span(&range, 1));

    co_await ctx.submit(std::move(cmd), keep_alive(std::move(scratch)));
}
```

### Example 10 — Chained setup (the case the current API cannot express)

```cpp
async::gpu_task<> gse::gpu::load_skinned_mesh(context& ctx, mesh_data data) {
    co_await buffer::upload_many(ctx, std::move(data.vertex_uploads));
    co_await buffer::upload_many(ctx, std::move(data.index_uploads));
    co_await build_blas(ctx, data.as_handle, data.geometry, data.prim_count);
}
```

Each step waits for the previous one to land on the GPU before submitting. With the old API this required the caller to wire fence-waits by hand, which nobody did, which is why these were sequenced by "submit them all and hope" instead of "submit them in order."

### Example 11 — Initial-load synchronous path

```cpp
auto gse::gpu::initial_load(context& ctx, asset_set assets) -> void {
    async::sync_wait(load_all(ctx, std::move(assets)));
}
```

Same `gpu_task<>` body. `sync_wait` blocks the caller until the timeline reaches the final submit value. No special API for "load before the loop starts."

### Example 12 — Image transitions via `frame_recorder` (use case A)

`image::transition_to` (`Engine/Engine/Source/Gpu/GpuImage.cppm:189`) currently issues a one-time submit with its own fence to flip layout. It should register on the frame recorder instead — the layout flip is a barrier, not a real GPU workload.

```cpp
auto gse::gpu::image::transition_to(context& ctx, image_layout target_layout) -> void {
    const bool is_depth = m_format == format::depth32_sfloat || m_format == format::depth24_stencil8;
    const std::uint32_t layers = static_cast<std::uint32_t>(m_layer_views.empty() ? 1u : m_layer_views.size());
    const auto img = m_resource.handle();
    const auto aspect = is_depth ? gpu::image_aspect_flag::depth : gpu::image_aspect_flag::color;

    ctx.frame().recorder().pre_frame([img, target_layout, aspect, layers, is_depth](command_buffer& cmd) {
        const auto dst_stages = is_depth
            ? (gpu::pipeline_stage_flag::early_fragment_tests | gpu::pipeline_stage_flag::late_fragment_tests)
            : gpu::pipeline_stage_flags{ gpu::pipeline_stage_flag::all_commands };
        const auto dst_access = is_depth
            ? (gpu::access_flag::depth_stencil_attachment_write | gpu::access_flag::depth_stencil_attachment_read)
            : gpu::access_flags{ gpu::access_flag::shader_read };

        const gpu::image_barrier barrier{
            .src_stages = gpu::pipeline_stage_flag::top_of_pipe,
            .src_access = {},
            .dst_stages = dst_stages,
            .dst_access = dst_access,
            .old_layout = gpu::image_layout::undefined,
            .new_layout = target_layout,
            .image = img,
            .aspects = aspect,
            .base_mip_level = 0,
            .level_count = 1,
            .base_array_layer = 0,
            .layer_count = layers,
        };
        cmd.image_barrier(barrier);
    });
}
```

### Example 13 — `frame_resource_bin` declaration

```cpp
export module gse.gpu:frame_resource_bin;

import std;

import :handles;
import gse.core;

export namespace gse::gpu {
    struct queue_progress {
        queue_id queue;
        std::uint64_t reached_value;
    };

    class frame_resource_bin final : public non_copyable {
    public:
        ~frame_resource_bin(
        );

        frame_resource_bin(
            frame_resource_bin&&
        ) noexcept = default;

        auto operator=(
            frame_resource_bin&&
        ) noexcept -> frame_resource_bin& = default;

        template <typename T>
        auto retain(
            queue_id queue,
            std::uint64_t until_value,
            T resource
        ) -> void;

        auto drain(
            std::span<const queue_progress> progress
        ) -> void;

    private:
        struct retained_base {
            virtual ~retained_base() = default;
        };

        template <typename T>
        struct retained_holder : retained_base {
            T m_value;
        };

        struct slot {
            queue_id m_queue;
            std::uint64_t m_until_value;
            std::unique_ptr<retained_base> m_holder;
        };

        std::vector<slot> m_slots;
    };
}
```

### Example 14 — `frame_resource_bin` definitions

```cpp
gse::gpu::frame_resource_bin::~frame_resource_bin() = default;

template <typename T>
auto gse::gpu::frame_resource_bin::retain(const queue_id queue, const std::uint64_t until_value, T resource) -> void {
    auto holder = std::make_unique<retained_holder<T>>();
    holder->m_value = std::move(resource);
    m_slots.push_back({
        .m_queue = queue,
        .m_until_value = until_value,
        .m_holder = std::move(holder),
    });
}

auto gse::gpu::frame_resource_bin::drain(std::span<const queue_progress> progress) -> void {
    auto reached = [progress](const queue_id q) -> std::uint64_t {
        for (const auto& p : progress) {
            if (p.queue == q) {
                return p.reached_value;
            }
        }
        return 0;
    };

    std::erase_if(m_slots, [&](const slot& s) {
        return s.m_until_value <= reached(s.m_queue);
    });
}
```

### Example 15 — Queue with timeline semaphore

```cpp
export namespace gse::gpu {
    class queue final : public non_copyable {
    public:
        ~queue(
        );

        queue(
            queue&&
        ) noexcept = default;

        auto operator=(
            queue&&
        ) noexcept -> queue& = default;

        [[nodiscard]] static auto create(
            const vulkan::device_config& device,
            queue_id id,
            std::uint32_t family
        ) -> queue;

        [[nodiscard]] auto next_value(
        ) -> std::uint64_t;

        [[nodiscard]] auto reached(
            std::uint64_t value
        ) const -> bool;

        [[nodiscard]] auto timeline_handle(
        ) const -> gpu::semaphore_handle;

        auto submit(
            const submit_info& info
        ) -> void;

        auto park(
            std::uint64_t value,
            std::coroutine_handle<> handle
        ) -> bool;

        auto poll(
        ) -> std::uint64_t;

    private:
        struct waiter {
            std::uint64_t m_value;
            std::coroutine_handle<> m_handle;
        };

        vk::raii::Queue m_vk_queue;
        vk::raii::Semaphore m_timeline;
        std::uint64_t m_next_value = 0;
        std::uint64_t m_progress = 0;
        std::priority_queue<waiter> m_waiters;
        queue_id m_id;
    };
}
```

### Example 16 — Recorder hooks inside `frame::begin` / `frame::end`

```cpp
auto gse::gpu::frame::begin(window& win) -> std::expected<frame_token, frame_status> {
    poll_queues();
    drain_resource_bin();

    auto status = acquire_image(win);
    if (!status.has_value()) {
        return std::unexpected(status.error());
    }

    auto& cmd = m_device->command_config().frame_command_buffer(m_current_frame);
    begin_command_buffer(cmd);
    m_recorder.run_pre_frame(cmd);

    return *status;
}

auto gse::gpu::frame::end(window& win) -> void {
    auto& cmd = m_device->command_config().frame_command_buffer(m_current_frame);
    m_recorder.run_post_frame(cmd);
    end_command_buffer(cmd);
    submit_frame(win);
    present(win);
    advance_frame();
}
```

---

## Considerations

### Lifetime of the coroutine frame

`gpu_task<>` allocates its frame on the heap by default. Two options:

1. **Default allocator.** Simple, allocates per upload. Fine for one-shots.
2. **Frame arena allocator.** Override `operator new`/`operator delete` on the promise to pull from `gse::concurrency::frame_arena`. Already in use by `async::task` (`Engine/Engine/Source/Concurrency/AsyncTask.cppm:43`). Recommended path.

### Awaiter resumption thread

When the timeline poller resumes a coroutine, `handle.resume()` runs on whichever thread polls. Two options:

1. **Resume on the main thread at frame begin.** Simple, deterministic, no synchronization between coroutine continuation and main-thread state. Latency = "at most one frame after the GPU finishes." Fine for asset loads, marginal for tight upload-then-use loops.
2. **Resume on a worker.** Dedicated polling thread calls `handle.resume()` directly; the coroutine continuation runs on that thread. Lower latency, but every continuation must be thread-safe with respect to whatever else it touches. Project already has `task::group` workers (`Engine/Engine/Source/Concurrency/Task.cppm`) that could serve.

Pick 1 first — it composes with the existing scheduler's "frame begin" hook and is hard to misuse. Move to 2 only if measured.

### Fence vs. timeline semaphore

Vulkan timeline semaphores subsume fences and binary semaphores for our needs. One per queue, monotonic counter, host queryable, queue-waitable. The current `vk::raii::Fence`-per-submit pattern is the only thing forcing the graveyard structure to be per-frame-index — a timeline lets the resource bin scale to as many in-flight submits as we want.

Vulkan 1.2 core feature; already required by the engine. No extension dance.

### Cross-queue synchronization

When a coroutine submits to the transfer queue and a later coroutine on the graphics queue depends on the result, the second submit must `wait` on the first's timeline value. The submit awaiter should accept an optional `std::span<const wait_value>` so this is expressible:

```cpp
co_await ctx.submit(std::move(cmd_a), keep_alive(...));
auto cmd_b = co_await ctx.begin_transient(queue_id::graphics);
cmd_b.do_thing();
co_await ctx.submit(std::move(cmd_b), wait_on(queue_id::transfer, prev_value));
```

If we always run `co_await prev_task` before submitting the dependent one, the wait is implicit at the host level — but the GPU still needs the cross-queue wait recorded on the submit. Worth pinning down in the awaiter contract before phase 3.

### Command pool ownership

Transient command buffers should come from per-queue, per-frame-in-flight pools — not from the same pool that backs the frame's primary command buffer (`command_config::pool_handle()` in `Engine/Engine/Source/Gpu/Vulkan/CommandPools.cppm:42`). Reset semantics differ; co-mingling them invites nasty bugs at frame boundaries.

```cpp
struct transient_pool_set {
    std::array<vk::raii::CommandPool, max_frames_in_flight> m_pools;
};
```

One set per queue type. `begin_transient` allocates from `m_pools[current_frame]`; the bin returns the buffer when the timeline value is reached.

### Exception safety in coroutines

If the body throws between `begin_transient` and `submit`, the partially recorded command buffer needs to come home. Two paths:

1. The `command_buffer` returned by `begin_transient` is a move-only RAII type that, on destruction without a corresponding `submit`, returns itself to the pool. Symmetric with how `vk::raii::CommandBuffer` already works.
2. The submit awaiter's `await_resume` is a no-op; failure paths drop the buffer through normal stack unwinding into the pool.

(1) is the right answer. Same shape as the proposed move-only `recording_context` in the coroutine scheduler unification doc.

### Interaction with the render graph

Render-graph passes already use `record_awaitable` (`Engine/Engine/Source/Gpu/Device/RenderGraph.cppm`). `gpu_task<>` is for work *outside* the render graph — it never participates in graph topology. The two systems share only the underlying queue and timeline, not awaiter types.

A pass that wants to launch a `gpu_task<>` (e.g. on first frame) must `co_await` it inside the pass body, which means the pass yields the secondary command buffer back to the graph for the duration of the wait. Acceptable for one-shot setup, not for per-frame work — that's why use case A exists.

### Migration order

1. Land `frame_resource_bin` and `queue` with timeline semaphore. Both can coexist with the current `add_transient_work` (which keeps using fences) — no observable change.
2. Land `frame_recorder`. Migrate `transition_depth` and `image::transition_to` first; they have no completion semantics to reason about.
3. Land `gpu_task<>`, `staging_awaiter`, `begin_transient_awaiter`, `submit_awaiter`. Migrate `image::upload`, `image::upload_cube`, `buffer::upload_many`. Run the asset-load suite end-to-end before touching BLAS.
4. Migrate `build_blas` and any other compute-side transients.
5. Delete `frame::add_transient_work`, `transient_gpu_work`, and `m_graveyard`.

### What this does not change

- The render graph and per-pass `record_awaitable` (`Engine/Engine/Source/Gpu/Device/RenderGraph.cppm`).
- `frame::add_wait_semaphore` — still the right shape for "the next frame's queue submit must wait on this external semaphore."
- Bindless descriptor management.
- Anything in `task_context` or `update_context`.

### Risks

| Risk | Mitigation |
|---|---|
| Coroutine frame allocations show up in profiles | Move promises to `frame_arena` allocator, same as `async::task`. |
| Resumption-on-main-thread adds one-frame latency to async loads | Acceptable for asset loads; revisit only if a callsite cares. |
| Cross-queue synchronization gets forgotten | Submit awaiter requires explicit `wait_on(...)` for cross-queue deps; no implicit ordering. |
| Resource bin grows without bound on a stuck queue | Bin tracks high-water mark; assert above a threshold. Stuck queue is already a hang anyway. |
| Mis-use: `co_await` a `gpu_task<>` from a render-graph pass body | Document the boundary. Long-term: a tag type on the awaiter to make graph-internal awaits explicit. |

---

## Open Questions

1. Is there a real per-frame compute-queue use case today, or is graphics-only enough for phase 3? (`AccelerationStructure.cppm:232` is the only candidate I see.)
2. Should `begin_transient` return a `command_buffer&` reference into the pool (the awaiter holds the storage) or move ownership out? Move is cleaner for exception safety; reference is one less pointer chase.
3. Does `staging` belong on `gpu::context` or on the allocator directly? Allocator is more truthful; context is a convenient one-stop shop. Lean toward context-as-facade with the implementation living on the allocator.
4. `keep_alive(...)` variadic vs. `std::span<retained_handle>` parameter on `submit`. Variadic is friendlier at call sites; span is friendlier inside generic helpers. Pick variadic, dispatch internally.

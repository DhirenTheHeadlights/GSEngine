export module gse.gpu:vulkan_commands;

import std;
import vulkan;

import :handles;
import :types;
import :vulkan_allocation;
import :vulkan_buffer;
import :vulkan_image;

import gse.math;

export namespace gse::vulkan {
    struct command_buffer;
    class pipeline;
    class pipeline_layout;
    class query_pool;
}

export namespace gse::gpu {
    struct buffer_barrier {
        pipeline_stage_flags src_stages;
        access_flags src_access;
        pipeline_stage_flags dst_stages;
        access_flags dst_access;
        gpu::handle<vulkan::buffer> buffer;
        device_size offset = 0;
        device_size size = 0;
    };

    struct image_barrier {
        pipeline_stage_flags src_stages;
        access_flags src_access;
        pipeline_stage_flags dst_stages;
        access_flags dst_access;
        image_layout old_layout = image_layout::undefined;
        image_layout new_layout = image_layout::undefined;
        gpu::handle<vulkan::image> image;
        image_aspect_flags aspects;
        std::uint32_t base_mip_level = 0;
        std::uint32_t level_count = 1;
        std::uint32_t base_array_layer = 0;
        std::uint32_t layer_count = 1;
    };

    struct dependency_info {
        std::span<const memory_barrier> memory_barriers;
        std::span<const buffer_barrier> buffer_barriers;
        std::span<const image_barrier> image_barriers;
    };

    struct rendering_attachment_info {
        gpu::handle<vulkan::image_view> image_view;
        image_layout layout = image_layout::undefined;
        load_op load = load_op::dont_care;
        store_op store = store_op::dont_care;
        color_clear color_clear_value;
        depth_clear depth_clear_value;
    };

    struct rendering_info {
        gse::rect_t<vec2i> render_area;
        std::uint32_t layer_count = 1;
        std::span<const rendering_attachment_info> color_attachments;
        const rendering_attachment_info* depth_attachment = nullptr;
        const rendering_attachment_info* stencil_attachment = nullptr;
    };
}

export namespace gse::vulkan {
    class commands {
    public:
        commands() = default;

        commands(
            gpu::handle<command_buffer> cmd
        );

        [[nodiscard]] auto native(
        ) const -> gpu::handle<command_buffer>;

        explicit operator bool(
        ) const;

        auto begin(
        ) const -> void;

        auto end(
        ) const -> void;

        auto reset(
        ) const -> void;

        auto begin_rendering(
            const gpu::rendering_info& info
        ) const -> void;

        auto begin_rendering(
            const vk::RenderingInfo& info
        ) const -> void;

        auto end_rendering(
        ) const -> void;

        auto pipeline_barrier(
            const gpu::dependency_info& dep
        ) const -> void;

        auto pipeline_barrier(
            const vk::DependencyInfo& dep
        ) const -> void;

        auto bind_pipeline(
            gpu::bind_point point,
            gpu::handle<pipeline> pipeline
        ) const -> void;

        auto push_constants(
            gpu::handle<pipeline_layout> layout,
            gpu::stage_flags stages,
            std::uint32_t offset,
            std::uint32_t size,
            const void* data
        ) const -> void;

        auto bind_vertex_buffers(
            std::uint32_t first_binding,
            std::span<const gpu::handle<buffer>> buffers,
            std::span<const gpu::device_size> offsets
        ) const -> void;

        auto bind_index_buffer_2(
            gpu::handle<buffer> buffer,
            gpu::device_size offset,
            gpu::device_size size,
            gpu::index_type type
        ) const -> void;

        auto draw_indexed_indirect(
            gpu::handle<buffer> buffer,
            gpu::device_size offset,
            std::uint32_t draw_count,
            std::uint32_t stride
        ) const -> void;

        auto dispatch(
            std::uint32_t x,
            std::uint32_t y,
            std::uint32_t z
        ) const -> void;

        auto dispatch_indirect(
            gpu::handle<buffer> buffer,
            gpu::device_size offset
        ) const -> void;

        auto set_viewport(
            const gpu::viewport& viewport
        ) const -> void;

        auto set_scissor(
            const gse::rect_t<vec2i>& scissor
        ) const -> void;

        auto copy_buffer(
            gpu::handle<buffer> src,
            gpu::handle<buffer> dst,
            const gpu::buffer_copy_region& region
        ) const -> void;

        auto copy_buffer_to_image(
            gpu::handle<buffer> src,
            gpu::handle<image> dst,
            gpu::image_layout dst_layout,
            std::span<const gpu::buffer_image_copy_region> regions
        ) const -> void;

        auto copy_image_to_buffer(
            gpu::handle<image> src,
            gpu::image_layout src_layout,
            gpu::handle<buffer> dst,
            std::span<const gpu::buffer_image_copy_region> regions
        ) const -> void;

        auto blit_image(
            gpu::handle<image> src,
            gpu::image_layout src_layout,
            gpu::handle<image> dst,
            gpu::image_layout dst_layout,
            const gpu::image_blit_region& region,
            gpu::sampler_filter filter
        ) const -> void;

        auto copy_image(
            gpu::handle<image> src,
            gpu::image_layout src_layout,
            gpu::handle<image> dst,
            gpu::image_layout dst_layout,
            const gpu::image_copy_region& region
        ) const -> void;

        auto draw(
            std::uint32_t vertex_count,
            std::uint32_t instance_count,
            std::uint32_t first_vertex,
            std::uint32_t first_instance
        ) const -> void;

        auto draw_indexed(
            std::uint32_t index_count,
            std::uint32_t instance_count,
            std::uint32_t first_index,
            std::int32_t vertex_offset,
            std::uint32_t first_instance
        ) const -> void;

        auto write_timestamp(
            gpu::pipeline_stage_flags stage,
            gpu::handle<query_pool> query_pool,
            std::uint32_t query
        ) const -> void;

        auto reset_query_pool(
            gpu::handle<query_pool> query_pool,
            std::uint32_t first_query,
            std::uint32_t query_count
        ) const -> void;

        auto draw_mesh_tasks(
            std::uint32_t x,
            std::uint32_t y,
            std::uint32_t z
        ) const -> void;

        auto build_acceleration_structures(
            const gpu::acceleration_structure_build_geometry_info& build_info,
            std::span<const gpu::acceleration_structure_build_range_info* const> range_infos
        ) const -> void;

        auto build_acceleration_structures(
            const vk::AccelerationStructureBuildGeometryInfoKHR& build_info,
            std::span<const vk::AccelerationStructureBuildRangeInfoKHR* const> range_infos
        ) const -> void;

    private:
        [[nodiscard]] auto raw(
        ) const -> vk::CommandBuffer;

        gpu::handle<command_buffer> m_cmd;
    };
}

gse::vulkan::commands::commands(const gpu::handle<command_buffer> cmd)
    : m_cmd(cmd) {}

auto gse::vulkan::commands::native() const -> gpu::handle<command_buffer> {
    return m_cmd;
}

gse::vulkan::commands::operator bool() const {
    return static_cast<bool>(m_cmd);
}

auto gse::vulkan::commands::raw() const -> vk::CommandBuffer {
    return std::bit_cast<vk::CommandBuffer>(m_cmd);
}

auto gse::vulkan::commands::begin() const -> void {
    raw().begin(vk::CommandBufferBeginInfo{});
}

auto gse::vulkan::commands::end() const -> void {
    raw().end();
}

auto gse::vulkan::commands::reset() const -> void {
    raw().reset();
}

namespace {
    auto build_vk_attachment(const gse::gpu::rendering_attachment_info& att, const bool is_depth) -> vk::RenderingAttachmentInfo {
        vk::ClearValue clear{};
        if (is_depth) {
            clear.depthStencil = vk::ClearDepthStencilValue{ att.depth_clear_value.depth, 0 };
        } else {
            clear.color = vk::ClearColorValue{ std::array{
                att.color_clear_value.r,
                att.color_clear_value.g,
                att.color_clear_value.b,
                att.color_clear_value.a,
            } };
        }
        return vk::RenderingAttachmentInfo{
            .imageView = std::bit_cast<vk::ImageView>(att.image_view),
            .imageLayout = gse::vulkan::to_vk(att.layout),
            .loadOp = gse::vulkan::to_vk(att.load),
            .storeOp = gse::vulkan::to_vk(att.store),
            .clearValue = clear,
        };
    }

    struct rendering_scratch {
        std::vector<vk::RenderingAttachmentInfo> color;
        std::optional<vk::RenderingAttachmentInfo> depth;
        std::optional<vk::RenderingAttachmentInfo> stencil;
    };

    auto build_vk_rendering_info(
        const gse::gpu::rendering_info& info,
        rendering_scratch& scratch
    ) -> vk::RenderingInfo {
        scratch.color.reserve(info.color_attachments.size());
        for (const auto& a : info.color_attachments) {
            scratch.color.push_back(build_vk_attachment(a, false));
        }
        if (info.depth_attachment) {
            scratch.depth = build_vk_attachment(*info.depth_attachment, true);
        }
        if (info.stencil_attachment) {
            scratch.stencil = build_vk_attachment(*info.stencil_attachment, true);
        }
        const auto min = info.render_area.min();
        const auto size = info.render_area.size();
        return vk::RenderingInfo{
            .renderArea = vk::Rect2D{
                .offset = vk::Offset2D{ min.x(), min.y() },
                .extent = vk::Extent2D{ static_cast<std::uint32_t>(size.x()), static_cast<std::uint32_t>(size.y()) },
            },
            .layerCount = info.layer_count,
            .colorAttachmentCount = static_cast<std::uint32_t>(scratch.color.size()),
            .pColorAttachments = scratch.color.data(),
            .pDepthAttachment = scratch.depth ? &*scratch.depth : nullptr,
            .pStencilAttachment = scratch.stencil ? &*scratch.stencil : nullptr,
        };
    }

    struct dependency_scratch {
        std::vector<vk::MemoryBarrier2> memory;
        std::vector<vk::BufferMemoryBarrier2> buffer;
        std::vector<vk::ImageMemoryBarrier2> image;
    };

    auto build_vk_dependency_info(
        const gse::gpu::dependency_info& dep,
        dependency_scratch& scratch
    ) -> vk::DependencyInfo {
        scratch.memory.reserve(dep.memory_barriers.size());
        for (const auto& b : dep.memory_barriers) {
            scratch.memory.push_back(vk::MemoryBarrier2{
                .srcStageMask = gse::vulkan::to_vk(b.src_stages),
                .srcAccessMask = gse::vulkan::to_vk(b.src_access),
                .dstStageMask = gse::vulkan::to_vk(b.dst_stages),
                .dstAccessMask = gse::vulkan::to_vk(b.dst_access),
            });
        }
        scratch.buffer.reserve(dep.buffer_barriers.size());
        for (const auto& b : dep.buffer_barriers) {
            scratch.buffer.push_back(vk::BufferMemoryBarrier2{
                .srcStageMask = gse::vulkan::to_vk(b.src_stages),
                .srcAccessMask = gse::vulkan::to_vk(b.src_access),
                .dstStageMask = gse::vulkan::to_vk(b.dst_stages),
                .dstAccessMask = gse::vulkan::to_vk(b.dst_access),
                .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                .buffer = std::bit_cast<vk::Buffer>(b.buffer),
                .offset = b.offset,
                .size = b.size == 0 ? vk::WholeSize : b.size,
            });
        }
        scratch.image.reserve(dep.image_barriers.size());
        for (const auto& b : dep.image_barriers) {
            scratch.image.push_back(vk::ImageMemoryBarrier2{
                .srcStageMask = gse::vulkan::to_vk(b.src_stages),
                .srcAccessMask = gse::vulkan::to_vk(b.src_access),
                .dstStageMask = gse::vulkan::to_vk(b.dst_stages),
                .dstAccessMask = gse::vulkan::to_vk(b.dst_access),
                .oldLayout = gse::vulkan::to_vk(b.old_layout),
                .newLayout = gse::vulkan::to_vk(b.new_layout),
                .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
                .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
                .image = std::bit_cast<vk::Image>(b.image),
                .subresourceRange = vk::ImageSubresourceRange{
                    .aspectMask = gse::vulkan::to_vk(b.aspects),
                    .baseMipLevel = b.base_mip_level,
                    .levelCount = b.level_count,
                    .baseArrayLayer = b.base_array_layer,
                    .layerCount = b.layer_count,
                },
            });
        }
        return vk::DependencyInfo{
            .memoryBarrierCount = static_cast<std::uint32_t>(scratch.memory.size()),
            .pMemoryBarriers = scratch.memory.data(),
            .bufferMemoryBarrierCount = static_cast<std::uint32_t>(scratch.buffer.size()),
            .pBufferMemoryBarriers = scratch.buffer.data(),
            .imageMemoryBarrierCount = static_cast<std::uint32_t>(scratch.image.size()),
            .pImageMemoryBarriers = scratch.image.data(),
        };
    }
}

auto gse::vulkan::commands::begin_rendering(const gpu::rendering_info& info) const -> void {
    rendering_scratch scratch;
    const auto vk_info = build_vk_rendering_info(info, scratch);
    raw().beginRendering(vk_info);
}

auto gse::vulkan::commands::begin_rendering(const vk::RenderingInfo& info) const -> void {
    raw().beginRendering(info);
}

auto gse::vulkan::commands::end_rendering() const -> void {
    raw().endRendering();
}

auto gse::vulkan::commands::pipeline_barrier(const gpu::dependency_info& dep) const -> void {
    dependency_scratch scratch;
    const auto vk_dep = build_vk_dependency_info(dep, scratch);
    raw().pipelineBarrier2(vk_dep);
}

auto gse::vulkan::commands::pipeline_barrier(const vk::DependencyInfo& dep) const -> void {
    raw().pipelineBarrier2(dep);
}

auto gse::vulkan::commands::bind_pipeline(const gpu::bind_point point, const gpu::handle<pipeline> pipeline) const -> void {
    raw().bindPipeline(to_vk(point), std::bit_cast<vk::Pipeline>(pipeline));
}

auto gse::vulkan::commands::push_constants(const gpu::handle<pipeline_layout> layout, const gpu::stage_flags stages, const std::uint32_t offset, const std::uint32_t size, const void* data) const -> void {
    raw().pushConstants(std::bit_cast<vk::PipelineLayout>(layout), to_vk(stages), offset, size, data);
}

auto gse::vulkan::commands::bind_vertex_buffers(const std::uint32_t first_binding, const std::span<const gpu::handle<buffer>> buffers, const std::span<const gpu::device_size> offsets) const -> void {
    std::vector<vk::Buffer> vk_buffers;
    vk_buffers.reserve(buffers.size());
    for (const auto h : buffers) {
        vk_buffers.push_back(std::bit_cast<vk::Buffer>(h));
    }
    raw().bindVertexBuffers(
        first_binding,
        static_cast<std::uint32_t>(vk_buffers.size()),
        vk_buffers.data(),
        offsets.data()
    );
}

auto gse::vulkan::commands::bind_index_buffer_2(const gpu::handle<buffer> buffer, const gpu::device_size offset, const gpu::device_size size, const gpu::index_type type) const -> void {
    raw().bindIndexBuffer2KHR(std::bit_cast<vk::Buffer>(buffer), offset, size, to_vk(type));
}

auto gse::vulkan::commands::draw_indexed_indirect(const gpu::handle<buffer> buffer, const gpu::device_size offset, const std::uint32_t draw_count, const std::uint32_t stride) const -> void {
    raw().drawIndexedIndirect(std::bit_cast<vk::Buffer>(buffer), offset, draw_count, stride);
}

auto gse::vulkan::commands::dispatch(const std::uint32_t x, const std::uint32_t y, const std::uint32_t z) const -> void {
    raw().dispatch(x, y, z);
}

auto gse::vulkan::commands::dispatch_indirect(const gpu::handle<buffer> buffer, const gpu::device_size offset) const -> void {
    raw().dispatchIndirect(std::bit_cast<vk::Buffer>(buffer), offset);
}

auto gse::vulkan::commands::set_viewport(const gpu::viewport& viewport) const -> void {
    raw().setViewport(0, to_vk(viewport));
}

auto gse::vulkan::commands::set_scissor(const gse::rect_t<vec2i>& scissor) const -> void {
    const auto min = scissor.min();
    const auto size = scissor.size();
    const vk::Rect2D rect{
        .offset = { min.x(), min.y() },
        .extent = { static_cast<std::uint32_t>(size.x()), static_cast<std::uint32_t>(size.y()) },
    };
    raw().setScissor(0, rect);
}

auto gse::vulkan::commands::copy_buffer(const gpu::handle<buffer> src, const gpu::handle<buffer> dst, const gpu::buffer_copy_region& region) const -> void {
    raw().copyBuffer(std::bit_cast<vk::Buffer>(src), std::bit_cast<vk::Buffer>(dst), to_vk(region));
}

auto gse::vulkan::commands::copy_buffer_to_image(const gpu::handle<buffer> src, const gpu::handle<image> dst, const gpu::image_layout dst_layout, const std::span<const gpu::buffer_image_copy_region> regions) const -> void {
    std::vector<vk::BufferImageCopy> vk_regions;
    vk_regions.reserve(regions.size());
    for (const auto& r : regions) {
        vk_regions.push_back(to_vk(r));
    }
    raw().copyBufferToImage(std::bit_cast<vk::Buffer>(src), std::bit_cast<vk::Image>(dst), to_vk(dst_layout), vk_regions);
}

auto gse::vulkan::commands::copy_image_to_buffer(const gpu::handle<image> src, const gpu::image_layout src_layout, const gpu::handle<buffer> dst, const std::span<const gpu::buffer_image_copy_region> regions) const -> void {
    std::vector<vk::BufferImageCopy> vk_regions;
    vk_regions.reserve(regions.size());
    for (const auto& r : regions) {
        vk_regions.push_back(to_vk(r));
    }
    raw().copyImageToBuffer(std::bit_cast<vk::Image>(src), to_vk(src_layout), std::bit_cast<vk::Buffer>(dst), vk_regions);
}

auto gse::vulkan::commands::blit_image(const gpu::handle<image> src, const gpu::image_layout src_layout, const gpu::handle<image> dst, const gpu::image_layout dst_layout, const gpu::image_blit_region& region, const gpu::sampler_filter filter) const -> void {
    raw().blitImage(
        std::bit_cast<vk::Image>(src),
        to_vk(src_layout),
        std::bit_cast<vk::Image>(dst),
        to_vk(dst_layout),
        to_vk(region),
        to_vk(filter)
    );
}

auto gse::vulkan::commands::copy_image(const gpu::handle<image> src, const gpu::image_layout src_layout, const gpu::handle<image> dst, const gpu::image_layout dst_layout, const gpu::image_copy_region& region) const -> void {
    raw().copyImage(
        std::bit_cast<vk::Image>(src),
        to_vk(src_layout),
        std::bit_cast<vk::Image>(dst),
        to_vk(dst_layout),
        to_vk(region)
    );
}

auto gse::vulkan::commands::draw(const std::uint32_t vertex_count, const std::uint32_t instance_count, const std::uint32_t first_vertex, const std::uint32_t first_instance) const -> void {
    raw().draw(vertex_count, instance_count, first_vertex, first_instance);
}

auto gse::vulkan::commands::draw_indexed(const std::uint32_t index_count, const std::uint32_t instance_count, const std::uint32_t first_index, const std::int32_t vertex_offset, const std::uint32_t first_instance) const -> void {
    raw().drawIndexed(index_count, instance_count, first_index, vertex_offset, first_instance);
}

auto gse::vulkan::commands::write_timestamp(const gpu::pipeline_stage_flags stage, const gpu::handle<query_pool> query_pool, const std::uint32_t query) const -> void {
    raw().writeTimestamp2(to_vk(stage), std::bit_cast<vk::QueryPool>(query_pool), query);
}

auto gse::vulkan::commands::reset_query_pool(const gpu::handle<query_pool> query_pool, const std::uint32_t first_query, const std::uint32_t query_count) const -> void {
    raw().resetQueryPool(std::bit_cast<vk::QueryPool>(query_pool), first_query, query_count);
}

auto gse::vulkan::commands::draw_mesh_tasks(const std::uint32_t x, const std::uint32_t y, const std::uint32_t z) const -> void {
    raw().drawMeshTasksEXT(x, y, z);
}

auto gse::vulkan::commands::build_acceleration_structures(const gpu::acceleration_structure_build_geometry_info& build_info, const std::span<const gpu::acceleration_structure_build_range_info* const> range_infos) const -> void {
    std::vector<vk::AccelerationStructureGeometryKHR> vk_geometries;
    vk_geometries.reserve(build_info.geometries.size());
    for (const auto& g : build_info.geometries) {
        vk::AccelerationStructureGeometryDataKHR data{};
        vk::GeometryTypeKHR vk_geometry_type = vk::GeometryTypeKHR::eInstances;
        if (g.type == gpu::acceleration_structure_geometry_type::triangles) {
            vk_geometry_type = vk::GeometryTypeKHR::eTriangles;
            data.triangles = vk::AccelerationStructureGeometryTrianglesDataKHR{
                .vertexFormat = to_vk(g.triangles.vertex_format),
                .vertexData = vk::DeviceOrHostAddressConstKHR{ .deviceAddress = g.triangles.vertex_data },
                .vertexStride = g.triangles.vertex_stride,
                .maxVertex = g.triangles.max_vertex,
                .indexType = to_vk(g.triangles.index_type),
                .indexData = vk::DeviceOrHostAddressConstKHR{ .deviceAddress = g.triangles.index_data },
            };
        } else {
            data.instances = vk::AccelerationStructureGeometryInstancesDataKHR{
                .arrayOfPointers = g.instances.array_of_pointers ? vk::True : vk::False,
                .data = vk::DeviceOrHostAddressConstKHR{ .deviceAddress = g.instances.data },
            };
        }
        vk_geometries.push_back(vk::AccelerationStructureGeometryKHR{
            .geometryType = vk_geometry_type,
            .geometry = data,
            .flags = to_vk(g.flags),
        });
    }

    const vk::AccelerationStructureBuildGeometryInfoKHR vk_build_info{
        .type = to_vk(build_info.type),
        .flags = to_vk(build_info.flags),
        .mode = to_vk(build_info.mode),
        .dstAccelerationStructure = std::bit_cast<vk::AccelerationStructureKHR>(build_info.dst.value),
        .geometryCount = static_cast<std::uint32_t>(vk_geometries.size()),
        .pGeometries = vk_geometries.data(),
        .scratchData = vk::DeviceOrHostAddressKHR{ .deviceAddress = build_info.scratch_address },
    };

    std::vector<vk::AccelerationStructureBuildRangeInfoKHR> vk_ranges;
    vk_ranges.reserve(range_infos.size());
    for (const auto* r : range_infos) {
        vk_ranges.push_back(to_vk(*r));
    }
    std::vector<const vk::AccelerationStructureBuildRangeInfoKHR*> vk_range_ptrs;
    vk_range_ptrs.reserve(vk_ranges.size());
    for (const auto& r : vk_ranges) {
        vk_range_ptrs.push_back(&r);
    }

    raw().buildAccelerationStructuresKHR(vk_build_info, vk_range_ptrs);
}

auto gse::vulkan::commands::build_acceleration_structures(const vk::AccelerationStructureBuildGeometryInfoKHR& build_info, const std::span<const vk::AccelerationStructureBuildRangeInfoKHR* const> range_infos) const -> void {
    raw().buildAccelerationStructuresKHR(build_info, range_infos);
}

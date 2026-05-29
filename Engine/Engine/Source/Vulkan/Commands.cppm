export module gse.vulkan:commands;

import std;
import vulkan;

import :handles;
import :types;
import :allocation;
import :buffer;
import :image;

import gse.math;

export namespace gse::vulkan {
	struct command_buffer;

	struct pipeline_state_cache {
		std::optional<gpu::topology> topology;
		std::optional<gpu::polygon_mode> polygon_mode;
		std::optional<gpu::cull_mode> cull_mode;
		std::optional<gpu::front_face> front_face;
		std::optional<bool> depth_test_enable;
		std::optional<bool> depth_write_enable;
		std::optional<gpu::compare_op> depth_compare_op;
		std::optional<bool> depth_bias_enable;
		std::optional<bool> rasterizer_discard_enable;
		std::optional<bool> primitive_restart_enable;
		std::optional<bool> alpha_to_coverage_enable;
		std::optional<bool> alpha_to_one_enable;
		std::optional<bool> logic_op_enable;
		std::optional<bool> depth_clamp_enable;

		auto invalidate() -> void;
	};
}

export namespace gse::gpu {
	struct buffer_barrier {
		pipeline_stage_flags src_stages;
		access_flags src_access;
		pipeline_stage_flags dst_stages;
		access_flags dst_access;
		gpu::buffer_handle buffer;
		device_size offset = 0;
		device_size size = 0;
	};

	struct image_barrier {
		pipeline_stage_flags src_stages;
		access_flags src_access;
		pipeline_stage_flags dst_stages;
		access_flags dst_access;
		bool discard_contents = false;
		gpu::image_handle image;
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
		gpu::image_view_handle image_view;
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
		bool secondary_command_buffers = false;
	};
}

export namespace gse::vulkan {
	class commands {
	public:
		commands() = default;

		commands(
			gpu::command_buffer_handle cmd
		);

		[[nodiscard]] auto native() const -> gpu::command_buffer_handle;

		[[nodiscard]] auto valid() const -> bool;

		auto begin() const -> void;

		auto begin_secondary(
			const gpu::secondary_inheritance_info& info
		) const -> void;

		auto end() const -> void;

		auto reset() const -> void;

		auto execute_commands(
			gpu::command_buffer_handle secondary
		) const -> void;

		auto begin_rendering(
			const gpu::rendering_info& info
		) const -> void;

		auto end_rendering() const -> void;

		auto pipeline_barrier(
			const gpu::dependency_info& dep
		) const -> void;

		auto reset_query_pool(
			gpu::query_pool_handle pool,
			std::uint32_t first_query,
			std::uint32_t query_count
		) const -> void;

		auto write_timestamp(
			gpu::pipeline_stage_flags stage,
			gpu::query_pool_handle pool,
			std::uint32_t query_index
		) const -> void;

		auto begin_query(
			gpu::query_pool_handle pool,
			std::uint32_t query_index
		) const -> void;

		auto end_query(
			gpu::query_pool_handle pool,
			std::uint32_t query_index
		) const -> void;

		auto bind_shaders(
			std::span<const gpu::stage_flag> stages,
			std::span<const gpu::shader_object_handle> shaders
		) const -> void;

		auto unbind_shaders(
			std::span<const gpu::stage_flag> stages
		) const -> void;

		auto bind_resource_heap(
			gpu::device_address heap_address,
			gpu::device_size heap_size,
			gpu::device_size reserved_offset,
			gpu::device_size reserved_size
		) const -> void;

		auto bind_sampler_heap(
			gpu::device_address heap_address,
			gpu::device_size heap_size,
			gpu::device_size reserved_offset,
			gpu::device_size reserved_size
		) const -> void;

		auto push_data(
			std::uint32_t offset,
			std::span<const std::byte> data
		) const -> void;

		auto push_constants(
			gpu::pipeline_layout_handle layout,
			gpu::stage_flags stages,
			std::uint32_t offset,
			std::uint32_t size,
			const void* data
		) const -> void;

		auto bind_index_buffer_2(
			gpu::buffer_handle buffer,
			gpu::device_size offset,
			gpu::device_size size,
			gpu::index_type type
		) const -> void;

		auto draw_indexed_indirect(
			gpu::buffer_handle buffer,
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
			gpu::buffer_handle buffer,
			gpu::device_size offset
		) const -> void;

		auto set_viewport(
			const gpu::viewport& viewport
		) const -> void;

		auto set_scissor(
			const gse::rect_t<vec2i>& scissor
		) const -> void;

		auto set_topology(
			gpu::topology t
		) const -> void;

		auto set_polygon_mode(
			gpu::polygon_mode m
		) const -> void;

		auto set_cull_mode(
			gpu::cull_mode m
		) const -> void;

		auto set_front_face(
			gpu::front_face f
		) const -> void;

		auto set_depth_test_enable(
			bool enable
		) const -> void;

		auto set_depth_write_enable(
			bool enable
		) const -> void;

		auto set_depth_compare_op(
			gpu::compare_op op
		) const -> void;

		auto set_depth_bias_enable(
			bool enable
		) const -> void;

		auto set_depth_bias(
			float constant,
			float clamp,
			float slope
		) const -> void;

		auto set_depth_clamp_enable(
			bool enable
		) const -> void;

		auto set_depth_bounds_test_enable(
			bool enable
		) const -> void;

		auto set_stencil_test_enable(
			bool enable
		) const -> void;

		auto set_line_width(
			float width
		) const -> void;

		auto set_rasterizer_discard_enable(
			bool enable
		) const -> void;

		auto set_primitive_restart_enable(
			bool enable
		) const -> void;

		auto set_rasterization_samples(
			gpu::sample_count samples
		) const -> void;

		auto set_sample_mask(
			gpu::sample_count samples,
			std::uint32_t mask
		) const -> void;

		auto set_alpha_to_coverage_enable(
			bool enable
		) const -> void;

		auto set_alpha_to_one_enable(
			bool enable
		) const -> void;

		auto set_logic_op_enable(
			bool enable
		) const -> void;

		auto set_color_blend_enable(
			std::uint32_t first_attachment,
			std::span<const std::uint8_t> enables
		) const -> void;

		auto set_color_blend_equation(
			std::uint32_t first_attachment,
			std::span<const gpu::color_blend_equation> equations
		) const -> void;

		auto set_color_write_mask(
			std::uint32_t first_attachment,
			std::span<const gpu::color_component_flags> masks
		) const -> void;

		auto set_blend_constants(
			std::array<float, 4> constants
		) const -> void;

		auto copy_buffer(
			gpu::buffer_handle src,
			gpu::buffer_handle dst,
			const gpu::buffer_copy_region& region
		) const -> void;

		auto fill_buffer(
			gpu::buffer_handle dst,
			gpu::device_size offset,
			gpu::device_size size,
			std::uint32_t data
		) const -> void;

		auto copy_buffer_to_image(
			gpu::buffer_handle src,
			gpu::image_handle dst,
			std::span<const gpu::buffer_image_copy_region> regions
		) const -> void;

		auto copy_image_to_buffer(
			gpu::image_handle src,
			gpu::buffer_handle dst,
			std::span<const gpu::buffer_image_copy_region> regions
		) const -> void;

		auto blit_image(
			gpu::image_handle src,
			gpu::image_handle dst,
			const gpu::image_blit_region& region,
			gpu::sampler_filter filter
		) const -> void;

		auto copy_image(
			gpu::image_handle src,
			gpu::image_handle dst,
			const gpu::image_copy_region& region
		) const -> void;

		auto release_swapchain_image_to_present(
			gpu::image_handle img,
			gpu::pipeline_stage_flags src_stages,
			gpu::access_flags src_access
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

		auto draw_mesh_tasks(
			std::uint32_t x,
			std::uint32_t y,
			std::uint32_t z
		) const -> void;

		auto draw_mesh_tasks_indirect(
			gpu::buffer_handle buffer,
			gpu::device_size offset,
			std::uint32_t draw_count,
			std::uint32_t stride
		) const -> void;

		auto build_acceleration_structures(
			const gpu::acceleration_structure_build_geometry_info& build_info,
			std::span<const gpu::acceleration_structure_build_range_info* const> range_infos
		) const -> void;

	private:
		[[nodiscard]] auto raw() const -> vk::CommandBuffer;

		gpu::command_buffer_handle m_cmd;
	};
}

gse::vulkan::commands::commands(const gpu::command_buffer_handle cmd) : m_cmd(cmd) {
}

auto gse::vulkan::commands::native() const -> gpu::command_buffer_handle {
	return m_cmd;
}

auto gse::vulkan::commands::valid() const -> bool {
	return static_cast<bool>(m_cmd);
}

auto gse::vulkan::commands::raw() const -> vk::CommandBuffer {
	return std::bit_cast<vk::CommandBuffer>(m_cmd);
}

auto gse::vulkan::commands::begin() const -> void {
	raw().begin(vk::CommandBufferBeginInfo{});
}

auto gse::vulkan::commands::begin_secondary(const gpu::secondary_inheritance_info& info) const -> void {
	std::vector<vk::Format> color_formats;
	color_formats.reserve(info.color_attachment_formats.size());
	for (const auto f : info.color_attachment_formats) {
		color_formats.push_back(static_cast<vk::Format>(f));
	}

	const vk::CommandBufferInheritanceRenderingInfo rendering_inherit{
		.viewMask = 0,
		.colorAttachmentCount = static_cast<std::uint32_t>(color_formats.size()),
		.pColorAttachmentFormats = color_formats.empty() ? nullptr : color_formats.data(),
		.depthAttachmentFormat = static_cast<vk::Format>(info.depth_attachment_format),
		.stencilAttachmentFormat = vk::Format::eUndefined,
		.rasterizationSamples = vk::SampleCountFlagBits::e1,
	};

	vk::CommandBufferInheritanceInfo inherit{};
	if (info.render_pass_continue) {
		inherit.pNext = &rendering_inherit;
	}
	if (info.pipeline_statistics.bits() != 0) {
		inherit.pipelineStatistics = to_vk(info.pipeline_statistics);
	}

	vk::CommandBufferUsageFlags flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
	if (info.render_pass_continue) {
		flags |= vk::CommandBufferUsageFlagBits::eRenderPassContinue;
	}

	const vk::CommandBufferBeginInfo begin_info{
		.flags = flags,
		.pInheritanceInfo = &inherit,
	};
	raw().begin(begin_info);
}

auto gse::vulkan::commands::end() const -> void {
	raw().end();
}

auto gse::vulkan::commands::reset() const -> void {
	raw().reset();
}

auto gse::vulkan::commands::execute_commands(const gpu::command_buffer_handle secondary) const -> void {
	const vk::CommandBuffer cb = std::bit_cast<vk::CommandBuffer>(secondary);
	raw().executeCommands(cb);
}

auto gse::vulkan::commands::reset_query_pool(const gpu::query_pool_handle pool, const std::uint32_t first_query, const std::uint32_t query_count) const -> void {
	raw().resetQueryPool(std::bit_cast<vk::QueryPool>(pool), first_query, query_count);
}

auto gse::vulkan::commands::write_timestamp(const gpu::pipeline_stage_flags stage, const gpu::query_pool_handle pool, const std::uint32_t query_index) const -> void {
	raw().writeTimestamp2(to_vk(stage), std::bit_cast<vk::QueryPool>(pool), query_index);
}

auto gse::vulkan::commands::begin_query(const gpu::query_pool_handle pool, const std::uint32_t query_index) const -> void {
	raw().beginQuery(
		std::bit_cast<vk::QueryPool>(pool),
		query_index,
		{}
	);
}

auto gse::vulkan::commands::end_query(const gpu::query_pool_handle pool, const std::uint32_t query_index) const -> void {
	raw().endQuery(std::bit_cast<vk::QueryPool>(pool), query_index);
}

namespace gse::vulkan {
	struct rendering_scratch {
		std::vector<vk::RenderingAttachmentInfo> color;
		std::optional<vk::RenderingAttachmentInfo> depth;
		std::optional<vk::RenderingAttachmentInfo> stencil;
	};

	struct dependency_scratch {
		std::vector<vk::MemoryBarrier2> memory;
		std::vector<vk::BufferMemoryBarrier2> buffer;
		std::vector<vk::ImageMemoryBarrier2> image;
	};

	auto build_vk_attachment(
		const gpu::rendering_attachment_info& att,
		bool is_depth
	) -> vk::RenderingAttachmentInfo;

	auto build_vk_rendering_info(
		const gpu::rendering_info& info,
		rendering_scratch& scratch
	) -> vk::RenderingInfo;

	auto build_vk_dependency_info(
		const gpu::dependency_info& dep,
		dependency_scratch& scratch
	) -> vk::DependencyInfo;
}

auto gse::vulkan::commands::begin_rendering(const gpu::rendering_info& info) const -> void {
	rendering_scratch scratch;
	const auto vk_info = build_vk_rendering_info(info, scratch);
	raw().beginRendering(vk_info);
}

auto gse::vulkan::commands::end_rendering() const -> void {
	raw().endRendering();
}

auto gse::vulkan::commands::pipeline_barrier(const gpu::dependency_info& dep) const -> void {
	dependency_scratch scratch;
	const auto vk_dep = build_vk_dependency_info(dep, scratch);
	raw().pipelineBarrier2(vk_dep);
}

auto gse::vulkan::commands::bind_shaders(const std::span<const gpu::stage_flag> stages, const std::span<const gpu::shader_object_handle> shaders) const -> void {
	std::vector<vk::ShaderStageFlagBits> vk_stages;
	vk_stages.reserve(stages.size());
	for (const auto s : stages) {
		vk_stages.push_back(to_vk(s));
	}

	std::vector<vk::ShaderEXT> vk_shaders;
	vk_shaders.reserve(shaders.size());
	for (const auto h : shaders) {
		vk_shaders.push_back(std::bit_cast<vk::ShaderEXT>(h));
	}

	raw().bindShadersEXT(vk_stages, vk_shaders);
}

auto gse::vulkan::commands::bind_resource_heap(const gpu::device_address heap_address, const gpu::device_size heap_size, const gpu::device_size reserved_offset, const gpu::device_size reserved_size) const -> void {
	const vk::BindHeapInfoEXT info{
		.heapRange = {
			.address = heap_address,
			.size = heap_size
		},
		.reservedRangeOffset = reserved_offset,
		.reservedRangeSize = reserved_size,
	};
	raw().bindResourceHeapEXT(info);
}

auto gse::vulkan::commands::bind_sampler_heap(const gpu::device_address heap_address, const gpu::device_size heap_size, const gpu::device_size reserved_offset, const gpu::device_size reserved_size) const -> void {
	const vk::BindHeapInfoEXT info{
		.heapRange = {
			.address = heap_address,
			.size = heap_size
		},
		.reservedRangeOffset = reserved_offset,
		.reservedRangeSize = reserved_size,
	};
	raw().bindSamplerHeapEXT(info);
}

auto gse::vulkan::commands::push_data(const std::uint32_t offset, const std::span<const std::byte> data) const -> void {
	const vk::PushDataInfoEXT info{
		.offset = offset,
		.data = {
			.address = data.data(),
			.size = data.size(),
		},
	};
	raw().pushDataEXT(info);
}

auto gse::vulkan::commands::unbind_shaders(const std::span<const gpu::stage_flag> stages) const -> void {
	std::vector<vk::ShaderStageFlagBits> vk_stages;
	vk_stages.reserve(stages.size());
	for (const auto s : stages) {
		vk_stages.push_back(to_vk(s));
	}

	std::vector<vk::ShaderEXT> vk_shaders(stages.size(), nullptr);
	raw().bindShadersEXT(vk_stages, vk_shaders);
}

auto gse::vulkan::commands::push_constants(const gpu::pipeline_layout_handle layout, const gpu::stage_flags stages, const std::uint32_t offset, const std::uint32_t size, const void* data) const -> void {
	raw().pushConstants(std::bit_cast<vk::PipelineLayout>(layout), to_vk(stages), offset, size, data);
}

auto gse::vulkan::commands::bind_index_buffer_2(const gpu::buffer_handle buffer, const gpu::device_size offset, const gpu::device_size size, const gpu::index_type type) const -> void {
	raw().bindIndexBuffer2KHR(std::bit_cast<vk::Buffer>(buffer), offset, size, to_vk(type));
}

auto gse::vulkan::commands::draw_indexed_indirect(const gpu::buffer_handle buffer, const gpu::device_size offset, const std::uint32_t draw_count, const std::uint32_t stride) const -> void {
	raw().drawIndexedIndirect(std::bit_cast<vk::Buffer>(buffer), offset, draw_count, stride);
}

auto gse::vulkan::commands::dispatch(const std::uint32_t x, const std::uint32_t y, const std::uint32_t z) const -> void {
	raw().dispatch(x, y, z);
}

auto gse::vulkan::commands::dispatch_indirect(const gpu::buffer_handle buffer, const gpu::device_size offset) const -> void {
	raw().dispatchIndirect(std::bit_cast<vk::Buffer>(buffer), offset);
}

auto gse::vulkan::commands::set_viewport(const gpu::viewport& viewport) const -> void {
	raw().setViewportWithCount(to_vk(viewport));
}

auto gse::vulkan::commands::set_scissor(const gse::rect_t<vec2i>& scissor) const -> void {
	const auto min = scissor.min();
	const auto size = scissor.size();
	const vk::Rect2D rect{
		.offset = { min.x(), min.y() },
		.extent = { static_cast<std::uint32_t>(size.x()), static_cast<std::uint32_t>(size.y()) },
	};
	raw().setScissorWithCount(rect);
}

auto gse::vulkan::commands::copy_buffer(const gpu::buffer_handle src, const gpu::buffer_handle dst, const gpu::buffer_copy_region& region) const -> void {
	raw().copyBuffer(std::bit_cast<vk::Buffer>(src), std::bit_cast<vk::Buffer>(dst), to_vk(region));
}

auto gse::vulkan::commands::fill_buffer(const gpu::buffer_handle dst, const gpu::device_size offset, const gpu::device_size size, const std::uint32_t data) const -> void {
	raw().fillBuffer(std::bit_cast<vk::Buffer>(dst), offset, size, data);
}

auto gse::vulkan::commands::copy_buffer_to_image(const gpu::buffer_handle src, const gpu::image_handle dst, const std::span<const gpu::buffer_image_copy_region> regions) const -> void {
	std::vector<vk::BufferImageCopy> vk_regions;
	vk_regions.reserve(regions.size());
	for (const auto& r : regions) {
		vk_regions.push_back(to_vk(r));
	}
	raw().copyBufferToImage(
		std::bit_cast<vk::Buffer>(src),
		std::bit_cast<vk::Image>(dst),
		vk::ImageLayout::eGeneral,
		vk_regions
	);
}

auto gse::vulkan::commands::copy_image_to_buffer(const gpu::image_handle src, const gpu::buffer_handle dst, const std::span<const gpu::buffer_image_copy_region> regions) const -> void {
	std::vector<vk::BufferImageCopy> vk_regions;
	vk_regions.reserve(regions.size());
	for (const auto& r : regions) {
		vk_regions.push_back(to_vk(r));
	}
	raw().copyImageToBuffer(
		std::bit_cast<vk::Image>(src),
		vk::ImageLayout::eGeneral,
		std::bit_cast<vk::Buffer>(dst),
		vk_regions
	);
}

auto gse::vulkan::commands::blit_image(const gpu::image_handle src, const gpu::image_handle dst, const gpu::image_blit_region& region, const gpu::sampler_filter filter) const -> void {
	raw().blitImage(
		std::bit_cast<vk::Image>(src),
		vk::ImageLayout::eGeneral,
		std::bit_cast<vk::Image>(dst),
		vk::ImageLayout::eGeneral,
		to_vk(region),
		to_vk(filter)
	);
}

auto gse::vulkan::commands::copy_image(const gpu::image_handle src, const gpu::image_handle dst, const gpu::image_copy_region& region) const -> void {
	raw().copyImage(
		std::bit_cast<vk::Image>(src),
		vk::ImageLayout::eGeneral,
		std::bit_cast<vk::Image>(dst),
		vk::ImageLayout::eGeneral,
		to_vk(region)
	);
}

auto gse::vulkan::commands::release_swapchain_image_to_present(const gpu::image_handle img, const gpu::pipeline_stage_flags src_stages, const gpu::access_flags src_access) const -> void {
	const vk::ImageMemoryBarrier2 barrier{
		.srcStageMask = to_vk(src_stages),
		.srcAccessMask = to_vk(src_access),
		.dstStageMask = vk::PipelineStageFlagBits2::eBottomOfPipe,
		.dstAccessMask = {},
		.oldLayout = vk::ImageLayout::eGeneral,
		.newLayout = vk::ImageLayout::ePresentSrcKHR,
		.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
		.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
		.image = std::bit_cast<vk::Image>(img),
		.subresourceRange = {
			.aspectMask = vk::ImageAspectFlagBits::eColor,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};
	const vk::DependencyInfo dep{
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &barrier,
	};
	raw().pipelineBarrier2(dep);
}

auto gse::vulkan::commands::draw(const std::uint32_t vertex_count, const std::uint32_t instance_count, const std::uint32_t first_vertex, const std::uint32_t first_instance) const -> void {
	raw().draw(vertex_count, instance_count, first_vertex, first_instance);
}

auto gse::vulkan::commands::draw_indexed(const std::uint32_t index_count, const std::uint32_t instance_count, const std::uint32_t first_index, const std::int32_t vertex_offset, const std::uint32_t first_instance) const -> void {
	raw().drawIndexed(index_count, instance_count, first_index, vertex_offset, first_instance);
}

auto gse::vulkan::commands::draw_mesh_tasks(const std::uint32_t x, const std::uint32_t y, const std::uint32_t z) const -> void {
	raw().drawMeshTasksEXT(x, y, z);
}

auto gse::vulkan::commands::draw_mesh_tasks_indirect(const gpu::buffer_handle buffer, const gpu::device_size offset, const std::uint32_t draw_count, const std::uint32_t stride) const -> void {
	raw().drawMeshTasksIndirectEXT(std::bit_cast<vk::Buffer>(buffer), offset, draw_count, stride);
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
				.vertexData =
					vk::DeviceOrHostAddressConstKHR{
						.deviceAddress = g.triangles.vertex_data
					},
				.vertexStride = g.triangles.vertex_stride,
				.maxVertex = g.triangles.max_vertex,
				.indexType = to_vk(g.triangles.index_type),
				.indexData =
					vk::DeviceOrHostAddressConstKHR{
						.deviceAddress = g.triangles.index_data
					},
			};
		}
		else {
			data.instances = vk::AccelerationStructureGeometryInstancesDataKHR{
				.arrayOfPointers = g.instances.array_of_pointers ? vk::True : vk::False,
				.data =
					vk::DeviceOrHostAddressConstKHR{
						.deviceAddress = g.instances.data
					},
			};
		}
		vk_geometries.push_back(
			vk::AccelerationStructureGeometryKHR{
				.geometryType = vk_geometry_type,
				.geometry = data,
				.flags = to_vk(g.flags),
			}
		);
	}

	const vk::AccelerationStructureBuildGeometryInfoKHR vk_build_info{
		.type = to_vk(build_info.type),
		.flags = to_vk(build_info.flags),
		.mode = to_vk(build_info.mode),
		.dstAccelerationStructure = std::bit_cast<vk::AccelerationStructureKHR>(build_info.dst.value),
		.geometryCount = static_cast<std::uint32_t>(vk_geometries.size()),
		.pGeometries = vk_geometries.data(),
		.scratchData =
			vk::DeviceOrHostAddressKHR{
				.deviceAddress = build_info.scratch_address
			},
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

auto gse::vulkan::build_vk_attachment(const gpu::rendering_attachment_info& att, const bool is_depth) -> vk::RenderingAttachmentInfo {
	vk::ClearValue clear{};
	if (is_depth) {
		clear.depthStencil = vk::ClearDepthStencilValue{
			.depth = att.depth_clear_value.depth,
			.stencil = 0
		};
	}
	else {
		clear.color = vk::ClearColorValue{ std::array{
			att.color_clear_value.r,
			att.color_clear_value.g,
			att.color_clear_value.b,
			att.color_clear_value.a,
		} };
	}
	return vk::RenderingAttachmentInfo{
		.imageView = std::bit_cast<vk::ImageView>(att.image_view),
		.imageLayout = vk::ImageLayout::eGeneral,
		.loadOp = to_vk(att.load),
		.storeOp = to_vk(att.store),
		.clearValue = clear,
	};
}

auto gse::vulkan::build_vk_rendering_info(const gpu::rendering_info& info, rendering_scratch& scratch) -> vk::RenderingInfo {
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
		.flags = info.secondary_command_buffers
			? vk::RenderingFlags{ vk::RenderingFlagBits::eContentsSecondaryCommandBuffers }
			: vk::RenderingFlags{},
		.renderArea =
			vk::Rect2D{
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

auto gse::vulkan::build_vk_dependency_info(const gpu::dependency_info& dep, dependency_scratch& scratch) -> vk::DependencyInfo {
	scratch.memory.reserve(dep.memory_barriers.size());
	for (const auto& b : dep.memory_barriers) {
		scratch.memory.push_back(
			vk::MemoryBarrier2{
				.srcStageMask = to_vk(b.src_stages),
				.srcAccessMask = to_vk(b.src_access),
				.dstStageMask = to_vk(b.dst_stages),
				.dstAccessMask = to_vk(b.dst_access),
			}
		);
	}
	scratch.buffer.reserve(dep.buffer_barriers.size());
	for (const auto& b : dep.buffer_barriers) {
		scratch.buffer.push_back(
			vk::BufferMemoryBarrier2{
				.srcStageMask = to_vk(b.src_stages),
				.srcAccessMask = to_vk(b.src_access),
				.dstStageMask = to_vk(b.dst_stages),
				.dstAccessMask = to_vk(b.dst_access),
				.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
				.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
				.buffer = std::bit_cast<vk::Buffer>(b.buffer),
				.offset = b.offset,
				.size = b.size == 0 ? vk::WholeSize : b.size,
			}
		);
	}
	scratch.image.reserve(dep.image_barriers.size());
	for (const auto& b : dep.image_barriers) {
		scratch.image.push_back(
			vk::ImageMemoryBarrier2{
				.srcStageMask = to_vk(b.src_stages),
				.srcAccessMask = to_vk(b.src_access),
				.dstStageMask = to_vk(b.dst_stages),
				.dstAccessMask = to_vk(b.dst_access),
				.oldLayout = b.discard_contents ? vk::ImageLayout::eUndefined : vk::ImageLayout::eGeneral,
				.newLayout = vk::ImageLayout::eGeneral,
				.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
				.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
				.image = std::bit_cast<vk::Image>(b.image),
				.subresourceRange =
					vk::ImageSubresourceRange{
						.aspectMask = to_vk(b.aspects),
						.baseMipLevel = b.base_mip_level,
						.levelCount = b.level_count,
						.baseArrayLayer = b.base_array_layer,
						.layerCount = b.layer_count,
					},
			}
		);
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

auto gse::vulkan::pipeline_state_cache::invalidate() -> void {
	*this = {};
}

auto gse::vulkan::commands::set_topology(const gpu::topology t) const -> void {
	raw().setPrimitiveTopology(to_vk(t));
}

auto gse::vulkan::commands::set_polygon_mode(const gpu::polygon_mode m) const -> void {
	raw().setPolygonModeEXT(to_vk(m));
}

auto gse::vulkan::commands::set_cull_mode(const gpu::cull_mode m) const -> void {
	raw().setCullMode(to_vk(m));
}

auto gse::vulkan::commands::set_front_face(const gpu::front_face f) const -> void {
	raw().setFrontFace(to_vk(f));
}

auto gse::vulkan::commands::set_depth_test_enable(const bool enable) const -> void {
	raw().setDepthTestEnable(enable ? vk::True : vk::False);
}

auto gse::vulkan::commands::set_depth_write_enable(const bool enable) const -> void {
	raw().setDepthWriteEnable(enable ? vk::True : vk::False);
}

auto gse::vulkan::commands::set_depth_compare_op(const gpu::compare_op op) const -> void {
	raw().setDepthCompareOp(to_vk(op));
}

auto gse::vulkan::commands::set_depth_bias_enable(const bool enable) const -> void {
	raw().setDepthBiasEnable(enable ? vk::True : vk::False);
}

auto gse::vulkan::commands::set_depth_bias(const float constant, const float clamp, const float slope) const -> void {
	raw().setDepthBias(constant, clamp, slope);
}

auto gse::vulkan::commands::set_depth_clamp_enable(const bool enable) const -> void {
	raw().setDepthClampEnableEXT(enable ? vk::True : vk::False);
}

auto gse::vulkan::commands::set_depth_bounds_test_enable(const bool enable) const -> void {
	raw().setDepthBoundsTestEnable(enable ? vk::True : vk::False);
}

auto gse::vulkan::commands::set_stencil_test_enable(const bool enable) const -> void {
	raw().setStencilTestEnable(enable ? vk::True : vk::False);
}

auto gse::vulkan::commands::set_line_width(const float width) const -> void {
	raw().setLineWidth(width);
}

auto gse::vulkan::commands::set_rasterizer_discard_enable(const bool enable) const -> void {
	raw().setRasterizerDiscardEnable(enable ? vk::True : vk::False);
}

auto gse::vulkan::commands::set_primitive_restart_enable(const bool enable) const -> void {
	raw().setPrimitiveRestartEnable(enable ? vk::True : vk::False);
}

auto gse::vulkan::commands::set_rasterization_samples(const gpu::sample_count samples) const -> void {
	raw().setRasterizationSamplesEXT(to_vk(samples));
}

auto gse::vulkan::commands::set_sample_mask(const gpu::sample_count samples, const std::uint32_t mask) const -> void {
	const vk::SampleMask sample_mask = mask;
	raw().setSampleMaskEXT(to_vk(samples), sample_mask);
}

auto gse::vulkan::commands::set_alpha_to_coverage_enable(const bool enable) const -> void {
	raw().setAlphaToCoverageEnableEXT(enable ? vk::True : vk::False);
}

auto gse::vulkan::commands::set_alpha_to_one_enable(const bool enable) const -> void {
	raw().setAlphaToOneEnableEXT(enable ? vk::True : vk::False);
}

auto gse::vulkan::commands::set_logic_op_enable(const bool enable) const -> void {
	raw().setLogicOpEnableEXT(enable ? vk::True : vk::False);
}

auto gse::vulkan::commands::set_color_blend_enable(const std::uint32_t first_attachment, const std::span<const std::uint8_t> enables) const -> void {
	std::vector<vk::Bool32> vk_enables;
	vk_enables.reserve(enables.size());
	for (const auto e : enables) {
		vk_enables.push_back(e ? vk::True : vk::False);
	}
	raw().setColorBlendEnableEXT(first_attachment, vk_enables);
}

auto gse::vulkan::commands::set_color_blend_equation(const std::uint32_t first_attachment, const std::span<const gpu::color_blend_equation> equations) const -> void {
	std::vector<vk::ColorBlendEquationEXT> vk_equations;
	vk_equations.reserve(equations.size());
	for (const auto& eq : equations) {
		vk_equations.push_back(to_vk(eq));
	}
	raw().setColorBlendEquationEXT(first_attachment, vk_equations);
}

auto gse::vulkan::commands::set_color_write_mask(const std::uint32_t first_attachment, const std::span<const gpu::color_component_flags> masks) const -> void {
	std::vector<vk::ColorComponentFlags> vk_masks;
	vk_masks.reserve(masks.size());
	for (const auto m : masks) {
		vk_masks.push_back(to_vk(m));
	}
	raw().setColorWriteMaskEXT(first_attachment, vk_masks);
}

auto gse::vulkan::commands::set_blend_constants(const std::array<float, 4> constants) const -> void {
	raw().setBlendConstants(constants.data());
}

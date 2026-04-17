export module gse.platform:render_graph;

import std;

import :gpu_types;
import :gpu_buffer;
import :gpu_pipeline;
import :gpu_image;
import :gpu_descriptor;
import :gpu_device;
import :gpu_swapchain;
import :gpu_frame;
import :descriptor_heap;
import :gpu_push_constants;

import gse.assert;
import gse.utility;
import gse.math;

export namespace gse::vulkan {
	class render_graph;
	class pass_builder;
	class recording_context;

	using color_clear = gpu::color_clear;
	using depth_clear = gpu::depth_clear;

	enum class load_op {
		clear_color,
		clear_depth,
		load,
		dont_care
	};

	struct color_output_info {
		bool is_swapchain = false;
		const image_resource* custom_target = nullptr;
		load_op op = load_op::clear_color;
		color_clear clear_value;
	};

	struct depth_output_info {
		load_op op = load_op::clear_depth;
		depth_clear clear_value;
	};


	enum class resource_type : std::uint8_t {
		buffer,
		image
	};

	struct resource_ref {
		const void* ptr = nullptr;
		resource_type type = resource_type::buffer;
	};

	struct resource_usage {
		resource_ref resource;
		vk::PipelineStageFlags2 stage;
		vk::AccessFlags2 access;
	};

	auto sampled(
		const image_resource& img,
		vk::PipelineStageFlags2 stage
	) -> resource_usage;

	auto storage(
		const buffer_resource& buf,
		vk::PipelineStageFlags2 stage
	) -> resource_usage;

	auto storage_read(
		const buffer_resource& buf,
		vk::PipelineStageFlags2 stage
	) -> resource_usage;

	auto transfer_read(
		const buffer_resource& buf
	) -> resource_usage;

	auto transfer_write(
		const buffer_resource& buf
	) -> resource_usage;

	auto indirect_read(
		const buffer_resource& buf,
		vk::PipelineStageFlags2 stage
	) -> resource_usage;

	auto attachment(
		const image_resource& img,
		vk::PipelineStageFlags2 stage
	) -> resource_usage;
}

export namespace gse::gpu {
	using recording_context = vulkan::recording_context;

	auto storage_read(
		const buffer& buf,
		pipeline_stage stage
	) -> vulkan::resource_usage;

	auto storage_write(
		const buffer& buf,
		pipeline_stage stage
	) -> vulkan::resource_usage;

	auto transfer_read(
		const buffer& buf
	) -> vulkan::resource_usage;

	auto transfer_write(
		const buffer& buf
	) -> vulkan::resource_usage;

	auto sampled(
		const image& img,
		pipeline_stage stage
	) -> vulkan::resource_usage;

	auto indirect_read(
		const buffer& buf,
		pipeline_stage stage
	) -> vulkan::resource_usage;
}

export namespace gse::vulkan {
	class recording_context {
	public:
		auto bind_pipeline(
			vk::PipelineBindPoint point,
			vk::Pipeline pipeline
		) const -> void;

		auto bind_descriptors(
			vk::PipelineBindPoint point,
			vk::PipelineLayout layout,
			const descriptor_region& region,
			std::uint32_t set_index = 0
		) const -> void;

		auto push(
			const gpu::cached_push_constants& cache,
			vk::PipelineLayout layout
		) const -> void;

		auto set_viewport(
			float x,
			float y,
			float width,
			float height,
			float min_depth = 0.0f,
			float max_depth = 1.0f
		) const -> void;

		auto set_scissor(
			std::int32_t x,
			std::int32_t y,
			std::uint32_t width,
			std::uint32_t height
		) const -> void;

		auto draw(
			std::uint32_t vertex_count,
			std::uint32_t instance_count = 1,
			std::uint32_t first_vertex = 0,
			std::uint32_t first_instance = 0
		) const -> void;

		auto draw_indexed(
			std::uint32_t index_count,
			std::uint32_t instance_count = 1,
			std::uint32_t first_index = 0,
			std::int32_t vertex_offset = 0,
			std::uint32_t first_instance = 0
		) const -> void;

		auto draw_indirect(
			vk::Buffer buffer,
			vk::DeviceSize offset,
			std::uint32_t draw_count,
			std::uint32_t stride
		) const -> void;

		auto draw_mesh_tasks(
			std::uint32_t x,
			std::uint32_t y = 1,
			std::uint32_t z = 1
		) const -> void;

		auto dispatch(
			std::uint32_t x,
			std::uint32_t y = 1,
			std::uint32_t z = 1
		) const -> void;

		auto bind_vertex_buffers(
			std::uint32_t first,
			std::span<const vk::Buffer> buffers,
			std::span<const vk::DeviceSize> offsets
		) const -> void;

		auto bind_index_buffer(
			vk::Buffer buffer,
			vk::DeviceSize offset,
			vk::IndexType index_type
		) const -> void;

		auto begin_rendering(
			const vk::RenderingInfo& info
		) const -> void;

		auto end_rendering(
		) const -> void;

		auto bind_pipeline(
			vk::PipelineBindPoint point,
			const gpu::pipeline& p
		) const -> void;

		auto bind_descriptors(
			vk::PipelineBindPoint point,
			const gpu::pipeline& p,
			const descriptor_region& region,
			std::uint32_t set_index = 0
		) const -> void;

		auto push(
			const gpu::pipeline& p,
			const gpu::cached_push_constants& cache
		) const -> void;

		auto draw_indirect(
			const gpu::buffer& buf,
			std::size_t offset,
			std::uint32_t draw_count,
			std::uint32_t stride
		) const -> void;

		auto bind_vertex_buffer(
			const gpu::buffer& buf,
			vk::DeviceSize offset = 0
		) const -> void;

		auto bind_index_buffer(
			const gpu::buffer& buf,
			vk::DeviceSize offset = 0,
			vk::IndexType index_type = vk::IndexType::eUint32
		) const -> void;

		auto bind(
			const gpu::pipeline& p
		) const -> void;

		auto bind_descriptors(
			const gpu::pipeline& p,
			const descriptor_region& region,
			std::uint32_t set_index = 0
		) const -> void;

		auto bind_descriptors(
			const gpu::pipeline& p,
			const gpu::descriptor_region& region,
			std::uint32_t set_index = 0
		) const -> void;

		auto bind_vertex(
			const gpu::buffer& buf,
			std::size_t offset = 0
		) const -> void;

		auto bind_index(
			const gpu::buffer& buf,
			gpu::index_type type = gpu::index_type::uint32,
			std::size_t offset = 0
		) const -> void;

		auto set_viewport(
			vec2u extent
		) const -> void;

		auto set_scissor(
			vec2u extent
		) const -> void;

		auto commit(const descriptor_writer& writer,
			const gpu::pipeline& p,
			std::uint32_t set_index = 0
		) const -> void;

		auto copy_buffer(
			const gpu::buffer& src,
			const gpu::buffer& dst,
			std::size_t size,
			std::size_t src_offset = 0,
			std::size_t dst_offset = 0
		) const -> void;

		auto barrier(
			gpu::barrier_scope scope
		) const -> void;

		auto build_acceleration_structure(
			const vk::AccelerationStructureBuildGeometryInfoKHR& build_info,
			std::span<const vk::AccelerationStructureBuildRangeInfoKHR* const> range_infos
		) const -> void;

		auto pipeline_barrier(
			const vk::DependencyInfo& dep
		) const -> void;

		auto copy_image_to_buffer(
			vk::Image src_image,
			vk::ImageLayout src_layout,
			vk::Buffer dst_buffer,
			std::span<const vk::BufferImageCopy> regions
		) const -> void;

		auto capture_swapchain(
			const gpu::swap_chain& swapchain,
			const gpu::frame& frame,
			const gpu::buffer& dst
		) const -> void;

		auto blit_swapchain_to_image(
			const gpu::swap_chain& swapchain,
			const gpu::frame& frame,
			const gpu::image& dst
		) const -> void;

		auto begin_rendering(
			vec2u extent,
			const gpu::image* depth = nullptr,
			gpu::image_layout depth_layout = gpu::image_layout::general,
			bool clear_depth = true,
			float clear_depth_value = 1.0f
		) const -> void;

		auto begin_rendering(
			gpu::image_view depth_view,
			vec2u extent,
			float clear_depth_value = 1.0f
		) const -> void;

	private:
		friend class render_graph;
		vk::CommandBuffer m_cmd;
		explicit recording_context(vk::CommandBuffer cmd);
	};

	struct render_pass_data {
		std::type_index pass_type = typeid(void);
		std::vector<resource_usage> reads;
		std::vector<resource_usage> writes;
		std::vector<const buffer_resource*> tracked_buffers;
		vk::PipelineStageFlags2 tracked_stage = vk::PipelineStageFlagBits2::eAllCommands;
		std::vector<std::type_index> after_passes;
		bool enabled = true;
		std::optional<color_output_info> color_output;
		std::optional<depth_output_info> depth_output;
		std::move_only_function<void(recording_context&)> record_fn;
	};

	class pass_builder {
	public:
		pass_builder(pass_builder&&) = default;
		auto operator=(pass_builder&&) -> pass_builder& = default;
		~pass_builder();

		auto track(
			const buffer_resource& buf
		) -> pass_builder&;

		auto track(
			const gpu::buffer& buf
		) -> pass_builder&;

		template <typename... Args>
		auto reads(
			Args&&... args
		) -> pass_builder&;

		template <typename... Args>
		auto writes(
			Args&&... args
		) -> pass_builder&;

		template <typename T>
		auto after(
		) -> pass_builder&;

		auto when(
			bool condition
		) -> pass_builder&;

		auto color_output(
			const color_clear& clear_value
		) -> pass_builder&;

		auto color_output_load(
		) -> pass_builder&;

		auto depth_output(
			const depth_clear& clear_value
		) -> pass_builder&;

		auto depth_output_load(
		) -> pass_builder&;

		auto record(
			std::move_only_function<void(recording_context&)> fn
		) -> void;

	private:
		friend class render_graph;
		friend struct gpu_buffer_handle;
		explicit pass_builder(
			render_graph& graph,
			std::type_index pass_type
		);

		auto submit(
		) -> void;

		auto add_tracked(
			const buffer_resource* buf
		) -> void;

		render_graph* m_graph;
		render_pass_data m_pass;
		bool m_submitted = false;
	};

	class render_graph {
	public:
		explicit render_graph(
			gpu::device& device,
			gpu::swap_chain& swapchain,
			gpu::frame& frame
		);

		template <typename PassType>
		[[nodiscard]] auto add_pass(
		) -> pass_builder;

		auto execute(
		) -> void;

		auto clear(
		) -> void;

		auto set_gpu_timestamps_enabled(
			bool enabled
		) -> void;

		auto set_gpu_pipeline_stats_enabled(
			bool enabled
		) -> void;

		[[nodiscard]] auto current_frame(
		) const -> std::uint32_t;

		[[nodiscard]] auto extent(
		) const -> vec2u;

		[[nodiscard]] auto swapchain_image_view(
		) const -> vk::ImageView;

		[[nodiscard]] auto depth_image_view(
		) const -> vk::ImageView;

		[[nodiscard]] auto depth_image(
			this auto& self
		) -> auto&;

		[[nodiscard]] auto frame_in_progress(
		) const -> bool;

	private:
		friend class pass_builder;

		static constexpr std::uint32_t max_profiled_passes = 128;

		struct gpu_profile_slot {
			vk::raii::QueryPool timestamp_pool = nullptr;
			vk::raii::QueryPool stats_pool = nullptr;
			static_vector<std::type_index, max_profiled_passes> pass_types;
			std::uint32_t pass_count = 0;
			bool stats_issued = false;
			time_t<std::uint64_t> cpu_ref{};
			std::uint64_t frame_counter = 0;
			bool results_valid = false;
		};

		auto submit_pass(
			render_pass_data pass
		) -> void;

		auto ensure_profile_pools(
			gpu_profile_slot& slot
		) const -> void;

		auto read_profile_slot(
			gpu_profile_slot& slot
		) -> void;

		gpu::device* m_device;
		gpu::swap_chain* m_swapchain;
		gpu::frame* m_frame;
		std::vector<render_pass_data> m_passes;
		std::mutex m_pass_mutex;
		per_frame_resource<gpu_profile_slot> m_profile_slots;
		std::atomic<bool> m_gpu_timestamps_enabled{ true };
		std::atomic<bool> m_gpu_pipeline_stats_enabled{ false };
		time_t<double> m_timestamp_period_per_tick = nanoseconds(1.0);
		std::uint64_t m_frames_submitted = 0;
	};
}

namespace gse::vulkan {
	auto to_vk_stage(
		gpu::pipeline_stage s
	) -> vk::PipelineStageFlags2;

	auto to_vk_barrier(
		gpu::barrier_scope scope
	) -> vk::MemoryBarrier2;
}

gse::vulkan::recording_context::recording_context(const vk::CommandBuffer cmd) : m_cmd(cmd) {}

auto gse::vulkan::recording_context::copy_buffer(const gpu::buffer& src, const gpu::buffer& dst, const std::size_t size, const std::size_t src_offset, const std::size_t dst_offset) const -> void {
	m_cmd.copyBuffer(src.native().buffer, dst.native().buffer, vk::BufferCopy{
		.srcOffset = src_offset,
		.dstOffset = dst_offset,
		.size = size
	});
}

auto gse::vulkan::recording_context::barrier(const gpu::barrier_scope scope) const -> void {
	const auto memory_barrier = to_vk_barrier(scope);
	const vk::DependencyInfo dep{
		.memoryBarrierCount = 1,
		.pMemoryBarriers = &memory_barrier
	};
	m_cmd.pipelineBarrier2(dep);
}

auto gse::vulkan::recording_context::build_acceleration_structure(
	const vk::AccelerationStructureBuildGeometryInfoKHR& build_info,
	const std::span<const vk::AccelerationStructureBuildRangeInfoKHR* const> range_infos
) const -> void {
	m_cmd.buildAccelerationStructuresKHR(build_info, range_infos);
}

auto gse::vulkan::recording_context::pipeline_barrier(const vk::DependencyInfo& dep) const -> void {
	m_cmd.pipelineBarrier2(dep);
}

auto gse::vulkan::recording_context::copy_image_to_buffer(const vk::Image src_image, const vk::ImageLayout src_layout, const vk::Buffer dst_buffer, const std::span<const vk::BufferImageCopy> regions) const -> void {
	m_cmd.copyImageToBuffer(src_image, src_layout, dst_buffer, regions);
}

auto gse::vulkan::recording_context::capture_swapchain(
	const gpu::swap_chain& swapchain,
	const gpu::frame& frame,
	const gpu::buffer& dst
) const -> void {
	const auto image = swapchain.image(frame.image_index());
	const auto ext = swapchain.extent();
	const auto dst_buffer = dst.native().buffer;

	constexpr vk::ImageSubresourceRange subresource{
		.aspectMask = vk::ImageAspectFlagBits::eColor,
		.baseMipLevel = 0, .levelCount = 1,
		.baseArrayLayer = 0, .layerCount = 1
	};

	const vk::ImageMemoryBarrier2 to_transfer{
		.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
		.dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
		.dstAccessMask = vk::AccessFlagBits2::eTransferRead,
		.oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
		.newLayout = vk::ImageLayout::eTransferSrcOptimal,
		.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
		.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
		.image = image,
		.subresourceRange = subresource
	};
	m_cmd.pipelineBarrier2(vk::DependencyInfo{
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &to_transfer
	});

	const vk::BufferImageCopy region{
		.bufferOffset = 0,
		.bufferRowLength = 0,
		.bufferImageHeight = 0,
		.imageSubresource = {
			.aspectMask = vk::ImageAspectFlagBits::eColor,
			.mipLevel = 0,
			.baseArrayLayer = 0,
			.layerCount = 1
		},
		.imageOffset = {0, 0, 0},
		.imageExtent = {ext.x(), ext.y(), 1}
	};
	m_cmd.copyImageToBuffer(image, vk::ImageLayout::eTransferSrcOptimal, dst_buffer, region);

	const vk::ImageMemoryBarrier2 back_to_color{
		.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
		.srcAccessMask = vk::AccessFlagBits2::eTransferRead,
		.dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
		.oldLayout = vk::ImageLayout::eTransferSrcOptimal,
		.newLayout = vk::ImageLayout::eColorAttachmentOptimal,
		.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
		.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
		.image = image,
		.subresourceRange = subresource
	};
	m_cmd.pipelineBarrier2(vk::DependencyInfo{
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &back_to_color
	});
}

auto gse::vulkan::recording_context::blit_swapchain_to_image(const gpu::swap_chain& swapchain, const gpu::frame& frame, const gpu::image& dst) const -> void {
	const auto src_image = swapchain.image(frame.image_index());
	const auto src_ext = swapchain.extent();
	const auto dst_ext = dst.extent();

	constexpr vk::ImageSubresourceRange subresource{
		.aspectMask = vk::ImageAspectFlagBits::eColor,
		.baseMipLevel = 0, .levelCount = 1,
		.baseArrayLayer = 0, .layerCount = 1
	};

	const vk::ImageMemoryBarrier2 src_to_transfer{
		.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
		.dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
		.dstAccessMask = vk::AccessFlagBits2::eTransferRead,
		.oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
		.newLayout = vk::ImageLayout::eTransferSrcOptimal,
		.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
		.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
		.image = src_image,
		.subresourceRange = subresource
	};

	const vk::ImageMemoryBarrier2 dst_to_transfer{
		.srcStageMask = vk::PipelineStageFlagBits2::eNone,
		.srcAccessMask = vk::AccessFlagBits2::eNone,
		.dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
		.dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
		.oldLayout = vk::ImageLayout::eUndefined,
		.newLayout = vk::ImageLayout::eTransferDstOptimal,
		.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
		.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
		.image = dst.native().image,
		.subresourceRange = subresource
	};

	const std::array pre_barriers = { src_to_transfer, dst_to_transfer };
	m_cmd.pipelineBarrier2(vk::DependencyInfo{
		.imageMemoryBarrierCount = static_cast<std::uint32_t>(pre_barriers.size()),
		.pImageMemoryBarriers = pre_barriers.data()
	});

	const vk::ImageBlit region{
		.srcSubresource = { .aspectMask = vk::ImageAspectFlagBits::eColor, .layerCount = 1 },
		.srcOffsets = std::array{
			vk::Offset3D{0, 0, 0},
			vk::Offset3D{static_cast<int>(src_ext.x()), static_cast<int>(src_ext.y()), 1}
		},
		.dstSubresource = { .aspectMask = vk::ImageAspectFlagBits::eColor, .layerCount = 1 },
		.dstOffsets = std::array{
			vk::Offset3D{0, 0, 0},
			vk::Offset3D{static_cast<int>(dst_ext.x()), static_cast<int>(dst_ext.y()), 1}
		}
	};
	m_cmd.blitImage(
		src_image, vk::ImageLayout::eTransferSrcOptimal,
		dst.native().image, vk::ImageLayout::eTransferDstOptimal,
		region, vk::Filter::eNearest
	);

	const vk::ImageMemoryBarrier2 src_back{
		.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
		.srcAccessMask = vk::AccessFlagBits2::eTransferRead,
		.dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
		.oldLayout = vk::ImageLayout::eTransferSrcOptimal,
		.newLayout = vk::ImageLayout::eColorAttachmentOptimal,
		.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
		.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
		.image = src_image,
		.subresourceRange = subresource
	};

	const vk::ImageMemoryBarrier2 dst_to_read{
		.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
		.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
		.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
		.dstAccessMask = vk::AccessFlagBits2::eShaderSampledRead,
		.oldLayout = vk::ImageLayout::eTransferDstOptimal,
		.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
		.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
		.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
		.image = dst.native().image,
		.subresourceRange = subresource
	};

	const std::array post_barriers = { src_back, dst_to_read };
	m_cmd.pipelineBarrier2(vk::DependencyInfo{
		.imageMemoryBarrierCount = static_cast<std::uint32_t>(post_barriers.size()),
		.pImageMemoryBarriers = post_barriers.data()
	});
}

auto gse::vulkan::recording_context::bind_pipeline(const vk::PipelineBindPoint point, const vk::Pipeline pipeline) const -> void {
	m_cmd.bindPipeline(point, pipeline);
}

auto gse::vulkan::recording_context::bind_descriptors(const vk::PipelineBindPoint point, const vk::PipelineLayout layout, const descriptor_region& region, const std::uint32_t set_index) const -> void {
	assert(region, std::source_location::current(), "Cannot bind null descriptor region");
	region.heap->bind(m_cmd, point, layout, set_index, region);
}

auto gse::vulkan::recording_context::push(const gpu::cached_push_constants& cache, const vk::PipelineLayout layout) const -> void {
	cache.replay(m_cmd, layout);
}

auto gse::vulkan::recording_context::set_viewport(const float x, const float y, const float width, const float height, const float min_depth, const float max_depth) const -> void {
	const vk::Viewport vp{ .x = x, .y = y, .width = width, .height = height, .minDepth = min_depth, .maxDepth = max_depth };
	m_cmd.setViewport(0, vp);
}

auto gse::vulkan::recording_context::set_scissor(const std::int32_t x, const std::int32_t y, const std::uint32_t width, const std::uint32_t height) const -> void {
	const vk::Rect2D sc{ .offset = { x, y }, .extent = { width, height } };
	m_cmd.setScissor(0, sc);
}

auto gse::vulkan::recording_context::draw(const std::uint32_t vertex_count, const std::uint32_t instance_count, const std::uint32_t first_vertex, const std::uint32_t first_instance) const -> void {
	m_cmd.draw(vertex_count, instance_count, first_vertex, first_instance);
}

auto gse::vulkan::recording_context::draw_indexed(const std::uint32_t index_count, const std::uint32_t instance_count, const std::uint32_t first_index, const std::int32_t vertex_offset, const std::uint32_t first_instance) const -> void {
	m_cmd.drawIndexed(index_count, instance_count, first_index, vertex_offset, first_instance);
}

auto gse::vulkan::recording_context::draw_indirect(const vk::Buffer buffer, const vk::DeviceSize offset, const std::uint32_t draw_count, const std::uint32_t stride) const -> void {
	m_cmd.drawIndexedIndirect(buffer, offset, draw_count, stride);
}

auto gse::vulkan::recording_context::draw_mesh_tasks(const std::uint32_t x, const std::uint32_t y, const std::uint32_t z) const -> void {
	m_cmd.drawMeshTasksEXT(x, y, z);
}

auto gse::vulkan::recording_context::dispatch(const std::uint32_t x, const std::uint32_t y, const std::uint32_t z) const -> void {
	m_cmd.dispatch(x, y, z);
}

auto gse::vulkan::recording_context::bind_vertex_buffers(const std::uint32_t first, const std::span<const vk::Buffer> buffers, const std::span<const vk::DeviceSize> offsets) const -> void {
	m_cmd.bindVertexBuffers(first, static_cast<std::uint32_t>(buffers.size()), buffers.data(), offsets.data());
}

auto gse::vulkan::recording_context::bind_index_buffer(const vk::Buffer buffer, const vk::DeviceSize offset, const vk::IndexType index_type) const -> void {
	m_cmd.bindIndexBuffer(buffer, offset, index_type);
}

auto gse::vulkan::recording_context::begin_rendering(const vk::RenderingInfo& info) const -> void {
	m_cmd.beginRendering(info);
}

auto gse::vulkan::recording_context::end_rendering() const -> void {
	m_cmd.endRendering();
}

auto gse::vulkan::recording_context::bind_pipeline(const vk::PipelineBindPoint point, const gpu::pipeline& p) const -> void {
	m_cmd.bindPipeline(point, p.native_pipeline());
}

auto gse::vulkan::recording_context::bind_descriptors(const vk::PipelineBindPoint point, const gpu::pipeline& p, const descriptor_region& region, const std::uint32_t set_index) const -> void {
	assert(region, std::source_location::current(), "Cannot bind null descriptor region");
	region.heap->bind(m_cmd, point, p.native_layout(), set_index, region);
}

auto gse::vulkan::recording_context::push(const gpu::pipeline& p, const gpu::cached_push_constants& cache) const -> void {
	cache.replay(m_cmd, p.native_layout());
}

auto gse::vulkan::recording_context::draw_indirect(const gpu::buffer& buf, const std::size_t offset, const std::uint32_t draw_count, const std::uint32_t stride) const -> void {
	m_cmd.drawIndexedIndirect(buf.native().buffer, offset, draw_count, stride);
}

auto gse::vulkan::recording_context::bind(const gpu::pipeline& p) const -> void {
	const auto point = p.point() == gpu::bind_point::graphics
		? vk::PipelineBindPoint::eGraphics
		: vk::PipelineBindPoint::eCompute;
	m_cmd.bindPipeline(point, p.native_pipeline());
}

auto gse::vulkan::recording_context::bind_descriptors(const gpu::pipeline& p, const descriptor_region& region, const std::uint32_t set_index) const -> void {
	assert(region, std::source_location::current(), "Cannot bind null descriptor region");
	const auto point = p.point() == gpu::bind_point::graphics
		? vk::PipelineBindPoint::eGraphics
		: vk::PipelineBindPoint::eCompute;
	region.heap->bind(m_cmd, point, p.native_layout(), set_index, region);
}

auto gse::vulkan::recording_context::bind_descriptors(const gpu::pipeline& p, const gpu::descriptor_region& region, const std::uint32_t set_index) const -> void {
	bind_descriptors(p, region.native(), set_index);
}

auto gse::vulkan::recording_context::bind_vertex(const gpu::buffer& buf, const std::size_t offset) const -> void {
	const vk::Buffer buffers[]{ buf.native().buffer };
	const vk::DeviceSize offsets[]{ offset };
	m_cmd.bindVertexBuffers(0, 1, buffers, offsets);
}

auto gse::vulkan::recording_context::bind_index(const gpu::buffer& buf, const gpu::index_type type, const std::size_t offset) const -> void {
	const auto vk_type = type == gpu::index_type::uint16 ? vk::IndexType::eUint16 : vk::IndexType::eUint32;
	m_cmd.bindIndexBuffer(buf.native().buffer, offset, vk_type);
}

auto gse::vulkan::recording_context::set_viewport(const vec2u extent) const -> void {
	const vk::Viewport vp{
		.x = 0.0f, .y = 0.0f,
		.width = static_cast<float>(extent.x()), .height = static_cast<float>(extent.y()),
		.minDepth = 0.0f, .maxDepth = 1.0f
	};
	m_cmd.setViewport(0, vp);
}

auto gse::vulkan::recording_context::set_scissor(const vec2u extent) const -> void {
	const vk::Rect2D sc{ .offset = { 0, 0 }, .extent = { extent.x(), extent.y() } };
	m_cmd.setScissor(0, sc);
}

auto gse::vulkan::recording_context::begin_rendering(const vec2u extent, const gpu::image* depth, const gpu::image_layout depth_layout, const bool clear_depth, const float clear_depth_value) const -> void {
	auto to_vk = [](const gpu::image_layout l) -> vk::ImageLayout {
		switch (l) {
			case gpu::image_layout::general: return vk::ImageLayout::eGeneral;
			case gpu::image_layout::shader_read_only: return vk::ImageLayout::eShaderReadOnlyOptimal;
			default: return vk::ImageLayout::eUndefined;
		}
	};

	std::optional<vk::RenderingAttachmentInfo> depth_att;
	if (depth) {
		depth_att = vk::RenderingAttachmentInfo{
			.imageView = depth->native().view,
			.imageLayout = to_vk(depth_layout),
			.loadOp = clear_depth ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.clearValue = vk::ClearValue{ .depthStencil = { clear_depth_value, 0 } }
		};
	}

	const vk::RenderingInfo ri{
		.renderArea = { { 0, 0 }, { extent.x(), extent.y() } },
		.layerCount = 1,
		.colorAttachmentCount = 0,
		.pColorAttachments = nullptr,
		.pDepthAttachment = depth_att ? &*depth_att : nullptr
	};
	m_cmd.beginRendering(ri);
}

auto gse::vulkan::recording_context::begin_rendering(const gpu::image_view depth_view, const vec2u extent, const float clear_depth_value) const -> void {
	const vk::RenderingAttachmentInfo depth_att{
		.imageView = depth_view.native(),
		.imageLayout = vk::ImageLayout::eGeneral,
		.loadOp = vk::AttachmentLoadOp::eClear,
		.storeOp = vk::AttachmentStoreOp::eStore,
		.clearValue = vk::ClearValue{ .depthStencil = { clear_depth_value, 0 } }
	};
	const vk::RenderingInfo ri{
		.renderArea = { { 0, 0 }, { extent.x(), extent.y() } },
		.layerCount = 1,
		.colorAttachmentCount = 0,
		.pColorAttachments = nullptr,
		.pDepthAttachment = &depth_att
	};
	m_cmd.beginRendering(ri);
}

auto gse::vulkan::recording_context::commit(const descriptor_writer& writer, const gpu::pipeline& p, const std::uint32_t set_index) const -> void {
	const auto point = p.point() == gpu::bind_point::graphics
		? vk::PipelineBindPoint::eGraphics
		: vk::PipelineBindPoint::eCompute;
	writer.commit(m_cmd, point, p.native_layout(), set_index);
}

auto gse::vulkan::recording_context::bind_vertex_buffer(const gpu::buffer& buf, const vk::DeviceSize offset) const -> void {
	const vk::Buffer buffers[]{ buf.native().buffer };
	const vk::DeviceSize offsets[]{ offset };
	m_cmd.bindVertexBuffers(0, 1, buffers, offsets);
}

auto gse::vulkan::recording_context::bind_index_buffer(const gpu::buffer& buf, const vk::DeviceSize offset, const vk::IndexType index_type) const -> void {
	m_cmd.bindIndexBuffer(buf.native().buffer, offset, index_type);
}

auto gse::vulkan::sampled(const image_resource& img, const vk::PipelineStageFlags2 stage) -> resource_usage {
	return { { .ptr = std::addressof(img), .type = resource_type::image }, stage, vk::AccessFlagBits2::eShaderSampledRead };
}

auto gse::vulkan::storage(const buffer_resource& buf, const vk::PipelineStageFlags2 stage) -> resource_usage {
	return { { .ptr = std::addressof(buf), .type = resource_type::buffer }, stage, vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite };
}

auto gse::vulkan::storage_read(const buffer_resource& buf, const vk::PipelineStageFlags2 stage) -> resource_usage {
	return { { .ptr = std::addressof(buf), .type = resource_type::buffer }, stage, vk::AccessFlagBits2::eShaderStorageRead };
}

auto gse::vulkan::transfer_read(const buffer_resource& buf) -> resource_usage {
	return {
		{
			.ptr = std::addressof(buf),
			.type = resource_type::buffer
		},
		vk::PipelineStageFlagBits2::eCopy,
		vk::AccessFlagBits2::eTransferRead
	};
}

auto gse::vulkan::transfer_write(const buffer_resource& buf) -> resource_usage {
	return {
		{
			.ptr = std::addressof(buf),
			.type = resource_type::buffer
		},
		vk::PipelineStageFlagBits2::eCopy,
		vk::AccessFlagBits2::eTransferWrite
	};
}

auto gse::vulkan::indirect_read(const buffer_resource& buf, const vk::PipelineStageFlags2 stage) -> resource_usage {
	return { { .ptr = std::addressof(buf), .type = resource_type::buffer }, stage, vk::AccessFlagBits2::eIndirectCommandRead };
}

auto gse::vulkan::attachment(const image_resource& img, const vk::PipelineStageFlags2 stage) -> resource_usage {
	const auto access = (stage == vk::PipelineStageFlagBits2::eLateFragmentTests || stage == vk::PipelineStageFlagBits2::eEarlyFragmentTests)
		? vk::AccessFlagBits2::eDepthStencilAttachmentWrite
		: vk::AccessFlagBits2::eColorAttachmentWrite;
	return { { .ptr = std::addressof(img), .type = resource_type::image }, stage, access };
}

auto gse::gpu::storage_read(const buffer& buf, const pipeline_stage stage) -> vulkan::resource_usage {
	return vulkan::storage_read(buf.native(), vulkan::to_vk_stage(stage));
}

auto gse::gpu::storage_write(const buffer& buf, const pipeline_stage stage) -> vulkan::resource_usage {
	return vulkan::storage(buf.native(), vulkan::to_vk_stage(stage));
}

auto gse::gpu::transfer_read(const buffer& buf) -> vulkan::resource_usage {
	return vulkan::transfer_read(buf.native());
}

auto gse::gpu::transfer_write(const buffer& buf) -> vulkan::resource_usage {
	return vulkan::transfer_write(buf.native());
}

auto gse::gpu::sampled(const image& img, const pipeline_stage stage) -> vulkan::resource_usage {
	return vulkan::sampled(img.native(), vulkan::to_vk_stage(stage));
}

auto gse::gpu::indirect_read(const buffer& buf, const pipeline_stage stage) -> vulkan::resource_usage {
	return vulkan::indirect_read(buf.native(), vulkan::to_vk_stage(stage));
}

namespace {
	const char swapchain_sentinel = 0;
}

gse::vulkan::pass_builder::pass_builder(render_graph& graph, const std::type_index pass_type)
	: m_graph(std::addressof(graph)), m_pass{ .pass_type = pass_type } {}

gse::vulkan::pass_builder::~pass_builder() {
	assert(m_submitted, std::source_location::current(), "pass_builder destroyed without calling record()");
}

auto gse::vulkan::pass_builder::track(const buffer_resource& buf) -> pass_builder& {
	add_tracked(std::addressof(buf));
	return *this;
}

auto gse::vulkan::pass_builder::track(const gpu::buffer& buf) -> pass_builder& {
	add_tracked(std::addressof(buf.native()));
	return *this;
}

auto gse::vulkan::pass_builder::add_tracked(const buffer_resource* buf) -> void {
	for (const auto* existing : m_pass.tracked_buffers) {
		if (existing == buf) return;
	}
	m_pass.tracked_buffers.push_back(buf);
}

template <typename... Args>
auto gse::vulkan::pass_builder::reads(Args&&... args) -> pass_builder& {
	(m_pass.reads.push_back(std::forward<Args>(args)), ...);
	return *this;
}

template <typename... Args>
auto gse::vulkan::pass_builder::writes(Args&&... args) -> pass_builder& {
	(m_pass.writes.push_back(std::forward<Args>(args)), ...);
	return *this;
}

template <typename T>
auto gse::vulkan::pass_builder::after() -> pass_builder& {
	m_pass.after_passes.push_back(std::type_index(typeid(T)));
	return *this;
}

auto gse::vulkan::pass_builder::when(const bool condition) -> pass_builder& {
	m_pass.enabled = condition;
	return *this;
}

auto gse::vulkan::pass_builder::color_output(const color_clear& clear_value) -> pass_builder& {
	m_pass.color_output = color_output_info{
		.is_swapchain = true,
		.op = load_op::clear_color,
		.clear_value = clear_value
	};
	return *this;
}

auto gse::vulkan::pass_builder::color_output_load() -> pass_builder& {
	m_pass.color_output = color_output_info{
		.is_swapchain = true,
		.op = load_op::load
	};
	m_pass.reads.push_back({
		{
			.ptr = &swapchain_sentinel, 
			.type = resource_type::image
		},
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::AccessFlagBits2::eColorAttachmentRead
	});
	return *this;
}

auto gse::vulkan::pass_builder::depth_output(const depth_clear& clear_value) -> pass_builder& {
	m_pass.depth_output = depth_output_info{
		.op = load_op::clear_depth,
		.clear_value = clear_value
	};
	m_pass.writes.push_back(attachment(m_graph->m_swapchain->depth_image().native(), vk::PipelineStageFlagBits2::eLateFragmentTests));
	return *this;
}

auto gse::vulkan::pass_builder::depth_output_load() -> pass_builder& {
	m_pass.depth_output = depth_output_info{
		.op = load_op::load
	};
	m_pass.reads.push_back({
		{
			.ptr = std::addressof(m_graph->m_swapchain->depth_image().native()),
			.type = resource_type::image
		},
		vk::PipelineStageFlagBits2::eEarlyFragmentTests,
		vk::AccessFlagBits2::eDepthStencilAttachmentRead
	});
	return *this;
}

auto gse::vulkan::pass_builder::record(std::move_only_function<void(recording_context&)> fn) -> void {
	if (m_pass.color_output) {
		m_pass.writes.push_back({
			{
				.ptr = &swapchain_sentinel, 
				.type = resource_type::image
			},
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead
		});
	}

	m_pass.record_fn = std::move(fn);
	m_submitted = true;
	submit();
}

auto gse::vulkan::pass_builder::submit() -> void {
	m_graph->submit_pass(std::move(m_pass));
}

gse::vulkan::render_graph::render_graph(gpu::device& device, gpu::swap_chain& swapchain, gpu::frame& frame) : m_device(std::addressof(device)), m_swapchain(std::addressof(swapchain)), m_frame(std::addressof(frame)) {
	m_passes.reserve(32);
	m_timestamp_period_per_tick = nanoseconds(static_cast<double>(device.device_config().physical_device.getProperties().limits.timestampPeriod));
}

auto gse::vulkan::render_graph::set_gpu_timestamps_enabled(const bool enabled) -> void {
	m_gpu_timestamps_enabled.store(enabled, std::memory_order_relaxed);
}

auto gse::vulkan::render_graph::set_gpu_pipeline_stats_enabled(const bool enabled) -> void {
	m_gpu_pipeline_stats_enabled.store(enabled, std::memory_order_relaxed);
}

auto gse::vulkan::render_graph::ensure_profile_pools(gpu_profile_slot& slot) const -> void {
	if (!*slot.timestamp_pool) {
		slot.timestamp_pool = m_device->logical_device().createQueryPool({
			.queryType = vk::QueryType::eTimestamp,
			.queryCount = max_profiled_passes * 2 + 1
		});
	}
	if (!*slot.stats_pool) {
		slot.stats_pool = m_device->logical_device().createQueryPool({
			.queryType = vk::QueryType::ePipelineStatistics,
			.queryCount = max_profiled_passes,
			.pipelineStatistics =
				vk::QueryPipelineStatisticFlagBits::eInputAssemblyVertices
				| vk::QueryPipelineStatisticFlagBits::eInputAssemblyPrimitives
				| vk::QueryPipelineStatisticFlagBits::eClippingInvocations
				| vk::QueryPipelineStatisticFlagBits::eFragmentShaderInvocations
		});
	}
}

auto gse::vulkan::render_graph::read_profile_slot(gpu_profile_slot& slot) -> void {
	if (!slot.results_valid || slot.pass_count == 0) {
		return;
	}

	const std::uint32_t timestamp_count = slot.pass_count * 2 + 1;
	const auto [ts_status, timestamps] = slot.timestamp_pool.getResults<std::uint64_t>(
		0,
		timestamp_count,
		timestamp_count * sizeof(std::uint64_t),
		sizeof(std::uint64_t),
		vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait
	);

	if (ts_status != vk::Result::eSuccess) {
		slot.results_valid = false;
		return;
	}

	const auto period = m_timestamp_period_per_tick;
	const auto gpu_ref = static_cast<double>(timestamps[0]) * period;
	const auto offset = time_t<double>(slot.cpu_ref) - gpu_ref;

	const auto pass_tag = [](const std::type_index& ti) -> std::string {
		std::string_view raw = ti.name();
		for (const std::string_view prefix : { "struct ", "class " }) {
			if (raw.starts_with(prefix)) {
				raw.remove_prefix(prefix.size());
			}
		}
		if (constexpr std::string_view ns = "gse::renderer::"; raw.starts_with(ns)) {
			raw.remove_prefix(ns.size());
		}

		const auto bracket = raw.find('[');
		std::string_view name = bracket == std::string_view::npos ? raw : raw.substr(0, bracket);
		const std::string_view module_suffix = bracket == std::string_view::npos ? "" : raw.substr(bracket);

		if (constexpr std::string_view tail = "::state"; name.ends_with(tail)) {
			name.remove_suffix(tail.size());
		}

		return std::format("gpu:{}{}", name, module_suffix);
	};

	for (std::uint32_t i = 0; i < slot.pass_count; ++i) {
		const auto start = static_cast<double>(timestamps[1 + i * 2]) * period + offset;
		const auto end = static_cast<double>(timestamps[2 + i * 2]) * period + offset;
		const auto gpu_id = find_or_generate_id(pass_tag(slot.pass_types[i]));
		const std::uint64_t key = (slot.frame_counter << 16) | i;

		trace::begin_async_at(gpu_id, key, trace::gpu_virtual_tid, time_t<std::uint64_t>(start));
		trace::end_async_at(gpu_id, key, trace::gpu_virtual_tid, time_t<std::uint64_t>(end));

		profile::ingest_gpu_sample(gpu_id, end - start);
	}

	if (slot.stats_issued) {
		constexpr std::uint32_t stats_per_pass = 4;
		const auto [stats_status, stats] = slot.stats_pool.getResults<std::uint64_t>(
			0,
			slot.pass_count,
			slot.pass_count * stats_per_pass * sizeof(std::uint64_t),
			sizeof(std::uint64_t) * stats_per_pass,
			vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait
		);

		if (stats_status == vk::Result::eSuccess) {
			static constexpr std::array<const char*, stats_per_pass> labels{
				":ia_verts", ":ia_prims", ":clip_invocs", ":fs_invocs"
			};
			for (std::uint32_t i = 0; i < slot.pass_count; ++i) {
				const auto start = static_cast<double>(timestamps[1 + i * 2]) * period + offset;
				const auto pass_name = pass_tag(slot.pass_types[i]);
				for (std::uint32_t s = 0; s < stats_per_pass; ++s) {
					const auto stat_id = find_or_generate_id(pass_name + labels[s]);
					trace::counter_at(stat_id, static_cast<double>(stats[i * stats_per_pass + s]), trace::gpu_stats_virtual_tid, time_t<std::uint64_t>(start));
				}
			}
		}
	}

	slot.results_valid = false;
}

template <typename PassType>
auto gse::vulkan::render_graph::add_pass() -> pass_builder {
	return pass_builder(*this, std::type_index(typeid(PassType)));
}

auto gse::vulkan::render_graph::submit_pass(render_pass_data pass) -> void {
	if (pass.enabled) {
		std::lock_guard lock(m_pass_mutex);
		m_passes.push_back(std::move(pass));
	}
}

auto gse::vulkan::render_graph::clear() -> void {
	std::lock_guard lock(m_pass_mutex);
	m_passes.clear();
}

auto gse::vulkan::render_graph::current_frame() const -> std::uint32_t {
	return m_frame->current_frame();
}

auto gse::vulkan::render_graph::extent() const -> vec2u {
	return m_swapchain->extent();
}

auto gse::vulkan::render_graph::swapchain_image_view() const -> vk::ImageView {
	return m_swapchain->image_view(m_frame->image_index());
}

auto gse::vulkan::render_graph::depth_image_view() const -> vk::ImageView {
	return m_swapchain->depth_image().native().view;
}

auto gse::vulkan::render_graph::depth_image(this auto& self) -> auto& {
	return self.m_swapchain->depth_image();
}

auto gse::vulkan::render_graph::frame_in_progress() const -> bool {
	return m_frame->frame_in_progress();
}

auto gse::vulkan::render_graph::execute() -> void {
	if (!m_frame->frame_in_progress()) {
		return;
	}

	std::vector<render_pass_data> passes;
	{
		std::lock_guard lock(m_pass_mutex);
		passes = std::move(m_passes);
	}

	const auto command = m_frame->command_buffer();
	const auto image_index = m_frame->image_index();
	const auto swap_image = m_swapchain->image(image_index);
	const auto swap_extent = m_swapchain->extent();
	const vk::Extent2D vk_extent{ swap_extent.x(), swap_extent.y() };

	auto& slot = m_profile_slots[m_frame->current_frame()];
	const bool timestamps_enabled = m_gpu_timestamps_enabled.load(std::memory_order_relaxed);
	const bool stats_enabled = m_gpu_pipeline_stats_enabled.load(std::memory_order_relaxed);

	if (m_frames_submitted >= per_frame_resource<gpu_profile_slot>::frames_in_flight) {
		read_profile_slot(slot);
	}

	auto bump_frames = make_scope_exit([this] {
		++m_frames_submitted;
	});

	const vk::ImageMemoryBarrier2 begin_barrier{
		.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
		.srcAccessMask = {},
		.dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead,
		.oldLayout = vk::ImageLayout::eUndefined,
		.newLayout = vk::ImageLayout::eColorAttachmentOptimal,
		.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
		.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
		.image = swap_image,
		.subresourceRange = {
			.aspectMask = vk::ImageAspectFlagBits::eColor,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1
		}
	};

	const vk::DependencyInfo begin_dep{
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &begin_barrier
	};
	command.pipelineBarrier2(begin_dep);

	slot.pass_types.clear();
	slot.pass_count = 0;
	slot.stats_issued = false;
	slot.results_valid = false;

	if (timestamps_enabled) {
		ensure_profile_pools(slot);
		command.resetQueryPool(*slot.timestamp_pool, 0, max_profiled_passes * 2 + 1);
		if (stats_enabled) {
			command.resetQueryPool(*slot.stats_pool, 0, max_profiled_passes);
		}
		slot.cpu_ref = system_clock::now<trace::tick_step>();
		slot.frame_counter = m_frames_submitted;
		command.writeTimestamp2(vk::PipelineStageFlagBits2::eAllCommands, *slot.timestamp_pool, 0);
	}

	if (passes.empty()) {
		const vk::ImageMemoryBarrier2 present_barrier{
			.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
			.dstStageMask = vk::PipelineStageFlagBits2::eBottomOfPipe,
			.dstAccessMask = {},
			.oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.newLayout = vk::ImageLayout::ePresentSrcKHR,
			.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
			.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
			.image = swap_image,
			.subresourceRange = {
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			}
		};
		const vk::DependencyInfo end_dep{
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &present_barrier
		};
		command.pipelineBarrier2(end_dep);
		return;
	}

	std::vector<std::size_t> sorted;

	gse::trace::scope(gse::find_or_generate_id("graph::plan"), [&] {
		std::unordered_map<std::type_index, std::size_t> type_to_index;
		for (std::size_t i = 0; i < passes.size(); ++i) {
			type_to_index[passes[i].pass_type] = i;
		}

		std::vector<std::vector<std::size_t>> adj(passes.size());
		std::vector<std::size_t> in_degree(passes.size(), 0);

		auto add_edge = [&](const std::size_t from, const std::size_t to) {
			for (const auto n : adj[from]) {
				if (n == to) return;
			}
			adj[from].push_back(to);
			++in_degree[to];
		};

		for (std::size_t i = 0; i < passes.size(); ++i) {
			for (const auto& dep : passes[i].after_passes) {
				if (auto it = type_to_index.find(dep); it != type_to_index.end()) {
					add_edge(it->second, i);
				}
			}
		}

		for (std::size_t i = 0; i < passes.size(); ++i) {
			for (std::size_t j = i + 1; j < passes.size(); ++j) {
				bool i_writes_j_reads = false;
				bool j_writes_i_reads = false;

				for (const auto& w : passes[i].writes) {
					for (const auto& r : passes[j].reads) {
						if (w.resource.ptr && r.resource.ptr && w.resource.ptr == r.resource.ptr) i_writes_j_reads = true;
					}
				}
				for (const auto& w : passes[j].writes) {
					for (const auto& r : passes[i].reads) {
						if (w.resource.ptr && r.resource.ptr && w.resource.ptr == r.resource.ptr) j_writes_i_reads = true;
					}
				}

				if (i_writes_j_reads && j_writes_i_reads) {
					add_edge(i, j);
				} else if (i_writes_j_reads) {
					add_edge(i, j);
				} else if (j_writes_i_reads) {
					add_edge(j, i);
				}
			}
		}

		sorted.reserve(passes.size());

		std::queue<std::size_t> queue;
		for (std::size_t i = 0; i < passes.size(); ++i) {
			if (in_degree[i] == 0) queue.push(i);
		}

		while (!queue.empty()) {
			auto front = queue.front();
			queue.pop();
			sorted.push_back(front);
			for (auto next : adj[front]) {
				if (--in_degree[next] == 0) queue.push(next);
			}
		}
	});

	auto& worker_pools = m_device->worker_command_pools();
	worker_pools.reset_frame(m_frame->current_frame());
	const auto color_format = m_swapchain->format();

	std::vector<std::size_t> pass_to_serial(passes.size());
	for (std::size_t si = 0; si < sorted.size(); ++si) {
		pass_to_serial[sorted[si]] = si;
	}

	std::vector<vk::CommandBuffer> pass_secondaries(passes.size());

	gse::trace::scope(gse::find_or_generate_id("graph::record_parallel"), [&] {
		task::parallel_invoke_range(0, passes.size(), [&](std::size_t pi) {
			auto& pass = passes[pi];
			const bool is_graphics_pass = pass.color_output || pass.depth_output;
			const std::size_t si = pass_to_serial[pi];
			const bool profile_pass = timestamps_enabled && si < max_profiled_passes;
			const bool issue_stats = profile_pass && stats_enabled && is_graphics_pass;

			const auto worker_idx = task::current_worker();
			assert(worker_idx.has_value(), std::source_location::current(), "graph::record_parallel: thread has no arena slot");
			const auto frame_idx = m_frame->current_frame();
			const auto secondary = worker_pools.acquire_secondary(*worker_idx, frame_idx);

			const std::array<vk::Format, 1> color_formats{ color_format };
			const vk::CommandBufferInheritanceRenderingInfo rendering_inherit{
				.viewMask = 0,
				.colorAttachmentCount = pass.color_output ? 1u : 0u,
				.pColorAttachmentFormats = pass.color_output ? color_formats.data() : nullptr,
				.depthAttachmentFormat = pass.depth_output ? vk::Format::eD32Sfloat : vk::Format::eUndefined,
				.stencilAttachmentFormat = vk::Format::eUndefined,
				.rasterizationSamples = vk::SampleCountFlagBits::e1,
			};

			vk::CommandBufferInheritanceInfo inherit{};
			vk::CommandBufferUsageFlags begin_flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
			if (is_graphics_pass) {
				inherit.pNext = &rendering_inherit;
				begin_flags |= vk::CommandBufferUsageFlagBits::eRenderPassContinue;
			}
			if (issue_stats) {
				inherit.pipelineStatistics =
					vk::QueryPipelineStatisticFlagBits::eInputAssemblyVertices
					| vk::QueryPipelineStatisticFlagBits::eInputAssemblyPrimitives
					| vk::QueryPipelineStatisticFlagBits::eClippingInvocations
					| vk::QueryPipelineStatisticFlagBits::eFragmentShaderInvocations;
			}

			secondary.begin({
				.flags = begin_flags,
				.pInheritanceInfo = &inherit
			});
			m_device->descriptor_heap().bind_buffer(secondary);
			recording_context secondary_ctx(secondary);
			pass.record_fn(secondary_ctx);
			secondary.end();

			pass_secondaries[pi] = secondary;
		});
	});

	gse::trace::scope(gse::find_or_generate_id("graph::record_replay"), [&] {
		for (std::size_t si = 0; si < sorted.size(); ++si) {
			auto& pass = passes[sorted[si]];

			std::vector<vk::MemoryBarrier2> barriers;

			if (!pass.tracked_buffers.empty()) {
				vk::PipelineStageFlags2 tracked_stage{};
				vk::AccessFlags2 tracked_access{};
				bool has_unmatched_tracked_buffer = false;

				for (const auto* tracked : pass.tracked_buffers) {
					bool matched = false;
					for (const auto& [resource, stage, access] : pass.reads) {
						if (resource.ptr == tracked && resource.type == resource_type::buffer) {
							tracked_stage |= stage;
							tracked_access |= access;
							matched = true;
						}
					}
					if (!matched) {
						has_unmatched_tracked_buffer = true;
					}
				}

				if (has_unmatched_tracked_buffer || tracked_stage == vk::PipelineStageFlags2{}) {
					tracked_stage |= pass.tracked_stage;
					tracked_access |= vk::AccessFlagBits2::eShaderStorageRead
						| vk::AccessFlagBits2::eUniformRead
						| vk::AccessFlagBits2::eTransferRead
						| vk::AccessFlagBits2::eIndirectCommandRead
						| vk::AccessFlagBits2::eVertexAttributeRead
						| vk::AccessFlagBits2::eIndexRead;
				}

				barriers.push_back({
					.srcStageMask = vk::PipelineStageFlagBits2::eHost,
					.srcAccessMask = vk::AccessFlagBits2::eHostWrite,
					.dstStageMask = tracked_stage,
					.dstAccessMask = tracked_access
				});
			}

			for (std::size_t pi = 0; pi < si; ++pi) {
				for (const auto& prev = passes[sorted[pi]]; const auto& [prev_resource, prev_stage, prev_access] : prev.writes) {
					if (!prev_resource.ptr) continue;
					for (const auto& [read_resource, read_stage, read_access] : pass.reads) {
						if (read_resource.ptr && prev_resource.ptr == read_resource.ptr) {
							barriers.push_back({
								.srcStageMask = prev_stage,
								.srcAccessMask = prev_access,
								.dstStageMask = read_stage,
								.dstAccessMask = read_access
							});
						}
					}
					for (const auto& [cur_resource, cur_stage, cur_access] : pass.writes) {
						if (cur_resource.ptr && prev_resource.ptr == cur_resource.ptr) {
							barriers.push_back({
								.srcStageMask = prev_stage,
								.srcAccessMask = prev_access,
								.dstStageMask = cur_stage,
								.dstAccessMask = cur_access
							});
						}
					}
				}
			}

			if (!barriers.empty()) {
				const vk::DependencyInfo dep{
					.memoryBarrierCount = static_cast<std::uint32_t>(barriers.size()),
					.pMemoryBarriers = barriers.data()
				};
				command.pipelineBarrier2(dep);
			}

			const bool profile_pass = timestamps_enabled && slot.pass_count < max_profiled_passes;
			const std::uint32_t pass_index = slot.pass_count;
			const bool is_graphics_pass = pass.color_output || pass.depth_output;
			const bool issue_stats = profile_pass && stats_enabled && is_graphics_pass;

			if (profile_pass) {
				command.writeTimestamp2(vk::PipelineStageFlagBits2::eNone, *slot.timestamp_pool, 1 + pass_index * 2);
				slot.pass_types.push_back(pass.pass_type);
				++slot.pass_count;
				if (issue_stats) {
					command.beginQuery(*slot.stats_pool, pass_index, {});
					slot.stats_issued = true;
				}
			}

			const auto secondary = pass_secondaries[sorted[si]];

			if (is_graphics_pass) {
				std::vector<vk::RenderingAttachmentInfo> color_attachments;
				std::optional<vk::RenderingAttachmentInfo> depth_att;

				if (pass.color_output) {
					const auto& co = *pass.color_output;
					auto vk_load = vk::AttachmentLoadOp::eDontCare;
					vk::ClearValue clear_val{};

					if (co.op == load_op::clear_color) {
						vk_load = vk::AttachmentLoadOp::eClear;
						clear_val.color = vk::ClearColorValue{
							.float32 = std::array{ co.clear_value.r, co.clear_value.g, co.clear_value.b, co.clear_value.a }
						};
					} else if (co.op == load_op::load) {
						vk_load = vk::AttachmentLoadOp::eLoad;
					}

					color_attachments.push_back({
						.imageView = m_swapchain->image_view(image_index),
						.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
						.loadOp = vk_load,
						.storeOp = vk::AttachmentStoreOp::eStore,
						.clearValue = clear_val
					});
				}

				if (pass.depth_output) {
					const auto& [op, clear_value] = *pass.depth_output;
					auto vk_load = vk::AttachmentLoadOp::eDontCare;
					vk::ClearValue clear_val{};

					if (op == load_op::clear_depth) {
						vk_load = vk::AttachmentLoadOp::eClear;
						clear_val.depthStencil = vk::ClearDepthStencilValue{ .depth = clear_value.depth };
					} else if (op == load_op::load) {
						vk_load = vk::AttachmentLoadOp::eLoad;
					}

					depth_att = vk::RenderingAttachmentInfo{
						.imageView = m_swapchain->depth_image().native().view,
						.imageLayout = vk::ImageLayout::eGeneral,
						.loadOp = vk_load,
						.storeOp = vk::AttachmentStoreOp::eStore,
						.clearValue = clear_val
					};
				}

				const vk::RenderingInfo ri{
					.flags = vk::RenderingFlagBits::eContentsSecondaryCommandBuffers,
					.renderArea = { { 0, 0 }, vk_extent },
					.layerCount = 1,
					.colorAttachmentCount = static_cast<std::uint32_t>(color_attachments.size()),
					.pColorAttachments = color_attachments.empty() ? nullptr : color_attachments.data(),
					.pDepthAttachment = depth_att ? &*depth_att : nullptr
				};
				command.beginRendering(ri);
				command.executeCommands(secondary);
				command.endRendering();
			} else {
				command.executeCommands(secondary);
			}

			if (profile_pass) {
				if (issue_stats) {
					command.endQuery(*slot.stats_pool, pass_index);
				}
				command.writeTimestamp2(vk::PipelineStageFlagBits2::eAllCommands, *slot.timestamp_pool, 2 + pass_index * 2);
			}
		}
	});

	if (timestamps_enabled && slot.pass_count > 0) {
		slot.results_valid = true;
	}

	const vk::ImageMemoryBarrier2 present_barrier{
		.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
		.dstStageMask = vk::PipelineStageFlagBits2::eBottomOfPipe,
		.dstAccessMask = {},
		.oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
		.newLayout = vk::ImageLayout::ePresentSrcKHR,
		.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
		.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
		.image = swap_image,
		.subresourceRange = {
			.aspectMask = vk::ImageAspectFlagBits::eColor,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1
		}
	};

	const vk::DependencyInfo end_dep{
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &present_barrier
	};
	command.pipelineBarrier2(end_dep);
}

auto gse::vulkan::to_vk_stage(const gpu::pipeline_stage s) -> vk::PipelineStageFlags2 {
	switch (s) {
		case gpu::pipeline_stage::vertex_shader:       return vk::PipelineStageFlagBits2::eVertexShader;
		case gpu::pipeline_stage::fragment_shader:     return vk::PipelineStageFlagBits2::eFragmentShader;
		case gpu::pipeline_stage::compute_shader:      return vk::PipelineStageFlagBits2::eComputeShader;
		case gpu::pipeline_stage::draw_indirect:       return vk::PipelineStageFlagBits2::eDrawIndirect;
		case gpu::pipeline_stage::late_fragment_tests: return vk::PipelineStageFlagBits2::eLateFragmentTests;
	}
	return vk::PipelineStageFlagBits2::eNone;
}

auto gse::vulkan::to_vk_barrier(const gpu::barrier_scope scope) -> vk::MemoryBarrier2 {
	using enum gpu::barrier_scope;
	switch (scope) {
		case compute_to_compute:
			return {
				.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
				.srcAccessMask = vk::AccessFlagBits2::eShaderStorageWrite,
				.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
				.dstAccessMask = vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite
			};
		case compute_to_indirect:
			return {
				.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
				.srcAccessMask = vk::AccessFlagBits2::eShaderStorageWrite,
				.dstStageMask = vk::PipelineStageFlagBits2::eDrawIndirect | vk::PipelineStageFlagBits2::eComputeShader,
				.dstAccessMask = vk::AccessFlagBits2::eIndirectCommandRead | vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite
			};
		case host_to_compute:
			return {
				.srcStageMask = vk::PipelineStageFlagBits2::eHost,
				.srcAccessMask = vk::AccessFlagBits2::eHostWrite,
				.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
				.dstAccessMask = vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite
			};
		case transfer_to_compute:
			return {
				.srcStageMask = vk::PipelineStageFlagBits2::eCopy,
				.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
				.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
				.dstAccessMask = vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite
			};
		case compute_to_transfer:
			return {
				.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
				.srcAccessMask = vk::AccessFlagBits2::eShaderStorageWrite,
				.dstStageMask = vk::PipelineStageFlagBits2::eCopy,
				.dstAccessMask = vk::AccessFlagBits2::eTransferRead
			};
		case transfer_to_host:
			return {
				.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
				.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
				.dstStageMask = vk::PipelineStageFlagBits2::eHost,
				.dstAccessMask = vk::AccessFlagBits2::eHostRead
			};
		case transfer_to_transfer:
			return {
				.srcStageMask = vk::PipelineStageFlagBits2::eCopy,
				.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
				.dstStageMask = vk::PipelineStageFlagBits2::eCopy,
				.dstAccessMask = vk::AccessFlagBits2::eTransferWrite
			};
	}
	return {};
}

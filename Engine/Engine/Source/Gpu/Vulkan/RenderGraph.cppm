export module gse.gpu:render_graph;

import std;
import vulkan;

import :types;
import :pipeline;
import :vulkan_buffer;
import :vulkan_device;
import :vulkan_commands;
import :vulkan_image;
import :vulkan_semaphore;
import :descriptor_heap;
import :device;
import :swap_chain;
import :frame;
import :bindless;

import gse.assert;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.math;

export namespace gse::vulkan {
	class render_graph;
	class recording_context;

	enum class load_op {
		clear_color,
		clear_depth,
		load,
		dont_care
	};

	struct color_output_info {
		bool is_swapchain = false;
		const image* custom_target = nullptr;
		load_op op = load_op::clear_color;
		gpu::color_clear clear_value;
	};

	struct depth_output_info {
		load_op op = load_op::clear_depth;
		gpu::depth_clear clear_value;
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
		gpu::pipeline_stage_flags stage;
		gpu::access_flags access;
	};

	auto sampled(
		const image& img,
		gpu::pipeline_stage_flags stage
	) -> resource_usage;

	auto storage(
		const buffer& buf,
		gpu::pipeline_stage_flags stage
	) -> resource_usage;

	auto storage_read(
		const buffer& buf,
		gpu::pipeline_stage_flags stage
	) -> resource_usage;

	auto transfer_read(
		const buffer& buf
	) -> resource_usage;

	auto transfer_write(
		const buffer& buf
	) -> resource_usage;

	auto indirect_read(
		const buffer& buf,
		gpu::pipeline_stage_flags stage
	) -> resource_usage;

	auto attachment(
		const image& img,
		gpu::pipeline_stage_flags stage
	) -> resource_usage;

	class recording_context {
	public:
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

		auto dispatch_indirect(
			const buffer& buf,
			std::size_t offset = 0
		) const -> void;

		auto end_rendering(
		) const -> void;

		template <typename T>
		auto push(
			const gpu::pipeline& p,
			const gpu::typed_push_constants<T>& typed
		) const -> void;

		auto draw_indirect(
			const buffer& buf,
			std::size_t offset,
			std::uint32_t draw_count,
			std::uint32_t stride
		) const -> void;

		auto draw_mesh_tasks_indirect(
			const buffer& buf,
			std::size_t offset,
			std::uint32_t draw_count,
			std::uint32_t stride
		) const -> void;

		auto bind(
			const gpu::pipeline& p
		) const -> void;

		auto bind_descriptors(
			const gpu::pipeline& p,
			const gpu::descriptor_region& region,
			std::uint32_t set_index = 0
		) const -> void;

		auto bind_vertex(
			const buffer& buf,
			std::size_t offset = 0
		) const -> void;

		auto bind_index(
			const buffer& buf,
			gpu::index_type type = gpu::index_type::uint32,
			std::size_t offset = 0
		) const -> void;

		auto set_viewport(
			vec2u extent
		) const -> void;

		auto set_scissor(
			vec2u extent
		) const -> void;

		auto commit(
			const gpu::descriptor_set_writer& writer,
			const gpu::pipeline& p,
			std::uint32_t set_index = 0
		) const -> void;

		auto copy_buffer(
			const buffer& src,
			const buffer& dst,
			std::size_t size,
			std::size_t src_offset = 0,
			std::size_t dst_offset = 0
		) const -> void;

		auto barrier(
			gpu::barrier_scope scope
		) const -> void;

		auto build_acceleration_structure(
			const gpu::acceleration_structure_build_geometry_info& build_info,
			std::span<const gpu::acceleration_structure_build_range_info* const> range_infos
		) const -> void;

		auto pipeline_barrier(
			const gpu::dependency_info& dep
		) const -> void;

		auto capture_swapchain(
			const gpu::swap_chain& swapchain,
			const gpu::frame& frame,
			const buffer& dst
		) const -> void;

		auto blit_swapchain_to_image(
			const gpu::swap_chain& swapchain,
			const gpu::frame& frame,
			const image& dst,
			vec2u dst_extent
		) const -> void;

		auto begin_rendering(
			vec2u extent,
			const image* depth = nullptr,
			gpu::image_layout depth_layout = gpu::image_layout::general,
			bool clear_depth = true,
			float clear_depth_value = 1.0f
		) const -> void;

		recording_context(
			recording_context&& other
		) noexcept;

		auto operator=(
			recording_context&& other
		) noexcept -> recording_context&;

		recording_context(
			const recording_context&
		) = delete;

		auto operator=(
			const recording_context&
		) -> recording_context& = delete;

		~recording_context();

	private:
		friend class render_graph;
		vulkan::commands m_cmd;
		std::span<const gpu::auto_bind_entry> m_auto_binds;
		recording_context(
			commands cmd,
			std::span<const gpu::auto_bind_entry> auto_binds
		);

		auto check_active(
		) const -> void;
	};

	struct render_pass_data {
		id pass_type{};
		gpu::queue_type queue = gpu::queue_type::graphics;
		std::vector<resource_usage> reads;
		std::vector<resource_usage> writes;
		std::vector<const buffer*> tracked_buffers;
		gpu::pipeline_stage_flags tracked_stage = gpu::pipeline_stage_flag::all_commands;
		std::vector<id> after_passes;
		std::optional<color_output_info> color_output;
		std::optional<depth_output_info> depth_output;
		std::coroutine_handle<> record_handle;
		std::optional<recording_context>* record_ctx_slot = nullptr;
	};

	class render_graph {
	public:
		explicit render_graph(
			gpu::device& device,
			gpu::swap_chain& swapchain,
			gpu::frame& frame,
			gpu::bindless_texture_set* bindless = nullptr
		);

		auto execute(
			std::vector<render_pass_data> passes
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

		[[nodiscard]] auto depth_image(
			this auto& self
		) -> auto&;

		[[nodiscard]] auto frame_in_progress(
		) const -> bool;

		[[nodiscard]] auto take_compute_submissions(
		) -> std::vector<gpu::queue_submission>;

		[[nodiscard]] auto take_graphics_extra_waits(
		) -> std::vector<gpu::semaphore_submit_info>;

	private:
		static constexpr std::uint32_t max_profiled_passes = 128;

		struct gpu_profile_slot {
			vk::raii::QueryPool timestamp_pool = nullptr;
			vk::raii::QueryPool stats_pool = nullptr;
			static_vector<id, max_profiled_passes> pass_types;
			static_vector<gpu::queue_type, max_profiled_passes> pass_queues;
			std::uint32_t pass_count = 0;
			bool stats_issued = false;
			time_t<std::uint64_t> cpu_ref{};
			std::uint64_t frame_counter = 0;
			bool results_valid = false;
		};

		auto ensure_profile_pools(
			gpu_profile_slot& slot,
			bool allow_stats
		) const -> void;

		auto read_profile_slot(
			gpu_profile_slot& slot
		) -> void;

		struct inheritance_storage {
			std::vector<vk::CommandBufferInheritanceRenderingInfo> rendering_inherits;
			std::vector<vk::CommandBufferInheritanceInfo> inherits;
			std::array<vk::Format, 1> color_formats{};
		};

		gpu::device* m_device;
		gpu::swap_chain* m_swapchain;
		gpu::frame* m_frame;
		gpu::bindless_texture_set* m_bindless = nullptr;
		std::array<per_frame_resource<gpu_profile_slot>, gpu::queue_type_count> m_profile_slots{
			per_frame_resource<gpu_profile_slot>{ gpu_profile_slot{}, gpu_profile_slot{} },
			per_frame_resource<gpu_profile_slot>{ gpu_profile_slot{}, gpu_profile_slot{} },
		};
		per_frame_resource<inheritance_storage> m_inheritance_storage{ inheritance_storage{}, inheritance_storage{} };
		std::vector<gpu::auto_bind_entry> m_auto_binds;
		std::atomic<bool> m_gpu_timestamps_enabled{ true };
		std::atomic<bool> m_gpu_pipeline_stats_enabled{ false };
		time_t<double> m_timestamp_period_per_tick = nanoseconds(1.0);
		std::uint64_t m_frames_submitted = 0;
		vulkan::semaphore m_compute_timeline;
		std::uint64_t m_compute_signal_counter = 0;
		std::vector<gpu::queue_submission> m_pending_compute_submissions;
		std::vector<gpu::semaphore_submit_info> m_pending_graphics_extra_waits;
	};
}

export namespace gse::gpu {
	auto storage_read(
		const vulkan::basic_buffer<vulkan::device>& buf,
		pipeline_stage stage
	) -> vulkan::resource_usage;

	auto storage_write(
		const vulkan::basic_buffer<vulkan::device>& buf,
		pipeline_stage stage
	) -> vulkan::resource_usage;

	auto transfer_read(
		const vulkan::basic_buffer<vulkan::device>& buf
	) -> vulkan::resource_usage;

	auto transfer_write(
		const vulkan::basic_buffer<vulkan::device>& buf
	) -> vulkan::resource_usage;

	auto sampled(
		const vulkan::basic_image<vulkan::device>& img,
		pipeline_stage stage
	) -> vulkan::resource_usage;

	auto indirect_read(
		const vulkan::basic_buffer<vulkan::device>& buf,
		pipeline_stage stage
	) -> vulkan::resource_usage;
}

gse::vulkan::recording_context::recording_context(const commands cmd, const std::span<const gpu::auto_bind_entry> auto_binds) : m_cmd(cmd), m_auto_binds(auto_binds) {
	if (m_cmd) {
		async::pass_recording_scope_push();
	}
}

gse::vulkan::recording_context::recording_context(recording_context&& other) noexcept : m_cmd(other.m_cmd), m_auto_binds(other.m_auto_binds) {
	other.m_cmd = commands{};
	other.m_auto_binds = {};
}

auto gse::vulkan::recording_context::operator=(recording_context&& other) noexcept -> recording_context& {
	if (this != &other) {
		if (m_cmd) {
			m_cmd.end();
			async::pass_recording_scope_pop();
		}
		m_cmd = other.m_cmd;
		m_auto_binds = other.m_auto_binds;
		other.m_cmd = commands{};
		other.m_auto_binds = {};
	}
	return *this;
}

gse::vulkan::recording_context::~recording_context() {
	if (m_cmd) {
		m_cmd.end();
		async::pass_recording_scope_pop();
	}
}

auto gse::vulkan::recording_context::check_active() const -> void {
	assert(async::pass_recording_scope_active() > 0, "recording_context method called outside an active pass; rec was captured by reference past its lifetime");
}

auto gse::vulkan::recording_context::copy_buffer(const buffer& src, const buffer& dst, const std::size_t size, const std::size_t src_offset, const std::size_t dst_offset) const -> void {
	check_active();
	m_cmd.copy_buffer(src.handle(), dst.handle(), gpu::buffer_copy_region{
		.src_offset = src_offset,
		.dst_offset = dst_offset,
		.size = size
	});
}

auto gse::vulkan::recording_context::barrier(const gpu::barrier_scope scope) const -> void {
	check_active();
	using ps = gpu::pipeline_stage_flag;
	using ac = gpu::access_flag;
	gpu::memory_barrier mb;
	switch (scope) {
		case gpu::barrier_scope::compute_to_compute:
			mb = {
				.src_stages = ps::compute_shader,
				.src_access = ac::shader_storage_write,
				.dst_stages = ps::compute_shader,
				.dst_access = ac::shader_storage_read | ac::shader_storage_write,
			};
			break;
		case gpu::barrier_scope::compute_to_indirect:
			mb = {
				.src_stages = ps::compute_shader,
				.src_access = ac::shader_storage_write,
				.dst_stages = ps::draw_indirect | ps::compute_shader,
				.dst_access = ac::indirect_command_read | ac::shader_storage_read | ac::shader_storage_write,
			};
			break;
		case gpu::barrier_scope::host_to_compute:
			mb = {
				.src_stages = ps::host,
				.src_access = ac::host_write,
				.dst_stages = ps::compute_shader,
				.dst_access = ac::shader_storage_read | ac::shader_storage_write,
			};
			break;
		case gpu::barrier_scope::transfer_to_compute:
			mb = {
				.src_stages = ps::copy,
				.src_access = ac::transfer_write,
				.dst_stages = ps::compute_shader,
				.dst_access = ac::shader_storage_read | ac::shader_storage_write,
			};
			break;
		case gpu::barrier_scope::compute_to_transfer:
			mb = {
				.src_stages = ps::compute_shader,
				.src_access = ac::shader_storage_write,
				.dst_stages = ps::copy,
				.dst_access = ac::transfer_read,
			};
			break;
		case gpu::barrier_scope::transfer_to_host:
			mb = {
				.src_stages = ps::transfer,
				.src_access = ac::transfer_write,
				.dst_stages = ps::host,
				.dst_access = ac::host_read,
			};
			break;
		case gpu::barrier_scope::transfer_to_transfer:
			mb = {
				.src_stages = ps::copy,
				.src_access = ac::transfer_write,
				.dst_stages = ps::copy,
				.dst_access = ac::transfer_write,
			};
			break;
	}
	const gpu::dependency_info dep{ .memory_barriers = std::span(&mb, 1) };
	m_cmd.pipeline_barrier(dep);
}

auto gse::vulkan::recording_context::build_acceleration_structure(
	const gpu::acceleration_structure_build_geometry_info& build_info,
	const std::span<const gpu::acceleration_structure_build_range_info* const> range_infos
) const -> void {
	check_active();
	m_cmd.build_acceleration_structures(build_info, range_infos);
}

auto gse::vulkan::recording_context::pipeline_barrier(const gpu::dependency_info& dep) const -> void {
	check_active();
	m_cmd.pipeline_barrier(dep);
}

auto gse::vulkan::recording_context::capture_swapchain(
	const gpu::swap_chain& swapchain,
	const gpu::frame& frame,
	const buffer& dst
) const -> void {
	check_active();
	const auto ext = swapchain.extent();
	const auto dst_buffer = dst.handle();
	const auto gpu_image = swapchain.image(frame.image_index());

	const gpu::image_barrier to_transfer{
		.src_stages = gpu::pipeline_stage_flag::color_attachment_output,
		.src_access = gpu::access_flag::color_attachment_write,
		.dst_stages = gpu::pipeline_stage_flag::transfer,
		.dst_access = gpu::access_flag::transfer_read,
		.old_layout = gpu::image_layout::color_attachment,
		.new_layout = gpu::image_layout::transfer_src,
		.image = gpu_image,
		.aspects = gpu::image_aspect_flag::color,
		.base_mip_level = 0,
		.level_count = 1,
		.base_array_layer = 0,
		.layer_count = 1,
	};
	m_cmd.pipeline_barrier(gpu::dependency_info{ .image_barriers = std::span(&to_transfer, 1) });

	const gpu::buffer_image_copy_region gpu_region{
		.buffer_offset = 0,
		.buffer_row_length = 0,
		.buffer_image_height = 0,
		.image_subresource = {
			.aspects = gpu::image_aspect_flag::color,
			.mip_level = 0,
			.base_array_layer = 0,
			.layer_count = 1,
		},
		.image_offset = vec3i{ 0, 0, 0 },
		.image_extent = vec3u{ ext.x(), ext.y(), 1 },
	};
	m_cmd.copy_image_to_buffer(gpu_image, gpu::image_layout::transfer_src, dst_buffer, std::span(&gpu_region, 1));

	const gpu::image_barrier back_to_color{
		.src_stages = gpu::pipeline_stage_flag::transfer,
		.src_access = gpu::access_flag::transfer_read,
		.dst_stages = gpu::pipeline_stage_flag::color_attachment_output,
		.dst_access = gpu::access_flag::color_attachment_write | gpu::access_flag::color_attachment_read,
		.old_layout = gpu::image_layout::transfer_src,
		.new_layout = gpu::image_layout::color_attachment,
		.image = gpu_image,
		.aspects = gpu::image_aspect_flag::color,
		.base_mip_level = 0,
		.level_count = 1,
		.base_array_layer = 0,
		.layer_count = 1,
	};
	m_cmd.pipeline_barrier(gpu::dependency_info{ .image_barriers = std::span(&back_to_color, 1) });
}

auto gse::vulkan::recording_context::blit_swapchain_to_image(const gpu::swap_chain& swapchain, const gpu::frame& frame, const image& dst, const vec2u dst_extent) const -> void {
	check_active();
	const auto src_image = swapchain.image(frame.image_index());
	const auto src_ext = swapchain.extent();

	const gpu::image_barrier src_to_transfer{
		.src_stages = gpu::pipeline_stage_flag::color_attachment_output,
		.src_access = gpu::access_flag::color_attachment_write,
		.dst_stages = gpu::pipeline_stage_flag::transfer,
		.dst_access = gpu::access_flag::transfer_read,
		.old_layout = gpu::image_layout::color_attachment,
		.new_layout = gpu::image_layout::transfer_src,
		.image = src_image,
		.aspects = gpu::image_aspect_flag::color,
		.base_mip_level = 0,
		.level_count = 1,
		.base_array_layer = 0,
		.layer_count = 1,
	};

	const gpu::image_barrier dst_to_transfer{
		.src_stages = {},
		.src_access = {},
		.dst_stages = gpu::pipeline_stage_flag::transfer,
		.dst_access = gpu::access_flag::transfer_write,
		.old_layout = gpu::image_layout::undefined,
		.new_layout = gpu::image_layout::transfer_dst,
		.image = dst.handle(),
		.aspects = gpu::image_aspect_flag::color,
		.base_mip_level = 0,
		.level_count = 1,
		.base_array_layer = 0,
		.layer_count = 1,
	};

	const std::array pre_barriers = { src_to_transfer, dst_to_transfer };
	m_cmd.pipeline_barrier(gpu::dependency_info{ .image_barriers = pre_barriers });

	const gpu::image_blit_region gpu_region{
		.src_subresource = {
			.aspects = gpu::image_aspect_flag::color,
			.mip_level = 0,
			.base_array_layer = 0,
			.layer_count = 1,
		},
		.src_offsets = {
			vec3i{ 0, 0, 0 },
			vec3i{ static_cast<int>(src_ext.x()), static_cast<int>(src_ext.y()), 1 },
		},
		.dst_subresource = {
			.aspects = gpu::image_aspect_flag::color,
			.mip_level = 0,
			.base_array_layer = 0,
			.layer_count = 1,
		},
		.dst_offsets = {
			vec3i{ 0, 0, 0 },
			vec3i{ static_cast<int>(dst_extent.x()), static_cast<int>(dst_extent.y()), 1 },
		},
	};
	m_cmd.blit_image(
		src_image,
		gpu::image_layout::transfer_src,
		dst.handle(),
		gpu::image_layout::transfer_dst,
		gpu_region,
		gpu::sampler_filter::nearest
	);

	const gpu::image_barrier src_back{
		.src_stages = gpu::pipeline_stage_flag::transfer,
		.src_access = gpu::access_flag::transfer_read,
		.dst_stages = gpu::pipeline_stage_flag::color_attachment_output,
		.dst_access = gpu::access_flag::color_attachment_write | gpu::access_flag::color_attachment_read,
		.old_layout = gpu::image_layout::transfer_src,
		.new_layout = gpu::image_layout::color_attachment,
		.image = src_image,
		.aspects = gpu::image_aspect_flag::color,
		.base_mip_level = 0,
		.level_count = 1,
		.base_array_layer = 0,
		.layer_count = 1,
	};

	const gpu::image_barrier dst_to_read{
		.src_stages = gpu::pipeline_stage_flag::transfer,
		.src_access = gpu::access_flag::transfer_write,
		.dst_stages = gpu::pipeline_stage_flag::compute_shader,
		.dst_access = gpu::access_flag::shader_sampled_read,
		.old_layout = gpu::image_layout::transfer_dst,
		.new_layout = gpu::image_layout::shader_read_only,
		.image = dst.handle(),
		.aspects = gpu::image_aspect_flag::color,
		.base_mip_level = 0,
		.level_count = 1,
		.base_array_layer = 0,
		.layer_count = 1,
	};

	const std::array post_barriers = { src_back, dst_to_read };
	m_cmd.pipeline_barrier(gpu::dependency_info{ .image_barriers = post_barriers });
}

auto gse::vulkan::recording_context::set_viewport(const float x, const float y, const float width, const float height, const float min_depth, const float max_depth) const -> void {
	check_active();
	m_cmd.set_viewport(gpu::viewport{
		.x = x,
		.y = y,
		.width = width,
		.height = height,
		.min_depth = min_depth,
		.max_depth = max_depth,
	});
}

auto gse::vulkan::recording_context::set_scissor(const std::int32_t x, const std::int32_t y, const std::uint32_t width, const std::uint32_t height) const -> void {
	check_active();
	const gse::rect_t<vec2i> sc{ {
		.min = vec2i{ x, y },
		.max = vec2i{ x + static_cast<int>(width), y + static_cast<int>(height) },
	} };
	m_cmd.set_scissor(sc);
}

auto gse::vulkan::recording_context::draw(const std::uint32_t vertex_count, const std::uint32_t instance_count, const std::uint32_t first_vertex, const std::uint32_t first_instance) const -> void {
	check_active();
	m_cmd.draw(vertex_count, instance_count, first_vertex, first_instance);
}

auto gse::vulkan::recording_context::draw_indexed(const std::uint32_t index_count, const std::uint32_t instance_count, const std::uint32_t first_index, const std::int32_t vertex_offset, const std::uint32_t first_instance) const -> void {
	check_active();
	m_cmd.draw_indexed(index_count, instance_count, first_index, vertex_offset, first_instance);
}

auto gse::vulkan::recording_context::draw_mesh_tasks(const std::uint32_t x, const std::uint32_t y, const std::uint32_t z) const -> void {
	check_active();
	m_cmd.draw_mesh_tasks(x, y, z);
}

auto gse::vulkan::recording_context::dispatch(const std::uint32_t x, const std::uint32_t y, const std::uint32_t z) const -> void {
	check_active();
	m_cmd.dispatch(x, y, z);
}

auto gse::vulkan::recording_context::dispatch_indirect(const buffer& buf, const std::size_t offset) const -> void {
	check_active();
	m_cmd.dispatch_indirect(buf.handle(), static_cast<gpu::device_size>(offset));
}

auto gse::vulkan::recording_context::end_rendering() const -> void {
	check_active();
	m_cmd.end_rendering();
}

template <typename T>
auto gse::vulkan::recording_context::push(const gpu::pipeline& p, const gpu::typed_push_constants<T>& typed) const -> void {
	check_active();
	typed.replay(m_cmd.native(), p.layout());
}

auto gse::vulkan::recording_context::draw_indirect(const buffer& buf, const std::size_t offset, const std::uint32_t draw_count, const std::uint32_t stride) const -> void {
	check_active();
	m_cmd.draw_indexed_indirect(buf.handle(), offset, draw_count, stride);
}

auto gse::vulkan::recording_context::draw_mesh_tasks_indirect(const buffer& buf, const std::size_t offset, const std::uint32_t draw_count, const std::uint32_t stride) const -> void {
	check_active();
	m_cmd.draw_mesh_tasks_indirect(buf.handle(), offset, draw_count, stride);
}

auto gse::vulkan::recording_context::bind(const gpu::pipeline& p) const -> void {
	check_active();
	m_cmd.bind_pipeline(p.bind_point(), p.handle());
	for (const auto set_idx : p.auto_bound_sets()) {
		for (const auto& [auto_set_idx, region] : m_auto_binds) {
			if (auto_set_idx == set_idx && region) {
				region.heap->bind(m_cmd.native(), p.bind_point(), p.layout(), set_idx, region);
				break;
			}
		}
	}
}

auto gse::vulkan::recording_context::bind_descriptors(const gpu::pipeline& p, const gpu::descriptor_region& region, const std::uint32_t set_index) const -> void {
	check_active();
	assert(region, "Cannot bind null descriptor region");
	region.heap->bind(m_cmd.native(), p.bind_point(), p.layout(), set_index, region);
}

auto gse::vulkan::recording_context::bind_vertex(const buffer& buf, const std::size_t offset) const -> void {
	check_active();
	const gpu::handle<buffer> buffers[]{ buf.handle() };
	const gpu::device_size offsets[]{ offset };
	m_cmd.bind_vertex_buffers(0, std::span<const gpu::handle<buffer>>(buffers), std::span<const gpu::device_size>(offsets));
}

auto gse::vulkan::recording_context::bind_index(const buffer& buf, const gpu::index_type type, const std::size_t offset) const -> void {
	check_active();
	m_cmd.bind_index_buffer_2(buf.handle(), offset, vk::WholeSize, type);
}

auto gse::vulkan::recording_context::set_viewport(const vec2u extent) const -> void {
	check_active();
	m_cmd.set_viewport(gpu::viewport{
		.x = 0.0f,
		.y = 0.0f,
		.width = static_cast<float>(extent.x()),
		.height = static_cast<float>(extent.y()),
		.min_depth = 0.0f,
		.max_depth = 1.0f,
	});
}

auto gse::vulkan::recording_context::set_scissor(const vec2u extent) const -> void {
	check_active();
	const gse::rect_t<vec2i> sc{ {
		.min = vec2i{ 0, 0 },
		.max = vec2i{ static_cast<int>(extent.x()), static_cast<int>(extent.y()) },
	} };
	m_cmd.set_scissor(sc);
}

auto gse::vulkan::recording_context::begin_rendering(const vec2u extent, const image* depth, const gpu::image_layout depth_layout, const bool clear_depth, const float clear_depth_value) const -> void {
	check_active();
	std::optional<vk::RenderingAttachmentInfo> depth_att;
	if (depth) {
		depth_att = vk::RenderingAttachmentInfo{
			.imageView = std::bit_cast<vk::ImageView>(depth->view()),
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
	m_cmd.begin_rendering(ri);
}

auto gse::vulkan::recording_context::commit(const gpu::descriptor_set_writer& writer, const gpu::pipeline& p, const std::uint32_t set_index) const -> void {
	check_active();
	writer.commit(
		m_cmd.native(),
		p.bind_point(),
		p.layout(),
		set_index
	);
}

namespace gse::vulkan {
	auto pipeline_stage_to_flags(
		gpu::pipeline_stage s
	) -> gpu::pipeline_stage_flags;
}

auto gse::vulkan::pipeline_stage_to_flags(const gpu::pipeline_stage s) -> gpu::pipeline_stage_flags {
	switch (s) {
		case gpu::pipeline_stage::vertex_shader:
			return gpu::pipeline_stage_flag::vertex_shader;
		case gpu::pipeline_stage::fragment_shader:
			return gpu::pipeline_stage_flag::fragment_shader;
		case gpu::pipeline_stage::compute_shader:
			return gpu::pipeline_stage_flag::compute_shader;
		case gpu::pipeline_stage::draw_indirect:
			return gpu::pipeline_stage_flag::draw_indirect;
		case gpu::pipeline_stage::late_fragment_tests:
			return gpu::pipeline_stage_flag::late_fragment_tests;
	}
	return {};
}

auto gse::vulkan::sampled(const image& img, const gpu::pipeline_stage_flags stage) -> resource_usage {
	return { { .ptr = std::addressof(img), .type = resource_type::image }, stage, gpu::access_flag::shader_sampled_read };
}

auto gse::vulkan::storage(const buffer& buf, const gpu::pipeline_stage_flags stage) -> resource_usage {
	return { { .ptr = std::addressof(buf), .type = resource_type::buffer }, stage, gpu::access_flag::shader_storage_read | gpu::access_flag::shader_storage_write };
}

auto gse::vulkan::storage_read(const buffer& buf, const gpu::pipeline_stage_flags stage) -> resource_usage {
	return { { .ptr = std::addressof(buf), .type = resource_type::buffer }, stage, gpu::access_flag::shader_storage_read };
}

auto gse::vulkan::transfer_read(const buffer& buf) -> resource_usage {
	return {
		{
			.ptr = std::addressof(buf),
			.type = resource_type::buffer,
		},
		gpu::pipeline_stage_flag::copy,
		gpu::access_flag::transfer_read,
	};
}

auto gse::vulkan::transfer_write(const buffer& buf) -> resource_usage {
	return {
		{
			.ptr = std::addressof(buf),
			.type = resource_type::buffer,
		},
		gpu::pipeline_stage_flag::copy,
		gpu::access_flag::transfer_write,
	};
}

auto gse::vulkan::indirect_read(const buffer& buf, const gpu::pipeline_stage_flags stage) -> resource_usage {
	return { { .ptr = std::addressof(buf), .type = resource_type::buffer }, stage, gpu::access_flag::indirect_command_read };
}

auto gse::vulkan::attachment(const image& img, const gpu::pipeline_stage_flags stage) -> resource_usage {
	const bool is_depth = stage.test(gpu::pipeline_stage_flag::late_fragment_tests) || stage.test(gpu::pipeline_stage_flag::early_fragment_tests);
	const auto access = is_depth
		? gpu::access_flags{ gpu::access_flag::depth_stencil_attachment_write }
		: gpu::access_flags{ gpu::access_flag::color_attachment_write };
	return { { .ptr = std::addressof(img), .type = resource_type::image }, stage, access };
}

auto gse::gpu::storage_read(const vulkan::basic_buffer<vulkan::device>& buf, const pipeline_stage stage) -> vulkan::resource_usage {
	return vulkan::storage_read(buf, vulkan::pipeline_stage_to_flags(stage));
}

auto gse::gpu::storage_write(const vulkan::basic_buffer<vulkan::device>& buf, const pipeline_stage stage) -> vulkan::resource_usage {
	return vulkan::storage(buf, vulkan::pipeline_stage_to_flags(stage));
}

auto gse::gpu::transfer_read(const vulkan::basic_buffer<vulkan::device>& buf) -> vulkan::resource_usage {
	return vulkan::transfer_read(buf);
}

auto gse::gpu::transfer_write(const vulkan::basic_buffer<vulkan::device>& buf) -> vulkan::resource_usage {
	return vulkan::transfer_write(buf);
}

auto gse::gpu::sampled(const vulkan::basic_image<vulkan::device>& img, const pipeline_stage stage) -> vulkan::resource_usage {
	return vulkan::sampled(img, vulkan::pipeline_stage_to_flags(stage));
}

auto gse::gpu::indirect_read(const vulkan::basic_buffer<vulkan::device>& buf, const pipeline_stage stage) -> vulkan::resource_usage {
	return vulkan::indirect_read(buf, vulkan::pipeline_stage_to_flags(stage));
}

namespace gse::vulkan {
	constexpr auto profile_stats_flags =
		vk::QueryPipelineStatisticFlagBits::eInputAssemblyVertices
		| vk::QueryPipelineStatisticFlagBits::eInputAssemblyPrimitives
		| vk::QueryPipelineStatisticFlagBits::eClippingInvocations
		| vk::QueryPipelineStatisticFlagBits::eFragmentShaderInvocations;
}

gse::vulkan::render_graph::render_graph(gpu::device& device, gpu::swap_chain& swapchain, gpu::frame& frame, gpu::bindless_texture_set* bindless) : m_device(std::addressof(device)), m_swapchain(std::addressof(swapchain)), m_frame(std::addressof(frame)), m_bindless(bindless) {
	m_timestamp_period_per_tick = nanoseconds(static_cast<double>(device.timestamp_period()));
	m_compute_timeline = vulkan::semaphore::create_timeline(device.vulkan_device(), 0);
}

auto gse::vulkan::render_graph::take_compute_submissions() -> std::vector<gpu::queue_submission> {
	return std::move(m_pending_compute_submissions);
}

auto gse::vulkan::render_graph::take_graphics_extra_waits() -> std::vector<gpu::semaphore_submit_info> {
	return std::move(m_pending_graphics_extra_waits);
}

auto gse::vulkan::render_graph::set_gpu_timestamps_enabled(const bool enabled) -> void {
	m_gpu_timestamps_enabled.store(enabled, std::memory_order_relaxed);
}

auto gse::vulkan::render_graph::set_gpu_pipeline_stats_enabled(const bool enabled) -> void {
	m_gpu_pipeline_stats_enabled.store(enabled, std::memory_order_relaxed);
}

auto gse::vulkan::render_graph::ensure_profile_pools(gpu_profile_slot& slot, const bool allow_stats) const -> void {
	if (!*slot.timestamp_pool) {
		slot.timestamp_pool = m_device->vulkan_device().raii_device().createQueryPool({
			.queryType = vk::QueryType::eTimestamp,
			.queryCount = max_profiled_passes * 2 + 1,
		});
	}
	if (allow_stats && !*slot.stats_pool) {
		slot.stats_pool = m_device->vulkan_device().raii_device().createQueryPool({
			.queryType = vk::QueryType::ePipelineStatistics,
			.queryCount = max_profiled_passes,
			.pipelineStatistics = profile_stats_flags,
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

	for (std::uint32_t i = 0; i < slot.pass_count; ++i) {
		const auto start = static_cast<double>(timestamps[1 + i * 2]) * period + offset;
		const auto end = static_cast<double>(timestamps[2 + i * 2]) * period + offset;
		const auto gpu_id = slot.pass_types[i];
		const auto queue = slot.pass_queues[i];
		const std::uint64_t key = (slot.frame_counter << 16) | (static_cast<std::uint64_t>(queue) << 14) | i;
		const auto tid = (queue == gpu::queue_type::compute)
			? trace::gpu_compute_virtual_tid
			: trace::gpu_virtual_tid;

		trace::begin_async_at(gpu_id, key, tid, time_t<std::uint64_t>(start));
		trace::end_async_at(gpu_id, key, tid, time_t<std::uint64_t>(end));

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
				const auto pass_name = std::string(slot.pass_types[i].tag());
				for (std::uint32_t s = 0; s < stats_per_pass; ++s) {
					const auto stat_id = find_or_generate_id(pass_name + labels[s]);
					trace::counter_at(stat_id, static_cast<double>(stats[i * stats_per_pass + s]), trace::gpu_stats_virtual_tid, time_t<std::uint64_t>(start));
				}
			}
		}
	}

	slot.results_valid = false;
}

auto gse::vulkan::render_graph::current_frame() const -> std::uint32_t {
	return m_frame->current_frame();
}

auto gse::vulkan::render_graph::extent() const -> vec2u {
	return m_swapchain->extent();
}

auto gse::vulkan::render_graph::depth_image(this auto& self) -> auto& {
	return self.m_swapchain->depth_image();
}

auto gse::vulkan::render_graph::frame_in_progress() const -> bool {
	return m_frame->frame_in_progress();
}

auto gse::vulkan::render_graph::execute(std::vector<render_pass_data> passes) -> void {
	m_pending_compute_submissions.clear();
	m_pending_graphics_extra_waits.clear();

	if (!m_frame->frame_in_progress()) {
		return;
	}

	m_auto_binds.clear();
	if (m_bindless) {
		constexpr auto bindless_idx = static_cast<std::uint32_t>(gpu::descriptor_set_type::bind_less);
		m_auto_binds.push_back({ .set_index = bindless_idx, .region = m_bindless->region() });
	}

	const auto frame_idx = m_frame->current_frame();
	const auto graphics_primary_handle = m_frame->command_buffer(gpu::queue_type::graphics);
	const auto compute_primary_handle = m_frame->command_buffer(gpu::queue_type::compute);
	const auto graphics_primary = std::bit_cast<vk::CommandBuffer>(graphics_primary_handle);
	const auto compute_primary = std::bit_cast<vk::CommandBuffer>(compute_primary_handle);
	const bool compute_distinct = m_device->vulkan_device().families_distinct();

	const auto image_index = m_frame->image_index();
	const auto swap_extent = m_swapchain->extent();
	const vk::Extent2D vk_extent{ swap_extent.x(), swap_extent.y() };

	auto& graphics_slot = m_profile_slots[static_cast<std::size_t>(gpu::queue_type::graphics)][frame_idx];
	auto& compute_slot = m_profile_slots[static_cast<std::size_t>(gpu::queue_type::compute)][frame_idx];
	const bool timestamps_enabled = m_gpu_timestamps_enabled.load(std::memory_order_relaxed);
	const bool stats_enabled = m_gpu_pipeline_stats_enabled.load(std::memory_order_relaxed);

	if (m_frames_submitted >= per_frame_resource<gpu_profile_slot>::frames_in_flight) {
		read_profile_slot(graphics_slot);
		read_profile_slot(compute_slot);
	}

	auto bump_frames = make_scope_exit([this] {
		++m_frames_submitted;
	});

	auto reset_slot = [](gpu_profile_slot& s) {
		s.pass_types.clear();
		s.pass_queues.clear();
		s.pass_count = 0;
		s.stats_issued = false;
		s.results_valid = false;
	};
	reset_slot(graphics_slot);
	reset_slot(compute_slot);

	bool has_compute_work = false;
	for (const auto& p : passes) {
		if (p.queue == gpu::queue_type::compute && compute_distinct) {
			has_compute_work = true;
			break;
		}
	}

	auto open_primary = [this](const gpu::handle<command_buffer> handle) {
		const vulkan::commands cmd(handle);
		cmd.reset();
		cmd.begin();
		m_device->descriptor_heap().bind_descriptor_storage(handle);
	};

	if (has_compute_work) {
		open_primary(compute_primary_handle);
	}

	auto setup_timestamps = [&](gpu_profile_slot& s, vk::CommandBuffer cb, const bool with_stats) {
		ensure_profile_pools(s, with_stats);
		cb.resetQueryPool(*s.timestamp_pool, 0, max_profiled_passes * 2 + 1);
		if (with_stats) {
			cb.resetQueryPool(*s.stats_pool, 0, max_profiled_passes);
		}
		s.cpu_ref = system_clock::now<trace::tick_step>();
		s.frame_counter = m_frames_submitted;
		cb.writeTimestamp2(vk::PipelineStageFlagBits2::eAllCommands, *s.timestamp_pool, 0);
	};

	if (timestamps_enabled) {
		setup_timestamps(graphics_slot, graphics_primary, stats_enabled);
		if (has_compute_work) {
			setup_timestamps(compute_slot, compute_primary, false);
		}
	}

	std::vector<std::size_t> sorted;

	{
		trace::scope_guard sg{gse::trace_id<"graph::plan">()};
		std::unordered_map<id, std::size_t> type_to_index;
		for (std::size_t i = 0; i < passes.size(); ++i) {
			type_to_index[passes[i].pass_type] = i;
		}

		std::vector<std::vector<std::size_t>> adj(passes.size());
		std::vector<std::size_t> in_degree(passes.size(), 0);

		auto add_edge = [&](const std::size_t from, const std::size_t to) {
			for (const auto n : adj[from]) {
				if (n == to) {
					return;
				}
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
						if (w.resource.ptr && r.resource.ptr && w.resource.ptr == r.resource.ptr) {
							i_writes_j_reads = true;
						}
					}
				}
				for (const auto& w : passes[j].writes) {
					for (const auto& r : passes[i].reads) {
						if (w.resource.ptr && r.resource.ptr && w.resource.ptr == r.resource.ptr) {
							j_writes_i_reads = true;
						}
					}
				}

				if (i_writes_j_reads) {
					add_edge(i, j);
				}
				else if (j_writes_i_reads) {
					add_edge(j, i);
				}
			}
		}

		sorted.reserve(passes.size());

		std::queue<std::size_t> queue;
		for (std::size_t i = 0; i < passes.size(); ++i) {
			if (in_degree[i] == 0) {
				queue.push(i);
			}
		}

		while (!queue.empty()) {
			auto front = queue.front();
			queue.pop();
			sorted.push_back(front);
			for (auto next : adj[front]) {
				if (--in_degree[next] == 0) {
					queue.push(next);
				}
			}
		}
	}

	auto pass_queue = [&](const std::size_t pi) -> gpu::queue_type {
		return (compute_distinct && passes[pi].queue == gpu::queue_type::compute)
			? gpu::queue_type::compute
			: gpu::queue_type::graphics;
	};

	bool graphics_waits_on_compute = false;
	bool compute_waits_on_graphics = false;
	for (std::size_t i = 0; i < passes.size() && (!graphics_waits_on_compute || !compute_waits_on_graphics); ++i) {
		for (std::size_t j = 0; j < passes.size(); ++j) {
			if (i == j) continue;
			const auto qi = pass_queue(i);
			const auto qj = pass_queue(j);
			if (qi == qj) continue;
			for (const auto& w : passes[i].writes) {
				for (const auto& r : passes[j].reads) {
					if (w.resource.ptr && r.resource.ptr && w.resource.ptr == r.resource.ptr) {
						if (qi == gpu::queue_type::compute && qj == gpu::queue_type::graphics) {
							graphics_waits_on_compute = true;
						}
						else if (qi == gpu::queue_type::graphics && qj == gpu::queue_type::compute) {
							compute_waits_on_graphics = true;
						}
					}
				}
			}
		}
	}

	assert(!compute_waits_on_graphics, "render_graph: graphics->compute cross-queue handoff not supported in one-shot-per-queue submission; split a submission or move the pass");

	auto& worker_pools = m_device->worker_command_pools();
	worker_pools.reset_frame(frame_idx);
	const auto color_format = m_swapchain->format();

	std::vector<std::size_t> pass_to_serial(passes.size());
	for (std::size_t si = 0; si < sorted.size(); ++si) {
		pass_to_serial[sorted[si]] = si;
	}

	std::vector<vk::CommandBuffer> pass_secondaries(passes.size());

	auto& inheritance = m_inheritance_storage[frame_idx];
	inheritance.color_formats = { to_vk(color_format) };
	inheritance.rendering_inherits.assign(passes.size(), vk::CommandBufferInheritanceRenderingInfo{});
	inheritance.inherits.assign(passes.size(), vk::CommandBufferInheritanceInfo{});

	task::parallel_invoke_range(0, passes.size(), [&](std::size_t pi) {
		auto& pass = passes[pi];
		const auto queue = pass_queue(pi);
		const bool is_graphics_pass = pass.color_output || pass.depth_output;
		const std::size_t si = pass_to_serial[pi];
		const bool profile_pass = timestamps_enabled && si < max_profiled_passes;
		const bool issue_stats = profile_pass && stats_enabled && is_graphics_pass;

		const auto worker_idx = task::current_worker();
		assert(worker_idx.has_value(), "graph::record_parallel: thread has no arena slot");
		const auto secondary = worker_pools.acquire_secondary(queue, *worker_idx, frame_idx);

		inheritance.rendering_inherits[pi] = vk::CommandBufferInheritanceRenderingInfo{
			.viewMask = 0,
			.colorAttachmentCount = pass.color_output ? 1u : 0u,
			.pColorAttachmentFormats = pass.color_output ? inheritance.color_formats.data() : nullptr,
			.depthAttachmentFormat = pass.depth_output ? vk::Format::eD32Sfloat : vk::Format::eUndefined,
			.stencilAttachmentFormat = vk::Format::eUndefined,
			.rasterizationSamples = vk::SampleCountFlagBits::e1,
		};

		auto& inherit = inheritance.inherits[pi];
		vk::CommandBufferUsageFlags begin_flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
		if (is_graphics_pass) {
			inherit.pNext = &inheritance.rendering_inherits[pi];
			begin_flags |= vk::CommandBufferUsageFlagBits::eRenderPassContinue;
		}
		if (issue_stats) {
			inherit.pipelineStatistics = profile_stats_flags;
		}

		secondary.begin({
			.flags = begin_flags,
			.pInheritanceInfo = &inherit
		});
		m_device->descriptor_heap().bind_descriptor_storage(std::bit_cast<gpu::handle<command_buffer>>(secondary));
		*pass.record_ctx_slot = recording_context{ commands{ std::bit_cast<gpu::handle<command_buffer>>(secondary) }, m_auto_binds };
		pass.record_handle.resume();

		pass_secondaries[pi] = secondary;
	});

	{
		trace::scope_guard sg{gse::trace_id<"graph::record_replay">()};

		auto append_tracked_barriers = [](const render_pass_data& p, std::vector<gpu::memory_barrier>& out) {
			if (p.tracked_buffers.empty()) {
				return;
			}
			gpu::pipeline_stage_flags tracked_stage{};
			gpu::access_flags tracked_access{};
			bool has_unmatched_tracked_buffer = false;

			for (const auto* tracked : p.tracked_buffers) {
				bool matched = false;
				for (const auto& [resource, stage, access] : p.reads) {
					if (resource.ptr == tracked && resource.type == resource_type::buffer) {
						tracked_stage |= stage;
						tracked_access |= access;
						matched = true;
					}
				}
				for (const auto& [resource, stage, access] : p.writes) {
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

			if (has_unmatched_tracked_buffer || !tracked_stage) {
				tracked_stage |= p.tracked_stage;
				tracked_access |= gpu::access_flag::shader_storage_read
					| gpu::access_flag::shader_storage_write
					| gpu::access_flag::uniform_read
					| gpu::access_flag::transfer_read
					| gpu::access_flag::indirect_command_read
					| gpu::access_flag::vertex_attribute_read
					| gpu::access_flag::index_read;
			}

			out.push_back({
				.src_stages = gpu::pipeline_stage_flag::host,
				.src_access = gpu::access_flag::host_write,
				.dst_stages = tracked_stage,
				.dst_access = tracked_access,
			});
		};

		auto append_prev_pass_barriers = [&](const render_pass_data& cur, const gpu::queue_type cur_queue, const std::size_t serial_index, std::vector<gpu::memory_barrier>& out) {
			for (std::size_t pi = 0; pi < serial_index; ++pi) {
				const auto& prev = passes[sorted[pi]];
				const auto prev_queue = pass_queue(sorted[pi]);
				if (prev_queue != cur_queue) {
					continue;
				}
				for (const auto& [prev_resource, prev_stage, prev_access] : prev.writes) {
					if (!prev_resource.ptr) {
						continue;
					}
					for (const auto& [read_resource, read_stage, read_access] : cur.reads) {
						if (read_resource.ptr && prev_resource.ptr == read_resource.ptr) {
							out.push_back({
								.src_stages = prev_stage,
								.src_access = prev_access,
								.dst_stages = read_stage,
								.dst_access = read_access,
							});
						}
					}
					for (const auto& [cur_resource, cur_stage, cur_access] : cur.writes) {
						if (cur_resource.ptr && prev_resource.ptr == cur_resource.ptr) {
							out.push_back({
								.src_stages = prev_stage,
								.src_access = prev_access,
								.dst_stages = cur_stage,
								.dst_access = cur_access,
							});
						}
					}
				}
			}
		};

		for (std::size_t si = 0; si < sorted.size(); ++si) {
			const auto pass_idx = sorted[si];
			auto& pass = passes[pass_idx];
			const auto queue = pass_queue(pass_idx);
			const auto target_handle = (queue == gpu::queue_type::compute) ? compute_primary_handle : graphics_primary_handle;
			const auto target_primary = (queue == gpu::queue_type::compute) ? compute_primary : graphics_primary;
			auto& target_slot = (queue == gpu::queue_type::compute) ? compute_slot : graphics_slot;

			std::vector<gpu::memory_barrier> barriers;

			append_tracked_barriers(pass, barriers);
			append_prev_pass_barriers(pass, queue, si, barriers);

			{
				std::vector<gpu::memory_barrier> coalesced;
				coalesced.reserve(barriers.size());
				for (const auto& b : barriers) {
					bool merged = false;
					for (auto& o : coalesced) {
						if (o.src_stages.bits() == b.src_stages.bits() && o.dst_stages.bits() == b.dst_stages.bits()) {
							o.src_access |= b.src_access;
							o.dst_access |= b.dst_access;
							merged = true;
							break;
						}
					}
					if (!merged) {
						coalesced.push_back(b);
					}
				}
				barriers = std::move(coalesced);
			}

			std::vector<gpu::image_barrier> image_barriers;

			const bool profile_pass = timestamps_enabled && target_slot.pass_count < max_profiled_passes;
			const std::uint32_t pass_index = target_slot.pass_count;
			const bool is_graphics_pass = pass.color_output || pass.depth_output;
			const bool issue_stats = profile_pass && stats_enabled && is_graphics_pass && queue == gpu::queue_type::graphics;

			const auto marker_handle = m_device->begin_pass_marker(target_handle, {
				.frame_counter = m_frames_submitted,
				.pass_index = static_cast<std::uint32_t>(si),
				.pass_type = pass.pass_type,
			});

			if (!barriers.empty()) {
				vulkan::commands(target_handle).pipeline_barrier(gpu::dependency_info{
					.memory_barriers = barriers,
				});
			}
			if (!image_barriers.empty()) {
				vulkan::commands(target_handle).pipeline_barrier(gpu::dependency_info{
					.image_barriers = image_barriers,
				});
			}

			m_device->checkpoint_pass_marker(target_handle, marker_handle);

			if (profile_pass) {
				target_primary.writeTimestamp2(vk::PipelineStageFlagBits2::eNone, *target_slot.timestamp_pool, 1 + pass_index * 2);
				target_slot.pass_types.push_back(pass.pass_type);
				target_slot.pass_queues.push_back(queue);
				++target_slot.pass_count;
				if (issue_stats) {
					target_primary.beginQuery(*target_slot.stats_pool, pass_index, {});
					target_slot.stats_issued = true;
				}
			}

			const auto secondary = pass_secondaries[sorted[si]];

			if (is_graphics_pass) {
				std::vector<vk::RenderingAttachmentInfo> color_attachments;
				std::optional<vk::RenderingAttachmentInfo> depth_att;

				if (pass.color_output) {
					const auto& info = *pass.color_output;
					const auto op = info.op;
					const auto& clear_value = info.clear_value;
					auto vk_load = vk::AttachmentLoadOp::eDontCare;
					vk::ClearValue clear_val{};

					if (op == load_op::clear_color) {
						vk_load = vk::AttachmentLoadOp::eClear;
						clear_val.color = vk::ClearColorValue{
							.float32 = std::array{ clear_value.r, clear_value.g, clear_value.b, clear_value.a }
						};
					}
					else if (op == load_op::load) {
						vk_load = vk::AttachmentLoadOp::eLoad;
					}

					color_attachments.push_back({
						.imageView = std::bit_cast<vk::ImageView>(m_swapchain->image_view(image_index)),
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
					}
					else if (op == load_op::load) {
						vk_load = vk::AttachmentLoadOp::eLoad;
					}

					depth_att = vk::RenderingAttachmentInfo{
						.imageView = std::bit_cast<vk::ImageView>(m_swapchain->depth_image().view()),
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
				target_primary.beginRendering(ri);
				target_primary.executeCommands(secondary);
				target_primary.endRendering();
			}
			else {
				target_primary.executeCommands(secondary);
			}

			m_device->post_renderpass_pass_marker(target_handle, marker_handle);

			if (profile_pass) {
				if (issue_stats) {
					target_primary.endQuery(*target_slot.stats_pool, pass_index);
				}
				target_primary.writeTimestamp2(vk::PipelineStageFlagBits2::eAllCommands, *target_slot.timestamp_pool, 2 + pass_index * 2);
			}

			m_device->end_pass_marker(target_handle, marker_handle);
		}
	}

	if (timestamps_enabled && graphics_slot.pass_count > 0) {
		graphics_slot.results_valid = true;
	}
	if (timestamps_enabled && compute_slot.pass_count > 0) {
		compute_slot.results_valid = true;
	}

	if (has_compute_work) {
		vulkan::commands(compute_primary_handle).end();

		const std::uint64_t signal_value = ++m_compute_signal_counter;

		gpu::queue_submission sub;
		sub.command_buffer = compute_primary_handle;
		sub.signals.push_back({
			.semaphore = m_compute_timeline.handle(),
			.value = signal_value,
			.stages = gpu::pipeline_stage_flag::compute_shader,
		});
		m_pending_compute_submissions.push_back(std::move(sub));

		if (graphics_waits_on_compute) {
			m_pending_graphics_extra_waits.push_back({
				.semaphore = m_compute_timeline.handle(),
				.value = signal_value,
				.stages = gpu::pipeline_stage_flag::all_commands,
			});
		}
	}
}


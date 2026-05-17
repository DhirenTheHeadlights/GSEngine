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
import :descriptors;
import :device;
import :swap_chain;
import :frame;
import :bindless;
import :transient_pool;

import gse.assert;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.math;
import gse.meta;

export namespace gse::gpu {
	class render_graph;
	class recording_context;
	struct render_pass_data;

	using pass_body_fn = std::shared_ptr<move_only_function<void(recording_context&)>>;

	struct color_output_info {
		bool is_swapchain = false;
		const image* custom_target = nullptr;
		transient_image_handle transient_target;
		load_op op = load_op::clear;
		gpu::color_clear clear_value;
	};

	struct depth_output_info {
		bool is_swapchain = false;
		const image* custom_target = nullptr;
		transient_image_handle transient_target;
		load_op op = load_op::clear;
		gpu::depth_clear clear_value;
	};

	struct resource_usage {
		resource_ref resource;
		gpu::pipeline_stage_flags stage;
		gpu::access_flags access;
	};

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
		) -> void;

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
		) -> void;

		auto draw_mesh_tasks_indirect(
			const buffer& buf,
			std::size_t offset,
			std::uint32_t draw_count,
			std::uint32_t stride
		) -> void;

		auto bind(
			const gpu::pipeline& p
		) const -> void;

		auto bind_descriptors(
			const gpu::pipeline& p,
			const gpu::descriptor_region& region,
			std::uint32_t set_index = 0
		) -> void;

		auto bind_vertex(
			const buffer& buf,
			std::size_t offset = 0
		) -> void;

		auto bind_index(
			const buffer& buf,
			gpu::index_type type = gpu::index_type::uint32,
			std::size_t offset = 0
		) -> void;

		auto set_viewport(
			vec2u extent
		) const -> void;

		auto set_scissor(
			vec2u extent
		) const -> void;

		auto commit(
			const gpu::descriptor_writer& writer,
			const gpu::pipeline& p,
			std::uint32_t set_index = 0
		) -> void;

		auto copy_buffer(
			const buffer& src,
			const buffer& dst,
			std::size_t size,
			std::size_t src_offset = 0,
			std::size_t dst_offset = 0
		) -> void;

		auto fill_buffer(
			const buffer& dst,
			std::size_t offset,
			std::size_t size,
			std::uint32_t data = 0
		) -> void;

		auto barrier(
			gpu::barrier_scope scope
		) const -> void;

		auto sample_image(
			const image& img,
			gpu::pipeline_stage_flags stages
		) -> void;

		auto build_acceleration_structure(
			const gpu::acceleration_structure_build_geometry_info& build_info,
			std::span<const gpu::acceleration_structure_build_range_info* const> range_infos
		) -> void;

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

		[[nodiscard]] auto resolve(
			transient_image_handle h
		) const -> const image&;

		[[nodiscard]] auto resolve(
			transient_buffer_handle h
		) const -> const buffer&;

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

		struct touched_resource {
			resource_ref ref;
			gpu::pipeline_stage_flags stages = {};
			gpu::access_flags access = {};
		};

		vulkan::commands m_cmd;
		std::span<const gpu::auto_bind_entry> m_auto_binds;
		render_pass_data* m_pass = nullptr;
		const gpu::transient_pool* m_transient_pool = nullptr;
		std::vector<touched_resource> m_touched;

		recording_context(
			commands cmd,
			std::span<const gpu::auto_bind_entry> auto_binds,
			render_pass_data* pass,
			const gpu::transient_pool* transient_pool
		);

		auto check_active(
		) const -> void;

		auto note_touched(
			resource_ref ref,
			gpu::pipeline_stage_flags stages,
			gpu::access_flags access
		) -> void;

		auto finalize_pass(
		) -> void;
	};

	struct render_pass_data {
		id pass_type{};
		gpu::queue_type queue = gpu::queue_type::graphics;
		const gpu::pipeline* primary_pipeline = nullptr;
		std::vector<resource_usage> reads;
		std::vector<resource_usage> writes;
		std::vector<id> after_passes;
		id chain_id;
		std::optional<color_output_info> color_output;
		std::optional<depth_output_info> depth_output;
		std::coroutine_handle<> record_handle;
		std::optional<recording_context>* record_ctx_slot = nullptr;
		gpu::pass_body_fn body;
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
			std::vector<render_pass_data> passes,
			std::vector<transient_image_request> transient_images = {},
			std::vector<transient_buffer_request> transient_buffers = {}
		) -> void;

		[[nodiscard]] auto transient_pool(
			this auto& self
		) -> auto&;

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

		[[nodiscard]] auto take_aux_submissions(
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
			std::vector<std::array<vk::Format, 1>> color_formats;
		};

		struct queue_state {
			vulkan::semaphore timeline;
			std::uint64_t signal_counter = 0;
		};

		gpu::device* m_device;
		gpu::swap_chain* m_swapchain;
		gpu::frame* m_frame;
		gpu::bindless_texture_set* m_bindless = nullptr;
		gpu::transient_pool m_transient_pool;
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
		std::array<queue_state, gpu::queue_type_count> m_queue_states;
		std::vector<gpu::queue_submission> m_pending_aux_submissions;
		std::vector<gpu::semaphore_submit_info> m_pending_graphics_extra_waits;
	};
}

gse::gpu::recording_context::recording_context(const commands cmd, const std::span<const gpu::auto_bind_entry> auto_binds, render_pass_data* pass, const gpu::transient_pool* transient_pool) : m_cmd(cmd), m_auto_binds(auto_binds), m_pass(pass), m_transient_pool(transient_pool) {
	if (m_cmd) {
		async::pass_recording_scope_push();
	}
}

namespace gse::gpu {
	auto barrier_access(
		descriptor_type type,
		descriptor_access access
	) -> access_flags;

	auto format_bound_slots(
		std::span<const resource_slot> resources
	) -> std::string;
}

gse::gpu::recording_context::recording_context(recording_context&& other) noexcept : m_cmd(other.m_cmd), m_auto_binds(other.m_auto_binds), m_pass(other.m_pass), m_transient_pool(other.m_transient_pool), m_touched(std::move(other.m_touched)) {
	other.m_cmd = commands{};
	other.m_auto_binds = {};
	other.m_pass = nullptr;
	other.m_transient_pool = nullptr;
}

auto gse::gpu::recording_context::operator=(recording_context&& other) noexcept -> recording_context& {
	if (this != &other) {
		if (m_cmd) {
			finalize_pass();
			m_cmd.end();
			async::pass_recording_scope_pop();
		}
		m_cmd = other.m_cmd;
		m_auto_binds = other.m_auto_binds;
		m_pass = other.m_pass;
		m_transient_pool = other.m_transient_pool;
		m_touched = std::move(other.m_touched);
		other.m_cmd = commands{};
		other.m_auto_binds = {};
		other.m_pass = nullptr;
		other.m_transient_pool = nullptr;
	}
	return *this;
}

auto gse::gpu::recording_context::resolve(const transient_image_handle h) const -> const image& {
	assert(m_transient_pool != nullptr, "recording_context::resolve called but no transient_pool is bound");
	const auto* img = m_transient_pool->resolve_image(h);
	assert(img != nullptr, "transient_image_handle {} could not be resolved; ensure it was declared via gpu::transient_image before this pass runs", h.key);
	return *img;
}

auto gse::gpu::recording_context::resolve(const transient_buffer_handle h) const -> const buffer& {
	assert(m_transient_pool != nullptr, "recording_context::resolve called but no transient_pool is bound");
	const auto* buf = m_transient_pool->resolve_buffer(h);
	assert(buf != nullptr, "transient_buffer_handle {} could not be resolved; ensure it was declared via gpu::transient_buffer before this pass runs", h.key);
	return *buf;
}

gse::gpu::recording_context::~recording_context() {
	if (m_cmd) {
		finalize_pass();
		m_cmd.end();
		async::pass_recording_scope_pop();
	}
}

auto gse::gpu::recording_context::check_active() const -> void {
	assert(async::pass_recording_scope_active() > 0, "recording_context method called outside an active pass; rec was captured by reference past its lifetime");
}

auto gse::gpu::recording_context::sample_image(const image& img, const gpu::pipeline_stage_flags stages) -> void {
	check_active();
	note_touched(
		{
			.ptr = std::addressof(img),
			.type = resource_type::image,
		},
		stages,
		gpu::access_flag::shader_sampled_read
	);
}

auto gse::gpu::recording_context::note_touched(const resource_ref ref, const gpu::pipeline_stage_flags stages, const gpu::access_flags access) -> void {
	if (!ref.ptr) {
		return;
	}
	for (auto& existing : m_touched) {
		if (existing.ref.ptr == ref.ptr) {
			existing.stages |= stages;
			existing.access |= access;
			return;
		}
	}
	m_touched.push_back({
		.ref = ref,
		.stages = stages,
		.access = access,
	});
}

auto gse::gpu::recording_context::finalize_pass() -> void {
	if (!m_pass) {
		return;
	}

	constexpr gpu::access_flags write_mask =
		gpu::access_flag::shader_write
		| gpu::access_flag::shader_storage_write
		| gpu::access_flag::color_attachment_write
		| gpu::access_flag::depth_stencil_attachment_write
		| gpu::access_flag::transfer_write
		| gpu::access_flag::host_write
		| gpu::access_flag::memory_write
		| gpu::access_flag::acceleration_structure_write;

	for (const auto& [ref, stages, access] : m_touched) {
		const bool has_writes = (access & write_mask).bits() != 0;
		auto& bucket = has_writes ? m_pass->writes : m_pass->reads;
		bucket.push_back({
			.resource = ref,
			.stage = stages,
			.access = access,
		});
	}
}

auto gse::gpu::recording_context::copy_buffer(const buffer& src, const buffer& dst, const std::size_t size, const std::size_t src_offset, const std::size_t dst_offset) -> void {
	check_active();
	note_touched(
		{
			.ptr = std::addressof(src),
			.type = resource_type::buffer,
		},
		gpu::pipeline_stage_flag::copy,
		gpu::access_flag::transfer_read
	);
	note_touched(
		{
			.ptr = std::addressof(dst),
			.type = resource_type::buffer,
		},
		gpu::pipeline_stage_flag::copy,
		gpu::access_flag::transfer_write
	);
	m_cmd.copy_buffer(src.handle(), dst.handle(), gpu::buffer_copy_region{
		.src_offset = src_offset,
		.dst_offset = dst_offset,
		.size = size
	});
}

auto gse::gpu::recording_context::fill_buffer(const buffer& dst, const std::size_t offset, const std::size_t size, const std::uint32_t data) -> void {
	check_active();
	note_touched(
		{
			.ptr = std::addressof(dst),
			.type = resource_type::buffer,
		},
		gpu::pipeline_stage_flag::copy,
		gpu::access_flag::transfer_write
	);
	m_cmd.fill_buffer(dst.handle(), offset, size, data);
}

auto gse::gpu::recording_context::barrier(const gpu::barrier_scope scope) const -> void {
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

auto gse::gpu::recording_context::build_acceleration_structure(
	const gpu::acceleration_structure_build_geometry_info& build_info,
	const std::span<const gpu::acceleration_structure_build_range_info* const> range_infos
) -> void {
	check_active();
	note_touched(
		{
			.ptr = std::bit_cast<const void*>(build_info.dst.value),
			.type = resource_type::acceleration_structure,
		},
		gpu::pipeline_stage_flag::acceleration_structure_build,
		gpu::access_flag::acceleration_structure_read | gpu::access_flag::acceleration_structure_write
	);
	m_cmd.build_acceleration_structures(build_info, range_infos);
}

auto gse::gpu::recording_context::pipeline_barrier(const gpu::dependency_info& dep) const -> void {
	check_active();
	m_cmd.pipeline_barrier(dep);
}

auto gse::gpu::recording_context::capture_swapchain(
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

auto gse::gpu::recording_context::blit_swapchain_to_image(const gpu::swap_chain& swapchain, const gpu::frame& frame, const image& dst, const vec2u dst_extent) const -> void {
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

auto gse::gpu::recording_context::set_viewport(const float x, const float y, const float width, const float height, const float min_depth, const float max_depth) const -> void {
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

auto gse::gpu::recording_context::set_scissor(const std::int32_t x, const std::int32_t y, const std::uint32_t width, const std::uint32_t height) const -> void {
	check_active();
	const gse::rect_t<vec2i> sc{ {
		.min = vec2i{ x, y },
		.max = vec2i{ x + static_cast<int>(width), y + static_cast<int>(height) },
	} };
	m_cmd.set_scissor(sc);
}

auto gse::gpu::recording_context::draw(const std::uint32_t vertex_count, const std::uint32_t instance_count, const std::uint32_t first_vertex, const std::uint32_t first_instance) const -> void {
	check_active();
	m_cmd.draw(vertex_count, instance_count, first_vertex, first_instance);
}

auto gse::gpu::recording_context::draw_indexed(const std::uint32_t index_count, const std::uint32_t instance_count, const std::uint32_t first_index, const std::int32_t vertex_offset, const std::uint32_t first_instance) const -> void {
	check_active();
	m_cmd.draw_indexed(index_count, instance_count, first_index, vertex_offset, first_instance);
}

auto gse::gpu::recording_context::draw_mesh_tasks(const std::uint32_t x, const std::uint32_t y, const std::uint32_t z) const -> void {
	check_active();
	m_cmd.draw_mesh_tasks(x, y, z);
}

auto gse::gpu::recording_context::dispatch(const std::uint32_t x, const std::uint32_t y, const std::uint32_t z) const -> void {
	check_active();
	m_cmd.dispatch(x, y, z);
}

auto gse::gpu::recording_context::dispatch_indirect(const buffer& buf, const std::size_t offset) -> void {
	check_active();
	note_touched(
		{
			.ptr = std::addressof(buf),
			.type = resource_type::buffer,
		},
		gpu::pipeline_stage_flag::draw_indirect,
		gpu::access_flag::indirect_command_read
	);
	m_cmd.dispatch_indirect(buf.handle(), static_cast<gpu::device_size>(offset));
}

auto gse::gpu::recording_context::end_rendering() const -> void {
	check_active();
	m_cmd.end_rendering();
}

template <typename T>
auto gse::gpu::recording_context::push(const gpu::pipeline& p, const gpu::typed_push_constants<T>& typed) const -> void {
	check_active();
	typed.replay(m_cmd.native(), p.layout());
}

auto gse::gpu::recording_context::draw_indirect(const buffer& buf, const std::size_t offset, const std::uint32_t draw_count, const std::uint32_t stride) -> void {
	check_active();
	note_touched(
		{
			.ptr = std::addressof(buf),
			.type = resource_type::buffer,
		},
		gpu::pipeline_stage_flag::draw_indirect,
		gpu::access_flag::indirect_command_read
	);
	m_cmd.draw_indexed_indirect(buf.handle(), offset, draw_count, stride);
}

auto gse::gpu::recording_context::draw_mesh_tasks_indirect(const buffer& buf, const std::size_t offset, const std::uint32_t draw_count, const std::uint32_t stride) -> void {
	check_active();
	note_touched(
		{
			.ptr = std::addressof(buf),
			.type = resource_type::buffer,
		},
		gpu::pipeline_stage_flag::draw_indirect,
		gpu::access_flag::indirect_command_read
	);
	m_cmd.draw_mesh_tasks_indirect(buf.handle(), offset, draw_count, stride);
}

auto gse::gpu::recording_context::bind(const gpu::pipeline& p) const -> void {
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

auto gse::gpu::recording_context::bind_descriptors(const gpu::pipeline& p, const gpu::descriptor_region& region, const std::uint32_t set_index) -> void {
	check_active();
	assert(region, "Cannot bind null descriptor region");
	for (const auto& [set, slot, access, type, stages] : p.active_bindings()) {
		if (set != set_index) {
			continue;
		}
		const auto it = std::ranges::find_if(region.resources, [slot](const resource_slot& rs) {
			return rs.slot == slot;
		});
		assert(
			it != region.resources.end(),
			"render pass '{}' bound a pipeline expecting set={} slot={} ({}, {}) but the descriptor region has no resource at that slot. Bound slots: {}",
			m_pass ? m_pass->pass_type.tag() : std::string_view{ "<unknown>" },
			set,
			slot,
			type,
			access,
			format_bound_slots(region.resources)
		);
		note_touched(it->ref, stages, barrier_access(type, access));
	}
	region.heap->bind(m_cmd.native(), p.bind_point(), p.layout(), set_index, region);
}

auto gse::gpu::recording_context::bind_vertex(const buffer& buf, const std::size_t offset) -> void {
	check_active();
	note_touched(
		{
			.ptr = std::addressof(buf),
			.type = resource_type::buffer,
		},
		gpu::pipeline_stage_flag::vertex_attribute_input,
		gpu::access_flag::vertex_attribute_read
	);
	const gpu::handle<buffer> buffers[]{ buf.handle() };
	const gpu::device_size offsets[]{ offset };
	m_cmd.bind_vertex_buffers(0, std::span<const gpu::handle<buffer>>(buffers), std::span<const gpu::device_size>(offsets));
}

auto gse::gpu::recording_context::bind_index(const buffer& buf, const gpu::index_type type, const std::size_t offset) -> void {
	check_active();
	note_touched(
		{
			.ptr = std::addressof(buf),
			.type = resource_type::buffer,
		},
		gpu::pipeline_stage_flag::index_input,
		gpu::access_flag::index_read
	);
	m_cmd.bind_index_buffer_2(buf.handle(), offset, vk::WholeSize, type);
}

auto gse::gpu::recording_context::set_viewport(const vec2u extent) const -> void {
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

auto gse::gpu::recording_context::set_scissor(const vec2u extent) const -> void {
	check_active();
	const gse::rect_t<vec2i> sc{ {
		.min = vec2i{ 0, 0 },
		.max = vec2i{ static_cast<int>(extent.x()), static_cast<int>(extent.y()) },
	} };
	m_cmd.set_scissor(sc);
}

auto gse::gpu::recording_context::begin_rendering(const vec2u extent, const image* depth, const gpu::image_layout depth_layout, const bool clear_depth, const float clear_depth_value) const -> void {
	check_active();
	std::optional<gpu::rendering_attachment_info> depth_att;
	if (depth) {
		depth_att = gpu::rendering_attachment_info{
			.image_view = depth->view(),
			.layout = depth_layout,
			.load = clear_depth ? gpu::load_op::clear : gpu::load_op::load,
			.store = gpu::store_op::store,
			.depth_clear_value = { .depth = clear_depth_value },
		};
	}

	const gpu::rendering_info ri{
		.render_area = gse::rect_t<vec2i>::from_position_size(vec2i{ 0, 0 }, vec2i{ static_cast<int>(extent.x()), static_cast<int>(extent.y()) }),
		.layer_count = 1,
		.depth_attachment = depth_att ? &*depth_att : nullptr,
	};
	m_cmd.begin_rendering(ri);
}

auto gse::gpu::recording_context::commit(const gpu::descriptor_writer& writer, const gpu::pipeline& p, const std::uint32_t set_index) -> void {
	check_active();
	const auto written = writer.touched_resources();
	for (const auto& [set, slot, access, type, stages] : p.active_bindings()) {
		if (set != set_index) {
			continue;
		}
		const auto it = std::ranges::find_if(written, [slot](const resource_slot& rs) {
			return rs.slot == slot;
		});
		assert(
			it != written.end(),
			"render pass '{}' committed a push descriptor for set={} slot={} ({}, {}) but the writer has no resource at that slot. Written slots: {}",
			m_pass ? m_pass->pass_type.tag() : std::string_view{ "<unknown>" },
			set,
			slot,
			type,
			access,
			format_bound_slots(written)
		);
		note_touched(it->ref, stages, barrier_access(type, access));
	}
	writer.native_writer().commit(
		m_cmd.native(),
		p.bind_point(),
		p.layout(),
		set_index
	);
}

auto gse::gpu::format_bound_slots(const std::span<const resource_slot> resources) -> std::string {
	if (resources.empty()) {
		return "<none>";
	}
	return resources
		| std::views::transform([](const resource_slot& rs) {
			return std::format("slot={} ({})", rs.slot, rs.ref.type);
		})
		| std::views::join_with(std::string_view{ ", " })
		| std::ranges::to<std::string>();
}

auto gse::gpu::barrier_access(const descriptor_type type, const descriptor_access access) -> access_flags {
	switch (type) {
		case descriptor_type::uniform_buffer:
			return access_flag::uniform_read;
		case descriptor_type::storage_buffer:
			return access == descriptor_access::read_write
				? access_flag::shader_storage_read | access_flag::shader_storage_write
				: access_flags{ access_flag::shader_storage_read };
		case descriptor_type::combined_image_sampler:
		case descriptor_type::sampled_image:
		case descriptor_type::sampler:
			return access_flag::shader_sampled_read;
		case descriptor_type::storage_image:
			return access == descriptor_access::read_write
				? access_flag::shader_storage_read | access_flag::shader_storage_write
				: access_flags{ access_flag::shader_storage_read };
		case descriptor_type::acceleration_structure:
			return access_flag::acceleration_structure_read;
	}
	return access_flag::shader_storage_read;
}

namespace gse::gpu {
	constexpr auto profile_stats_flags =
		vk::QueryPipelineStatisticFlagBits::eInputAssemblyVertices
		| vk::QueryPipelineStatisticFlagBits::eInputAssemblyPrimitives
		| vk::QueryPipelineStatisticFlagBits::eClippingInvocations
		| vk::QueryPipelineStatisticFlagBits::eFragmentShaderInvocations;
}

gse::gpu::render_graph::render_graph(gpu::device& device, gpu::swap_chain& swapchain, gpu::frame& frame, gpu::bindless_texture_set* bindless) : m_device(std::addressof(device)), m_swapchain(std::addressof(swapchain)), m_frame(std::addressof(frame)), m_bindless(bindless), m_transient_pool(device) {
	m_timestamp_period_per_tick = nanoseconds(static_cast<double>(device.timestamp_period()));
	for (auto& q : m_queue_states) {
		q.timeline = vulkan::semaphore::create_timeline(device.vulkan_device(), 0);
	}
}

auto gse::gpu::render_graph::take_aux_submissions() -> std::vector<gpu::queue_submission> {
	return std::move(m_pending_aux_submissions);
}

auto gse::gpu::render_graph::take_graphics_extra_waits() -> std::vector<gpu::semaphore_submit_info> {
	return std::move(m_pending_graphics_extra_waits);
}

auto gse::gpu::render_graph::set_gpu_timestamps_enabled(const bool enabled) -> void {
	m_gpu_timestamps_enabled.store(enabled, std::memory_order_relaxed);
}

auto gse::gpu::render_graph::set_gpu_pipeline_stats_enabled(const bool enabled) -> void {
	m_gpu_pipeline_stats_enabled.store(enabled, std::memory_order_relaxed);
}

auto gse::gpu::render_graph::ensure_profile_pools(gpu_profile_slot& slot, const bool allow_stats) const -> void {
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

auto gse::gpu::render_graph::read_profile_slot(gpu_profile_slot& slot) -> void {
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

auto gse::gpu::render_graph::current_frame() const -> std::uint32_t {
	return m_frame->current_frame();
}

auto gse::gpu::render_graph::extent() const -> vec2u {
	return m_swapchain->extent();
}

auto gse::gpu::render_graph::depth_image(this auto& self) -> auto& {
	return self.m_swapchain->depth_image();
}

auto gse::gpu::render_graph::transient_pool(this auto& self) -> auto& {
	return self.m_transient_pool;
}

auto gse::gpu::render_graph::frame_in_progress() const -> bool {
	return m_frame->frame_in_progress();
}

auto gse::gpu::render_graph::execute(std::vector<render_pass_data> passes, std::vector<transient_image_request> transient_images, std::vector<transient_buffer_request> transient_buffers) -> void {
	m_pending_aux_submissions.clear();
	m_pending_graphics_extra_waits.clear();

	if (!m_frame->frame_in_progress()) {
		return;
	}

	{
		std::vector<id> pass_kind_order;
		pass_kind_order.reserve(passes.size());
		for (const auto& p : passes) {
			pass_kind_order.push_back(p.pass_type);
		}
		m_transient_pool.plan(m_frame->current_frame(), transient_images, transient_buffers, pass_kind_order);
	}

	m_auto_binds.clear();
	if (m_bindless) {
		constexpr auto bindless_idx = static_cast<std::uint32_t>(gpu::descriptor_set_type::bind_less);
		m_auto_binds.push_back({ .set_index = bindless_idx, .region = m_bindless->region() });
	}

	const auto frame_idx = m_frame->current_frame();
	std::array<gpu::handle<command_buffer>, gpu::queue_type_count> primary_handles;
	std::array<vk::CommandBuffer, gpu::queue_type_count> primary_buffers;
	for (std::size_t qi = 0; qi < gpu::queue_type_count; ++qi) {
		primary_handles[qi] = m_frame->command_buffer(static_cast<gpu::queue_type>(qi));
		primary_buffers[qi] = std::bit_cast<vk::CommandBuffer>(primary_handles[qi]);
	}
	const auto graphics_family = m_device->vulkan_device().queue_family(gpu::queue_type::graphics);
	std::array<bool, gpu::queue_type_count> queue_distinct{};
	for (std::size_t qi = 0; qi < gpu::queue_type_count; ++qi) {
		queue_distinct[qi] = m_device->vulkan_device().queue_family(static_cast<gpu::queue_type>(qi)) != graphics_family;
	}
	queue_distinct[static_cast<std::size_t>(gpu::queue_type::graphics)] = true;

	auto effective_queue = [&](const gpu::queue_type requested) -> gpu::queue_type {
		return queue_distinct[static_cast<std::size_t>(requested)] ? requested : gpu::queue_type::graphics;
	};

	const auto image_index = m_frame->image_index();
	const auto swap_extent = m_swapchain->extent();
	const vk::Extent2D vk_extent{ swap_extent.x(), swap_extent.y() };

	const bool timestamps_enabled = m_gpu_timestamps_enabled.load(std::memory_order_relaxed);
	const bool stats_enabled = m_gpu_pipeline_stats_enabled.load(std::memory_order_relaxed);

	if (m_frames_submitted >= per_frame_resource<gpu_profile_slot>::frames_in_flight) {
		for (auto& slots : m_profile_slots) {
			read_profile_slot(slots[frame_idx]);
		}
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
	for (auto& slots : m_profile_slots) {
		reset_slot(slots[frame_idx]);
	}

	std::array<bool, gpu::queue_type_count> queue_has_work{};
	queue_has_work[static_cast<std::size_t>(gpu::queue_type::graphics)] = true;
	for (const auto& p : passes) {
		queue_has_work[static_cast<std::size_t>(effective_queue(p.queue))] = true;
	}

	auto open_primary = [this](const gpu::handle<command_buffer> handle) {
		const vulkan::commands cmd(handle);
		cmd.reset();
		cmd.begin();
		m_device->descriptor_heap().bind_descriptor_storage(handle);
	};

	for (std::size_t qi = 0; qi < gpu::queue_type_count; ++qi) {
		if (qi == static_cast<std::size_t>(gpu::queue_type::graphics)) continue;
		if (queue_has_work[qi]) {
			open_primary(m_frame->command_buffer(static_cast<gpu::queue_type>(qi)));
		}
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
		for (std::size_t qi = 0; qi < gpu::queue_type_count; ++qi) {
			if (!queue_has_work[qi]) continue;
			const auto q = static_cast<gpu::queue_type>(qi);
			const bool with_stats = (q == gpu::queue_type::graphics) && stats_enabled;
			auto cb = std::bit_cast<vk::CommandBuffer>(m_frame->command_buffer(q));
			setup_timestamps(m_profile_slots[qi][frame_idx], cb, with_stats);
		}
	}

	auto pass_queue = [&](const std::size_t pi) -> gpu::queue_type {
		return effective_queue(passes[pi].queue);
	};

	auto& worker_pools = m_device->worker_command_pools();
	worker_pools.reset_frame(frame_idx);
	const auto color_format = m_swapchain->format();

	std::vector<vk::CommandBuffer> pass_secondaries(passes.size());

	auto& inheritance = m_inheritance_storage[frame_idx];
	inheritance.color_formats.assign(passes.size(), std::array<vk::Format, 1>{});
	inheritance.rendering_inherits.assign(passes.size(), vk::CommandBufferInheritanceRenderingInfo{});
	inheritance.inherits.assign(passes.size(), vk::CommandBufferInheritanceInfo{});

	auto resolve_color_target = [&](const color_output_info& info) -> const image* {
		if (info.transient_target) {
			return m_transient_pool.resolve_image(info.transient_target);
		}
		if (info.custom_target) {
			return info.custom_target;
		}
		return nullptr;
	};

	auto resolve_depth_target = [&](const depth_output_info& info) -> const image* {
		if (info.transient_target) {
			return m_transient_pool.resolve_image(info.transient_target);
		}
		if (info.custom_target) {
			return info.custom_target;
		}
		return nullptr;
	};

	task::parallel_invoke_range(0, passes.size(), [&](std::size_t pi) {
		auto& pass = passes[pi];
		const auto queue = pass_queue(pi);
		const bool is_graphics_pass = pass.color_output || pass.depth_output;
		const bool maybe_issue_stats = timestamps_enabled && stats_enabled && is_graphics_pass;

		const auto worker_idx = task::current_worker();
		assert(worker_idx.has_value(), "graph::record_parallel: thread has no arena slot");
		const auto secondary = worker_pools.acquire_secondary(queue, *worker_idx, frame_idx);

		const auto* color_target = pass.color_output ? resolve_color_target(*pass.color_output) : nullptr;
		const auto* depth_target = pass.depth_output ? resolve_depth_target(*pass.depth_output) : nullptr;

		const auto color_attach_format = color_target
			? static_cast<vk::Format>(color_target->format())
			: vulkan::to_vk(color_format);
		const auto depth_attach_format = depth_target
			? static_cast<vk::Format>(depth_target->format())
			: vk::Format::eD32Sfloat;
		inheritance.color_formats[pi] = { color_attach_format };

		inheritance.rendering_inherits[pi] = vk::CommandBufferInheritanceRenderingInfo{
			.viewMask = 0,
			.colorAttachmentCount = pass.color_output ? 1u : 0u,
			.pColorAttachmentFormats = pass.color_output ? inheritance.color_formats[pi].data() : nullptr,
			.depthAttachmentFormat = pass.depth_output ? depth_attach_format : vk::Format::eUndefined,
			.stencilAttachmentFormat = vk::Format::eUndefined,
			.rasterizationSamples = vk::SampleCountFlagBits::e1,
		};

		auto& inherit = inheritance.inherits[pi];
		vk::CommandBufferUsageFlags begin_flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
		if (is_graphics_pass) {
			inherit.pNext = &inheritance.rendering_inherits[pi];
			begin_flags |= vk::CommandBufferUsageFlagBits::eRenderPassContinue;
		}
		if (maybe_issue_stats) {
			inherit.pipelineStatistics = profile_stats_flags;
		}

		secondary.begin({
			.flags = begin_flags,
			.pInheritanceInfo = &inherit
		});
		m_device->descriptor_heap().bind_descriptor_storage(std::bit_cast<gpu::handle<command_buffer>>(secondary));
		recording_context rec{ commands{ std::bit_cast<gpu::handle<command_buffer>>(secondary) }, m_auto_binds, std::addressof(pass), std::addressof(m_transient_pool) };
		if (pass.primary_pipeline) {
			rec.bind(*pass.primary_pipeline);
		}
		if (pass.depth_output) {
			const auto* depth_img = depth_target ? depth_target : std::addressof(m_swapchain->depth_image());
			const auto depth_ref = resource_ref{
				.ptr = depth_img,
				.type = resource_type::image,
			};
			if (pass.depth_output->op == load_op::load) {
				rec.note_touched(
					depth_ref,
					gpu::pipeline_stage_flag::early_fragment_tests,
					gpu::access_flag::depth_stencil_attachment_read
				);
			}
			else {
				rec.note_touched(
					depth_ref,
					gpu::pipeline_stage_flag::late_fragment_tests,
					gpu::access_flag::depth_stencil_attachment_write
				);
			}
		}
		if (pass.color_output) {
			const auto* color_img = color_target;
			if (color_img) {
				const auto color_ref = resource_ref{
					.ptr = color_img,
					.type = resource_type::image,
				};
				if (pass.color_output->op == load_op::load) {
					rec.note_touched(
						color_ref,
						gpu::pipeline_stage_flag::color_attachment_output,
						gpu::access_flag::color_attachment_read | gpu::access_flag::color_attachment_write
					);
				}
				else {
					rec.note_touched(
						color_ref,
						gpu::pipeline_stage_flag::color_attachment_output,
						gpu::access_flag::color_attachment_write
					);
				}
			}
		}
		if (pass.body) {
			(*pass.body)(rec);
		}
		else {
			*pass.record_ctx_slot = std::move(rec);
			pass.record_handle.resume();
		}

		pass_secondaries[pi] = secondary;
	});

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
				if (passes[i].chain_id.exists() && passes[i].chain_id == passes[j].chain_id) {
					add_edge(i, j);
					continue;
				}

				bool i_writes_j_reads = false;
				bool j_writes_i_reads = false;
				bool both_write = false;

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
				for (const auto& wi : passes[i].writes) {
					for (const auto& wj : passes[j].writes) {
						if (wi.resource.ptr && wj.resource.ptr && wi.resource.ptr == wj.resource.ptr) {
							both_write = true;
						}
					}
				}

				if (i_writes_j_reads || both_write) {
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
		assert(sorted.size() == passes.size(), "render_graph: cyclic pass dependency graph");
	}

	std::array<std::array<bool, gpu::queue_type_count>, gpu::queue_type_count> queue_waits_on{};
	for (std::size_t i = 0; i < passes.size(); ++i) {
		for (std::size_t j = 0; j < passes.size(); ++j) {
			if (i == j) continue;
			const auto qi = pass_queue(i);
			const auto qj = pass_queue(j);
			if (qi == qj) continue;
			for (const auto& w : passes[i].writes) {
				for (const auto& r : passes[j].reads) {
					if (w.resource.ptr && r.resource.ptr && w.resource.ptr == r.resource.ptr) {
						queue_waits_on[static_cast<std::size_t>(qj)][static_cast<std::size_t>(qi)] = true;
					}
				}
			}
		}
	}

	for (std::size_t a = 0; a < gpu::queue_type_count; ++a) {
		for (std::size_t b = 0; b < gpu::queue_type_count; ++b) {
			if (a == b) continue;
			assert(!(queue_waits_on[a][b] && queue_waits_on[b][a]),
				"render_graph: cyclic cross-queue dependency between two queues; split a submission or move the pass");
		}
	}

	{
		trace::scope_guard sg{gse::trace_id<"graph::record_replay">()};

		auto aspect_for_image = [](const image& img) -> gpu::image_aspect_flags {
			constexpr auto d32 = static_cast<gpu::image_format_value>(vk::Format::eD32Sfloat);
			if (img.format() == d32) {
				return gpu::image_aspect_flag::depth;
			}
			return gpu::image_aspect_flag::color;
		};

		auto append_barrier_for_resource = [&](
			const resource_ref& resource,
			const gpu::pipeline_stage_flags src_stages,
			const gpu::access_flags src_access,
			const gpu::pipeline_stage_flags dst_stages,
			const gpu::access_flags dst_access,
			std::vector<gpu::memory_barrier>& memory_out,
			std::vector<gpu::buffer_barrier>& buffer_out,
			std::vector<gpu::image_barrier>& image_out
		) {
			if (resource.type == resource_type::buffer) {
				const auto* buf = static_cast<const buffer*>(resource.ptr);
				buffer_out.push_back({
					.src_stages = src_stages,
					.src_access = src_access,
					.dst_stages = dst_stages,
					.dst_access = dst_access,
					.buffer = buf->handle(),
					.offset = 0,
					.size = buf->size(),
				});
			}
			else if (resource.type == resource_type::image) {
				const auto* img = static_cast<const image*>(resource.ptr);
				const auto layout = img->layout();
				image_out.push_back({
					.src_stages = src_stages,
					.src_access = src_access,
					.dst_stages = dst_stages,
					.dst_access = dst_access,
					.old_layout = layout,
					.new_layout = layout,
					.image = img->handle(),
					.aspects = aspect_for_image(*img),
					.base_mip_level = 0,
					.level_count = 1,
					.base_array_layer = 0,
					.layer_count = 1,
				});
			}
			else {
				memory_out.push_back({
					.src_stages = src_stages,
					.src_access = src_access,
					.dst_stages = dst_stages,
					.dst_access = dst_access,
				});
			}
		};

		auto append_host_dirty_barriers = [&](const render_pass_data& p, std::vector<gpu::buffer_barrier>& out) {
			auto walk = [&](const std::vector<resource_usage>& list) {
				for (const auto& [resource, stage, access] : list) {
					if (resource.type != resource_type::buffer || !resource.ptr) {
						continue;
					}
					const auto* buf = static_cast<const buffer*>(resource.ptr);
					if (!buf->host_dirty()) {
						continue;
					}
					out.push_back({
						.src_stages = gpu::pipeline_stage_flag::host,
						.src_access = gpu::access_flag::host_write,
						.dst_stages = stage,
						.dst_access = access,
						.buffer = buf->handle(),
						.offset = 0,
						.size = buf->size(),
					});
					buf->clear_host_dirty();
				}
			};
			walk(p.reads);
			walk(p.writes);
		};

		auto append_prev_pass_barriers = [&](const render_pass_data& cur, const gpu::queue_type cur_queue, const std::size_t serial_index, std::vector<gpu::memory_barrier>& memory_out, std::vector<gpu::buffer_barrier>& buffer_out, std::vector<gpu::image_barrier>& image_out) {
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
							append_barrier_for_resource(prev_resource, prev_stage, prev_access, read_stage, read_access, memory_out, buffer_out, image_out);
						}
					}
					for (const auto& [cur_resource, cur_stage, cur_access] : cur.writes) {
						if (cur_resource.ptr && prev_resource.ptr == cur_resource.ptr) {
							append_barrier_for_resource(prev_resource, prev_stage, prev_access, cur_stage, cur_access, memory_out, buffer_out, image_out);
						}
					}
				}
				for (const auto& [prev_resource, prev_stage, prev_access] : prev.reads) {
					if (!prev_resource.ptr) {
						continue;
					}
					for (const auto& [cur_resource, cur_stage, cur_access] : cur.writes) {
						if (cur_resource.ptr && prev_resource.ptr == cur_resource.ptr) {
							append_barrier_for_resource(prev_resource, prev_stage, {}, cur_stage, cur_access, memory_out, buffer_out, image_out);
						}
					}
				}
			}
		};

		std::vector<std::vector<gpu::image_barrier>> alias_barriers_for_sorted(sorted.size());
		{
			const auto transient_infos = m_transient_pool.transient_images();
			for (const auto& info : transient_infos) {
				std::size_t first_si = sorted.size();
				gpu::pipeline_stage_flags first_stages;
				gpu::access_flags first_access;
				for (std::size_t si = 0; si < sorted.size(); ++si) {
					const auto& p = passes[sorted[si]];
					auto match = [&](const std::vector<resource_usage>& list) -> bool {
						for (const auto& u : list) {
							if (u.resource.type == resource_type::image && u.resource.ptr == info.resource) {
								first_stages |= u.stage;
								first_access |= u.access;
								return true;
							}
						}
						return false;
					};
					const bool in_reads = match(p.reads);
					const bool in_writes = match(p.writes);
					if (in_reads || in_writes) {
						first_si = si;
						break;
					}
				}
				if (first_si == sorted.size()) {
					continue;
				}
				alias_barriers_for_sorted[first_si].push_back({
					.src_stages = gpu::pipeline_stage_flag::all_commands,
					.src_access = gpu::access_flag::memory_write,
					.dst_stages = first_stages,
					.dst_access = first_access,
					.old_layout = gpu::image_layout::undefined,
					.new_layout = info.target_layout,
					.image = info.resource->handle(),
					.aspects = info.aspects,
					.base_mip_level = 0,
					.level_count = 1,
					.base_array_layer = 0,
					.layer_count = 1,
				});
			}
		}

		for (std::size_t si = 0; si < sorted.size(); ++si) {
			const auto pass_idx = sorted[si];
			auto& pass = passes[pass_idx];
			const auto queue = pass_queue(pass_idx);
			const auto target_handle = primary_handles[static_cast<std::size_t>(queue)];
			const auto target_primary = primary_buffers[static_cast<std::size_t>(queue)];
			auto& target_slot = m_profile_slots[static_cast<std::size_t>(queue)][frame_idx];

			std::vector<gpu::memory_barrier> memory_barriers;
			std::vector<gpu::buffer_barrier> buffer_barriers;
			std::vector<gpu::image_barrier> image_barriers = std::move(alias_barriers_for_sorted[si]);

			append_host_dirty_barriers(pass, buffer_barriers);
			append_prev_pass_barriers(pass, queue, si, memory_barriers, buffer_barriers, image_barriers);

			{
				std::vector<gpu::memory_barrier> coalesced;
				coalesced.reserve(memory_barriers.size());
				for (const auto& b : memory_barriers) {
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
				memory_barriers = std::move(coalesced);
			}

			{
				std::vector<gpu::buffer_barrier> coalesced;
				coalesced.reserve(buffer_barriers.size());
				for (const auto& b : buffer_barriers) {
					bool merged = false;
					for (auto& o : coalesced) {
						if (o.buffer.value == b.buffer.value
							&& o.offset == b.offset
							&& o.size == b.size
							&& o.src_stages.bits() == b.src_stages.bits()
							&& o.dst_stages.bits() == b.dst_stages.bits()) {
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
				buffer_barriers = std::move(coalesced);
			}

			{
				std::vector<gpu::image_barrier> coalesced;
				coalesced.reserve(image_barriers.size());
				for (const auto& b : image_barriers) {
					bool merged = false;
					for (auto& o : coalesced) {
						if (o.image.value == b.image.value
							&& o.old_layout == b.old_layout
							&& o.new_layout == b.new_layout
							&& o.aspects.bits() == b.aspects.bits()
							&& o.base_mip_level == b.base_mip_level
							&& o.level_count == b.level_count
							&& o.base_array_layer == b.base_array_layer
							&& o.layer_count == b.layer_count
							&& o.src_stages.bits() == b.src_stages.bits()
							&& o.dst_stages.bits() == b.dst_stages.bits()) {
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
				image_barriers = std::move(coalesced);
			}

			const bool profile_pass = timestamps_enabled && target_slot.pass_count < max_profiled_passes;
			const std::uint32_t pass_index = target_slot.pass_count;
			const bool is_graphics_pass = pass.color_output || pass.depth_output;
			const bool issue_stats = profile_pass && stats_enabled && is_graphics_pass && queue == gpu::queue_type::graphics;

			const auto marker_domain = (queue == gpu::queue_type::graphics)
				? gpu::device::pass_marker_domain::graphics_queue
				: gpu::device::pass_marker_domain::compute_queue;
			const auto marker_handle = m_device->begin_pass_marker(target_handle, marker_domain, {
				.frame_counter = m_frames_submitted,
				.pass_index = static_cast<std::uint32_t>(si),
				.pass_type = pass.pass_type,
			});

			if (!memory_barriers.empty() || !buffer_barriers.empty() || !image_barriers.empty()) {
				vulkan::commands(target_handle).pipeline_barrier(gpu::dependency_info{
					.memory_barriers = memory_barriers,
					.buffer_barriers = buffer_barriers,
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
				vk::Extent2D pass_extent = vk_extent;

				if (pass.color_output) {
					const auto& info = *pass.color_output;
					const auto op = info.op;
					const auto& clear_value = info.clear_value;
					auto vk_load = vk::AttachmentLoadOp::eDontCare;
					vk::ClearValue clear_val{};

					if (op == load_op::clear) {
						vk_load = vk::AttachmentLoadOp::eClear;
						clear_val.color = vk::ClearColorValue{
							.float32 = std::array{ clear_value.r, clear_value.g, clear_value.b, clear_value.a }
						};
					}
					else if (op == load_op::load) {
						vk_load = vk::AttachmentLoadOp::eLoad;
					}

					const auto* color_target = resolve_color_target(info);
					vk::ImageView color_view;
					vk::ImageLayout color_layout;
					if (color_target) {
						color_view = std::bit_cast<vk::ImageView>(color_target->view());
						color_layout = vulkan::to_vk(color_target->layout());
						const auto ext = color_target->extent();
						pass_extent = vk::Extent2D{ ext.x(), ext.y() };
					}
					else {
						color_view = std::bit_cast<vk::ImageView>(m_swapchain->image_view(image_index));
						color_layout = vk::ImageLayout::eColorAttachmentOptimal;
					}

					color_attachments.push_back({
						.imageView = color_view,
						.imageLayout = color_layout,
						.loadOp = vk_load,
						.storeOp = vk::AttachmentStoreOp::eStore,
						.clearValue = clear_val
					});
				}

				if (pass.depth_output) {
					const auto& info = *pass.depth_output;
					const auto op = info.op;
					const auto& clear_value = info.clear_value;
					auto vk_load = vk::AttachmentLoadOp::eDontCare;
					vk::ClearValue clear_val{};

					if (op == load_op::clear) {
						vk_load = vk::AttachmentLoadOp::eClear;
						clear_val.depthStencil = vk::ClearDepthStencilValue{ .depth = clear_value.depth };
					}
					else if (op == load_op::load) {
						vk_load = vk::AttachmentLoadOp::eLoad;
					}

					const auto* depth_target = resolve_depth_target(info);
					vk::ImageView depth_view;
					vk::ImageLayout depth_layout;
					if (depth_target) {
						depth_view = std::bit_cast<vk::ImageView>(depth_target->view());
						depth_layout = vulkan::to_vk(depth_target->layout());
						if (!pass.color_output) {
							const auto ext = depth_target->extent();
							pass_extent = vk::Extent2D{ ext.x(), ext.y() };
						}
					}
					else {
						depth_view = std::bit_cast<vk::ImageView>(m_swapchain->depth_image().view());
						depth_layout = vk::ImageLayout::eGeneral;
					}

					depth_att = vk::RenderingAttachmentInfo{
						.imageView = depth_view,
						.imageLayout = depth_layout,
						.loadOp = vk_load,
						.storeOp = vk::AttachmentStoreOp::eStore,
						.clearValue = clear_val
					};
				}

				const vk::RenderingInfo ri{
					.flags = vk::RenderingFlagBits::eContentsSecondaryCommandBuffers,
					.renderArea = { { 0, 0 }, pass_extent },
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

	if (timestamps_enabled) {
		for (std::size_t qi = 0; qi < gpu::queue_type_count; ++qi) {
			auto& slot = m_profile_slots[qi][frame_idx];
			if (slot.pass_count > 0) {
				slot.results_valid = true;
			}
		}
	}

	std::array<std::uint64_t, gpu::queue_type_count> this_frame_signal_values{};

	for (std::size_t qi = 0; qi < gpu::queue_type_count; ++qi) {
		if (qi == static_cast<std::size_t>(gpu::queue_type::graphics)) continue;
		if (!queue_has_work[qi]) continue;

		const auto q = static_cast<gpu::queue_type>(qi);
		const auto handle = m_frame->command_buffer(q);
		vulkan::commands(handle).end();

		auto& state = m_queue_states[qi];
		const std::uint64_t previous_value = state.signal_counter;
		const std::uint64_t signal_value = ++state.signal_counter;
		this_frame_signal_values[qi] = signal_value;

		gpu::queue_submission sub;
		sub.queue = q;
		sub.command_buffer = handle;
		if (previous_value > 0) {
			sub.waits.push_back({
				.semaphore = state.timeline.handle(),
				.value = previous_value,
				.stages = gpu::pipeline_stage_flag::all_commands,
			});
		}
		for (std::size_t producer = 0; producer < gpu::queue_type_count; ++producer) {
			if (producer == qi) continue;
			if (queue_waits_on[qi][producer] && this_frame_signal_values[producer] > 0) {
				sub.waits.push_back({
					.semaphore = m_queue_states[producer].timeline.handle(),
					.value = this_frame_signal_values[producer],
					.stages = gpu::pipeline_stage_flag::all_commands,
				});
			}
		}
		sub.signals.push_back({
			.semaphore = state.timeline.handle(),
			.value = signal_value,
			.stages = gpu::pipeline_stage_flag::all_commands,
		});
		m_pending_aux_submissions.push_back(std::move(sub));
	}

	const auto graphics_qi = static_cast<std::size_t>(gpu::queue_type::graphics);
	for (std::size_t producer = 0; producer < gpu::queue_type_count; ++producer) {
		if (producer == graphics_qi) continue;
		if (queue_waits_on[graphics_qi][producer] && this_frame_signal_values[producer] > 0) {
			m_pending_graphics_extra_waits.push_back({
				.semaphore = m_queue_states[producer].timeline.handle(),
				.value = this_frame_signal_values[producer],
				.stages = gpu::pipeline_stage_flag::all_commands,
			});
		}
	}
}


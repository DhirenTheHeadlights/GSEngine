export module gse.gpu:render_graph;

import std;

import :aliases;
import :device;
import :swap_chain;
import :frame;
import :bindless;
import :pipeline_builder;
import :transient_pool;
import :image;

import gse.assert;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.log;
import gse.math;
import gse.meta;

export namespace gse::gpu {
	class render_graph;
	class recording_context;
	struct render_pass_data;

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

	struct framebuffer_image_desc {
		image_format format = image_format::r8g8b8a8_unorm;
		image_usage usage;
		image_aspect_flags aspects = image_aspect_flag::color;
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

		template <typename Entry>
		auto dispatch(
			const entry_push_constants_t<Entry>& pc,
			const binding_args<entry_bindings_pack_t<Entry>>& args,
			vec3u groups
		) const -> void;

		template <typename Entry>
		auto dispatch(
			const binding_args<entry_bindings_pack_t<Entry>>& args,
			vec3u groups
		) const -> void;

		template <typename Entry>
		auto push_bindings(
			const entry_push_constants_t<Entry>& pc,
			const binding_args<entry_bindings_pack_t<Entry>>& args
		) const -> void;

		template <typename Entry>
		auto push_bindings(
			const binding_args<entry_bindings_pack_t<Entry>>& args
		) const -> void;

		auto dispatch_indirect(
			const buffer& buf,
			std::size_t offset = 0
		) -> void;

		template <typename T>
		auto push_data(
			const T& value,
			std::uint32_t offset = 0
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
			const gpu::shader_program& p
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

		static auto finalize_active_on_current_thread() noexcept -> void;

	private:
		friend class render_graph;

		struct touched_resource {
			resource_ref ref;
			gpu::pipeline_stage_flags stages = {};
			gpu::access_flags access = {};
		};

		gpu::commands m_cmd;
		render_pass_data* m_pass = nullptr;
		const gpu::transient_pool* m_transient_pool = nullptr;
		bindless_heaps* m_bindless_heaps = nullptr;
		std::vector<touched_resource> m_touched;
		std::thread::id m_origin_thread;
		pipeline_state_cache m_state_cache;
		bool m_bindless_heaps_valid = false;

		recording_context(
			commands cmd,
			render_pass_data* pass,
			const gpu::transient_pool* transient_pool,
			bindless_heaps* heaps
		);

		auto check_active() const -> void;

		auto ensure_descriptor_heaps() -> void;

		auto note_touched(
			resource_ref ref,
			gpu::pipeline_stage_flags stages,
			gpu::access_flags access
		) -> void;

		auto apply_dynamic_state(
			const gpu::dynamic_pipeline_state& s
		) -> void;

		auto finalize_pass() -> void;
	};

	struct render_pass_data {
		id pass_type{};
		gpu::queue_type queue = gpu::queue_type::graphics;
		const gpu::shader_program* primary_pipeline = nullptr;
		std::vector<resource_usage> reads;
		std::vector<resource_usage> writes;
		std::vector<id> after_passes;
		id chain_id;
		std::vector<color_output_info> color_outputs;
		std::optional<depth_output_info> depth_output;
		std::coroutine_handle<> record_handle;
		std::optional<recording_context>* record_ctx_slot = nullptr;
	};

	struct frame_request_drain {
		move_only_function<std::vector<render_pass_data>()> drain_passes;
		move_only_function<std::vector<transient_image_request>()> drain_images;
		move_only_function<std::vector<transient_buffer_request>()> drain_buffers;
	};

	class render_graph {
	public:
		explicit render_graph(
			gpu::device& device,
			gpu::swap_chain& swapchain,
			gpu::frame& frame,
			gpu::bindless_texture_set* bindless = nullptr,
			bindless_heaps* heaps = nullptr
		);

		auto execute(
			frame_request_drain drain
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

		[[nodiscard]] auto current_frame() const -> std::uint32_t;

		[[nodiscard]] auto extent() const -> vec2u;

		[[nodiscard]] auto depth_image(
			this auto& self
		) -> auto&;

		auto register_framebuffer_image(
			id name,
			const framebuffer_image_desc& desc
		) -> const image&;

		template <typename Marker>
		auto framebuffer_image(
			this auto& self
		) -> auto&;

		[[nodiscard]] auto frame_in_progress() const -> bool;

		[[nodiscard]] auto take_aux_submissions() -> std::vector<gpu::queue_submission>;

		[[nodiscard]] auto take_graphics_extra_waits() -> std::vector<gpu::semaphore_submit_info>;

	private:
		static constexpr std::uint32_t max_profiled_passes = 128;

		struct gpu_profile_slot {
			gpu::query_pool timestamp_pool;
			gpu::query_pool stats_pool;
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

		auto recreate_framebuffer_images() -> void;

		auto create_framebuffer_image(
			const framebuffer_image_desc& desc,
			std::string_view tag
		) -> image;

		struct registered_image {
			framebuffer_image_desc desc;
			image img;
		};

		struct queue_state {
			gpu::semaphore timeline;
			std::uint64_t signal_counter = 0;
		};

		gpu::device* m_device;
		gpu::swap_chain* m_swapchain;
		gpu::frame* m_frame;
		gpu::bindless_texture_set* m_bindless = nullptr;
		bindless_heaps* m_bindless_heaps = nullptr;
		gpu::transient_pool m_transient_pool;
		std::unordered_map<id, std::unique_ptr<registered_image>> m_framebuffer_images;
		std::array<per_frame_resource<gpu_profile_slot>, gpu::queue_type_count> m_profile_slots{
			per_frame_resource<gpu_profile_slot>{ gpu_profile_slot{}, gpu_profile_slot{} },
			per_frame_resource<gpu_profile_slot>{ gpu_profile_slot{}, gpu_profile_slot{} },
		};
		std::atomic<bool> m_gpu_timestamps_enabled{ true };
		std::atomic<bool> m_gpu_pipeline_stats_enabled{ false };
		time_t<double> m_timestamp_period_per_tick = nanoseconds(1.0);
		std::uint64_t m_frames_submitted = 0;
		std::array<queue_state, gpu::queue_type_count> m_queue_states;
		std::vector<gpu::queue_submission> m_pending_aux_submissions;
		std::vector<gpu::semaphore_submit_info> m_pending_graphics_extra_waits;
		std::set<std::pair<id, id>> m_warned_ambiguous_pairs;
	};
}

namespace gse::gpu {
	inline thread_local recording_context* tl_active_recording_context = nullptr;
}

gse::gpu::recording_context::recording_context(const commands cmd, render_pass_data* pass, const gpu::transient_pool* transient_pool, bindless_heaps* heaps)
	: m_cmd(cmd), m_pass(pass), m_transient_pool(transient_pool), m_bindless_heaps(heaps) {
	if (m_cmd.valid()) {
		assert(
			tl_active_recording_context == nullptr,
			"another recording_context is still active on this worker thread; a prior coroutine held its rec across a "
			"co_await that was not gpu::pass<...>(ctx). Scope each rec to end before any non-pass await."
		);
		tl_active_recording_context = this;
		m_origin_thread = std::this_thread::get_id();
	}
}

gse::gpu::recording_context::recording_context(recording_context&& other) noexcept
	: m_cmd(other.m_cmd), m_pass(other.m_pass), m_transient_pool(other.m_transient_pool), m_bindless_heaps(other.m_bindless_heaps), m_touched(std::move(other.m_touched)), m_origin_thread(other.m_origin_thread), m_state_cache(other.m_state_cache), m_bindless_heaps_valid(other.m_bindless_heaps_valid) {
	if (tl_active_recording_context == &other) {
		tl_active_recording_context = this;
	}
	other.m_state_cache.invalidate();
	other.m_cmd = commands{};
	other.m_pass = nullptr;
	other.m_transient_pool = nullptr;
	other.m_bindless_heaps = nullptr;
	other.m_bindless_heaps_valid = false;
}

auto gse::gpu::recording_context::operator=(recording_context&& other) noexcept -> recording_context& {
	if (this != &other) {
		if (m_cmd.valid()) {
			assert(
				std::this_thread::get_id() == m_origin_thread,
				"recording_context's secondary command buffer is being ended on a thread other than its origin. "
				"This means the rec was held alive across a co_await that was not gpu::pass<...>(ctx) and the "
				"coroutine "
				"resumed on a different worker. Scope the rec to end before any non-pass await."
			);
			finalize_pass();
			m_cmd.end();
		}
		if (tl_active_recording_context == this) {
			tl_active_recording_context = nullptr;
		}
		m_cmd = other.m_cmd;
		m_pass = other.m_pass;
		m_transient_pool = other.m_transient_pool;
		m_bindless_heaps = other.m_bindless_heaps;
		m_touched = std::move(other.m_touched);
		m_origin_thread = other.m_origin_thread;
		m_state_cache = other.m_state_cache;
		m_bindless_heaps_valid = other.m_bindless_heaps_valid;
		if (tl_active_recording_context == &other) {
			tl_active_recording_context = this;
		}
		other.m_state_cache.invalidate();
		other.m_cmd = commands{};
		other.m_pass = nullptr;
		other.m_transient_pool = nullptr;
		other.m_bindless_heaps = nullptr;
		other.m_bindless_heaps_valid = false;
	}
	return *this;
}

auto gse::gpu::recording_context::resolve(const transient_image_handle h) const -> const image& {
	assert(m_transient_pool != nullptr, "recording_context::resolve called but no transient_pool is bound");
	const auto* img = m_transient_pool->resolve_image(h);
	assert(
		img != nullptr,
		"transient_image_handle {} could not be resolved; ensure it was declared via gpu::transient_image before this "
		"pass runs",
		h.key
	);
	return *img;
}

auto gse::gpu::recording_context::resolve(const transient_buffer_handle h) const -> const buffer& {
	assert(m_transient_pool != nullptr, "recording_context::resolve called but no transient_pool is bound");
	const auto* buf = m_transient_pool->resolve_buffer(h);
	assert(
		buf != nullptr,
		"transient_buffer_handle {} could not be resolved; ensure it was declared via gpu::transient_buffer before "
		"this pass runs",
		h.key
	);
	return *buf;
}

gse::gpu::recording_context::~recording_context() {
	if (m_cmd.valid()) {
		assert(
			std::this_thread::get_id() == m_origin_thread,
			"recording_context's secondary command buffer is being ended on a thread other than its origin. "
			"This means the rec was held alive across a co_await that was not gpu::pass<...>(ctx) and the coroutine "
			"resumed on a different worker. Scope the rec to end before any non-pass await."
		);
		finalize_pass();
		m_cmd.end();
	}
	if (tl_active_recording_context == this) {
		tl_active_recording_context = nullptr;
	}
}

auto gse::gpu::recording_context::finalize_active_on_current_thread() noexcept -> void {
	auto* active = tl_active_recording_context;
	if (active != nullptr && active->m_cmd.valid()) {
		assert(
			std::this_thread::get_id() == active->m_origin_thread,
			"finalize_active_on_current_thread invoked on a thread that does not own the active rec's secondary; "
			"tl_active_recording_context was corrupted (probably by a rec being held across a non-pass co_await)."
		);
		active->finalize_pass();
		active->m_cmd.end();
		active->m_cmd = commands{};
	}
	tl_active_recording_context = nullptr;
}

auto gse::gpu::recording_context::check_active() const -> void {
	assert(
		m_cmd.valid(),
		"recording_context method called after the pass's secondary was finalized. This happens when the previous rec "
		"was implicitly closed by a subsequent `co_await gpu::pass(...)` (each new pass await closes the prior rec on "
		"the suspending thread to keep VkCommandPool access single-threaded). Use only the rec returned by the most "
		"recent co_await, or scope each rec in its own block."
	);
	assert(
		std::this_thread::get_id() == m_origin_thread,
		"recording_context method called from a different thread than the one that constructed it. This means the rec "
		"was held alive across a co_await that was not gpu::pass<...>(ctx) and the coroutine resumed on a different "
		"worker. Recording onto a secondary from a thread that does not own its command pool is undefined; scope the "
		"rec to end before any non-pass await."
	);
}

auto gse::gpu::recording_context::ensure_descriptor_heaps() -> void {
	if (m_bindless_heaps_valid) {
		return;
	}
	assert(m_bindless_heaps != nullptr, "recording_context has no bindless heaps");
	m_bindless_heaps->bind(m_cmd);
	m_bindless_heaps_valid = true;
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

	constexpr gpu::access_flags write_mask = gpu::access_flag::shader_write | gpu::access_flag::shader_storage_write |
		gpu::access_flag::color_attachment_write | gpu::access_flag::depth_stencil_attachment_write |
		gpu::access_flag::transfer_write | gpu::access_flag::host_write | gpu::access_flag::memory_write |
		gpu::access_flag::acceleration_structure_write;

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
	m_cmd.copy_buffer(
		src.handle(),
		dst.handle(),
		gpu::buffer_copy_region{
			.src_offset = src_offset,
			.dst_offset = dst_offset,
			.size = size
		}
	);
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
				.dst_access = ac::shader_storage_read | ac::shader_storage_write | ac::shader_sampled_read,
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
	const gpu::dependency_info dep{
		.memory_barriers = std::span(&mb, 1)
	};
	m_cmd.pipeline_barrier(dep);
}

auto gse::gpu::recording_context::build_acceleration_structure(const gpu::acceleration_structure_build_geometry_info& build_info, const std::span<const gpu::acceleration_structure_build_range_info* const> range_infos) -> void {
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

auto gse::gpu::recording_context::capture_swapchain(const gpu::swap_chain& swapchain, const gpu::frame& frame, const buffer& dst) const -> void {
	check_active();
	const auto ext = swapchain.extent();
	const auto dst_buffer = dst.handle();
	const auto gpu_image = swapchain.image(frame.image_index());

	const gpu::image_barrier to_transfer{
		.src_stages = gpu::pipeline_stage_flag::color_attachment_output,
		.src_access = gpu::access_flag::color_attachment_write,
		.dst_stages = gpu::pipeline_stage_flag::transfer,
		.dst_access = gpu::access_flag::transfer_read,
		.image = gpu_image,
		.aspects = gpu::image_aspect_flag::color,
	};
	m_cmd.pipeline_barrier(
		gpu::dependency_info{
			.image_barriers = std::span(&to_transfer, 1)
		}
	);

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
	m_cmd.copy_image_to_buffer(
		gpu_image,
		gpu::image_layout::general,
		dst_buffer,
		std::span(
			&gpu_region,
			1
		)
	);

	const gpu::image_barrier back_to_color{
		.src_stages = gpu::pipeline_stage_flag::transfer,
		.src_access = gpu::access_flag::transfer_read,
		.dst_stages = gpu::pipeline_stage_flag::color_attachment_output,
		.dst_access = gpu::access_flag::color_attachment_write | gpu::access_flag::color_attachment_read,
		.image = gpu_image,
		.aspects = gpu::image_aspect_flag::color,
	};
	m_cmd.pipeline_barrier(
		gpu::dependency_info{
			.image_barriers = std::span(&back_to_color, 1)
		}
	);
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
		.image = src_image,
		.aspects = gpu::image_aspect_flag::color,
	};

	const gpu::image_barrier dst_to_transfer{
		.src_stages = {},
		.src_access = {},
		.dst_stages = gpu::pipeline_stage_flag::transfer,
		.dst_access = gpu::access_flag::transfer_write,
		.old_layout = gpu::image_layout::undefined,
		.new_layout = gpu::image_layout::general,
		.image = dst.handle(),
		.aspects = gpu::image_aspect_flag::color,
	};

	const std::array pre_barriers = { src_to_transfer, dst_to_transfer };
	m_cmd.pipeline_barrier(
		gpu::dependency_info{
			.image_barriers = pre_barriers
		}
	);

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
		gpu::image_layout::general,
		dst.handle(),
		gpu::image_layout::general,
		gpu_region,
		gpu::sampler_filter::nearest
	);

	const gpu::image_barrier src_back{
		.src_stages = gpu::pipeline_stage_flag::transfer,
		.src_access = gpu::access_flag::transfer_read,
		.dst_stages = gpu::pipeline_stage_flag::color_attachment_output,
		.dst_access = gpu::access_flag::color_attachment_write | gpu::access_flag::color_attachment_read,
		.image = src_image,
		.aspects = gpu::image_aspect_flag::color,
	};

	const gpu::image_barrier dst_to_read{
		.src_stages = gpu::pipeline_stage_flag::transfer,
		.src_access = gpu::access_flag::transfer_write,
		.dst_stages = gpu::pipeline_stage_flag::compute_shader | gpu::pipeline_stage_flag::fragment_shader,
		.dst_access = gpu::access_flag::shader_sampled_read,
		.image = dst.handle(),
		.aspects = gpu::image_aspect_flag::color,
	};

	const std::array post_barriers = { src_back, dst_to_read };
	m_cmd.pipeline_barrier(
		gpu::dependency_info{
			.image_barriers = post_barriers
		}
	);
}

auto gse::gpu::recording_context::set_viewport(const float x, const float y, const float width, const float height, const float min_depth, const float max_depth) const -> void {
	check_active();
	m_cmd.set_viewport(
		gpu::viewport{
			.x = x,
			.y = y,
			.width = width,
			.height = height,
			.min_depth = min_depth,
			.max_depth = max_depth,
		}
	);
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

template <typename Entry>
auto gse::gpu::recording_context::dispatch(const entry_push_constants_t<Entry>& pc, const binding_args<entry_bindings_pack_t<Entry>>& args, const vec3u groups) const -> void {
	push_data(pc, 0);
	push_data(args, sizeof(entry_push_constants_t<Entry>));
	dispatch(groups.x(), groups.y(), groups.z());
}

template <typename Entry>
auto gse::gpu::recording_context::dispatch(const binding_args<entry_bindings_pack_t<Entry>>& args, const vec3u groups) const -> void {
	push_data(args, 0);
	dispatch(groups.x(), groups.y(), groups.z());
}

template <typename Entry>
auto gse::gpu::recording_context::push_bindings(const entry_push_constants_t<Entry>& pc, const binding_args<entry_bindings_pack_t<Entry>>& args) const -> void {
	push_data(pc, 0);
	push_data(args, sizeof(entry_push_constants_t<Entry>));
}

template <typename Entry>
auto gse::gpu::recording_context::push_bindings(const binding_args<entry_bindings_pack_t<Entry>>& args) const -> void {
	push_data(args, 0);
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

template <typename T>
auto gse::gpu::recording_context::push_data(const T& value, const std::uint32_t offset) const -> void {
	check_active();
	const auto bytes = std::span(reinterpret_cast<const std::byte*>(std::addressof(value)), sizeof(T));
	m_cmd.push_data(offset, bytes);
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
	m_cmd.bind_vertex_buffers(
		0,
		std::span<const gpu::handle<buffer>>(buffers),
		std::span<const gpu::device_size>(offsets)
	);
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
	m_cmd.bind_index_buffer_2(buf.handle(), offset, gpu::whole_size, type);
}

auto gse::gpu::recording_context::set_viewport(const vec2u extent) const -> void {
	check_active();
	m_cmd.set_viewport(
		gpu::viewport{
			.x = 0.0f,
			.y = 0.0f,
			.width = static_cast<float>(extent.x()),
			.height = static_cast<float>(extent.y()),
			.min_depth = 0.0f,
			.max_depth = 1.0f,
		}
	);
}

auto gse::gpu::recording_context::set_scissor(const vec2u extent) const -> void {
	check_active();
	const gse::rect_t<vec2i> sc{ {
		.min = vec2i{ 0, 0 },
		.max = vec2i{ static_cast<int>(extent.x()), static_cast<int>(extent.y()) },
	} };
	m_cmd.set_scissor(sc);
}

auto gse::gpu::recording_context::bind(const gpu::shader_program& p) -> void {
	check_active();

	if (p.is_compute()) {
		m_cmd.bind_shaders(p.stages(), p.shader_handles());
	}
	else {
		constexpr std::array all_graphics_stages = {
			gpu::stage_flag::vertex,
			gpu::stage_flag::fragment,
			gpu::stage_flag::task,
			gpu::stage_flag::mesh,
		};
		std::array<gpu::handle<shader_object>, 4> bound{};
		const auto stages = p.stages();
		const auto handles = p.shader_handles();
		for (std::size_t i = 0; i < all_graphics_stages.size(); ++i) {
			for (std::size_t j = 0; j < stages.size(); ++j) {
				if (stages[j] == all_graphics_stages[i]) {
					bound[i] = handles[j];
					break;
				}
			}
		}
		m_cmd.bind_shaders(all_graphics_stages, bound);
		apply_dynamic_state(p.state());
	}

	ensure_descriptor_heaps();
}

auto gse::gpu::recording_context::apply_dynamic_state(const gpu::dynamic_pipeline_state& s) -> void {
	if (!m_state_cache.topology || *m_state_cache.topology != s.topology) {
		m_cmd.set_topology(s.topology);
		m_state_cache.topology = s.topology;
	}
	if (!m_state_cache.polygon_mode || *m_state_cache.polygon_mode != s.polygon) {
		m_cmd.set_polygon_mode(s.polygon);
		m_state_cache.polygon_mode = s.polygon;
	}
	if (!m_state_cache.cull_mode || *m_state_cache.cull_mode != s.cull) {
		m_cmd.set_cull_mode(s.cull);
		m_state_cache.cull_mode = s.cull;
	}
	if (!m_state_cache.front_face || *m_state_cache.front_face != s.front) {
		m_cmd.set_front_face(s.front);
		m_state_cache.front_face = s.front;
	}
	if (!m_state_cache.depth_test_enable || *m_state_cache.depth_test_enable != s.depth.test) {
		m_cmd.set_depth_test_enable(s.depth.test);
		m_state_cache.depth_test_enable = s.depth.test;
	}
	if (!m_state_cache.depth_write_enable || *m_state_cache.depth_write_enable != s.depth.write) {
		m_cmd.set_depth_write_enable(s.depth.write);
		m_state_cache.depth_write_enable = s.depth.write;
	}
	if (!m_state_cache.depth_compare_op || *m_state_cache.depth_compare_op != s.depth.compare) {
		m_cmd.set_depth_compare_op(s.depth.compare);
		m_state_cache.depth_compare_op = s.depth.compare;
	}
	if (!m_state_cache.depth_bias_enable || *m_state_cache.depth_bias_enable != s.depth_bias_enable) {
		m_cmd.set_depth_bias_enable(s.depth_bias_enable);
		m_state_cache.depth_bias_enable = s.depth_bias_enable;
	}
	if (s.depth_bias_enable) {
		m_cmd.set_depth_bias(s.depth_bias_constant, s.depth_bias_clamp, s.depth_bias_slope);
	}
	if (!m_state_cache.depth_clamp_enable || *m_state_cache.depth_clamp_enable != s.depth_clamp_enable) {
		m_cmd.set_depth_clamp_enable(s.depth_clamp_enable);
		m_state_cache.depth_clamp_enable = s.depth_clamp_enable;
	}
	if (!m_state_cache.rasterizer_discard_enable || *m_state_cache.rasterizer_discard_enable != s.rasterizer_discard_enable) {
		m_cmd.set_rasterizer_discard_enable(s.rasterizer_discard_enable);
		m_state_cache.rasterizer_discard_enable = s.rasterizer_discard_enable;
	}
	if (!m_state_cache.primitive_restart_enable || *m_state_cache.primitive_restart_enable != s.primitive_restart_enable) {
		m_cmd.set_primitive_restart_enable(s.primitive_restart_enable);
		m_state_cache.primitive_restart_enable = s.primitive_restart_enable;
	}
	if (!m_state_cache.alpha_to_coverage_enable || *m_state_cache.alpha_to_coverage_enable != s.alpha_to_coverage_enable) {
		m_cmd.set_alpha_to_coverage_enable(s.alpha_to_coverage_enable);
		m_state_cache.alpha_to_coverage_enable = s.alpha_to_coverage_enable;
	}
	if (!m_state_cache.alpha_to_one_enable || *m_state_cache.alpha_to_one_enable != s.alpha_to_one_enable) {
		m_cmd.set_alpha_to_one_enable(s.alpha_to_one_enable);
		m_state_cache.alpha_to_one_enable = s.alpha_to_one_enable;
	}
	if (!m_state_cache.logic_op_enable || *m_state_cache.logic_op_enable != s.logic_op_enable) {
		m_cmd.set_logic_op_enable(s.logic_op_enable);
		m_state_cache.logic_op_enable = s.logic_op_enable;
	}

	m_cmd.set_rasterization_samples(s.samples);
	m_cmd.set_sample_mask(s.samples, s.sample_mask);
	m_cmd.set_depth_bounds_test_enable(false);
	m_cmd.set_stencil_test_enable(false);
	m_cmd.set_line_width(1.0f);

	if (!s.blend_enables.empty()) {
		m_cmd.set_color_blend_enable(0, s.blend_enables);
	}
	if (!s.blend_equations.empty()) {
		m_cmd.set_color_blend_equation(0, s.blend_equations);
	}
	if (!s.color_write_masks.empty()) {
		m_cmd.set_color_write_mask(0, s.color_write_masks);
	}
	m_cmd.set_vertex_input(s.vertex_bindings, s.vertex_attributes);
}

namespace gse::gpu {
	constexpr auto profile_stats_flags = gpu::pipeline_statistic_flag::input_assembly_vertices |
		gpu::pipeline_statistic_flag::input_assembly_primitives | gpu::pipeline_statistic_flag::clipping_invocations |
		gpu::pipeline_statistic_flag::fragment_shader_invocations;
}

gse::gpu::render_graph::render_graph(gpu::device& device, gpu::swap_chain& swapchain, gpu::frame& frame, gpu::bindless_texture_set* bindless, bindless_heaps* heaps)
	: m_device(std::addressof(device)), m_swapchain(std::addressof(swapchain)), m_frame(std::addressof(frame)), m_bindless(bindless), m_bindless_heaps(heaps), m_transient_pool(device) {
	m_timestamp_period_per_tick = nanoseconds(static_cast<double>(device.timestamp_period()));
	for (auto& q : m_queue_states) {
		q.timeline = gpu::semaphore::create_timeline(device.vulkan_device(), 0);
	}
	swapchain.on_recreate([this] {
		recreate_framebuffer_images();
	});
}

auto gse::gpu::render_graph::create_framebuffer_image(const framebuffer_image_desc& desc, const std::string_view tag) -> image {
	const auto ext = m_swapchain->extent();
	if (ext.x() == 0 || ext.y() == 0) {
		return {};
	}
	auto img = gpu::image::create(
		m_device->allocator(),
		gpu::image_desc{
			.size = ext,
			.format = desc.format,
			.usage = desc.usage,
		},
		tag
	);
	gpu::transition_image_to(*m_device, img);
	return img;
}

auto gse::gpu::render_graph::recreate_framebuffer_images() -> void {
	for (auto& [name, slot] : m_framebuffer_images) {
		slot->img = create_framebuffer_image(slot->desc, name.tag());
	}
}

auto gse::gpu::render_graph::register_framebuffer_image(const id name, const framebuffer_image_desc& desc) -> const image& {
	auto& slot = m_framebuffer_images[name];
	if (!slot) {
		slot = std::make_unique<registered_image>(registered_image{
			.desc = desc,
			.img = create_framebuffer_image(
				desc,
				name.tag()
			),
		});
	}
	return slot->img;
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
	if (!slot.timestamp_pool.valid()) {
		slot.timestamp_pool = gpu::query_pool::create_timestamp(
			m_device->vulkan_device(),
			max_profiled_passes * 2 + 1
		);
	}
	if (allow_stats && !slot.stats_pool.valid()) {
		slot.stats_pool =
			gpu::query_pool::create_pipeline_stats(
				m_device->vulkan_device(),
				max_profiled_passes,
				profile_stats_flags
			);
	}
}

auto gse::gpu::render_graph::read_profile_slot(gpu_profile_slot& slot) -> void {
	if (!slot.results_valid || slot.pass_count == 0) {
		return;
	}

	const std::uint32_t timestamp_count = slot.pass_count * 2 + 1;
	const auto [ts_status, timestamps] =
		slot.timestamp_pool.results<std::uint64_t>(
			0,
			timestamp_count,
			sizeof(std::uint64_t)
		);

	if (ts_status != gpu::query_status::success) {
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
		const auto tid = (queue == gpu::queue_type::compute) ? trace::gpu_compute_virtual_tid : trace::gpu_virtual_tid;

		trace::begin_async_at(gpu_id, key, tid, time_t<std::uint64_t>(start));
		trace::end_async_at(gpu_id, key, tid, time_t<std::uint64_t>(end));

		profile::ingest_gpu_sample(gpu_id, end - start);
	}

	if (slot.stats_issued) {
		constexpr std::uint32_t stats_per_pass = 4;
		const auto [stats_status, stats] =
			slot.stats_pool.results<std::uint64_t>(
				0,
				slot.pass_count,
				sizeof(std::uint64_t) * stats_per_pass
			);

		if (stats_status == gpu::query_status::success) {
			static constexpr std::array<const char*, stats_per_pass> labels{ ":ia_verts",
																			 ":ia_prims",
																			 ":clip_invocs",
																			 ":fs_invocs" };
			for (std::uint32_t i = 0; i < slot.pass_count; ++i) {
				const auto start = static_cast<double>(timestamps[1 + i * 2]) * period + offset;
				const auto pass_name = std::string(slot.pass_types[i].tag());
				for (std::uint32_t s = 0; s < stats_per_pass; ++s) {
					const auto stat_id = find_or_generate_id(pass_name + labels[s]);
					trace::counter_at(
						stat_id,
						static_cast<double>(stats[i * stats_per_pass + s]),
						trace::gpu_stats_virtual_tid,
						time_t<std::uint64_t>(start)
					);
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

template <typename Marker>
auto gse::gpu::render_graph::framebuffer_image(this auto& self) -> auto& {
	const auto& img = self.register_framebuffer_image(trace_id<Marker>(), Marker::desc);
	return img;
}

auto gse::gpu::render_graph::transient_pool(this auto& self) -> auto& {
	return self.m_transient_pool;
}

auto gse::gpu::render_graph::frame_in_progress() const -> bool {
	return m_frame->frame_in_progress();
}

auto gse::gpu::render_graph::execute(frame_request_drain drain) -> void {
	m_pending_aux_submissions.clear();
	m_pending_graphics_extra_waits.clear();

	if (!m_frame->frame_in_progress()) {
		return;
	}

	const auto frame_idx = m_frame->current_frame();
	std::array<gpu::handle<command_buffer>, gpu::queue_type_count> primary_handles;
	std::array<gpu::commands, gpu::queue_type_count> primary_buffers;
	for (std::size_t qi = 0; qi < gpu::queue_type_count; ++qi) {
		primary_handles[qi] = m_frame->command_buffer(static_cast<gpu::queue_type>(qi));
		primary_buffers[qi] = gpu::commands(primary_handles[qi]);
	}
	const auto graphics_family = m_device->queue_family(gpu::queue_type::graphics);
	std::array<bool, gpu::queue_type_count> queue_distinct{};
	for (std::size_t qi = 0; qi < gpu::queue_type_count; ++qi) {
		queue_distinct[qi] = m_device->queue_family(static_cast<gpu::queue_type>(qi)) != graphics_family;
	}
	queue_distinct[static_cast<std::size_t>(gpu::queue_type::graphics)] = true;

	auto effective_queue = [&](const gpu::queue_type requested) -> gpu::queue_type {
		return queue_distinct[static_cast<std::size_t>(requested)] ? requested : gpu::queue_type::graphics;
	};

	const auto image_index = m_frame->image_index();
	const auto swap_extent = m_swapchain->extent();

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

	m_device->reset_worker_command_pools(frame_idx);
	const auto color_format_value = vulkan::format_value(m_swapchain->format());

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

	auto passes = drain.drain_passes();
	auto transient_images = drain.drain_images();
	auto transient_buffers = drain.drain_buffers();

	{
		std::vector<id> pass_kind_order;
		pass_kind_order.reserve(passes.size());
		for (const auto& p : passes) {
			pass_kind_order.push_back(p.pass_type);
		}
		m_transient_pool.plan(frame_idx, transient_images, transient_buffers, pass_kind_order);
	}

	auto pass_queue = [&](const std::size_t pi) -> gpu::queue_type {
		return effective_queue(passes[pi].queue);
	};

	std::vector<gpu::handle<command_buffer>> pass_secondaries;

	auto record_range = [&](const std::size_t start, const std::size_t end) {
		task::parallel_invoke_range(
			start,
			end,
			[&](std::size_t pi) {
				auto& pass = passes[pi];
				const auto queue = pass_queue(pi);
				const bool is_graphics_pass = !pass.color_outputs.empty() || pass.depth_output;
				const bool maybe_issue_stats = timestamps_enabled && stats_enabled && is_graphics_pass;

				const auto worker_idx = task::current_worker();
				assert(worker_idx.has_value(), "graph::record_parallel: thread has no arena slot");
				const auto secondary = m_device->acquire_worker_command_buffer(queue, *worker_idx, frame_idx);

				const auto* depth_target = pass.depth_output ? resolve_depth_target(*pass.depth_output) : nullptr;

				std::vector<const image*> color_targets;
				color_targets.reserve(pass.color_outputs.size());
				std::vector<gpu::image_format_value> color_formats;
				color_formats.reserve(pass.color_outputs.size());
				for (const auto& info : pass.color_outputs) {
					const auto* target = resolve_color_target(info);
					color_targets.push_back(target);
					color_formats.push_back(target ? target->format() : color_format_value);
				}

				const auto depth_attach_format =
					depth_target ? depth_target->format() : vulkan::format_value(gpu::image_format::d32_sfloat);

				const gpu::secondary_inheritance_info inherit_info{
					.render_pass_continue = is_graphics_pass,
					.color_attachment_formats = std::span<const gpu::image_format_value>{ color_formats },
					.depth_attachment_format = pass.depth_output ? depth_attach_format : gpu::image_format_value{ 0 },
					.pipeline_statistics = maybe_issue_stats ? profile_stats_flags : gpu::pipeline_statistic_flags{},
				};

				const gpu::commands sec_cmd(secondary);
				sec_cmd.begin_secondary(inherit_info);
				recording_context rec{
					sec_cmd,
					std::addressof(pass),
					std::addressof(m_transient_pool),
					m_bindless_heaps
				};
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
				for (std::size_t ci = 0; ci < pass.color_outputs.size(); ++ci) {
					const auto* color_img = color_targets[ci];
					if (color_img == nullptr) {
						continue;
					}
					const auto color_ref = resource_ref{
						.ptr = color_img,
						.type = resource_type::image,
					};
					if (pass.color_outputs[ci].op == load_op::load) {
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
				*pass.record_ctx_slot = std::move(rec);
				pass.record_handle.resume();

				pass_secondaries[pi] = secondary;
			},
			trace_id<"render_graph::record_passes">()
		);
	};

	std::size_t round_start = 0;
	while (true) {
		pass_secondaries.resize(passes.size());
		record_range(round_start, passes.size());

		auto more = drain.drain_passes();
		if (more.empty()) {
			break;
		}

		auto more_images = drain.drain_images();
		auto more_buffers = drain.drain_buffers();
		assert(
			more_images.empty(),
			"render_graph: round 2+ passes may not request new transient_image resources (option A); declare all "
			"transients before the first co_await pass(...)"
		);
		assert(
			more_buffers.empty(),
			"render_graph: round 2+ passes may not request new transient_buffer resources (option A); declare all "
			"transients before the first co_await pass(...)"
		);

		round_start = passes.size();
		passes.insert(
			passes.end(),
			std::make_move_iterator(more.begin()),
			std::make_move_iterator(more.end())
		);
	}

	std::array<bool, gpu::queue_type_count> queue_has_work{};
	queue_has_work[static_cast<std::size_t>(gpu::queue_type::graphics)] = true;
	for (const auto& p : passes) {
		queue_has_work[static_cast<std::size_t>(effective_queue(p.queue))] = true;
	}

	auto open_primary = [](const gpu::handle<command_buffer> handle) {
		const gpu::commands cmd(handle);
		cmd.reset();
		cmd.begin();
	};

	for (std::size_t qi = 0; qi < gpu::queue_type_count; ++qi) {
		if (qi == static_cast<std::size_t>(gpu::queue_type::graphics)) {
			continue;
		}
		if (queue_has_work[qi]) {
			open_primary(m_frame->command_buffer(static_cast<gpu::queue_type>(qi)));
		}
	}

	auto setup_timestamps = [&](gpu_profile_slot& s, const gpu::commands& cb, const bool with_stats) {
		ensure_profile_pools(s, with_stats);
		cb.reset_query_pool(s.timestamp_pool.handle(), 0, max_profiled_passes * 2 + 1);
		if (with_stats) {
			cb.reset_query_pool(s.stats_pool.handle(), 0, max_profiled_passes);
		}
		s.cpu_ref = system_clock::now<trace::tick_step>();
		s.frame_counter = m_frames_submitted;
		cb.write_timestamp(gpu::pipeline_stage_flag::all_commands, s.timestamp_pool.handle(), 0);
	};

	if (timestamps_enabled) {
		for (std::size_t qi = 0; qi < gpu::queue_type_count; ++qi) {
			if (!queue_has_work[qi]) {
				continue;
			}
			const auto q = static_cast<gpu::queue_type>(qi);
			const bool with_stats = (q == gpu::queue_type::graphics) && stats_enabled;
			setup_timestamps(m_profile_slots[qi][frame_idx], primary_buffers[qi], with_stats);
		}
	}

	std::vector<std::size_t> sorted;

	{
		trace::scope_guard sg{ gse::trace_id<"graph::plan">() };
		std::unordered_map<id, std::size_t> type_to_index;
		for (std::size_t i = 0; i < passes.size(); ++i) {
			type_to_index[passes[i].pass_type] = i;
		}

		enum class edge_kind : std::uint8_t {
			explicit_after,
			chain,
			write_after_read,
			both_write,
		};

		struct edge {
			std::size_t to;
			edge_kind kind;
		};

		std::vector<std::vector<edge>> adj(passes.size());
		std::vector<std::size_t> in_degree(passes.size(), 0);

		const std::size_t n = passes.size();
		std::vector<std::vector<bool>> reaches(n, std::vector<bool>(n, false));

		auto has_edge = [&](const std::size_t from, const std::size_t to) -> bool {
			for (const auto& e : adj[from]) {
				if (e.to == to) {
					return true;
				}
			}
			return false;
		};

		auto update_reachability_for_new_edge = [&](const std::size_t from, const std::size_t to) {
			for (std::size_t x = 0; x < n; ++x) {
				const bool x_reaches_from = (x == from) || reaches[x][from];
				if (!x_reaches_from) {
					continue;
				}
				for (std::size_t y = 0; y < n; ++y) {
					const bool to_reaches_y = (y == to) || reaches[to][y];
					if (to_reaches_y) {
						reaches[x][y] = true;
					}
				}
			}
		};

		auto add_edge = [&](const std::size_t from, const std::size_t to, const edge_kind kind) {
			if (has_edge(from, to)) {
				return;
			}
			adj[from].push_back({
				.to = to,
				.kind = kind
			});
			++in_degree[to];
			update_reachability_for_new_edge(from, to);
		};

		for (std::size_t i = 0; i < passes.size(); ++i) {
			for (const auto& dep : passes[i].after_passes) {
				if (auto it = type_to_index.find(dep); it != type_to_index.end()) {
					add_edge(it->second, i, edge_kind::explicit_after);
				}
			}
		}

		for (std::size_t i = 0; i < passes.size(); ++i) {
			for (std::size_t j = i + 1; j < passes.size(); ++j) {
				if (passes[i].chain_id.exists() && passes[i].chain_id == passes[j].chain_id) {
					add_edge(i, j, edge_kind::chain);
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
					if (!reaches[j][i]) {
						if (both_write && !reaches[i][j]) {
							const auto a = passes[i].pass_type;
							const auto b = passes[j].pass_type;
							const auto key = std::pair{ std::min(a, b), std::max(a, b) };
							if (m_warned_ambiguous_pairs.insert(key).second) {
								log::println(
									log::level::warning,
									log::category::render,
									"render_graph: passes '{}' and '{}' both write the same resource with no "
									"explicit ordering; recording order picks ('{}' first). Add `.after<>` to "
									"pin the visual order.",
									a.tag(),
									b.tag(),
									a.tag()
								);
							}
						}
						add_edge(i, j, both_write ? edge_kind::both_write : edge_kind::write_after_read);
					}
				}
				else if (j_writes_i_reads) {
					if (!reaches[i][j]) {
						add_edge(j, i, edge_kind::write_after_read);
					}
				}
			}
		}

		sorted.reserve(passes.size());

		std::vector<std::size_t> remaining_in_degree = in_degree;
		std::queue<std::size_t> queue;
		for (std::size_t i = 0; i < passes.size(); ++i) {
			if (remaining_in_degree[i] == 0) {
				queue.push(i);
			}
		}

		while (!queue.empty()) {
			auto front = queue.front();
			queue.pop();
			sorted.push_back(front);
			for (const auto& e : adj[front]) {
				if (--remaining_in_degree[e.to] == 0) {
					queue.push(e.to);
				}
			}
		}

		if (sorted.size() != passes.size()) {
			std::vector<std::uint8_t> state(passes.size(), 0);
			std::vector<std::size_t> stack;
			std::vector<edge_kind> stack_kinds;
			std::vector<std::size_t> cycle_nodes;
			std::vector<edge_kind> cycle_kinds;

			auto find_cycle = [&](this auto& self, const std::size_t u) -> bool {
				state[u] = 1;
				stack.push_back(u);
				for (const auto& e : adj[u]) {
					if (state[e.to] == 1) {
						const auto start_it = std::ranges::find(stack, e.to);
						const auto start_idx = static_cast<std::size_t>(start_it - stack.begin());
						cycle_nodes.assign(start_it, stack.end());
						cycle_nodes.push_back(e.to);
						cycle_kinds.assign(stack_kinds.begin() + start_idx, stack_kinds.end());
						cycle_kinds.push_back(e.kind);
						return true;
					}
					if (state[e.to] == 0) {
						stack_kinds.push_back(e.kind);
						if (self(e.to)) {
							return true;
						}
						stack_kinds.pop_back();
					}
				}
				stack.pop_back();
				state[u] = 2;
				return false;
			};

			for (std::size_t i = 0; i < passes.size() && cycle_nodes.empty(); ++i) {
				if (state[i] == 0) {
					find_cycle(i);
				}
			}

			std::string cycle_str;
			for (std::size_t k = 0; k < cycle_nodes.size(); ++k) {
				if (k > 0) {
					cycle_str += std::format(" --[{}]--> ", cycle_kinds[k - 1]);
				}
				cycle_str += passes[cycle_nodes[k]].pass_type.tag();
			}

			assert(false, "render_graph: cyclic pass dependency graph:\n  {}", cycle_str);
		}
	}

	std::array<std::array<bool, gpu::queue_type_count>, gpu::queue_type_count> queue_waits_on{};
	for (std::size_t i = 0; i < passes.size(); ++i) {
		for (std::size_t j = 0; j < passes.size(); ++j) {
			if (i == j) {
				continue;
			}
			const auto qi = pass_queue(i);
			const auto qj = pass_queue(j);
			if (qi == qj) {
				continue;
			}
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
			if (a == b) {
				continue;
			}
			assert(
				!(queue_waits_on[a][b] && queue_waits_on[b][a]),
				"render_graph: cyclic cross-queue dependency between two queues; split a submission or move the pass"
			);
		}
	}

	{
		trace::scope_guard sg{ gse::trace_id<"graph::record_replay">() };

		auto aspect_for_image = [](const image& img) -> gpu::image_aspect_flags {
			return vulkan::image_aspect_for(img.format());
		};

		auto access_has_write = [](const gpu::access_flags a) -> bool {
			using ac = gpu::access_flag;
			constexpr auto write_mask = ac::shader_storage_write | ac::shader_write | ac::color_attachment_write |
				ac::depth_stencil_attachment_write | ac::transfer_write | ac::host_write | ac::memory_write |
				ac::acceleration_structure_write;
			return static_cast<bool>(a & write_mask);
		};

		auto append_barrier_for_resource = [&](const resource_ref& resource,
											   const gpu::pipeline_stage_flags src_stages,
											   const gpu::access_flags src_access,
											   const gpu::pipeline_stage_flags dst_stages,
											   const gpu::access_flags dst_access,
											   std::vector<gpu::memory_barrier>& memory_out,
											   std::vector<gpu::buffer_barrier>& buffer_out,
											   std::vector<gpu::image_barrier>& image_out) {
			if (!access_has_write(src_access) && !access_has_write(dst_access) && src_stages.bits() == dst_stages.bits()) {
				return;
			}
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
				image_out.push_back({
					.src_stages = src_stages,
					.src_access = src_access,
					.dst_stages = dst_stages,
					.dst_access = dst_access,
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

		struct prev_use_record {
			resource_ref resource;
			gpu::pipeline_stage_flags stages;
			gpu::access_flags access;
		};

		std::array<std::unordered_map<const void*, prev_use_record>, gpu::queue_type_count> latest_writes;
		std::array<std::unordered_map<const void*, std::vector<prev_use_record>>, gpu::queue_type_count> reads_since_write;

		auto append_prev_pass_barriers = [&](const render_pass_data& cur,
											 const gpu::queue_type cur_queue,
											 std::vector<gpu::memory_barrier>& memory_out,
											 std::vector<gpu::buffer_barrier>& buffer_out,
											 std::vector<gpu::image_barrier>& image_out) {
			auto& latest = latest_writes[static_cast<std::size_t>(cur_queue)];
			auto& reads = reads_since_write[static_cast<std::size_t>(cur_queue)];

			for (const auto& [cur_resource, cur_stage, cur_access] : cur.reads) {
				if (!cur_resource.ptr) {
					continue;
				}
				if (const auto it = latest.find(cur_resource.ptr); it != latest.end()) {
					const auto& prev = it->second;
					append_barrier_for_resource(
						prev.resource,
						prev.stages,
						prev.access,
						cur_stage,
						cur_access,
						memory_out,
						buffer_out,
						image_out
					);
				}
			}

			for (const auto& [cur_resource, cur_stage, cur_access] : cur.writes) {
				if (!cur_resource.ptr) {
					continue;
				}
				if (const auto it = latest.find(cur_resource.ptr); it != latest.end()) {
					const auto& prev = it->second;
					append_barrier_for_resource(
						prev.resource,
						prev.stages,
						prev.access,
						cur_stage,
						cur_access,
						memory_out,
						buffer_out,
						image_out
					);
				}
				if (const auto it = reads.find(cur_resource.ptr); it != reads.end()) {
					for (const auto& prev_read : it->second) {
						append_barrier_for_resource(
							prev_read.resource,
							prev_read.stages,
							{},
							cur_stage,
							cur_access,
							memory_out,
							buffer_out,
							image_out
						);
					}
				}
			}

			for (const auto& [cur_resource, cur_stage, cur_access] : cur.writes) {
				if (!cur_resource.ptr) {
					continue;
				}
				latest[cur_resource.ptr] = { cur_resource, cur_stage, cur_access };
				reads.erase(cur_resource.ptr);
			}
			for (const auto& [cur_resource, cur_stage, cur_access] : cur.reads) {
				if (!cur_resource.ptr) {
					continue;
				}
				reads[cur_resource.ptr].push_back({ cur_resource, cur_stage, cur_access });
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
					.new_layout = gpu::image_layout::general,
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
			append_prev_pass_barriers(pass, queue, memory_barriers, buffer_barriers, image_barriers);

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
						if (o.buffer.value == b.buffer.value && o.offset == b.offset && o.size == b.size && o.src_stages.bits() == b.src_stages.bits() && o.dst_stages.bits() == b.dst_stages.bits()) {
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
						if (o.image.value == b.image.value && o.aspects.bits() == b.aspects.bits() && o.base_mip_level == b.base_mip_level && o.level_count == b.level_count && o.base_array_layer == b.base_array_layer && o.layer_count == b.layer_count && o.src_stages.bits() == b.src_stages.bits() && o.dst_stages.bits() == b.dst_stages.bits()) {
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
			const bool is_graphics_pass = !pass.color_outputs.empty() || pass.depth_output;
			const bool issue_stats =
				profile_pass && stats_enabled && is_graphics_pass && queue == gpu::queue_type::graphics;

			const auto marker_domain = (queue == gpu::queue_type::graphics)
				? gpu::device::pass_marker_domain::graphics_queue
				: gpu::device::pass_marker_domain::compute_queue;
			const auto marker_handle = m_device->begin_pass_marker(
				target_handle,
				marker_domain,
				{
					.frame_counter = m_frames_submitted,
					.pass_index = static_cast<std::uint32_t>(si),
					.pass_type = pass.pass_type,
				}
			);

			if (!memory_barriers.empty() || !buffer_barriers.empty() || !image_barriers.empty()) {
				target_primary.pipeline_barrier(
					gpu::dependency_info{
						.memory_barriers = memory_barriers,
						.buffer_barriers = buffer_barriers,
						.image_barriers = image_barriers,
					}
				);
			}

			m_device->checkpoint_pass_marker(target_handle, marker_handle);

			if (profile_pass) {
				target_primary.write_timestamp(
					gpu::pipeline_stage_flags{},
					target_slot.timestamp_pool.handle(),
					1 + pass_index * 2
				);
				target_slot.pass_types.push_back(pass.pass_type);
				target_slot.pass_queues.push_back(queue);
				++target_slot.pass_count;
				if (issue_stats) {
					target_primary.begin_query(target_slot.stats_pool.handle(), pass_index);
					target_slot.stats_issued = true;
				}
			}

			const auto secondary = pass_secondaries[sorted[si]];

			if (is_graphics_pass) {
				std::vector<gpu::rendering_attachment_info> color_attachments;
				color_attachments.reserve(pass.color_outputs.size());
				std::optional<gpu::rendering_attachment_info> depth_att;
				vec2u pass_extent = swap_extent;
				bool extent_set = false;

				for (const auto& info : pass.color_outputs) {
					const auto op = info.op;
					const auto* color_target = resolve_color_target(info);

					gpu::handle<image_view> color_view;
					if (color_target) {
						color_view = color_target->view();
						if (!extent_set) {
							const auto ext = color_target->extent();
							pass_extent = vec2u{ ext.x(), ext.y() };
							extent_set = true;
						}
					}
					else {
						color_view = m_swapchain->image_view(image_index);
					}

					color_attachments.push_back(
						gpu::rendering_attachment_info{
							.image_view = color_view,
							.layout = gpu::image_layout::general,
							.load = op,
							.store = gpu::store_op::store,
							.color_clear_value = info.clear_value,
						}
					);
				}

				if (pass.depth_output) {
					const auto& info = *pass.depth_output;
					const auto op = info.op;
					const auto* depth_target = resolve_depth_target(info);

					gpu::handle<image_view> depth_view;
					if (depth_target) {
						depth_view = depth_target->view();
						if (!extent_set) {
							const auto ext = depth_target->extent();
							pass_extent = vec2u{ ext.x(), ext.y() };
							extent_set = true;
						}
					}
					else {
						depth_view = m_swapchain->depth_image().view();
					}

					depth_att = gpu::rendering_attachment_info{
						.image_view = depth_view,
						.layout = gpu::image_layout::general,
						.load = op,
						.store = gpu::store_op::store,
						.depth_clear_value = info.clear_value,
					};
				}

				const gpu::rendering_info ri{
					.render_area = gse::rect_t<vec2i>({
						.min = vec2i{ 0, 0 },
						.max = vec2i{ static_cast<int>(pass_extent.x()), static_cast<int>(pass_extent.y()) }
					}),
					.layer_count = 1,
					.color_attachments = color_attachments,
					.depth_attachment = depth_att ? &*depth_att : nullptr,
					.secondary_command_buffers = true,
				};
				target_primary.begin_rendering(ri);
				target_primary.execute_commands(secondary);
				target_primary.end_rendering();
			}
			else {
				target_primary.execute_commands(secondary);
			}

			m_device->post_renderpass_pass_marker(target_handle, marker_handle);

			if (profile_pass) {
				if (issue_stats) {
					target_primary.end_query(target_slot.stats_pool.handle(), pass_index);
				}
				target_primary.write_timestamp(
					gpu::pipeline_stage_flag::all_commands,
					target_slot.timestamp_pool.handle(),
					2 + pass_index * 2
				);
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
		if (qi == static_cast<std::size_t>(gpu::queue_type::graphics)) {
			continue;
		}
		if (!queue_has_work[qi]) {
			continue;
		}

		const auto q = static_cast<gpu::queue_type>(qi);
		const auto handle = m_frame->command_buffer(q);
		gpu::commands(handle).end();

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
			if (producer == qi) {
				continue;
			}
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
		if (producer == graphics_qi) {
			continue;
		}
		if (queue_waits_on[graphics_qi][producer] && this_frame_signal_values[producer] > 0) {
			m_pending_graphics_extra_waits.push_back({
				.semaphore = m_queue_states[producer].timeline.handle(),
				.value = this_frame_signal_values[producer],
				.stages = gpu::pipeline_stage_flag::all_commands,
			});
		}
	}
}

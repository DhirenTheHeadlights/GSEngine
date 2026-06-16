export module gse.gpu:render_graph;

import std;

import :device;
import :swap_chain;
import :frame;
import :pipeline_builder;
import :transient_pool;
import :image;
import :pass_recorder;

import gse.gpu_backend;
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

		gpu::pass_recorder m_recorder;
		render_pass_data* m_pass = nullptr;
		const gpu::transient_pool* m_transient_pool = nullptr;
		gpu::device* m_device = nullptr;
		std::vector<touched_resource> m_touched;
		std::thread::id m_origin_thread;
		gpu::pipeline_state_cache m_state_cache;
		bool m_bindless_heaps_valid = false;

		recording_context(
			pass_recorder rec,
			render_pass_data* pass,
			const gpu::transient_pool* transient_pool,
			gpu::device* device
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
		std::move_only_function<std::vector<render_pass_data>()> drain_passes;
		std::move_only_function<std::vector<transient_image_request>()> drain_images;
		std::move_only_function<std::vector<transient_buffer_request>()> drain_buffers;
	};

	class render_graph {
	public:
		explicit render_graph(
			gpu::device& device,
			gpu::swap_chain* swapchain,
			gpu::frame& frame
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

		auto set_swapchain_clear(
			gpu::color_clear value,
			load_op op = load_op::clear
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
			gpu::handle<gpu::query_pool> timestamp_pool;
			gpu::handle<gpu::query_pool> stats_pool;
			std::vector<id> pass_types;
			std::vector<gpu::queue_type> pass_queues;
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
			gpu::queue_timeline<gpu::device> timeline;
			std::uint64_t signal_counter = 0;
		};

		gpu::device* m_device;
		gpu::swap_chain* m_swapchain;
		gpu::frame* m_frame;
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
		gpu::color_clear m_swapchain_clear{};
		load_op m_swapchain_load = load_op::clear;
	};
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

template <typename T>
auto gse::gpu::recording_context::push_data(const T& value, const std::uint32_t offset) const -> void {
	check_active();
	const auto bytes = std::span(reinterpret_cast<const std::byte*>(std::addressof(value)), sizeof(T));
	m_recorder.push_data(offset, bytes);
}

template <typename Marker>
auto gse::gpu::render_graph::framebuffer_image(this auto& self) -> auto& {
	const auto& img = self.register_framebuffer_image(trace_id<Marker>(), Marker::desc);
	return img;
}

auto gse::gpu::render_graph::depth_image(this auto& self) -> auto& {
	return self.m_swapchain->depth_image();
}

auto gse::gpu::render_graph::transient_pool(this auto& self) -> auto& {
	return self.m_transient_pool;
}


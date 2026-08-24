export module gse.gpu:render_graph;

import std;

import :device;
import :swap_chain;
import :frame;
import :transient_pool;
import :image;
import :pass_recorder;
import :graph_channel;

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
	struct render_pass_data;

	struct color_output_info {
		bool is_swapchain = false;
		gse::id window;
		const image* custom_target = nullptr;
		transient_image_handle transient_target;
		load_op op = load_op::clear;
		color_clear clear_value;
	};

	struct depth_output_info {
		bool is_swapchain = false;
		const image* custom_target = nullptr;
		transient_image_handle transient_target;
		load_op op = load_op::clear;
		depth_clear clear_value;
	};

	struct resource_usage {
		resource_ref resource;
		pipeline_stage_flags stage;
		access_flags access;
	};

	struct framebuffer_image_desc {
		image_format format = image_format::r8g8b8a8_unorm;
		image_usage usage;
		image_aspect_flags aspects = image_aspect_flag::color;
	};


	struct render_pass_data {
		id pass_type{};
		std::string_view pass_name{};
		queue_type queue = queue_type::graphics;
		const shader_program* primary_pipeline = nullptr;
		std::vector<resource_usage> reads;
		std::vector<resource_usage> writes;
		std::vector<id> after_passes;
		id chain_id;
		bool early_signal = false;
		std::vector<color_output_info> color_outputs;
		std::optional<depth_output_info> depth_output;
		std::coroutine_handle<> record_handle;
		void* record_ctx_slot = nullptr;
	};

	struct rec_touch {
		resource_ref ref;
		pipeline_stage_flags stages;
		access_flags access;
	};

	struct recording_context_init {
		pass_recorder recorder;
		render_pass_data* pass = nullptr;
		const gpu::transient_pool* transient_pool = nullptr;
		gpu::device* device = nullptr;
		const shader_program* primary = nullptr;
		std::vector<rec_touch> touches;
	};

	struct frame_request_drain {
		std::move_only_function<std::vector<render_pass_data>()> drain_passes;
		std::move_only_function<std::vector<transient_image_request>()> drain_images;
		std::move_only_function<std::vector<transient_buffer_request>()> drain_buffers;
	};

	class render_graph {
	public:
		explicit render_graph(
			device& device,
			frame& frame
		);

		~render_graph();

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
			color_clear value,
			load_op op = load_op::clear
		) -> void;

		auto set_offscreen_target(
			const image* target
		) -> void;

		[[nodiscard]] auto current_frame() const -> std::uint32_t;

		[[nodiscard]] auto extent() const -> vec2u;

		[[nodiscard]] auto extent(
			gse::id window
		) const -> vec2u;

		[[nodiscard]] auto depth_image(
			this auto& self
		) -> auto&;

		auto register_framebuffer_image(
			id name,
			const framebuffer_image_desc& desc
		) -> const image&;

		[[nodiscard]] auto create_readback_channel(
			std::size_t size,
			std::string_view tag = {}
		) const -> readback_channel;

		[[nodiscard]] auto create_upload_channel(
			const buffer_desc& desc,
			std::string_view tag = {}
		) const -> upload_channel;

		template <typename Marker>
		auto framebuffer_image(
			this auto& self
		) -> auto&;

		[[nodiscard]] auto frame_in_progress() const -> bool;

		[[nodiscard]] auto take_aux_submissions() -> std::vector<queue_submission>;

		[[nodiscard]] auto take_graphics_extra_waits() -> std::vector<semaphore_submit_info>;

		auto add_graphics_signal(
			semaphore_submit_info signal
		) -> void;

		auto add_graphics_wait(
			semaphore_submit_info wait
		) -> void;

		[[nodiscard]] auto take_graphics_extra_signals() -> std::vector<semaphore_submit_info>;

		[[nodiscard]] auto take_graphics_buffers() -> std::vector<command_buffer_handle>;

	private:
		static constexpr std::uint32_t max_profiled_passes = 128;
		static constexpr std::uint32_t stats_per_pass = 4;

		struct gpu_profile_slot {
			handle<query_pool> timestamp_pool;
			handle<query_pool> stats_pool;
			std::vector<id> pass_types;
			std::vector<queue_type> pass_queues;
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

		auto log_pass_graph(
			std::span<const render_pass_data> passes
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
			queue_timeline<device> timeline;
			std::uint64_t signal_counter = 0;
		};

		static int s_live_count;

		device* m_device;
		swap_chain* m_swapchain;
		frame* m_frame;
		gpu::transient_pool m_transient_pool;
		std::unordered_map<id, std::unique_ptr<registered_image>> m_framebuffer_images;
		interval_timer<> m_graph_report{ seconds(5.f) };
		std::size_t m_graph_report_hash = 0;
		std::array<per_frame_resource<gpu_profile_slot>, queue_type_count> m_profile_slots{
			per_frame_resource<gpu_profile_slot>{ gpu_profile_slot{}, gpu_profile_slot{} },
			per_frame_resource<gpu_profile_slot>{ gpu_profile_slot{}, gpu_profile_slot{} },
			per_frame_resource<gpu_profile_slot>{ gpu_profile_slot{}, gpu_profile_slot{} },
		};
		std::atomic<bool> m_gpu_timestamps_enabled{ true };
		std::atomic<bool> m_gpu_pipeline_stats_enabled{ false };
		time_t<double> m_timestamp_period_per_tick = nanoseconds(1.0);
		std::uint64_t m_frames_submitted = 0;
		std::array<queue_state, queue_type_count> m_queue_states;
		std::vector<queue_submission> m_pending_aux_submissions;
		std::vector<semaphore_submit_info> m_pending_graphics_extra_waits;
		std::vector<semaphore_submit_info> m_pending_graphics_extra_signals;
		std::vector<command_buffer_handle> m_pending_graphics_buffers;
		std::set<std::pair<id, id>> m_warned_ambiguous_pairs;
		std::set<std::pair<std::size_t, std::size_t>> m_warned_queue_cycles;
		std::set<std::pair<std::size_t, std::size_t>> m_warned_dropped_waits;
		std::unordered_map<id, std::array<id, stats_per_pass>> m_stat_ids;
		color_clear m_swapchain_clear{};
		load_op m_swapchain_load = load_op::clear;
		const image* m_offscreen_target = nullptr;
	};
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

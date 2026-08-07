export module gse.runtime:engine;

import std;

import gse.core;
import gse.math;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.introspection;
import gse.network;
import gse.graphics;
import gse.audio;
import gse.physics;
import gse.os;
import gse.gpu;
import gse.log;
import gse.save;
import gse.config;
import gse.scenario;

import :scene;
import :world_system;

export namespace gse {
	using engine_networked_components =
		type_pack<physics::motion_component, physics::collision_component, render_component, player_controller>;

	struct bench_config {
		bool enabled = false;
		int warmup_frames = 120;
		int frames = 600;
		std::string scene;
		std::string scenario;
		std::string profile_out;
		std::string summary_out;
		scenario::body_fn scenario_body = nullptr;
	};

	struct engine_config {
		std::string title = "GSEngine Application";
		bool create_window = true;
		bool render = true;
		bool render_world = true;
		bool simulate_world = true;
		bool use_gpu_solver = false;
		bool custom_chrome = false;
		bool scale_ui_with_resolution = true;
		bool persist_settings = true;
		std::filesystem::path gui_layout_path;
		std::filesystem::path project_settings_path;
		bool attached = false;
		std::string ipc_pipe_name;
		std::uint32_t parent_pid = 0;
		std::string dump_system_graph_path;
		bench_config bench;
	};

	constexpr std::uint32_t attached_surface_magic = 0x47535334;
	constexpr std::size_t attached_ring_size = 3;

	struct attached_surface_message {
		std::uint32_t magic = 0;
		std::uint32_t pid = 0;
		vec2u extent{ 0, 0 };
		gpu::image_format format = gpu::image_format::r8g8b8a8_unorm;
		gpu::backend_kind backend = gpu::backend_kind::vulkan;
		void* surface_handles[attached_ring_size] = {};
		void* produced_semaphore_handle = nullptr;
		void* consumed_semaphore_handle = nullptr;
	};

	constexpr std::uint32_t attached_input_magic = 0x47535332;

	struct attached_input_message {
		std::uint32_t magic = 0;
		input::event event;
	};

	constexpr std::uint32_t attached_pacing_magic = 0x47535333;

	struct attached_pacing_message {
		std::uint32_t magic = 0;
		time_t<std::uint64_t> refresh{};
	};

	class engine : public identifiable {
	public:
		using setup_fn = std::function<void(engine&)>;

		engine(
			const engine_config& config
		);

		auto initialize(
			const setup_fn& app_setup = {}
		) -> void;

		auto update() -> void;

		auto render() -> void;

		auto shutdown() -> void;

		auto make_channel_writer() -> channel_writer;

		auto registry() -> gse::registry&;

		auto world() -> world_system::data&;

		auto window_should_close() -> bool;

		auto tick_window() -> void;

		auto attached_surface_ready() const -> bool;

		auto abandon_attach() -> void;

		auto attached_message() const -> const attached_surface_message&;

		auto push_attached_input(
			const input::event& event
		) -> void;

		[[nodiscard]] auto snapshot_graph() const -> introspection::system_graph;

		[[nodiscard]] auto all_settled() const -> bool;

		[[nodiscard]] auto world_state_hash() -> std::uint64_t;

		[[nodiscard]] auto world_populated() -> bool;

		auto add_system_node(
			system_node node
		) -> void;

		template <typename T>
		auto register_external_resource(
			T* resource
		) -> void;

	private:
		auto destroy_attached_surface(
			gpu::device& device
		) -> void;

		engine_config m_config;
		scheduler m_scheduler;
		save::registry m_save;
		primitives::data m_primitives;
		gse::registry m_registry;
		loading::state m_loading;
		std::function<void()> m_deferred_boot;
		std::atomic<bool> m_boot_tasks_done = false;
		std::uint32_t m_boot_init_baseline_settled = 0;
		bool m_boot_init_baseline_captured = false;
		std::uint32_t m_frames_since_rendered = 0;
		bool m_window_shown = false;
		std::array<gpu::shared_surface, attached_ring_size> m_attached_surfaces{};
		std::array<gpu::image, attached_ring_size> m_attached_surface_images;
		gpu::handle<gpu::semaphore> m_attached_produced_semaphore{};
		gpu::handle<gpu::semaphore> m_attached_consumed_semaphore{};
		attached_surface_message m_attached_message{};
		std::uint64_t m_attached_counter = 0;
		bool m_attached_surface_ready = false;
		bool m_attached_surface_attempted = false;
	};
}

template <typename T>
auto gse::engine::register_external_resource(T* resource) -> void {
	m_scheduler.register_external_resource<T>(resource);
}

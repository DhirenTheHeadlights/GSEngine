export module gse.ecs:system_node;

import std;

import gse.core;
import gse.concurrency;

import :phase_context;
import :registries;
import :run_context;
import :frame_context;

export namespace gse {
	template <typename S>
	concept names_state = requires { typename S::state; };

	template <typename S>
	concept has_resources = requires { typename S::resources; };

	template <typename S>
	concept has_update_data = requires { typename S::update_data; };

	template <typename S>
	concept has_frame_data = requires { typename S::frame_data; };

	template <typename S>
	concept has_settings = requires { typename S::settings; };

	template <typename S>
	concept names_run = requires { &S::run; };

	template <typename S>
	concept names_shutdown = requires { &S::shutdown; };

	template <typename S>
	concept names_frame = requires { &S::frame; };

	struct system_node : non_copyable {
		~system_node(
		) = default;

		system_node(
		) = default;

		system_node(
			system_node&&
		) noexcept = default;

		auto operator=(
			system_node&&
		) noexcept -> system_node& = default;

		std::unique_ptr<void, void(*)(void*)> data{ nullptr, nullptr };

		void (*invoke_shutdown_fn)(shutdown_context&, void*) = nullptr;
		auto (*invoke_run_fn)(run_context&, void*) -> async::task<> = nullptr;
		auto (*invoke_frame_fn)(frame_context&, void*) -> async::task<> = nullptr;
		void (*invoke_snapshot_fn)(void*) = nullptr;
		void (*invoke_apply_settings_fn)(void*, channel_registry&, channel_writer&) = nullptr;

		std::vector<id> run_state_deps;
		std::vector<id> frame_state_deps;

		void* state_ptr = nullptr;
		const void* state_snapshot_ptr = nullptr;
		void* resources_ptr = nullptr;
		void* settings_ptr = nullptr;
		const void* settings_snapshot_ptr = nullptr;

		bool has_frame = false;
		bool is_in_update_loop = false;
		bool run_launched = false;

		std::unique_ptr<async::manual_event> tick_event;
		std::unique_ptr<async::manual_event> tick_done_event;
		std::unique_ptr<channel_writer> tick_writer;
		std::unique_ptr<run_context> tick_ctx;
		async::task<> run_task;

		id state_id;
		id state_type_id;
		id resources_id;
		id settings_id;
		id update_wall_id;
		id frame_wall_id;
		id trace_id;
	};

	template <typename S, typename... Args>
	auto make_system_node(
		Args&&... args
	) -> system_node;
}

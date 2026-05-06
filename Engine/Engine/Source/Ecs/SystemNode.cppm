export module gse.ecs:system_node;

import std;

import gse.core;
import gse.concurrency;

import :phase_context;
import :registries;
import :update_context;
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
	concept names_initialize = requires { &S::initialize; };

	template <typename S>
	concept names_update = requires { &S::update; };

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

		void (*invoke_initialize_fn)(init_context&, void*) = nullptr;
		void (*invoke_shutdown_fn)(shutdown_context&, void*) = nullptr;
		auto (*invoke_update_fn)(update_context&, void*) -> async::task<> = nullptr;
		auto (*invoke_frame_fn)(frame_context&, void*) -> async::task<> = nullptr;
		void (*invoke_snapshot_fn)(void*) = nullptr;
		void (*invoke_apply_settings_fn)(void*, channel_registry&, channel_writer&) = nullptr;

		std::vector<id> init_state_deps;
		std::vector<id> update_state_deps;
		std::vector<id> frame_state_deps;

		void* state_ptr = nullptr;
		const void* state_snapshot_ptr = nullptr;
		void* resources_ptr = nullptr;
		void* settings_ptr = nullptr;
		const void* settings_snapshot_ptr = nullptr;

		bool has_frame = false;
		bool initialized = false;

		id state_id;
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

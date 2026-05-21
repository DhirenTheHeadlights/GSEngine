export module gse.ecs:system_node;

import std;
import gse.std_meta;

import gse.core;
import gse.concurrency;
import gse.meta;

import :phase_context;
import :registries;
import :run_context;
import :frame_context;
import :settings;

export namespace gse {
	template <typename S>
	concept names_data = requires { typename S::data; };

	template <typename T>
	consteval auto has_describe_fields() -> bool;

	template <typename S>
	concept has_settings = names_data<S> && has_describe_fields<typename S::data>();

	template <typename S>
	concept names_run = requires { &S::run; };

	template <typename S>
	concept names_shutdown = requires { &S::shutdown; };

	template <typename S>
	concept names_frame = requires { &S::frame; };

	struct system_node : non_copyable {
		~system_node() = default;

		system_node() = default;

		system_node(system_node&&) noexcept = default;

		auto operator=(system_node&&) noexcept -> system_node& = default;

		std::unique_ptr<void, void (*)(void*)> data{ nullptr, nullptr };

		void (*invoke_shutdown_fn)(shutdown_context&, void*) = nullptr;
		auto (*invoke_run_fn)(run_context&, void*) -> async::task<> = nullptr;
		auto (*invoke_frame_fn)(frame_context&, void*) -> async::task<> = nullptr;
		void (*invoke_snapshot_fn)(void*) = nullptr;
		void (*invoke_apply_settings_fn)(void*, channel_registry&, channel_writer&) = nullptr;

		std::vector<id> run_state_deps;
		std::vector<id> frame_state_deps;

		void* state_ptr = nullptr;
		const void* state_snapshot_ptr = nullptr;

		bool has_frame = false;
		bool is_in_update_loop = false;
		bool run_launched = false;
		bool settled = false;
		bool advance_in_flight = false;

		std::unique_ptr<async::manual_event> resume_event;
		std::unique_ptr<async::manual_event> paused_event;
		std::unique_ptr<channel_writer> tick_writer;
		std::unique_ptr<run_context> tick_ctx;
		async::task<> run_task;

		id state_id;
		id state_type_id;
		id update_wall_id;
		id frame_wall_id;
		id frame_start_id;
		id trace_id;

		std::optional<settings::register_settings_type> settings_record;
	};

	template <typename S, typename... Args>
	auto make_system_node(Args&&... args) -> system_node;
}

template <typename T>
consteval auto gse::has_describe_fields() -> bool {
	bool found = false;
	template for (
		constexpr auto m : std::define_static_array(
			std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked())
		)
	) {
		if constexpr (meta::find_describe(m) != std::meta::info{}) {
			found = true;
		}
	}
	return found;
}

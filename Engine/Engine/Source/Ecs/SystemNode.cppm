export module gse.ecs:system_node;

import std;

import gse.core;
import gse.concurrency;
import gse.meta;

import :registries;
import :context;
import :context;
import :settings;
import :traits;

export namespace gse {
	template <typename T>
	consteval auto has_describe_fields() -> bool;

	template <typename T>
	consteval auto has_category_annotation() -> bool;

	struct system_node : non_copyable {
		~system_node() = default;

		system_node() = default;

		system_node(
			system_node&&
		) noexcept = default;

		auto operator=(
			system_node&&
		) noexcept -> system_node& = default;

		std::unique_ptr<void, void (*)(void*)> data{ nullptr, nullptr };

		void (
			*invoke_shutdown_fn
		)(
			void*
		) = nullptr;
		auto (
			*invoke_run_fn
		)(
			context&,
			void*
		) -> async::task<> = nullptr;
		auto (
			*invoke_init_fn
		)(
			context&,
			void*
		) -> async::task<> = nullptr;
		auto (
			*invoke_frame_fn
		)(
			context&,
			void*
		) -> async::task<> = nullptr;
		void (
			*invoke_snapshot_fn
		)(
			void*
		) = nullptr;
		void (
			*invoke_apply_settings_fn
		)(
			void*,
			channel_registry&,
			channel_writer&
		) = nullptr;

		std::vector<id> run_state_deps;
		std::vector<id> init_state_deps;
		std::vector<id> frame_state_deps;
		std::vector<id> optional_run_state_deps;
		std::vector<id> optional_init_state_deps;
		std::vector<id> component_reads;
		std::vector<id> component_writes;
		std::vector<id> component_structural;
		std::vector<id> shared_view_reads;

		void* state_ptr = nullptr;
		const void* state_snapshot_ptr = nullptr;

		bool has_frame = false;
		bool entity_structural = false;
		bool deferred = false;
		bool init_launched = false;
		bool init_done = false;
		bool init_in_flight = false;
		bool ran_once = false;

		std::unique_ptr<async::manual_event> resume_event;
		std::unique_ptr<async::manual_event> paused_event;
		std::unique_ptr<channel_writer> init_writer;
		std::unique_ptr<context> init_ctx;
		async::task<> init_task;

		id state_id;
		id state_type_id;
		id update_wall_id;
		id frame_wall_id;
		id frame_start_id;
		id trace_id;

		std::optional<settings::register_settings_type> settings_record;

		std::string system_name;
		std::string display_name;
		std::string def_file;
		std::uint32_t def_line = 0;
		std::uint32_t def_column = 0;
	};

}

template <typename T>
consteval auto gse::has_describe_fields() -> bool {
	bool found = false;
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
		if constexpr (meta::find_describe(m) != std::meta::info{}) {
			found = true;
		}
	}
	return found;
}

template <typename T>
consteval auto gse::has_category_annotation() -> bool {
	return meta::find_category(^^T) != std::meta::info{};
}

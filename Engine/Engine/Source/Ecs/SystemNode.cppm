export module gse.ecs:system_node;

import gse.concurrency;
import gse.core;
import gse.meta;
import std;

import :context;
import :registries;
import :registry;
import :settings;

export namespace gse {
	template <typename T>
	consteval auto has_describe_fields() -> bool;

	template <typename T>
	consteval auto has_bind_fields() -> bool;

	enum class dep_kind : std::uint8_t {
		declared,
		lend,
		pinned,
		structural,
		reordered_read,
		output
	};

	struct dep_reason {
		dep_kind kind = dep_kind::declared;
		id via;

		auto operator==(
			const dep_reason& other
		) const -> bool = default;
	};

	struct run_dep {
		id state;
		std::vector<dep_reason> reasons;

		[[nodiscard]] auto kind() const -> dep_kind;
	};

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
		void (
			*invoke_persist_fn
		)(
			const void*
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
			channel_registry&
		) = nullptr;

		std::vector<void (registry::*)()> ensure_storage_fns;
		std::vector<id> declared_run_state_deps;
		std::vector<id> run_order_deps;
		std::vector<run_dep> run_deps;
		std::vector<id> init_state_deps;
		std::vector<id> frame_state_deps;
		std::vector<id> optional_run_state_deps;
		std::vector<id> optional_init_state_deps;
		std::vector<id> component_reads;
		std::vector<id> component_writes;
		std::vector<id> resource_reads;
		std::vector<id> resource_writes;
		std::vector<id> shared_view_reads;
		std::vector<id> channel_publishes;
		std::vector<id> channel_consumes;
		std::vector<id> lent_channels;

		void* state_ptr = nullptr;
		const void* state_snapshot_ptr = nullptr;

		bool entity_structural = false;
		bool deferred = false;
		bool init_launched = false;
		bool init_done = false;
		bool init_in_flight = false;
		bool ran_once = false;

		std::unique_ptr<init_yield> yield;
		std::unique_ptr<context> init_ctx;
		async::task<> init_task;

		id state_id;
		id update_wall_id;
		id frame_wall_id;
		id frame_start_id;
		id trace_id;

		std::optional<settings::register_settings_type> settings_record;
		std::vector<actions::registration> action_records;
		std::vector<actions::axis_registration> axis_records;

		std::string system_name;
		std::string display_name;
		std::string def_file;
		std::uint32_t def_line = 0;
		std::uint32_t def_column = 0;

		[[nodiscard]] auto required_deps() const -> std::vector<id>;
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

auto gse::run_dep::kind() const -> dep_kind {
	return std::ranges::min(reasons, {}, &dep_reason::kind).kind;
}

auto gse::system_node::required_deps() const -> std::vector<id> {
	std::vector<id> deps = declared_run_state_deps;
	deps.insert(deps.end(), init_state_deps.begin(), init_state_deps.end());
	deps.insert(deps.end(), frame_state_deps.begin(), frame_state_deps.end());
	return deps;
}

template <typename T>
consteval auto gse::has_bind_fields() -> bool {
	bool found = false;
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
		if constexpr (meta::is_action_member(m) || meta::find_axis2(m) != std::meta::info{} || meta::find_axis2_mouse(m) != std::meta::info{} || meta::find_axis1(m) != std::meta::info{} || meta::find_axis1_scroll(m) != std::meta::info{} || has_annotation<actions::set>(m)) {
			found = true;
		}
	}
	return found;
}
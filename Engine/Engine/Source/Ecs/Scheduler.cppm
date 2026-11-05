export module gse.ecs:scheduler;

import gse.assert;
import gse.concurrency;
import gse.core;
import gse.diag;
import gse.introspection;
import gse.math;
import gse.meta;
import gse.time;
import std;

import :access;
import :context;
import :registries;
import :registry;
import :settings;
import :system_node;
import :task_graph;

namespace gse {
	enum class wait_phase : std::uint8_t {
		init,
		update,
		frame
	};
}

export namespace gse {
	enum class scheduler_phase : std::uint8_t {
		boot,
		running,
		shutdown
	};

	class scheduler {
	public:
		scheduler() = default;

		auto set_registry(
			registry& reg
		) -> void;

		auto set_settings_register_hook(
			std::function<void(settings::register_settings_type)> fn
		) -> void;

		auto set_actions_register_hook(
			std::function<void(std::vector<actions::registration>, std::vector<actions::axis_registration>)> fn
		) -> void;

		auto enter_running() -> void;

		auto enter_shutdown() -> void;

		auto initialize() -> void;

		[[nodiscard]] auto all_settled() const -> bool;

		struct settle_stats {
			std::uint32_t settled = 0;
			std::uint32_t total = 0;
		};

		[[nodiscard]] auto settle_progress() const -> settle_stats;

		[[nodiscard]] auto snapshot_graph() const -> introspection::system_graph;

		auto update() -> void;

		auto render(
			bool frame_ok,
			const std::function<void()>& in_frame = {}
		) -> void;

		auto persist() const -> void;

		auto shutdown() -> void;

		auto clear() -> void;

		auto add_system_node(
			system_node node
		) -> void;

		template <typename T>
		auto register_external_resource(
			T* ptr
		) -> void;

		template <typename State>
		auto state() -> State&;

		template <typename State>
		auto try_state_of() -> State*;

		template <typename T>
		auto read_channel() -> std::span<const T>;

		auto make_channel_writer() -> channel_writer;

		auto flip_channels() -> void;

		auto begin_staging() -> void;

		auto resolve_activation(
			const std::unordered_set<id>& disabled_roots
		) -> void;

		auto register_deferred() -> void;

		template <typename T>
		requires is_same_frame_channel_v<T>
		auto drain_channel() -> std::vector<T>;

		auto set_stall_probe(
			std::function<void()> probe
		) -> void;

		auto set_init_stall_dump(
			time interval
		) -> void;

	private:
		static auto persist_node(
			const system_node& node
		) -> void;

		auto register_node(
			system_node node
		) -> void;

		auto wire_component_deps() -> void;

		auto promote_optional_deps() -> void;

		auto check_state_dep_cycles() -> void;

		auto check_closed_dep_graph() -> void;

		auto run_init_phase() -> void;

		auto advance_inits() -> void;

		auto dispatch_run_systems() -> void;

		auto advance_one_init_system(
			system_node& node
		) -> async::task<>;

		auto run_node_update(
			context& ctx,
			system_node& node
		) -> async::task<>;

		auto run_node_frame(
			context& ctx,
			system_node& node
		) -> async::task<>;

		[[nodiscard]] auto dep_init_done(
			id dep
		) const -> bool;

		[[nodiscard]] auto dispatchable_nodes() const -> std::vector<bool>;

		auto drain_hot_add_queue() -> void;

		auto snapshot_all_states() -> void;

		auto sync_wait_or_dump(
			std::vector<async::task<>>&& tasks,
			wait_phase phase
		) -> void;

		auto log_stall_state(
			wait_phase phase,
			time_t<float> elapsed,
			int dump_count
		) -> void;

		std::deque<system_node> m_nodes;
		std::unordered_map<id, std::size_t> m_node_index;
		std::vector<std::unique_ptr<context>> m_run_contexts;
		std::vector<system_node> m_candidates;
		std::vector<system_node> m_deferred_nodes;
		bool m_staging = false;
		scheduler_phase m_phase = scheduler_phase::boot;
		state_registry m_states;
		std::unordered_set<std::size_t> m_logged_cycle_warnings;
		resource_registry m_resources_store;
		channel_registry m_channels_store;
		std::vector<system_node> m_hot_add_queue;
		mutable std::mutex m_hot_add_mutex;
		registry* m_registry = nullptr;
		std::function<void(settings::register_settings_type)> m_settings_register_hook;
		std::function<void(std::vector<actions::registration>, std::vector<actions::axis_registration>)> m_actions_register_hook;
		std::function<void()> m_stall_probe;
		time m_init_stall_dump{};
		task_graph m_update_graph;
		task_graph m_frame_graph;
		bool m_dep_graph_checked = false;
	};
}

template <typename State>
auto gse::scheduler::state() -> State& {
	auto* p = m_states.state_ptr(id_of<State>());
	assert(p != nullptr, "state not found");
	return *static_cast<State*>(p);
}

template <typename State>
auto gse::scheduler::try_state_of() -> State* {
	return static_cast<State*>(m_states.state_ptr(id_of<State>()));
}

template <typename T>
auto gse::scheduler::register_external_resource(T* ptr) -> void {
	m_resources_store.register_resource(id_of<T>(), ptr);
}

template <typename T>
auto gse::scheduler::read_channel() -> std::span<const T> {
	return m_channels_store.ensure_typed<T>().data.read_raw();
}

template <typename T>
requires gse::is_same_frame_channel_v<T>
auto gse::scheduler::drain_channel() -> std::vector<T> {
	return m_channels_store.template drain<T>();
}

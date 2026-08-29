export module gse.ecs:scheduler;

import std;

import gse.assert;
import gse.core;
import gse.concurrency;
import gse.time;
import gse.math;
import gse.diag;
import gse.introspection;

import :registries;
import :context;
import :settings;
import :system_node;
import :system_dispatch;
import :registry;
import :task_graph;
import :traits;
import :access_token;

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

		[[nodiscard]] auto current_phase() const -> scheduler_phase;

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

		auto tick(
			bool frame_ok,
			const std::function<void()>& in_frame = {}
		) -> void;

		auto render(
			bool frame_ok,
			const std::function<void()>& in_frame = {}
		) -> void;

		auto shutdown() -> void;

		auto clear() -> void;

		auto add_system_node(
			system_node node
		) -> void;

		auto queue_system_node(
			system_node node
		) -> void;

		template <typename T>
		auto register_external_resource(
			T* ptr
		) -> void;

		template <typename State>
		auto state(
			this scheduler& self
		) -> State&;

		template <typename State>
		auto state(
			this const scheduler& self
		) -> const State&;

		template <typename State>
		auto try_state_of(
			this scheduler& self
		) -> State*;

		template <typename State>
		auto try_state_of(
			this const scheduler& self
		) -> const State*;

		template <typename State>
		auto has() const -> bool;

		template <typename Resources>
		auto resources_of() const -> const Resources&;

		template <typename T>
		auto channel() -> gse::channel<T>&;

		template <typename T>
		auto read_channel() -> channel_read_guard<T>;

		auto make_channel_writer() -> channel_writer;

		auto begin_staging() -> void;

		auto resolve_activation(
			const std::unordered_set<id>& disabled_roots
		) -> void;

		auto register_deferred() -> void;

		template <typename T>
		requires is_same_frame_channel_v<T>
		auto drain_channel() -> std::vector<T>;

	private:
		auto register_node(
			system_node node
		) -> void*;

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
		std::optional<channel_writer> m_run_writer;
		std::vector<system_node> m_candidates;
		std::vector<system_node> m_deferred_nodes;
		bool m_staging = false;
		scheduler_phase m_phase = scheduler_phase::boot;
		state_registry m_states;
		std::unordered_map<id, std::vector<id>> m_state_deps;
		std::unordered_set<id> m_external_resources;
		resource_registry m_resources_store;
		channel_registry m_channels_store;
		std::vector<system_node> m_hot_add_queue;
		mutable std::mutex m_hot_add_mutex;
		registry* m_registry = nullptr;
		std::function<void(settings::register_settings_type)> m_settings_register_hook;
		task_graph m_update_graph;
		task_graph m_frame_graph;
		access_guard m_guard;
		bool m_initialized = false;
		bool m_dep_graph_checked = false;
	};
}

template <typename State>
auto gse::scheduler::state(this scheduler& self) -> State& {
	auto* p = self.m_states.state_ptr(id_of<State>());
	assert(p != nullptr, "state not found");
	return *static_cast<State*>(p);
}

template <typename State>
auto gse::scheduler::state(this const scheduler& self) -> const State& {
	const auto* p = self.m_states.state_ptr(id_of<State>());
	assert(p != nullptr, "state not found");
	return *static_cast<const State*>(p);
}

template <typename State>
auto gse::scheduler::try_state_of(this scheduler& self) -> State* {
	auto* p = self.m_states.state_ptr(id_of<State>());
	return static_cast<State*>(p);
}

template <typename State>
auto gse::scheduler::try_state_of(this const scheduler& self) -> const State* {
	const auto* p = self.m_states.state_ptr(id_of<State>());
	return static_cast<const State*>(p);
}

template <typename State>
auto gse::scheduler::has() const -> bool {
	return m_states.contains(id_of<State>());
}

template <typename Resources>
auto gse::scheduler::resources_of() const -> const Resources& {
	const auto* ptr = m_resources_store.resources_ptr(id_of<Resources>());
	assert(ptr != nullptr, "resources not found");
	return *static_cast<const Resources*>(ptr);
}

template <typename T>
auto gse::scheduler::register_external_resource(T* ptr) -> void {
	const auto type_id = id_of<T>();
	m_resources_store.register_resource(type_id, ptr);
	m_external_resources.insert(type_id);
	m_update_graph.notify_state_ready(type_id);
}

template <typename T>
auto gse::scheduler::channel() -> gse::channel<T>& {
	return m_channels_store.ensure_typed<T>().data;
}

template <typename T>
auto gse::scheduler::read_channel() -> channel_read_guard<T> {
	return channel_read_guard<T>(m_channels_store.ensure_typed<T>().data.read_raw());
}

template <typename T>
requires gse::is_same_frame_channel_v<T>
auto gse::scheduler::drain_channel() -> std::vector<T> {
	return m_channels_store.template drain<T>();
}


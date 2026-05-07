export module gse.ecs:scheduler;

import std;

import gse.assert;
import gse.core;
import gse.concurrency;
import gse.time;
import gse.diag;

import :phase_context;
import :registries;
import :run_context;
import :settings;
import :frame_context;
import :system_node;
import :system_dispatch;
import :registry;

export namespace gse {
	class scheduler {
	public:
		scheduler(
		) = default;

		auto set_registry(
			registry& reg
		) -> void;

		auto set_settings_sink(
			std::function<void(settings::register_settings_type)> fn
		) -> void;

		auto initialize(
		) -> void;

		auto update(
		) -> void;

		auto tick(
			bool frame_ok,
			const std::function<void()>& in_frame = {}
		) -> void;

		auto render(
			bool frame_ok,
			const std::function<void()>& in_frame = {}
		) -> void;

		auto shutdown(
		) -> void;

		auto clear(
		) -> void;

		auto push_deferred(
			gse::move_only_function<void()> fn
		) -> void;

		template <typename S, typename... Args>
		auto add_system(
			Args&&... args
		) -> state_of_t<S>&;

		template <typename S, typename... Args>
		auto ensure_system(
			Args&&... args
		) -> state_of_t<S>&;

		template <typename State>
		auto state(
			this auto& self
		) -> auto&;

		template <typename State>
		auto try_state_of(
			this auto& self
		) -> auto*;

		template <typename State>
		auto has() const -> bool;

		template <typename Resources>
		auto resources_of() const -> const Resources&;

		template <typename T>
		auto channel() -> gse::channel<T>&;

		template <typename T>
			requires is_same_frame_channel_v<T>
		auto drain_channel(
		) -> std::vector<T>;

		template <typename State, typename F>
		auto defer(
			F&& fn
		) -> void;

	private:
		auto drain_deferred(
		) -> void;

		auto check_state_dep_cycles(
		) -> void;

		auto check_closed_dep_graph(
		) -> void;

		auto run_unified_update(
		) -> void;

		auto advance_run_systems_during_init(
		) -> void;

		auto advance_one_run_system(
			system_node& node
		) -> async::task<>;

		auto drain_hot_add_queue(
		) -> void;

		auto snapshot_all_states(
		) -> void;

		std::vector<system_node> m_nodes;
		state_registry m_states;
		std::unordered_map<id, std::vector<id>> m_state_deps;
		resource_registry m_resources_store;
		channel_registry m_channels_store;
		std::vector<gse::move_only_function<void()>> m_deferred;
		std::mutex m_deferred_mutex;
		std::vector<system_node> m_hot_add_queue;
		std::mutex m_hot_add_mutex;
		registry* m_registry = nullptr;
		std::function<void(settings::register_settings_type)> m_settings_sink;
		task_graph m_update_graph;
		task_graph m_frame_graph;
		async::rw_mutex_registry m_access_mutexes;
		bool m_initialized = false;
		bool m_dep_graph_checked = false;
	};
}

template <typename State>
auto gse::scheduler::state(this auto& self) -> auto& {
	auto* p = self.m_states.state_ptr(compute_state_dep_id<State>());
	assert(p != nullptr, "state not found");
	using state_t = std::conditional_t<std::is_const_v<std::remove_pointer_t<decltype(p)>>, const State, State>;
	return *static_cast<state_t*>(p);
}

template <typename State>
auto gse::scheduler::try_state_of(this auto& self) -> auto* {
	auto* p = self.m_states.state_ptr(compute_state_dep_id<State>());
	using state_t = std::conditional_t<std::is_const_v<std::remove_pointer_t<decltype(p)>>, const State, State>;
	return static_cast<state_t*>(p);
}

template <typename State>
auto gse::scheduler::has() const -> bool {
	return m_states.contains(compute_state_dep_id<State>());
}

template <typename Resources>
auto gse::scheduler::resources_of() const -> const Resources& {
	const auto* ptr = m_resources_store.resources_ptr(id_of<Resources>());
	assert(ptr != nullptr, "resources not found");
	return *static_cast<const Resources*>(ptr);
}

template <typename T>
auto gse::scheduler::channel() -> gse::channel<T>& {
	auto& base = m_channels_store.ensure(id_of<T>(), +[]() -> std::unique_ptr<channel_base> {
		return std::make_unique<typed_channel<T>>();
	});
	return static_cast<typed_channel<T>&>(base).data;
}

template <typename T>
	requires gse::is_same_frame_channel_v<T>
auto gse::scheduler::drain_channel() -> std::vector<T> {
	return m_channels_store.template drain<T>();
}

template <typename State, typename F>
auto gse::scheduler::defer(F&& fn) -> void {
	using state_t = std::remove_cvref_t<State>;
	push_deferred([this, f = std::forward<F>(fn)]() mutable {
		auto* ptr = m_states.state_ptr(compute_state_dep_id<state_t>());
		if (ptr) {
			f(*static_cast<state_t*>(ptr));
		}
	});
}

template <typename S, typename... Args>
auto gse::scheduler::ensure_system(Args&&... args) -> state_of_t<S>& {
	using state_t = state_of_t<S>;
	if (auto* existing = try_state_of<state_t>()) {
		return *existing;
	}
	return add_system<S>(std::forward<Args>(args)...);
}

template <typename S, typename... Args>
auto gse::scheduler::add_system(Args&&... args) -> state_of_t<S>& {
	using state_t = state_of_t<S>;
	assert(m_registry != nullptr, "scheduler::set_registry must be called before add_system");

	auto node = make_system_node<S>(std::forward<Args>(args)...);
	auto* state_ref = static_cast<state_t*>(node.state_ptr);

	const auto canonical_idx = id_of<S>();
	(void)trace_id<S>();
	m_states.register_state(canonical_idx, node.state_ptr, node.state_snapshot_ptr);

	auto combined_deps = node.run_state_deps;
	combined_deps.insert(combined_deps.end(), node.frame_state_deps.begin(), node.frame_state_deps.end());
	m_state_deps.emplace(canonical_idx, std::move(combined_deps));

	if constexpr (has_resources<S>) {
		(void)trace_id<typename S::resources>();
		m_resources_store.register_resource(id_of<typename S::resources>(), node.resources_ptr);
	}

	if constexpr (has_settings<S>) {
		using settings_t = typename S::settings;
		(void)trace_id<settings_t>();
		m_states.register_state(node.settings_id, node.settings_ptr, node.settings_snapshot_ptr);

		if (m_settings_sink) {
			const std::string_view category = settings::category_of<settings_t>();
			m_settings_sink({
				.category = std::string(category),
				.type_id = node.settings_id,
				.settings_ptr = node.settings_ptr,
				.write = &settings::write_settings_for<settings_t>,
				.read = &settings::read_settings_for<settings_t>,
			});
		}
	}

	m_nodes.push_back(std::move(node));

	return *state_ref;
}

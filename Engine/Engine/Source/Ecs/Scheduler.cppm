export module gse.ecs:scheduler;

import std;

import gse.assert;
import gse.core;
import gse.concurrency;
import gse.time;
import gse.math;
import gse.diag;

import :phase_context;
import :registries;
import :run_context;
import :settings;
import :frame_context;
import :system_node;
import :system_dispatch;
import :registry;

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

	template <typename S>
	struct system_handle {
		auto state() const -> state_of_t<S>& {
			return *m_state;
		}

		auto operator*() const -> state_of_t<S>& {
			return *m_state;
		}

		auto operator->() const -> state_of_t<S>* {
			return m_state;
		}

		explicit operator bool() const {
			return m_state != nullptr;
		}

	private:
		friend class scheduler;
		state_of_t<S>* m_state = nullptr;
	};

	class scheduler {
	public:
		scheduler() = default;

		auto set_registry(
			registry& reg
		) -> void;

		auto set_advance_hook(
			std::function<void(id system_id, std::string_view phase)> fn
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

		template <typename S, typename... Args>
		auto add_system(
			Args&&... args
		) -> system_handle<S>;

		template <typename S, typename... Args>
		auto ensure_system(
			Args&&... args
		) -> system_handle<S>;

		template <typename S, typename... Args>
		auto queue_add_system(
			Args&&... args
		) -> system_handle<S>;

		template <typename T>
		auto register_external_resource(
			T* ptr
		) -> void;

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

		auto make_channel_writer() -> channel_writer;

		template <typename T>
		requires is_same_frame_channel_v<T>
		auto drain_channel() -> std::vector<T>;

	private:
		auto register_node(
			system_node node
		) -> void*;

		auto check_state_dep_cycles() -> void;

		auto check_closed_dep_graph() -> void;

		auto run_unified_update() -> void;

		auto advance_run_systems_during_init() -> void;

		auto advance_one_run_system(
			system_node& node
		) -> async::task<>;

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
		scheduler_phase m_phase = scheduler_phase::boot;
		state_registry m_states;
		std::unordered_map<id, std::vector<id>> m_state_deps;
		std::unordered_set<id> m_external_resources;
		resource_registry m_resources_store;
		channel_registry m_channels_store;
		std::vector<system_node> m_hot_add_queue;
		mutable std::mutex m_hot_add_mutex;
		registry* m_registry = nullptr;
		std::function<void(id, std::string_view)> m_advance_hook;
		std::function<void(settings::register_settings_type)> m_settings_register_hook;
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
requires gse::is_same_frame_channel_v<T>
auto gse::scheduler::drain_channel() -> std::vector<T> {
	return m_channels_store.template drain<T>();
}

template <typename S, typename... Args>
auto gse::scheduler::ensure_system(Args&&... args) -> system_handle<S> {
	using state_t = state_of_t<S>;
	if (auto* existing = try_state_of<state_t>()) {
		system_handle<S> h;
		h.m_state = existing;
		return h;
	}
	return add_system<S>(std::forward<Args>(args)...);
}

template <typename S, typename... Args>
auto gse::scheduler::add_system(Args&&... args) -> system_handle<S> {
	using state_t = state_of_t<S>;
	assert(m_registry != nullptr, "scheduler::set_registry must be called before add_system");
	assert(m_phase != scheduler_phase::shutdown, "scheduler::add_system called during shutdown");

	if (m_phase == scheduler_phase::boot) {
		(void)trace_id<S>();
		auto* state_ref = register_node(make_system_node<S>(std::forward<Args>(args)...));
		system_handle<S> h;
		h.m_state = static_cast<state_t*>(state_ref);
		return h;
	}

	return queue_add_system<S>(std::forward<Args>(args)...);
}

template <typename S, typename... Args>
auto gse::scheduler::queue_add_system(Args&&... args) -> system_handle<S> {
	using state_t = state_of_t<S>;
	(void)trace_id<S>();
	auto node = make_system_node<S>(std::forward<Args>(args)...);
	auto* state_ptr = static_cast<state_t*>(node.state_ptr);
	{
		std::lock_guard lock(m_hot_add_mutex);
		m_hot_add_queue.push_back(std::move(node));
	}
	system_handle<S> h;
	h.m_state = state_ptr;
	return h;
}

template <typename S, typename... Args>
auto gse::run_context::add_system(Args&&... args) -> void {
	(void)m_sched.queue_add_system<S>(std::forward<Args>(args)...);
}

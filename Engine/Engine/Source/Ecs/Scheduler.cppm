export module gse.ecs:scheduler;

import std;

import gse.assert;
import gse.core;
import gse.concurrency;
import gse.time;
import gse.diag;

import :phase_context;
import :task_context;
import :update_context;
import :frame_context;
import :system_node;
import :registry;

export namespace gse {
	class scheduler {
	public:
		scheduler(
		) = default;

		auto set_gpu_context(
			void* ctx
		) -> void;

		auto system_ptr(
			id idx
		) -> void*;

		auto system_ptr(
			id idx
		) const -> const void*;

		auto snapshot_ptr(
			id type
		) const -> const void*;

		auto frame_snapshot_ptr(
			id type
		) const -> const void*;

		auto channel_snapshot_ptr(
			id type
		) const -> const void*;

		auto resources_ptr(
			id type
		) const -> const void*;

		auto ensure_channel(
			id idx,
			channel_factory_fn factory
		) -> channel_base&;

		auto initialize(
		) -> void;

		auto update(
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

		template <typename S, typename State, typename... Args>
		auto add_system(
			registry& reg,
			Args&&... args
		) -> State&;

		template <typename State>
		auto state() const -> const State&;

		template <typename State>
		auto has() const -> bool;

		template <typename T>
		auto channel() -> channel<T>&;

		template <typename State, typename F>
		auto defer(
			F&& fn
		) -> void;

	private:
		auto ensure_channel_internal(
			id idx,
			channel_factory_fn factory
		) -> channel_base&;

		auto make_channel_writer(
		) -> channel_writer;

		auto drain_deferred(
		) -> void;

		auto check_state_dep_cycles(
		) -> void;

		auto run_graph_update(
		) -> void;

		auto snapshot_all_states(
		) -> void;

		auto snapshot_all_channels(
		) -> void;

		std::vector<std::unique_ptr<system_node_base>> m_nodes;
		std::unordered_map<id, system_node_base*> m_state_index;
		std::unordered_map<id, std::vector<id>> m_state_deps;
		std::unordered_map<id, system_node_base*> m_resources_index;
		std::unordered_map<id, std::unique_ptr<channel_base>> m_channels;
		mutable std::mutex m_channels_mutex;
		std::vector<gse::move_only_function<void()>> m_deferred;
		std::mutex m_deferred_mutex;
		registry* m_registry = nullptr;
		void* m_gpu_ctx = nullptr;
		task_graph m_update_graph;
		task_graph m_frame_graph;
		async::rw_mutex_registry m_access_mutexes;
		bool m_initialized = false;
	};
}

template <typename State>
auto gse::scheduler::state() const -> const State& {
	const auto it = m_state_index.find(id_of<State>());
	assert(it != m_state_index.end(), std::source_location::current(), "state not found");
	return *static_cast<const State*>(it->second->state_ptr());
}

template <typename State>
auto gse::scheduler::has() const -> bool {
	return m_state_index.contains(id_of<State>());
}

template <typename T>
auto gse::scheduler::channel() -> gse::channel<T>& {
	auto& base = ensure_channel_internal(id_of<T>(), +[]() -> std::unique_ptr<channel_base> {
		return std::make_unique<typed_channel<T>>();
	});

	return static_cast<typed_channel<T>&>(base).data;
}

template <typename State, typename F>
auto gse::scheduler::defer(F&& fn) -> void {
	using state_t = std::remove_cvref_t<State>;
	push_deferred([this, f = std::forward<F>(fn)]() mutable {
		auto* ptr = system_ptr(id_of<state_t>());
		if (ptr) {
			f(*static_cast<state_t*>(ptr));
		}
	});
}

template <typename S, typename State, typename... Args>
auto gse::scheduler::add_system(registry& reg, Args&&... args) -> State& {
	if (m_registry == nullptr) {
		m_registry = &reg;
	}

	auto ptr = std::make_unique<system_node<S, State>>(std::forward<Args>(args)...);
	auto* raw = ptr.get();

	const auto state_idx = id_of<State>();
	find_or_generate_id(type_tag<State>());
	m_state_index.emplace(state_idx, raw);
	m_state_deps.emplace(state_idx, extract_state_deps<S, State>());

	if constexpr (has_resources<S>) {
		find_or_generate_id(type_tag<typename S::resources>());
		m_resources_index.emplace(id_of<typename S::resources>(), raw);
	}

	m_nodes.push_back(std::move(ptr));

	if (m_initialized) {
		auto writer = make_channel_writer();
		init_context phase{
			.reg = *m_registry,
			.sched = *this,
			.channels = writer
		};
		phase.gpu_ctx = m_gpu_ctx;
		raw->initialize(phase);
	}

	return raw->state();
}

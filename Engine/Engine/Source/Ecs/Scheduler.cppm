export module gse.ecs:scheduler;

import std;

import gse.assert;
import gse.core;
import gse.concurrency;
import gse.time;
import gse.diag;

import :phase_context;
import :registries;
import :update_context;
import :frame_context;
import :system_node;
import :system_dispatch;
import :registry;

export namespace gse {
	class scheduler {
	public:
		scheduler(
		) = default;

		auto set_gpu_context(
			void* ctx
		) -> void;

		auto set_asset_registry(
			void* reg
		) -> void;

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

		template <typename State, typename F>
		auto defer(
			F&& fn
		) -> void;

	private:
		auto drain_deferred(
		) -> void;

		auto check_state_dep_cycles(
		) -> void;

		auto run_graph_update(
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
		registry* m_registry = nullptr;
		void* m_gpu_ctx = nullptr;
		void* m_asset_registry = nullptr;
		task_graph m_update_graph;
		task_graph m_frame_graph;
		async::rw_mutex_registry m_access_mutexes;
		bool m_initialized = false;
	};
}

template <typename State>
auto gse::scheduler::state(this auto& self) -> auto& {
	auto* p = self.m_states.state_ptr(id_of<State>());
	assert(p != nullptr, std::source_location::current(), "state not found");
	using state_t = std::conditional_t<std::is_const_v<std::remove_pointer_t<decltype(p)>>, const State, State>;
	return *static_cast<state_t*>(p);
}

template <typename State>
auto gse::scheduler::try_state_of(this auto& self) -> auto* {
	auto* p = self.m_states.state_ptr(id_of<State>());
	using state_t = std::conditional_t<std::is_const_v<std::remove_pointer_t<decltype(p)>>, const State, State>;
	return static_cast<state_t*>(p);
}

template <typename State>
auto gse::scheduler::has() const -> bool {
	return m_states.contains(id_of<State>());
}

template <typename Resources>
auto gse::scheduler::resources_of() const -> const Resources& {
	const auto* ptr = m_resources_store.resources_ptr(id_of<Resources>());
	assert(ptr != nullptr, std::source_location::current(), "resources not found");
	return *static_cast<const Resources*>(ptr);
}

template <typename T>
auto gse::scheduler::channel() -> gse::channel<T>& {
	auto& base = m_channels_store.ensure(id_of<T>(), +[]() -> std::unique_ptr<channel_base> {
		return std::make_unique<typed_channel<T>>();
	});
	return static_cast<typed_channel<T>&>(base).data;
}

template <typename State, typename F>
auto gse::scheduler::defer(F&& fn) -> void {
	using state_t = std::remove_cvref_t<State>;
	push_deferred([this, f = std::forward<F>(fn)]() mutable {
		auto* ptr = m_states.state_ptr(id_of<state_t>());
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

	auto node = make_system_node<S, State>(std::forward<Args>(args)...);
	auto* state_ref = static_cast<State*>(node.state_ptr);

	const auto state_idx = id_of<State>();
	find_or_generate_id(type_tag<State>());
	m_states.register_state(state_idx, node.state_ptr, node.state_snapshot_ptr);

	auto combined_deps = node.update_state_deps;
	combined_deps.insert(combined_deps.end(), node.frame_state_deps.begin(), node.frame_state_deps.end());
	m_state_deps.emplace(state_idx, std::move(combined_deps));

	if constexpr (has_resources<S>) {
		find_or_generate_id(type_tag<typename S::resources>());
		m_resources_store.register_resource(id_of<typename S::resources>(), node.resources_ptr);
	}

	m_nodes.push_back(std::move(node));

	if (m_initialized) {
		auto writer = m_channels_store.make_writer();
		init_context phase{
			.gpu_ctx = m_gpu_ctx,
			.assets_ptr = m_asset_registry,
			.reg = *m_registry,
			.sched = *this,
			.states = m_states,
			.resources_store = m_resources_store,
			.channels_store = m_channels_store,
			.channels = writer,
		};
		m_nodes.back().invoke_initialize_fn(phase, m_nodes.back().data.get());
	}

	return *state_ref;
}

export module gse.utility:scheduler;

import std;

import :phase_context;
import :update_context;
import :frame_context;
import :task_graph;
import :async_task;
import :frame_arena;
import :mpsc_ring_buffer;
import :system_node;
import :registry;
import :task;
import :system_clock;
import :frame_sync;
import :trace;

import gse.assert;

export namespace gse {
	class scheduler final : public state_snapshot_provider, public channel_reader_provider, public system_provider {
	public:
		scheduler() = default;

		auto set_gpu_context(
			void* ctx
		) -> void;

		auto system_ptr(
			std::type_index idx
		) -> void* override;

		auto system_ptr(
			std::type_index idx
		) const -> const void* override;

		template <typename S, typename State, typename RenderState = void, typename... Args>
		auto add_system(
			registry& reg,
			Args&&... args
		) -> State&;

		template <typename State>
		auto state(
		) const -> const State&;

		template <typename State>
		auto has(
		) const -> bool;

		auto snapshot_ptr(
			std::type_index type
		) const -> const void* override;

		auto channel_snapshot_ptr(
			std::type_index type
		) const -> const void* override;

		auto ensure_channel(
			std::type_index idx,
			channel_factory_fn factory
		) -> channel_base& override;

		template <typename T>
		auto channel(
		) -> channel<T>&;

		auto initialize(
		) -> void;

		auto update(
		) -> void;

		auto render(
			const std::function<void()>& in_frame = {}
		) -> void;

		auto shutdown(
		) -> void;

		auto clear(
		) -> void;

		auto push_deferred(
			std::move_only_function<void()> fn
		) -> void;

		template <typename State, typename F>
		auto defer(
			F&& fn
		) -> void;
	private:
		auto drain_deferred(
		) -> void;

		auto run_graph_update(
		) -> void;

		auto run_graph_frame(
		) -> void;

		std::vector<std::unique_ptr<system_node_base>> m_nodes;
		std::unordered_map<std::type_index, system_node_base*> m_state_index;
		std::unordered_map<std::type_index, std::unique_ptr<channel_base>> m_channels;
		mutable std::mutex m_channels_mutex;
		std::vector<std::move_only_function<void()>> m_deferred;
		std::mutex m_deferred_mutex;
		registry* m_registry = nullptr;
		registry_access m_registry_access{};
		void* m_gpu_ctx = nullptr;
		task_graph m_update_graph;
		task_graph m_frame_graph;
		mpsc_ring_buffer<std::move_only_function<void()>, 64> m_update_deferred_ops;
		std::binary_semaphore m_update_complete{ 0 };

		auto snapshot_all_channels(
		) -> void;

		auto ensure_channel_internal(
			std::type_index idx,
			channel_factory_fn factory
		) -> channel_base&;

		auto make_channel_writer(
		) -> channel_writer;
	};
}

namespace gse {
	auto wrap_work(
		std::move_only_function<void()> fn
	) -> async::task<>;
}

auto gse::wrap_work(std::move_only_function<void()> fn) -> async::task<> {
	fn();
	co_return;
}

template <typename S, typename State, typename RenderState, typename... Args>
auto gse::scheduler::add_system(registry& reg, Args&&... args) -> State& {
	if (m_registry == nullptr) {
		m_registry = &reg;
		m_registry_access.reg = m_registry;
	}

	auto ptr = std::make_unique<system_node<S, State, RenderState>>(std::forward<Args>(args)...);
	auto* raw = ptr.get();

	m_state_index.emplace(std::type_index(typeid(State)), raw);
	m_nodes.push_back(std::move(ptr));

	return raw->state();
}

template <typename State>
auto gse::scheduler::state() const -> const State& {
	const auto it = m_state_index.find(std::type_index(typeid(State)));
	assert(it != m_state_index.end(), std::source_location::current(), "state not found");
	return *static_cast<const State*>(it->second->state_ptr());
}

template <typename State>
auto gse::scheduler::has() const -> bool {
	return m_state_index.contains(std::type_index(typeid(State)));
}

auto gse::scheduler::snapshot_ptr(const std::type_index type) const -> const void* {
	const auto it = m_state_index.find(type);
	if (it == m_state_index.end()) {
		return nullptr;
	}
	return it->second->state_ptr();
}

auto gse::scheduler::system_ptr(const std::type_index idx) -> void* {
	const auto it = m_state_index.find(idx);
	if (it == m_state_index.end()) {
		return nullptr;
	}
	return it->second->state_ptr();
}

auto gse::scheduler::system_ptr(const std::type_index idx) const -> const void* {
	const auto it = m_state_index.find(idx);
	if (it == m_state_index.end()) {
		return nullptr;
	}
	return it->second->state_ptr();
}

auto gse::scheduler::channel_snapshot_ptr(const std::type_index type) const -> const void* {
	std::lock_guard lock(m_channels_mutex);
	const auto it = m_channels.find(type);
	if (it == m_channels.end()) {
		return nullptr;
	}
	return it->second->snapshot_data();
}

template <typename T>
auto gse::scheduler::channel() -> gse::channel<T>& {
	auto& base = ensure_channel_internal(std::type_index(typeid(T)), +[]() -> std::unique_ptr<channel_base> {
		return std::make_unique<typed_channel<T>>();
	});

	return static_cast<typed_channel<T>&>(base).data;
}

auto gse::scheduler::set_gpu_context(void* ctx) -> void {
	m_gpu_ctx = ctx;
}

auto gse::scheduler::initialize() -> void {
	frame_arena::create();

	frame_sync::on_begin([this] {
		snapshot_all_channels();
	});

	auto writer = make_channel_writer();

	initialize_phase phase{
		.registry = m_registry_access,
		.snapshots = *this,
		.channels = writer
	};
	phase.gpu_ctx = m_gpu_ctx;

	for (const auto& n : m_nodes) {
		n->initialize(phase);
	}
}

auto gse::scheduler::push_deferred(std::move_only_function<void()> fn) -> void {
	std::lock_guard lock(m_deferred_mutex);
	m_deferred.push_back(std::move(fn));
}

auto gse::scheduler::drain_deferred() -> void {
	std::vector<std::move_only_function<void()>> batch;
	{
		std::lock_guard lock(m_deferred_mutex);
		batch.swap(m_deferred);
	}
	for (auto& fn : batch) {
		fn();
	}
}

template <typename State, typename F>
auto gse::scheduler::defer(F&& fn) -> void {
	push_deferred([this, f = std::forward<F>(fn)]() mutable {
		auto* ptr = system_ptr(std::type_index(typeid(State)));
		if (ptr) f(*static_cast<State*>(ptr));
	});
}

auto gse::scheduler::update() -> void {
	drain_deferred();
	run_graph_update();
	m_update_complete.release();
}

auto gse::scheduler::run_graph_update() -> void {
	auto writer = make_channel_writer();
	std::vector<scheduled_work> collected_work;

	update_context u_ctx{
		.reg = *m_registry,
		.snapshots = *this,
		.channels = writer,
		.channel_reader = *this,
		.graph = m_update_graph,
		.work = collected_work,
		.deferred_ops = m_update_deferred_ops
	};
	u_ctx.gpu_ctx = m_gpu_ctx;

	for (const auto& node : m_nodes) {
		trace::scope(node->trace_id(), [&] {
			node->graph_update(u_ctx);
		});
	}

	if (!collected_work.empty()) {
		for (auto& item : collected_work) {
			m_update_graph.submit(
				find_or_generate_id("schedule_work"),
				wrap_work(std::move(item.execute)),
				std::move(item.reads),
				std::move(item.writes)
			);
		}
		m_update_graph.execute();
		m_update_graph.clear();
	}

	std::move_only_function<void()> op;
	while (m_update_deferred_ops.pop(op)) {
		op();
	}
}

auto gse::scheduler::run_graph_frame() -> void {
	frame_context f_ctx{
		.snapshots = *this,
		.channel_reader = *this,
		.graph = m_frame_graph
	};
	f_ctx.gpu_ctx = m_gpu_ctx;

	for (const auto& node : m_nodes) {
		trace::scope(node->trace_id(), [&] {
			auto coro = node->graph_frame(f_ctx);
			coro.start();
		});
	}
}

auto gse::scheduler::render(const std::function<void()>& in_frame) -> void {
	begin_frame_phase bf_phase{
		.snapshots = *this
	};
	bf_phase.gpu_ctx = m_gpu_ctx;

	std::vector started(m_nodes.size(), false);

	if (!m_nodes.empty()) {
		started[0] = m_nodes[0]->begin_frame(bf_phase);
		m_update_complete.acquire();
		if (!started[0]) {
			return;
		}

		for (auto i : std::views::iota(std::size_t{1}, m_nodes.size())) {
			started[i] = m_nodes[i]->begin_frame(bf_phase);
		}
	}

	prepare_render_phase pr_phase{
		.snapshots = *this,
		.channel_reader = *this
	};
	pr_phase.gpu_ctx = m_gpu_ctx;

	for (std::size_t i = 0; i < m_nodes.size(); ++i) {
		if (started[i]) {
			m_nodes[i]->prepare_render(pr_phase);
		}
	}

	const registry_access const_reg_access = m_registry_access;

	render_phase r_phase{
		.registry = const_reg_access,
		.snapshots = *this,
		.channel_reader = *this
	};
	r_phase.gpu_ctx = m_gpu_ctx;

	for (const auto& n : m_nodes) {
		n->render(r_phase);
	}

	run_graph_frame();

	if (in_frame) {
		in_frame();
	}

	auto ef_writer = make_channel_writer();

	end_frame_phase ef_phase{
		.snapshots = *this,
		.channels = ef_writer
	};
	ef_phase.gpu_ctx = m_gpu_ctx;

	for (std::size_t i = m_nodes.size(); i-- > 0;) {
		if (started[i]) {
			m_nodes[i]->end_frame(ef_phase);
		}
	}

	frame_arena::reset();
}

auto gse::scheduler::shutdown() -> void {
	shutdown_phase phase{
		.registry = m_registry_access
	};
	phase.gpu_ctx = m_gpu_ctx;

	for (const auto& node : m_nodes | std::views::reverse) {
		node->shutdown(phase);
	}

	frame_arena::destroy();
}

auto gse::scheduler::clear() -> void {
	m_nodes.clear();
	m_state_index.clear();
	m_channels.clear();
}

auto gse::scheduler::snapshot_all_channels() -> void {
	std::lock_guard lock(m_channels_mutex);
	for (const auto& ch_ptr : m_channels | std::views::values) {
		ch_ptr->take_snapshot();
	}
}

auto gse::scheduler::ensure_channel_internal(const std::type_index idx, const channel_factory_fn factory) -> channel_base& {
	std::lock_guard lock(m_channels_mutex);
	auto it = m_channels.find(idx);

	if (it == m_channels.end()) {
		it = m_channels.emplace(idx, factory()).first;
	}

	return *it->second;
}

auto gse::scheduler::ensure_channel(const std::type_index idx, const channel_factory_fn factory) -> channel_base& {
	return ensure_channel_internal(idx, factory);
}

auto gse::scheduler::make_channel_writer() -> channel_writer {
	return channel_writer([this](const std::type_index type, std::any item, const channel_factory_fn factory) {
		std::lock_guard lock(m_channels_mutex);
		auto it = m_channels.find(type);
		if (it == m_channels.end()) {
			it = m_channels.emplace(type, factory()).first;
		}
		it->second->push_any(std::move(item));
	});
}


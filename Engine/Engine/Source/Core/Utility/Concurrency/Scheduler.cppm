export module gse.utility:scheduler;

import std;

import :phase_context;
import :update_context;
import :frame_context;
import :task_graph;
import :async_task;
import :frame_arena;
import :system_node;
import :registry;
import :task;
import :system_clock;
import :frame_sync;
import :trace;

import gse.assert;

export namespace gse {
	class scheduler final : public state_snapshot_provider, public channel_reader_provider, public resources_provider, public system_provider {
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

		template <typename S, typename State, typename... Args>
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

		auto resources_ptr(
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

		auto snapshot_all_states(
		) -> void;

		std::vector<std::unique_ptr<system_node_base>> m_nodes;
		std::unordered_map<std::type_index, system_node_base*> m_state_index;
		std::unordered_map<std::type_index, system_node_base*> m_resources_index;
		std::unordered_map<std::type_index, std::unique_ptr<channel_base>> m_channels;
		mutable std::mutex m_channels_mutex;
		std::vector<gse::move_only_function<void()>> m_deferred;
		std::mutex m_deferred_mutex;
		registry* m_registry = nullptr;
		void* m_gpu_ctx = nullptr;
		task_graph m_update_graph;
		task_graph m_frame_graph;
		bool m_initialized = false;

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
		gse::move_only_function<void()> fn
	) -> async::task<>;

	class frame_snapshot_provider final : public state_snapshot_provider {
	public:
		explicit frame_snapshot_provider(
			const std::unordered_map<std::type_index, system_node_base*>& index
		);

		auto snapshot_ptr(
			std::type_index type
		) const -> const void* override;

	private:
		const std::unordered_map<std::type_index, system_node_base*>& m_index;
	};
}

auto gse::wrap_work(gse::move_only_function<void()> fn) -> async::task<> {
	fn();
	co_return;
}

gse::frame_snapshot_provider::frame_snapshot_provider(const std::unordered_map<std::type_index, system_node_base*>& index)
	: m_index(index) {}

auto gse::frame_snapshot_provider::snapshot_ptr(const std::type_index type) const -> const void* {
	const auto it = m_index.find(type);
	if (it == m_index.end()) {
		return nullptr;
	}
	return it->second->state_snapshot_ptr();
}

template <typename S, typename State, typename... Args>
auto gse::scheduler::add_system(registry& reg, Args&&... args) -> State& {
	if (m_registry == nullptr) {
		m_registry = &reg;
	}

	auto ptr = std::make_unique<system_node<S, State>>(std::forward<Args>(args)...);
	auto* raw = ptr.get();

	m_state_index.emplace(std::type_index(typeid(State)), raw);

	if constexpr (has_resources<S>) {
		m_resources_index.emplace(std::type_index(typeid(typename S::resources)), raw);
	}

	m_nodes.push_back(std::move(ptr));

	if (m_initialized) {
		auto writer = make_channel_writer();
		init_context phase{
			.reg = *m_registry,
			.snapshots = *this,
			.resource_provider = *this,
			.channels = writer
		};
		phase.gpu_ctx = m_gpu_ctx;
		raw->initialize(phase);
	}

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

auto gse::scheduler::resources_ptr(const std::type_index type) const -> const void* {
	const auto it = m_resources_index.find(type);
	if (it == m_resources_index.end()) {
		return nullptr;
	}
	return it->second->resources_ptr();
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
	frame_sync::on_begin([this] {
		snapshot_all_channels();
	});

	auto writer = make_channel_writer();

	init_context phase{
		.reg = *m_registry,
		.snapshots = *this,
		.resource_provider = *this,
		.channels = writer
	};
	phase.gpu_ctx = m_gpu_ctx;

	for (const auto& n : m_nodes) {
		n->initialize(phase);
	}

	m_initialized = true;
}

auto gse::scheduler::push_deferred(gse::move_only_function<void()> fn) -> void {
	std::lock_guard lock(m_deferred_mutex);
	m_deferred.push_back(std::move(fn));
}

auto gse::scheduler::drain_deferred() -> void {
	std::vector<gse::move_only_function<void()>> batch;
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
		if (ptr) {
			f(*static_cast<State*>(ptr));
		}
	});
}

auto gse::scheduler::update() -> void {
	trace::scope(find_or_generate_id("scheduler::update"), [&] {
		trace::scope(find_or_generate_id("scheduler::drain_deferred"), [&] {
			drain_deferred();
		});
		run_graph_update();
		if (m_registry) {
			trace::scope(find_or_generate_id("scheduler::registry_sync"), [&] {
				m_registry->sync();
			});
		}
		trace::scope(find_or_generate_id("scheduler::snapshot_states"), [&] {
			snapshot_all_states();
		});
	});
}

auto gse::scheduler::run_graph_update() -> void {
	trace::scope(find_or_generate_id("scheduler::run_graph_update"), [&] {
		auto writer = make_channel_writer();
		std::vector<scheduled_work> collected_work;
		std::mutex collected_work_mutex;

		update_context u_ctx{
			.reg = *m_registry,
			.snapshots = *this,
			.channels = writer,
			.channel_reader = *this,
			.resources = *this,
			.graph = m_update_graph,
			.work = collected_work,
			.work_mutex = collected_work_mutex
		};
		u_ctx.gpu_ctx = m_gpu_ctx;

		{
			task::group group(find_or_generate_id("scheduler::parallel_updates"));
			for (const auto& node : m_nodes) {
				group.post([&, node = node.get()] {
					node->graph_update(u_ctx);
				}, node->trace_id());
			}
			group.wait();
		}

		if (!collected_work.empty()) {
			trace::scope(find_or_generate_id("scheduler::update_submit"), [&] {
				for (auto& item : collected_work) {
					m_update_graph.submit(
						item.work_id,
						wrap_work(std::move(item.execute)),
						std::move(item.reads),
						std::move(item.writes)
					);
				}
			});
			trace::scope(find_or_generate_id("scheduler::update_graph_execute"), [&] {
				m_update_graph.execute();
			});
			trace::scope(find_or_generate_id("scheduler::update_graph_clear"), [&] {
				m_update_graph.clear();
			});
		}
	});
}

auto gse::scheduler::snapshot_all_states() -> void {
	for (const auto& node : m_nodes) {
		node->snapshot_state();
	}
}

auto gse::scheduler::run_graph_frame() -> void {
	trace::scope(find_or_generate_id("scheduler::run_graph_frame"), [&] {
		frame_snapshot_provider frame_snapshots(m_state_index);
		auto writer = make_channel_writer();

		frame_context f_ctx{
			.reg = *m_registry,
			.snapshots = frame_snapshots,
			.channel_reader = *this,
			.channels = writer,
			.resources = *this,
			.graph = m_frame_graph
		};
		f_ctx.gpu_ctx = m_gpu_ctx;

		std::vector<async::task<>> tasks;
		trace::scope(find_or_generate_id("scheduler::collect_frame_tasks"), [&] {
			for (const auto& node : m_nodes) {
				if (!node->has_frame()) {
					continue;
				}
				tasks.push_back(node->graph_frame(f_ctx));
			}
		});

		if (!tasks.empty()) {
			trace::scope(find_or_generate_id("scheduler::frame_sync_wait"), [&] {
				async::sync_wait(async::when_all(std::move(tasks)));
			});
			trace::scope(find_or_generate_id("scheduler::frame_graph_clear"), [&] {
				m_frame_graph.clear();
			});
		}
	});
}

auto gse::scheduler::render(const bool frame_ok, const std::function<void()>& in_frame) -> void {
	if (!frame_ok) {
		return;
	}

	trace::scope(find_or_generate_id("scheduler::render"), [&] {
		run_graph_frame();

		if (in_frame) {
			trace::scope(find_or_generate_id("scheduler::in_frame_callback"), [&] {
				in_frame();
			});
		}
	});
}

auto gse::scheduler::shutdown() -> void {
	shutdown_context phase{
		.reg = *m_registry
	};
	phase.gpu_ctx = m_gpu_ctx;

	for (auto it = m_nodes.rbegin(); it != m_nodes.rend(); ++it) {
		(*it)->shutdown(phase);
	}
}

auto gse::scheduler::clear() -> void {
	m_nodes.clear();
	m_state_index.clear();
	m_resources_index.clear();
	m_channels.clear();
	m_initialized = false;
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

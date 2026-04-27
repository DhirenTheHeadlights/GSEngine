module gse.ecs;

import std;

import gse.assert;
import gse.core;
import gse.concurrency;
import gse.time;
import gse.diag;

auto gse::scheduler::set_gpu_context(void* ctx) -> void {
	m_gpu_ctx = ctx;
}

auto gse::scheduler::system_ptr(const id idx) -> void* {
	const auto it = m_state_index.find(idx);
	if (it == m_state_index.end()) {
		return nullptr;
	}
	return it->second->state_ptr();
}

auto gse::scheduler::system_ptr(const id idx) const -> const void* {
	const auto it = m_state_index.find(idx);
	if (it == m_state_index.end()) {
		return nullptr;
	}
	return it->second->state_ptr();
}

auto gse::scheduler::snapshot_ptr(const id type) const -> const void* {
	const auto it = m_state_index.find(type);
	if (it == m_state_index.end()) {
		return nullptr;
	}
	return it->second->state_ptr();
}

auto gse::scheduler::frame_snapshot_ptr(const id type) const -> const void* {
	const auto it = m_state_index.find(type);
	if (it == m_state_index.end()) {
		return nullptr;
	}
	return it->second->state_snapshot_ptr();
}

auto gse::scheduler::channel_snapshot_ptr(const id type) const -> const void* {
	std::lock_guard lock(m_channels_mutex);
	const auto it = m_channels.find(type);
	if (it == m_channels.end()) {
		return nullptr;
	}
	return it->second->snapshot_data();
}

auto gse::scheduler::resources_ptr(const id type) const -> const void* {
	const auto it = m_resources_index.find(type);
	if (it == m_resources_index.end()) {
		return nullptr;
	}
	return it->second->resources_ptr();
}

auto gse::scheduler::ensure_channel(const id idx, const channel_factory_fn factory) -> channel_base& {
	return ensure_channel_internal(idx, factory);
}

auto gse::scheduler::ensure_channel_internal(const id idx, const channel_factory_fn factory) -> channel_base& {
	std::lock_guard lock(m_channels_mutex);
	auto it = m_channels.find(idx);
	if (it == m_channels.end()) {
		it = m_channels.emplace(idx, factory()).first;
	}
	return *it->second;
}

auto gse::scheduler::make_channel_writer() -> channel_writer {
	return channel_writer([this](const id type, std::any item, const channel_factory_fn factory) {
		std::lock_guard lock(m_channels_mutex);
		auto it = m_channels.find(type);
		if (it == m_channels.end()) {
			it = m_channels.emplace(type, factory()).first;
		}
		it->second->push_any(std::move(item));
	});
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

auto gse::scheduler::snapshot_all_states() -> void {
	for (const auto& node : m_nodes) {
		node->snapshot_state();
	}
}

auto gse::scheduler::snapshot_all_channels() -> void {
	std::lock_guard lock(m_channels_mutex);
	for (const auto& ch_ptr : m_channels | std::views::values) {
		ch_ptr->take_snapshot();
	}
}

auto gse::scheduler::check_state_dep_cycles() -> void {
	enum class color : std::uint8_t { white, gray, black };
	std::unordered_map<id, color> colors;
	for (const auto& [state_idx, _] : m_state_deps) {
		colors[state_idx] = color::white;
	}

	std::vector<id> stack;

	auto format_cycle = [&](const id from) -> std::string {
		const auto cycle_start = std::ranges::find(stack, from);
		std::string out;
		for (auto it = cycle_start; it != stack.end(); ++it) {
			if (!out.empty()) {
				out += " -> ";
			}
			out += it->tag();
		}
		out += " -> ";
		out += from.tag();
		return out;
	};

	auto visit = [&](const id node, auto& self) -> void {
		colors[node] = color::gray;
		stack.push_back(node);

		if (const auto it = m_state_deps.find(node); it != m_state_deps.end()) {
			for (const auto& dep : it->second) {
				if (!colors.contains(dep)) {
					continue;
				}
				if (colors[dep] == color::gray) {
					assert(
						false,
						std::source_location::current(),
						"state_deps cycle detected: {}",
						format_cycle(dep)
					);
					continue;
				}
				if (colors[dep] == color::white) {
					self(dep, self);
				}
			}
		}

		stack.pop_back();
		colors[node] = color::black;
	};

	for (const auto& [state_idx, _] : m_state_deps) {
		if (colors[state_idx] == color::white) {
			visit(state_idx, visit);
		}
	}
}

auto gse::scheduler::initialize() -> void {
	check_state_dep_cycles();

	frame_sync::on_begin([this] {
		snapshot_all_channels();
	});

	auto writer = make_channel_writer();
	init_context phase{
		.reg = *m_registry,
		.sched = *this,
		.channels = writer
	};
	phase.gpu_ctx = m_gpu_ctx;

	for (std::size_t i = 0; i < m_nodes.size(); ++i) {
		m_nodes[i]->initialize(phase);
	}

	m_initialized = true;
}

auto gse::scheduler::run_graph_update() -> void {
	trace::scope_guard sg{ trace_id<"scheduler::run_graph_update">() };
	auto writer = make_channel_writer();

	update_context u_ctx(
		m_gpu_ctx,
		*this,
		writer,
		m_update_graph,
		*m_registry,
		m_access_mutexes
	);

	std::vector<async::task<>> tasks;
	tasks.reserve(m_nodes.size());
	for (const auto& node : m_nodes) {
		tasks.push_back(node->graph_update(u_ctx));
	}
	{
		trace::scope_guard sg2{ trace_id<"scheduler::update_sync_wait">() };
		async::sync_wait(async::when_all(std::move(tasks)));
	}
}

auto gse::scheduler::update() -> void {
	trace::scope_guard sg{ trace_id<"scheduler::update">() };
	{
		trace::scope_guard sg2{ trace_id<"scheduler::drain_deferred">() };
		drain_deferred();
	}
	run_graph_update();
	{
		trace::scope_guard sg2{ trace_id<"scheduler::snapshot_states">() };
		snapshot_all_states();
	}
}

auto gse::scheduler::render(const bool frame_ok, const std::function<void()>& in_frame) -> void {
	if (!frame_ok) {
		return;
	}

	trace::scope_guard sg{ trace_id<"scheduler::render">() };
	auto writer = make_channel_writer();

	frame_context f_ctx(
		m_gpu_ctx,
		*this,
		writer,
		m_frame_graph,
		*m_registry
	);

	std::vector<async::task<>> tasks;
	{
		trace::scope_guard sg2{ trace_id<"scheduler::collect_frame_tasks">() };
		for (const auto& node : m_nodes) {
			if (!node->has_frame()) {
				continue;
			}
			tasks.push_back(node->graph_frame(f_ctx));
		}
	}

	if (!tasks.empty()) {
		trace::scope_guard sg2{ trace_id<"scheduler::start_frame_tasks">() };
		task::group group(trace_id<"scheduler::start_frame_tasks">());
		for (auto& t : tasks) {
			group.post([t_ptr = std::addressof(t)] {
				t_ptr->start();
			});
		}
		group.wait();
	}

	if (in_frame) {
		trace::scope_guard sg2{ trace_id<"scheduler::in_frame_callback">() };
		in_frame();
	}

	if (!tasks.empty()) {
		{
			trace::scope_guard sg2{ trace_id<"scheduler::frame_sync_wait">() };
			async::sync_wait(async::when_all(std::move(tasks)));
		}
		{
			trace::scope_guard sg2{ trace_id<"scheduler::frame_graph_clear">() };
			m_frame_graph.clear();
		}
	}
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

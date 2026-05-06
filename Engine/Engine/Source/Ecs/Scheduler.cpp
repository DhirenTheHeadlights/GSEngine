module gse.ecs;

import std;

import gse.assert;
import gse.core;
import gse.concurrency;
import gse.time;
import gse.diag;

auto gse::scheduler::set_registry(registry& reg) -> void {
	m_registry = &reg;
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
	for (auto& node : m_nodes) {
		node.invoke_snapshot_fn(node.data.get());
	}
}

namespace gse {
	auto run_node_update(
		update_context& ctx,
		system_node& node
	) -> async::task<>;

	auto run_node_frame(
		frame_context& ctx,
		system_node& node
	) -> async::task<>;
}

auto gse::run_node_update(update_context& ctx, system_node& node) -> async::task<> {
	const auto eid = trace::begin_block(node.update_wall_id, 0);
	auto guard = make_scope_exit([uwid = node.update_wall_id, eid] {
		trace::end_block(uwid, eid, 0);
	});

	for (const id& dep : node.update_state_deps) {
		co_await ctx.after_id(dep);
	}
	co_await node.invoke_update_fn(ctx, node.data.get());
	ctx.notify_ready_by_id(node.state_id);
	if (node.resources_id.exists()) {
		ctx.notify_ready_by_id(node.resources_id);
	}
	if (node.settings_id.exists()) {
		ctx.notify_ready_by_id(node.settings_id);
	}
}

auto gse::run_node_frame(frame_context& ctx, system_node& node) -> async::task<> {
	const auto eid = trace::begin_block(node.frame_wall_id, 0);
	auto guard = make_scope_exit([fwid = node.frame_wall_id, eid] {
		trace::end_block(fwid, eid, 0);
	});

	for (const id& dep : node.frame_state_deps) {
		co_await ctx.after_id(dep);
	}
	co_await node.invoke_frame_fn(ctx, node.data.get());
	ctx.notify_ready_by_id(node.state_id);
	if (node.resources_id.exists()) {
		ctx.notify_ready_by_id(node.resources_id);
	}
	if (node.settings_id.exists()) {
		ctx.notify_ready_by_id(node.settings_id);
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
			out += std::format("{}", *it);
		}
		out += " -> ";
		out += std::format("{}", from);
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
	if (!m_initialized) {
		frame_sync::on_begin([this] {
			m_channels_store.take_snapshot_all();
		});
		m_initialized = true;
	}

	m_channels_store.flip_all();

	auto writer = m_channels_store.make_writer();
	init_context phase{
		.reg = *m_registry,
		.sched = *this,
		.states = m_states,
		.resources_store = m_resources_store,
		.channels_store = m_channels_store,
		.channels = writer,
	};

	while (true) {
		const auto pending_order = topo_sort_pending_inits();
		if (pending_order.empty()) {
			break;
		}
		for (const std::size_t idx : pending_order) {
			m_nodes[idx].invoke_initialize_fn(phase, m_nodes[idx].data.get());
			m_nodes[idx].initialized = true;
		}
	}

	check_state_dep_cycles();
}

auto gse::scheduler::topo_sort_pending_inits() const -> std::vector<std::size_t> {
	std::unordered_map<id, std::size_t> id_to_idx;
	id_to_idx.reserve(m_nodes.size() * 2);
	for (std::size_t i = 0; i < m_nodes.size(); ++i) {
		id_to_idx[m_nodes[i].state_id] = i;
		if (m_nodes[i].resources_id.exists()) {
			id_to_idx[m_nodes[i].resources_id] = i;
		}
	}

	std::vector<std::size_t> pending;
	pending.reserve(m_nodes.size());
	for (std::size_t i = 0; i < m_nodes.size(); ++i) {
		if (!m_nodes[i].initialized) {
			pending.push_back(i);
		}
	}

	if (pending.empty()) {
		return pending;
	}

	std::unordered_set<std::size_t> pending_set(pending.begin(), pending.end());
	std::unordered_map<std::size_t, std::size_t> indegree;
	std::unordered_map<std::size_t, std::vector<std::size_t>> dependents;
	indegree.reserve(pending.size());
	dependents.reserve(pending.size());

	for (const std::size_t idx : pending) {
		indegree[idx] = 0;
	}

	for (const std::size_t idx : pending) {
		for (const id dep : m_nodes[idx].init_state_deps) {
			const auto it = id_to_idx.find(dep);
			if (it == id_to_idx.end()) {
				continue;
			}
			const std::size_t dep_idx = it->second;
			if (!pending_set.contains(dep_idx)) {
				continue;
			}
			dependents[dep_idx].push_back(idx);
			++indegree[idx];
		}
	}

	std::vector<std::size_t> ready;
	for (const std::size_t idx : pending) {
		if (indegree[idx] == 0) {
			ready.push_back(idx);
		}
	}
	std::ranges::sort(ready);

	std::vector<std::size_t> sorted;
	sorted.reserve(pending.size());
	std::size_t cursor = 0;
	while (cursor < ready.size()) {
		const std::size_t idx = ready[cursor++];
		sorted.push_back(idx);
		const auto it = dependents.find(idx);
		if (it == dependents.end()) {
			continue;
		}
		std::vector<std::size_t> newly_ready;
		for (const std::size_t dep_idx : it->second) {
			if (--indegree[dep_idx] == 0) {
				newly_ready.push_back(dep_idx);
			}
		}
		std::ranges::sort(newly_ready);
		ready.insert(ready.end(), newly_ready.begin(), newly_ready.end());
	}

	assert(sorted.size() == pending.size(), "init dep cycle detected: not all pending systems could be ordered");
	return sorted;
}

auto gse::scheduler::check_closed_dep_graph() -> void {
	std::unordered_set<id> registered;
	for (const auto& node : m_nodes) {
		registered.insert(node.state_id);
		if (node.resources_id.exists()) {
			registered.insert(node.resources_id);
		}
		if (node.settings_id.exists()) {
			registered.insert(node.settings_id);
		}
	}

	std::vector<std::string> violations;

	auto check_deps = [&](const std::span<const id> deps, const id source, const std::string_view phase_tag) {
		for (const id& dep : deps) {
			if (!registered.contains(dep)) {
				violations.push_back(std::format(
					"system {} {} reads {} but no system registers that state or resources type",
					source,
					phase_tag,
					dep
				));
			}
		}
	};

	for (const auto& node : m_nodes) {
		check_deps(node.update_state_deps, node.state_id, "update()");
		check_deps(node.frame_state_deps, node.state_id, "frame()");
	}

	if (!violations.empty()) {
		std::string message = "closed-graph violation: cross-system dependencies reference unregistered systems\n";
		for (const auto& v : violations) {
			message += "  - " + v + "\n";
		}
		message += "fix: add the missing system to your scheduler config, or remove the parameter from the reader's update/frame signature";
		assert(false, "{}", message);
	}
}

auto gse::scheduler::run_graph_update() -> void {
	trace::scope_guard sg{ trace_id<"scheduler::run_graph_update">() };
	auto writer = m_channels_store.make_writer();

	update_context u_ctx(
		m_states,
		m_resources_store,
		m_channels_store,
		writer,
		m_update_graph,
		*m_registry,
		m_access_mutexes
	);

	std::vector<async::task<>> tasks;
	tasks.reserve(m_nodes.size());
	for (auto& node : m_nodes) {
		tasks.push_back(run_node_update(u_ctx, node));
	}
	{
		trace::scope_guard sg2{ trace_id<"scheduler::update_sync_wait">() };
		async::sync_wait(async::when_all(std::move(tasks)));
	}
}

auto gse::scheduler::update() -> void {
	trace::scope_guard sg{ trace_id<"scheduler::update">() };
	if (!m_dep_graph_checked) {
		check_closed_dep_graph();
		m_dep_graph_checked = true;
	}
	{
		trace::scope_guard sg2{ trace_id<"scheduler::drain_deferred">() };
		drain_deferred();
	}
	{
		trace::scope_guard sg2{ trace_id<"scheduler::apply_settings">() };
		auto writer = m_channels_store.make_writer();
		for (auto& node : m_nodes) {
			if (node.invoke_apply_settings_fn) {
				node.invoke_apply_settings_fn(node.data.get(), m_channels_store, writer);
			}
		}
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
	auto writer = m_channels_store.make_writer();

	frame_context f_ctx(
		m_states,
		m_resources_store,
		m_channels_store,
		writer,
		m_frame_graph,
		*m_registry
	);

	for (auto& node : m_nodes) {
		if (!node.has_frame) {
			m_frame_graph.notify_state_ready(node.state_id);
		}
		if (node.resources_id.exists()) {
			m_frame_graph.notify_state_ready(node.resources_id);
		}
		if (node.settings_id.exists()) {
			m_frame_graph.notify_state_ready(node.settings_id);
		}
	}

	std::vector<async::task<>> tasks;
	{
		trace::scope_guard sg2{ trace_id<"scheduler::collect_frame_tasks">() };
		for (auto& node : m_nodes) {
			if (!node.has_frame) {
				continue;
			}
			tasks.push_back(run_node_frame(f_ctx, node));
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
		.reg = *m_registry,
	};

	while (!m_nodes.empty()) {
		auto& node = m_nodes.back();
		node.invoke_shutdown_fn(phase, node.data.get());
		m_nodes.pop_back();
	}
}

auto gse::scheduler::clear() -> void {
	m_nodes.clear();
	m_states.clear();
	m_resources_store.clear();
	m_channels_store.clear();
	m_state_deps.clear();
	m_initialized = false;
	m_dep_graph_checked = false;
}

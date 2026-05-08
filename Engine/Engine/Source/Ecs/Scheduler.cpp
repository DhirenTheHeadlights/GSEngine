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

auto gse::scheduler::set_advance_hook(std::function<void(id, std::string_view)> fn) -> void {
	m_advance_hook = std::move(fn);
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
	auto run_node_frame(
		frame_context& ctx,
		system_node& node
	) -> async::task<>;

	auto wrap_run_task(
		async::task<> inner,
		async::manual_event* done
	) -> async::task<>;
}

auto gse::wrap_run_task(async::task<> inner, async::manual_event* done) -> async::task<> {
	co_await std::move(inner);
	done->set();
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

	advance_run_systems_during_init();

	check_state_dep_cycles();
}

auto gse::scheduler::advance_run_systems_during_init() -> void {
	trace::scope_guard sg{ trace_id<"scheduler::advance_run_systems_during_init">() };
	while (true) {
		std::vector<async::task<>> tasks;
		tasks.reserve(m_nodes.size());
		for (auto& node : m_nodes) {
			if (!node.invoke_run_fn || node.run_launched) {
				continue;
			}
			bool deps_ready = true;
			for (const id& dep : node.run_state_deps) {
				if (!m_update_graph.is_state_ready(dep)) {
					deps_ready = false;
					break;
				}
			}
			if (!deps_ready) {
				continue;
			}
			tasks.push_back(advance_one_run_system(node));
		}
		if (tasks.empty()) {
			break;
		}
		async::sync_wait(async::when_all(std::move(tasks)));
	}
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
	registered.insert(m_external_resources.begin(), m_external_resources.end());

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
		check_deps(node.run_state_deps, node.state_id, "run()");
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

auto gse::scheduler::run_unified_update() -> void {
	trace::scope_guard sg{ trace_id<"scheduler::run_unified_update">() };

	std::vector<async::task<>> tasks;
	tasks.reserve(m_nodes.size());
	for (auto& node : m_nodes) {
		if (!node.invoke_run_fn) {
			continue;
		}
		if (!node.run_launched) {
			bool deps_ready = true;
			for (const id& dep : node.run_state_deps) {
				if (!m_update_graph.is_state_ready(dep)) {
					deps_ready = false;
					break;
				}
			}
			if (!deps_ready) {
				continue;
			}
		}
		tasks.push_back(advance_one_run_system(node));
	}
	{
		trace::scope_guard sg2{ trace_id<"scheduler::update_sync_wait">() };
		async::sync_wait(async::when_all(std::move(tasks)));
	}
}

auto gse::scheduler::update() -> void {
	trace::scope_guard sg{ trace_id<"scheduler::update">() };
	drain_hot_add_queue();
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
	run_unified_update();
	{
		trace::scope_guard sg2{ trace_id<"scheduler::snapshot_states">() };
		snapshot_all_states();
	}
}

auto gse::scheduler::tick(const bool frame_ok, const std::function<void()>& in_frame) -> void {
	trace::scope_guard sg{ trace_id<"scheduler::tick">() };
	if (!m_initialized) {
		initialize();
	}
	update();
	render(frame_ok, in_frame);
}

auto gse::scheduler::drain_hot_add_queue() -> void {
	std::vector<system_node> drained;
	{
		std::lock_guard lock(m_hot_add_mutex);
		drained.swap(m_hot_add_queue);
	}
	for (auto& node : drained) {
		m_nodes.push_back(std::move(node));
	}
}

auto gse::scheduler::advance_one_run_system(system_node& node) -> async::task<> {
	if (!node.tick_ctx) {
		node.tick_writer = std::make_unique<channel_writer>(m_channels_store.make_writer());
		node.tick_ctx = std::make_unique<run_context>(
			m_states,
			m_resources_store,
			m_channels_store,
			*node.tick_writer,
			m_update_graph,
			*m_registry,
			m_access_mutexes,
			*node.tick_event,
			*node.tick_done_event,
			node.is_in_update_loop
		);
	}

	for (const id& dep : node.run_state_deps) {
		co_await m_update_graph.wait_state_ready(dep);
	}

	if (m_advance_hook) {
		m_advance_hook(node.state_id, "before");
	}

	if (!node.run_launched) {
		node.run_launched = true;
		node.run_task = wrap_run_task(
			node.invoke_run_fn(*node.tick_ctx, node.data.get()),
			node.tick_done_event.get()
		);
		node.run_task.start();
	}
	else if (!node.run_task.done()) {
		node.tick_done_event->reset();
		node.tick_event->set();
		co_await node.tick_done_event->wait();
	}

	if (m_advance_hook) {
		m_advance_hook(node.state_id, "after");
	}

	if (node.is_in_update_loop || node.run_task.done()) {
		m_update_graph.notify_state_ready(node.state_id);
		if (node.resources_id.exists()) {
			m_update_graph.notify_state_ready(node.resources_id);
		}
		if (node.settings_id.exists()) {
			m_update_graph.notify_state_ready(node.settings_id);
		}
	}
}

auto gse::scheduler::render(const bool frame_ok, const std::function<void()>& in_frame) -> void {
	if (!frame_ok) {
		return;
	}

	for (const auto& node : m_nodes) {
		if (!node.has_frame) {
			continue;
		}
		if (!node.run_launched) {
			return;
		}
		if (!node.is_in_update_loop && !node.run_task.done()) {
			return;
		}
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
	for (const id& type_id : m_external_resources) {
		m_frame_graph.notify_state_ready(type_id);
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
	m_external_resources.clear();
	m_initialized = false;
	m_dep_graph_checked = false;
}

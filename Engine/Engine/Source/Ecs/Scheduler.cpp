module gse.ecs;

import std;

import gse.assert;
import gse.core;
import gse.concurrency;
import gse.time;
import gse.diag;
import gse.log;

auto gse::scheduler::set_gpu_context(void* ctx) -> void {
	m_gpu_ctx = ctx;
}

auto gse::scheduler::set_asset_registry(void* reg) -> void {
	m_asset_registry = reg;
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
	const auto state_tag = node.state_id.tag();
	log::println(log::category::runtime, "[freeze-trace] node update enter: '{}' deps={}", state_tag, node.update_state_deps.size());
	for (const id& dep : node.update_state_deps) {
		log::println(log::category::runtime, "[freeze-trace] node update '{}': awaiting dep '{}'", state_tag, dep.tag());
		co_await ctx.after_id(dep);
	}
	log::println(log::category::runtime, "[freeze-trace] node update '{}': invoking", state_tag);
	co_await node.invoke_update_fn(ctx, node.data.get());
	log::println(log::category::runtime, "[freeze-trace] node update '{}': invoke returned", state_tag);
	ctx.notify_ready_by_id(node.state_id);
	if (node.resources_id.exists()) {
		ctx.notify_ready_by_id(node.resources_id);
	}
	log::println(log::category::runtime, "[freeze-trace] node update exit: '{}'", state_tag);
}

auto gse::run_node_frame(frame_context& ctx, system_node& node) -> async::task<> {
	const auto state_tag = node.state_id.tag();
	log::println(log::category::runtime, "[freeze-trace] node frame enter: '{}' deps={}", state_tag, node.frame_state_deps.size());
	const auto eid = trace::begin_block(node.frame_wall_id, 0);
	auto guard = make_scope_exit([fwid = node.frame_wall_id, eid] {
		trace::end_block(fwid, eid, 0);
	});

	for (const id& dep : node.frame_state_deps) {
		log::println(log::category::runtime, "[freeze-trace] node frame '{}': awaiting dep '{}'", state_tag, dep.tag());
		co_await ctx.after_id(dep);
	}
	log::println(log::category::runtime, "[freeze-trace] node frame '{}': invoking", state_tag);
	co_await node.invoke_frame_fn(ctx, node.data.get());
	log::println(log::category::runtime, "[freeze-trace] node frame exit: '{}'", state_tag);
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
	frame_sync::on_begin([this] {
		m_channels_store.take_snapshot_all();
	});

	m_initialized = true;

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

	for (std::size_t i = 0; i < m_nodes.size(); ++i) {
		if (m_nodes[i].initialized) {
			continue;
		}
		m_nodes[i].invoke_initialize_fn(phase, m_nodes[i].data.get());
		m_nodes[i].initialized = true;
	}

	check_state_dep_cycles();
	check_closed_dep_graph();
}

auto gse::scheduler::check_closed_dep_graph() -> void {
	std::unordered_set<id> registered;
	for (const auto& node : m_nodes) {
		registered.insert(node.state_id);
		if (node.resources_id.exists()) {
			registered.insert(node.resources_id);
		}
	}

	std::vector<std::string> violations;

	auto check_deps = [&](const std::span<const id> deps, const id source, const std::string_view phase_tag) {
		for (const id& dep : deps) {
			if (!registered.contains(dep)) {
				violations.push_back(std::format(
					"system '{}' {} reads '{}' but no system registers that state or resources type",
					source.tag(),
					phase_tag,
					dep.tag()
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
		m_gpu_ctx,
		m_asset_registry,
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
		log::println(log::category::runtime, "[freeze-trace] scheduler: update sync_wait enter ({} tasks)", tasks.size());
		async::sync_wait(async::when_all(std::move(tasks)));
		log::println(log::category::runtime, "[freeze-trace] scheduler: update sync_wait exit");
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
	auto writer = m_channels_store.make_writer();

	frame_context f_ctx(
		m_gpu_ctx,
		m_asset_registry,
		m_states,
		m_resources_store,
		m_channels_store,
		writer,
		m_frame_graph,
		*m_registry
	);

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
		log::println(log::category::runtime, "[freeze-trace] scheduler: start_frame_tasks enter ({} tasks)", tasks.size());
		task::group group(trace_id<"scheduler::start_frame_tasks">());
		for (auto& t : tasks) {
			group.post([t_ptr = std::addressof(t)] {
				t_ptr->start();
			});
		}
		group.wait();
		log::println(log::category::runtime, "[freeze-trace] scheduler: start_frame_tasks exit");
	}

	if (in_frame) {
		trace::scope_guard sg2{ trace_id<"scheduler::in_frame_callback">() };
		log::println(log::category::runtime, "[freeze-trace] scheduler: in_frame callback enter");
		in_frame();
		log::println(log::category::runtime, "[freeze-trace] scheduler: in_frame callback exit");
	}

	if (!tasks.empty()) {
		{
			trace::scope_guard sg2{ trace_id<"scheduler::frame_sync_wait">() };
			log::println(log::category::runtime, "[freeze-trace] scheduler: frame sync_wait enter");
			async::sync_wait(async::when_all(std::move(tasks)));
			log::println(log::category::runtime, "[freeze-trace] scheduler: frame sync_wait exit");
		}
		{
			trace::scope_guard sg2{ trace_id<"scheduler::frame_graph_clear">() };
			m_frame_graph.clear();
		}
	}
}

auto gse::scheduler::shutdown() -> void {
	shutdown_context phase{
		.gpu_ctx = m_gpu_ctx,
		.assets_ptr = m_asset_registry,
		.reg = *m_registry,
	};

	for (auto it = m_nodes.rbegin(); it != m_nodes.rend(); ++it) {
		it->invoke_shutdown_fn(phase, it->data.get());
	}
}

auto gse::scheduler::clear() -> void {
	m_nodes.clear();
	m_states.clear();
	m_resources_store.clear();
	m_channels_store.clear();
	m_state_deps.clear();
	m_initialized = false;
}

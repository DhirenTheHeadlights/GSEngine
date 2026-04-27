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
		m_channels_store.take_snapshot_all();
	});

	auto writer = m_channels_store.make_writer();
	init_context phase{
		.gpu_ctx = m_gpu_ctx,
		.reg = *m_registry,
		.sched = *this,
		.states = m_states,
		.resources_store = m_resources_store,
		.channels_store = m_channels_store,
		.channels = writer,
	};

	for (std::size_t i = 0; i < m_nodes.size(); ++i) {
		m_nodes[i]->initialize(phase);
	}

	m_initialized = true;
}

auto gse::scheduler::run_graph_update() -> void {
	trace::scope_guard sg{ trace_id<"scheduler::run_graph_update">() };
	auto writer = m_channels_store.make_writer();

	update_context u_ctx(
		m_gpu_ctx,
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
	auto writer = m_channels_store.make_writer();

	frame_context f_ctx(
		m_gpu_ctx,
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
		.gpu_ctx = m_gpu_ctx,
		.reg = *m_registry,
	};

	for (auto it = m_nodes.rbegin(); it != m_nodes.rend(); ++it) {
		(*it)->shutdown(phase);
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

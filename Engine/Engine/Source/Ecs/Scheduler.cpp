module gse.ecs;

import std;

import gse.assert;
import gse.core;
import gse.concurrency;
import gse.time;
import gse.math;
import gse.diag;
import gse.log;

auto gse::scheduler::set_registry(registry& reg) -> void {
	m_registry = &reg;
}

auto gse::scheduler::set_advance_hook(std::function<void(id, std::string_view)> fn) -> void {
	m_advance_hook = std::move(fn);
}

auto gse::scheduler::make_channel_writer() -> channel_writer {
	return m_channels_store.make_writer();
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

	template <std::invocable OnComplete>
	auto wrap_run_task(
		async::task<> inner,
		OnComplete on_complete
	) -> async::task<> {
		co_await std::move(inner);
		on_complete();
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
	m_initialized = true;
	m_channels_store.flip_all();

	advance_run_systems_during_init();

	check_state_dep_cycles();
}

auto gse::scheduler::advance_run_systems_during_init() -> void {
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
		sync_wait_or_dump(std::move(tasks), wait_phase::init);
	}
}

auto gse::scheduler::check_closed_dep_graph() -> void {
	std::unordered_set<id> registered;
	for (const auto& node : m_nodes) {
		registered.insert(node.state_id);
		if (node.state_type_id.exists()) {
			registered.insert(node.state_type_id);
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
	{
		trace::scope_guard sg_dispatch{ trace_id<"sched::run_dispatch">() };
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
	}
	{
		trace::scope_guard sg_wait{ trace_id<"sched::run_wait">() };
		sync_wait_or_dump(std::move(tasks), wait_phase::update);
	}
}

auto gse::scheduler::update() -> void {
	trace::scope_guard sg{ trace_id<"scheduler::update">() };
	{
		trace::scope_guard sg_drain{ trace_id<"sched::drain_hot_add">() };
		drain_hot_add_queue();
	}
	if (!m_dep_graph_checked) {
		check_closed_dep_graph();
		m_dep_graph_checked = true;
	}
	{
		trace::scope_guard sg_apply{ trace_id<"sched::apply_settings">() };
		auto writer = m_channels_store.make_writer();
		for (auto& node : m_nodes) {
			if (node.invoke_apply_settings_fn) {
				node.invoke_apply_settings_fn(node.data.get(), m_channels_store, writer);
			}
		}
	}
	run_unified_update();
	{
		trace::scope_guard sg_snap{ trace_id<"sched::snapshot_all">() };
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
		register_node(std::move(node));
	}
}

auto gse::scheduler::register_node(system_node node) -> void* {
	const auto canonical_idx = node.state_id;
	auto* state_ptr = node.state_ptr;
	m_states.register_state(canonical_idx, node.state_ptr, node.state_snapshot_ptr);

	if (node.state_type_id.exists() && node.state_type_id != canonical_idx) {
		m_states.register_state(node.state_type_id, node.state_ptr, node.state_snapshot_ptr);
	}

	auto combined_deps = node.run_state_deps;
	combined_deps.insert(combined_deps.end(), node.frame_state_deps.begin(), node.frame_state_deps.end());
	m_state_deps.emplace(canonical_idx, std::move(combined_deps));

	m_nodes.push_back(std::move(node));
	return state_ptr;
}

auto gse::scheduler::advance_one_run_system(system_node& node) -> async::task<> {
	node.advance_in_flight = true;
	auto in_flight_guard = make_scope_exit([&node] {
		node.advance_in_flight = false;
	});

	if (!node.tick_ctx) {
		node.tick_writer = std::make_unique<channel_writer>(m_channels_store.make_writer());
		node.tick_ctx = std::make_unique<run_context>(
			*this,
			m_states,
			m_resources_store,
			m_channels_store,
			*node.tick_writer,
			m_update_graph,
			*m_registry,
			m_access_mutexes,
			*node.resume_event,
			*node.paused_event,
			node.is_in_update_loop,
			node.settled
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
			[&node] {
				node.settled = true;
				node.paused_event->set();
			}
		);
		node.run_task.start();
	}
	else if (!node.run_task.done()) {
		node.paused_event->reset();
		node.resume_event->set();
		co_await node.paused_event->wait();
	}

	if (m_advance_hook) {
		m_advance_hook(node.state_id, "after");
	}

	if (node.settled) {
		m_update_graph.notify_state_ready(node.state_id);
		if (node.state_type_id.exists() && node.state_type_id != node.state_id) {
			m_update_graph.notify_state_ready(node.state_type_id);
		}
	}
}

auto gse::scheduler::sync_wait_or_dump(std::vector<async::task<>>&& tasks, const wait_phase phase) -> void {
	if (tasks.empty()) {
		return;
	}

	const auto budget = [phase] {
		if (phase == wait_phase::init) {
			return milliseconds(2000.f);
		}
		if (phase == wait_phase::frame) {
			return milliseconds(250.f);
		}
		return milliseconds(500.f);
	}();

	std::atomic<bool> done_flag{ false };
	auto wrapper = [&]() -> async::task<> {
		co_await async::when_all(std::move(tasks));
		done_flag.store(true, std::memory_order_release);
	};
	auto w = wrapper();
	w.start();

	clock wait_clock;
	auto next_dump_at = budget;
	int dump_count = 0;

	while (!done_flag.load(std::memory_order_acquire)) {
		if (!task::try_run_one()) {
			std::this_thread::yield();
		}
		const auto elapsed = wait_clock.elapsed<float>();
		if (elapsed >= next_dump_at) {
			++dump_count;
			log_stall_state(phase, elapsed, dump_count);
			log::flush();
			next_dump_at = elapsed + budget;
		}
	}
	while (!w.done()) {
		std::this_thread::yield();
	}
}

auto gse::scheduler::log_stall_state(const wait_phase phase, const time_t<float> elapsed, const int dump_count) -> void {
	constexpr std::array<std::string_view, 3> phase_names{ "init", "update", "frame" };
	const auto phase_name = phase_names[static_cast<std::size_t>(phase)];

	if (phase == wait_phase::frame) {
		for (const auto& node : m_nodes) {
			if (!node.has_frame) {
				continue;
			}
			if (m_frame_graph.is_state_ready(node.state_id)) {
				continue;
			}
			std::string missing;
			for (const id& dep : node.frame_state_deps) {
				if (!m_frame_graph.is_state_ready(dep)) {
					if (!missing.empty()) {
						missing += ", ";
					}
					missing += std::format("{}", dep);
				}
			}
			if (missing.empty()) {
				log::println(
					log::level::error,
					log::category::runtime,
					"STALL [phase={} elapsed={::ms} dump=#{}] frame system {} stuck inside frame() (all deps ready)",
					phase_name, elapsed, dump_count, node.trace_id
				);
			}
			else {
				log::println(
					log::level::error,
					log::category::runtime,
					"STALL [phase={} elapsed={::ms} dump=#{}] frame system {} waiting on deps: {}",
					phase_name, elapsed, dump_count, node.trace_id, missing
				);
			}
		}
		return;
	}

	for (const auto& node : m_nodes) {
		if (!node.invoke_run_fn) {
			continue;
		}
		if (!node.advance_in_flight) {
			continue;
		}
		if (!node.run_launched) {
			std::string missing;
			for (const id& dep : node.run_state_deps) {
				if (!m_update_graph.is_state_ready(dep)) {
					if (!missing.empty()) {
						missing += ", ";
					}
					missing += std::format("{}", dep);
				}
			}
			if (missing.empty()) {
				log::println(
					log::level::error,
					log::category::runtime,
					"STALL [phase={} elapsed={::ms} dump=#{}] system {} advance entered but not launched (no deps blocking)",
					phase_name, elapsed, dump_count, node.trace_id
				);
			}
			else {
				log::println(
					log::level::error,
					log::category::runtime,
					"STALL [phase={} elapsed={::ms} dump=#{}] system {} waiting on upstream deps: {}",
					phase_name, elapsed, dump_count, node.trace_id, missing
				);
			}
		}
		else if (!node.settled) {
			log::println(
				log::level::warning,
				log::category::runtime,
				"STALL [phase={} elapsed={::ms} dump=#{}] system {} tick in-flight, first next_tick() not reached yet (slow init or stuck in initial setup)",
				phase_name, elapsed, dump_count, node.trace_id
			);
		}
		else {
			log::println(
				log::level::error,
				log::category::runtime,
				"STALL [phase={} elapsed={::ms} dump=#{}] system {} tick in-flight past budget; user coroutine has not yielded back this tick",
				phase_name, elapsed, dump_count, node.trace_id
			);
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
		if (!node.settled) {
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
			if (node.state_type_id.exists() && node.state_type_id != node.state_id) {
				m_frame_graph.notify_state_ready(node.state_type_id);
			}
		}
	}
	for (const id& type_id : m_external_resources) {
		m_frame_graph.notify_state_ready(type_id);
	}

	std::vector<async::task<>> tasks;
	for (auto& node : m_nodes) {
		if (!node.has_frame) {
			continue;
		}
		tasks.push_back(run_node_frame(f_ctx, node));
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
		in_frame();
	}

	if (!tasks.empty()) {
		sync_wait_or_dump(std::move(tasks), wait_phase::frame);
		m_frame_graph.clear();
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

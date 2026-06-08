module gse.ecs:scheduler_impl;

import std;

import :scheduler;
import :phase_context;
import :registries;
import :run_context;
import :settings;
import :frame_context;
import :system_node;
import :system_dispatch;
import :registry;

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

auto gse::scheduler::set_settings_register_hook(std::function<void(settings::register_settings_type)> fn) -> void {
	m_settings_register_hook = std::move(fn);
}

auto gse::scheduler::current_phase() const -> scheduler_phase {
	return m_phase;
}

auto gse::scheduler::enter_running() -> void {
	assert(m_phase == scheduler_phase::boot, "scheduler::enter_running requires boot phase");
	m_phase = scheduler_phase::running;
}

auto gse::scheduler::enter_shutdown() -> void {
	m_phase = scheduler_phase::shutdown;
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
	struct wait_section_info {
		id section_id;
		time budget;
	};

	auto wait_section_for(
		wait_phase phase
	) -> wait_section_info;

	auto run_node_frame(
		frame_context& ctx,
		system_node& node
	) -> async::task<>;

	template <std::invocable OnComplete>
	auto wrap_run_task(async::task<> inner, OnComplete on_complete) -> async::task<> {
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

auto gse::scheduler::wire_component_deps() -> void {
	std::unordered_map<id, std::vector<std::pair<std::size_t, id>>> writers;
	{
		std::size_t idx = 0;
		for (const auto& node : m_nodes) {
			for (const id w : node.component_writes) {
				writers[w].emplace_back(idx, node.state_id);
			}
			++idx;
		}
	}

	std::size_t idx = 0;
	for (auto& node : m_nodes) {
		auto depend_on_earlier_writers = [&](const id comp) {
			const auto it = writers.find(comp);
			if (it == writers.end()) {
				return;
			}
			for (const auto& [writer_idx, writer_state] : it->second) {
				if (writer_idx >= idx || writer_state == node.state_id) {
					continue;
				}
				if (std::ranges::find(node.run_state_deps, writer_state) == node.run_state_deps.end()) {
					node.run_state_deps.push_back(writer_state);
				}
			}
		};
		for (const id r : node.component_reads) {
			depend_on_earlier_writers(r);
		}
		for (const id w : node.component_writes) {
			depend_on_earlier_writers(w);
		}
		++idx;
	}

	m_state_deps.clear();
	for (const auto& node : m_nodes) {
		auto combined = node.run_state_deps;
		combined.insert(combined.end(), node.init_state_deps.begin(), node.init_state_deps.end());
		combined.insert(combined.end(), node.frame_state_deps.begin(), node.frame_state_deps.end());
		m_state_deps.emplace(node.state_id, std::move(combined));
	}
}

auto gse::scheduler::check_state_dep_cycles() -> void {
	enum class color : std::uint8_t {
		white,
		gray,
		black
	};
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
					assert(false, "state_deps cycle detected: {}", format_cycle(dep));
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

	run_init_phase();

	check_state_dep_cycles();
}

auto gse::scheduler::all_settled() const -> bool {
	{
		std::lock_guard lock(m_hot_add_mutex);
		if (!m_hot_add_queue.empty()) {
			return false;
		}
	}
	for (const auto& node : m_nodes) {
		if (node.invoke_init_fn && !node.init_done) {
			return false;
		}
		if (node.invoke_run_fn && !node.ran_once) {
			return false;
		}
	}
	return true;
}

auto gse::scheduler::settle_progress() const -> settle_stats {
	settle_stats stats{};
	{
		std::lock_guard lock(m_hot_add_mutex);
		for (const auto& node : m_hot_add_queue) {
			if (node.invoke_run_fn || node.invoke_init_fn) {
				++stats.total;
			}
		}
	}
	for (const auto& node : m_nodes) {
		if (!node.invoke_run_fn && !node.invoke_init_fn) {
			continue;
		}
		++stats.total;
		const bool init_ok = !node.invoke_init_fn || node.init_done;
		const bool run_ok = !node.invoke_run_fn || node.ran_once;
		if (init_ok && run_ok) {
			++stats.settled;
		}
	}
	return stats;
}

auto gse::scheduler::dep_init_done(const id dep) const -> bool {
	for (const auto& node : m_nodes) {
		if (node.state_id == dep || node.state_type_id == dep) {
			return node.init_done;
		}
	}
	return true;
}

auto gse::scheduler::is_dispatchable(const id node_id) const -> bool {
	const system_node* found = nullptr;
	for (const auto& node : m_nodes) {
		if (node.state_id == node_id || node.state_type_id == node_id) {
			found = &node;
			break;
		}
	}
	if (!found) {
		return true;
	}
	if (!found->init_done) {
		return false;
	}
	if (!found->invoke_run_fn) {
		return true;
	}
	for (const id& dep : found->run_state_deps) {
		if (!is_dispatchable(dep)) {
			return false;
		}
	}
	return true;
}

auto gse::scheduler::run_init_phase() -> void {
	while (true) {
		std::size_t before = 0;
		for (const auto& node : m_nodes) {
			before += node.init_launched ? 1 : 0;
			before += node.init_done ? 1 : 0;
		}
		advance_inits();
		std::size_t after = 0;
		for (const auto& node : m_nodes) {
			after += node.init_launched ? 1 : 0;
			after += node.init_done ? 1 : 0;
		}
		if (after == before) {
			break;
		}
	}
}

auto gse::scheduler::advance_inits() -> void {
	std::vector<async::task<>> tasks;
	for (auto& node : m_nodes) {
		if (!node.invoke_init_fn || node.init_done || node.init_in_flight) {
			continue;
		}
		if (!node.init_launched) {
			bool deps_ready = true;
			for (const id& dep : node.init_state_deps) {
				if (!dep_init_done(dep)) {
					deps_ready = false;
					break;
				}
			}
			if (!deps_ready) {
				continue;
			}
		}
		tasks.push_back(advance_one_init_system(node));
	}
	if (!tasks.empty()) {
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
				violations.push_back(
					std::format(
						"system {} {} reads {} but no system registers that state or resources type",
						source,
						phase_tag,
						dep
					)
				);
			}
		}
	};

	for (const auto& node : m_nodes) {
		check_deps(node.run_state_deps, node.state_id, "run()");
		check_deps(node.init_state_deps, node.state_id, "init()");
		check_deps(node.frame_state_deps, node.state_id, "frame()");
	}

	if (!violations.empty()) {
		std::string message = "closed-graph violation: cross-system dependencies reference unregistered systems\n";
		for (const auto& v : violations) {
			message += "  - " + v + "\n";
		}
		message += "fix: add the missing system to your scheduler config, or remove the parameter from the reader's "
				   "update/frame signature";
		assert(false, "{}", message);
	}
}

auto gse::scheduler::dispatch_run_systems() -> void {
	trace::scope_guard sg{ trace_id<"scheduler::dispatch_run_systems">() };

	for (auto& node : m_nodes) {
		if (!node.invoke_run_fn || !is_dispatchable(node.state_id)) {
			continue;
		}
		m_update_graph.reset_state(node.state_id);
		if (node.state_type_id.exists() && node.state_type_id != node.state_id) {
			m_update_graph.reset_state(node.state_type_id);
		}
	}

	async::manual_event dummy_resume;
	async::manual_event dummy_paused;
	auto writer = m_channels_store.make_writer();

	std::vector<std::unique_ptr<run_context>> contexts;
	std::vector<async::task<>> tasks;
	contexts.reserve(m_nodes.size());
	tasks.reserve(m_nodes.size());
	{
		trace::scope_guard sg_dispatch{ trace_id<"sched::run_dispatch">() };
		for (auto& node : m_nodes) {
			if (!node.invoke_run_fn || !is_dispatchable(node.state_id)) {
				continue;
			}
			auto& ctx = *contexts.emplace_back(std::make_unique<run_context>(
				*this,
				m_states,
				m_resources_store,
				m_channels_store,
				writer,
				m_update_graph,
				*m_registry,
				m_access_mutexes,
				dummy_resume,
				dummy_paused
			));
			ctx.set_access_lint(&node.lint);
			tasks.push_back(run_node_update(ctx, node));
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
		wire_component_deps();
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
	advance_inits();
	dispatch_run_systems();
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
	if (drained.empty()) {
		return;
	}
	for (auto& node : drained) {
		register_node(std::move(node));
	}
	m_dep_graph_checked = false;
}

auto gse::scheduler::register_node(system_node node) -> void* {
	const auto canonical_idx = node.state_id;
	auto* state_ptr = node.state_ptr;
	m_states.register_state(canonical_idx, node.state_ptr, node.state_snapshot_ptr);

	if (node.state_type_id.exists() && node.state_type_id != canonical_idx) {
		m_states.register_state(node.state_type_id, node.state_ptr, node.state_snapshot_ptr);
	}

	auto combined_deps = node.run_state_deps;
	combined_deps.insert(combined_deps.end(), node.init_state_deps.begin(), node.init_state_deps.end());
	combined_deps.insert(combined_deps.end(), node.frame_state_deps.begin(), node.frame_state_deps.end());
	m_state_deps.emplace(canonical_idx, std::move(combined_deps));

	{
		const auto fmt_deps = [](const std::vector<id>& deps) {
			std::string s;
			for (const auto& dep : deps) {
				s += std::format("{} ", dep);
			}
			return s;
		};
		log::println(
			"[regnode] {} state_id={} type_id={} init=[ {}] run=[ {}]",
			node.system_name,
			node.state_id,
			node.state_type_id,
			fmt_deps(node.init_state_deps),
			fmt_deps(node.run_state_deps)
		);
		log::flush();
	}

	const bool has_run = node.invoke_run_fn != nullptr;
	const auto state_id = node.state_id;
	const auto state_type_id = node.state_type_id;

	if (node.settings_record && m_settings_register_hook) {
		m_settings_register_hook(std::move(*node.settings_record));
	}

	m_nodes.push_back(std::move(node));

	if (!has_run) {
		m_update_graph.notify_state_ready(state_id);
		if (state_type_id.exists() && state_type_id != state_id) {
			m_update_graph.notify_state_ready(state_type_id);
		}
	}

	return state_ptr;
}

auto gse::scheduler::advance_one_init_system(system_node& node) -> async::task<> {
	node.init_in_flight = true;
	auto in_flight_guard = make_scope_exit([&node] {
		node.init_in_flight = false;
	});

	if (!node.init_ctx) {
		node.init_writer = std::make_unique<channel_writer>(m_channels_store.make_writer());
		node.init_ctx = std::make_unique<run_context>(
			*this,
			m_states,
			m_resources_store,
			m_channels_store,
			*node.init_writer,
			m_update_graph,
			*m_registry,
			m_access_mutexes,
			*node.resume_event,
			*node.paused_event
		);
	}

	if (!node.init_launched) {
		std::string dep_dump;
		for (const auto& dep : node.init_state_deps) {
			dep_dump += std::format("{} ", dep);
		}
		log::println(
			"[init-launch] {} state_id={} type_id={} init_deps=[ {}]",
			node.system_name,
			node.state_id,
			node.state_type_id,
			dep_dump
		);
		log::flush();
		node.init_launched = true;
		node.init_task = wrap_run_task(
			node.invoke_init_fn(*node.init_ctx, node.data.get()),
			[&node] {
				node.init_done = true;
				node.paused_event->set();
			}
		);
		node.init_task.start();
	}
	else if (!node.init_task.done()) {
		node.paused_event->reset();
		node.resume_event->set();
		co_await node.paused_event->wait();
	}
}

auto gse::scheduler::run_node_update(run_context& ctx, system_node& node) -> async::task<> {
	const auto eid = trace::begin_block(node.update_wall_id, 0);
	auto guard = make_scope_exit([wid = node.update_wall_id, eid] {
		trace::end_block(wid, eid, 0);
	});

	for (const id& dep : node.run_state_deps) {
		co_await m_update_graph.wait_state_ready(dep);
	}

	node.lint.acquires_this_run = 0;
	node.lint.structural_this_run = false;

	if (m_advance_hook) {
		m_advance_hook(node.state_id, "before");
	}

	co_await node.invoke_run_fn(ctx, node.data.get());
	node.ran_once = true;
	warn_if_whole_tick_acquire(node);

	if (m_advance_hook) {
		m_advance_hook(node.state_id, "after");
	}

	m_update_graph.notify_state_ready(node.state_id);
	if (node.state_type_id.exists() && node.state_type_id != node.state_id) {
		m_update_graph.notify_state_ready(node.state_type_id);
	}
}

auto gse::scheduler::warn_if_whole_tick_acquire(system_node& node) -> void {
	auto& lint = node.lint;
	++lint.runs;
	if (lint.acquires_this_run > 0) {
		++lint.acquire_runs;
	}
	if (lint.acquires_this_run > 1) {
		lint.multi_acquire_seen = true;
	}
	if (lint.structural_this_run) {
		lint.structural_seen = true;
	}

	constexpr std::uint32_t warmup_ticks = 30;
	const bool declares_access = !node.component_reads.empty() || !node.component_writes.empty();
	const bool whole_tick_acquire =
		!declares_access &&
		lint.runs >= warmup_ticks &&
		lint.acquire_runs == lint.runs &&
		!lint.multi_acquire_seen &&
		!lint.structural_seen;
	if (whole_tick_acquire && !lint.warned) {
		lint.warned = true;
		log::println(
			"access lint: system '{}' acquires component locks unconditionally for the entire tick with no "
			"structural changes; declare read<>/write<> run() parameters instead of calling ctx.acquire()",
			node.system_name
		);
	}
}

auto gse::wait_section_for(const wait_phase phase) -> wait_section_info {
	if (phase == wait_phase::init) {
		return { trace_id<"sched::wait::init">(), milliseconds(2000.f) };
	}
	if (phase == wait_phase::frame) {
		return { trace_id<"sched::wait::frame">(), milliseconds(250.f) };
	}
	return { trace_id<"sched::wait::update">(), milliseconds(500.f) };
}

auto gse::scheduler::sync_wait_or_dump(std::vector<async::task<>>&& tasks, const wait_phase phase) -> void {
	if (tasks.empty()) {
		return;
	}

	std::atomic<bool> done_flag{ false };
	auto wrapper = [&]() -> async::task<> {
		co_await async::when_all(std::move(tasks));
		done_flag.store(true, std::memory_order_release);
	};
	auto w = wrapper();
	w.start();

	const auto [section_id, budget] = wait_section_for(phase);
	watchdog::section watch{ section_id, budget };

	clock wait_clock;
	auto seen_pulse = watchdog::dump_pulse();
	int dump_count = 0;

	while (!done_flag.load(std::memory_order_acquire)) {
		if (!task::try_run_one()) {
			std::this_thread::yield();
		}
		if (const auto pulse = watchdog::dump_pulse(); pulse != seen_pulse) {
			seen_pulse = pulse;
			++dump_count;
			log_stall_state(phase, wait_clock.elapsed<float>(), dump_count);
			log::flush();
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
					phase_name,
					elapsed,
					dump_count,
					node.trace_id
				);
			}
			else {
				log::println(
					log::level::error,
					log::category::runtime,
					"STALL [phase={} elapsed={::ms} dump=#{}] frame system {} waiting on deps: {}",
					phase_name,
					elapsed,
					dump_count,
					node.trace_id,
					missing
				);
			}
		}
		return;
	}

	for (const auto& node : m_nodes) {
		if (!node.invoke_init_fn || node.init_done) {
			continue;
		}
		log::println(
			log::level::warning,
			log::category::runtime,
			"STALL [phase={} elapsed={::ms} dump=#{}] system {} init() not complete (slow load or stuck setup)",
			phase_name,
			elapsed,
			dump_count,
			node.trace_id
		);
	}

	for (const auto& node : m_nodes) {
		if (!node.invoke_run_fn || !node.init_done) {
			continue;
		}
		if (m_update_graph.is_state_ready(node.state_id)) {
			continue;
		}
		std::string missing;
		for (const id& dep : node.run_state_deps) {
			if (!dep_init_done(dep)) {
				continue;
			}
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
				"STALL [phase={} elapsed={::ms} dump=#{}] system {} stuck inside run() (all deps ready)",
				phase_name,
				elapsed,
				dump_count,
				node.trace_id
			);
		}
		else {
			log::println(
				log::level::error,
				log::category::runtime,
				"STALL [phase={} elapsed={::ms} dump=#{}] system {} waiting on upstream deps: {}",
				phase_name,
				elapsed,
				dump_count,
				node.trace_id,
				missing
			);
		}
	}
}

auto gse::scheduler::render(const bool frame_ok, const std::function<void()>& in_frame) -> void {
	if (!frame_ok) {
		return;
	}

	trace::scope_guard sg{ trace_id<"scheduler::render">() };
	auto writer = m_channels_store.make_writer();

	frame_context f_ctx(m_states, m_resources_store, m_channels_store, writer, m_frame_graph, *m_registry);

	for (auto& node : m_nodes) {
		const bool run_satisfied = node.ran_once || !node.invoke_run_fn;
		const bool frame_ready = node.has_frame && node.init_done && run_satisfied;
		if (frame_ready) {
			continue;
		}
		m_frame_graph.notify_state_ready(node.state_id);
		if (node.state_type_id.exists() && node.state_type_id != node.state_id) {
			m_frame_graph.notify_state_ready(node.state_type_id);
		}
	}
	for (const id& type_id : m_external_resources) {
		m_frame_graph.notify_state_ready(type_id);
	}

	std::vector<async::task<>> tasks;
	std::vector<system_node*> task_nodes;
	for (auto& node : m_nodes) {
		if (!node.has_frame) {
			continue;
		}
		if (!node.init_done || (node.invoke_run_fn && !node.ran_once)) {
			continue;
		}
		tasks.push_back(run_node_frame(f_ctx, node));
		task_nodes.push_back(&node);
	}

	if (!tasks.empty()) {
		trace::scope_guard sg2{ trace_id<"scheduler::start_frame_tasks">() };
		task::group group(trace_id<"scheduler::start_frame_tasks">());
		for (std::size_t i = 0; i < tasks.size(); ++i) {
			group.post(
				[t_ptr = std::addressof(tasks[i])] {
					t_ptr->start();
				},
				task_nodes[i]->frame_start_id
			);
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
	m_phase = scheduler_phase::boot;
}

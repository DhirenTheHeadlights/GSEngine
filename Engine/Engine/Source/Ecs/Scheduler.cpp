module gse.ecs:scheduler_impl;

import std;

import :scheduler;
import :registries;
import :context;
import :settings;
import :context;
import :system_node;
import :system_dispatch;
import :registry;
import :task_graph;
import :access_token;

import gse.assert;
import gse.core;
import gse.concurrency;
import gse.time;
import gse.math;
import gse.diag;
import gse.log;
import gse.introspection;

auto gse::scheduler::set_registry(registry& reg) -> void {
	m_registry = &reg;
}

auto gse::scheduler::set_settings_register_hook(std::function<void(settings::register_settings_type)> fn) -> void {
	m_settings_register_hook = std::move(fn);
}

auto gse::scheduler::current_phase() const -> scheduler_phase {
	return m_phase;
}

auto gse::scheduler::snapshot_graph() const -> introspection::system_graph {
	introspection::system_graph graph;
	graph.nodes.reserve(m_nodes.size());

	const auto resolve_name = [](const id value) -> std::string {
		if (value.exists() && gse::exists(value.number())) {
			std::string tag(value.tag());
			if (const auto suffix = tag.find('#'); suffix != std::string::npos) {
				tag.erase(suffix);
			}
			return tag;
		}
		return std::format("#{}", value.number());
	};

	const auto derive_category = [](const std::string& name) -> std::string {
		const auto first = name.find("::");
		if (first == std::string::npos) {
			return name;
		}
		const auto second = name.find("::", first + 2);
		if (second == std::string::npos) {
			return name.substr(0, first);
		}
		return name.substr(first + 2, second - (first + 2));
	};

	for (const auto& node : m_nodes) {
		introspection::graph_node gn{
			.id = node.state_id.number(),
			.name = node.system_name,
			.display = node.display_name,
			.category = derive_category(node.system_name),
			.file = node.def_file,
			.line = node.def_line,
			.column = node.def_column,
			.has_init = node.invoke_init_fn != nullptr,
			.has_run = node.invoke_run_fn != nullptr,
			.has_frame = node.has_frame,
			.deferred = node.deferred
		};
		gn.reads.reserve(node.component_reads.size());
		for (const id r : node.component_reads) {
			gn.reads.push_back(resolve_name(r));
		}
		gn.writes.reserve(node.component_writes.size());
		for (const id w : node.component_writes) {
			gn.writes.push_back(resolve_name(w));
		}
		graph.nodes.push_back(std::move(gn));
	}

	std::unordered_map<id, std::vector<std::size_t>> writers;
	std::unordered_map<id, std::vector<std::size_t>> readers;
	{
		std::size_t idx = 0;
		for (const auto& node : m_nodes) {
			for (const id w : node.component_writes) {
				writers[w].push_back(idx);
			}
			for (const id r : node.component_reads) {
				readers[r].push_back(idx);
			}
			++idx;
		}
	}

	std::map<std::tuple<std::size_t, std::size_t, std::uint8_t>, std::vector<std::string>> edge_via;

	const auto add_component = [&](const std::size_t from_idx, const std::size_t to_idx, const introspection::edge_kind kind, const id component) {
		if (from_idx == to_idx) {
			return;
		}
		auto& via = edge_via[std::tuple{ from_idx, to_idx, static_cast<std::uint8_t>(kind) }];
		auto name = resolve_name(component);
		if (std::ranges::find(via, name) == via.end()) {
			via.push_back(std::move(name));
		}
	};

	{
		std::size_t bi = 0;
		for (const auto& node : m_nodes) {
			for (const id r : node.component_reads) {
				if (const auto it = writers.find(r); it != writers.end()) {
					for (const std::size_t ai : it->second) {
						if (ai < bi) {
							add_component(ai, bi, introspection::edge_kind::data_raw, r);
						}
					}
				}
			}
			for (const id w : node.component_writes) {
				if (const auto wit = writers.find(w); wit != writers.end()) {
					for (const std::size_t ai : wit->second) {
						if (ai < bi) {
							add_component(ai, bi, introspection::edge_kind::data_waw, w);
						}
					}
				}
				if (const auto rit = readers.find(w); rit != readers.end()) {
					for (const std::size_t ai : rit->second) {
						if (ai < bi) {
							add_component(ai, bi, introspection::edge_kind::data_war, w);
						}
					}
				}
			}
			++bi;
		}
	}

	for (auto& [key, via] : edge_via) {
		const auto [from_idx, to_idx, kind] = key;
		graph.edges.push_back(introspection::graph_edge{
			.from = m_nodes[from_idx].state_id.number(),
			.to = m_nodes[to_idx].state_id.number(),
			.kind = static_cast<introspection::edge_kind>(kind),
			.via = std::move(via)
		});
	}

	for (const auto& node : m_nodes) {
		const auto to_id = node.state_id.number();
		const auto add_lifecycle = [&](const std::vector<id>& deps) {
			for (const id dep : deps) {
				if (dep.number() == to_id) {
					continue;
				}
				graph.edges.push_back(introspection::graph_edge{
					.from = dep.number(),
					.to = to_id,
					.kind = introspection::edge_kind::lifecycle
				});
			}
		};
		add_lifecycle(node.init_state_deps);
		add_lifecycle(node.frame_state_deps);
	}

	for (const auto& node : m_nodes) {
		const auto viewer_id = node.state_id.number();
		for (const id target : node.shared_view_reads) {
			if (target.number() == viewer_id) {
				continue;
			}
			graph.edges.push_back(introspection::graph_edge{
				.from = target.number(),
				.to = viewer_id,
				.kind = introspection::edge_kind::shared_view
			});
		}
	}

	return graph;
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

	template <std::invocable OnComplete>
	auto wrap_run_task(async::task<> inner, OnComplete on_complete) -> async::task<> {
		co_await std::move(inner);
		on_complete();
	}
}

auto gse::scheduler::run_node_frame(context& ctx, system_node& node) -> async::task<> {
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
	std::unordered_map<id, std::vector<std::pair<std::size_t, id>>> readers;
	{
		std::size_t idx = 0;
		for (const auto& node : m_nodes) {
			for (const id w : node.component_writes) {
				writers[w].emplace_back(idx, node.state_id);
			}
			for (const id r : node.component_reads) {
				readers[r].emplace_back(idx, node.state_id);
			}
			++idx;
		}
	}

	std::size_t idx = 0;
	for (auto& node : m_nodes) {
		auto depend_on_earlier = [&](const auto& accessors, const id comp) {
			const auto it = accessors.find(comp);
			if (it == accessors.end()) {
				return;
			}
			for (const auto& [other_idx, other_state] : it->second) {
				if (other_idx >= idx || other_state == node.state_id) {
					continue;
				}
				if (std::ranges::find(node.run_state_deps, other_state) == node.run_state_deps.end()) {
					node.run_state_deps.push_back(other_state);
				}
			}
		};
		for (const id r : node.component_reads) {
			depend_on_earlier(writers, r);
		}
		for (const id w : node.component_writes) {
			depend_on_earlier(writers, w);
			depend_on_earlier(readers, w);
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

auto gse::scheduler::promote_optional_deps() -> void {
	std::unordered_set<id> registered;
	for (const auto& node : m_nodes) {
		registered.insert(node.state_id);
		if (node.state_type_id.exists()) {
			registered.insert(node.state_type_id);
		}
	}

	for (auto& node : m_nodes) {
		auto promote = [&](const std::vector<id>& optional_deps, std::vector<id>& deps) {
			for (const id& dep : optional_deps) {
				if (!registered.contains(dep)) {
					continue;
				}
				if (std::ranges::find(deps, dep) == deps.end()) {
					deps.push_back(dep);
				}
			}
		};
		promote(node.optional_run_state_deps, node.run_state_deps);
		promote(node.optional_init_state_deps, node.init_state_deps);
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
					const auto cycle = format_cycle(dep);
					log::println(log::level::error, log::category::runtime, "[war-probe] state_deps cycle detected: {}", cycle);
					log::flush();
					assert(false, "state_deps cycle detected: {}", cycle);
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

	promote_optional_deps();

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

	std::vector<std::unique_ptr<context>> contexts;
	std::vector<async::task<>> tasks;
	contexts.reserve(m_nodes.size());
	tasks.reserve(m_nodes.size());
	{
		trace::scope_guard sg_dispatch{ trace_id<"sched::run_dispatch">() };
		for (auto& node : m_nodes) {
			if (!node.invoke_run_fn || !is_dispatchable(node.state_id)) {
				continue;
			}
			auto& ctx = *contexts.emplace_back(std::make_unique<context>(
				*this,
				m_states,
				m_resources_store,
				m_channels_store,
				writer,
				m_update_graph,
				*m_registry,
				m_guard,
				dummy_resume,
				dummy_paused
			));
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
		promote_optional_deps();
		wire_component_deps();
		check_closed_dep_graph();
		check_state_dep_cycles();
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

auto gse::scheduler::add_system_node(system_node node) -> void {
	assert(m_registry != nullptr, "scheduler::set_registry must be called before add_system_node");
	assert(m_phase != scheduler_phase::shutdown, "scheduler::add_system_node called during shutdown");

	if (m_staging) {
		m_candidates.push_back(std::move(node));
		return;
	}

	if (m_phase == scheduler_phase::boot) {
		register_node(std::move(node));
		return;
	}

	std::lock_guard lock(m_hot_add_mutex);
	m_hot_add_queue.push_back(std::move(node));
}

auto gse::scheduler::begin_staging() -> void {
	m_staging = true;
}

auto gse::scheduler::resolve_activation(const std::unordered_set<id>& disabled_roots) -> void {
	const auto required = [](const system_node& n) {
		std::vector<id> deps = n.run_state_deps;
		deps.insert(deps.end(), n.init_state_deps.begin(), n.init_state_deps.end());
		deps.insert(deps.end(), n.frame_state_deps.begin(), n.frame_state_deps.end());
		return deps;
	};

	std::unordered_set<id> provided;
	std::unordered_map<id, std::string> name_of;
	for (const auto& n : m_candidates) {
		provided.insert(n.state_id);
		name_of[n.state_id] = n.system_name;
	}

	std::unordered_map<id, std::vector<id>> dependents;
	std::unordered_set<id> inactive;
	std::vector<id> work;

	for (const auto& n : m_candidates) {
		for (const id dep : required(n)) {
			dependents[dep].push_back(n.state_id);
			if (!provided.contains(dep)) {
				log::println(
					log::level::warning,
					log::category::runtime,
					"system '{}': required dependency not provided by any registered system — likely a missing registration",
					n.system_name
				);
				if (inactive.insert(n.state_id).second) {
					work.push_back(n.state_id);
				}
			}
		}
		if (disabled_roots.contains(n.state_id)) {
			if (inactive.insert(n.state_id).second) {
				work.push_back(n.state_id);
			}
		}
	}

	for (std::size_t i = 0; i < work.size(); ++i) {
		for (const id dep : dependents[work[i]]) {
			if (inactive.insert(dep).second) {
				work.push_back(dep);
			}
		}
	}

	for (auto& n : m_candidates) {
		if (!inactive.contains(n.state_id)) {
			if (n.deferred) {
				m_deferred_nodes.push_back(std::move(n));
			}
			else {
				register_node(std::move(n));
			}
			continue;
		}
		std::string reason = disabled_roots.contains(n.state_id) ? std::string("disabled in this mode") : std::string{};
		if (reason.empty()) {
			for (const id dep : required(n)) {
				if (inactive.contains(dep)) {
					const auto it = name_of.find(dep);
					reason = std::format("dependency '{}' is inactive", it != name_of.end() ? it->second : std::string("<unprovided>"));
					break;
				}
			}
		}
		log::println(log::category::runtime, "system '{}' not registered: {}", n.system_name, reason);
	}

	m_candidates.clear();
	m_staging = false;
}

auto gse::scheduler::register_deferred() -> void {
	for (auto& n : m_deferred_nodes) {
		add_system_node(std::move(n));
	}
	m_deferred_nodes.clear();
}

auto gse::scheduler::queue_system_node(system_node node) -> void {
	assert(m_registry != nullptr, "scheduler::set_registry must be called before queue_system_node");
	assert(m_phase != scheduler_phase::shutdown, "scheduler::queue_system_node called during shutdown");

	std::lock_guard lock(m_hot_add_mutex);
	m_hot_add_queue.push_back(std::move(node));
}

auto gse::context::add_system_node(system_node node) -> void {
	m_sched.queue_system_node(std::move(node));
}

auto gse::scheduler::advance_one_init_system(system_node& node) -> async::task<> {
	node.init_in_flight = true;
	auto in_flight_guard = make_scope_exit([&node] {
		node.init_in_flight = false;
	});

	if (!node.init_ctx) {
		node.init_writer = std::make_unique<channel_writer>(m_channels_store.make_writer());
		node.init_ctx = std::make_unique<context>(
			*this,
			m_states,
			m_resources_store,
			m_channels_store,
			*node.init_writer,
			m_update_graph,
			*m_registry,
			m_guard,
			*node.resume_event,
			*node.paused_event
		);
	}

	if (!node.init_launched) {
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

auto gse::scheduler::run_node_update(context& ctx, system_node& node) -> async::task<> {
	const auto eid = trace::begin_block(node.update_wall_id, 0);
	auto guard = make_scope_exit([wid = node.update_wall_id, eid] {
		trace::end_block(wid, eid, 0);
	});

	for (const id& dep : node.run_state_deps) {
		co_await m_update_graph.wait_state_ready(dep);
	}

	co_await node.invoke_run_fn(ctx, node.data.get());
	node.ran_once = true;

	m_update_graph.notify_state_ready(node.state_id);
	if (node.state_type_id.exists() && node.state_type_id != node.state_id) {
		m_update_graph.notify_state_ready(node.state_type_id);
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

	async::manual_event frame_resume;
	async::manual_event frame_paused;
	context f_ctx(*this, m_states, m_resources_store, m_channels_store, writer, m_frame_graph, *m_registry, m_guard, frame_resume, frame_paused, false);

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
	while (!m_nodes.empty()) {
		auto& node = m_nodes.back();
		node.invoke_shutdown_fn(node.data.get());
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

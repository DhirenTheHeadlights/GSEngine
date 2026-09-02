module gse.ecs:scheduler_impl;

import std;

import :scheduler;
import :registries;
import :context;
import :settings;
import :system_node;
import :system_dispatch;
import :registry;
import :task_graph;
import :access_token;

import gse.assert;
import gse.core;
import gse.meta;
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

auto gse::scheduler::set_actions_register_hook(std::function<void(std::vector<actions::registration>, std::vector<actions::axis_registration>)> fn) -> void {
	m_actions_register_hook = std::move(fn);
}

auto gse::scheduler::current_phase() const -> scheduler_phase {
	return m_phase;
}

auto gse::scheduler::snapshot_graph() const -> introspection::system_graph {
	introspection::system_graph graph;
	graph.nodes.reserve(m_nodes.size());

	const auto resolve_name = [](const id value) -> std::string {
		if (value.exists() && exists(value.number())) {
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
		gn.publishes.reserve(node.channel_publishes.size());
		for (const id p : node.channel_publishes) {
			gn.publishes.push_back(resolve_name(p));
		}
		gn.consumes.reserve(node.channel_consumes.size());
		for (const id c : node.channel_consumes) {
			gn.consumes.push_back(resolve_name(c));
		}
		graph.nodes.push_back(std::move(gn));
	}

	std::unordered_map<id, std::vector<std::size_t>> writers;
	std::unordered_map<id, std::vector<std::size_t>> structural_writers;
	std::vector<std::size_t> entity_structural_nodes;
	{
		std::size_t idx = 0;
		for (const auto& node : m_nodes) {
			for (const id w : node.component_writes) {
				writers[w].push_back(idx);
			}
			for (const id s : node.component_structural) {
				structural_writers[s].push_back(idx);
			}
			if (node.entity_structural) {
				entity_structural_nodes.push_back(idx);
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
			const auto add_structural = [&](const id comp) {
				const auto it = structural_writers.find(comp);
				if (it == structural_writers.end()) {
					return;
				}
				for (const std::size_t ai : it->second) {
					add_component(ai, bi, introspection::edge_kind::structural, comp);
				}
			};

			for (const id r : node.component_reads) {
				if (const auto it = writers.find(r); it != writers.end()) {
					for (const std::size_t ai : it->second) {
						add_component(ai, bi, introspection::edge_kind::data_raw, r);
					}
				}
				add_structural(r);
			}
			for (const id w : node.component_writes) {
				if (const auto wit = writers.find(w); wit != writers.end()) {
					for (const std::size_t ai : wit->second) {
						if (ai < bi) {
							add_component(ai, bi, introspection::edge_kind::data_waw, w);
						}
					}
				}
				add_structural(w);
			}
			for (const id s : node.component_structural) {
				if (const auto it = structural_writers.find(s); it != structural_writers.end()) {
					for (const std::size_t ai : it->second) {
						if (ai < bi) {
							add_component(ai, bi, introspection::edge_kind::data_waw, s);
						}
					}
				}
			}
			++bi;
		}
	}

	for (const std::size_t ai : entity_structural_nodes) {
		std::size_t bi = 0;
		for (const auto& node : m_nodes) {
			const bool touches_components =
				!node.component_reads.empty() || !node.component_writes.empty() || !node.component_structural.empty();
			if (ai != bi && touches_components) {
				graph.edges.push_back(introspection::graph_edge{
					.from = m_nodes[ai].state_id.number(),
					.to = node.state_id.number(),
					.kind = introspection::edge_kind::structural
				});
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
	enum class dep_kind : std::uint8_t {
		pinned,
		structural,
		reordered_read,
		output
	};

	enum class dispatch_state : std::uint8_t {
		unknown,
		checking,
		ready,
		blocked
	};

	struct component_dep {
		id state;
		id via;
		dep_kind kind;
	};

	auto find_dep_cycle(
		const std::vector<std::vector<component_dep>>& deps,
		const std::unordered_map<id, std::size_t>& state_to_index
	) -> std::vector<std::size_t>;

	auto dep_path_exists(
		const std::vector<std::vector<component_dep>>& deps,
		const std::unordered_map<id, std::size_t>& state_to_index,
		std::size_t from,
		std::size_t to
	) -> bool;

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
	trace::open_span span(node.frame_wall_id, 0);

	for (const id& dep : node.frame_state_deps) {
		co_await ctx.after_id(dep);
	}
	co_await node.invoke_frame_fn(ctx, node.data.get());
	ctx.notify_ready_by_id(node.state_id);
}

auto gse::find_dep_cycle(const std::vector<std::vector<component_dep>>& deps, const std::unordered_map<id, std::size_t>& state_to_index) -> std::vector<std::size_t> {
	enum class color : std::uint8_t {
		white,
		gray,
		black
	};

	std::vector<color> colors(deps.size(), color::white);
	std::vector<std::size_t> stack;
	std::vector<std::size_t> cycle;

	auto visit = [&](const std::size_t node, auto& self) -> bool {
		colors[node] = color::gray;
		stack.push_back(node);

		for (const auto& dep : deps[node]) {
			const auto it = state_to_index.find(dep.state);
			if (it == state_to_index.end()) {
				continue;
			}
			const auto next = it->second;
			if (colors[next] == color::gray) {
				const auto start = std::ranges::find(stack, next);
				cycle.assign(start, stack.end());
				return true;
			}
			if (colors[next] == color::white && self(next, self)) {
				return true;
			}
		}

		stack.pop_back();
		colors[node] = color::black;
		return false;
	};

	for (std::size_t i = 0; i < deps.size(); ++i) {
		if (colors[i] == color::white && visit(i, visit)) {
			break;
		}
	}

	return cycle;
}

auto gse::dep_path_exists(const std::vector<std::vector<component_dep>>& deps, const std::unordered_map<id, std::size_t>& state_to_index, const std::size_t from, const std::size_t to) -> bool {
	std::vector<bool> seen(deps.size(), false);
	std::vector<std::size_t> stack{ from };

	while (!stack.empty()) {
		const auto node = stack.back();
		stack.pop_back();
		if (node == to) {
			return true;
		}
		if (seen[node]) {
			continue;
		}
		seen[node] = true;

		for (const auto& dep : deps[node]) {
			if (const auto it = state_to_index.find(dep.state); it != state_to_index.end()) {
				stack.push_back(it->second);
			}
		}
	}

	return false;
}

auto gse::scheduler::wire_component_deps() -> void {
	std::unordered_map<id, std::vector<std::pair<std::size_t, id>>> writers;
	std::unordered_map<id, std::vector<std::pair<std::size_t, id>>> structural_writers;
	std::unordered_map<id, std::vector<std::pair<std::size_t, id>>> resource_writers;
	std::vector<std::pair<std::size_t, id>> entity_structural_nodes;
	std::unordered_map<id, std::size_t> state_to_index;
	{
		std::size_t idx = 0;
		for (const auto& node : m_nodes) {
			for (const id w : node.component_writes) {
				writers[w].emplace_back(idx, node.state_id);
			}
			for (const id s : node.component_structural) {
				structural_writers[s].emplace_back(idx, node.state_id);
			}
			for (const id w : node.resource_writes) {
				resource_writers[w].emplace_back(idx, node.state_id);
			}
			if (node.entity_structural) {
				entity_structural_nodes.emplace_back(idx, node.state_id);
			}
			state_to_index.emplace(node.state_id, idx);
			++idx;
		}
	}

	std::vector<std::vector<component_dep>> deps(m_nodes.size());
	{
		std::size_t idx = 0;
		for (const auto& node : m_nodes) {
			for (const id dep : node.declared_run_state_deps) {
				deps[idx].push_back({
					.state = dep,
					.via = {},
					.kind = dep_kind::pinned,
				});
			}
			++idx;
		}
	}

	std::size_t idx = 0;
	for (const auto& node : m_nodes) {
		auto add_dep = [&](const id other_state, const id via, const dep_kind kind) {
			if (other_state == node.state_id) {
				return;
			}
			auto& list = deps[idx];
			if (const auto existing = std::ranges::find(list, other_state, &component_dep::state); existing != list.end()) {
				existing->kind = std::min(existing->kind, kind);
				return;
			}
			list.push_back({
				.state = other_state,
				.via = via,
				.kind = kind,
			});
		};

		auto depend_on_structural = [&](const id comp) {
			const auto it = structural_writers.find(comp);
			if (it == structural_writers.end()) {
				return;
			}
			for (const auto& other_state : std::views::values(it->second)) {
				add_dep(other_state, comp, dep_kind::structural);
			}
		};

		for (const id r : node.component_reads) {
			if (const auto it = writers.find(r); it != writers.end()) {
				for (const auto& [other_idx, other_state] : it->second) {
					add_dep(other_state, r, other_idx > idx ? dep_kind::reordered_read : dep_kind::pinned);
				}
			}
			depend_on_structural(r);
		}
		for (const id w : node.component_writes) {
			if (const auto it = writers.find(w); it != writers.end()) {
				for (const auto& [other_idx, other_state] : it->second) {
					if (other_idx >= idx) {
						continue;
					}
					add_dep(other_state, w, dep_kind::output);
				}
			}
			depend_on_structural(w);
		}
		for (const id s : node.component_structural) {
			if (const auto it = structural_writers.find(s); it != structural_writers.end()) {
				for (const auto& [other_idx, other_state] : it->second) {
					if (other_idx >= idx) {
						continue;
					}
					add_dep(other_state, s, dep_kind::output);
				}
			}
		}

		for (const id r : node.resource_reads) {
			if (const auto it = resource_writers.find(r); it != resource_writers.end()) {
				for (const auto& [other_idx, other_state] : it->second) {
					add_dep(other_state, r, other_idx > idx ? dep_kind::reordered_read : dep_kind::pinned);
				}
			}
		}
		for (const id w : node.resource_writes) {
			if (const auto it = resource_writers.find(w); it != resource_writers.end()) {
				for (const auto& [other_idx, other_state] : it->second) {
					if (other_idx >= idx) {
						continue;
					}
					add_dep(other_state, w, dep_kind::output);
				}
			}
		}

		const bool touches_components =
			!node.component_reads.empty() || !node.component_writes.empty() || !node.component_structural.empty();

		if (touches_components || node.entity_structural) {
			for (const auto& [other_idx, other_state] : entity_structural_nodes) {
				if (node.entity_structural && other_idx >= idx) {
					continue;
				}
				add_dep(other_state, {}, node.entity_structural ? dep_kind::output : dep_kind::structural);
			}
		}

		++idx;
	}

	struct dropped_protection {
		std::size_t accessor = 0;
		std::size_t structural_writer = 0;
		id via;
	};

	std::vector<dropped_protection> dropped;

	while (true) {
		const auto cycle = find_dep_cycle(deps, state_to_index);
		if (cycle.empty()) {
			break;
		}

		bool demoted = false;
		for (const auto target : { dep_kind::output, dep_kind::reordered_read, dep_kind::structural }) {
			for (std::size_t i = 0; i < cycle.size(); ++i) {
				const auto from = cycle[i];
				const auto to = cycle[(i + 1) % cycle.size()];
				auto& list = deps[from];
				const auto edge = std::ranges::find(list, m_nodes[to].state_id, &component_dep::state);
				if (edge == list.end() || edge->kind != target) {
					continue;
				}
				if (target == dep_kind::output) {
					log::println(
						log::level::warning,
						log::category::runtime,
						"scheduler: {} and {} both write {} and ordering them closes a cycle. dropping the write ordering — if both actually mutate it, one needs an explicit ordering annotation",
						m_nodes[from].state_id,
						m_nodes[to].state_id,
						edge->via
					);
				}
				else if (target == dep_kind::structural) {
					dropped.push_back({
						.accessor = from,
						.structural_writer = to,
						.via = edge->via,
					});
				}
				else {
					log::println(
						log::level::warning,
						log::category::runtime,
						"scheduler: cyclic data dependency; {} reads {} written by {}, but that ordering closes a cycle. falling back to registration order — add an explicit ordering annotation to make this deterministic",
						m_nodes[from].state_id,
						edge->via,
						m_nodes[to].state_id
					);
				}
				list.erase(edge);
				demoted = true;
				break;
			}
			if (demoted) {
				break;
			}
		}

		if (!demoted) {
			break;
		}
	}

	for (const auto& [accessor, structural_writer, via] : dropped) {
		if (dep_path_exists(deps, state_to_index, accessor, structural_writer)
			|| dep_path_exists(deps, state_to_index, structural_writer, accessor)) {
			log::println(
				log::level::warning,
				log::category::runtime,
				"scheduler: {} accesses {} while {} adds or removes it, and ordering them that way closes a cycle. they stay serialised by the reverse path, so the access sees the state from before the structural change",
				m_nodes[accessor].state_id,
				via,
				m_nodes[structural_writer].state_id
			);
			continue;
		}

		assert(
			false,
			"scheduler: {} accesses {} while {} adds or removes it, and breaking the cycle left them unordered, so they can run concurrently and the storage can reallocate mid-access. annotate one of them with runs_after<> to force an order",
			m_nodes[accessor].state_id,
			via,
			m_nodes[structural_writer].state_id
		);
	}

	{
		std::size_t write_idx = 0;
		for (auto& node : m_nodes) {
			node.run_state_deps.clear();
			for (const auto& dep : deps[write_idx]) {
				node.run_state_deps.push_back(dep.state);
			}
			++write_idx;
		}
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
		promote(node.optional_run_state_deps, node.declared_run_state_deps);
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
	const auto it = m_node_index.find(dep);
	if (it == m_node_index.end()) {
		return true;
	}
	return m_nodes[it->second].init_done;
}

auto gse::scheduler::dispatchable_nodes() const -> std::vector<bool> {
	std::vector<dispatch_state> visited(m_nodes.size(), dispatch_state::unknown);

	auto resolve = [&](const std::size_t idx, auto& self) -> bool {
		if (visited[idx] == dispatch_state::checking) {
			return true;
		}
		if (visited[idx] != dispatch_state::unknown) {
			return visited[idx] == dispatch_state::ready;
		}
		visited[idx] = dispatch_state::checking;

		const auto& node = m_nodes[idx];
		bool ready = node.init_done;
		if (ready && node.invoke_run_fn) {
			for (const id& dep : node.run_state_deps) {
				const auto it = m_node_index.find(dep);
				if (it != m_node_index.end() && !self(it->second, self)) {
					ready = false;
					break;
				}
			}
		}

		visited[idx] = ready ? dispatch_state::ready : dispatch_state::blocked;
		return ready;
	};

	std::vector<bool> out(m_nodes.size(), false);
	for (std::size_t i = 0; i < m_nodes.size(); ++i) {
		out[i] = resolve(i, resolve);
	}
	return out;
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
		check_deps(node.declared_run_state_deps, node.state_id, "run()");
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

	const auto dispatchable = dispatchable_nodes();

	{
		std::size_t idx = 0;
		for (auto& node : m_nodes) {
			if (node.invoke_run_fn && dispatchable[idx]) {
				m_update_graph.reset_state(node.state_id);
				if (node.state_type_id.exists() && node.state_type_id != node.state_id) {
					m_update_graph.reset_state(node.state_type_id);
				}
			}
			++idx;
		}
	}

	if (!m_run_writer) {
		m_run_writer.emplace(m_channels_store.make_writer());
	}
	while (m_run_contexts.size() < m_nodes.size()) {
		m_run_contexts.push_back(std::make_unique<context>(
			*this,
			m_states,
			m_resources_store,
			m_channels_store,
			*m_run_writer,
			m_update_graph,
			*m_registry,
			m_guard
		));
	}

	std::vector<async::task<>> tasks;
	tasks.reserve(m_nodes.size());
	{
		trace::scope_guard sg_dispatch{ trace_id<"sched::run_dispatch">() };
		std::size_t idx = 0;
		for (auto& node : m_nodes) {
			if (!node.invoke_run_fn || !dispatchable[idx]) {
				++idx;
				continue;
			}
			auto& ctx = *m_run_contexts[idx];
			const int held = ctx.held_lock_count();
			assert(held == 0, "system {} still held {} component access handle(s) when its next run() was dispatched; scope read<>/write<>/structural<> to the function body", node.trace_id, held);
			tasks.push_back(run_node_update(ctx, node));
			++idx;
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
		for (auto& node : m_nodes) {
			if (node.invoke_apply_settings_fn) {
				node.invoke_apply_settings_fn(node.data.get(), m_channels_store);
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
	assert(m_registry != nullptr, "scheduler::set_registry must be called before a system node is registered");

	const auto canonical_idx = node.state_id;
	auto* state_ptr = node.state_ptr;
	m_states.register_state(canonical_idx, node.state_ptr, node.state_snapshot_ptr);

	if (node.state_type_id.exists() && node.state_type_id != canonical_idx) {
		m_states.register_state(node.state_type_id, node.state_ptr, node.state_snapshot_ptr);
	}

	if (node.invoke_ensure_storages_fn) {
		node.invoke_ensure_storages_fn(*m_registry);
	}

	auto combined_deps = node.declared_run_state_deps;
	combined_deps.insert(combined_deps.end(), node.init_state_deps.begin(), node.init_state_deps.end());
	combined_deps.insert(combined_deps.end(), node.frame_state_deps.begin(), node.frame_state_deps.end());
	m_state_deps.emplace(canonical_idx, std::move(combined_deps));

	const bool has_run = node.invoke_run_fn != nullptr;
	const auto state_id = node.state_id;
	const auto state_type_id = node.state_type_id;

	if (node.settings_record && m_settings_register_hook) {
		m_settings_register_hook(std::move(*node.settings_record));
	}

	if ((!node.action_records.empty() || !node.axis_records.empty()) && m_actions_register_hook) {
		m_actions_register_hook(std::move(node.action_records), std::move(node.axis_records));
	}

	const auto node_idx = m_nodes.size();
	m_node_index.emplace(state_id, node_idx);
	if (state_type_id.exists() && state_type_id != state_id) {
		m_node_index.emplace(state_type_id, node_idx);
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
		std::vector<id> deps = n.declared_run_state_deps;
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

	std::unordered_map<id, std::vector<id>> publishers;
	for (const auto& n : m_candidates) {
		for (const id ch : n.channel_publishes) {
			publishers[ch].push_back(n.state_id);
		}
	}

	std::unordered_map<id, std::vector<id>> dependents;
	std::unordered_set<id> inactive;
	std::vector<id> work;

	const auto has_active_producer = [&](const id ch) {
		const auto it = publishers.find(ch);
		if (it == publishers.end()) {
			return true;
		}
		return std::ranges::any_of(it->second, [&](const id p) {
			return !inactive.contains(p);
		});
	};

	std::unordered_set<id> unproduced_channels;

	for (const auto& n : m_candidates) {
		for (const id ch : n.channel_consumes) {
			if (!publishers.contains(ch)) {
				unproduced_channels.insert(ch);
			}
		}
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

	if (!unproduced_channels.empty()) {
		std::string tags;
		for (const id ch : unproduced_channels) {
			if (!tags.empty()) {
				tags += ", ";
			}
			tags += ch.tag();
		}
		log::println(
			log::level::warning,
			log::category::runtime,
			"{} consumed channels have no registered producer — no system declares them in a channel_write<...> parameter: {}",
			unproduced_channels.size(),
			tags
		);
	}

	std::size_t propagated = 0;
	const auto propagate = [&] {
		for (; propagated < work.size(); ++propagated) {
			for (const id dep : dependents[work[propagated]]) {
				if (inactive.insert(dep).second) {
					work.push_back(dep);
				}
			}
		}
	};

	propagate();
	for (bool changed = true; changed;) {
		changed = false;
		for (const auto& n : m_candidates) {
			if (inactive.contains(n.state_id)) {
				continue;
			}
			const bool starved = std::ranges::any_of(n.channel_consumes, [&](const id ch) {
				return !has_active_producer(ch);
			});
			if (!starved) {
				continue;
			}
			inactive.insert(n.state_id);
			work.push_back(n.state_id);
			changed = true;
		}
		propagate();
	}

	std::unordered_set<id> deferred;
	std::vector<id> deferred_work;
	for (const auto& n : m_candidates) {
		if (n.deferred && deferred.insert(n.state_id).second) {
			deferred_work.push_back(n.state_id);
		}
	}
	for (std::size_t i = 0; i < deferred_work.size(); ++i) {
		for (const id dep : dependents[deferred_work[i]]) {
			if (deferred.insert(dep).second) {
				deferred_work.push_back(dep);
			}
		}
	}

	for (auto& n : m_candidates) {
		if (!inactive.contains(n.state_id)) {
			if (deferred.contains(n.state_id)) {
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
		if (reason.empty()) {
			for (const id ch : n.channel_consumes) {
				if (!has_active_producer(ch)) {
					reason = std::format("channel {} has no active producer", ch);
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
			node.resume_event.get(),
			node.paused_event.get()
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
	trace::open_span span(node.update_wall_id, 0);

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

	while (!done_flag.load(std::memory_order_acquire) || !w.done()) {
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

	context f_ctx(*this, m_states, m_resources_store, m_channels_store, writer, m_frame_graph, *m_registry, m_guard, nullptr, nullptr, false);

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
	m_channels_store.clear();
	m_run_contexts.clear();
	m_run_writer.reset();
	m_node_index.clear();

	while (!m_nodes.empty()) {
		auto& node = m_nodes.back();
		node.invoke_shutdown_fn(node.data.get());
		m_nodes.pop_back();
	}
}

auto gse::scheduler::clear() -> void {
	m_nodes.clear();
	m_node_index.clear();
	m_run_contexts.clear();
	m_run_writer.reset();
	m_states.clear();
	m_resources_store.clear();
	m_channels_store.clear();
	m_state_deps.clear();
	m_external_resources.clear();
	m_initialized = false;
	m_dep_graph_checked = false;
	m_phase = scheduler_phase::boot;
}
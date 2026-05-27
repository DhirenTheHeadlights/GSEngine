module gse.diag;

import std;

import gse.core;
import gse.containers;
import gse.time;
import gse.math;
import gse.assert;

auto gse::trace::start(const config& cfg) -> void {
	global_config = cfg;

	ensure_tls_registered();
	make_tid();

	frames = frame_storage{};
	global_open_spans.clear();

	mark_hidden(trace_id<"task.start.reentrant">());
	mark_hidden(trace_id<"task.start.body">());

	register_virtual_thread(gpu_virtual_tid, "GPU");
	register_virtual_thread(gpu_stats_virtual_tid, "GPU Stats");
	register_virtual_thread(gpu_compute_virtual_tid, "GPU Compute");
}

auto gse::trace::begin_block(const id id, std::uint64_t parent) -> std::uint64_t {
	if (paused() || !id.exists()) {
		return 0;
	}

	ensure_tls_registered();

	if (parent != 0 && parent < 1024) {
		parent = 0;
	}

	const auto tid = make_tid();
	const auto eid = allocate_span_eid();

	emit({
		.type = event_type::begin,
		.id = id,
		.eid = eid,
		.parent_eid = parent,
		.tid = tid,
		.ts = system_clock::now<tick_step>()
	});

	return eid;
}

auto gse::trace::end_block(const id id, const std::uint64_t eid, const std::uint64_t parent) -> void {
	if (paused() || eid == 0 || !id.exists()) {
		return;
	}

	ensure_tls_registered();

	emit({
		.type = event_type::end,
		.id = id,
		.eid = eid,
		.parent_eid = parent,
		.tid = make_tid(),
		.ts = system_clock::now<tick_step>()
	});
}

auto gse::trace::begin_async(const id id, const std::uint64_t key) -> void {
	if (paused() || !id.exists()) {
		return;
	}

	ensure_tls_registered();

	emit({
		.type = event_type::async_begin,
		.id = id,
		.eid = 0,
		.parent_eid = current_parent_eid(),
		.tid = make_tid(),
		.ts = system_clock::now<tick_step>(),
		.value = 0.0,
		.key = key
	});
}

auto gse::trace::end_async(const id id, const std::uint64_t key) -> void {
	if (paused() || !id.exists()) {
		return;
	}

	ensure_tls_registered();

	emit({
		.type = event_type::async_end,
		.id = id,
		.eid = 0,
		.parent_eid = 0,
		.tid = make_tid(),
		.ts = system_clock::now<tick_step>(),
		.value = 0.0,
		.key = key
	});
}

auto gse::trace::mark(const id id) -> void {
	if (paused() || !id.exists()) {
		return;
	}

	ensure_tls_registered();

	emit({
		.type = event_type::instant,
		.id = id,
		.eid = 0,
		.parent_eid = current_parent_eid(),
		.tid = make_tid(),
		.ts = system_clock::now<tick_step>(),
		.value = 0.0,
		.key = 0
	});
}

auto gse::trace::counter(const id id, const double value) -> void {
	if (paused() || !id.exists()) {
		return;
	}

	ensure_tls_registered();

	emit({
		.type = event_type::counter,
		.id = id,
		.eid = 0,
		.parent_eid = 0,
		.tid = make_tid(),
		.ts = system_clock::now<tick_step>(),
		.value = value,
		.key = 0,
	});
}

auto gse::trace::begin_async_at(const id id, const std::uint64_t key, const std::uint32_t tid, const time_t<std::uint64_t> ts) -> void {
	if (paused() || !id.exists()) {
		return;
	}

	ensure_tls_registered();

	emit({
		.type = event_type::async_begin,
		.id = id,
		.eid = 0,
		.parent_eid = 0,
		.tid = tid,
		.ts = ts,
		.value = 0.0,
		.key = key
	});
}

auto gse::trace::end_async_at(const id id, const std::uint64_t key, const std::uint32_t tid, const time_t<std::uint64_t> ts) -> void {
	if (paused() || !id.exists()) {
		return;
	}

	ensure_tls_registered();

	emit({
		.type = event_type::async_end,
		.id = id,
		.eid = 0,
		.parent_eid = 0,
		.tid = tid,
		.ts = ts,
		.value = 0.0,
		.key = key
	});
}

auto gse::trace::counter_at(const id id, const double value, const std::uint32_t tid, const time_t<std::uint64_t> ts) -> void {
	if (paused() || !id.exists()) {
		return;
	}

	ensure_tls_registered();

	emit({
		.type = event_type::counter,
		.id = id,
		.eid = 0,
		.parent_eid = 0,
		.tid = tid,
		.ts = ts,
		.value = value,
		.key = 0,
	});
}

auto gse::trace::register_virtual_thread(const std::uint32_t tid, const std::string_view name) -> void {
	std::unique_lock lk(virtual_thread_mutex);
	virtual_thread_names[tid] = std::string(name);
}

auto gse::trace::virtual_thread_name(const std::uint32_t tid) -> std::optional<std::string> {
	std::shared_lock lk(virtual_thread_mutex);
	if (const auto it = virtual_thread_names.find(tid); it != virtual_thread_names.end()) {
		return it->second;
	}
	return std::nullopt;
}

auto gse::trace::register_main_thread() -> void {
	ensure_tls_registered();
	main_tid_value.store(make_tid(), std::memory_order_relaxed);
}

auto gse::trace::main_tid() -> std::uint32_t {
	return main_tid_value.load(std::memory_order_relaxed);
}

auto gse::trace::mark_hidden(const id id) -> void {
	std::unique_lock lk(hidden_ids_mutex);
	hidden_ids.insert(id);
}

auto gse::trace::is_hidden(const id id) -> bool {
	std::shared_lock lk(hidden_ids_mutex);
	return hidden_ids.contains(id);
}

auto gse::trace::hidden_ids_snapshot() -> std::unordered_set<id> {
	std::shared_lock lk(hidden_ids_mutex);
	return hidden_ids;
}

auto gse::trace::current_eid() -> std::uint64_t {
	return current_parent_eid();
}

auto gse::trace::finalize_frame() -> void {
	static interval_timer timer(milliseconds(100.f));

	build_tree(frames.write());

	if (timer.tick() && !finalize_paused()) {
		frames.flip();
	}
}

auto gse::trace::view() -> frame_view {
	const auto& fs = frames.read();
	return {
		.roots = std::span(fs.roots),
		.storage = reinterpret_cast<const std::byte*>(fs.flat.data()),
	};
}

gse::trace::thread_pause::thread_pause() {
	++tls_pause_depth;
}

gse::trace::thread_pause::~thread_pause() {
	--tls_pause_depth;
}

auto gse::trace::paused() -> bool {
	return !trace_enabled.load(std::memory_order_relaxed) || tls_pause_depth > 0;
}

auto gse::trace::set_enabled(const bool enable) -> void {
	trace_enabled.store(enable, std::memory_order_relaxed);
}

auto gse::trace::enabled() -> bool {
	return trace_enabled.load(std::memory_order_relaxed);
}

auto gse::trace::set_finalize_paused(const bool pause) -> void {
	finalize_paused_flag.store(pause, std::memory_order_relaxed);
}

auto gse::trace::finalize_paused() -> bool {
	return finalize_paused_flag.load(std::memory_order_relaxed);
}

auto gse::trace::scsp_events::push(const event& e) noexcept -> void {
	gse::assert(
		m_events != nullptr,
		"trace push: m_events is null, tid_hash={}",
		std::hash<std::thread::id>{}(std::this_thread::get_id())
	);

	const std::uint32_t w = m_w.load(std::memory_order_acquire);
	const std::uint32_t next = (w + 1) & capacity_mask;

	if (next == m_r) {
		return;
	}

	m_events[w] = e;
	m_w.store(next, std::memory_order_release);
}

template <typename Out>
auto gse::trace::scsp_events::drain_to(Out& out) noexcept -> void {
	const std::uint32_t w_snapshot = m_w.load(std::memory_order_acquire);
	while (m_r != w_snapshot) {
		out.push_back(std::move(m_events[m_r]));
		m_r = (m_r + 1) & capacity_mask;
	}
}

auto gse::trace::scsp_events::clear() noexcept -> void {
	m_r = m_w.load(std::memory_order_acquire);
}

auto gse::trace::scsp_events::size() const noexcept -> std::size_t {
	return (m_w.load(std::memory_order_acquire) - m_r) & capacity_mask;
}

auto gse::trace::scsp_events::ensure_storage() -> void {
	if (!m_events) {
		m_events = std::make_unique<event[]>(capacity);
	}
}

auto gse::trace::ensure_tls_registered() -> void {
	if (tls.registered) {
		return;
	}

	tls.events.ensure_storage();

	std::lock_guard lock(tls_registry_mutex);
	tls_registry.push_back(&tls);
	tls.registered = true;
}

auto gse::trace::make_tid() -> std::uint32_t {
	if (tls.tid != 0) {
		return tls.tid;
	}
	tls.tid = next_tid.fetch_add(1, std::memory_order::relaxed) + 1;
	return tls.tid;
}

auto gse::trace::emit(const event& e) -> void {
	if (tls.events.size() >= global_config.per_thread_event_cap) {
		return;
	}

	tls.events.push(e);
}

auto gse::trace::current_parent_eid() -> std::uint64_t {
	return tls.stack.empty() ? 0 : tls.stack.back();
}

auto gse::trace::compute_self_time(frame_storage& fs, std::size_t i) -> void {
	auto& n = fs.flat[i];

	const std::uint32_t child_first = n.children_first;
	const std::uint32_t child_count = n.children_count;

	for (std::uint32_t k = 0; k < child_count; ++k) {
		compute_self_time(fs, fs.children_arena[child_first + k]);
	}

	const auto parent_begin = n.start;
	const auto parent_end = n.end;
	const auto parent_tot = parent_end - parent_begin;

	if (child_count == 0 || parent_tot <= decltype(parent_tot){}) {
		n.self = parent_tot;
		return;
	}

	const std::size_t segs_base = fs.segs_scratch.size();

	for (std::uint32_t k = 0; k < child_count; ++k) {
		const auto& ch = fs.flat[fs.children_arena[child_first + k]];

		auto a = std::max(ch.start, parent_begin);
		auto b = std::min(ch.end, parent_end);

		if (b > a) {
			fs.segs_scratch.push_back({ a, b });
		}
	}

	const std::size_t segs_count = fs.segs_scratch.size() - segs_base;

	if (segs_count == 0) {
		n.self = parent_tot;
		return;
	}

	const auto segs_first = fs.segs_scratch.begin() + segs_base;
	const auto segs_last = fs.segs_scratch.end();

	std::ranges::sort(
		segs_first,
		segs_last,
		[](
		const frame_storage::seg& x,
		const frame_storage::seg& y
	) {
			return x.a < y.a;
		}
	);

	time_t<std::uint64_t> covered{};
	frame_storage::seg cur = *segs_first;

	for (auto it = segs_first + 1; it != segs_last; ++it) {
		if (it->a <= cur.b) {
			if (it->b > cur.b) {
				cur.b = it->b;
			}
		}
		else {
			covered += (cur.b - cur.a);
			cur = *it;
		}
	}
	covered += (cur.b - cur.a);

	fs.segs_scratch.resize(segs_base);

	n.self = (covered < parent_tot) ? (parent_tot - covered) : decltype(parent_tot){};
}

auto gse::trace::emplace_shallow_node(frame_storage& fs, std::size_t flat_i) -> std::size_t {
	const auto& fn = fs.flat[flat_i];
	fs.node_pool.push_back(
		node{
			.id = fn.id,
			.trace_id = fn.tid,
			.start = fn.start,
			.stop = fn.end,
			.self = fn.self,
			.children_first = nullptr,
			.children_count = 0
		}
	);
	return fs.node_pool.size() - 1;
}

auto gse::trace::build_subtree(frame_storage& fs, std::size_t node_idx, std::size_t flat_i) -> void {
	const auto& fn = fs.flat[flat_i];

	if (fn.children_count == 0) {
		fs.node_pool[node_idx].children_first = nullptr;
		fs.node_pool[node_idx].children_count = 0;
		return;
	}

	const std::uint32_t arena_first = fn.children_first;
	const std::uint32_t arena_count = fn.children_count;
	const std::size_t pool_start = fs.node_pool.size();

	for (std::uint32_t k = 0; k < arena_count; ++k) {
		emplace_shallow_node(fs, fs.children_arena[arena_first + k]);
	}

	fs.node_pool[node_idx].children_first = fs.node_pool.data() + pool_start;
	fs.node_pool[node_idx].children_count = arena_count;

	for (std::uint32_t k = 0; k < arena_count; ++k) {
		build_subtree(fs, pool_start + k, fs.children_arena[arena_first + k]);
	}
}

auto gse::trace::build_tree(frame_storage& fs) -> void {
	fs.merged.clear();

	{
		std::lock_guard lk(tls_registry_mutex);
		for (auto* tb : tls_registry) {
			if (!tb) {
				continue;
			}
			tb->events.drain_to(fs.merged);
		}
	}

	std::ranges::sort(
		fs.merged,
		[](const event& a, const event& b) {
			if (a.ts != b.ts) {
				return a.ts < b.ts;
			}
			return static_cast<int>(a.type) < static_cast<int>(b.type);
		}
	);

	auto& spans = fs.spans_scratch;
	spans.clear();
	spans.reserve(global_open_spans.size() + fs.merged.size() / 2);

	for (const auto& [eid, sp] : global_open_spans) {
		spans.emplace_back(eid, sp);
	}

	for (const auto& e : fs.merged) {
		if (e.type == event_type::begin) {
			spans.emplace_back(
				e.eid,
				span_info{
					.id = e.id,
					.tid = static_cast<std::uint32_t>(e.tid),
					.t0 = e.ts,
					.t1 = {},
					.parent = e.parent_eid
				}
			);
		}
	}

	std::ranges::sort(
		spans,
		{},
		&std::pair<std::uint64_t, span_info>::first
	);

	for (const auto& e : fs.merged) {
		if (e.type != event_type::end) {
			continue;
		}
		const auto it = std::ranges::lower_bound(
			spans,
			e.eid,
			{},
			&std::pair<std::uint64_t, span_info>::first
		);
		if (it != spans.end() && it->first == e.eid) {
			it->second.t1 = e.ts;
		}
	}

	global_open_spans.clear();
	for (const auto& [eid, sp] : spans) {
		if (sp.t1 == decltype(sp.t1){}) {
			global_open_spans.emplace_back(eid, sp);
		}
	}

	fs.flat.clear();
	fs.flat.reserve(spans.size());

	for (auto& [eid, sp] : spans) {
		if (sp.t1 < sp.t0) {
			sp.t1 = sp.t0;
		}

		fs.flat.push_back(
			frame_storage::flat_node{
				.id = sp.id,
				.tid = sp.tid,
				.start = sp.t0,
				.end = sp.t1,
				.self = {},
				.children_first = 0,
				.children_count = 0
			}
		);
	}

	constexpr auto no_parent = std::numeric_limits<std::uint32_t>::max();

	auto& parent_idx = fs.parent_idx_scratch;
	parent_idx.assign(spans.size(), no_parent);

	for (std::size_t i = 0; i < spans.size(); ++i) {
		const auto& sp = spans[i].second;
		if (sp.parent == 0) {
			continue;
		}
		const auto it = std::ranges::lower_bound(
			spans,
			sp.parent,
			{},
			&std::pair<std::uint64_t, span_info>::first
		);
		if (it != spans.end() && it->first == sp.parent) {
			parent_idx[i] = static_cast<std::uint32_t>(it - spans.begin());
		}
	}

	auto& child_counts = fs.child_counts_scratch;
	child_counts.assign(fs.flat.size(), 0);

	for (std::size_t i = 0; i < parent_idx.size(); ++i) {
		if (parent_idx[i] != no_parent) {
			++child_counts[parent_idx[i]];
		}
	}

	std::uint32_t arena_offset = 0;
	for (std::size_t i = 0; i < fs.flat.size(); ++i) {
		fs.flat[i].children_first = arena_offset;
		fs.flat[i].children_count = 0;
		arena_offset += child_counts[i];
	}

	fs.children_arena.assign(arena_offset, 0);

	for (std::size_t i = 0; i < parent_idx.size(); ++i) {
		const std::uint32_t p = parent_idx[i];
		if (p == no_parent) {
			continue;
		}
		auto& parent_node = fs.flat[p];
		fs.children_arena[parent_node.children_first + parent_node.children_count] = static_cast<std::uint32_t>(i);
		++parent_node.children_count;
	}

	auto& roots_idx = fs.roots_idx_scratch;
	roots_idx.clear();
	roots_idx.reserve(fs.flat.size());

	for (std::size_t i = 0; i < parent_idx.size(); ++i) {
		if (parent_idx[i] == no_parent) {
			roots_idx.push_back(i);
		}
	}

	fs.segs_scratch.clear();
	for (const auto r : roots_idx) {
		compute_self_time(fs, r);
	}

	fs.node_pool.clear();
	fs.roots.clear();
	fs.node_pool.reserve(fs.flat.size());
	fs.roots.reserve(roots_idx.size());

	for (const auto r : roots_idx) {
		const std::size_t root_idx = emplace_shallow_node(fs, r);
		build_subtree(fs, root_idx, r);
		fs.roots.push_back(fs.node_pool[root_idx]);
	}
}

auto gse::trace::allocate_span_eid() -> std::uint64_t {
	return next_eid.fetch_add(1, std::memory_order_relaxed);
}

auto gse::trace::allocate_async_key() -> std::uint64_t {
	return next_async_key.fetch_add(1, std::memory_order_relaxed);
}

gse::trace::scope_guard::scope_guard(const id id) : m_id(id) {
	enter(current_parent_eid());
}

gse::trace::scope_guard::scope_guard(const id id, const std::uint64_t parent) : m_id(id) {
	enter(parent);
}

auto gse::trace::scope_guard::enter(std::uint64_t parent) -> void {
	if (paused() || !m_id.exists()) {
		return;
	}

	ensure_tls_registered();

	if (parent != 0 && parent < 1024) {
		parent = 0;
	}

	m_tid = make_tid();
	m_pushed_parent = parent != 0 && (tls.stack.empty() || tls.stack.back() != parent);
	if (m_pushed_parent) {
		tls.stack.push_back(parent);
	}

	m_parent = parent;
	m_eid = allocate_span_eid();

	emit({
		.type = event_type::begin,
		.id = m_id,
		.eid = m_eid,
		.parent_eid = m_parent,
		.tid = m_tid,
		.ts = system_clock::now<tick_step>()
	});

	tls.stack.push_back(m_eid);
}

gse::trace::scope_guard::~scope_guard() {
	if (m_eid == 0) {
		return;
	}

	if (tls.stack.empty() || tls.stack.back() != m_eid) {
		return;
	}

	emit({
		.type = event_type::end,
		.id = m_id,
		.eid = m_eid,
		.parent_eid = m_parent,
		.tid = m_tid,
		.ts = system_clock::now<tick_step>()
	});

	tls.stack.pop_back();
	if (m_pushed_parent && !tls.stack.empty() && tls.stack.back() == m_parent) {
		tls.stack.pop_back();
	}
}

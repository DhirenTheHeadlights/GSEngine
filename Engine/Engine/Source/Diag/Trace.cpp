module gse.diag:trace_impl;

import std;

import :trace;

import gse.core;
import gse.containers;
import gse.time;
import gse.math;

auto gse::trace::start(const config& cfg) -> void {
	global_config = cfg;

	ensure_tls_registered();
	make_tid();

	frames = frame_storage{};
	open_spans.clear();
	closed_spans.clear();
	build_frame_index = 0;
	published_generation = 0;

	register_virtual_thread(gpu_virtual_tid, "GPU");
	register_virtual_thread(gpu_stats_virtual_tid, "GPU Stats");
	register_virtual_thread(gpu_compute_virtual_tid, "GPU Compute");
}

auto gse::trace::current_eid() -> std::uint64_t {
	return tls.stack.empty() ? 0 : tls.stack.back();
}

auto gse::trace::begin_async(const id id, const std::uint64_t key) -> void {
	if (paused() || !id.exists()) {
		return;
	}

	emit({
		.type = event_type::async_begin,
		.id = id,
		.parent_eid = current_eid(),
		.tid = make_tid(),
		.ts = system_clock::now<tick_step>(),
		.key = key
	});
}

auto gse::trace::end_async(const id id, const std::uint64_t key) -> void {
	if (paused() || !id.exists()) {
		return;
	}

	emit({
		.type = event_type::async_end,
		.id = id,
		.tid = make_tid(),
		.ts = system_clock::now<tick_step>(),
		.key = key
	});
}

auto gse::trace::allocate_async_key() -> std::uint64_t {
	return next_async_key.fetch_add(1, std::memory_order_relaxed);
}

auto gse::trace::mark(const id id) -> void {
	if (paused() || !id.exists()) {
		return;
	}

	emit({
		.type = event_type::instant,
		.id = id,
		.parent_eid = current_eid(),
		.tid = make_tid(),
		.ts = system_clock::now<tick_step>()
	});
}

auto gse::trace::counter(const id id, const double value) -> void {
	if (paused() || !id.exists()) {
		return;
	}

	emit({
		.type = event_type::counter,
		.id = id,
		.tid = make_tid(),
		.ts = system_clock::now<tick_step>(),
		.value = value
	});
}

auto gse::trace::begin_async_at(const id id, const std::uint64_t key, const std::uint32_t tid, const time_t<std::uint64_t> ts) -> void {
	if (paused() || !id.exists()) {
		return;
	}

	emit({
		.type = event_type::async_begin,
		.id = id,
		.tid = tid,
		.ts = ts,
		.key = key
	});
}

auto gse::trace::end_async_at(const id id, const std::uint64_t key, const std::uint32_t tid, const time_t<std::uint64_t> ts) -> void {
	if (paused() || !id.exists()) {
		return;
	}

	emit({
		.type = event_type::async_end,
		.id = id,
		.tid = tid,
		.ts = ts,
		.key = key
	});
}

auto gse::trace::counter_at(const id id, const double value, const std::uint32_t tid, const time_t<std::uint64_t> ts) -> void {
	if (paused() || !id.exists()) {
		return;
	}

	emit({
		.type = event_type::counter,
		.id = id,
		.tid = tid,
		.ts = ts,
		.value = value
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

auto gse::trace::finalize_frame() -> void {
	++build_frame_index;

	drain_events(scratch.merged);
	absorb_events(scratch.merged);
	evict_stale_open_spans();

	build_frame(frames.write());
	frames.flip();
}

auto gse::trace::view() -> frame_view {
	const auto& fs = frames.read();
	return {
		.nodes = std::span(fs.nodes),
		.children = std::span(fs.children),
		.roots = std::span(fs.roots),
		.generation = fs.generation
	};
}

auto gse::trace::dropped_events() -> std::uint64_t {
	auto& reg = registry();
	std::lock_guard lock(reg.mutex);

	std::uint64_t total = 0;
	for (const auto* tb : reg.buffers) {
		total += tb->events.dropped();
	}
	return total;
}

auto gse::trace::abandoned_spans() -> std::uint64_t {
	return abandoned_span_count.load(std::memory_order_relaxed);
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

auto gse::trace::scsp_events::push(const event& e) noexcept -> void {
	const std::uint32_t w = m_w.load(std::memory_order_relaxed);
	const std::uint32_t next = (w + 1) & capacity_mask;

	if (next == m_r.load(std::memory_order_acquire)) {
		m_dropped.fetch_add(1, std::memory_order_relaxed);
		return;
	}

	m_events[w] = e;
	m_w.store(next, std::memory_order_release);
}

template <typename Out>
auto gse::trace::scsp_events::drain_to(Out& out) noexcept -> void {
	const std::uint32_t w = m_w.load(std::memory_order_acquire);
	std::uint32_t r = m_r.load(std::memory_order_relaxed);

	while (r != w) {
		out.push_back(m_events[r]);
		r = (r + 1) & capacity_mask;
	}

	m_r.store(r, std::memory_order_release);
}

auto gse::trace::scsp_events::ensure_storage() -> void {
	if (!m_events) {
		m_events = std::make_unique<event[]>(capacity);
	}
}

auto gse::trace::scsp_events::dropped() const noexcept -> std::uint64_t {
	return m_dropped.load(std::memory_order_relaxed);
}

auto gse::trace::registry() -> thread_registry& {
	static auto* instance = new thread_registry();
	return *instance;
}

gse::trace::thread_buffer::~thread_buffer() {
	if (!registered) {
		return;
	}

	auto& reg = registry();
	std::lock_guard lock(reg.mutex);
	std::erase(reg.buffers, this);
}

auto gse::trace::ensure_tls_registered() -> void {
	if (tls.registered) {
		return;
	}

	tls.events.ensure_storage();

	auto& reg = registry();
	std::lock_guard lock(reg.mutex);
	reg.buffers.push_back(&tls);
	tls.registered = true;
}

auto gse::trace::make_tid() -> std::uint32_t {
	if (tls.tid != 0) {
		return tls.tid;
	}
	tls.tid = next_tid.fetch_add(1, std::memory_order_relaxed) + 1;
	return tls.tid;
}

auto gse::trace::emit(const event& e) -> void {
	ensure_tls_registered();
	tls.events.push(e);
}

auto gse::trace::allocate_span_eid() -> std::uint64_t {
	return next_eid.fetch_add(1, std::memory_order_relaxed);
}

auto gse::trace::close_span(const id id, const std::uint64_t eid, const std::uint64_t parent, const std::uint32_t tid) -> void {
	emit({
		.type = event_type::end,
		.id = id,
		.eid = eid,
		.parent_eid = parent,
		.tid = tid,
		.ts = system_clock::now<tick_step>()
	});
}

auto gse::trace::drain_events(std::vector<event>& out) -> void {
	out.clear();

	{
		auto& reg = registry();
		std::lock_guard lock(reg.mutex);
		for (auto* tb : reg.buffers) {
			tb->events.drain_to(out);
		}
	}

	std::ranges::sort(
		out,
		[](const event& a, const event& b) {
			if (a.ts != b.ts) {
				return a.ts < b.ts;
			}
			return static_cast<int>(a.type) < static_cast<int>(b.type);
		}
	);
}

auto gse::trace::absorb_events(const std::span<const event> events) -> void {
	closed_spans.clear();

	for (const auto& e : events) {
		if (e.type == event_type::begin) {
			open_spans.insert_or_assign(
				e.eid,
				span_info{
					.id = e.id,
					.tid = static_cast<std::uint32_t>(e.tid),
					.t0 = e.ts,
					.t1 = {},
					.parent = e.parent_eid,
					.opened_frame = build_frame_index
				}
			);
			continue;
		}

		if (e.type != event_type::end) {
			continue;
		}

		const auto it = open_spans.find(e.eid);
		if (it == open_spans.end()) {
			continue;
		}

		auto info = it->second;
		info.t1 = std::max(e.ts, info.t0);

		closed_spans.push_back({
			.eid = e.eid,
			.info = info,
			.open = false
		});

		open_spans.erase(it);
	}
}

auto gse::trace::evict_stale_open_spans() -> void {
	const auto erased = std::erase_if(
		open_spans,
		[](const auto& entry) {
			return build_frame_index - entry.second.opened_frame > max_open_span_frames;
		}
	);

	abandoned_span_count.fetch_add(erased, std::memory_order_relaxed);
}

auto gse::trace::collect_frame_spans(std::vector<frame_span>& out) -> void {
	out.clear();
	out.reserve(closed_spans.size() + open_spans.size());
	out.insert(out.end(), closed_spans.begin(), closed_spans.end());

	for (const auto& [eid, info] : open_spans) {
		out.push_back({
			.eid = eid,
			.info = info,
			.open = true
		});
	}

	std::ranges::sort(out, {}, &frame_span::eid);
}

auto gse::trace::link_parents(const std::span<const frame_span> spans, std::vector<std::uint32_t>& parent_idx) -> void {
	parent_idx.assign(spans.size(), no_parent);

	for (std::size_t i = 0; i < spans.size(); ++i) {
		const auto parent = spans[i].info.parent;
		if (parent == 0 || parent >= spans[i].eid) {
			continue;
		}

		const auto it = std::ranges::lower_bound(spans, parent, {}, &frame_span::eid);
		if (it != spans.end() && it->eid == parent) {
			parent_idx[i] = static_cast<std::uint32_t>(it - spans.begin());
		}
	}
}

auto gse::trace::build_frame(frame_storage& fs) -> void {
	auto& spans = scratch.spans;
	collect_frame_spans(spans);

	fs.nodes.clear();
	fs.nodes.reserve(spans.size());

	for (const auto& sp : spans) {
		fs.nodes.push_back({
			.id = sp.info.id,
			.trace_id = sp.info.tid,
			.start = sp.info.t0,
			.stop = sp.open ? sp.info.t0 : sp.info.t1,
			.self = {},
			.children_first = 0,
			.children_count = 0,
			.open = sp.open
		});
	}

	auto& parent_idx = scratch.parent_idx;
	link_parents(spans, parent_idx);

	auto& child_counts = scratch.child_counts;
	child_counts.assign(fs.nodes.size(), 0);

	for (const auto p : parent_idx) {
		if (p != no_parent) {
			++child_counts[p];
		}
	}

	std::uint32_t offset = 0;
	for (std::size_t i = 0; i < fs.nodes.size(); ++i) {
		fs.nodes[i].children_first = offset;
		offset += child_counts[i];
	}

	fs.children.assign(offset, 0);
	fs.roots.clear();

	for (std::size_t i = 0; i < parent_idx.size(); ++i) {
		if (parent_idx[i] == no_parent) {
			fs.roots.push_back(static_cast<std::uint32_t>(i));
			continue;
		}

		auto& parent = fs.nodes[parent_idx[i]];
		fs.children[parent.children_first + parent.children_count] = static_cast<std::uint32_t>(i);
		++parent.children_count;
	}

	compute_self_times(fs, scratch.segs);

	fs.generation = ++published_generation;
}

auto gse::trace::compute_self_times(frame_storage& fs, std::vector<seg>& segs) -> void {
	for (auto& n : fs.nodes) {
		const auto total = n.stop - n.start;

		if (n.children_count == 0 || total <= tick_step{}) {
			n.self = total;
			continue;
		}

		segs.clear();
		for (const auto ci : std::span(fs.children).subspan(n.children_first, n.children_count)) {
			const auto& child = fs.nodes[ci];
			const auto a = std::max(child.start, n.start);
			const auto b = std::min(child.stop, n.stop);
			if (b > a) {
				segs.push_back({
					.a = a,
					.b = b
				});
			}
		}

		if (segs.empty()) {
			n.self = total;
			continue;
		}

		std::ranges::sort(segs, {}, &seg::a);

		tick_step covered{};
		seg cur = segs.front();

		for (const auto& s : std::span(segs).subspan(1)) {
			if (s.a <= cur.b) {
				if (s.b > cur.b) {
					cur.b = s.b;
				}
			}
			else {
				covered += cur.b - cur.a;
				cur = s;
			}
		}
		covered += cur.b - cur.a;

		n.self = covered < total ? total - covered : tick_step{};
	}
}

gse::trace::open_span::open_span(const id id, const std::uint64_t parent) : m_id(id), m_parent(parent < first_span_eid ? 0 : parent) {
	if (paused() || !m_id.exists()) {
		return;
	}

	m_tid = make_tid();
	m_eid = allocate_span_eid();

	emit({
		.type = event_type::begin,
		.id = m_id,
		.eid = m_eid,
		.parent_eid = m_parent,
		.tid = m_tid,
		.ts = system_clock::now<tick_step>()
	});
}

gse::trace::open_span::~open_span() {
	close();
}

gse::trace::open_span::open_span(open_span&& other) noexcept : m_id(other.m_id), m_eid(other.m_eid), m_parent(other.m_parent), m_tid(other.m_tid) {
	other.m_eid = 0;
}

auto gse::trace::open_span::operator=(open_span&& other) noexcept -> open_span& {
	if (this == &other) {
		return *this;
	}

	close();

	m_id = other.m_id;
	m_eid = other.m_eid;
	m_parent = other.m_parent;
	m_tid = other.m_tid;
	other.m_eid = 0;

	return *this;
}

auto gse::trace::open_span::close() -> void {
	if (m_eid == 0) {
		return;
	}

	close_span(m_id, m_eid, m_parent, m_tid);
	m_eid = 0;
}

auto gse::trace::open_span::eid() const -> std::uint64_t {
	return m_eid;
}

gse::trace::scope_guard::scope_guard(const id id) : m_id(id) {
	enter(current_eid());
}

gse::trace::scope_guard::scope_guard(const id id, const std::uint64_t parent) : m_id(id) {
	enter(parent);
}

auto gse::trace::scope_guard::enter(std::uint64_t parent) -> void {
	if (paused() || !m_id.exists()) {
		return;
	}

	if (parent < first_span_eid) {
		parent = 0;
	}

	m_tid = make_tid();
	m_depth = tls.stack.size();
	m_parent = parent;
	m_eid = allocate_span_eid();

	if (parent != 0 && (tls.stack.empty() || tls.stack.back() != parent)) {
		tls.stack.push_back(parent);
	}

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

	close_span(m_id, m_eid, m_parent, m_tid);

	if (tls.tid == m_tid && tls.stack.size() > m_depth) {
		tls.stack.resize(m_depth);
	}
}

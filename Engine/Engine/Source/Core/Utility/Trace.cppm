export module gse.utility:trace;

import std;

import :id;
import :double_buffer;
import :scope_exit;
import :system_clock;
import :interval_timer;

export namespace gse::trace {
	struct config {
		std::size_t per_thread_event_cap = 262144;
		bool enable_browser_dump = false;
	};

	auto start(
		const config& cfg = {}
	) -> void;

	auto begin_block(
		const id& id,
		std::uint64_t parent
	) -> std::uint64_t;

	auto end_block(
		const id& id,
		std::uint64_t eid,
		std::uint64_t parent
	) -> void;

	template <typename F>
	auto scope(
		const id& id,
		F&& f
	) -> void;

	template <typename F>
	auto scope(
		const id& id,
		F&& f,
		std::uint64_t parent
	) -> void;

	auto begin_async(
		const id& id,
		std::uint64_t key
	) -> void;

	auto end_async(
		const id& id,
		std::uint64_t key
	) -> void;

	auto mark(
		const id& id
	) -> void;

	auto counter(
		const id& id,
		double value
	) -> void;

	auto current_eid(
	) -> std::uint64_t;

	struct node {
		uuid id;
		std::uint32_t trace_id;
		time_t<std::uint64_t> start;
		time_t<std::uint64_t> stop;
		time_t<std::uint64_t> self;
		const node* children_first = nullptr;
		std::size_t children_count = 0;
	};

	using node_view = std::span<const node>;

	struct frame_view {
		std::span<const node> roots;
		const std::byte* storage;
	};

	auto finalize_frame(
	) -> void;

	auto dump_browser(
		const std::filesystem::path& path,
		const frame_view& view
	) -> void;

	auto view(
	) -> frame_view;

	struct thread_pause {
		thread_pause();
		~thread_pause();
	};

	auto paused(
	) -> bool;

	auto set_enabled(
		bool enable
	) -> void;

	auto enabled(
	) -> bool;

	auto set_finalize_paused(
		bool pause
	) -> void;

	auto finalize_paused(
	) -> bool;

	std::atomic<std::uint64_t> frame_seq{0};
}

namespace gse::trace {
	enum struct event_type : std::uint8_t {
		begin,
		end,
		instant,
		async_begin,
		async_end,
		counter
	};

	struct event {
		event_type type;
		uuid id;
		std::uint64_t eid = 0;
		std::uint64_t parent_eid = 0;
		std::uint64_t tid = 0;
		time_t<std::uint64_t> ts;
		double value;
		std::uint64_t key;
	};

	class scsp_events {
	public:
		auto push(
			const event& e
		) noexcept -> void;

		template <typename Out>
		auto drain_to(
			Out& out
		) noexcept -> void;

		auto clear(
		) noexcept -> void;

		auto size(
		) const noexcept -> std::size_t;
	private:
		static constexpr std::uint32_t capacity_pow_2 = 1u << 18;
		static constexpr std::uint32_t capacity = capacity_pow_2;
		static constexpr std::uint32_t capacity_mask = capacity_pow_2 - 1;

		alignas(64) std::atomic<std::uint32_t> m_w{ 0 };
		alignas(64) std::uint32_t m_r{ 0 };

		std::unique_ptr<event[]> m_events = std::make_unique<event[]>(capacity);
	};

	struct thread_buffer {
		scsp_events events;
		std::vector<std::uint64_t> stack;
		std::uint32_t tid = 0;
		bool registered = false;
	};

	thread_local int tls_pause_depth = 0;

	std::atomic trace_enabled = true;
	std::atomic finalize_paused_flag = false;

	std::atomic<std::uint64_t> next_eid{ 1024 };
	std::atomic<std::uint32_t> next_tid{ 0 };

	config global_config;

	thread_local thread_buffer tls;

	std::mutex tid_map_mutex;
	std::unordered_map<std::thread::id, std::uint32_t> tid_map;

	std::mutex tls_registry_mutex;
	std::vector<thread_buffer*> tls_registry;

	struct frame_storage {
		std::vector<event> merged;

		struct flat_node {
			uuid id;
			std::uint32_t tid;
			time_t<std::uint64_t> start = {};
			time_t<std::uint64_t> end = {};
			time_t<std::uint64_t> self = {};
			std::vector<std::size_t> children_idx;
		};

		std::vector<flat_node> flat;
		std::vector<node> node_pool;
		std::vector<node> roots;

		struct span {
			uuid id;
			std::uint32_t tid;
			time_t<std::uint64_t> t0;
			time_t<std::uint64_t> t1;
			std::uint64_t parent;
		};

		std::unordered_map<std::uint64_t, span> open_spans;
	};

	double_buffer<frame_storage> frames;

	std::unordered_set<std::uint64_t> open_parents;
	std::mutex open_m;

	auto ensure_tls_registered(
	) -> void;

	auto make_tid(
	) -> std::uint32_t;

	auto emit(
		const event& e
	) -> void;

	auto current_parent_eid(
	) -> std::uint64_t;

	auto build_tree(
		frame_storage& fs
	) -> void;

	auto make_loc_id(
		const std::source_location& loc
	) -> id;

	auto allocate_span_eid(
	) -> std::uint64_t;
}

auto gse::trace::start(const config& cfg) -> void {
	global_config = cfg;

	ensure_tls_registered();
	make_tid();

	frames = frame_storage{};
}

auto gse::trace::begin_block(const id& id, std::uint64_t parent) -> std::uint64_t {
	if (paused()) return 0;

	ensure_tls_registered();

	if (parent != 0 && parent < 1024) {
		parent = 0;
	}

	const auto tid = make_tid();
	const auto eid = allocate_span_eid();

	{
		std::scoped_lock lk(open_m);
		open_parents.erase(eid);
	}

	emit({
		.type = event_type::begin,
		.id = id.number(),
		.eid = eid,
		.parent_eid = parent,
		.tid = tid,
		.ts = system_clock::now<std::uint64_t>()
	});

	return eid;
}

auto gse::trace::end_block(const id& id, const std::uint64_t eid, const std::uint64_t parent) -> void {
	if (paused() || eid == 0) {
		return;
	}

	ensure_tls_registered();

	{
		std::scoped_lock lk(open_m);
		open_parents.erase(eid);
	}

	emit({
		.type = event_type::end,
		.id = id.number(),
		.eid = eid,
		.parent_eid = parent,
		.tid = make_tid(),
		.ts = system_clock::now<std::uint64_t>()
	});
}

template <typename F>
auto gse::trace::scope(const id& id, F&& f) -> void {
	const auto parent = current_parent_eid();
	scope(id, std::forward<F>(f), parent);
}

template <typename F>
auto gse::trace::scope(const id& id, F&& f, std::uint64_t parent) -> void {
	if (paused()) {
		std::forward<F>(f)();
		return;
	}

	ensure_tls_registered();

	if (parent != 0 && parent < 1024) {
		parent = 0;
	}

	const auto tid = make_tid();
	const bool need_push_parent = parent != 0 && (tls.stack.empty() || tls.stack.back() != parent);

	if (need_push_parent) {
		tls.stack.push_back(parent);
	}

	const auto eid = allocate_span_eid();

	{
		std::scoped_lock lk(open_m);
		open_parents.insert(eid);
	}

	emit({
		.type = event_type::begin,
		.id = id.number(),
		.eid = eid,
		.parent_eid = parent,
		.tid = tid,
		.ts = system_clock::now<std::uint64_t>()
	});

	tls.stack.push_back(eid);

	auto guard = make_scope_exit([eid, parent, tid, uid = id.number(), need_push_parent] {
		{
			std::scoped_lock lk(open_m);
			open_parents.erase(eid);
		}

		if (!tls.stack.empty() && tls.stack.back() == eid) {
			emit({
				.type = event_type::end,
				.id = uid,
				.eid = eid,
				.parent_eid = parent,
				.tid = tid,
				.ts = system_clock::now<std::uint64_t>()
			});
			tls.stack.pop_back();
			if (need_push_parent && !tls.stack.empty() && tls.stack.back() == parent) {
				tls.stack.pop_back();
			}
		}
	});

	std::forward<F>(f)();
}

auto gse::trace::begin_async(const id& id, const std::uint64_t key) -> void {
	if (paused()) {
		return;
	}

	ensure_tls_registered();

	emit({
		.type = event_type::async_begin,
		.id = id.number(),
		.eid = 0,
		.parent_eid = current_parent_eid(),
		.tid = make_tid(),
		.ts = system_clock::now<std::uint64_t>(),
		.value = 0.0,
		.key = key
	});
}

auto gse::trace::end_async(const id& id, const std::uint64_t key) -> void {
	if (paused()) {
		return;
	}

	ensure_tls_registered();

	emit({
		.type = event_type::async_end,
		.id = id.number(),
		.eid = 0,
		.parent_eid = 0,
		.tid = make_tid(),
		.ts = system_clock::now<std::uint64_t>(),
		.value = 0.0,
		.key = key
	});
}

auto gse::trace::mark(const id& id) -> void {
	if (paused()) {
		return;
	}

	ensure_tls_registered();

	emit({
		.type = event_type::instant,
		.id = id.number(),
		.eid = 0,
		.parent_eid = current_parent_eid(),
		.tid = make_tid(),
		.ts = system_clock::now<std::uint64_t>(),
		.value = 0.0,
		.key = 0
	});
}

auto gse::trace::counter(const id& id, const double value) -> void {
	if (paused()) {
		return;
	}

	ensure_tls_registered();

	emit({
		.type = event_type::counter,
		.id = id.number(),
		.eid = 0,
		.parent_eid = 0,
		.tid = make_tid(),
		.ts = system_clock::now<std::uint64_t>(),
		.value = value,
		.key = 0,
	});
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

auto gse::trace::dump_browser(const std::filesystem::path& path, const frame_view& view) -> void {
	std::ofstream out(path);

	assert(
		out.is_open(),
		std::source_location::current(),
		"Failed to open trace output file"
	);

	out << R"({"traceEvents":[)";

	bool first = true;

	std::function<void(const node&)> emit_node = [&](const node& n) {
		const auto& tag = find(n.id).tag();

		if (!first) {
			out << ',';
		}

		first = false;

		const auto dur = n.stop - n.start;

		out << R"({"name":")" << tag << R"(",)"
			<< R"("ph":"X",)"
			<< R"("ts":)" << n.start.as<microseconds>() << ','
			<< R"("dur":)" << dur.as<microseconds>() << ','
			<< R"("pid":0,)"
			<< R"("tid":)" << n.trace_id
			<< "}";

		for (std::size_t i = 0; i < n.children_count; ++i) {
			emit_node(n.children_first[i]);
		}
	};

	for (const auto& r : view.roots) {
		emit_node(r);
	}

	out << "]}";
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

auto gse::trace::set_finalize_paused(bool pause) -> void {
	finalize_paused_flag.store(pause, std::memory_order_relaxed);
}

auto gse::trace::finalize_paused() -> bool {
	return finalize_paused_flag.load(std::memory_order_relaxed);
}

auto gse::trace::scsp_events::push(const event& e) noexcept -> void {
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
		m_r = m_r + 1 & capacity_mask;
	}
}

auto gse::trace::scsp_events::clear() noexcept -> void {
	m_r = m_w.load(std::memory_order_acquire);
}

auto gse::trace::scsp_events::size() const noexcept -> std::size_t {
	return (m_w.load(std::memory_order_acquire) - m_r) & capacity_mask;
}

auto gse::trace::ensure_tls_registered() -> void {
	if (tls.registered) {
		return;
	}

	std::lock_guard lock(tls_registry_mutex);
	tls_registry.push_back(&tls);
	tls.registered = true;
}

auto gse::trace::make_tid() -> std::uint32_t {
	if (tls.tid != 0) {
		return tls.tid;
	}

	std::lock_guard lock(tid_map_mutex);

	if (const auto it = tid_map.find(std::this_thread::get_id()); it == tid_map.end()) {
		const auto id = next_tid.fetch_add(1, std::memory_order::relaxed) + 1;
		tls.tid = id;
		tid_map.emplace(std::this_thread::get_id(), id);
	}
	else {
		tls.tid = it->second;
	}

	return tls.tid;
}

auto gse::trace::emit(const event& e) -> void {
	if (paused()) {
		return;
	}

	if (tls.events.size() >= global_config.per_thread_event_cap) {
		return;
	}

	tls.events.push(e);
}

auto gse::trace::current_parent_eid() -> std::uint64_t {
	while (!tls.stack.empty()) {
		const auto top = tls.stack.back();

		if (top == 0) {
			return 0;
		}

		std::scoped_lock lk(open_m);
		if (open_parents.contains(top)) {
			return top;
		}
		tls.stack.pop_back();

	}

	return 0;
}

auto gse::trace::build_tree(frame_storage& fs) -> void {
	fs.merged.clear();

	{
		std::lock_guard lk(tls_registry_mutex);
		for (auto* tb : tls_registry) {
			if (!tb) continue;
			tb->events.drain_to(fs.merged);
		}
	}

	std::ranges::sort(fs.merged, [](const event& a, const event& b) {
		if (a.ts != b.ts) return a.ts < b.ts;
		return static_cast<int>(a.type) < static_cast<int>(b.type);
	});

	std::unordered_map<std::uint64_t, frame_storage::span> spans = fs.open_spans;

	for (const auto& e : fs.merged) {
		switch (e.type) {
			case event_type::begin:
				spans.emplace(
					e.eid,
					frame_storage::span{
						.id	 = e.id,
						.tid	= static_cast<std::uint32_t>(e.tid),
						.t0	 = e.ts,
						.t1	 = {},
						.parent = e.parent_eid
					}
				);
				break;
			case event_type::end:
				if (auto it = spans.find(e.eid); it != spans.end()) {
					it->second.t1 = e.ts;
				}
				break;
			default:
				break;
		}
	}

	fs.flat.clear();
	fs.flat.reserve(spans.size());

	std::unordered_map<std::uint64_t, std::size_t> index_of;
	index_of.reserve(spans.size());

	for (auto& [eid, sp] : spans) {
		if (sp.t1 < sp.t0) {
			sp.t1 = sp.t0;
		}

		const std::size_t i = fs.flat.size();
		fs.flat.push_back(frame_storage::flat_node{
			.id = sp.id,
			.tid = sp.tid,
			.start = sp.t0,
			.end = sp.t1,
			.self = {},
			.children_idx = {}
		});

		index_of.emplace(eid, i);
	}

	std::vector has_parent(fs.flat.size(), false);
	for (const auto& [eid, sp] : spans) {
		const auto child_i = index_of[eid];
		if (sp.parent != 0 && index_of.contains(sp.parent)) {
			const auto parent_i = index_of[sp.parent];
			fs.flat[parent_i].children_idx.push_back(child_i);
			has_parent[child_i] = true;
		}
	}

	std::vector<std::size_t> roots_idx;
	roots_idx.reserve(fs.flat.size());

	for (std::size_t i = 0; i < fs.flat.size(); ++i) {
		if (!has_parent[i]) roots_idx.push_back(i);
	}

	auto total_ns = [&](const std::size_t i) -> time_t<std::uint64_t> {
		return fs.flat[i].end - fs.flat[i].start;
	};

	std::function<void(std::size_t)> compute_self = [&](const std::size_t i) {
		auto& n = fs.flat[i];
		time_t<std::uint64_t> sum_children = {};

		for (const auto c : n.children_idx) {
			compute_self(c);
			sum_children += total_ns(c);
		}

		const auto tot = total_ns(i);
		n.self = (tot > sum_children) ? (tot - sum_children) : time_t<std::uint64_t>{};
	};

	for (const auto r : roots_idx) {
		compute_self(r);
	}

	fs.node_pool.clear();
	fs.roots.clear();
	fs.node_pool.reserve(fs.flat.size());
	fs.roots.reserve(roots_idx.size());

	auto emplace_shallow = [&](std::size_t flat_i) -> std::size_t {
		const auto& fn = fs.flat[flat_i];
		fs.node_pool.push_back(gse::trace::node{
			.id = fn.id,
			.trace_id = fn.tid,
			.start = fn.start,
			.stop  = fn.end,
			.self  = fn.self,
			.children_first = nullptr,
			.children_count = 0
		});
		return fs.node_pool.size() - 1;
	};

	std::function<void(std::size_t, std::size_t)> build_subtree = [&](std::size_t node_idx, std::size_t flat_i) {
		auto& n  = fs.node_pool[node_idx];
		const auto& fn = fs.flat[flat_i];

		if (fn.children_idx.empty()) {
			n.children_first = nullptr;
			n.children_count = 0;
			return;
		}

		const std::size_t start = fs.node_pool.size();

		for (std::size_t ci : fn.children_idx) {
			emplace_shallow(ci);
		}

		n.children_first = fs.node_pool.data() + start;
		n.children_count = fn.children_idx.size();

		for (std::size_t k = 0; k < fn.children_idx.size(); ++k) {
			const std::size_t ci = fn.children_idx[k];
			const std::size_t child_node_idx = start + k;
			build_subtree(child_node_idx, ci);
		}
	};

	for (auto r : roots_idx) {
		const std::size_t root_idx = emplace_shallow(r);
		build_subtree(root_idx, r);
		fs.roots.push_back(fs.node_pool[root_idx]);
	}

	fs.open_spans.clear();
	for (auto& [eid, sp] : spans) {
		if (sp.t1 == time_t<std::uint64_t>{}) {
			fs.open_spans.emplace(eid, sp);
		}
	}
}

auto gse::trace::make_loc_id(const std::source_location& loc) -> id {
	std::string_view fn = loc.function_name();

	if (const auto lp = fn.find('('); lp != std::string_view::npos) {
		fn = fn.substr(0, lp);
	}

	while (!fn.empty() && std::isspace(static_cast<unsigned char>(fn.back()))) {
		fn.remove_suffix(1);
	}

	const auto last_col_col = fn.rfind("::");
	std::string_view tag;

	if (last_col_col != std::string_view::npos) {
		std::size_t start = fn.rfind(' ', last_col_col);

		if (start == std::string_view::npos) {
			start = 0;
		} else {
			start += 1;
		}

		tag = fn.substr(start);

		static constexpr std::string_view candidates[] = {
			"__cdecl", "__stdcall", "__thiscall", "__vectorcall", "cdecl", "stdcall", "thiscall", "vectorcall"
		};

		for (auto cc : candidates) {
			if (tag.size() > cc.size() && tag.substr(0, cc.size()) == cc) {
				tag.remove_prefix(cc.size());
				while (!tag.empty() && std::isspace(static_cast<unsigned char>(tag.front()))) {
					tag.remove_prefix(1);
				}
				break;
			}
		}
	} else {
		const auto last_sp = fn.rfind(' ');
		tag = (last_sp == std::string_view::npos) ? fn : fn.substr(last_sp + 1);
	}

	return find_or_generate_id(tag);
}

auto gse::trace::allocate_span_eid() -> std::uint64_t {
	return next_eid.fetch_add(1, std::memory_order_relaxed);
}

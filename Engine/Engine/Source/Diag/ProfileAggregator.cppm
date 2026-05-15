export module gse.diag:profile_aggregator;

import std;

import gse.config;
import gse.math;
import gse.meta;
import gse.core;
import gse.containers;
import gse.time;

import :trace;

export namespace gse::profile {
	using sample_time = time_t<double>;

	struct entry {
		id id;
		sample_time ema{};
		sample_time last{};
		sample_time peak{};
		std::uint64_t sample_count = 0;
		std::uint32_t thread_id = 0;
		bool pooled = false;
		flat_map<std::uint32_t, std::uint64_t> samples_by_tid;
	};

	auto ingest_frame(
	) -> void;

	auto ingest_gpu_sample(
		id pass_id,
		sample_time duration
	) -> void;

	auto lookup_cpu(
		id id
	) -> std::optional<entry>;

	auto lookup_gpu(
		id id
	) -> std::optional<entry>;

	auto top_n(
		std::size_t n,
		bool gpu
	) -> std::vector<entry>;

	auto set_alpha(
		double alpha
	) -> void;

	auto alpha(
	) -> double;

	auto set_enabled(
		bool enabled
	) -> void;

	auto enabled(
	) -> bool;

	auto reset(
	) -> void;

	auto dump(
		const std::filesystem::path& path = config::resource_path / "Misc" / "profile.txt"
	) -> void;

	auto dump_chrome_trace(
		const std::filesystem::path& path = config::resource_path / "Misc" / "profile.json"
	) -> void;
}

namespace gse::profile {
	std::shared_mutex state_mutex;
	flat_map<id, entry> cpu_entries;
	flat_map<id, entry> gpu_entries;
	std::atomic ema_alpha{ 0.1 };
	std::atomic is_enabled{ true };
	std::atomic<std::uint64_t> frame_count{ 0 };

	auto update_entry(
		flat_map<id, entry>& map,
		id id,
		sample_time duration,
		std::uint32_t thread_id,
		bool pooled
	) -> void;

	auto walk_node(
		const trace::node& n,
		flat_map<id, entry>& cpu_agg,
		const std::unordered_set<id>& hidden,
		bool pooled
	) -> void;

	auto write_dag(
		std::ofstream& out
	) -> void;

	auto write_thread_breakdown(
		std::ofstream& out,
		const std::vector<entry>& worker_src
	) -> void;
}

auto gse::profile::update_entry(flat_map<id, entry>& map, const id id, const sample_time duration, const std::uint32_t thread_id, const bool pooled) -> void {
	auto& e = map[id];
	if (e.sample_count == 0) {
		e.id = id;
		e.ema = duration;
	}
	else {
		const double a = ema_alpha.load(std::memory_order_relaxed);
		e.ema = a * duration + (1.0 - a) * e.ema;
	}
	e.last = duration;
	if (duration > e.peak) {
		e.peak = duration;
	}
	e.thread_id = thread_id;
	e.pooled = e.pooled || pooled;
	++e.sample_count;
	++e.samples_by_tid[thread_id];
}

auto gse::profile::walk_node(const trace::node& n, flat_map<id, entry>& cpu_agg, const std::unordered_set<id>& hidden, const bool pooled) -> void {
	const auto main_tid = trace::main_tid();
	const bool node_pooled = pooled || (main_tid != 0 && n.trace_id != main_tid);

	if (!hidden.contains(n.id)) {
		update_entry(cpu_agg, n.id, sample_time(n.self), n.trace_id, node_pooled);
	}
	for (std::size_t i = 0; i < n.children_count; ++i) {
		walk_node(n.children_first[i], cpu_agg, hidden, node_pooled);
	}
}

auto gse::profile::ingest_frame() -> void {
	if (!is_enabled.load(std::memory_order_relaxed)) {
		return;
	}

	const auto [roots, storage] = trace::view();
	const auto hidden = trace::hidden_ids_snapshot();

	std::unique_lock lk(state_mutex);
	frame_count.fetch_add(1, std::memory_order_relaxed);
	for (const auto& root : roots) {
		walk_node(root, cpu_entries, hidden, false);
	}
}

auto gse::profile::ingest_gpu_sample(const id pass_id, const sample_time duration) -> void {
	if (!is_enabled.load(std::memory_order_relaxed)) {
		return;
	}

	std::unique_lock lk(state_mutex);
	update_entry(gpu_entries, pass_id, duration, 0, false);
}

auto gse::profile::lookup_cpu(const id id) -> std::optional<entry> {
	std::shared_lock lk(state_mutex);
	if (const auto it = cpu_entries.find(id); it != cpu_entries.end()) {
		return it->second;
	}
	return std::nullopt;
}

auto gse::profile::lookup_gpu(const id id) -> std::optional<entry> {
	std::shared_lock lk(state_mutex);
	if (const auto it = gpu_entries.find(id); it != gpu_entries.end()) {
		return it->second;
	}
	return std::nullopt;
}

auto gse::profile::top_n(const std::size_t n, const bool gpu) -> std::vector<entry> {
	std::shared_lock lk(state_mutex);
	const auto& source = gpu ? gpu_entries : cpu_entries;

	std::vector<entry> out;
	out.reserve(source.size());
	for (const auto& e : source | std::views::values) {
		out.push_back(e);
	}

	std::ranges::sort(out, [](const entry& a, const entry& b) {
		return a.ema > b.ema;
	});

	if (out.size() > n) {
		out.resize(n);
	}
	return out;
}

auto gse::profile::set_alpha(const double alpha) -> void {
	ema_alpha.store(std::clamp(alpha, 0.001, 1.0), std::memory_order_relaxed);
}

auto gse::profile::alpha() -> double {
	return ema_alpha.load(std::memory_order_relaxed);
}

auto gse::profile::set_enabled(const bool enabled) -> void {
	is_enabled.store(enabled, std::memory_order_relaxed);
}

auto gse::profile::enabled() -> bool {
	return is_enabled.load(std::memory_order_relaxed);
}

auto gse::profile::reset() -> void {
	std::unique_lock lk(state_mutex);
	cpu_entries.clear();
	gpu_entries.clear();
	frame_count.store(0, std::memory_order_relaxed);
}

auto gse::profile::dump(const std::filesystem::path& path) -> void {
	std::filesystem::create_directories(path.parent_path());

	std::ofstream out(path);
	if (!out.is_open()) {
		return;
	}

	const auto fps = system_clock::fps();
	const auto frame_time = fps > 0 ? milliseconds(1000.0 / static_cast<double>(fps)) : sample_time{};
	const auto frames = frame_count.load(std::memory_order_relaxed);

	struct row_view {
		const entry* e;
		sample_time per_frame;
		double calls_per_frame;
	};

	const auto build_rows = [frames](const std::vector<entry>& src) {
		std::vector<row_view> out;
		out.reserve(src.size());
		for (const auto& e : src) {
			const double calls = frames > 0 ? static_cast<double>(e.sample_count) / static_cast<double>(frames) : 0.0;
			out.push_back({ &e, e.ema * calls, calls });
		}
		std::ranges::sort(out, [](const row_view& a, const row_view& b) { return a.per_frame > b.per_frame; });
		return out;
	};

	const auto write_section = [&out, frame_time](const std::string_view title, const std::vector<row_view>& rows) {
		std::size_t tag_width = std::string_view("tag").size();
		for (const auto& [e, per_frame, calls_per_frame] : rows) {
			tag_width = std::max(tag_width, e->id.tag().size());
		}

		out << "--- " << title << " ---\n";
		out << std::format(
			"{:<{}} {:>13} {:>13} {:>13} {:>13} {:>7} {:>8} {:>14} {:>9}\n",
			"tag", tag_width, "per/f", "avg", "peak", "last", "% top", "% frame", "total", "calls/f"
		);
		out << std::string(tag_width + 13 * 4 + 7 + 8 + 14 + 9 + 8, '-') << '\n';

		const auto top = rows.empty() ? sample_time{} : rows.front().per_frame;

		for (const auto& [e, per_frame, calls_per_frame] : rows) {
			const double pct_top = top > sample_time{} ? (per_frame / top) * 100.0 : 0.0;
			const double pct_frame = frame_time > sample_time{} ? (per_frame / frame_time) * 100.0 : 0.0;
			const auto total = e->ema * static_cast<double>(e->sample_count);

			out << std::format(
				"{:<{}} {:>10.2f:us} {:>10.2f:us} {:>10.2f:us} {:>10.2f:us} {:>6.1f}% {:>7.1f}% {:>11.2f:ms} {:>9.2f}\n",
				e->id.tag(),
				tag_width,
				per_frame,
				e->ema,
				e->peak,
				e->last,
				pct_top,
				pct_frame,
				total,
				calls_per_frame
			);
		}
		out << '\n';
	};

	std::vector<entry> main_src;
	std::vector<entry> worker_src;
	std::vector<entry> gpu_src;
	{
		std::shared_lock lk(state_mutex);
		main_src.reserve(cpu_entries.size());
		worker_src.reserve(cpu_entries.size());
		for (const auto& e : cpu_entries | std::views::values) {
			(e.pooled ? worker_src : main_src).push_back(e);
		}
		gpu_src.reserve(gpu_entries.size());
		for (const auto& e : gpu_entries | std::views::values) {
			gpu_src.push_back(e);
		}
	}

	const auto main_rows = build_rows(main_src);
	const auto worker_rows = build_rows(worker_src);
	const auto gpu_rows = build_rows(gpu_src);

	const auto cpu_top = main_rows.empty() ? sample_time{} : main_rows.front().per_frame;
	const auto gpu_top = gpu_rows.empty() ? sample_time{} : gpu_rows.front().per_frame;

	out << std::format("=== Profile dump ({}) ===\n", system_clock::timestamp_filename());
	out << std::format(
		"frame: {:.2f:ms} ({} fps)    main-thread top: {:.2f:ms}    GPU top: {:.2f:ms}    {} frames profiled    EMA alpha: {:.3f}\n",
		frame_time,
		fps,
		cpu_top,
		gpu_top,
		frames,
		alpha()
	);
	out << "sorted by per/f = avg * calls/f (real per-frame cost).  % top = per/f relative to top row.  % frame = per/f / frame_time.  Worker rows can sum > 100% (parallel).\n\n";

	write_section("CPU - Main Thread (sequential, blocks the frame)", main_rows);
	write_section("CPU - Workers (parallel; sums can exceed 100%)", worker_rows);
	write_section("GPU (per-pass time)", gpu_rows);

	write_thread_breakdown(out, worker_src);
	write_dag(out);
}

auto gse::profile::write_thread_breakdown(std::ofstream& out, const std::vector<entry>& worker_src) -> void {
	const auto frames = frame_count.load(std::memory_order_relaxed);
	const auto main_tid = trace::main_tid();

	std::vector<const entry*> sorted;
	sorted.reserve(worker_src.size());
	for (const auto& e : worker_src) {
		if (e.sample_count > 0) {
			sorted.push_back(&e);
		}
	}
	std::ranges::sort(sorted, [](const entry* a, const entry* b) {
		return a->ema * static_cast<double>(a->sample_count) > b->ema * static_cast<double>(b->sample_count);
	});

	out << "--- Worker tag thread breakdown (where each tag actually ran) ---\n";
	out << std::format("main tid = {}.  Counts shown are samples per frame on each thread.\n\n", main_tid);

	std::size_t tag_width = std::string_view("tag").size();
	for (const auto* e : sorted) {
		tag_width = std::max(tag_width, e->id.tag().size());
	}

	out << std::format("{:<{}}  {:>10}  {:>10}  {:>40}\n", "tag", tag_width, "main/f", "worker/f", "per-tid /f (top 6)");
	out << std::string(tag_width + 2 + 10 + 2 + 10 + 2 + 40, '-') << '\n';

	const std::size_t max_to_show = std::min<std::size_t>(20, sorted.size());
	for (std::size_t i = 0; i < max_to_show; ++i) {
		const auto& e = *sorted[i];

		double main_per_frame = 0.0;
		double worker_per_frame = 0.0;
		std::vector<std::pair<std::uint32_t, std::uint64_t>> tids;
		tids.reserve(e.samples_by_tid.size());
		for (const auto& [tid, count] : e.samples_by_tid) {
			tids.emplace_back(tid, count);
			const double per = frames > 0 ? static_cast<double>(count) / static_cast<double>(frames) : 0.0;
			if (tid == main_tid) {
				main_per_frame += per;
			}
			else {
				worker_per_frame += per;
			}
		}
		std::ranges::sort(tids, [](const auto& a, const auto& b) { return a.second > b.second; });

		std::string tid_breakdown;
		const std::size_t tid_show = std::min<std::size_t>(6, tids.size());
		for (std::size_t j = 0; j < tid_show; ++j) {
			const double per = frames > 0 ? static_cast<double>(tids[j].second) / static_cast<double>(frames) : 0.0;
			if (j > 0) {
				tid_breakdown += " ";
			}
			tid_breakdown += std::format("t{}:{:.2f}", tids[j].first, per);
		}

		out << std::format("{:<{}}  {:>10.2f}  {:>10.2f}  {:<40}\n",
			e.id.tag(), tag_width, main_per_frame, worker_per_frame, tid_breakdown);
	}
	out << '\n';
}

auto gse::profile::write_dag(std::ofstream& out) -> void {
	const auto [roots, storage] = trace::view();
	if (roots.empty()) {
		return;
	}

	auto tmin = roots.front().start;
	auto tmax = roots.front().stop;
	for (const auto& r : roots) {
		if (r.start < tmin) {
			tmin = r.start;
		}
		if (r.stop > tmax) {
			tmax = r.stop;
		}
	}
	if (tmax <= tmin) {
		return;
	}

	const auto total_range = sample_time(tmax - tmin);
	const double total_us = total_range.as<gse::microseconds>();

	constexpr std::size_t bar_width = 80;
	constexpr int max_depth = 12;
	constexpr double min_us_to_show = 5.0;

	const auto main_tid = trace::main_tid();

	std::function<std::size_t(const trace::node&, int)> max_label_width = [&](const trace::node& n, int depth) -> std::size_t {
		if (depth > max_depth) {
			return 0;
		}
		std::size_t w = static_cast<std::size_t>(depth) * 2 + n.id.tag().size();
		for (std::size_t i = 0; i < n.children_count; ++i) {
			w = std::max(w, max_label_width(n.children_first[i], depth + 1));
		}
		return w;
	};

	std::size_t name_width = 0;
	for (const auto& r : roots) {
		name_width = std::max(name_width, max_label_width(r, 0));
	}

	out << "--- Frame DAG (absolute timeline; bars at same column = parallel) ---\n";
	out << std::format("total range: {:.2f:us} across {} columns ({:.2f} us per column).  '#' = main thread, '=' = worker.\n\n", total_range, bar_width, total_us / static_cast<double>(bar_width));

	std::function<void(const trace::node&, int)> render = [&](const trace::node& n, int depth) {
		if (depth > max_depth) {
			return;
		}
		if (n.stop <= n.start) {
			return;
		}
		const auto duration = sample_time(n.stop - n.start);
		if (duration.as<gse::microseconds>() < min_us_to_show) {
			return;
		}

		const double offset_us = sample_time(n.start - tmin).as<gse::microseconds>();
		const double dur_us = duration.as<gse::microseconds>();

		std::size_t col_start = static_cast<std::size_t>((offset_us / total_us) * static_cast<double>(bar_width));
		std::size_t col_end = static_cast<std::size_t>(((offset_us + dur_us) / total_us) * static_cast<double>(bar_width));
		col_start = std::min(col_start, bar_width - 1);
		col_end = std::min(col_end, bar_width);
		if (col_end <= col_start) {
			col_end = col_start + 1;
		}

		const char fill = (main_tid != 0 && n.trace_id != main_tid) ? '=' : '#';
		std::string bar(bar_width, ' ');
		for (std::size_t i = col_start; i < col_end; ++i) {
			bar[i] = fill;
		}

		const std::string indent(static_cast<std::size_t>(depth) * 2, ' ');
		const auto label = std::format("{}{}", indent, n.id.tag());
		out << std::format("|{}| {:>10.2f:us}  tid:{:<3}  {:<{}}\n", bar, duration, n.trace_id, label, name_width);

		for (std::size_t i = 0; i < n.children_count; ++i) {
			render(n.children_first[i], depth + 1);
		}
	};

	std::vector<const trace::node*> sorted_roots;
	sorted_roots.reserve(roots.size());
	for (const auto& r : roots) {
		sorted_roots.push_back(&r);
	}
	std::ranges::sort(sorted_roots, [](const trace::node* a, const trace::node* b) {
		return a->start < b->start;
	});

	for (const auto* r : sorted_roots) {
		render(*r, 0);
	}
}

auto gse::profile::dump_chrome_trace(const std::filesystem::path& path) -> void {
	std::filesystem::create_directories(path.parent_path());

	std::ofstream out(path);
	if (!out.is_open()) {
		return;
	}

	const auto [roots, storage] = trace::view();

	out << "[\n";

	if (roots.empty()) {
		out << "]\n";
		return;
	}

	auto tmin = roots.front().start;
	for (const auto& r : roots) {
		if (r.start < tmin) {
			tmin = r.start;
		}
	}

	const auto escape = [](const std::string_view s) {
		std::string r;
		r.reserve(s.size() + 2);
		for (const char c : s) {
			switch (c) {
				case '"':  r += "\\\""; break;
				case '\\': r += "\\\\"; break;
				case '\n': r += "\\n"; break;
				case '\r': r += "\\r"; break;
				case '\t': r += "\\t"; break;
				default:
					if (static_cast<unsigned char>(c) < 0x20) {
						r += std::format("\\u{:04x}", static_cast<unsigned>(c));
					}
					else {
						r += c;
					}
			}
		}
		return r;
	};

	bool first = true;
	const auto sep = [&] {
		if (!first) {
			out << ",\n";
		}
		first = false;
	};

	std::unordered_set<std::uint32_t> tids;
	const auto hidden = trace::hidden_ids_snapshot();

	std::function<void(const trace::node&)> emit_node = [&](const trace::node& n) {
		if (n.stop > n.start && !hidden.contains(n.id)) {
			const double ts_us = sample_time(n.start - tmin).as<gse::microseconds>();
			const double dur_us = sample_time(n.stop - n.start).as<gse::microseconds>();
			tids.insert(n.trace_id);

			sep();
			out << std::format(
				R"({{"ph":"X","name":"{}","cat":"perf","ts":{:.3f},"dur":{:.3f},"pid":1,"tid":{}}})",
				escape(n.id.tag()), ts_us, dur_us, n.trace_id
			);
		}
		for (std::size_t i = 0; i < n.children_count; ++i) {
			emit_node(n.children_first[i]);
		}
	};

	for (const auto& r : roots) {
		emit_node(r);
	}

	const auto main_tid = trace::main_tid();
	for (const auto tid : tids) {
		std::string name;
		if (tid == main_tid) {
			name = "main";
		}
		else if (const auto vname = trace::virtual_thread_name(tid)) {
			name = *vname;
		}
		else {
			name = std::format("worker {}", tid);
		}

		sep();
		out << std::format(
			R"({{"ph":"M","name":"thread_name","pid":1,"tid":{},"args":{{"name":"{}"}}}})",
			tid, escape(name)
		);
	}

	out << "\n]\n";
}

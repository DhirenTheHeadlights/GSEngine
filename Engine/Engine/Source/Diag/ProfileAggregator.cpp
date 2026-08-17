module gse.diag:profile_aggregator_impl;

import std;

import :profile_aggregator;
import :trace;

import gse.config;
import gse.math;
import gse.meta;
import gse.core;
import gse.containers;
import gse.time;
import gse.win32;

auto gse::profile::storage_for(const domain domain) -> std::flat_map<id, entry>& {
	return entries[static_cast<std::size_t>(domain)];
}

auto gse::profile::sample_count_of(const entry& e) -> std::uint64_t {
	std::uint64_t total = 0;
	for (const auto count : std::views::values(e.samples_by_tid)) {
		total += count;
	}
	return total;
}

auto gse::profile::dominant_tid_of(const entry& e) -> std::uint32_t {
	std::uint32_t best_tid = 0;
	std::uint64_t best_count = 0;

	for (const auto& [tid, count] : e.samples_by_tid) {
		if (count > best_count) {
			best_count = count;
			best_tid = tid;
		}
	}

	return best_tid;
}

auto gse::profile::on_main_thread(const std::uint32_t dominant_tid, const std::uint32_t main_tid) -> bool {
	return main_tid == 0 || dominant_tid == main_tid;
}

auto gse::profile::to_row(const entry& e) -> row {
	return {
		.id = e.id,
		.ema = e.ema,
		.last = e.last,
		.peak = e.peak,
		.sample_count = sample_count_of(e)
	};
}

auto gse::profile::to_report(const entry& e, const std::uint64_t frames) -> report_entry {
	const auto count = sample_count_of(e);
	const double calls = frames > 0 ? static_cast<double>(count) / static_cast<double>(frames) : 0.0;

	return {
		.id = e.id,
		.per_frame = e.ema * calls,
		.ema = e.ema,
		.last = e.last,
		.peak = e.peak,
		.calls_per_frame = calls,
		.sample_count = count,
		.peak_frame = e.peak_frame,
		.spike_count = e.spike_count,
		.dominant_tid = dominant_tid_of(e)
	};
}

auto gse::profile::update_entry(std::flat_map<id, entry>& map, const id id, const sample_time duration, const std::uint32_t thread_id, const std::uint64_t frame_index) -> void {
	auto& e = map[id];

	if (e.samples_by_tid.empty()) {
		e.id = id;
		e.ema = duration;
	}
	else {
		if (e.ema > sample_time{} && duration > e.ema * spike_ratio) {
			++e.spike_count;
		}
		const double a = ema_alpha.load(std::memory_order_relaxed);
		e.ema = a * duration + (1.0 - a) * e.ema;
	}

	e.last = duration;
	if (duration > e.peak) {
		e.peak = duration;
		e.peak_frame = frame_index;
	}

	++e.samples_by_tid[thread_id];
}

auto gse::profile::ingest_frame() -> void {
	if (!is_enabled.load(std::memory_order_relaxed)) {
		return;
	}

	const auto fv = trace::view();
	if (fv.generation == last_generation.load(std::memory_order_relaxed)) {
		return;
	}
	last_generation.store(fv.generation, std::memory_order_relaxed);

	if (const auto remaining = warmup_remaining.load(std::memory_order_relaxed); remaining > 0) {
		warmup_remaining.store(remaining - 1, std::memory_order_relaxed);
		return;
	}

	const auto hidden = trace::hidden_ids_snapshot();

	const auto frame_index = frame_count.load(std::memory_order_relaxed);

	std::unique_lock lk(state_mutex);
	auto& agg = storage_for(domain::cpu);

	for (const auto& n : fv.nodes) {
		if (n.open || hidden.contains(n.id)) {
			continue;
		}
		update_entry(agg, n.id, sample_time(n.self), n.trace_id, frame_index);
	}

	frame_count.fetch_add(1, std::memory_order_relaxed);
	lk.unlock();

	if (recording_frames.load(std::memory_order_relaxed)) {
		record_frame(fv);
	}
}

auto gse::profile::set_frame_recording(const bool enabled) -> void {
	recording_frames.store(enabled, std::memory_order_relaxed);
}

auto gse::profile::frame_recording() -> bool {
	return recording_frames.load(std::memory_order_relaxed);
}

auto gse::profile::record_frame(const trace::frame_view& view) -> void {
	if (view.nodes.empty() || view.roots.empty()) {
		return;
	}

	std::lock_guard lock(recorded_mutex);

	report_frame frame;
	frame.generation = view.generation;
	frame.children.assign(view.children.begin(), view.children.end());
	frame.roots.assign(view.roots.begin(), view.roots.end());
	frame.nodes.reserve(view.nodes.size());

	time_t<std::uint64_t> first = view.nodes.front().start;
	time_t<std::uint64_t> last = first;
	for (const trace::node& n : view.nodes) {
		const auto [it, inserted] = recorded_tag_indices.try_emplace(n.id.number(), static_cast<std::uint32_t>(recorded_tags.size()));
		if (inserted) {
			recorded_tags.emplace_back(n.id.tag());
		}
		frame.nodes.push_back({
			.tag = it->second,
			.trace_id = n.trace_id,
			.start = n.start,
			.stop = n.stop,
			.self = n.self,
			.children_first = n.children_first,
			.children_count = n.children_count,
			.open = n.open,
		});
		if (n.start < first) {
			first = n.start;
		}
		if (!n.open && n.stop > last) {
			last = n.stop;
		}
	}
	frame.origin = first;
	frame.span = sample_time(last - first);

	recorded_node_total += frame.nodes.size();
	recorded_frames.push_back(std::move(frame));

	const auto capacity = max_recorded_frames.load(std::memory_order_relaxed);
	while (recorded_frames.size() > capacity || (recorded_node_total > max_recorded_nodes && recorded_frames.size() > 1)) {
		recorded_node_total -= recorded_frames.front().nodes.size();
		recorded_frames.pop_front();
	}
}

auto gse::profile::ingest_gpu_sample(const id pass_id, const sample_time duration) -> void {
	if (!is_enabled.load(std::memory_order_relaxed) || warming_up()) {
		return;
	}

	const auto frame_index = frame_count.load(std::memory_order_relaxed);

	std::unique_lock lk(state_mutex);
	update_entry(storage_for(domain::gpu), pass_id, duration, 0, frame_index);
}

auto gse::profile::lookup(const id id, const domain domain) -> std::optional<row> {
	std::shared_lock lk(state_mutex);
	const auto& source = storage_for(domain);

	if (const auto it = source.find(id); it != source.end()) {
		return to_row(it->second);
	}
	return std::nullopt;
}

auto gse::profile::lookup_report(const id id, const domain domain) -> std::optional<report_entry> {
	const std::uint64_t frames = frames_profiled();

	std::shared_lock lk(state_mutex);
	const auto& source = storage_for(domain);

	if (const auto it = source.find(id); it != source.end()) {
		return to_report(it->second, frames);
	}
	return std::nullopt;
}

auto gse::profile::top_n(const std::size_t n, const domain domain, std::vector<row>& out) -> void {
	std::shared_lock lk(state_mutex);
	const auto& source = storage_for(domain);

	out.clear();
	out.reserve(source.size());

	for (const auto& e : std::views::values(source)) {
		out.push_back(to_row(e));
	}

	const auto keep = std::min(n, out.size());

	std::ranges::partial_sort(
		out,
		out.begin() + static_cast<std::ptrdiff_t>(keep),
		[](const row& a, const row& b) {
			return a.ema > b.ema;
		}
	);

	out.resize(keep);
}

auto gse::profile::set_alpha(const double alpha) -> void {
	ema_alpha.store(std::clamp(alpha, 0.001, 1.0), std::memory_order_relaxed);
}

auto gse::profile::alpha() -> double {
	return ema_alpha.load(std::memory_order_relaxed);
}

auto gse::profile::set_warmup_frames(const std::uint64_t frames) -> void {
	warmup_target.store(frames, std::memory_order_relaxed);
	warmup_remaining.store(frames, std::memory_order_relaxed);
}

auto gse::profile::warmup_frames() -> std::uint64_t {
	return warmup_target.load(std::memory_order_relaxed);
}

auto gse::profile::warming_up() -> bool {
	return warmup_remaining.load(std::memory_order_relaxed) > 0;
}

auto gse::profile::set_enabled(const bool enabled) -> void {
	is_enabled.store(enabled, std::memory_order_relaxed);
}

auto gse::profile::enabled() -> bool {
	return is_enabled.load(std::memory_order_relaxed);
}

auto gse::profile::reset() -> void {
	{
		std::unique_lock lk(state_mutex);
		for (auto& map : entries) {
			map.clear();
		}
		frame_count.store(0, std::memory_order_relaxed);
		warmup_remaining.store(0, std::memory_order_relaxed);
	}

	std::lock_guard lock(recorded_mutex);
	recorded_frames.clear();
	recorded_node_total = 0;
	recorded_tags.clear();
	recorded_tag_indices.clear();
}

auto gse::profile::snapshot_entries(const domain domain, std::vector<entry>& out) -> void {
	std::shared_lock lk(state_mutex);
	const auto& source = storage_for(domain);

	out.clear();
	out.reserve(source.size());

	for (const auto& e : std::views::values(source)) {
		out.push_back(e);
	}
}

auto gse::profile::frames_profiled() -> std::uint64_t {
	return frame_count.load(std::memory_order_relaxed);
}

auto gse::profile::build_report(const domain domain, std::vector<report_entry>& out) -> void {
	const std::uint64_t frames = frames_profiled();

	{
		std::shared_lock lk(state_mutex);
		const auto& source = storage_for(domain);

		out.clear();
		out.reserve(source.size());

		for (const auto& e : std::views::values(source)) {
			out.push_back(to_report(e, frames));
		}
	}

	std::ranges::sort(
		out,
		[](const report_entry& a, const report_entry& b) {
			return a.per_frame > b.per_frame;
		}
	);
}

auto gse::profile::write_section(std::ofstream& out, const std::string_view title, const std::span<const report_entry> rows, const sample_time frame_time) -> void {
	std::size_t tag_width = std::string_view("tag").size();
	for (const auto& r : rows) {
		tag_width = std::max(tag_width, r.id.tag().size());
	}

	const auto header = std::format(
		"{:<{}} {:>13} {:>13} {:>13} {:>10} {:>7} {:>13} {:>7} {:>8} {:>14} {:>9}",
		"tag",
		tag_width,
		"per/f",
		"avg",
		"peak",
		"peak@f",
		"spikes",
		"last",
		"% top",
		"% frame",
		"total",
		"calls/f"
	);

	out << "--- " << title << " ---\n";
	out << header << '\n';
	out << std::string(header.size(), '-') << '\n';

	const auto top = rows.empty() ? sample_time{} : rows.front().per_frame;

	for (const auto& r : rows) {
		const double pct_top = top > sample_time{} ? (r.per_frame / top) * 100.0 : 0.0;
		const double pct_frame = frame_time > sample_time{} ? (r.per_frame / frame_time) * 100.0 : 0.0;
		const auto total = r.ema * static_cast<double>(r.sample_count);

		out << std::format(
			"{:<{}} {:>10.2f:us} {:>10.2f:us} {:>10.2f:us} {:>10} {:>7} {:>10.2f:us} {:>6.1f}% {:>7.1f}% {:>11.2f:ms} {:>9.2f}\n",
			r.id.tag(),
			tag_width,
			r.per_frame,
			r.ema,
			r.peak,
			r.peak_frame,
			r.spike_count,
			r.last,
			pct_top,
			pct_frame,
			total,
			r.calls_per_frame
		);
	}

	out << '\n';
}

auto gse::profile::stale_run(const std::filesystem::directory_entry& entry, const std::string_view prefix) -> bool {
	const std::string name = entry.path().filename().native_encoded_string();
	const std::string_view digits = std::string_view(name).substr(prefix.size());

	std::uint32_t pid = 0;
	if (std::from_chars(digits.data(), digits.data() + digits.size(), pid).ec != std::errc{}) {
		return false;
	}

	if (void* owner = win32::OpenProcess(win32::process_query_limited_information, 0, pid)) {
		win32::CloseHandle(owner);
		return false;
	}

	std::error_code ec;
	return std::filesystem::is_empty(entry.path(), ec);
}

auto gse::profile::prune_runs(const std::filesystem::path& dir, const std::string_view prefix) -> void {
	std::error_code ec;
	std::vector<std::pair<std::filesystem::file_time_type, std::filesystem::path>> existing;

	for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
		if (!entry.is_directory(ec) || !entry.path().filename().native_encoded_string().starts_with(prefix)) {
			continue;
		}
		if (stale_run(entry, prefix)) {
			std::filesystem::remove_all(entry.path(), ec);
			continue;
		}
		existing.emplace_back(entry.last_write_time(ec), entry.path());
	}

	std::ranges::sort(existing, std::ranges::greater{}, [](const auto& row) {
		return row.first;
	});

	for (const auto& stale : existing | std::views::drop(runs_kept - 1) | std::views::values) {
		std::filesystem::remove_all(stale, ec);
	}
}

auto gse::profile::resolve_run_dir() -> std::filesystem::path {
	const std::filesystem::path& root = config::profile_dir();
	const std::string prefix = std::format("{}.", config::executable_stem());
	const std::filesystem::path dir = root / std::format("{}{}", prefix, win32::GetCurrentProcessId());

	std::error_code ec;
	std::filesystem::remove_all(dir, ec);
	prune_runs(root, prefix);
	std::filesystem::create_directories(dir, ec);
	return dir;
}

auto gse::profile::run_dir() -> const std::filesystem::path& {
	static const std::filesystem::path dir = resolve_run_dir();
	return dir;
}

auto gse::profile::latest_run_dir(const std::string_view stem) -> std::filesystem::path {
	const std::string prefix = std::format("{}.", stem);

	std::error_code ec;
	std::filesystem::path newest;
	std::filesystem::file_time_type newest_time;

	for (const auto& entry : std::filesystem::directory_iterator(config::profile_dir(), ec)) {
		if (!entry.is_directory(ec) || !entry.path().filename().native_encoded_string().starts_with(prefix)) {
			continue;
		}
		const std::filesystem::file_time_type stamp = entry.last_write_time(ec);
		if (newest.empty() || stamp > newest_time) {
			newest = entry.path();
			newest_time = stamp;
		}
	}

	return newest;
}

auto gse::profile::dump(const std::filesystem::path& path) -> void {
	std::filesystem::create_directories(path.parent_path());

	std::ofstream out(path);
	if (!out.is_open()) {
		return;
	}

	const auto fps = system_clock::fps();
	const auto frame_time = fps > 0 ? milliseconds(1000.0 / static_cast<double>(fps)) : sample_time{};
	const auto frames = frames_profiled();
	const auto main_tid = trace::main_tid();

	std::vector<entry> cpu_src;
	snapshot_entries(domain::cpu, cpu_src);

	std::vector<entry> worker_src;
	worker_src.reserve(cpu_src.size());
	for (auto& e : cpu_src) {
		if (!on_main_thread(dominant_tid_of(e), main_tid)) {
			worker_src.push_back(std::move(e));
		}
	}

	std::vector<report_entry> cpu_rows;
	std::vector<report_entry> gpu_rows;
	build_report(domain::cpu, cpu_rows);
	build_report(domain::gpu, gpu_rows);

	const auto workers = std::ranges::stable_partition(cpu_rows, [main_tid](const report_entry& r) {
		return on_main_thread(r.dominant_tid, main_tid);
	});
	const std::span<const report_entry> main_rows(cpu_rows.begin(), workers.begin());
	const std::span<const report_entry> worker_rows(workers.begin(), cpu_rows.end());

	const auto cpu_top = main_rows.empty() ? sample_time{} : main_rows.front().per_frame;
	const auto gpu_top = gpu_rows.empty() ? sample_time{} : gpu_rows.front().per_frame;

	out << std::format("=== Profile dump ({}) ===\n", system_clock::timestamp_filename());
	out << std::format(
		"frame: {:.2f:ms} ({} fps)    main-thread top: {:.2f:ms}    GPU top: {:.2f:ms}    {} frames profiled    EMA "
		"alpha: {:.3f}\n",
		frame_time,
		fps,
		cpu_top,
		gpu_top,
		frames,
		alpha()
	);
	out << std::format(
		"dropped trace events: {}    abandoned spans: {}    warmup frames discarded: {}{}\n",
		trace::dropped_events(),
		trace::abandoned_spans(),
		warmup_frames(),
		warming_up() ? "  (STILL WARMING UP - rows below are empty or partial)" : ""
	);
	out << "sorted by per/f = avg * calls/f (real per-frame cost).  % top = per/f relative to top row.  % frame = "
		   "per/f / frame_time.  Worker rows can sum > 100% (parallel).\n";
	out << std::format(
		"peak@f = frame index the peak was set on (0 = first counted frame, warmup excluded).  spikes = samples over "
		"{:.0f}x the running avg.  A large peak with spikes <= 1 is a one-off, not a workload.  GPU rows attribute to "
		"the frame that ingested the sample, which trails the frame that produced it.\n\n",
		spike_ratio
	);

	write_section(out, "CPU - Main Thread (sequential, blocks the frame)", main_rows, frame_time);
	write_section(out, "CPU - Workers (parallel; sums can exceed 100%)", worker_rows, frame_time);
	write_section(out, "GPU (per-pass time)", gpu_rows, frame_time);

	write_thread_breakdown(out, worker_src);
	write_dag(out);
}

auto gse::profile::write_thread_breakdown(std::ofstream& out, const std::span<const entry> worker_src) -> void {
	const auto frames = frame_count.load(std::memory_order_relaxed);
	const auto main_tid = trace::main_tid();

	std::vector<const entry*> sorted;
	sorted.reserve(worker_src.size());
	for (const auto& e : worker_src) {
		if (!e.samples_by_tid.empty()) {
			sorted.push_back(&e);
		}
	}

	std::ranges::sort(
		sorted,
		[](const entry* a, const entry* b) {
			return a->ema * static_cast<double>(sample_count_of(*a)) > b->ema * static_cast<double>(sample_count_of(*b));
		}
	);

	std::size_t tag_width = std::string_view("tag").size();
	for (const auto* e : sorted) {
		tag_width = std::max(tag_width, e->id.tag().size());
	}

	const auto header = std::format("{:<{}}  {:>10}  {:>10}  {:>40}", "tag", tag_width, "main/f", "worker/f", "per-tid /f (top 6)");

	out << "--- Worker tag thread breakdown (where each tag actually ran) ---\n";
	out << std::format("main tid = {}.  Counts shown are samples per frame on each thread.\n\n", main_tid);
	out << header << '\n';
	out << std::string(header.size(), '-') << '\n';

	std::vector<std::pair<std::uint32_t, std::uint64_t>> tids;
	const std::size_t max_to_show = std::min(breakdown_row_limit, sorted.size());

	for (std::size_t i = 0; i < max_to_show; ++i) {
		const auto& e = *sorted[i];

		double main_per_frame = 0.0;
		double worker_per_frame = 0.0;

		tids.clear();
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

		std::ranges::sort(
			tids,
			[](const auto& a, const auto& b) {
				return a.second > b.second;
			}
		);

		std::string tid_breakdown;
		const std::size_t tid_show = std::min(breakdown_tid_limit, tids.size());

		for (std::size_t j = 0; j < tid_show; ++j) {
			const double per = frames > 0 ? static_cast<double>(tids[j].second) / static_cast<double>(frames) : 0.0;
			if (j > 0) {
				tid_breakdown += " ";
			}
			tid_breakdown += std::format("t{}:{:.2f}", tids[j].first, per);
		}

		out << std::format(
			"{:<{}}  {:>10.2f}  {:>10.2f}  {:<40}\n",
			e.id.tag(),
			tag_width,
			main_per_frame,
			worker_per_frame,
			tid_breakdown
		);
	}

	out << '\n';
}

auto gse::profile::flatten_dag(const trace::frame_view& fv, std::vector<dag_visit>& out) -> void {
	out.clear();

	std::vector<std::uint32_t> ordered_roots(fv.roots.begin(), fv.roots.end());
	std::ranges::sort(
		ordered_roots,
		[&fv](const std::uint32_t a, const std::uint32_t b) {
			return fv.nodes[a].start < fv.nodes[b].start;
		}
	);

	std::vector<dag_visit> stack;
	stack.reserve(fv.nodes.size());

	for (const auto r : std::views::reverse(ordered_roots)) {
		stack.push_back({
			.index = r,
			.depth = 0
		});
	}

	while (!stack.empty()) {
		const auto visit = stack.back();
		stack.pop_back();
		out.push_back(visit);

		if (visit.depth >= max_dag_depth) {
			continue;
		}

		for (const auto ci : std::views::reverse(fv.child_indices(fv.nodes[visit.index]))) {
			stack.push_back({
				.index = ci,
				.depth = visit.depth + 1
			});
		}
	}
}

auto gse::profile::write_dag(std::ofstream& out) -> void {
	const auto fv = trace::view();
	if (fv.roots.empty()) {
		return;
	}

	auto tmin = fv.nodes[fv.roots.front()].start;
	auto tmax = fv.nodes[fv.roots.front()].stop;

	for (const auto r : fv.roots) {
		const auto& n = fv.nodes[r];
		if (n.start < tmin) {
			tmin = n.start;
		}
		if (n.stop > tmax) {
			tmax = n.stop;
		}
	}

	if (tmax <= tmin) {
		return;
	}

	std::vector<dag_visit> visits;
	flatten_dag(fv, visits);

	const auto total_range = sample_time(tmax - tmin);
	const sample_time min_span = microseconds(5.0);
	const auto main_tid = trace::main_tid();

	std::size_t name_width = 0;
	for (const auto& v : visits) {
		name_width = std::max(name_width, static_cast<std::size_t>(v.depth) * 2 + fv.nodes[v.index].id.tag().size());
	}

	out << "--- Frame DAG (absolute timeline; bars at same column = parallel) ---\n";
	out << std::format(
		"total range: {:.2f:us} across {} columns ({:.2f:us} per column).  '#' = main thread, '=' = worker.\n\n",
		total_range,
		dag_bar_width,
		total_range / static_cast<double>(dag_bar_width)
	);

	std::string bar;

	for (const auto& v : visits) {
		const auto& n = fv.nodes[v.index];
		if (n.stop <= n.start) {
			continue;
		}

		const auto duration = sample_time(n.stop - n.start);
		if (duration < min_span) {
			continue;
		}

		const auto offset = sample_time(n.start - tmin);

		std::size_t col_start = static_cast<std::size_t>(offset / total_range * static_cast<double>(dag_bar_width));
		std::size_t col_end = static_cast<std::size_t>((offset + duration) / total_range * static_cast<double>(dag_bar_width));
		col_start = std::min(col_start, dag_bar_width - 1);
		col_end = std::min(col_end, dag_bar_width);
		if (col_end <= col_start) {
			col_end = col_start + 1;
		}

		const char fill = (main_tid != 0 && n.trace_id != main_tid) ? '=' : '#';
		bar.assign(dag_bar_width, ' ');
		for (std::size_t i = col_start; i < col_end; ++i) {
			bar[i] = fill;
		}

		const std::string label = std::format("{}{}", std::string(static_cast<std::size_t>(v.depth) * 2, ' '), n.id.tag());
		out << std::format("|{}| {:>10.2f:us}  tid:{:<3}  {:<{}}\n", bar, duration, n.trace_id, label, name_width);
	}
}

auto gse::profile::escape_json(const std::string_view s) -> std::string {
	std::string r;
	r.reserve(s.size() + 2);

	for (const char c : s) {
		switch (c) {
			case '"':
				r += "\\\"";
				break;
			case '\\':
				r += "\\\\";
				break;
			case '\n':
				r += "\\n";
				break;
			case '\r':
				r += "\\r";
				break;
			case '\t':
				r += "\\t";
				break;
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
}

auto gse::profile::worst_recorded_frames(const std::size_t count) -> std::vector<report_frame> {
	std::vector<report_frame> frames;
	{
		std::lock_guard lock(recorded_mutex);
		frames.assign(recorded_frames.begin(), recorded_frames.end());
	}

	std::ranges::sort(frames, [](const report_frame& a, const report_frame& b) {
		return a.span > b.span;
	});
	if (frames.size() > count) {
		frames.resize(count);
	}
	return frames;
}

auto gse::profile::dump_chrome_trace(const std::filesystem::path& path) -> void {
	std::filesystem::create_directories(path.parent_path());

	std::ofstream out(path);
	if (!out.is_open()) {
		return;
	}

	const auto frames = worst_recorded_frames(chrome_trace_frames);

	std::unordered_set<std::uint32_t> tids;
	bool first = true;

	out << "[\n";

	std::vector<std::string> tags;
	{
		std::lock_guard lock(recorded_mutex);
		tags = recorded_tags;
	}

	sample_time cursor{};
	const sample_time frame_gap = milliseconds(1.0);

	for (std::size_t f = 0; f < frames.size(); ++f) {
		const auto& frame = frames[f];

		if (!first) {
			out << ",\n";
		}
		first = false;

		out << std::format(
			R"({{"ph":"X","name":"frame {} - {:.2f:ms}","cat":"perf","ts":{:.3f},"dur":{:.3f},"pid":1,"tid":0}})",
			f,
			frame.span,
			cursor.as<microseconds>(),
			frame.span.as<microseconds>()
		);

		for (const auto& n : frame.nodes) {
			if (n.open || n.stop <= n.start || n.tag >= tags.size()) {
				continue;
			}

			const sample_time ts = cursor + sample_time(n.start - frame.origin);
			const sample_time dur = sample_time(n.stop - n.start);
			tids.insert(n.trace_id);

			out << std::format(
				",\n" R"({{"ph":"X","name":"{}","cat":"perf","ts":{:.3f},"dur":{:.3f},"pid":1,"tid":{}}})",
				escape_json(tags[n.tag]),
				ts.as<microseconds>(),
				dur.as<microseconds>(),
				n.trace_id
			);
		}

		cursor += frame.span + frame_gap;
	}

	if (frames.empty()) {
		const auto fv = trace::view();
		if (fv.roots.empty()) {
			out << "]\n";
			return;
		}

		auto tmin = fv.nodes[fv.roots.front()].start;
		for (const auto r : fv.roots) {
			if (fv.nodes[r].start < tmin) {
				tmin = fv.nodes[r].start;
			}
		}

		const auto hidden = trace::hidden_ids_snapshot();

		for (const auto& n : fv.nodes) {
			if (n.open || n.stop <= n.start || hidden.contains(n.id)) {
				continue;
			}

			const double ts_us = sample_time(n.start - tmin).as<microseconds>();
			const double dur_us = sample_time(n.stop - n.start).as<microseconds>();
			tids.insert(n.trace_id);

			if (!first) {
				out << ",\n";
			}
			first = false;

			out << std::format(
				R"({{"ph":"X","name":"{}","cat":"perf","ts":{:.3f},"dur":{:.3f},"pid":1,"tid":{}}})",
				escape_json(n.id.tag()),
				ts_us,
				dur_us,
				n.trace_id
			);
		}
	}

	const auto main_tid = trace::main_tid();

	if (!frames.empty()) {
		tids.insert(0);
	}

	for (const auto tid : tids) {
		std::string name;
		if (tid == 0) {
			name = "frames";
		}
		else if (tid == main_tid) {
			name = "main";
		}
		else if (const auto vname = trace::virtual_thread_name(tid)) {
			name = *vname;
		}
		else {
			name = std::format("worker {}", tid);
		}

		if (!first) {
			out << ",\n";
		}
		first = false;

		out << std::format(
			R"({{"ph":"M","name":"thread_name","pid":1,"tid":{},"args":{{"name":"{}"}}}})",
			tid,
			escape_json(name)
		);
	}

	out << "\n]\n";
}

auto gse::profile::fill_records(const domain domain, std::vector<report_record>& out) -> void {
	std::vector<report_entry> rows;
	build_report(domain, rows);

	out.clear();
	out.reserve(rows.size());
	for (const report_entry& row : rows) {
		out.push_back({
			.tag = std::string(row.id.tag()),
			.per_frame = row.per_frame,
			.ema = row.ema,
			.last = row.last,
			.peak = row.peak,
			.calls_per_frame = row.calls_per_frame,
			.sample_count = row.sample_count,
			.peak_frame = row.peak_frame,
			.spike_count = row.spike_count,
			.dominant_tid = row.dominant_tid,
		});
	}
}

auto gse::profile::set_recording_capacity(const std::size_t frames) -> void {
	max_recorded_frames.store(std::max<std::size_t>(frames, 1), std::memory_order_relaxed);
}

auto gse::profile::recording_capacity() -> std::size_t {
	return max_recorded_frames.load(std::memory_order_relaxed);
}

auto gse::profile::build_report_file() -> report_file {
	report_file file;
	fill_records(domain::cpu, file.cpu);
	fill_records(domain::gpu, file.gpu);

	{
		std::lock_guard lock(recorded_mutex);
		file.tags = recorded_tags;
		file.recorded.assign(recorded_frames.begin(), recorded_frames.end());
	}

	const auto fps = system_clock::fps();
	file.frame_time = fps > 0 ? milliseconds(1000.0 / static_cast<double>(fps)) : sample_time{};
	file.frames = frames_profiled();
	file.main_tid = trace::main_tid();
	return file;
}

auto gse::profile::dump_report(const std::filesystem::path& path) -> void {
	std::filesystem::create_directories(path.parent_path());

	std::ofstream out(path, std::ios::binary);
	if (!out.is_open()) {
		return;
	}

	const report_file file = build_report_file();

	binary_writer writer(out, report_magic, report_version);
	writer & file;
}

auto gse::profile::load_report(const std::filesystem::path& path) -> std::expected<report_file, archive_mismatch> {
	std::ifstream in(path, std::ios::binary);
	if (!in.is_open()) {
		return std::unexpected(archive_mismatch{});
	}

	std::expected<binary_reader, archive_mismatch> opened = binary_reader::open(in, report_magic, report_version);
	if (!opened) {
		return std::unexpected(opened.error());
	}

	report_file file;
	*opened & file;
	if (!opened->valid()) {
		return std::unexpected(archive_mismatch{ .readable = true });
	}
	return file;
}

export module gse.utility:profile_aggregator;

import std;

import gse.config;
import gse.math;

import :id;
import :flat_map;
import :trace;
import :system_clock;

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
}

namespace gse::profile {
	std::shared_mutex state_mutex;
	flat_map<id, entry> cpu_entries;
	flat_map<id, entry> gpu_entries;
	std::atomic ema_alpha{ 0.1 };
	std::atomic is_enabled{ true };
	std::atomic<std::uint32_t> main_thread_id{ 0 };
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
		bool pooled
	) -> void;
}

auto gse::profile::update_entry(flat_map<id, entry>& map, const id id, const sample_time duration, const std::uint32_t thread_id, const bool pooled) -> void {
	auto& [e_id, ema, last, peak, sample_count, e_thread, e_pooled] = map[id];
	if (sample_count == 0) {
		e_id = id;
		ema = duration;
	}
	else {
		const double a = ema_alpha.load(std::memory_order_relaxed);
		ema = a * duration + (1.0 - a) * ema;
	}
	last = duration;
	if (duration > peak) {
		peak = duration;
	}
	e_thread = thread_id;
	e_pooled = e_pooled || pooled;
	++sample_count;
}

auto gse::profile::walk_node(const trace::node& n, flat_map<id, entry>& cpu_agg, const bool pooled) -> void {
	if (!trace::is_hidden(n.id)) {
		update_entry(cpu_agg, generate_temp_id(n.id), sample_time(n.self), n.trace_id, pooled);
	}
	const bool child_pooled = pooled || trace::is_pool_root(n.id);
	for (std::size_t i = 0; i < n.children_count; ++i) {
		walk_node(n.children_first[i], cpu_agg, child_pooled);
	}
}

auto gse::profile::ingest_frame() -> void {
	if (!is_enabled.load(std::memory_order_relaxed)) {
		return;
	}

	const auto [roots, storage] = trace::view();

	std::unique_lock lk(state_mutex);
	if (!roots.empty()) {
		main_thread_id.store(roots.front().trace_id, std::memory_order_relaxed);
	}
	frame_count.fetch_add(1, std::memory_order_relaxed);
	for (const auto& root : roots) {
		walk_node(root, cpu_entries, false);
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

	const auto write_section = [&out, frame_time, frames](const std::string_view title, const std::vector<entry>& rows) {
		out << "--- " << title << " ---\n";
		out << std::format(
			"{:<48} {:>13} {:>13} {:>13} {:>7} {:>8} {:>14} {:>9}\n",
			"tag", "avg", "peak", "last", "% top", "% frame", "total", "calls/f"
		);
		out << std::string(48 + 13 + 13 + 13 + 7 + 8 + 14 + 9 + 7, '-') << '\n';

		const auto top = rows.empty() ? sample_time{} : rows.front().ema;

		for (const auto& [e_id, ema, last, peak, sample_count, thread_id, pooled] : rows) {
			const double pct_top = top > sample_time{} ? (ema / top) * 100.0 : 0.0;
			const double calls_per_frame = frames > 0 ? static_cast<double>(sample_count) / static_cast<double>(frames) : 0.0;
			const auto per_frame_time = ema * calls_per_frame;
			const double pct_frame = frame_time > sample_time{} ? (per_frame_time / frame_time) * 100.0 : 0.0;
			const auto total = ema * static_cast<double>(sample_count);

			out << std::format(
				"{:<48} {:>10.2f} {:>10.2f} {:>10.2f} {:>6.1f}% {:>7.1f}% {:>11.2f} {:>9.2f}\n",
				e_id.tag(),
				gse::in<microseconds>(ema),
				gse::in<microseconds>(peak),
				gse::in<microseconds>(last),
				pct_top,
				pct_frame,
				gse::in<milliseconds>(total),
				calls_per_frame
			);
		}
		out << '\n';
	};

	std::vector<entry> main_rows;
	std::vector<entry> worker_rows;
	std::vector<entry> gpu_rows;
	{
		std::shared_lock lk(state_mutex);
		main_rows.reserve(cpu_entries.size());
		worker_rows.reserve(cpu_entries.size());
		for (const auto& e : cpu_entries | std::views::values) {
			(e.pooled ? worker_rows : main_rows).push_back(e);
		}
		gpu_rows.reserve(gpu_entries.size());
		for (const auto& e : gpu_entries | std::views::values) {
			gpu_rows.push_back(e);
		}
	}

	const auto sort_by_ema = [](std::vector<entry>& v) {
		std::ranges::sort(v, [](const entry& a, const entry& b) { return a.ema > b.ema; });
	};
	sort_by_ema(main_rows);
	sort_by_ema(worker_rows);
	sort_by_ema(gpu_rows);

	const auto cpu_root_ms = main_rows.empty() ? sample_time{} : main_rows.front().ema;
	const auto gpu_root_ms = gpu_rows.empty() ? sample_time{} : gpu_rows.front().ema;

	out << std::format("=== Profile dump ({}) ===\n", system_clock::timestamp_filename());
	out << std::format(
		"frame: {:.2f} ({} fps)    main-thread top: {:.2f}    GPU top: {:.2f}    {} frames profiled    EMA alpha: {:.3f}\n",
		gse::in<milliseconds>(frame_time),
		fps,
		gse::in<milliseconds>(cpu_root_ms),
		gse::in<milliseconds>(gpu_root_ms),
		frames,
		alpha()
	);
	out << "% top = relative to slowest entry in section.  % frame = (avg * calls/f) / frame_time.  Worker rows can sum > 100% (parallel).\n\n";

	write_section("CPU - Main Thread (sequential, blocks the frame)", main_rows);
	write_section("CPU - Workers (parallel; sums can exceed 100%)", worker_rows);
	write_section("GPU (per-pass time)", gpu_rows);
}

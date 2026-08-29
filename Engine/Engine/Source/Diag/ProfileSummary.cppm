export module gse.diag:profile_summary;

import std;

import gse.core;
import gse.math;
import gse.time;

import :profile_aggregator;

export namespace gse::profile {
	struct tag_percentiles {
		id id;
		sample_time p50;
		sample_time p95;
		sample_time p99;
		sample_time peak;
		std::uint64_t frames_present = 0;
	};

	struct run_summary {
		std::vector<tag_percentiles> rows;
		sample_time frame_p50;
		sample_time frame_p95;
		sample_time frame_p99;
		sample_time frame_peak;
		std::uint64_t frames = 0;
		bool truncated = false;
	};

	struct tag_delta {
		id id;
		sample_time baseline_p50;
		sample_time current_p50;
		double ratio = 0.0;
		bool in_baseline = false;
		bool in_current = false;
	};

	struct run_diff {
		std::vector<tag_delta> rows;
		sample_time baseline_frame_p50;
		sample_time current_frame_p50;
		sample_time baseline_frame_p95;
		sample_time current_frame_p95;
		double frame_p50_ratio = 0.0;
		double frame_p95_ratio = 0.0;
		std::uint64_t baseline_frames = 0;
		std::uint64_t current_frames = 0;
		double threshold = 0.0;
		bool regressed = false;
	};

	auto summarize(
		const report_file& file
	) -> run_summary;

	auto write_summary(
		const run_summary& summary,
		const std::filesystem::path& path
	) -> void;

	auto diff_summaries(
		const run_summary& baseline,
		const run_summary& current,
		double threshold
	) -> run_diff;

	auto write_diff(
		const run_diff& diff,
		const std::filesystem::path& path
	) -> void;
}

namespace gse::profile {
	constexpr std::size_t summary_row_limit = 40;

	auto ratio_of(
		sample_time baseline,
		sample_time current
	) -> double;

	auto percentile_of(
		std::span<const sample_time> sorted,
		double fraction
	) -> sample_time;

	auto accumulate_frame(
		const report_frame& frame,
		std::vector<sample_time>& scratch,
		std::vector<std::vector<sample_time>>& by_tag
	) -> void;

	auto frame_duration(
		const report_frame& frame
	) -> sample_time;
}

auto gse::profile::frame_duration(const report_frame& frame) -> sample_time {
	time_t<std::uint64_t> first{};
	time_t<std::uint64_t> last{};
	bool any = false;

	for (const auto root : frame.roots) {
		if (root >= frame.nodes.size()) {
			continue;
		}
		const report_node& node = frame.nodes[root];
		if (node.open) {
			continue;
		}
		if (!any) {
			first = node.start;
			last = node.stop;
			any = true;
			continue;
		}
		if (node.start < first) {
			first = node.start;
		}
		if (node.stop > last) {
			last = node.stop;
		}
	}

	return any ? sample_time(last - first) : sample_time{};
}

auto gse::profile::percentile_of(const std::span<const sample_time> sorted, const double fraction) -> sample_time {
	if (sorted.empty()) {
		return {};
	}
	const auto span = static_cast<double>(sorted.size() - 1);
	const auto index = static_cast<std::size_t>(fraction * span + 0.5);
	return sorted[std::min(index, sorted.size() - 1)];
}

auto gse::profile::accumulate_frame(const report_frame& frame, std::vector<sample_time>& scratch, std::vector<std::vector<sample_time>>& by_tag) -> void {
	std::ranges::fill(scratch, sample_time{});

	for (const report_node& node : frame.nodes) {
		if (node.open || node.tag >= scratch.size()) {
			continue;
		}
		scratch[node.tag] += sample_time(node.self);
	}

	for (std::size_t tag = 0; tag < scratch.size(); ++tag) {
		if (scratch[tag] > sample_time{}) {
			by_tag[tag].push_back(scratch[tag]);
		}
	}
}

auto gse::profile::summarize(const report_file& file) -> run_summary {
	const std::uint64_t frames = file.recorded.size();
	const bool truncated = file.frames > file.recorded.size();

	if (file.recorded.empty()) {
		return {
			.frames = frames,
			.truncated = truncated,
		};
	}

	std::vector<sample_time> scratch(file.tags.size());
	std::vector<std::vector<sample_time>> by_tag(file.tags.size());
	std::vector<sample_time> frame_spans;
	frame_spans.reserve(file.recorded.size());

	for (const report_frame& frame : file.recorded) {
		accumulate_frame(frame, scratch, by_tag);
		frame_spans.push_back(frame_duration(frame));
	}

	std::ranges::sort(frame_spans);

	std::vector<tag_percentiles> rows;
	rows.reserve(file.tags.size());
	for (std::size_t tag = 0; tag < file.tags.size(); ++tag) {
		std::vector<sample_time>& samples = by_tag[tag];
		if (samples.empty()) {
			continue;
		}
		std::ranges::sort(samples);
		rows.push_back({
			.id = find_or_generate_id(file.tags[tag]),
			.p50 = percentile_of(samples, 0.50),
			.p95 = percentile_of(samples, 0.95),
			.p99 = percentile_of(samples, 0.99),
			.peak = samples.back(),
			.frames_present = samples.size(),
		});
	}

	std::ranges::sort(
		rows,
		[](const tag_percentiles& a, const tag_percentiles& b) {
			return a.p50 > b.p50;
		}
	);

	return {
		.rows = std::move(rows),
		.frame_p50 = percentile_of(frame_spans, 0.50),
		.frame_p95 = percentile_of(frame_spans, 0.95),
		.frame_p99 = percentile_of(frame_spans, 0.99),
		.frame_peak = frame_spans.back(),
		.frames = frames,
		.truncated = truncated,
	};
}

auto gse::profile::ratio_of(const sample_time baseline, const sample_time current) -> double {
	return baseline > sample_time{} ? current / baseline : 0.0;
}

auto gse::profile::diff_summaries(const run_summary& baseline, const run_summary& current, const double threshold) -> run_diff {
	std::unordered_map<id, const tag_percentiles*> by_id;
	by_id.reserve(baseline.rows.size());
	for (const tag_percentiles& row : baseline.rows) {
		by_id.emplace(row.id, &row);
	}

	std::vector<tag_delta> rows;
	rows.reserve(current.rows.size() + baseline.rows.size());

	for (const tag_percentiles& row : current.rows) {
		const auto it = by_id.find(row.id);
		const bool present = it != by_id.end();
		rows.push_back({
			.id = row.id,
			.baseline_p50 = present ? it->second->p50 : sample_time{},
			.current_p50 = row.p50,
			.ratio = present ? ratio_of(it->second->p50, row.p50) : 0.0,
			.in_baseline = present,
			.in_current = true,
		});
		if (present) {
			by_id.erase(it);
		}
	}

	for (const tag_percentiles* row : std::views::values(by_id)) {
		rows.push_back({
			.id = row->id,
			.baseline_p50 = row->p50,
			.current_p50 = sample_time{},
			.ratio = 0.0,
			.in_baseline = true,
			.in_current = false,
		});
	}

	std::ranges::sort(
		rows,
		[](const tag_delta& a, const tag_delta& b) {
			return a.current_p50 - a.baseline_p50 > b.current_p50 - b.baseline_p50;
		}
	);

	const double frame_p50_ratio = ratio_of(baseline.frame_p50, current.frame_p50);

	return {
		.rows = std::move(rows),
		.baseline_frame_p50 = baseline.frame_p50,
		.current_frame_p50 = current.frame_p50,
		.baseline_frame_p95 = baseline.frame_p95,
		.current_frame_p95 = current.frame_p95,
		.frame_p50_ratio = frame_p50_ratio,
		.frame_p95_ratio = ratio_of(baseline.frame_p95, current.frame_p95),
		.baseline_frames = baseline.frames,
		.current_frames = current.frames,
		.threshold = threshold,
		.regressed = frame_p50_ratio > threshold,
	};
}

auto gse::profile::write_summary(const run_summary& summary, const std::filesystem::path& path) -> void {
	std::filesystem::create_directories(path.parent_path());

	std::ofstream out(path);
	if (!out.is_open()) {
		return;
	}

	out << std::format("=== Bench summary ({}) ===\n", system_clock::timestamp_filename());
	out << std::format(
		"frames measured: {}    p50 {:.3f:ms}    p95 {:.3f:ms}    p99 {:.3f:ms}    peak {:.3f:ms}\n",
		summary.frames,
		summary.frame_p50,
		summary.frame_p95,
		summary.frame_p99,
		summary.frame_peak
	);

	if (summary.frames == 0) {
		out << "WARNING: no frames were recorded, so every percentile above is zero by absence rather than by measurement.\n";
	}
	else if (summary.truncated) {
		out << "WARNING: the recorded frame ring evicted frames during this run; percentiles cover only the tail.\n";
	}

	out << "\nper-tag self time, one sample per frame, sorted by p50.\n\n";

	std::size_t tag_width = std::string_view("tag").size();
	for (const tag_percentiles& row : summary.rows | std::views::take(summary_row_limit)) {
		tag_width = std::max(tag_width, row.id.tag().size());
	}

	out << std::format("{:<{}}  {:>13}  {:>13}  {:>13}  {:>13}  {:>8}\n", "tag", tag_width, "p50 us", "p95 us", "p99 us", "peak us", "frames");

	for (const tag_percentiles& row : summary.rows | std::views::take(summary_row_limit)) {
		out << std::format(
			"{:<{}}  {:>10.2f:us}  {:>10.2f:us}  {:>10.2f:us}  {:>10.2f:us}  {:>8}\n",
			row.id.tag(),
			tag_width,
			row.p50,
			row.p95,
			row.p99,
			row.peak,
			row.frames_present
		);
	}

	if (summary.rows.size() > summary_row_limit) {
		out << std::format("\n({} further tags omitted)\n", summary.rows.size() - summary_row_limit);
	}

	std::vector<row> gpu_rows;
	top_n(summary_row_limit * 2, domain::gpu, gpu_rows);
	std::ranges::sort(gpu_rows, std::greater{}, &row::ema);

	if (!gpu_rows.empty()) {
		out << "\nper-pass gpu time from timestamp queries, ema over the run, sorted by ema.\n\n";

		std::size_t pass_width = std::string_view("pass").size();
		for (const row& r : gpu_rows) {
			pass_width = std::max(pass_width, r.id.tag().size());
		}

		out << std::format("{:<{}}  {:>13}  {:>13}  {:>8}\n", "pass", pass_width, "ema us", "peak us", "samples");

		for (const row& r : gpu_rows) {
			out << std::format(
				"{:<{}}  {:>10.2f:us}  {:>10.2f:us}  {:>8}\n",
				r.id.tag(),
				pass_width,
				r.ema,
				r.peak,
				r.sample_count
			);
		}
	}
}

auto gse::profile::write_diff(const run_diff& diff, const std::filesystem::path& path) -> void {
	std::filesystem::create_directories(path.parent_path());

	std::ofstream out(path);
	if (!out.is_open()) {
		return;
	}

	out << std::format("=== Bench diff ({}) ===\n", system_clock::timestamp_filename());
	out << std::format("frames: baseline {}, current {}\n", diff.baseline_frames, diff.current_frames);
	out << std::format(
		"frame p50 {:.3f:ms} -> {:.3f:ms}  ({:.3f}x)\n",
		diff.baseline_frame_p50,
		diff.current_frame_p50,
		diff.frame_p50_ratio
	);
	out << std::format(
		"frame p95 {:.3f:ms} -> {:.3f:ms}  ({:.3f}x)\n",
		diff.baseline_frame_p95,
		diff.current_frame_p95,
		diff.frame_p95_ratio
	);
	out << std::format(
		"verdict: {} against a {:.3f}x threshold on frame p50\n",
		diff.regressed ? "REGRESSED" : "within threshold",
		diff.threshold
	);

	out << "\nper-tag p50, joined on tag id, sorted by absolute change.\n\n";

	std::size_t tag_width = std::string_view("tag").size();
	for (const tag_delta& row : diff.rows | std::views::take(summary_row_limit)) {
		tag_width = std::max(tag_width, row.id.tag().size());
	}

	out << std::format("{:<{}}  {:>13}  {:>13}  {:>10}  {:>8}\n", "tag", tag_width, "baseline us", "current us", "ratio", "state");

	for (const tag_delta& row : diff.rows | std::views::take(summary_row_limit)) {
		const std::string_view state = !row.in_baseline ? "added" : !row.in_current ? "removed" : "";
		out << std::format(
			"{:<{}}  {:>10.2f:us}  {:>10.2f:us}  {:>10.3f}  {:>8}\n",
			row.id.tag(),
			tag_width,
			row.baseline_p50,
			row.current_p50,
			row.ratio,
			state
		);
	}

	if (diff.rows.size() > summary_row_limit) {
		out << std::format("\n({} further tags omitted)\n", diff.rows.size() - summary_row_limit);
	}
}

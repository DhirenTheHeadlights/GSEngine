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

	auto summarize(
		const report_file& file
	) -> run_summary;

	auto write_summary(
		const run_summary& summary,
		const std::filesystem::path& path
	) -> void;
}

namespace gse::profile {
	constexpr std::size_t summary_row_limit = 40;

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
	run_summary summary;
	summary.frames = file.recorded.size();
	summary.truncated = file.frames > file.recorded.size();

	if (file.recorded.empty()) {
		return summary;
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
	summary.frame_p50 = percentile_of(frame_spans, 0.50);
	summary.frame_p95 = percentile_of(frame_spans, 0.95);
	summary.frame_p99 = percentile_of(frame_spans, 0.99);
	summary.frame_peak = frame_spans.back();

	summary.rows.reserve(file.tags.size());
	for (std::size_t tag = 0; tag < file.tags.size(); ++tag) {
		std::vector<sample_time>& samples = by_tag[tag];
		if (samples.empty()) {
			continue;
		}
		std::ranges::sort(samples);
		summary.rows.push_back({
			.id = find_or_generate_id(file.tags[tag]),
			.p50 = percentile_of(samples, 0.50),
			.p95 = percentile_of(samples, 0.95),
			.p99 = percentile_of(samples, 0.99),
			.peak = samples.back(),
			.frames_present = samples.size(),
		});
	}

	std::ranges::sort(
		summary.rows,
		[](const tag_percentiles& a, const tag_percentiles& b) {
			return a.p50 > b.p50;
		}
	);

	return summary;
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

	if (summary.truncated) {
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
			"{:<{}}  {:>13.2f:us}  {:>13.2f:us}  {:>13.2f:us}  {:>13.2f:us}  {:>8}\n",
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
}

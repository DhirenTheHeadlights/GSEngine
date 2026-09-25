export module gse.diag:frame_analysis;

import std;

import gse.core;
import gse.math;
import gse.meta;
import gse.time;

import :profile_aggregator;
import :profile_summary;
import :trace;

export namespace gse::profile {
	enum struct path_step : std::uint8_t {
		work,
		wait,
		wake,
		untraced
	};

	constexpr std::uint32_t untagged = std::numeric_limits<std::uint32_t>::max();

	struct path_row {
		std::uint32_t tag = untagged;
		path_step step = path_step::work;
		sample_time mean;
		sample_time p50;
		sample_time p95;
	};

	struct stage_row {
		std::uint32_t tag = untagged;
		sample_time wall_mean;
		sample_time wall_p50;
		sample_time wall_p95;
		sample_time self_mean;
	};

	struct thread_row {
		std::uint32_t tid = 0;
		sample_time busy_mean;
	};

	struct frame_analysis {
		std::vector<path_row> path;
		std::vector<stage_row> stages;
		std::vector<thread_row> threads;
		std::vector<std::uint32_t> dag_path;
		std::size_t dag_frame = 0;
		sample_time frame_mean;
		std::uint64_t frames = 0;
	};

	auto analyze_frames(
		const report_file& file
	) -> frame_analysis;

	auto write_path_verdict(
		std::ofstream& out,
		const frame_analysis& analysis
	) -> void;

	auto write_frame_analysis(
		std::ofstream& out,
		const report_file& file,
		const frame_analysis& analysis
	) -> void;
}

namespace gse::profile {
	constexpr std::uint32_t no_owner = std::numeric_limits<std::uint32_t>::max();
	constexpr std::size_t analysis_row_limit = 30;

	struct interval {
		trace::tick_step start;
		trace::tick_step stop;
	};

	struct timeline_segment {
		trace::tick_step start;
		trace::tick_step stop;
		std::uint32_t owner = no_owner;
	};

	struct thread_timeline {
		std::uint32_t tid = 0;
		std::vector<timeline_segment> segments;
	};

	struct path_key {
		std::uint32_t tag = untagged;
		path_step step = path_step::work;

		auto operator<=>(
			const path_key&
		) const = default;
	};

	struct path_segment {
		path_key key;
		sample_time duration;
		std::uint32_t node = no_owner;
	};

	struct frame_scratch {
		std::vector<std::uint32_t> order;
		std::vector<std::uint32_t> stack;
		std::vector<std::uint32_t> finished;
		std::vector<thread_timeline> threads;
		std::vector<path_segment> path;
	};

	struct frame_series {
		std::map<path_key, std::vector<sample_time>> path;
		std::map<std::uint32_t, std::vector<sample_time>> wall;
		std::map<std::uint32_t, std::vector<sample_time>> self;
		std::map<std::uint32_t, std::vector<sample_time>> busy;
	};

	struct series_stats {
		sample_time mean;
		sample_time p50;
		sample_time p95;
	};

	auto window_of(
		const report_frame& frame
	) -> interval;

	auto clip(
		const report_node& node,
		const interval& window
	) -> interval;

	auto participates(
		const report_node& node,
		const interval& window
	) -> bool;

	auto build_timelines(
		const report_frame& frame,
		const interval& window,
		frame_scratch& scratch
	) -> void;

	auto segment_at(
		const thread_timeline& timeline,
		trace::tick_step t
	) -> const timeline_segment&;

	auto latest_release(
		const report_frame& frame,
		const interval& window,
		std::span<const std::uint32_t> finished,
		std::uint32_t tid,
		const interval& blocked
	) -> std::optional<std::uint32_t>;

	auto walk_critical_path(
		const report_frame& frame,
		const interval& window,
		std::uint32_t main_tid,
		frame_scratch& scratch
	) -> void;

	auto accumulate_stages(
		const report_frame& frame,
		const interval& window,
		std::size_t frame_index,
		std::size_t frame_count,
		frame_scratch& scratch,
		frame_series& series
	) -> void;

	template <typename Key>
	auto add_sample(
		std::map<Key, std::vector<sample_time>>& series,
		const Key& key,
		std::size_t frame_index,
		std::size_t frame_count,
		sample_time value
	) -> void;

	auto stats_of(
		std::vector<sample_time> values
	) -> series_stats;

	auto tag_label(
		const report_file& file,
		std::uint32_t tag
	) -> std::string_view;
}

auto gse::profile::window_of(const report_frame& frame) -> interval {
	return {
		.start = frame.boundary - quantity_cast<trace::tick_step>(frame.elapsed),
		.stop = frame.boundary,
	};
}

auto gse::profile::clip(const report_node& node, const interval& window) -> interval {
	return {
		.start = std::max(node.start, window.start),
		.stop = std::min(node.stop, window.stop),
	};
}

auto gse::profile::participates(const report_node& node, const interval& window) -> bool {
	const interval span = clip(node, window);
	return trace::cpu_scope(node) && span.stop > span.start;
}

auto gse::profile::build_timelines(const report_frame& frame, const interval& window, frame_scratch& scratch) -> void {
	scratch.order.clear();
	scratch.finished.clear();
	for (std::uint32_t i = 0; i < frame.nodes.size(); ++i) {
		if (!participates(frame.nodes[i], window)) {
			continue;
		}
		scratch.order.push_back(i);
		if (frame.nodes[i].kind == trace::span_kind::work) {
			scratch.finished.push_back(i);
		}
	}

	std::ranges::sort(scratch.order, [&](const std::uint32_t a, const std::uint32_t b) {
		const report_node& na = frame.nodes[a];
		const report_node& nb = frame.nodes[b];
		if (na.trace_id != nb.trace_id) {
			return na.trace_id < nb.trace_id;
		}
		if (na.start != nb.start) {
			return na.start < nb.start;
		}
		return na.stop > nb.stop;
	});

	std::ranges::sort(scratch.finished, {}, [&](const std::uint32_t i) {
		return clip(frame.nodes[i], window).stop;
	});

	const auto same_thread = [&](const std::uint32_t a, const std::uint32_t b) {
		return frame.nodes[a].trace_id == frame.nodes[b].trace_id;
	};

	scratch.threads.clear();
	for (const auto group : scratch.order | std::views::chunk_by(same_thread)) {
		thread_timeline& timeline = scratch.threads.emplace_back();
		timeline.tid = frame.nodes[group.front()].trace_id;

		trace::tick_step cursor = window.start;
		const auto emit = [&](const trace::tick_step until, const std::uint32_t owner) {
			if (until > cursor) {
				timeline.segments.push_back({
					.start = cursor,
					.stop = until,
					.owner = owner,
				});
				cursor = until;
			}
		};

		scratch.stack.clear();
		for (const std::uint32_t i : group) {
			const interval span = clip(frame.nodes[i], window);
			while (!scratch.stack.empty() && clip(frame.nodes[scratch.stack.back()], window).stop <= span.start) {
				emit(clip(frame.nodes[scratch.stack.back()], window).stop, scratch.stack.back());
				scratch.stack.pop_back();
			}
			emit(span.start, scratch.stack.empty() ? no_owner : scratch.stack.back());
			scratch.stack.push_back(i);
		}
		while (!scratch.stack.empty()) {
			emit(clip(frame.nodes[scratch.stack.back()], window).stop, scratch.stack.back());
			scratch.stack.pop_back();
		}
		emit(window.stop, no_owner);
	}
}

auto gse::profile::segment_at(const thread_timeline& timeline, const trace::tick_step t) -> const timeline_segment& {
	return *std::ranges::lower_bound(timeline.segments, t, {}, &timeline_segment::stop);
}

auto gse::profile::latest_release(const report_frame& frame, const interval& window, const std::span<const std::uint32_t> finished, const std::uint32_t tid, const interval& blocked) -> std::optional<std::uint32_t> {
	const auto stop_of = [&](const std::uint32_t i) {
		return clip(frame.nodes[i], window).stop;
	};

	const auto end = std::ranges::lower_bound(finished, blocked.stop, {}, stop_of);
	for (const std::uint32_t i : std::ranges::subrange(finished.begin(), end) | std::views::reverse) {
		if (stop_of(i) <= blocked.start) {
			return std::nullopt;
		}
		if (frame.nodes[i].trace_id != tid) {
			return i;
		}
	}
	return std::nullopt;
}

auto gse::profile::walk_critical_path(const report_frame& frame, const interval& window, const std::uint32_t main_tid, frame_scratch& scratch) -> void {
	scratch.path.clear();

	std::uint32_t tid = main_tid;
	trace::tick_step t = window.stop;

	while (t > window.start) {
		const auto timeline = std::ranges::find(scratch.threads, tid, &thread_timeline::tid);
		if (timeline == scratch.threads.end()) {
			return;
		}

		const timeline_segment& segment = segment_at(*timeline, t);
		const sample_time held = sample_time(t - segment.start);
		const std::uint32_t owner = segment.owner;
		const std::uint32_t tag = owner == no_owner ? untagged : frame.nodes[owner].tag;

		if (owner != no_owner && frame.nodes[owner].kind == trace::span_kind::work) {
			scratch.path.push_back({
				.key = {
					.tag = tag,
					.step = path_step::work,
				},
				.duration = held,
				.node = owner,
			});
			t = segment.start;
			continue;
		}

		if (owner == no_owner && tid == main_tid) {
			scratch.path.push_back({
				.key = {
					.tag = untagged,
					.step = path_step::untraced,
				},
				.duration = held,
				.node = no_owner,
			});
			t = segment.start;
			continue;
		}

		const interval blocked = {
			.start = segment.start,
			.stop = t,
		};

		if (const auto release = latest_release(frame, window, scratch.finished, tid, blocked)) {
			const trace::tick_step released = clip(frame.nodes[*release], window).stop;
			scratch.path.push_back({
				.key = {
					.tag = tag,
					.step = path_step::wake,
				},
				.duration = sample_time(t - released),
				.node = owner,
			});
			tid = frame.nodes[*release].trace_id;
			t = released;
			continue;
		}

		scratch.path.push_back({
			.key = {
				.tag = tag,
				.step = path_step::wait,
			},
			.duration = held,
			.node = owner,
		});
		t = segment.start;
	}
}

auto gse::profile::accumulate_stages(const report_frame& frame, const interval& window, const std::size_t frame_index, const std::size_t frame_count, frame_scratch& scratch, frame_series& series) -> void {
	std::ranges::sort(scratch.order, [&](const std::uint32_t a, const std::uint32_t b) {
		const report_node& na = frame.nodes[a];
		const report_node& nb = frame.nodes[b];
		if (na.tag != nb.tag) {
			return na.tag < nb.tag;
		}
		return na.start < nb.start;
	});

	const auto same_tag = [&](const std::uint32_t a, const std::uint32_t b) {
		return frame.nodes[a].tag == frame.nodes[b].tag;
	};

	for (const auto group : scratch.order | std::views::chunk_by(same_tag)) {
		sample_time wall{};
		sample_time self{};
		interval merged = clip(frame.nodes[group.front()], window);

		for (const std::uint32_t i : group) {
			const interval span = clip(frame.nodes[i], window);
			self += sample_time(frame.nodes[i].self);
			if (span.start > merged.stop) {
				wall += sample_time(merged.stop - merged.start);
				merged = span;
				continue;
			}
			merged.stop = std::max(merged.stop, span.stop);
		}
		wall += sample_time(merged.stop - merged.start);

		const std::uint32_t tag = frame.nodes[group.front()].tag;
		add_sample(series.wall, tag, frame_index, frame_count, wall);
		add_sample(series.self, tag, frame_index, frame_count, self);
	}
}

template <typename Key>
auto gse::profile::add_sample(std::map<Key, std::vector<sample_time>>& series, const Key& key, const std::size_t frame_index, const std::size_t frame_count, const sample_time value) -> void {
	series.try_emplace(key, frame_count, sample_time{}).first->second[frame_index] += value;
}

auto gse::profile::stats_of(std::vector<sample_time> values) -> series_stats {
	std::ranges::sort(values);
	sample_time total{};
	for (const sample_time value : values) {
		total += value;
	}
	return {
		.mean = total / static_cast<double>(values.size()),
		.p50 = percentile_of(values, 0.50),
		.p95 = percentile_of(values, 0.95),
	};
}

auto gse::profile::tag_label(const report_file& file, const std::uint32_t tag) -> std::string_view {
	return tag < file.tags.size() ? std::string_view(file.tags[tag]) : std::string_view("-");
}

auto gse::profile::analyze_frames(const report_file& file) -> frame_analysis {
	frame_analysis analysis{
		.frames = file.recorded.size(),
	};
	if (file.recorded.empty()) {
		return analysis;
	}

	const std::size_t frame_count = file.recorded.size();

	std::vector<std::size_t> by_elapsed(frame_count);
	std::ranges::iota(by_elapsed, std::size_t{ 0 });
	std::ranges::sort(by_elapsed, {}, [&](const std::size_t i) {
		return file.recorded[i].elapsed;
	});
	analysis.dag_frame = by_elapsed[frame_count / 2];

	frame_scratch scratch;
	frame_series series;
	sample_time elapsed_total{};

	for (std::size_t f = 0; f < frame_count; ++f) {
		const report_frame& frame = file.recorded[f];
		const interval window = window_of(frame);
		elapsed_total += frame.elapsed;

		build_timelines(frame, window, scratch);
		walk_critical_path(frame, window, file.main_tid, scratch);

		for (const path_segment& segment : scratch.path) {
			add_sample(series.path, segment.key, f, frame_count, segment.duration);
		}

		for (const thread_timeline& timeline : scratch.threads) {
			sample_time busy{};
			for (const timeline_segment& segment : timeline.segments) {
				if (segment.owner != no_owner) {
					busy += sample_time(segment.stop - segment.start);
				}
			}
			add_sample(series.busy, timeline.tid, f, frame_count, busy);
		}

		if (f == analysis.dag_frame) {
			for (const path_segment& segment : scratch.path) {
				if (segment.node != no_owner) {
					analysis.dag_path.push_back(segment.node);
				}
			}
			std::ranges::sort(analysis.dag_path);
			const auto repeated = std::ranges::unique(analysis.dag_path);
			analysis.dag_path.erase(repeated.begin(), repeated.end());
		}

		accumulate_stages(frame, window, f, frame_count, scratch, series);
	}

	analysis.frame_mean = elapsed_total / static_cast<double>(frame_count);

	for (auto& [key, values] : series.path) {
		const series_stats stats = stats_of(std::move(values));
		analysis.path.push_back({
			.tag = key.tag,
			.step = key.step,
			.mean = stats.mean,
			.p50 = stats.p50,
			.p95 = stats.p95,
		});
	}
	std::ranges::sort(analysis.path, std::ranges::greater{}, &path_row::mean);

	for (auto& [tag, values] : series.wall) {
		const series_stats stats = stats_of(std::move(values));
		analysis.stages.push_back({
			.tag = tag,
			.wall_mean = stats.mean,
			.wall_p50 = stats.p50,
			.wall_p95 = stats.p95,
			.self_mean = stats_of(std::move(series.self.at(tag))).mean,
		});
	}
	std::ranges::sort(analysis.stages, std::ranges::greater{}, &stage_row::self_mean);

	for (auto& [tid, values] : series.busy) {
		analysis.threads.push_back({
			.tid = tid,
			.busy_mean = stats_of(std::move(values)).mean,
		});
	}
	std::ranges::sort(analysis.threads, std::ranges::greater{}, &thread_row::busy_mean);

	return analysis;
}

auto gse::profile::write_path_verdict(std::ofstream& out, const frame_analysis& analysis) -> void {
	if (analysis.frames == 0) {
		return;
	}

	out << std::format("critical path over {} recorded frames (mean {:.2f:ms}):", analysis.frames, analysis.frame_mean);
	for (const path_step step : enum_values<path_step>()) {
		sample_time total{};
		for (const path_row& row : analysis.path) {
			if (row.step == step) {
				total += row.mean;
			}
		}
		out << std::format("    {} {:.2f:ms} ({:.0f}%)", step, total, total / analysis.frame_mean * 100.0);
	}
	out << "\n";
}

auto gse::profile::write_frame_analysis(std::ofstream& out, const report_file& file, const frame_analysis& analysis) -> void {
	out << "--- Critical path (walked back from the frame boundary on the main thread) ---\n";
	if (analysis.frames == 0) {
		out << "no recorded frames.\n\n";
		return;
	}
	out << "work = a traced span ran on the path. wait = the path blocked in a wait span, or a worker sat idle, and no other "
		   "thread finished work inside it: the GPU, the OS or an untraced thread released it. wake = blocked until another "
		   "thread finished work, then the path moved to that thread; a long wake means a weak edge. untraced = main-thread "
		   "time outside every span. The steps tile the frame, so their means sum to the mean frame.\n";

	std::size_t tag_width = std::string_view("tag").size();
	for (const path_row& row : analysis.path | std::views::take(analysis_row_limit)) {
		tag_width = std::max(tag_width, tag_label(file, row.tag).size());
	}

	const auto path_header = std::format("{:<{}} {:>9} {:>13} {:>13} {:>13} {:>8}", "tag", tag_width, "step", "mean", "p50", "p95", "% frame");
	out << path_header << '\n';
	out << std::string(path_header.size(), '-') << '\n';
	for (const path_row& row : analysis.path | std::views::take(analysis_row_limit)) {
		out << std::format(
			"{:<{}} {:>9} {:>10.2f:us} {:>10.2f:us} {:>10.2f:us} {:>7.1f}%\n",
			tag_label(file, row.tag),
			tag_width,
			row.step,
			row.mean,
			row.p50,
			row.p95,
			row.mean / analysis.frame_mean * 100.0
		);
	}
	out << '\n';

	out << "--- Stage wall time (union of each tag's spans across threads, per frame) ---\n";
	out << "self is the summed self time of the tag's spans, as in the parallel-sum table. par = self / wall; a fan-out with par "
		   "far below the worker count is imbalanced or too fine.\n";

	tag_width = std::string_view("tag").size();
	for (const stage_row& row : analysis.stages | std::views::take(analysis_row_limit)) {
		tag_width = std::max(tag_width, tag_label(file, row.tag).size());
	}

	const auto stage_header = std::format("{:<{}} {:>13} {:>13} {:>13} {:>13} {:>7}", "tag", tag_width, "wall", "wall p50", "wall p95", "self", "par");
	out << stage_header << '\n';
	out << std::string(stage_header.size(), '-') << '\n';
	for (const stage_row& row : analysis.stages | std::views::take(analysis_row_limit)) {
		out << std::format(
			"{:<{}} {:>10.2f:us} {:>10.2f:us} {:>10.2f:us} {:>10.2f:us} {:>7.2f}\n",
			tag_label(file, row.tag),
			tag_width,
			row.wall_mean,
			row.wall_p50,
			row.wall_p95,
			row.self_mean,
			row.self_mean / row.wall_mean
		);
	}
	out << '\n';

	out << "--- Thread utilisation (time inside any span, per frame) ---\n";
	const auto thread_header = std::format("{:<8} {:>13} {:>8}", "tid", "busy", "% frame");
	out << thread_header << '\n';
	out << std::string(thread_header.size(), '-') << '\n';
	for (const thread_row& row : analysis.threads) {
		out << std::format(
			"{:<8} {:>10.2f:us} {:>7.1f}%\n",
			row.tid == file.main_tid ? std::format("{} main", row.tid) : std::format("{}", row.tid),
			row.busy_mean,
			row.busy_mean / analysis.frame_mean * 100.0
		);
	}
	out << '\n';
}

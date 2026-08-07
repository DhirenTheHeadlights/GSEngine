export module gse.graphics:profiler_overlay;

import std;

import gse.os;
import gse.assets;
import gse.gpu;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.math;

import :types;
import :ids;
import :styles;
import :text_widget;
import :tree_widget;
import :cursor;
import :builder;

export namespace gse::gui {
	struct profiler {
		using result = void;
		static auto draw(
			draw_context& ctx,
			id& hot,
			id& active,
			id& focus
		) -> void;
	};
}

namespace gse::gui {
	struct profile_row {
		id id;
		time_t<std::uint64_t> start;
		time_t<std::uint64_t> stop;
		time_t<std::uint64_t> self;
		std::vector<profile_row> children;
	};

	struct profile_tree {
		std::vector<profile_row> roots;
		double frame_ns = 0.0;
		std::uint64_t generation = 0;
	};

	auto collect_visible_rows(
		const trace::frame_view& fv,
		std::span<const std::uint32_t> indices,
		const std::unordered_set<id>& hidden,
		std::vector<profile_row>& out
	) -> void;

	auto rebuild_profile_tree(
		profile_tree& tree
	) -> void;
}

auto gse::gui::collect_visible_rows(const trace::frame_view& fv, const std::span<const std::uint32_t> indices, const std::unordered_set<id>& hidden, std::vector<profile_row>& out) -> void {
	for (const auto index : indices) {
		const auto& n = fv.nodes[index];

		if (hidden.contains(n.id)) {
			collect_visible_rows(fv, fv.child_indices(n), hidden, out);
			continue;
		}

		profile_row row{
			.id = n.id,
			.start = n.start,
			.stop = n.stop,
			.self = n.self
		};

		collect_visible_rows(fv, fv.child_indices(n), hidden, row.children);

		std::ranges::sort(
			row.children,
			[](const profile_row& a, const profile_row& b) {
				return (a.stop - a.start) > (b.stop - b.start);
			}
		);

		out.push_back(std::move(row));
	}
}

auto gse::gui::rebuild_profile_tree(profile_tree& tree) -> void {
	const auto fv = trace::view();
	const auto hidden = trace::hidden_ids_snapshot();

	tree.roots.clear();
	tree.generation = fv.generation;
	tree.frame_ns = 0.0;

	collect_visible_rows(fv, fv.roots, hidden, tree.roots);

	std::ranges::sort(
		tree.roots,
		[](const profile_row& a, const profile_row& b) {
			return (a.stop - a.start) > (b.stop - b.start);
		}
	);

	if (!fv.roots.empty()) {
		auto frame_start = fv.nodes[fv.roots.front()].start;
		auto frame_end = fv.nodes[fv.roots.front()].stop;

		for (const auto index : fv.roots) {
			const auto& n = fv.nodes[index];
			if (n.start < frame_start) {
				frame_start = n.start;
			}
			if (n.stop > frame_end) {
				frame_end = n.stop;
			}
		}

		tree.frame_ns = static_cast<double>(static_cast<std::uint64_t>(frame_end - frame_start));
	}

	static const id gpu_root_id = trace_id<"GPU">();

	std::vector<profile::row> gpu_rows;
	profile::top_n(32, profile::domain::gpu, gpu_rows);

	if (gpu_rows.empty()) {
		return;
	}

	profile_row gpu_root{ .id = gpu_root_id };
	gpu_root.children.reserve(gpu_rows.size());

	for (const auto& r : gpu_rows) {
		gpu_root.children.push_back({ .id = r.id });
	}

	tree.roots.push_back(std::move(gpu_root));
}

auto gse::gui::profiler::draw(draw_context& ctx, id&, id& active, id&) -> void {
	trace::thread_pause pause;

	static profile_tree tree;
	static interval_timer refresh(milliseconds(100.f));
	static bool interacting = false;

	const bool refresh_due = refresh.tick();
	if (!interacting && (tree.generation == 0 || refresh_due)) {
		rebuild_profile_tree(tree);
	}

	if (tree.roots.empty()) {
		interacting = false;
		draw::text(ctx, "", "No trace data");
		return;
	}

	if (!ctx.current_menu) {
		return;
	}

	static float w_dur = 80.f;
	static float w_self = 80.f;
	static float w_avg = 80.f;
	static float w_peak = 80.f;
	static float w_frame = 60.f;

	static int resizing_col_idx = -1;

	const float pad = ctx.style.padding;
	const float font_sz = ctx.style.font_size;
	const auto code_view = ctx.fonts.code.resolve();
	const float row_h = code_view->line_height(font_sz) + pad * 0.5f;
	const rectf menu_content = ctx.current_menu->rect.inset({ pad, pad });

	const bool mouse_held = ctx.mouse_held();
	const vec2f mouse_pos = ctx.mouse_position();

	if (!mouse_held) {
		resizing_col_idx = -1;
	}

	const float x_frame_right = menu_content.right();
	const float x_frame_left = x_frame_right - w_frame;

	const float x_peak_right = x_frame_left - pad;
	const float x_peak_left = x_peak_right - w_peak;

	const float x_avg_right = x_peak_left - pad;
	const float x_avg_left = x_avg_right - w_avg;

	const float x_self_right = x_avg_left - pad;
	const float x_self_left = x_self_right - w_self;

	const float x_dur_right = x_self_left - pad;
	const float x_dur_left = x_dur_right - w_dur;

	const float header_y = ctx.layout_cursor.y();
	constexpr float hit_w = 6.0f;

	auto handle_resize = [&](float& width, const float right_anchor_x, const float split_x, const int idx) {
		const rectf hit_rect = rectf::from_position_size(
			{ split_x - hit_w * 0.5f, header_y },
			{ hit_w, row_h }
		);

		const bool hovered = ctx.hovers(hit_rect);

		if (resizing_col_idx == idx) {
			width = std::max(20.f, right_anchor_x - mouse_pos.x());
			set_style(cursor::style::resize_ew);
		}
		else if (resizing_col_idx == -1 && hovered) {
			set_style(cursor::style::resize_ew);
			if (mouse_held) {
				resizing_col_idx = idx;
			}
		}
	};

	handle_resize(w_frame, x_frame_right, x_frame_left, 4);
	handle_resize(w_peak, x_peak_right, x_peak_left, 3);
	handle_resize(w_avg, x_avg_right, x_avg_left, 2);
	handle_resize(w_self, x_self_right, x_self_left, 1);
	handle_resize(w_dur, x_dur_right, x_dur_left, 0);

	const float draw_x_frame = menu_content.right() - w_frame;
	const float draw_x_peak = draw_x_frame - pad - w_peak;
	const float draw_x_avg = draw_x_peak - pad - w_avg;
	const float draw_x_self = draw_x_avg - pad - w_self;
	const float draw_x_dur = draw_x_self - pad - w_dur;
	const float total_cols_w = w_dur + w_self + w_avg + w_peak + w_frame + (pad * 5);

	auto draw_header_item = [&](const std::string& txt, float x, float w) {
		const rectf r = rectf::from_position_size(
			{ x, header_y },
			{ w, row_h }
		);
		ctx.queue_sprite({
			.rect = r,
			.color = ctx.style.color_title_bar,
			.texture = ctx.blank_texture
		});
		ctx.queue_text({
			.font = ctx.fonts.code,
			.text = txt,
			.position = { r.left() + pad * 0.5f, r.center().y() + code_view->vertical_center_offset(font_sz) },
			.scale = font_sz,
			.clip_rect = r
		});
	};

	draw_header_item("Duration", draw_x_dur, w_dur);
	draw_header_item("Self", draw_x_self, w_self);
	draw_header_item("Avg", draw_x_avg, w_avg);
	draw_header_item("Peak", draw_x_peak, w_peak);
	draw_header_item("% Frame", draw_x_frame, w_frame);

	const float tree_w = draw_x_dur - menu_content.left() - pad;
	draw_header_item("Node Name", menu_content.left(), std::max(0.f, tree_w));

	ctx.layout_cursor.y() -= (row_h + (row_h * 0.15f) + pad);

	static draw::tree_selection selection;
	static draw::tree_options options{
		.indent_per_level = 15.f,
		.extra_right_padding = total_cols_w,
		.toggle_on_row_click = true,
		.multi_select = false,
	};
	options.extra_right_padding = total_cols_w;

	const double frame_ns = tree.frame_ns;

	const draw::tree_ops<profile_row> ops{
		.children = [](const profile_row& n) -> std::span<const profile_row> {
			return n.children;
		},
		.label = [](const profile_row& n) -> std::string_view {
			return n.id.tag();
		},
		.key = [](const profile_row& n) -> std::uint64_t {
			return n.id.number();
		},
		.is_leaf = [](const profile_row& n) -> bool {
			return n.children.empty();
		},
		.custom_draw =
			[=](
		const profile_row& n,
		const draw_context& draw_ctx,
		const rectf& row,
		bool,
		bool,
		int
	) {
				const bool has_cpu_timing = n.stop > n.start;
				const double dur_ns = has_cpu_timing ? static_cast<double>(static_cast<std::uint64_t>(n.stop - n.start)) : 0.0;
				const double self_ns = has_cpu_timing ? static_cast<double>(static_cast<std::uint64_t>(n.self)) : 0.0;
				const double pct_frame = (frame_ns > 0.0 && has_cpu_timing) ? (dur_ns / frame_ns) * 100.0 : 0.0;

				auto to_fixed = [](const double v, char* buf, const std::size_t len, const int prec) -> std::string_view {
					auto [p, ec] = std::to_chars(buf, buf + len, v, std::chars_format::fixed, prec);
					return ec == std::errc{} ? std::string_view{ buf, static_cast<std::size_t>(p - buf) } : std::string_view{};
				};

				char buf[32];

				auto draw_col = [&](const std::string_view val, float x, float w) {
					const rectf box = rectf::from_position_size(
						{ x, row.top() },
						{ w, row.height() }
					);

					draw_ctx.queue_sprite({
						.rect = box,
						.color = draw_ctx.style.color_widget_background,
						.texture = draw_ctx.blank_texture
					});

					draw_ctx.queue_text({
						.font = draw_ctx.fonts.code,
						.text = val,
						.position = { box.left() + pad * 0.5f, box.center().y() + draw_ctx.fonts.code.resolve()->vertical_center_offset(font_sz) },
						.scale = font_sz,
						.clip_rect = box
					});
				};

				if (has_cpu_timing) {
					draw_col(to_fixed(dur_ns / 1000.0, buf, 32, 1), draw_x_dur, w_dur);
					draw_col(to_fixed(self_ns / 1000.0, buf, 32, 1), draw_x_self, w_self);
					draw_col(to_fixed(pct_frame, buf, 32, 1), draw_x_frame, w_frame);
				}
				else {
					draw_col("", draw_x_dur, w_dur);
					draw_col("", draw_x_self, w_self);
					draw_col("", draw_x_frame, w_frame);
				}

				const auto node_id = n.id;
				const auto gpu_agg = profile::lookup(node_id, profile::domain::gpu);
				const auto cpu_agg = gpu_agg ? std::nullopt : profile::lookup(node_id, profile::domain::cpu);
				if (const auto& agg = gpu_agg ? gpu_agg : cpu_agg) {
					draw_col(to_fixed(agg->ema.as<microseconds>(), buf, 32, 1), draw_x_avg, w_avg);
					draw_col(to_fixed(agg->peak.as<microseconds>(), buf, 32, 1), draw_x_peak, w_peak);
				}
				else {
					draw_col("", draw_x_avg, w_avg);
					draw_col("", draw_x_peak, w_peak);
				}
			}
	};

	ids::scope tree_scope("gui.tree.profiler");

	const scroll_region_info body_info{
		.id = "gui.profiler.body",
		.size = { 0.f, 0.f },
		.config = {
			.scrollbar_width = 6.f,
			.scrollbar_min_height = 20.f,
			.scroll_speed = row_h * 2.f,
			.smooth_factor = 0.2f,
			.auto_hide_scrollbar = true,
			.smooth_scrolling = true,
		},
	};

	{
		auto region = scroll_region(ctx, body_info);
		interacting = draw::tree(
			ctx,
			std::span<const profile_row>(tree.roots),
			ops,
			options,
			&selection,
			active
		);
	}
}

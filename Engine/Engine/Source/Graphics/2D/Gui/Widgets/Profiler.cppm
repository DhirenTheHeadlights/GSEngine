export module gse.graphics:profiler_widget;

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
import :scroll_widget;
import :cursor;
import :builder;

export namespace gse::gui {
	struct profiler {
		using result = void;
		static auto draw(draw_context& ctx, id& hot, id& active, id& focus) -> void;
	};
}

auto gse::gui::profiler::draw(draw_context& ctx, id&, id& active, id&) -> void {
	trace::thread_pause pause;

	const std::span<const trace::node> roots = trace::view().roots;
	if (roots.empty()) {
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
	const float row_h = ctx.font->line_height(font_sz) + pad * 0.5f;
	const ui_rect menu_content = ctx.current_menu->rect.inset({ pad, pad });

	const bool mouse_held = ctx.input.mouse_button_held(mouse_button::button_1);
	const vec2f mouse_pos = ctx.input.mouse_position();

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
		const ui_rect hit_rect = ui_rect::from_position_size(
			{ split_x - hit_w * 0.5f, header_y },
			{ hit_w, row_h }
		);

		const bool hovered = hit_rect.contains(mouse_pos);

		if (resizing_col_idx == idx) {
			width = std::max(20.f, right_anchor_x - mouse_pos.x());
			set_style(cursor::style::resize_ew);
		} else if (resizing_col_idx == -1 && hovered) {
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
		const ui_rect r = ui_rect::from_position_size({ x, header_y }, { w, row_h });
		ctx.queue_sprite({
			.rect = r,
			.color = ctx.style.color_title_bar,
			.texture = ctx.blank_texture
		});
		ctx.queue_text({
			.font = ctx.font,
			.text = txt,
			.position = { r.left() + pad * 0.5f, r.center().y() + font_sz * 0.5f },
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

	const float tree_start_y = ctx.layout_cursor.y();
	const float visible_height = tree_start_y - menu_content.bottom() - pad;

	const ui_rect scroll_rect = ui_rect::from_position_size(
		{ menu_content.left(), tree_start_y },
		{ menu_content.width(), std::max(0.f, visible_height) }
	);

	const scroll_config profiler_scroll_config{
		.scrollbar_width = 6.f,
		.scrollbar_min_height = 20.f,
		.scroll_speed = row_h * 2.f,
		.smooth_factor = 0.2f,
		.auto_hide_scrollbar = true,
		.smooth_scrolling = true
	};

	static std::unordered_map<std::uint64_t, scroll_state> profiler_scrolls;
	scroll_state& profiler_scroll = profiler_scrolls[ids::stable_key("gui.profiler")];
	auto scroll_ctx = scroll::begin(profiler_scroll, scroll_rect, ctx.style, ctx.input, profiler_scroll_config);

	static draw::tree_selection selection;
	static draw::tree_options options{
		.indent_per_level = 15.f,
		.extra_right_padding = total_cols_w,
		.toggle_on_row_click = true,
		.multi_select = false
	};
	options.extra_right_padding = total_cols_w;
	options.scroll_offset = profiler_scroll.offset;
	options.clip_rect = scroll_rect;

	time_t<std::uint64_t> frame_start = roots[0].start;
	time_t<std::uint64_t> frame_end = roots[0].stop;

	for (const trace::node& r : roots) {
		if (r.start < frame_start) frame_start = r.start;
		if (r.stop > frame_end) frame_end = r.stop;
	}

	const double frame_ns = static_cast<double>((frame_end - frame_start).as<nanoseconds>());

	static std::unordered_map<const trace::node*, std::vector<trace::node>> children_sort_cache;
	static std::vector<trace::node> sorted_roots_buf;

	children_sort_cache.clear();

	const auto sort_by_duration = [](const trace::node& a, const trace::node& b) {
		return (a.stop - a.start) > (b.stop - b.start);
	};

	const auto flatten_hidden = [](auto& self, std::span<const trace::node> input, std::vector<trace::node>& out) -> void {
		for (const auto& n : input) {
			if (trace::is_hidden(n.id)) {
				self(self, { n.children_first, n.children_count }, out);
			} else {
				out.push_back(n);
			}
		}
	};

	sorted_roots_buf.clear();
	flatten_hidden(flatten_hidden, roots, sorted_roots_buf);
	std::ranges::sort(sorted_roots_buf, sort_by_duration);

	static const id gpu_root_id = trace_id<"GPU">();

	static std::vector<trace::node> gpu_children_buf;
	gpu_children_buf.clear();
	for (const auto& e : profile::top_n(32, true)) {
		gpu_children_buf.push_back(trace::node{ .id = e.id });
	}

	if (!gpu_children_buf.empty()) {
		sorted_roots_buf.push_back(trace::node{
			.id = gpu_root_id,
			.children_first = gpu_children_buf.data(),
			.children_count = gpu_children_buf.size()
		});
	}

	const draw::tree_ops<trace::node> ops{
		.children = [&flatten_hidden](const trace::node& n) -> std::span<const trace::node> {
			if (n.children_count == 0) return {};
			auto& vec = children_sort_cache[&n];
			if (vec.empty()) {
				flatten_hidden(flatten_hidden, { n.children_first, n.children_count }, vec);
				if (n.id != gpu_root_id) {
					std::ranges::sort(vec, [](const trace::node& a, const trace::node& b) {
						return (a.stop - a.start) > (b.stop - b.start);
					});
				}
			}
			return vec;
		},
		.label = [](const trace::node& n) -> std::string_view {
			return n.id.tag();
		},
		.key = [](const trace::node& n) -> std::uint64_t {
			return n.id.number();
		},
		.is_leaf = [](const trace::node& n) -> bool {
			return n.children_count == 0;
		},
		.custom_draw = [=](const trace::node& n, const draw_context& draw_ctx, const ui_rect& row, bool, bool, int) {
			const bool has_cpu_timing = n.stop > n.start;
			const double dur_ns = has_cpu_timing ? static_cast<double>((n.stop - n.start).as<nanoseconds>()) : 0.0;
			const double self_ns = has_cpu_timing ? static_cast<double>(n.self.as<nanoseconds>()) : 0.0;
			const double pct_frame = (frame_ns > 0.0 && has_cpu_timing) ? (dur_ns / frame_ns) * 100.0 : 0.0;

			auto to_fixed = [](const double v, char* buf, const std::size_t len, const int prec) -> std::string_view {
				auto [p, ec] = std::to_chars(buf, buf + len, v, std::chars_format::fixed, prec);
				return ec == std::errc{} ? std::string_view{ buf, static_cast<std::size_t>(p - buf) } : std::string_view{};
			};

			char buf[32];

			auto draw_col = [&](const std::string_view val, float x, float w) {
				const ui_rect box = ui_rect::from_position_size({ x, row.top() }, { w, row.height() });

				draw_ctx.queue_sprite({
					.rect = box,
					.color = draw_ctx.style.color_widget_background,
					.texture = draw_ctx.blank_texture
				});

				draw_ctx.queue_text({
					.font = draw_ctx.font,
					.text = std::string(val),
					.position = { box.left() + pad * 0.5f, box.center().y() + font_sz * 0.5f },
					.scale = font_sz,
					.clip_rect = box
				});
			};

			if (has_cpu_timing) {
				draw_col(to_fixed(dur_ns / 1000.0, buf, 32, 1), draw_x_dur, w_dur);
				draw_col(to_fixed(self_ns / 1000.0, buf, 32, 1), draw_x_self, w_self);
				draw_col(to_fixed(pct_frame, buf, 32, 1), draw_x_frame, w_frame);
			} else {
				draw_col("", draw_x_dur, w_dur);
				draw_col("", draw_x_self, w_self);
				draw_col("", draw_x_frame, w_frame);
			}

			const auto node_id = n.id;
			const auto gpu_agg = profile::lookup_gpu(node_id);
			const auto cpu_agg = gpu_agg ? std::nullopt : profile::lookup_cpu(node_id);
			if (const auto& agg = gpu_agg ? gpu_agg : cpu_agg) {
				draw_col(to_fixed(agg->ema.as<microseconds>(), buf, 32, 1), draw_x_avg, w_avg);
				draw_col(to_fixed(agg->peak.as<microseconds>(), buf, 32, 1), draw_x_peak, w_peak);
			} else {
				draw_col("", draw_x_avg, w_avg);
				draw_col("", draw_x_peak, w_peak);
			}
		}
	};

	ids::scope tree_scope("gui.tree.profiler");
	trace::set_finalize_paused(draw::tree(ctx, std::span<const trace::node>(sorted_roots_buf), ops, options, &selection, active));

	const float tree_end_y = ctx.layout_cursor.y();
	scroll::end(
		profiler_scroll, scroll_ctx,
		tree_end_y + profiler_scroll.offset,
		ctx.style, ctx.input,
		ctx.blank_texture, ctx.sprites, ctx.current_layer,
		profiler_scroll_config
	);

	const ui_rect header_cover = ui_rect::from_position_size(
		{ menu_content.left(), header_y },
		{ menu_content.width(), row_h }
	);
	ctx.queue_sprite({
		.rect = header_cover,
		.color = ctx.style.color_title_bar,
		.texture = ctx.blank_texture
	});
}

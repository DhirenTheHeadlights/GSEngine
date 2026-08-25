export module gse.ide.problems;

import std;
import gse;

import gse.ide.diagnostic;
import gse.ide.workspace;
import gse.ide.navigation;

export namespace gse::ide {
	struct problems_view_state {
		bool show_errors = true;
		bool show_warnings = true;
		bool show_hints = false;
	};

	struct problem_row {
		const diagnostic* entry = nullptr;
		const std::filesystem::path* path = nullptr;
		std::string_view document_name;
	};

	auto draw_problems_panel(
		gse::gui::builder& ui,
		const rectf& rect,
		problems_view_state& state,
		const workspace::data& ws,
		gse::channel_write<jump_to_request> channels
	) -> void;
}

namespace gse::ide {
	[[nodiscard]] auto severity_color(
		const gse::gui::style& sty,
		severity level
	) -> vec4f;

	[[nodiscard]] auto severity_label(
		severity level
	) -> std::string_view;

	[[nodiscard]] auto shows_severity(
		const problems_view_state& state,
		severity level
	) -> bool;

	[[nodiscard]] auto collect_problems(
		const problems_view_state& state,
		const workspace::data& ws
	) -> std::vector<problem_row>;

	auto draw_problem_row(
		const gse::gui::draw_context& ctx,
		const rectf& row,
		const problem_row& item,
		bool hovered
	) -> void;

	auto draw_problems_header(
		gse::gui::builder& ui,
		const rectf& row,
		problems_view_state& state,
		std::size_t shown,
		std::size_t total
	) -> void;

	auto draw_filter_toggle(
		const gse::gui::draw_context& ctx,
		const rectf& rect,
		std::string_view label,
		const vec4f& accent,
		bool& enabled
	) -> void;
}

auto gse::ide::severity_color(const gse::gui::style& sty, const severity level) -> vec4f {
	switch (level) {
		case severity::error:
			return sty.color_error;
		case severity::warning:
			return sty.color_warning;
		default:
			return sty.color_text_secondary;
	}
}

auto gse::ide::severity_label(const severity level) -> std::string_view {
	switch (level) {
		case severity::error:
			return "error";
		case severity::warning:
			return "warning";
		case severity::note:
			return "note";
		default:
			return "hint";
	}
}

auto gse::ide::shows_severity(const problems_view_state& state, const severity level) -> bool {
	switch (level) {
		case severity::error:
			return state.show_errors;
		case severity::warning:
			return state.show_warnings;
		default:
			return state.show_hints;
	}
}

auto gse::ide::collect_problems(const problems_view_state& state, const workspace::data& ws) -> std::vector<problem_row> {
	std::vector<problem_row> rows;
	for (const gse::id doc_id : ws.documents.order()) {
		const document& doc = ws.documents.at(doc_id);
		for (const std::vector<diagnostic>* list : { &doc.diagnostics, &doc.lint }) {
			for (const diagnostic& entry : *list) {
				if (!shows_severity(state, entry.level)) {
					continue;
				}
				rows.push_back({
					.entry = &entry,
					.path = &doc.path,
					.document_name = doc.tab_name,
				});
			}
		}
	}

	std::ranges::stable_sort(rows, [](const problem_row& a, const problem_row& b) {
		if (a.entry->level != b.entry->level) {
			return std::to_underlying(a.entry->level) < std::to_underlying(b.entry->level);
		}
		return a.entry->line < b.entry->line;
	});
	return rows;
}

auto gse::ide::draw_filter_toggle(const gse::gui::draw_context& ctx, const rectf& rect, const std::string_view label, const vec4f& accent, bool& enabled) -> void {
	const auto& sty = ctx.style;
	const auto text_view = ctx.fonts.text.resolve();
	const bool hovered = ctx.hovers(rect);

	if (hovered && ctx.mouse_pressed_for(rect)) {
		enabled = !enabled;
	}

	ctx.queue_sprite({
		.rect = rect,
		.color = enabled ? sty.color_widget_selected : (hovered ? sty.color_widget_hovered : sty.color_widget_background),
		.texture = ctx.blank_texture,
		.corner_radius = sty.corner_radius,
	});
	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = label,
		.position = { rect.left() + sty.padding * 0.5f, rect.center().y() + text_view->vertical_center_offset(sty.font_size) },
		.scale = sty.font_size,
		.color = enabled ? accent : sty.color_text_disabled,
		.clip_rect = rect,
	});
}

auto gse::ide::draw_problems_header(gse::gui::builder& ui, const rectf& row, problems_view_state& state, const std::size_t shown, const std::size_t total) -> void {
	const auto& ctx = ui.ctx;
	const auto& sty = ctx.style;
	const auto text_view = ctx.fonts.text.resolve();
	const float pad = sty.padding;
	const float toggle_h = std::max(0.f, row.height() - pad * 0.5f);

	ctx.queue_sprite({
		.rect = row,
		.color = sty.color_panel_alt,
		.texture = ctx.blank_texture,
	});

	float x = row.left() + pad;
	const std::array<std::string_view, 3> labels{ "Errors", "Warnings", "Hints" };
	const std::array<bool*, 3> flags{ &state.show_errors, &state.show_warnings, &state.show_hints };
	const std::array<vec4f, 3> accents{ sty.color_error, sty.color_warning, sty.color_text_secondary };

	for (std::size_t i = 0; i < labels.size(); ++i) {
		const float w = text_view->width(labels[i], sty.font_size) + pad;
		draw_filter_toggle(ctx, rectf::from_position_size({ x, row.top() - pad * 0.25f }, { w, toggle_h }), labels[i], accents[i], *flags[i]);
		x += w + pad * 0.5f;
	}

	const std::string summary = shown == total
		? std::format("{} problems", total)
		: std::format("{} of {} problems", shown, total);
	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = summary,
		.position = { row.right() - text_view->width(summary, sty.font_size) - pad, row.center().y() + text_view->vertical_center_offset(sty.font_size) },
		.scale = sty.font_size,
		.color = sty.color_text_secondary,
		.clip_rect = row,
	});
}

auto gse::ide::draw_problem_row(const gse::gui::draw_context& ctx, const rectf& row, const problem_row& item, const bool hovered) -> void {
	const auto& sty = ctx.style;
	const auto text_view = ctx.fonts.text.resolve();
	const float pad = sty.padding;
	const vec4f accent = severity_color(sty, item.entry->level);

	if (hovered) {
		ctx.queue_sprite({
			.rect = row,
			.color = sty.color_widget_hovered,
			.texture = ctx.blank_texture,
		});
	}

	ctx.queue_sprite({
		.rect = rectf::from_position_size({ row.left(), row.top() }, { sty.accent_bar_width, row.height() }),
		.color = accent,
		.texture = ctx.blank_texture,
	});

	const std::string location = std::format("{}:{}", item.document_name, item.entry->line + 1);
	const float location_w = text_view->width(location, sty.font_size);
	const float text_x = row.left() + sty.accent_bar_width + pad;
	const rectf message_clip = rectf::from_position_size(
		{ text_x, row.top() },
		{ std::max(0.f, row.right() - location_w - pad * 2.f - text_x), row.height() }
	);

	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = item.entry->message,
		.position = { text_x, row.center().y() + text_view->vertical_center_offset(sty.font_size) },
		.scale = sty.font_size,
		.color = sty.color_text,
		.clip_rect = message_clip,
	});
	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = location,
		.position = { row.right() - location_w - pad, row.center().y() + text_view->vertical_center_offset(sty.font_size) },
		.scale = sty.font_size,
		.color = sty.color_text_secondary,
		.clip_rect = row,
	});
}

auto gse::ide::draw_problems_panel(gse::gui::builder& ui, const rectf& rect, problems_view_state& state, const workspace::data& ws, const gse::channel_write<jump_to_request> channels) -> void {
	auto& ctx = ui.ctx;
	const auto& sty = ctx.style;
	const auto text_view = ctx.fonts.text.resolve();
	const float pad = sty.padding;
	const float row_h = text_view->line_height(sty.font_size) + pad;

	std::size_t total = 0;
	for (const gse::id doc_id : ws.documents.order()) {
		const document& doc = ws.documents.at(doc_id);
		total += doc.diagnostics.size() + doc.lint.size();
	}

	const std::vector<problem_row> rows = collect_problems(state, ws);

	const rectf header_rect = rectf::from_position_size(
		{ rect.left(), rect.top() },
		{ rect.width(), std::min(row_h + pad * 0.5f, rect.height()) }
	);
	draw_problems_header(ui, header_rect, state, rows.size(), total);

	const rectf list_rect = rectf::from_position_size(
		{ rect.left(), header_rect.bottom() },
		{ rect.width(), std::max(0.f, rect.height() - header_rect.height()) }
	);

	if (rows.empty()) {
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = total == 0 ? "No problems in open documents." : "All problems are filtered out.",
			.position = { list_rect.left() + pad, list_rect.top() - row_h },
			.scale = sty.font_size,
			.color = sty.color_text_secondary,
			.clip_rect = list_rect,
		});
		return;
	}

	ui.row_list({
		.id = "##problem_list",
		.bounds = list_rect,
		.row_height = row_h,
		.row_count = rows.size(),
	}, [&](gse::gui::builder& b, const gse::gui::row& r) {
		auto& c = b.ctx;
		const problem_row& item = rows[r.index];

		if (r.hovered && c.mouse_pressed_for(r.visible)) {
			const bool byte_columns = item.entry->rule.has_value();
			channels.push<jump_to_request>({
				.path = item.entry->file.empty() ? *item.path : item.entry->file,
				.line = item.entry->line,
				.column = item.entry->start_col,
				.end_line = byte_columns ? item.entry->end_line : 0,
				.end_column = byte_columns ? item.entry->end_col : 0,
			});
		}

		draw_problem_row(c, r.rect, item, r.hovered);
	});
}

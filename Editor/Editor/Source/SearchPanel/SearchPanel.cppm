export module gse.ide.search_panel;

import std;
import gse;

import gse.ide.search;
import gse.ide.navigation;

export namespace gse::ide {
	struct search_panel_state {
		search::query_driver driver;
		gse::gui::text_input_state input;
		bool include_files = true;
		bool include_symbols = true;
		bool include_content = true;
	};

	auto draw_search_panel(
		gse::gui::builder& ui,
		const rectf& rect,
		search_panel_state& state,
		const gse::input::state& keys,
		const search::index_state* index,
		gse::channel_write<jump_to_request> channels
	) -> void;
}

namespace gse::ide {
	[[nodiscard]] auto domain_label(
		search::domain source
	) -> std::string_view;

	[[nodiscard]] auto domain_color(
		const gse::gui::style& sty,
		search::domain source
	) -> vec4f;

	auto draw_scope_toggle(
		const gse::gui::draw_context& ctx,
		const rectf& rect,
		std::string_view label,
		bool& enabled
	) -> void;

	auto draw_search_result_row(
		const gse::gui::draw_context& ctx,
		const rectf& row,
		const search::result& entry,
		bool hovered,
		bool selected
	) -> void;
}

auto gse::ide::domain_label(const search::domain source) -> std::string_view {
	switch (source) {
		case search::domain::content:
			return "text";
		case search::domain::symbol:
			return "symbol";
		default:
			return "file";
	}
}

auto gse::ide::domain_color(const gse::gui::style& sty, const search::domain source) -> vec4f {
	switch (source) {
		case search::domain::content:
			return sty.color_text_secondary;
		case search::domain::symbol:
			return sty.color_accent;
		default:
			return sty.color_file;
	}
}

auto gse::ide::draw_scope_toggle(const gse::gui::draw_context& ctx, const rectf& rect, const std::string_view label, bool& enabled) -> void {
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
		.color = enabled ? sty.color_text : sty.color_text_disabled,
		.clip_rect = rect,
	});
}

auto gse::ide::draw_search_result_row(const gse::gui::draw_context& ctx, const rectf& row, const search::result& entry, const bool hovered, const bool selected) -> void {
	const auto& sty = ctx.style;
	const auto text_view = ctx.fonts.text.resolve();
	const float pad = sty.padding;

	if (selected || hovered) {
		ctx.queue_sprite({
			.rect = row,
			.color = selected ? sty.color_widget_selected : sty.color_widget_hovered,
			.texture = ctx.blank_texture,
		});
	}

	const std::string_view tag = domain_label(entry.source);
	const float tag_w = text_view->width(tag, sty.font_size) + pad;
	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = tag,
		.position = { row.left() + pad * 0.5f, row.center().y() + text_view->vertical_center_offset(sty.font_size) },
		.scale = sty.font_size,
		.color = domain_color(sty, entry.source),
		.clip_rect = row,
	});

	const float detail_w = text_view->width(entry.detail, sty.font_size);
	const float display_x = row.left() + tag_w + pad * 0.5f;
	const rectf display_clip = rectf::from_position_size(
		{ display_x, row.top() },
		{ std::max(0.f, row.right() - detail_w - pad * 2.f - display_x), row.height() }
	);

	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = entry.display,
		.position = { display_x, row.center().y() + text_view->vertical_center_offset(sty.font_size) },
		.scale = sty.font_size,
		.color = sty.color_text,
		.clip_rect = display_clip,
	});

	if (!entry.detail.empty()) {
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = entry.detail,
			.position = { row.right() - detail_w - pad, row.center().y() + text_view->vertical_center_offset(sty.font_size) },
			.scale = sty.font_size,
			.color = sty.color_text_secondary,
			.clip_rect = row,
		});
	}
}

auto gse::ide::draw_search_panel(gse::gui::builder& ui, const rectf& rect, search_panel_state& state, const gse::input::state& keys, const search::index_state* index, const gse::channel_write<jump_to_request> channels) -> void {
	auto& ctx = ui.ctx;
	const auto& sty = ctx.style;
	const auto text_view = ctx.fonts.text.resolve();
	const float pad = sty.padding;
	const float row_h = text_view->line_height(sty.font_size) + pad * 0.5f;

	const rectf query_rect = rectf::from_position_size(
		{ rect.left() + pad, rect.top() - pad * 0.5f },
		{ std::max(0.f, rect.width() - pad * 2.f), row_h }
	);

	ctx.queue_sprite({
		.rect = query_rect,
		.color = sty.color_input_background,
		.texture = ctx.blank_texture,
		.corner_radius = sty.corner_radius,
	});

	const gse::id query_id = gse::gui::ids::make("##search_panel_query");
	gse::gui::draw::text_input_in_rect(ctx, query_id, state.driver.query, state.input, query_rect, ui.hot_widget_id, ui.focus_widget_id);
	const bool focused = ui.focus_widget_id == query_id;

	if (state.driver.query.empty() && !focused) {
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = "Search the project...",
			.position = { query_rect.left() + pad, query_rect.center().y() + text_view->vertical_center_offset(sty.font_size) },
			.scale = sty.font_size,
			.color = sty.color_text_secondary,
			.clip_rect = query_rect,
		});
	}

	const rectf scope_row = rectf::from_position_size(
		{ rect.left() + pad, query_rect.bottom() - pad * 0.5f },
		{ std::max(0.f, rect.width() - pad * 2.f), row_h }
	);

	float x = scope_row.left();
	const std::array<std::string_view, 3> labels{ "Files", "Symbols", "Text" };
	const std::array<bool*, 3> flags{ &state.include_files, &state.include_symbols, &state.include_content };
	for (std::size_t i = 0; i < labels.size(); ++i) {
		const float w = text_view->width(labels[i], sty.font_size) + pad;
		draw_scope_toggle(ctx, rectf::from_position_size({ x, scope_row.top() }, { w, scope_row.height() }), labels[i], *flags[i]);
		x += w + pad * 0.5f;
	}

	const time now = gse::system_clock::now<time>();
	state.driver.update(now, index, search::options{
		.max_results = 400,
		.include_content = state.include_content,
		.include_symbols = state.include_symbols,
		.include_files = state.include_files,
	});

	const rectf list_rect = rectf::from_position_size(
		{ rect.left(), scope_row.bottom() - pad * 0.5f },
		{ rect.width(), std::max(0.f, scope_row.bottom() - pad * 0.5f - rect.bottom()) }
	);

	if (state.driver.results.empty()) {
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = state.driver.query.empty() ? "Type to search files, symbols and text." : "No matches.",
			.position = { list_rect.left() + pad, list_rect.top() - row_h },
			.scale = sty.font_size,
			.color = sty.color_text_secondary,
			.clip_rect = list_rect,
		});
		return;
	}

	if (focused) {
		if (keys.key_pressed(gse::key::down)) {
			state.driver.selected = std::min<int>(state.driver.selected + 1, static_cast<int>(state.driver.results.size()) - 1);
		}
		if (keys.key_pressed(gse::key::up)) {
			state.driver.selected = std::max(state.driver.selected - 1, 0);
		}
		if (keys.key_pressed(gse::key::enter)) {
			const int idx = state.driver.selected >= 0 ? state.driver.selected : 0;
			const search::result& chosen = state.driver.results[static_cast<std::size_t>(idx)];
			channels.push<jump_to_request>({
				.path = chosen.path,
				.line = chosen.line,
				.column = chosen.column,
			});
		}
	}

	ctx.layout_cursor = { list_rect.left(), list_rect.top() };

	ui.scroll_region({
		.id = "##search_panel_results",
		.size = list_rect.size(),
	}, [&](gse::gui::builder& b) {
		auto& c = b.ctx;
		for (std::size_t i = 0; i < state.driver.results.size(); ++i) {
			const search::result& entry = state.driver.results[i];
			const rectf row = rectf::from_position_size(
				{ list_rect.left(), c.layout_cursor.y() },
				{ list_rect.width(), row_h }
			);
			const rectf visible = row.intersection(list_rect);
			const bool hovered = visible.height() > 0.f && c.hovers(visible);

			if (hovered && c.mouse_pressed_for(visible)) {
				state.driver.selected = static_cast<int>(i);
				channels.push<jump_to_request>({
					.path = entry.path,
					.line = entry.line,
					.column = entry.column,
				});
			}

			draw_search_result_row(c, row, entry, hovered, state.driver.selected == static_cast<int>(i));
			c.layout_cursor.y() -= row_h;
		}
	});
}

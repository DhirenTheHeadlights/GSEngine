export module gse.ide.app:search_screen;

import std;
import gse;

import gse.ide.workspace;
import gse.ide.search;

export namespace gse::ide {
	class search_screen : public gse::gui::screen {
	public:
		search_screen(
			gse::channel_writer channels,
			const search::index_state* index
		);

		auto build(
			gse::gui::builder& ui,
			gse::gui::nav& n
		) -> void override;

		auto title() const -> std::string_view override;

		auto captures_input() const -> bool override;

		auto dismissable() const -> bool override;

		auto should_dismiss() const -> bool override;

		auto body_rect(
			const gse::gui::style& sty,
			gse::vec2f viewport_size
		) const -> gse::rect_t<gse::vec2f> override;

		auto draw_backdrop(
			gse::gui::draw_context& ctx,
			gse::vec2f viewport_size
		) const -> void override;

	private:
		using ui_rect = gse::rect_t<gse::vec2f>;

		auto draw_row(
			gse::gui::draw_context& ctx,
			const ui_rect& row,
			const search::result& r,
			bool selected
		) const -> void;

		gse::channel_writer m_channels;
		const search::index_state* m_index;
		std::string m_query;
		std::string m_last_query;
		gse::time m_change_time{};
		bool m_dirty = false;
		gse::gui::text_input_state m_input;
		std::shared_ptr<search::query_buffer> m_pending;
		std::vector<search::result> m_results;
		int m_selected = 0;
		bool m_dismiss = false;
	};
}

gse::ide::search_screen::search_screen(gse::channel_writer channels, const search::index_state* index)
	: m_channels(std::move(channels)), m_index(index) {
}

auto gse::ide::search_screen::title() const -> std::string_view {
	return "Search Everything";
}

auto gse::ide::search_screen::captures_input() const -> bool {
	return true;
}

auto gse::ide::search_screen::dismissable() const -> bool {
	return true;
}

auto gse::ide::search_screen::should_dismiss() const -> bool {
	return m_dismiss;
}

auto gse::ide::search_screen::body_rect(const gse::gui::style&, const gse::vec2f viewport_size) const -> gse::rect_t<gse::vec2f> {
	const float w = std::min(viewport_size.x() * 0.6f, 900.f);
	const float h = std::min(viewport_size.y() * 0.7f, 640.f);
	return ui_rect::from_position_size(
		{ (viewport_size.x() - w) * 0.5f, (viewport_size.y() + h) * 0.5f },
		{ w, h }
	);
}

auto gse::ide::search_screen::draw_backdrop(gse::gui::draw_context& ctx, const gse::vec2f viewport_size) const -> void {
	ctx.sprites.push_back({
		.rect = ui_rect::from_position_size({ 0.f, viewport_size.y() }, viewport_size),
		.color = { 0.f, 0.f, 0.f, 0.55f },
		.texture = ctx.blank_texture,
		.layer = gse::render_layer::overlay,
	});
	const ui_rect card = body_rect(ctx.style, viewport_size);
	ctx.sprites.push_back({
		.rect = card,
		.color = { ctx.style.color_menu_body.x(), ctx.style.color_menu_body.y(), ctx.style.color_menu_body.z(), 1.f },
		.texture = ctx.blank_texture,
		.layer = gse::render_layer::overlay,
		.corner_radius = ctx.style.corner_radius_menu,
	});
}

auto gse::ide::search_screen::draw_row(gse::gui::draw_context& ctx, const ui_rect& row, const search::result& r, const bool selected) const -> void {
	const auto& sty = ctx.style;
	const float pad = sty.padding;
	const float fs = sty.font_size;

	if (selected) {
		ctx.queue_sprite({
			.rect = row,
			.color = sty.color_selection,
			.texture = ctx.blank_texture,
			.corner_radius = sty.corner_radius,
		});
	}

	const ui_rect icon_rect = ui_rect::from_position_size(
		{ row.left() + pad, row.center().y() + fs * 0.5f },
		{ fs, fs }
	);
	gse::gui::symbol::draw(ctx, gse::gui::symbol::file(), icon_rect, {
		.color = sty.color_icon,
		.scale = sty.icon_scale,
		.clip_rect = row,
	});

	const float text_x = icon_rect.right() + pad;
	const float baseline = row.center().y() + ctx.font->vertical_center_offset(fs);

	std::string location;
	if (r.source != search::domain::file) {
		location = r.path.filename().generic_display_string() + ":" + std::to_string(r.line + 1);
		if (r.source == search::domain::symbol && !r.detail.empty()) {
			location = r.detail + "  " + location;
		}
	}
	const float loc_w = location.empty() ? 0.f : ctx.font->width(location, fs);
	const float loc_x = row.right() - pad - loc_w;
	if (!location.empty()) {
		ctx.queue_text({
			.font = ctx.font,
			.text = location,
			.position = { loc_x, baseline },
			.scale = fs,
			.color = sty.color_text_secondary,
			.clip_rect = row,
		});
	}

	const ui_rect display_clip = ui_rect::from_position_size(
		{ text_x, row.top() },
		{ std::max(0.f, loc_x - pad - text_x), row.height() }
	);

	if (!r.highlight.empty()) {
		const std::vector<float> offsets = ctx.font->caret_offsets(r.display, fs);
		for (const search::match_range& mr : r.highlight) {
			const std::size_t a = std::min<std::size_t>(mr.start, r.display.size());
			const std::size_t b = std::min<std::size_t>(mr.start + mr.length, r.display.size());
			if (a >= b) {
				continue;
			}
			ctx.queue_sprite({
				.rect = ui_rect::from_position_size(
					{ text_x + offsets[a], row.center().y() + fs * 0.5f },
					{ offsets[b] - offsets[a], fs }
				),
				.color = sty.color_accent_dim,
				.texture = ctx.blank_texture,
				.clip_rect = display_clip,
			});
		}
	}

	ctx.queue_text({
		.font = ctx.font,
		.text = r.display,
		.position = { text_x, baseline },
		.scale = fs,
		.color = sty.color_text,
		.clip_rect = display_clip,
	});
}

auto gse::ide::search_screen::build(gse::gui::builder& ui, gse::gui::nav&) -> void {
	auto& ctx = ui.ctx;
	if (!ctx.current_menu) {
		return;
	}

	const auto scope = ctx.scoped_layer(gse::render_layer::popup);
	const auto& sty = ctx.style;
	const ui_rect card = ctx.current_menu->rect;
	const float pad = sty.padding;

	const float input_h = ctx.font->line_height(sty.font_size) + pad;
	const ui_rect input_rect = ui_rect::from_position_size(
		{ card.left() + pad, card.top() - pad },
		{ card.width() - pad * 2.f, input_h }
	);

	const gse::id input_id = gse::gui::ids::make("##search_palette_input");
	ui.focus_widget_id = input_id;
	gse::gui::draw::text_input_in_rect(ctx, input_id, m_query, m_input, input_rect, ui.hot_widget_id, ui.focus_widget_id);

	if (m_query.empty()) {
		ctx.queue_text({
			.font = ctx.font,
			.text = "Search files, symbols, contents...",
			.position = { input_rect.left() + pad, input_rect.center().y() + ctx.font->vertical_center_offset(sty.font_size) },
			.scale = sty.font_size,
			.color = sty.color_text_secondary,
			.clip_rect = input_rect,
		});
	}

	const gse::time now = gse::system_clock::now<gse::time>();
	if (m_query != m_last_query) {
		m_last_query = m_query;
		m_change_time = now;
		m_dirty = true;
	}
	if (m_dirty && now - m_change_time > gse::milliseconds(120)) {
		m_dirty = false;
		if (m_query.empty()) {
			m_results.clear();
			m_pending.reset();
		}
		else {
			m_pending = std::make_shared<search::query_buffer>();
			search::engine::submit(m_pending, *m_index, m_query, search::options{});
		}
	}
	if (m_pending && m_pending->done.load(std::memory_order_acquire)) {
		m_results = std::move(m_pending->results);
		m_selected = 0;
		m_pending.reset();
	}

	if (ctx.input.key_pressed(gse::key::escape)) {
		m_dismiss = true;
		return;
	}
	if (!m_results.empty()) {
		if (ctx.input.key_pressed(gse::key::down)) {
			m_selected = std::min<int>(m_selected + 1, static_cast<int>(m_results.size()) - 1);
		}
		if (ctx.input.key_pressed(gse::key::up)) {
			m_selected = std::max(m_selected - 1, 0);
		}
		if (ctx.input.key_pressed(gse::key::enter)) {
			const search::result& r = m_results[static_cast<std::size_t>(m_selected)];
			m_channels.push<jump_to_request>({ .path = r.path, .line = r.line, .column = r.column });
			m_dismiss = true;
			return;
		}
	}

	const float list_top = input_rect.bottom() - pad;
	const float list_bottom = card.bottom() + pad;
	const ui_rect list_rect = ui_rect::from_position_size(
		{ card.left() + pad, list_top },
		{ card.width() - pad * 2.f, std::max(0.f, list_top - list_bottom) }
	);
	const float row_h = ctx.font->line_height(sty.font_size) + pad;

	ctx.layout_cursor = { list_rect.left(), list_rect.top() };

	ui.scroll_region({ .id = "##search_palette_list", .size = list_rect.size() }, [&](gse::gui::builder& b) {
		auto& c = b.ctx;
		const gse::vec2f mouse = c.input.mouse_position();
		const bool clicked = c.input.mouse_button_pressed(gse::mouse_button::button_1);
		const ui_rect clip = c.current_clip().value_or(list_rect);
		for (std::size_t i = 0; i < m_results.size(); ++i) {
			const ui_rect row = ui_rect::from_position_size(
				{ list_rect.left(), c.layout_cursor.y() },
				{ list_rect.width(), row_h }
			);
			const bool over = row.contains(mouse) && clip.contains(mouse);
			if (over) {
				m_selected = static_cast<int>(i);
			}
			draw_row(c, row, m_results[i], m_selected == static_cast<int>(i));
			if (over && clicked && !c.is_press_consumed(gse::mouse_button::button_1)) {
				c.consume_press(gse::mouse_button::button_1);
				m_channels.push<jump_to_request>({ .path = m_results[i].path, .line = m_results[i].line, .column = m_results[i].column });
				m_dismiss = true;
			}
			c.layout_cursor.y() -= row_h;
		}
	});
}

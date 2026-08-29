export module gse.ide.app:search_screen;

import std;
import gse;

import gse.ide.workspace;
import gse.ide.search;

export namespace gse::ide {
	class search_screen : public gui::screen {
	public:
		search_screen(
			channel_write<jump_to_request> channels,
			const search::index_state* index
		);

		auto build(
			gui::builder& ui,
			gui::nav& n
		) -> void override;

		auto title() const -> std::string_view override;

		auto captures_input() const -> bool override;

		auto dismissable() const -> bool override;

		auto should_dismiss() const -> bool override;

		auto body_rect(
			const gui::style& sty,
			vec2f viewport_size
		) const -> rect_t<vec2f> override;

		auto draw_backdrop(
			gui::draw_context& ctx,
			vec2f viewport_size
		) const -> void override;

	private:

		auto draw_row(
			gui::draw_context& ctx,
			const rectf& row,
			const search::result& r,
			const std::string& location,
			bool selected
		) const -> void;

		channel_write<jump_to_request> m_channels;
		const search::index_state* m_index;
		search::query_driver m_driver;
		std::vector<std::string> m_locations;
		gui::text_input_state m_input;
		bool m_dismiss = false;
	};
}

gse::ide::search_screen::search_screen(channel_write<jump_to_request> channels, const search::index_state* index)
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

auto gse::ide::search_screen::body_rect(const gui::style& sty, const vec2f viewport_size) const -> rect_t<vec2f> {
	const float w = std::min(viewport_size.x() * 0.6f, 900.f * sty.scale_factor);
	const float h = std::min(viewport_size.y() * 0.7f, 640.f * sty.scale_factor);
	return rectf::from_position_size(
		{ (viewport_size.x() - w) * 0.5f, (viewport_size.y() + h) * 0.5f },
		{ w, h }
	);
}

auto gse::ide::search_screen::draw_backdrop(gui::draw_context& ctx, const vec2f viewport_size) const -> void {
	ctx.sprites.push_back({
		.rect = rectf::from_position_size({ 0.f, viewport_size.y() }, viewport_size),
		.color = { 0.f, 0.f, 0.f, 0.55f },
		.texture = ctx.blank_texture,
		.layer = render_layer::overlay,
	});
	const rectf card = body_rect(ctx.style, viewport_size);
	ctx.sprites.push_back({
		.rect = card,
		.color = { vec3f(ctx.style.color_menu_body), 1.f },
		.texture = ctx.blank_texture,
		.layer = render_layer::overlay,
		.corner_radius = ctx.style.corner_radius_menu,
	});
}

auto gse::ide::search_screen::draw_row(gui::draw_context& ctx, const rectf& row, const search::result& r, const std::string& location, const bool selected) const -> void {
	const auto& sty = ctx.style;
	const auto text_view = ctx.fonts.text.resolve();
	const auto code_view = ctx.fonts.code.resolve();
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

	const rectf icon_rect = rectf::from_position_size(
		{ row.left() + pad, row.center().y() + fs * 0.5f },
		{ fs, fs }
	);
	gui::symbol::draw(ctx, gui::symbol::file(), icon_rect, {
		.color = sty.color_icon,
		.extent = sty.icon_extent,
		.clip_rect = row,
	});

	const float text_x = icon_rect.right() + pad;
	const float baseline = row.center().y() + text_view->vertical_center_offset(fs);

	const float loc_w = location.empty() ? 0.f : code_view->width(location, fs);
	const float loc_x = row.right() - pad - loc_w;
	if (!location.empty()) {
		ctx.queue_text({
			.font = ctx.fonts.code,
			.text = location,
			.position = { loc_x, row.center().y() + code_view->vertical_center_offset(fs) },
			.scale = fs,
			.color = sty.color_text_secondary,
			.clip_rect = row,
		});
	}

	const rectf display_clip = rectf::from_position_size(
		{ text_x, row.top() },
		{ std::max(0.f, loc_x - pad - text_x), row.height() }
	);

	if (!r.highlight.empty()) {
		const std::vector<float> offsets = text_view->caret_offsets(r.display, fs);
		for (const search::match_range& mr : r.highlight) {
			const std::size_t a = std::min<std::size_t>(mr.start, r.display.size());
			const std::size_t b = std::min<std::size_t>(mr.start + mr.length, r.display.size());
			if (a >= b) {
				continue;
			}
			ctx.queue_sprite({
				.rect = rectf::from_position_size(
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
		.font = ctx.fonts.text,
		.text = r.display,
		.position = { text_x, baseline },
		.scale = fs,
		.color = sty.color_text,
		.clip_rect = display_clip,
	});
}

auto gse::ide::search_screen::build(gui::builder& ui, gui::nav&) -> void {
	auto& ctx = ui.ctx;
	const auto text_view = ctx.fonts.text.resolve();
	if (!ctx.current_menu) {
		return;
	}

	const auto scope = ctx.scoped_layer(render_layer::popup);
	const auto& sty = ctx.style;
	const rectf card = ctx.current_menu->rect;
	const float pad = sty.padding;

	const float input_h = text_view->line_height(sty.font_size) + pad;
	const rectf input_rect = rectf::from_position_size(
		{ card.left() + pad, card.top() - pad },
		{ card.width() - pad * 2.f, input_h }
	);

	const id input_id = gui::ids::make("##search_palette_input");
	ui.focus_widget_id = input_id;
	gui::draw::text_input_in_rect(ctx, input_id, m_driver.query, m_input, input_rect, ui.hot_widget_id, ui.focus_widget_id);

	if (m_driver.query.empty()) {
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = "Search files, symbols, contents...",
			.position = { input_rect.left() + pad, input_rect.center().y() + text_view->vertical_center_offset(sty.font_size) },
			.scale = sty.font_size,
			.color = sty.color_text_secondary,
			.clip_rect = input_rect,
		});
	}

	const time now = system_clock::now<time>();
	if (m_driver.update(now, m_index, search::options{})) {
		m_driver.selected = 0;
		m_locations.clear();
		m_locations.reserve(m_driver.results.size());
		for (const search::result& r : m_driver.results) {
			std::string location;
			if (r.source != search::domain::file) {
				location = r.path.filename().generic_display_string() + ":" + std::to_string(r.line + 1);
				if (r.source == search::domain::symbol && !r.detail.empty()) {
					location = r.detail + "  " + location;
				}
			}
			m_locations.push_back(std::move(location));
		}
	}

	if (ctx.key_pressed(key::escape)) {
		m_driver.accept();
		m_dismiss = true;
		return;
	}
	if (!m_driver.results.empty()) {
		if (ctx.key_pressed(key::down)) {
			m_driver.selected = std::min<int>(m_driver.selected + 1, static_cast<int>(m_driver.results.size()) - 1);
		}
		if (ctx.key_pressed(key::up)) {
			m_driver.selected = std::max(m_driver.selected - 1, 0);
		}
		if (ctx.key_pressed(key::enter)) {
			const int idx = m_driver.selected >= 0 ? m_driver.selected : 0;
			const search::result& r = m_driver.results[static_cast<std::size_t>(idx)];
			m_channels.push<jump_to_request>(search::jump_target(r));
			m_driver.accept();
			m_dismiss = true;
			return;
		}
	}

	const float list_top = input_rect.bottom() - pad;
	const float list_bottom = card.bottom() + pad;
	const rectf list_rect = rectf::from_position_size(
		{ card.left() + pad, list_top },
		{ card.width() - pad * 2.f, std::max(0.f, list_top - list_bottom) }
	);
	const float row_h = text_view->line_height(sty.font_size) + pad;

	ui.row_list({
		.id = "##search_palette_list",
		.bounds = list_rect,
		.row_height = row_h,
		.row_count = m_driver.results.size(),
	}, [&](gui::builder& b, const gui::row& r) {
		auto& c = b.ctx;
		if (r.hovered) {
			m_driver.selected = static_cast<int>(r.index);
		}
		draw_row(c, r.rect, m_driver.results[r.index], m_locations[r.index], m_driver.selected == static_cast<int>(r.index));
		if (r.hovered && c.mouse_pressed_for(r.visible)) {
			m_channels.push<jump_to_request>(search::jump_target(m_driver.results[r.index]));
			m_driver.accept();
			m_dismiss = true;
		}
	});
}

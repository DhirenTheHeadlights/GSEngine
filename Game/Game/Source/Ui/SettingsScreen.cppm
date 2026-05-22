export module gs:settings_screen;

import std;
import gse;

export namespace gs {
	class settings_screen : public gse::gui::screen {
	public:
		settings_screen(
			const gse::save::registry& save_reg,
			gse::channel_writer channels
		);

		auto build(
			gse::gui::builder& ui,
			gse::gui::nav& n
		) -> void override;

		auto title() const -> std::string_view override;

		auto body_rect(
			const gse::gui::style& sty,
			gse::vec2f viewport_size
		) const -> gse::gui::ui_rect override;

		auto draw_backdrop(
			gse::gui::draw_context& ctx,
			gse::vec2f viewport_size
		) const -> void override;

	private:
		auto refresh_categories() -> void;

		auto draw_header(
			gse::gui::draw_context& ctx,
			const gse::gui::ui_rect& rect,
			gse::gui::nav& n
		) const -> void;

		auto draw_close_button(
			gse::gui::draw_context& ctx,
			const gse::gui::ui_rect& rect,
			gse::gui::nav& n
		) const -> void;

		const gse::save::registry* m_save_reg;
		gse::channel_writer m_channels;
		gse::settings::panel_state m_panel_state;
		std::string m_selected_category;
		std::vector<std::string> m_categories;

		static constexpr float max_card_width = 1100.f;
		static constexpr float max_card_height = 760.f;
		static constexpr float min_card_width = 720.f;
		static constexpr float min_card_height = 480.f;
		static constexpr float card_margin_x = 80.f;
		static constexpr float card_margin_y = 60.f;
		static constexpr float sidebar_width = 220.f;
		static constexpr float header_height = 56.f;
		static constexpr float backdrop_alpha = 0.55f;
		static constexpr float close_button_size = 28.f;
	};
}

gs::settings_screen::settings_screen(const gse::save::registry& save_reg, gse::channel_writer channels)
	: m_save_reg(&save_reg),
	  m_channels(std::move(channels)) {
}

auto gs::settings_screen::title() const -> std::string_view {
	return "Settings";
}

auto gs::settings_screen::body_rect(const gse::gui::style&, const gse::vec2f viewport_size) const -> gse::gui::ui_rect {
	const float w = std::clamp(viewport_size.x() - card_margin_x * 2.f, min_card_width, max_card_width);
	const float h = std::clamp(viewport_size.y() - card_margin_y * 2.f, min_card_height, max_card_height);
	const float left = (viewport_size.x() - w) * 0.5f;
	const float top = (viewport_size.y() + h) * 0.5f;
	return gse::gui::ui_rect::from_position_size({ left, top }, { w, h });
}

auto gs::settings_screen::draw_backdrop(gse::gui::draw_context& ctx, const gse::vec2f viewport_size) const -> void {
	const gse::gui::ui_rect full =
		gse::gui::ui_rect::from_position_size({ 0.f, viewport_size.y() }, { viewport_size.x(), viewport_size.y() });

	ctx.sprites.push_back({
		.rect = full,
		.color = { 0.f, 0.f, 0.f, backdrop_alpha },
		.texture = ctx.blank_texture,
		.layer = gse::render_layer::popup,
	});

	const gse::gui::ui_rect card = body_rect(ctx.style, viewport_size);

	ctx.sprites.push_back({
		.rect = card,
		.color = ctx.style.color_menu_body,
		.texture = ctx.blank_texture,
		.layer = gse::render_layer::popup,
		.corner_radius = ctx.style.corner_radius_menu,
	});
}

auto gs::settings_screen::draw_close_button(gse::gui::draw_context& ctx, const gse::gui::ui_rect& rect, gse::gui::nav& n) const -> void {
	const gse::id close_id = gse::gui::ids::make("settings.close");
	const bool hovered = rect.contains(ctx.input.mouse_position()) && ctx.input_available();
	const gse::vec4f bg = hovered ? ctx.style.color_widget_hovered : gse::vec4f{ 0.f, 0.f, 0.f, 0.f };

	ctx.queue_sprite({
		.rect = rect,
		.color = ctx.animated_color(close_id, bg),
		.texture = ctx.blank_texture,
		.corner_radius = rect.width() * 0.5f,
	});

	const std::string glyph = "x";
	const float glyph_size = ctx.style.font_size;
	const float glyph_w = ctx.font->width(glyph, glyph_size);
	const gse::vec4f glyph_color = hovered ? ctx.style.color_icon_hovered : ctx.style.color_icon;
	ctx.queue_text({
		.font = ctx.font,
		.text = glyph,
		.position = { rect.center().x() - glyph_w * 0.5f, rect.center().y() + ctx.font->vertical_center_offset(glyph_size) },
		.scale = glyph_size,
		.color = glyph_color,
		.clip_rect = rect,
	});

	if (hovered && ctx.input.mouse_button_pressed(gse::mouse_button::button_1)) {
		n.pop();
	}
}

auto gs::settings_screen::draw_header(gse::gui::draw_context& ctx, const gse::gui::ui_rect& rect, gse::gui::nav& n) const -> void {
	const float title_size = ctx.style.font_size * 1.6f;
	const float title_left = rect.left() + ctx.style.padding * 1.5f;
	ctx.queue_text({
		.font = ctx.font,
		.text = "Settings",
		.position = { title_left, rect.center().y() + ctx.font->vertical_center_offset(title_size) },
		.scale = title_size,
		.color = ctx.style.color_text,
		.clip_rect = rect,
	});

	const float close_pad = (rect.height() - close_button_size) * 0.5f;
	const gse::gui::ui_rect close_rect = gse::gui::ui_rect::from_position_size(
		{ rect.right() - close_button_size - close_pad, rect.top() - close_pad },
		{ close_button_size, close_button_size }
	);
	draw_close_button(ctx, close_rect, n);

	const gse::gui::ui_rect separator = gse::gui::ui_rect::from_position_size(
		{ rect.left() + ctx.style.padding, rect.bottom() },
		{ rect.width() - ctx.style.padding * 2.f, 1.f }
	);
	ctx.queue_sprite({
		.rect = separator,
		.color = ctx.style.color_separator,
		.texture = ctx.blank_texture,
	});
}

auto gs::settings_screen::refresh_categories() -> void {
	if (!m_categories.empty()) {
		return;
	}
	std::unordered_set<std::string> seen;
	m_save_reg->for_each_entry([&](const gse::settings::register_settings_type& entry) {
		if (entry.category.empty()) {
			return;
		}
		if (seen.insert(entry.category).second) {
			m_categories.push_back(entry.category);
		}
	});
	std::ranges::sort(m_categories);
	if (m_selected_category.empty() && !m_categories.empty()) {
		m_selected_category = m_categories.front();
	}
}

auto gs::settings_screen::build(gse::gui::builder& ui, gse::gui::nav& n) -> void {
	refresh_categories();
	if (m_categories.empty()) {
		return;
	}

	auto& ctx = ui.ctx;
	const gse::gui::ui_rect card = ctx.current_menu->rect;

	const gse::gui::ui_rect header_rect =
		gse::gui::ui_rect::from_position_size(card.top_left(), { card.width(), header_height });
	const float body_top = card.top() - header_height;
	const float body_height = card.height() - header_height;
	const gse::gui::ui_rect sidebar_rect =
		gse::gui::ui_rect::from_position_size({ card.left(), body_top }, { sidebar_width, body_height });
	const gse::gui::ui_rect content_rect = gse::gui::ui_rect::from_position_size(
		{ card.left() + sidebar_width, body_top },
		{ card.width() - sidebar_width, body_height }
	);

	draw_header(ctx, header_rect, n);

	const gse::gui::ui_rect vertical_sep = gse::gui::ui_rect::from_position_size(
		{ card.left() + sidebar_width, body_top - ctx.style.padding },
		{ 1.f, body_height - ctx.style.padding * 2.f }
	);
	ctx.queue_sprite({
		.rect = vertical_sep,
		.color = ctx.style.color_separator,
		.texture = ctx.blank_texture,
	});

	const gse::gui::ui_rect saved_menu_rect = ctx.current_menu->rect;
	const gse::vec2f saved_cursor = ctx.layout_cursor;

	{
		ctx.current_menu->rect = sidebar_rect;
		ctx.layout_cursor = sidebar_rect.top_left();
		ctx.layout_cursor.y() -= ctx.style.padding;

		for (const auto& cat : m_categories) {
			const bool selected = m_selected_category == cat;
			if (ui.draw<gse::gui::nav_item>({
					.text = cat,
					.selected = selected,
				})) {
				m_selected_category = cat;
			}
		}
	}

	{
		ctx.current_menu->rect = content_rect;
		ctx.layout_cursor = content_rect.top_left();
		ctx.layout_cursor.y() -= ctx.style.padding * 0.5f;

		ui.scroll_region(
			{
				.id = "settings.content",
			},
			[this](gse::gui::builder& b) {
				gse::settings::panel(b, m_panel_state, m_channels, *m_save_reg, m_selected_category);
				b.ctx.layout_cursor.y() -= b.ctx.style.padding * 2.f;
			}
		);
	}

	ctx.current_menu->rect = saved_menu_rect;
	ctx.layout_cursor = saved_cursor;
}

export module gs:settings_screen;

import std;
import gse;

export namespace gs {
	class settings_screen : public gse::gui::screen {
	public:
		settings_screen(const gse::save::registry& save_reg, gse::channel_writer channels);

		auto build(gse::gui::builder& ui, gse::gui::nav& n) -> void override;

		auto title() const -> std::string_view override;

		auto body_rect(const gse::gui::style& sty, gse::vec2f viewport_size) const -> gse::gui::ui_rect override;

		auto draw_backdrop(gse::gui::draw_context& ctx, gse::vec2f viewport_size) const -> void override;

	private:
		const gse::save::registry* m_save_reg;
		gse::channel_writer m_channels;
		gse::settings::panel_state m_panel_state;

		static constexpr float panel_max_width = 520.f;
		static constexpr float dim_alpha = 0.55f;
	};
}

gs::settings_screen::settings_screen(const gse::save::registry& save_reg, gse::channel_writer channels)
	: m_save_reg(&save_reg),
	  m_channels(std::move(channels)) {
}

auto gs::settings_screen::body_rect(const gse::gui::style&, const gse::vec2f viewport_size) const -> gse::gui::ui_rect {
	const float width = std::min(panel_max_width, viewport_size.x() * 0.5f);
	return gse::gui::ui_rect::from_position_size({ 0.f, viewport_size.y() }, { width, viewport_size.y() });
}

auto gs::settings_screen::draw_backdrop(gse::gui::draw_context& ctx, const gse::vec2f viewport_size) const -> void {
	const gse::gui::ui_rect full =
		gse::gui::ui_rect::from_position_size({ 0.f, viewport_size.y() }, { viewport_size.x(), viewport_size.y() });

	ctx.sprites.push_back(
		{
			.rect = full,
			.color = { 0.f, 0.f, 0.f, dim_alpha },
			.texture = ctx.blank_texture,
			.layer = gse::render_layer::popup,
		}
	);

	const gse::gui::ui_rect panel = body_rect(ctx.style, viewport_size);
	const gse::vec4f panel_color = {
		ctx.style.color_menu_body.x() * 1.05f,
		ctx.style.color_menu_body.y() * 1.05f,
		ctx.style.color_menu_body.z() * 1.05f,
		1.0f,
	};

	ctx.sprites.push_back(
		{
			.rect = panel,
			.color = panel_color,
			.texture = ctx.blank_texture,
			.layer = gse::render_layer::popup,
		}
	);

	const gse::gui::ui_rect border =
		gse::gui::ui_rect::from_position_size({ panel.right() - 1.f, panel.top() }, { 1.f, panel.height() });
	ctx.sprites.push_back(
		{
			.rect = border,
			.color = ctx.style.color_border,
			.texture = ctx.blank_texture,
			.layer = gse::render_layer::popup,
		}
	);
}

auto gs::settings_screen::build(gse::gui::builder& ui, gse::gui::nav&) -> void {
	ui.scroll_region({ .id = "settings.body" }, [this](gse::gui::builder& b) {
		gse::settings::panel(b, m_panel_state, m_channels, *m_save_reg);
		b.ctx.layout_cursor.y() -= b.ctx.style.padding * 4.f;
	});
}

auto gs::settings_screen::title() const -> std::string_view {
	return "Settings";
}

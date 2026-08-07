export module gse.graphics:settings_screen;

import std;

import gse.core;
import gse.math;
import gse.os;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.save;
import gse.shell;

import :types;
import :ids;
import :builder;
import :interaction;
import :menu_stack;
import :styles;
import :layout_ops;
import :nav_item_widget;
import :settings;
import :render_layer;
import :symbols;

export namespace gse::gui {
	struct settings_screen_config {
		std::string title = "Settings";
		bool opaque = false;
	};

	class settings_screen : public screen {
	public:
		settings_screen(
			const gse::save::registry& save_reg,
			gse::channel_writer channels,
			settings_screen_config config = {}
		);

		auto build(
			builder& ui,
			nav& n
		) -> void override;

		auto title() const -> std::string_view override;

		auto body_rect(
			const style& sty,
			gse::vec2f viewport_size
		) const -> gse::rect_t<gse::vec2f> override;

		auto draw_backdrop(
			draw_context& ctx,
			gse::vec2f viewport_size
		) const -> void override;

	private:
		auto refresh_categories() -> void;

		auto draw_header(
			draw_context& ctx,
			const gse::rect_t<gse::vec2f>& rect,
			nav& n
		) const -> void;

		auto draw_close_button(
			draw_context& ctx,
			const gse::rect_t<gse::vec2f>& rect,
			nav& n
		) const -> void;

		auto draw_footer(
			builder& ui,
			const gse::rect_t<gse::vec2f>& rect
		) -> void;

		auto draw_scope_paths(
			draw_context& ctx,
			const gse::rect_t<gse::vec2f>& rect
		) const -> void;

		static auto draw_scope_entry(
			draw_context& ctx,
			const gse::rect_t<gse::vec2f>& rect,
			std::string_view label,
			const std::filesystem::path& path
		) -> void;

		static auto draw_footer_button(
			builder& ui,
			const gse::rect_t<gse::vec2f>& rect,
			std::string_view label,
			bool enabled,
			bool primary,
			gse::id key
		) -> bool;

		struct footer_status_cache {
			std::size_t pending = std::numeric_limits<std::size_t>::max();
			std::size_t pending_restart = 0;
			bool needs_restart = false;
			std::string text;
		};

		const gse::save::registry* m_save_reg;
		gse::channel_writer m_channels;
		std::string m_title;
		bool m_opaque;
		gse::settings::panel_state m_panel_state;
		std::string m_selected_category;
		std::vector<std::string> m_categories;
		footer_status_cache m_footer_status;

		static constexpr float backdrop_alpha = 0.55f;
	};
}

gse::gui::settings_screen::settings_screen(const gse::save::registry& save_reg, gse::channel_writer channels, settings_screen_config config)
	: m_save_reg(&save_reg), m_channels(std::move(channels)), m_title(std::move(config.title)), m_opaque(config.opaque) {
}

auto gse::gui::settings_screen::title() const -> std::string_view {
	return m_title;
}

auto gse::gui::settings_screen::body_rect(const style& sty, const gse::vec2f viewport_size) const -> gse::rect_t<gse::vec2f> {
	return gse::gui::layout::fit_card(viewport_size, sty.card_min_size, sty.card_max_size, sty.card_margin);
}

auto gse::gui::settings_screen::draw_backdrop(draw_context& ctx, const gse::vec2f viewport_size) const -> void {
	const gse::rect_t<gse::vec2f> full = gse::rect_t<gse::vec2f>::from_position_size(
		{ 0.f, viewport_size.y() },
		{ viewport_size.x(), viewport_size.y() }
	);

	ctx.sprites.push_back({
		.rect = full,
		.color = { 0.f, 0.f, 0.f, backdrop_alpha },
		.texture = ctx.blank_texture,
		.layer = gse::render_layer::popup,
	});

	const gse::rect_t<gse::vec2f> card = body_rect(ctx.style, viewport_size);

	const gse::vec4f card_color = m_opaque
		? gse::vec4f{ gse::vec3f(ctx.style.color_menu_body), 1.0f }
		: ctx.style.color_menu_body;
	ctx.sprites.push_back({
		.rect = card,
		.color = card_color,
		.texture = ctx.blank_texture,
		.layer = gse::render_layer::popup,
		.corner_radius = ctx.style.corner_radius_menu,
	});
}

auto gse::gui::settings_screen::draw_close_button(draw_context& ctx, const gse::rect_t<gse::vec2f>& rect, nav& n) const -> void {
	const auto& sty = ctx.style;
	const gse::id close_id = gse::gui::ids::make("settings.close");
	const bool hovered = ctx.hovers(rect);
	const gse::vec4f bg = hovered ? sty.color_widget_hovered : gse::vec4f{ 0.f, 0.f, 0.f, 0.f };

	ctx.queue_sprite({
		.rect = rect,
		.color = ctx.animated_color(close_id, bg),
		.texture = ctx.blank_texture,
		.corner_radius = rect.width() * 0.5f,
	});

	symbol::draw(ctx, symbol::close(), rect, {
		.color = hovered ? sty.color_icon_hovered : sty.color_icon,
		.extent = sty.icon_extent,
	});

	if (ctx.mouse_pressed_for(rect)) {
		n.pop();
	}
}

auto gse::gui::settings_screen::draw_header(draw_context& ctx, const gse::rect_t<gse::vec2f>& rect, nav& n) const -> void {
	const auto& sty = ctx.style;
	namespace lo = gse::gui::layout;

	const float title_size = sty.font_size * 1.6f;
	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = m_title,
		.position = { rect.left() + sty.padding * 1.5f, rect.center().y() + ctx.fonts.text.resolve()->vertical_center_offset(title_size) },
		.scale = title_size,
		.color = sty.color_text,
		.clip_rect = rect,
	});

	const float close_pad = (rect.height() - sty.close_button_size) * 0.5f;
	const gse::rect_t<gse::vec2f> close_band = lo::inset_per_side(rect, close_pad, close_pad, close_pad, 0.f);
	const gse::rect_t<gse::vec2f> close_rect = lo::align_in(
		close_band,
		{ sty.close_button_size, sty.close_button_size },
		lo::halign::end,
		lo::valign::center
	);
	draw_close_button(ctx, close_rect, n);

	const gse::rect_t<gse::vec2f> separator = gse::rect_t<gse::vec2f>::from_position_size(
		{ rect.left() + sty.padding, rect.bottom() },
		{ rect.width() - sty.padding * 2.f, sty.separator_thickness }
	);
	ctx.queue_sprite({
		.rect = separator,
		.color = sty.color_separator,
		.texture = ctx.blank_texture,
	});
}

auto gse::gui::settings_screen::refresh_categories() -> void {
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

auto gse::gui::settings_screen::build(builder& ui, nav& n) -> void {
	refresh_categories();
	if (m_categories.empty()) {
		return;
	}

	auto& ctx = ui.ctx;
	const auto& sty = ctx.style;
	namespace lo = gse::gui::layout;
	using spec = lo::size_spec;

	const gse::rect_t<gse::vec2f> card = ctx.current_menu->rect;

	const auto [header, body, footer] = lo::split_vertical<3>(
		card,
		{
			spec::px(sty.header_height),
			spec::flex(),
			spec::px(sty.footer_height),
		}
	);

	const auto [sidebar, content] = lo::split_horizontal<2>(
		body,
		{
			spec::px(sty.sidebar_width),
			spec::flex(),
		}
	);

	draw_header(ctx, header, n);

	const gse::rect_t<gse::vec2f> vertical_sep = gse::rect_t<gse::vec2f>::from_position_size(
		{ body.left() + sty.sidebar_width, body.top() - sty.padding },
		{ sty.separator_thickness, body.height() - sty.padding * 2.f }
	);
	ctx.queue_sprite({
		.rect = vertical_sep,
		.color = sty.color_separator,
		.texture = ctx.blank_texture,
	});

	{
		auto scope = lo::within(ctx, sidebar);
		lo::skip(ctx, sty.padding);
		for (const auto& cat : m_categories) {
			const bool selected = m_selected_category == cat;
			if (ui.draw<gse::gui::nav_item>({
					.text = cat,
					.selected = selected,
				})) {
				m_selected_category = cat;
			}
		}
		draw_scope_paths(ctx, sidebar);
	}

	{
		auto scope = lo::within(ctx, content);
		lo::skip(ctx, sty.padding * 0.5f);
		gse::settings::panel(ui, m_panel_state, m_channels, *m_save_reg, m_selected_category);
	}

	draw_footer(ui, footer);
}

auto gse::gui::settings_screen::draw_scope_entry(draw_context& ctx, const gse::rect_t<gse::vec2f>& rect, const std::string_view label, const std::filesystem::path& path) -> void {
	const auto& sty = ctx.style;
	namespace lo = gse::gui::layout;

	const float line_size = sty.font_size * 0.85f;
	const auto [name_row, dir_row] = lo::split_vertical<2>(
		rect,
		{
			lo::size_spec::flex(),
			lo::size_spec::flex(),
		}
	);

	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = std::format("{}  {}", label, path.filename().generic_display_string()),
		.position = { name_row.left(), name_row.center().y() + ctx.fonts.text.resolve()->vertical_center_offset(line_size) },
		.scale = line_size,
		.color = sty.color_text_secondary,
		.clip_rect = name_row,
	});

	ctx.queue_text({
		.font = ctx.fonts.code,
		.text = path.parent_path().generic_display_string(),
		.position = { dir_row.left(), dir_row.center().y() + ctx.fonts.code.resolve()->vertical_center_offset(line_size * 0.9f) },
		.scale = line_size * 0.9f,
		.color = sty.color_text_disabled,
		.clip_rect = dir_row,
	});

	if (ctx.mouse_pressed_for(rect)) {
		gse::shell::reveal(path);
	}
}

auto gse::gui::settings_screen::draw_scope_paths(draw_context& ctx, const gse::rect_t<gse::vec2f>& rect) const -> void {
	const auto& sty = ctx.style;
	namespace lo = gse::gui::layout;

	const std::filesystem::path& user = m_save_reg->user_path();
	const std::filesystem::path& project = m_save_reg->project_path();
	if (user.empty() && project.empty()) {
		return;
	}

	const float line_height = sty.font_size * 1.15f;
	const std::size_t scopes = static_cast<std::size_t>(!user.empty()) + static_cast<std::size_t>(!project.empty());
	const float block_height = line_height * (1.f + 2.f * static_cast<float>(scopes));

	const gse::rect_t<gse::vec2f> block = lo::inset_per_side(
		lo::align_in(rect, { rect.width(), block_height + sty.padding }, lo::halign::start, lo::valign::bottom),
		sty.padding * 0.5f,
		sty.padding,
		sty.padding * 0.5f,
		sty.padding * 1.5f
	);

	const gse::rect_t<gse::vec2f> separator = gse::rect_t<gse::vec2f>::from_position_size(
		{ block.left(), block.top() },
		{ block.width(), sty.separator_thickness }
	);
	ctx.queue_sprite({
		.rect = separator,
		.color = sty.color_separator,
		.texture = ctx.blank_texture,
	});

	gse::rect_t<gse::vec2f> cursor = gse::rect_t<gse::vec2f>::from_position_size(
		{ block.left(), block.top() },
		{ block.width(), line_height }
	);

	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = "Saved to",
		.position = { cursor.left(), cursor.center().y() + ctx.fonts.text.resolve()->vertical_center_offset(sty.font_size * 0.85f) },
		.scale = sty.font_size * 0.85f,
		.color = sty.color_text_disabled,
		.clip_rect = cursor,
	});

	auto advance = [&cursor, block](const float amount) {
		cursor = gse::rect_t<gse::vec2f>::from_position_size({ block.left(), cursor.bottom() }, { block.width(), amount });
	};

	if (!user.empty()) {
		advance(line_height * 2.f);
		draw_scope_entry(ctx, cursor, "User", user);
	}
	if (!project.empty()) {
		advance(line_height * 2.f);
		draw_scope_entry(ctx, cursor, "Project", project);
	}
}

auto gse::gui::settings_screen::draw_footer_button(builder& ui, const gse::rect_t<gse::vec2f>& rect, const std::string_view label, const bool enabled, const bool primary, const gse::id key) -> bool {
	draw_context& ctx = ui.ctx;
	const auto& sty = ctx.style;
	const auto btn = interaction::press_in_rect(ctx, ui.hot_widget_id, ui.active_widget_id, key, rect, enabled);

	gse::vec4f disabled_color = sty.color_button_background;
	disabled_color.w() = disabled_color.w() * 0.4f;

	const gse::vec4f target_color = btn.color({
		.idle = primary ? sty.color_widget_active : sty.color_button_background,
		.hot = primary ? sty.color_accent : sty.color_button_hovered,
		.active = primary ? sty.color_accent : sty.color_button_hovered,
		.disabled = disabled_color,
	});

	ctx.queue_sprite({
		.rect = rect,
		.color = ctx.animated_color(key, target_color),
		.texture = ctx.blank_texture,
		.corner_radius = sty.corner_radius,
	});

	const auto text_view = ctx.fonts.text.resolve();
	const float text_width = text_view->width(label, sty.font_size);
	const gse::vec4f text_color = enabled ? sty.color_text : sty.color_text_disabled;
	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = label,
		.position = { rect.center().x() - text_width / 2.f, rect.center().y() + text_view->vertical_center_offset(sty.font_size) },
		.scale = sty.font_size,
		.color = text_color,
		.clip_rect = rect,
	});

	return btn.activated;
}

auto gse::gui::settings_screen::draw_footer(builder& ui, const gse::rect_t<gse::vec2f>& rect) -> void {
	draw_context& ctx = ui.ctx;
	const auto& sty = ctx.style;
	namespace lo = gse::gui::layout;

	const gse::rect_t<gse::vec2f> separator = gse::rect_t<gse::vec2f>::from_position_size(
		{ rect.left() + sty.padding, rect.top() },
		{ rect.width() - sty.padding * 2.f, sty.separator_thickness }
	);
	ctx.queue_sprite({
		.rect = separator,
		.color = sty.color_separator,
		.texture = ctx.blank_texture,
	});

	const std::size_t pending = m_panel_state.pending_count();
	const std::size_t pending_restart = m_panel_state.pending_restart_count();
	const bool can_apply = pending > 0;
	const bool needs_restart = m_panel_state.needs_restart();

	if (pending != m_footer_status.pending || pending_restart != m_footer_status.pending_restart || needs_restart != m_footer_status.needs_restart) {
		m_footer_status.pending = pending;
		m_footer_status.pending_restart = pending_restart;
		m_footer_status.needs_restart = needs_restart;
		const std::string_view plural = pending == 1 ? "" : "s";
		if (pending > 0 && needs_restart) {
			m_footer_status.text = std::format("{} unsaved change{} - Restart required", pending, plural);
		}
		else if (pending_restart > 0) {
			m_footer_status.text = std::format("{} unsaved change{} (restart required)", pending, plural);
		}
		else if (pending > 0) {
			m_footer_status.text = std::format("{} unsaved change{}", pending, plural);
		}
		else if (needs_restart) {
			m_footer_status.text = "Restart required to take effect";
		}
		else {
			m_footer_status.text = "All settings saved";
		}
	}

	const gse::vec4f status_color = (can_apply || needs_restart) ? sty.color_accent : sty.color_text_secondary;
	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = m_footer_status.text,
		.position = { rect.left() + sty.padding * 1.5f, rect.center().y() + ctx.fonts.text.resolve()->vertical_center_offset(sty.font_size) },
		.scale = sty.font_size,
		.color = status_color,
		.clip_rect = rect,
	});

	const float vertical_inset = (rect.height() - sty.button_height) * 0.5f;
	const gse::rect_t<gse::vec2f> button_band = lo::inset_per_side(rect,
															 vertical_inset,
															 sty.padding,
															 vertical_inset,
															 sty.padding);
	float cursor_x = button_band.right();

	if (needs_restart) {
		cursor_x -= sty.accent_button_min_width;
		const gse::rect_t<gse::vec2f> restart_rect = gse::rect_t<gse::vec2f>::from_position_size(
			{ cursor_x, button_band.top() },
			{ sty.accent_button_min_width, sty.button_height }
		);
		if (draw_footer_button(ui, restart_rect, "Restart Now", true, true, gse::gui::ids::make("settings.footer.restart"))) {
			m_save_reg->trigger_restart();
		}
		cursor_x -= sty.button_spacing;
	}

	cursor_x -= sty.button_min_width;
	const gse::rect_t<gse::vec2f> apply_rect = gse::rect_t<gse::vec2f>::from_position_size(
		{ cursor_x, button_band.top() },
		{ sty.button_min_width, sty.button_height }
	);
	if (draw_footer_button(ui, apply_rect, "Apply", can_apply, true, gse::gui::ids::make("settings.footer.apply"))) {
		m_panel_state.apply_all(m_channels);
	}
	cursor_x -= sty.button_spacing;

	cursor_x -= sty.button_min_width;
	const gse::rect_t<gse::vec2f> discard_rect = gse::rect_t<gse::vec2f>::from_position_size(
		{ cursor_x, button_band.top() },
		{ sty.button_min_width, sty.button_height }
	);
	if (draw_footer_button(ui, discard_rect, "Discard", can_apply, false, gse::gui::ids::make("settings.footer.discard"))) {
		m_panel_state.discard_all();
	}
}

export module gse.graphics:settings_screen;

import gse.concurrency;
import gse.containers;
import gse.core;
import gse.diag;
import gse.ecs;
import gse.math;
import gse.os;
import gse.save;
import gse.shell;
import gse.time;
import std;

import :builder;
import :ids;
import :interaction;
import :layout_ops;
import :menu_stack;
import :nav_item_widget;
import :button_widget;
import :render_layer;
import :settings;
import :styles;
import :surface_paint;
import :symbols;
import :types;
import :widget_skin;

export namespace gse::gui {
	struct settings_screen_config {
		std::string title = "Settings";
		bool opaque = false;
	};

	class settings_screen : public screen {
	public:
		settings_screen(
			const save::registry& save_reg,
			settings::panel_writer channels,
			settings_screen_config config = {}
		);

		auto build(
			builder& ui,
			nav& n
		) -> void override;

		auto title() const -> std::string_view override;

		auto body_rect(
			const style& sty,
			vec2f viewport_size
		) const -> rect_t<vec2f> override;

		auto draw_backdrop(
			draw_context& ctx,
			vec2f viewport_size
		) const -> void override;

	private:
		auto refresh_categories() -> void;

		auto draw_header(
			draw_context& ctx,
			const rect_t<vec2f>& rect,
			nav& n
		) const -> void;

		auto draw_close_button(
			draw_context& ctx,
			const rect_t<vec2f>& rect,
			nav& n
		) const -> void;

		auto draw_footer(
			builder& ui,
			const rect_t<vec2f>& rect
		) -> void;

		auto scope_paths_height(
			const style& sty
		) const -> float;

		auto draw_scope_paths(
			draw_context& ctx,
			const rect_t<vec2f>& rect
		) const -> void;

		static auto draw_scope_entry(
			draw_context& ctx,
			const rect_t<vec2f>& rect,
			std::string_view label,
			const std::filesystem::path& path
		) -> void;

		static auto draw_footer_button(
			builder& ui,
			const rect_t<vec2f>& rect,
			std::string_view label,
			bool enabled,
			bool primary,
			id key
		) -> bool;

		struct footer_status_cache {
			std::size_t pending = std::numeric_limits<std::size_t>::max();
			std::size_t pending_restart = 0;
			bool needs_restart = false;
			std::string text;
		};

		const save::registry* m_save_reg;
		settings::panel_writer m_channels;
		std::string m_title;
		bool m_opaque;
		settings::panel_state m_panel_state;
		std::string m_selected_category;
		std::vector<std::string> m_categories;
		footer_status_cache m_footer_status;

		static constexpr float scope_line_scale = 1.15f;
	};
}

gse::gui::settings_screen::settings_screen(const save::registry& save_reg, settings::panel_writer channels, settings_screen_config config)
	: m_save_reg(&save_reg), m_channels(std::move(channels)), m_title(std::move(config.title)), m_opaque(config.opaque) {
}

auto gse::gui::settings_screen::title() const -> std::string_view {
	return m_title;
}

auto gse::gui::settings_screen::body_rect(const style& sty, const vec2f viewport_size) const -> rect_t<vec2f> {
	return layout::fit_card(viewport_size, sty.card_min_size, sty.card_max_size, sty.card_margin);
}

auto gse::gui::settings_screen::draw_backdrop(draw_context& ctx, const vec2f viewport_size) const -> void {
	const rect_t<vec2f> full = rect_t<vec2f>::from_position_size(
		{ 0.f, viewport_size.y() },
		{ viewport_size.x(), viewport_size.y() }
	);

	ctx.sprites.push_back({
		.rect = full,
		.color = ctx.style.modal.scrim.fill,
		.texture = ctx.blank_texture,
		.layer = render_layer::popup,
	});

	const surface& card_surface = m_opaque ? ctx.style.modal.panel : ctx.style.menu.floating;
	draw::fill_surface(ctx, body_rect(ctx.style, viewport_size), card_surface, {
		.layer = render_layer::popup,
	});
}

auto gse::gui::settings_screen::draw_close_button(draw_context& ctx, const rect_t<vec2f>& rect, nav& n) const -> void {
	const auto& sty = ctx.style;
	const id close_id = ids::make("settings.close");

	draw_icon_button(ctx, sty.window_caption, {
		.rect = rect,
		.glyph = symbol::close(),
		.state = { .hovered = ctx.hovers(rect) },
		.anim = close_id,
	});

	if (ctx.clicked_in_rect(rect)) {
		n.pop();
	}
}

auto gse::gui::settings_screen::draw_header(draw_context& ctx, const rect_t<vec2f>& rect, nav& n) const -> void {
	const auto& sty = ctx.style;
	namespace lo = gse::gui::layout;

	draw::label(ctx, rect.inset({ sty.padding * 1.5f, 0.f }), m_title, sty.screen.title, {
		.color = sty.screen.title_color,
		.clip_rect = rect,
	});

	const float close_pad = (rect.height() - sty.close_button_size) * 0.5f;
	const rect_t<vec2f> close_band = lo::inset_per_side(rect, close_pad, close_pad, close_pad, 0.f);
	const rect_t<vec2f> close_rect = lo::align_in(
		close_band,
		{ sty.close_button_size, sty.close_button_size },
		lo::halign::end,
		lo::valign::center
	);
	draw_close_button(ctx, close_rect, n);

	const rect_t<vec2f> separator = rect_t<vec2f>::from_position_size(
		{ rect.left() + sty.padding, rect.bottom() },
		{ rect.width() - sty.padding * 2.f, sty.separator_thickness }
	);
	draw::fill_surface(ctx, separator, sty.separator_line, {});
}

auto gse::gui::settings_screen::refresh_categories() -> void {
	if (!m_categories.empty()) {
		return;
	}
	std::unordered_set<std::string> seen;
	m_save_reg->for_each_entry([&](const settings::register_settings_type& entry) {
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

	const rect_t<vec2f> card = ctx.current_menu->rect;

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

	const rect_t<vec2f> vertical_sep = rect_t<vec2f>::from_position_size(
		{ body.left() + sty.sidebar_width, body.top() - sty.padding },
		{ sty.separator_thickness, body.height() - sty.padding * 2.f }
	);
	draw::fill_surface(ctx, vertical_sep, sty.separator_line, {});

	const auto [category_band, scope_band] = lo::split_vertical<2>(
		sidebar,
		{
			spec::flex(),
			spec::px(scope_paths_height(sty)),
		}
	);

	{
		const rect_t<vec2f> list_rect = lo::inset_per_side(category_band, sty.padding, 0.f, 0.f, 0.f);
		auto _ = lo::within(ctx, list_rect);
		ui.scroll_region(
			{
				.id = "settings.categories",
				.size = list_rect.size(),
			},
			[&](builder& sub) {
				for (const auto& cat : m_categories) {
					const bool selected = m_selected_category == cat;
					if (sub.draw<nav_item>({
						.text = cat,
						.selected = selected,
					})) {
						m_selected_category = cat;
					}
				}
			}
		);
	}

	draw_scope_paths(ctx, scope_band);

	{
		auto _ = lo::within(ctx, content);
		lo::skip(ctx, sty.padding * 0.5f);
		settings::panel(ui, m_panel_state, m_channels, *m_save_reg, m_selected_category);
	}

	draw_footer(ui, footer);
}

auto gse::gui::settings_screen::draw_scope_entry(draw_context& ctx, const rect_t<vec2f>& rect, const std::string_view label, const std::filesystem::path& path) -> void {
	const auto& sty = ctx.style;
	namespace lo = gse::gui::layout;

	const id entry_id = ids::make(label);
	const bool hovered = ctx.hovers(rect);

	const interaction::press entry_state{
		.hovered = hovered,
	};

	draw::fill_surface(ctx, rect, entry_state.select(sty.nav_item.surfaces), {
		.anim = entry_id,
	});

	const auto [icon_column, text_column] = lo::split_horizontal<2>(
		rect,
		{
			lo::size_spec::px(sty.icon_extent + sty.padding * 0.75f),
			lo::size_spec::flex(),
		}
	);

	symbol::draw(
		ctx,
		symbol::folder(),
		lo::align_in(icon_column, { sty.icon_extent, sty.icon_extent }, lo::halign::center, lo::valign::center),
		{
			.color = hovered ? sty.color_icon_hovered : sty.color_folder,
			.extent = sty.icon_extent,
		}
	);

	const auto [name_row, dir_row] = lo::split_vertical<2>(
		text_column,
		{
			lo::size_spec::flex(),
			lo::size_spec::flex(),
		}
	);

	draw::label(
		ctx,
		name_row,
		ctx.intern(std::format("{}  {}", label, path.filename().generic_display_string())),
		sty.screen.entry_name,
		{
			.color = hovered ? sty.color_text : sty.color_text_secondary,
			.clip_rect = name_row,
		}
	);

	const float dir_size = sty.font_size * sty.screen.entry_path.size_mult;
	const std::string dir_text = path.parent_path().generic_display_string();
	const vec4f link_color = hovered ? sty.color_accent : sty.color_text_disabled;
	const auto code_view = ctx.fonts.code.resolve();
	const float dir_top = dir_row.center().y() + code_view->vertical_center_offset(dir_size);

	ctx.queue_text({
		.font = ctx.fonts.code,
		.text = dir_text,
		.position = { dir_row.left(), dir_top },
		.scale = dir_size,
		.color = link_color,
		.clip_rect = dir_row,
	});

	ctx.queue_sprite({
		.rect = rect_t<vec2f>::from_position_size(
			{ dir_row.left(), dir_top - code_view->ascender_height(dir_size) - dir_size * 0.28f },
			{ std::min(code_view->width(dir_text, dir_size), dir_row.width()), sty.separator_thickness }
		),
		.color = link_color,
		.texture = ctx.blank_texture,
		.clip_rect = dir_row,
	});

	if (hovered) {
		ctx.set_tooltip(entry_id, std::format("{}\nClick to reveal in file browser", path.generic_display_string()));
	}

	if (ctx.clicked_in_rect(rect)) {
		shell::reveal(path);
	}
}

auto gse::gui::settings_screen::scope_paths_height(const style& sty) const -> float {
	const int scopes = static_cast<int>(!m_save_reg->user_path().empty()) + static_cast<int>(!m_save_reg->project_path().empty());
	if (scopes == 0) {
		return 0.f;
	}
	return sty.font_size * scope_line_scale * (1.f + 2.f * static_cast<float>(scopes)) + sty.padding * 2.f;
}

auto gse::gui::settings_screen::draw_scope_paths(draw_context& ctx, const rect_t<vec2f>& rect) const -> void {
	const auto& sty = ctx.style;
	namespace lo = gse::gui::layout;

	const std::filesystem::path& user = m_save_reg->user_path();
	const std::filesystem::path& project = m_save_reg->project_path();
	if (user.empty() && project.empty()) {
		return;
	}

	const ids::scope _("settings.scope");
	const float line_height = sty.font_size * scope_line_scale;

	const rect_t<vec2f> block = lo::inset_per_side(
		rect,
		sty.padding,
		sty.padding * 0.5f,
		sty.padding,
		sty.padding * 0.5f
	);

	const rect_t<vec2f> separator = rect_t<vec2f>::from_position_size(
		{ block.left(), rect.top() },
		{ block.width(), sty.separator_thickness }
	);
	draw::fill_surface(ctx, separator, sty.separator_line, {});

	rect_t<vec2f> cursor = rect_t<vec2f>::from_position_size(
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
		cursor = rect_t<vec2f>::from_position_size({ block.left(), cursor.bottom() }, { block.width(), amount });
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

auto gse::gui::settings_screen::draw_footer_button(builder& ui, const rect_t<vec2f>& rect, const std::string_view label, const bool enabled, const bool primary, const id key) -> bool {
	return draw::button_in_rect(ui.ctx, {
		.rect = rect,
		.label = label,
		.widget_id = key,
		.role = primary ? button_role::accent : button_role::standard,
		.enabled = enabled,
	}, ui.hot_widget_id, ui.active_widget_id);
}

auto gse::gui::settings_screen::draw_footer(builder& ui, const rect_t<vec2f>& rect) -> void {
	draw_context& ctx = ui.ctx;
	const auto& sty = ctx.style;
	namespace lo = gse::gui::layout;

	const rect_t<vec2f> separator = rect_t<vec2f>::from_position_size(
		{ rect.left() + sty.padding, rect.top() },
		{ rect.width() - sty.padding * 2.f, sty.separator_thickness }
	);
	draw::fill_surface(ctx, separator, sty.separator_line, {});

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

	const vec4f status_color = (can_apply || needs_restart) ? sty.color_accent : sty.color_text_secondary;
	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = m_footer_status.text,
		.position = { rect.left() + sty.padding * 1.5f, rect.center().y() + ctx.fonts.text.resolve()->vertical_center_offset(sty.font_size) },
		.scale = sty.font_size,
		.color = status_color,
		.clip_rect = rect,
	});

	const float vertical_inset = (rect.height() - sty.button_height) * 0.5f;
	const rect_t<vec2f> button_band = lo::inset_per_side(rect,
		vertical_inset,
		sty.padding,
		vertical_inset,
		sty.padding);
	float cursor_x = button_band.right();

	if (needs_restart) {
		cursor_x -= sty.accent_button_min_width;
		const rect_t<vec2f> restart_rect = rect_t<vec2f>::from_position_size(
			{ cursor_x, button_band.top() },
			{ sty.accent_button_min_width, sty.button_height }
		);
		if (draw_footer_button(ui, restart_rect, "Restart Now", true, true, ids::make("settings.footer.restart"))) {
			m_channels.push<settings::override_request>({
				.op = settings::override_op::restart,
			});
		}
		cursor_x -= sty.button_spacing;
	}

	cursor_x -= sty.button_min_width;
	const rect_t<vec2f> apply_rect = rect_t<vec2f>::from_position_size(
		{ cursor_x, button_band.top() },
		{ sty.button_min_width, sty.button_height }
	);
	if (draw_footer_button(ui, apply_rect, "Apply", can_apply, true, ids::make("settings.footer.apply"))) {
		m_panel_state.apply_all(m_channels);
	}
	cursor_x -= sty.button_spacing;

	cursor_x -= sty.button_min_width;
	const rect_t<vec2f> discard_rect = rect_t<vec2f>::from_position_size(
		{ cursor_x, button_band.top() },
		{ sty.button_min_width, sty.button_height }
	);
	if (draw_footer_button(ui, discard_rect, "Discard", can_apply, false, ids::make("settings.footer.discard"))) {
		m_panel_state.discard_all(m_channels);
	}
}
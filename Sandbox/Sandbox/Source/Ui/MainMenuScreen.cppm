export module sandbox:main_menu_screen;

import std;
import gse;

import :network_screen;
import :world_catalog;
import :world_tile;

export namespace sandbox {
	class main_menu_screen : public gse::gui::screen {
	public:
		main_menu_screen(
			gse::shared_view<gse::world_system::data> world,
			gse::shared_view<gse::network::data> net,
			const gse::save::registry& save_reg,
			gse::channel_write<gse::activate_scene_request, gse::deactivate_active_scene_request, gse::reset_active_scene_request, gse::gui::push_screen_request, gse::gui::pop_screen_request, gse::network::connect_request, gse::network::refresh_servers_request, gse::network::refresh_server_info_request, gse::network::ping_request, gse::network::remember_server_request, gse::network::forget_server_request, gse::settings::change_request, gse::settings::override_request, gse::gui::popout_toggle> channels
		);

		auto build(
			gse::gui::builder& ui,
			gse::gui::nav& n
		) -> void override;

		auto title() const -> std::string_view override;

		auto on_push() -> void override;

		auto dismissable() const -> bool override;

		auto body_rect(
			const gse::gui::style& sty,
			gse::vec2f viewport_size
		) const -> gse::rect_t<gse::vec2f> override;

		auto draw_backdrop(
			gse::gui::draw_context& ctx,
			gse::vec2f viewport_size
		) const -> void override;

	private:
		[[nodiscard]] auto eased_progress() const -> float;

		[[nodiscard]] auto live_scene() const -> std::string_view;

		[[nodiscard]] auto scene_id_for(
			std::string_view name
		) const -> gse::id;

		auto draw_header(
			gse::gui::builder& ui,
			const gse::rect_t<gse::vec2f>& rect
		) -> void;

		auto draw_sheet(
			gse::gui::builder& ui,
			const gse::rect_t<gse::vec2f>& rect
		) -> void;

		auto draw_group(
			gse::gui::builder& ui,
			world_group group,
			const gse::rect_t<gse::vec2f>& rect
		) -> void;

		auto draw_detail(
			gse::gui::builder& ui,
			gse::gui::nav& n,
			const gse::rect_t<gse::vec2f>& rect
		) -> void;

		auto refresh_status() -> void;

		struct status_cache {
			std::size_t worlds = std::numeric_limits<std::size_t>::max();
			gse::network::client::state connection = gse::network::client::state::disconnected;
			std::string text;
		};

		gse::shared_view<gse::world_system::data> m_world;
		gse::shared_view<gse::network::data> m_net;
		const gse::save::registry* m_save_reg;
		gse::channel_write<gse::activate_scene_request, gse::deactivate_active_scene_request, gse::reset_active_scene_request, gse::gui::push_screen_request, gse::gui::pop_screen_request, gse::network::connect_request, gse::network::refresh_servers_request, gse::network::refresh_server_info_request, gse::network::ping_request, gse::network::remember_server_request, gse::network::forget_server_request, gse::settings::change_request, gse::settings::override_request, gse::gui::popout_toggle> m_channels;
		gse::clock m_opened_at;
		std::size_t m_selected = 0;
		status_cache m_status;
	};
}

namespace sandbox {
	constexpr float parity_tile_scale = 0.52f;
	constexpr float group_heading_gap = 0.5f;

	struct detail_metrics {
		float hero = 0.f;
		float blurb = 0.f;

		[[nodiscard]] auto total() const -> float;
	};

	[[nodiscard]] auto detail_metrics_of(
		const gse::gui::draw_context& ctx
	) -> detail_metrics;
}

auto sandbox::detail_metrics::total() const -> float {
	return hero + blurb;
}

auto sandbox::detail_metrics_of(const gse::gui::draw_context& ctx) -> detail_metrics {
	const auto& sty = ctx.style;
	const auto face = ctx.fonts.text.resolve();
	return {
		.hero = face->line_height(sty.font_size * sty.screen.title.size_mult),
		.blurb = face->line_height(sty.font_size * sty.screen.note.size_mult),
	};
}

sandbox::main_menu_screen::main_menu_screen(const gse::shared_view<gse::world_system::data> world, const gse::shared_view<gse::network::data> net, const gse::save::registry& save_reg, gse::channel_write<gse::activate_scene_request, gse::deactivate_active_scene_request, gse::reset_active_scene_request, gse::gui::push_screen_request, gse::gui::pop_screen_request, gse::network::connect_request, gse::network::refresh_servers_request, gse::network::refresh_server_info_request, gse::network::ping_request, gse::network::remember_server_request, gse::network::forget_server_request, gse::settings::change_request, gse::settings::override_request, gse::gui::popout_toggle> channels)
	: m_world(world), m_net(net), m_save_reg(&save_reg), m_channels(std::move(channels)) {
}

auto sandbox::main_menu_screen::title() const -> std::string_view {
	return "Sandbox";
}

auto sandbox::main_menu_screen::dismissable() const -> bool {
	return m_world.active_scene.has_value();
}

auto sandbox::main_menu_screen::on_push() -> void {
	m_opened_at.reset();

	const std::string_view live = live_scene();
	if (live.empty()) {
		return;
	}

	const auto entries = world_catalog();
	const auto found = std::ranges::find(entries, live, &world_entry::name);
	if (found != entries.end()) {
		m_selected = static_cast<std::size_t>(std::ranges::distance(entries.begin(), found));
	}
}

auto sandbox::main_menu_screen::eased_progress() const -> float {
	const gse::time settle_duration = gse::milliseconds(220.f);
	const float t = std::clamp(m_opened_at.elapsed<float>() / settle_duration, 0.f, 1.f);
	const float inv = 1.f - t;
	return 1.f - inv * inv * inv;
}

auto sandbox::main_menu_screen::live_scene() const -> std::string_view {
	return m_world.active_scene.has_value() ? m_world.active_scene->tag() : std::string_view{};
}

auto sandbox::main_menu_screen::scene_id_for(const std::string_view name) const -> gse::id {
	const auto& ids = m_world.scene_ids;
	const auto found = std::ranges::find(ids, name, &gse::id::tag);
	return found == std::ranges::end(ids) ? gse::id{} : *found;
}

auto sandbox::main_menu_screen::body_rect(const gse::gui::style& sty, const gse::vec2f viewport_size) const -> gse::rect_t<gse::vec2f> {
	const auto full = gse::rect_t<gse::vec2f>::from_position_size(
		{ 0.f, viewport_size.y() },
		viewport_size
	);
	return m_world.active_scene.has_value() ? full.inset(sty.card_margin) : full;
}

auto sandbox::main_menu_screen::draw_backdrop(gse::gui::draw_context& ctx, const gse::vec2f viewport_size) const -> void {
	const auto& sty = ctx.style;

	if (m_world.active_scene.has_value()) {
		const gse::vec4f scrim = sty.modal.scrim.fill;
		ctx.sprites.push_back({
			.rect = gse::rect_t<gse::vec2f>::from_position_size({ 0.f, viewport_size.y() }, viewport_size),
			.color = { gse::vec3f(scrim), scrim.w() * eased_progress() },
			.texture = ctx.blank_texture,
			.layer = gse::render_layer::popup,
		});
	}

	gse::gui::draw::fill_surface(ctx, body_rect(sty, viewport_size), sty.screen.backdrop, {});
}

auto sandbox::main_menu_screen::refresh_status() -> void {
	const std::size_t worlds = world_catalog().size();
	const gse::network::client::state connection = m_net.connection_state;

	if (worlds == m_status.worlds && connection == m_status.connection) {
		return;
	}

	m_status.worlds = worlds;
	m_status.connection = connection;
	m_status.text = connection == gse::network::client::state::connected
		? std::format("{} worlds, joined", worlds)
		: std::format("{} worlds", worlds);
}

auto sandbox::main_menu_screen::draw_header(gse::gui::builder& ui, const gse::rect_t<gse::vec2f>& rect) -> void {
	namespace gui = gse::gui;
	namespace lo = gse::gui::layout;

	auto& ctx = ui.ctx;
	const auto& sty = ctx.style;

	refresh_status();

	const auto quit = lo::align_in(
		rect,
		{ sty.button_min_width, sty.button_height },
		lo::halign::end,
		lo::valign::center
	);

	gui::draw::label(ctx, rect, title(), sty.screen.caption, {
		.color = sty.screen.caption_color,
		.clip_rect = rect,
	});

	const auto status_band = lo::inset_per_side(rect, 0.f, quit.width() + sty.padding, 0.f, 0.f);

	gui::label_spec status_spec = sty.screen.entry_path;
	status_spec.h = lo::halign::end;

	gui::draw::label(ctx, status_band, m_status.text, status_spec, {
		.color = sty.color_text_secondary,
		.clip_rect = status_band,
	});

	if (ui.draw<gui::button>({
			.text = "Quit",
			.rect = quit,
			.key = "##sandbox_quit",
			.glyph = gui::symbol::close(),
		})) {
		gse::shutdown();
	}

	gui::draw::fill_surface(
		ctx,
		gse::rect_t<gse::vec2f>::from_position_size(
			{ rect.left(), rect.bottom() + sty.separator_thickness },
			{ rect.width(), sty.separator_thickness }
		),
		sty.separator_line,
		{}
	);
}

auto sandbox::main_menu_screen::draw_group(gse::gui::builder& ui, const world_group group, const gse::rect_t<gse::vec2f>& rect) -> void {
	namespace gui = gse::gui;

	auto& ctx = ui.ctx;
	const auto& sty = ctx.style;

	const world_group_info info = gse::annotation_from_enum(group, world_group_info{});
	const auto face = ctx.fonts.text.resolve();
	const float heading_height = face->line_height(sty.font_size * sty.section.title.size_mult);

	const auto heading = gse::rect_t<gse::vec2f>::from_position_size(
		{ rect.left(), ctx.layout_cursor.y() },
		{ rect.width(), heading_height }
	);

	gui::draw::label(ctx, heading, info.heading, sty.section.title, {
		.color = sty.section.title_color,
		.clip_rect = rect,
	});

	ctx.layout_cursor.y() -= heading_height + sty.padding * group_heading_gap;

	const float extent = group == world_group::parity ? sty.preview_height * parity_tile_scale : sty.preview_height;
	const auto columns = std::max<std::size_t>(1, static_cast<std::size_t>(rect.width() / extent));
	const float tile_width = rect.width() / static_cast<float>(columns);
	const float top = ctx.layout_cursor.y();
	const std::string_view live = live_scene();

	std::size_t placed = 0;
	for (const auto& [index, entry] : std::views::enumerate(world_catalog())) {
		if (entry.group != group) {
			continue;
		}

		const auto column = placed % columns;
		const auto row = placed / columns;
		++placed;

		const auto tile = gse::rect_t<gse::vec2f>::from_position_size(
			{ rect.left() + static_cast<float>(column) * tile_width, top - static_cast<float>(row) * extent },
			{ tile_width, extent }
		);

		if (ui.draw<sandbox::ui::world_tile>({
				.rect = tile,
				.label = entry.label,
				.key = entry.name,
				.diagram = entry.diagram,
				.selected = static_cast<std::size_t>(index) == m_selected,
				.live = entry.name == live,
			})) {
			m_selected = static_cast<std::size_t>(index);
		}
	}

	const auto rows = (placed + columns - 1) / columns;
	ctx.layout_cursor.y() = top - static_cast<float>(rows) * extent - sty.padding;
}

auto sandbox::main_menu_screen::draw_sheet(gse::gui::builder& ui, const gse::rect_t<gse::vec2f>& rect) -> void {
	namespace gui = gse::gui;

	auto& ctx = ui.ctx;

	const gui::scroll_config config{};

	ctx.layout_cursor = rect.top_left();

	const gui::scroll_handle region = ui.scroll_region({
		.id = "##sandbox_sheet",
		.size = rect.size(),
		.config = config,
	});
	if (!region.valid()) {
		return;
	}

	const gse::rect_t<gse::vec2f> grid = gse::rect_t<gse::vec2f>::from_position_size(
		region.visible_rect().top_left(),
		{ std::max(0.f, region.visible_rect().width() - config.scrollbar_width), region.visible_rect().height() }
	);

	for (const world_group group : gse::enum_values<world_group>()) {
		draw_group(ui, group, grid);
	}
}

auto sandbox::main_menu_screen::draw_detail(gse::gui::builder& ui, gse::gui::nav& n, const gse::rect_t<gse::vec2f>& rect) -> void {
	namespace gui = gse::gui;
	namespace lo = gse::gui::layout;

	auto& ctx = ui.ctx;
	const auto& sty = ctx.style;

	const world_entry& entry = world_catalog()[m_selected];
	const bool selection_live = entry.name == live_scene();

	gui::draw::fill_surface(
		ctx,
		gse::rect_t<gse::vec2f>::from_position_size(
			rect.top_left(),
			{ rect.width(), sty.separator_thickness }
		),
		sty.separator_line,
		{}
	);

	const float gap = sty.button_spacing;
	const float secondary_width = sty.button_min_width;
	const float primary_width = sty.accent_button_min_width;
	const float secondary_count = selection_live ? 4.f : 2.f;
	const float row_width = primary_width + (secondary_width + gap) * secondary_count;
	const auto row = lo::align_in(rect, { row_width, sty.button_height }, lo::halign::end, lo::valign::center);

	const detail_metrics metrics = detail_metrics_of(ctx);
	const auto band = lo::inset_per_side(rect, sty.padding, row_width + gap, sty.padding, 0.f);
	const auto text_block = lo::align_in(
		band,
		{ band.width(), metrics.total() },
		lo::halign::start,
		lo::valign::center
	);

	const auto [hero, blurb] = lo::split_vertical<2>(
		text_block,
		{
			lo::size_spec::px(metrics.hero),
			lo::size_spec::px(metrics.blurb),
		}
	);

	gui::draw::label(ctx, hero, entry.name, sty.screen.title, {
		.color = sty.screen.title_color,
		.clip_rect = rect,
	});

	gui::draw::label(ctx, blurb, entry.blurb, sty.screen.note, {
		.color = sty.color_text_secondary,
		.clip_rect = rect,
	});

	float x = row.left();
	const auto slot = [&x, &row, gap](const float width) -> gse::rect_t<gse::vec2f> {
		const auto placed = gse::rect_t<gse::vec2f>::from_position_size(
			{ x, row.top() },
			{ width, row.height() }
		);
		x += width + gap;
		return placed;
	};

	if (ui.draw<gui::button>({
			.text = selection_live ? "Resume" : "Play",
			.rect = slot(primary_width),
			.key = "##sandbox_play",
			.glyph = gui::symbol::play(),
			.role = gui::button_role::accent,
		})) {
		if (!selection_live) {
			m_channels.push<gse::activate_scene_request>({
				.scene_id = scene_id_for(entry.name),
			});
		}
		n.pop();
	}

	if (selection_live && ui.draw<gui::button>({
			.text = "Restart",
			.rect = slot(secondary_width),
			.key = "##sandbox_restart",
			.glyph = gui::symbol::restart(),
		})) {
		m_channels.push<gse::reset_active_scene_request>({});
	}

	if (selection_live && ui.draw<gui::button>({
			.text = "Stop",
			.rect = slot(secondary_width),
			.key = "##sandbox_stop",
			.glyph = gui::symbol::stop(),
			.role = gui::button_role::danger,
		})) {
		m_channels.push<gse::deactivate_active_scene_request>({});
	}

	if (ui.draw<gui::button>({
			.text = "Network",
			.rect = slot(secondary_width),
			.key = "##sandbox_network",
			.glyph = gui::symbol::terminal(),
		})) {
		n.push<network_screen>(m_net, m_channels);
	}

	if (ui.draw<gui::button>({
			.text = "Settings",
			.rect = slot(secondary_width),
			.key = "##sandbox_settings",
			.glyph = gui::symbol::gear(),
		})) {
		n.push<gui::settings_screen>(
			std::cref(*m_save_reg),
			gse::settings::panel_writer(m_channels),
			gui::settings_screen_config{
				.title = "Engine Settings",
				.opaque = true,
			}
		);
	}
}

auto sandbox::main_menu_screen::build(gse::gui::builder& ui, gse::gui::nav& n) -> void {
	namespace lo = gse::gui::layout;
	using spec = lo::size_spec;

	auto& ctx = ui.ctx;
	const auto& sty = ctx.style;

	const auto content = ctx.current_menu->rect.inset({ sty.padding, sty.padding });

	const float detail_height = detail_metrics_of(ctx).total() + sty.padding * 2.f;

	const auto [header, sheet, detail] = lo::split_vertical<3>(
		content,
		{
			spec::px(sty.header_height),
			spec::flex(),
			spec::px(detail_height),
		}
	);

	draw_header(ui, header);
	draw_sheet(ui, sheet);
	draw_detail(ui, n, detail);
}

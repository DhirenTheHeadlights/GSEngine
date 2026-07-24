export module gse.ide.app:chrome;

import std;
import gse;

import gse.ide.workspace;
import gse.ide.git;
import gse.ide.build;
import gse.ide.search;

import :search_screen;

namespace gse::ide {
	auto rebuild_glyph() -> std::span<const gse::gui::symbol::stroke>;

	auto git_status_color(
		git::file_status status
	) -> gse::vec4f;

	struct toggle_settings_request {};

	struct quick_search_state {
		search::query_driver driver;
		gse::gui::text_input_state input;
	};

	class editor_screen : public gse::gui::screen {
	public:
		editor_screen(
			gse::channel_writer channels,
			const search::index_state* index
		);

		auto build(
			gse::gui::builder& ui,
			gse::gui::nav& n
		) -> void override;

		auto title() const -> std::string_view override;

		auto dismissable() const -> bool override;

		auto captures_input() const -> bool override;

		auto draw_backdrop(
			gse::gui::draw_context& ctx,
			gse::vec2f viewport_size
		) const -> void override;

	private:
		auto chrome_button(
			gse::gui::builder& ui,
			const gse::rectf& rect,
			const std::string& key,
			std::span<const gse::gui::symbol::stroke> glyph,
			gse::vec4f hover_color
		) -> bool;

		gse::channel_writer m_channels;
		const search::index_state* m_index = nullptr;
		std::optional<std::string> m_loc_label;
		std::uint64_t m_loc_value = 0;
	};

	auto draw_search_bar(
		gse::gui::builder& ui,
		quick_search_state& state,
		const search::index_state* index,
		gse::channel_writer channels,
		const gse::rectf& search_rect,
		std::string_view id_key
	) -> void;

	auto draw_explorer_panel(
		gse::gui::builder& ui,
		workspace::data& ws,
		quick_search_state& search,
		const search::index_state* index,
		gse::channel_writer channels,
		const git::status_map* git_status
	) -> void;

	auto spinner_rotation() -> gse::angle;

	auto draw_spinner(
		const gse::gui::draw_context& ctx,
		const rectf& rect,
		gse::vec4f color,
		gse::angle rotation
	) -> void;
}

gse::ide::editor_screen::editor_screen(gse::channel_writer channels, const search::index_state* index)
	: m_channels(std::move(channels)), m_index(index) {
}

auto gse::ide::editor_screen::title() const -> std::string_view {
	return "GSEditor";
}

auto gse::ide::editor_screen::dismissable() const -> bool {
	return false;
}

auto gse::ide::editor_screen::captures_input() const -> bool {
	return false;
}

auto gse::ide::editor_screen::draw_backdrop(gse::gui::draw_context&, gse::vec2f) const -> void {
}

auto gse::ide::editor_screen::chrome_button(gse::gui::builder& ui, const gse::rectf& rect, const std::string& key, const std::span<const gse::gui::symbol::stroke> glyph, const gse::vec4f hover_color) -> bool {
	const auto& ctx = ui.ctx;
	const gse::id widget_id = gse::gui::ids::make(key);

	const bool hovered = rect.contains(ctx.input.mouse_position()) && ctx.input_available();
	const bool released = ctx.input.mouse_button_released(gse::mouse_button::button_1);

	gse::gui::interaction::mark_hot(ui.hot_widget_id, widget_id, hovered);
	const bool activated = gse::gui::interaction::activate_on_click(ui.active_widget_id, widget_id, hovered, ctx.mouse_pressed_for(rect), released);

	const bool engaged = ui.active_widget_id == widget_id || ui.hot_widget_id == widget_id;

	ctx.queue_sprite({
		.rect = rect,
		.color = engaged ? hover_color : ctx.style.color_input_background,
		.texture = ctx.blank_texture,
	});

	gse::gui::symbol::draw(ctx, glyph, rect, {
		.color = ctx.style.color_text,
		.scale = ctx.style.font_size / rect.height() * 1.2f,
	});

	return activated;
}

auto gse::ide::rebuild_glyph() -> std::span<const gse::gui::symbol::stroke> {
	static const std::array<gse::gui::symbol::stroke, 12> data = [] {
		std::array<gse::gui::symbol::stroke, 12> strokes{};
		constexpr int segments = 10;
		constexpr float pi = std::numbers::pi_v<float>;
		constexpr float radius = 0.27f;
		const gse::angle begin = gse::radians(0.4f * pi);
		const gse::angle end = gse::radians(2.f * pi);
		const gse::vec2f center{ 0.5f, 0.5f };
		std::size_t index = 0;
		gse::vec2f previous{};
		for (int i = 0; i <= segments; ++i) {
			const float t = static_cast<float>(i) / static_cast<float>(segments);
			const gse::angle angle = begin + (end - begin) * t;
			const gse::vec2f point{ center.x() + gse::cos(angle) * radius, center.y() + gse::sin(angle) * radius };
			if (i > 0) {
				strokes[index++] = { previous, point };
			}
			previous = point;
		}
		const gse::vec2f tangent{ -gse::sin(end), gse::cos(end) };
		const gse::vec2f normal{ gse::cos(end), gse::sin(end) };
		strokes[index++] = { previous, gse::vec2f{ previous.x() - tangent.x() * 0.12f + normal.x() * 0.07f, previous.y() - tangent.y() * 0.12f + normal.y() * 0.07f } };
		strokes[index++] = { previous, gse::vec2f{ previous.x() - tangent.x() * 0.12f - normal.x() * 0.07f, previous.y() - tangent.y() * 0.12f - normal.y() * 0.07f } };
		return strokes;
	}();
	return data;
}

auto gse::ide::editor_screen::build(gse::gui::builder& ui, gse::gui::nav& n) -> void {
	const auto& ctx = ui.ctx;
	if (!ctx.current_menu) {
		return;
	}

	if ((ctx.input.key_held(gse::key::left_control) || ctx.input.key_held(gse::key::right_control)) && ctx.input.key_pressed(gse::key::f)) {
		n.push<search_screen>(m_channels, m_index);
	}

	const gse::rectf screen_rect = ctx.current_menu->rect;
	const float bar_height = ctx.style.title_bar_height;

	const gse::rectf bar_rect = gse::rectf::from_position_size(
		{ screen_rect.left(), screen_rect.top() },
		{ screen_rect.width(), bar_height }
	);

	ctx.queue_sprite({
		.rect = bar_rect,
		.color = ctx.style.color_input_background,
		.texture = ctx.blank_texture,
	});

	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = std::string(title()),
		.position = { bar_rect.left() + ctx.style.padding, bar_rect.center().y() + ctx.fonts.text->vertical_center_offset(ctx.style.font_size) },
		.scale = ctx.style.font_size,
		.color = ctx.style.color_text,
		.clip_rect = bar_rect,
	});

	if (m_index) {
		if (const std::uint64_t loc = m_index->cpp_loc.load(std::memory_order_acquire)) {
			if (m_loc_value != loc) {
				m_loc_value = loc;
				m_loc_label = std::format("{}k LOC", (loc + 500) / 1000);
			}
			const float title_w = ctx.fonts.text->width(title(), ctx.style.font_size);
			const float badge_pad = ctx.style.padding * 0.5f;
			const float badge_height = ctx.style.font_size + ctx.style.padding * 0.5f;
			const float badge_width = ctx.fonts.text->width(*m_loc_label, ctx.style.font_size) + badge_pad * 2.f;
			const gse::rectf badge_rect = gse::rectf::from_position_size(
				{ bar_rect.left() + ctx.style.padding + title_w + ctx.style.padding, bar_rect.center().y() + badge_height * 0.5f },
				{ badge_width, badge_height }
			);
			ctx.queue_sprite({
				.rect = badge_rect,
				.color = ctx.style.color_accent_dim,
				.texture = ctx.blank_texture,
				.corner_radius = badge_height * 0.5f,
			});
			ctx.queue_text({
				.font = ctx.fonts.text,
				.text = *m_loc_label,
				.position = { badge_rect.left() + badge_pad, badge_rect.center().y() + ctx.fonts.text->vertical_center_offset(ctx.style.font_size) },
				.scale = ctx.style.font_size,
				.color = ctx.style.color_accent,
				.clip_rect = badge_rect,
			});
		}
	}

	const float button_w = bar_height * 1.5f;
	auto button_slot = [&](const int from_right) -> gse::rectf {
		return gse::rectf::from_position_size(
			{ bar_rect.right() - button_w * static_cast<float>(from_right + 1), bar_rect.top() },
			{ button_w, bar_height }
		);
	};

	if (chrome_button(ui, button_slot(0), "##chrome_close", gse::gui::symbol::close(), gse::vec4f{ 0.78f, 0.22f, 0.22f, 1.f })) {
		gse::shutdown();
	}
	if (chrome_button(ui, button_slot(1), "##chrome_max", gse::gui::symbol::maximize(), ctx.style.color_widget_hovered)) {
		m_channels.push<gse::window_toggle_maximize_request>({});
	}
	if (chrome_button(ui, button_slot(2), "##chrome_min", gse::gui::symbol::minimize(), ctx.style.color_widget_hovered)) {
		m_channels.push<gse::window_minimize_request>({});
	}
	if (chrome_button(ui, button_slot(3), "##chrome_settings", gse::gui::symbol::gear(), ctx.style.color_widget_hovered)) {
		m_channels.push<toggle_settings_request>({});
	}
	if (chrome_button(ui, button_slot(4), "##chrome_rebuild", rebuild_glyph(), ctx.style.color_widget_hovered)) {
		m_channels.push<build_runner::build_request>({
			.target = build_runner::build_target::editor,
		});
	}

	if (m_index) {
		std::string status;
		std::string status_tooltip;
		bool spinning = false;
		gse::vec4f pill_color = ctx.style.color_input_background;
		gse::vec4f pill_fg = ctx.style.color_text_secondary;
		const search::index_phase phase = m_index->phase.load(std::memory_order_acquire);
		if (phase != search::index_phase::idle) {
			const search::index_phase_info info = gse::annotation_from_enum<search::index_phase_info>(phase, {
				.label = "Unknown indexing stage",
				.detail = "No explanation was recorded for this indexing stage.",
			});
			const std::size_t total = m_index->progress_total.load(std::memory_order_acquire);
			const std::size_t done = std::min(m_index->progress_done.load(std::memory_order_acquire), total);
			const std::int64_t started_ns = m_index->phase_started_ns.load(std::memory_order_acquire);
			const std::int64_t now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
				std::chrono::steady_clock::now().time_since_epoch()
			).count();
			const float elapsed_seconds = started_ns == 0 ? 0.f : static_cast<float>(now_ns - started_ns) / 1'000'000'000.f;
			status = std::string(info.label);
			if (total > 0) {
				status += std::format(" {}/{}", done, total);
			}
			status += elapsed_seconds < 10.f
				? std::format(" · {:.1f}s", elapsed_seconds)
				: std::format(" · {:.0f}s", elapsed_seconds);
			status_tooltip = std::format(
				"{} — {} Progress: {} of {} items. Time in this stage: {:.1f} seconds.",
				info.label,
				info.detail,
				done,
				total,
				elapsed_seconds
			);
			spinning = true;
			pill_color = ctx.style.color_accent_dim;
			pill_fg = ctx.style.color_accent;
		}
		else if (const std::size_t syms = m_index->symbol_count.load(std::memory_order_acquire); syms > 0) {
			status = syms >= 1000 ? std::format("{}k symbols", (syms + 500) / 1000) : std::format("{} symbols", syms);
			status_tooltip = std::format("Semantic index ready. {} symbols are available for highlighting, hover, and navigation.", syms);
		}
		if (!status.empty()) {
			const float badge_h = ctx.style.font_size + ctx.style.padding * 0.5f;
			const float pad = ctx.style.padding * 0.6f;
			const float spin_w = spinning ? ctx.style.font_size : 0.f;
			const float badge_w = pad + spin_w + ctx.fonts.text->width(status, ctx.style.font_size) + pad;
			const float right_edge = bar_rect.right() - button_w * 5.f - ctx.style.padding;
			const gse::rectf status_rect = gse::rectf::from_position_size(
				{ right_edge - badge_w, bar_rect.center().y() + badge_h * 0.5f },
				{ badge_w, badge_h }
			);
			ctx.queue_sprite({
				.rect = status_rect,
				.color = pill_color,
				.texture = ctx.blank_texture,
				.corner_radius = badge_h * 0.5f,
			});
			float sx = status_rect.left() + pad;
			if (spinning) {
				draw_spinner(ctx, gse::rectf::from_position_size({ sx, status_rect.center().y() + spin_w * 0.5f }, { spin_w, spin_w }), pill_fg, spinner_rotation());
				sx += spin_w;
			}
			ctx.queue_text({
				.font = ctx.fonts.text,
				.text = status,
				.position = { sx, status_rect.center().y() + ctx.fonts.text->vertical_center_offset(ctx.style.font_size) },
				.scale = ctx.style.font_size,
				.color = pill_fg,
				.clip_rect = status_rect,
			});
			if (status_rect.contains(ctx.input.mouse_position()) && ctx.input_available()) {
				ctx.set_tooltip(gse::gui::ids::make("##semantic_index_status"), status_tooltip);
			}
		}
	}

	int resize_exclude_y0 = 0;
	int resize_exclude_y1 = 0;
	if (ctx.hit_regions) {
		if (const auto span = ctx.hit_regions->right_edge_block_span(screen_rect.right(), 2.f * ctx.style.scale_factor)) {
			resize_exclude_y0 = static_cast<int>(std::floor(screen_rect.top() - span->second));
			resize_exclude_y1 = static_cast<int>(std::ceil(screen_rect.top() - span->first));
		}
	}

	m_channels.push<gse::window_chrome_metrics_request>({
		.caption_height = static_cast<int>(bar_height),
		.controls_width = static_cast<int>(button_w * 5.f),
		.interactive_x0 = 0,
		.interactive_x1 = 0,
		.resize_exclude_y0 = resize_exclude_y0,
		.resize_exclude_y1 = resize_exclude_y1,
	});
}

auto gse::ide::draw_search_bar(gse::gui::builder& ui, quick_search_state& state, const search::index_state* index, gse::channel_writer channels, const gse::rectf& search_rect, const std::string_view id_key) -> void {
	const auto& ctx = ui.ctx;
	const auto& sty = ctx.style;
	const float pad = sty.padding;
	const float outline = 1.5f * sty.scale_factor;

	const gse::id search_id = gse::gui::ids::make(id_key);
	const bool was_focused = ui.focus_widget_id == search_id;

	ctx.queue_sprite({
		.rect = gse::rectf::from_position_size(
			{ search_rect.left() - outline, search_rect.top() + outline },
			{ search_rect.width() + outline * 2.f, search_rect.height() + outline * 2.f }
		),
		.color = was_focused ? sty.color_accent : sty.color_accent_dim,
		.texture = ctx.blank_texture,
		.corner_radius = sty.corner_radius,
	});
	gse::gui::draw::text_input_in_rect(ctx, search_id, state.driver.query, state.input, search_rect, ui.hot_widget_id, ui.focus_widget_id);
	const bool focused = ui.focus_widget_id == search_id;

	if (state.driver.query.empty() && !focused) {
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = "Search...",
			.position = { search_rect.left() + pad, search_rect.center().y() + ctx.fonts.text->vertical_center_offset(sty.font_size) },
			.scale = sty.font_size,
			.color = sty.color_text_secondary,
			.clip_rect = search_rect,
		});
	}

	const gse::time now = gse::system_clock::now<gse::time>();
	state.driver.update(now, index, search::options{
		.max_results = 8,
	});

	if ((!was_focused && !focused) || state.driver.results.empty()) {
		return;
	}

	if (was_focused && !state.driver.query.empty()) {
		if (ctx.input.key_pressed(gse::key::down)) {
			state.driver.selected = std::min<int>(state.driver.selected + 1, static_cast<int>(state.driver.results.size()) - 1);
		}
		if (ctx.input.key_pressed(gse::key::up)) {
			state.driver.selected = std::max(state.driver.selected - 1, 0);
		}
		if (ctx.input.key_pressed(gse::key::enter)) {
			const int idx = state.driver.selected >= 0 ? state.driver.selected : 0;
			const search::result& r = state.driver.results[static_cast<std::size_t>(idx)];
			channels.push<jump_to_request>({
				.path = r.path,
				.line = r.line,
				.column = r.column,
			});
			state.driver.accept();
			ui.focus_widget_id = {};
			return;
		}
	}

	const auto layer = ctx.scoped_layer(gse::render_layer::popup);
	const float row_h = ctx.fonts.text->line_height(sty.font_size) + pad * 0.5f;
	const gse::rectf list_rect = gse::rectf::from_position_size(
		{ search_rect.left(), search_rect.bottom() - 2.f * sty.scale_factor },
		{ search_rect.width(), row_h * static_cast<float>(state.driver.results.size()) }
	);
	ctx.queue_sprite({
		.rect = list_rect,
		.color = { sty.color_menu_body.x(), sty.color_menu_body.y(), sty.color_menu_body.z(), 1.f },
		.texture = ctx.blank_texture,
		.corner_radius = sty.corner_radius,
	});
	const gse::vec2f mouse = ctx.input.mouse_position();
	const bool clicked = ctx.input.mouse_button_pressed(gse::mouse_button::button_1);
	for (std::size_t i = 0; i < state.driver.results.size(); ++i) {
		const gse::rectf row = gse::rectf::from_position_size(
			{ list_rect.left(), list_rect.top() - row_h * static_cast<float>(i) },
			{ list_rect.width(), row_h }
		);
		const bool over = row.contains(mouse);
		if (over) {
			state.driver.selected = static_cast<int>(i);
		}
		if (state.driver.selected == static_cast<int>(i)) {
			ctx.queue_sprite({
				.rect = row,
				.color = sty.color_selection,
				.texture = ctx.blank_texture,
			});
		}
		const search::result& r = state.driver.results[i];
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = r.display,
			.position = { row.left() + pad, row.center().y() + ctx.fonts.text->vertical_center_offset(sty.font_size) },
			.scale = sty.font_size,
			.color = sty.color_text,
			.clip_rect = row,
		});
		if (over && clicked && !ctx.is_press_consumed(gse::mouse_button::button_1)) {
			ctx.consume_press(gse::mouse_button::button_1);
			channels.push<jump_to_request>({
				.path = r.path,
				.line = r.line,
				.column = r.column,
			});
			state.driver.accept();
			ui.focus_widget_id = {};
		}
	}
}

namespace gse::ide::explorer_menu {
	[[= gse::gui::context_action<"New File", "create", gse::gui::symbol::file>{}]]
	auto new_file(workspace::data& w, const fs_node& n) -> void {
		workspace::create_entry(w, n, false);
	}

	[[= gse::gui::context_action<"New Folder", "create", gse::gui::symbol::folder>{}]]
	auto new_folder(workspace::data& w, const fs_node& n) -> void {
		workspace::create_entry(w, n, true);
	}

	[[= gse::gui::context_action<"Rename", "edit">{}]]
	auto rename(workspace::data& w, const fs_node& n) -> void {
		workspace::rename_entry(w, n);
	}

	[[= gse::gui::context_action<"Delete", "danger", gse::gui::symbol::trash>{}]]
	[[= gse::gui::destructive]]
	auto remove(workspace::data& w, const fs_node& n) -> void {
		workspace::delete_entry(w, n);
	}
}

namespace gse::ide::tab_menu {
	[[= gse::gui::context_action<"Close", "close", gse::gui::symbol::close>{}]]
	auto close(workspace::data& w, std::uint32_t doc_id) -> void {
		workspace::close_document(w, doc_id);
	}

	[[= gse::gui::context_action<"Close Others", "close">{}]]
	auto close_others(workspace::data& w, std::uint32_t doc_id) -> void {
		std::vector<std::uint32_t> ids;
		for (const auto& [id, doc] : w.documents) {
			if (id != doc_id) {
				ids.push_back(id);
			}
		}
		for (const std::uint32_t id : ids) {
			workspace::close_document(w, id);
		}
	}

	[[= gse::gui::context_action<"Close All", "bulk">{}]]
	auto close_all(workspace::data& w, std::uint32_t) -> void {
		std::vector<std::uint32_t> ids;
		for (const auto& [id, doc] : w.documents) {
			ids.push_back(id);
		}
		for (const std::uint32_t id : ids) {
			workspace::close_document(w, id);
		}
	}
}

namespace gse::ide {
	using explorer_sig = void(workspace::data&, const fs_node&);

	auto explorer_actions() -> const std::vector<gse::gui::context_action_entry<explorer_sig>>& {
		static const auto table = gse::gui::build_context_actions<explorer_sig,
			^^explorer_menu::new_file,
			^^explorer_menu::new_folder,
			^^explorer_menu::rename,
			^^explorer_menu::remove
		>();
		return table;
	}

	auto explorer_context_tag() -> gse::id {
		return gse::find_or_generate_id("explorer_context");
	}

	using tab_sig = void(workspace::data&, std::uint32_t);

	auto tab_actions() -> const std::vector<gse::gui::context_action_entry<tab_sig>>& {
		static const auto table = gse::gui::build_context_actions<tab_sig,
			^^tab_menu::close,
			^^tab_menu::close_others,
			^^tab_menu::close_all
		>();
		return table;
	}

	auto tab_context_tag() -> gse::id {
		return gse::find_or_generate_id("tab_context");
	}

	auto editor_text_context_tag() -> gse::id {
		return gse::find_or_generate_id("editor_text_context");
	}

	auto editor_menu(gse::gui::data& gui, const std::string_view name) -> gse::gui::menu* {
		const gse::id existing_id = gse::find_or_generate_id(std::string(name));
		if (gse::gui::menu* existing = gui.menus.try_get(existing_id)) {
			return existing;
		}

		gse::gui::menu new_menu(
			name,
			gse::gui::menu_data{
				.rect = rectf::from_position_size(
					{ 100.f, 100.f },
					{ 300.f, 200.f }
				),
				.parent_id = gse::id(),
			}
		);

		const gse::id new_id = new_menu.id();
		gui.menus.add(new_id, std::move(new_menu));
		return gui.menus.try_get(new_id);
	}
}

auto gse::ide::git_status_color(const git::file_status status) -> gse::vec4f {
	switch (status) {
		case git::file_status::modified:
			return { 0.86f, 0.66f, 0.32f, 1.0f };
		case git::file_status::added:
			return { 0.46f, 0.80f, 0.48f, 1.0f };
		case git::file_status::untracked:
			return { 0.40f, 0.72f, 0.55f, 1.0f };
		case git::file_status::deleted:
			return { 0.86f, 0.40f, 0.40f, 1.0f };
		case git::file_status::renamed:
			return { 0.46f, 0.68f, 0.90f, 1.0f };
		case git::file_status::conflicted:
			return { 0.92f, 0.48f, 0.30f, 1.0f };
		default:
			return { 0.96f, 0.97f, 0.99f, 1.0f };
	}
}

auto gse::ide::draw_explorer_panel(gse::gui::builder& ui, workspace::data& ws, quick_search_state& search, const search::index_state* index, gse::channel_writer channels, const git::status_map* git_status) -> void {
	const auto& ctx = ui.ctx;
	if (ctx.clip_stack.empty()) {
		return;
	}

	const gse::rectf body = ctx.clip_stack.back();
	const float pad = ctx.style.padding;
	const float search_height = ctx.fonts.text->line_height(ctx.style.font_size) + pad * 0.8f;
	const float accent_width = std::max(2.f, ctx.style.accent_bar_width);
	const gse::vec4f explorer_bg{ 0.045f, 0.06f, 0.095f, 0.98f };
	const gse::vec4f explorer_accent{ 0.62f, 0.34f, 1.0f, 1.0f };

	const gse::rectf panel = gse::gui::draw::panel_backdrop(ctx, {
		.rect = body,
		.background = explorer_bg,
		.accent = gse::gui::panel_accent{
			.edge = gse::gui::panel_edge::right,
			.width = accent_width,
			.color = explorer_accent,
		},
	});

	const gse::rectf content = panel.inset({ pad, pad });
	const gse::rectf search_rect = gse::rectf::from_position_size(
		content.top_left(),
		{ std::max(0.f, content.width()), search_height }
	);

	draw_search_bar(ui, search, index, channels, search_rect, "##explorer_search");
	ctx.layout_cursor.x() = content.left();
	ctx.layout_cursor.y() = search_rect.bottom() - pad;

	if (!ws.fs_root.loaded) {
		workspace::load_children(ws.fs_root);
	}

	if (ws.fs_root.children.empty()) {
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = "(empty) " + ws.fs_root.path.display_string(),
			.position = { content.left(), ctx.layout_cursor.y() + ctx.fonts.text->vertical_center_offset(ctx.style.font_size) },
			.scale = ctx.style.font_size,
			.color = ctx.style.color_text_secondary,
			.clip_rect = body,
		});
		return;
	}

	enum class explorer_name_action {
		none,
		commit,
		cancel,
	};
	explorer_name_action name_action = explorer_name_action::none;

	const gse::gui::draw::tree_ops<fs_node> ops{
		.children = [](const fs_node& n) -> std::span<const fs_node> {
			if (n.is_dir && !n.loaded) {
				workspace::load_children(n);
			}
			return n.children;
		},
		.label = [&ws](const fs_node& n) -> std::string_view {
			if (ws.pending_name && ws.pending_name->key == n.key) {
				return {};
			}
			return n.name;
		},
		.key = [](const fs_node& n) -> std::uint64_t {
			return n.key;
		},
		.is_leaf = [](const fs_node& n) -> bool {
			return !n.is_dir;
		},
		.custom_draw = [&ui, &ws, &name_action](const fs_node& n, const gse::gui::draw_context& c, const gse::rect_t<gse::vec2f>& row_rect, bool, bool, int) {
			if (!ws.pending_name || ws.pending_name->key != n.key) {
				return;
			}
			auto& pending = *ws.pending_name;
			const float row_height = row_rect.height();
			const float arrow_w = c.style.font_size;
			const float icon_w = c.style.font_size;
			const float label_x = row_rect.left() + arrow_w + icon_w + c.style.padding * 0.5f;
			const auto input_rect = gse::rect_t<gse::vec2f>::from_position_size(
				{ label_x, row_rect.top() },
				{ std::max(0.f, row_rect.right() - label_x), row_height }
			);
			const gse::id input_id = gse::gui::ids::make_from_key(gse::hash_combine(n.key, gse::stable_id("explorer_name_input")));
			if (pending.focus_requested) {
				ui.focus_widget_id = input_id;
				pending.input.caret = static_cast<int>(pending.name.size());
				pending.input.anchor = 0;
				pending.focus_requested = false;
			}
			gse::gui::draw::text_input_in_rect(c, input_id, pending.name, pending.input, input_rect, ui.hot_widget_id, ui.focus_widget_id);
			if (c.input.key_pressed(gse::key::escape)) {
				name_action = explorer_name_action::cancel;
			}
			else if (c.input.key_pressed(gse::key::enter) || c.input.key_pressed(gse::key::kp_enter)) {
				name_action = explorer_name_action::commit;
			}
			else if (ui.focus_widget_id != input_id) {
				name_action = explorer_name_action::commit;
			}
		},
		.on_context = [](const fs_node& n, const gse::gui::draw_context& c, gse::vec2f pos) {
			c.open_context_menu({
				.position = pos,
				.items = gse::gui::to_menu_items(explorer_actions()),
				.target = n.key,
				.tag = explorer_context_tag(),
			});
		},
		.icon = [](const fs_node& n) -> std::span<const gse::gui::symbol::stroke> {
			return n.is_dir ? gse::gui::symbol::folder() : gse::gui::symbol::file();
		},
		.label_color = [git_status, base = ctx.style.color_text](const fs_node& n) -> gse::vec4f {
			if (!git_status) {
				return base;
			}
			if (n.is_dir) {
				return git_status->dir_has_changes(n.path) ? gse::lerp(git_status_color(git::file_status::modified), base, 0.5f) : base;
			}
			if (const git::file_status status = git_status->status_of(n.path); status != git::file_status::none) {
				return git_status_color(status);
			}
			return base;
		},
	};

	ui.scroll_region({
		.id = "explorer_tree",
	}, [&](gse::gui::builder& b) {
		b.draw<gse::gui::tree<fs_node>>({
			.roots = ws.fs_root.children,
			.ops = ops,
			.selection = &ws.explorer_selection,
		});
	});

	if (name_action == explorer_name_action::cancel) {
		workspace::cancel_pending_name(ws);
	}
	else if (name_action == explorer_name_action::commit) {
		workspace::commit_pending_name(ws);
	}

	for (const std::uint64_t key : ws.explorer_selection.keys) {
		if (ws.pending_name && key == ws.pending_name->key) {
			continue;
		}
		if (ws.explorer_selection_seen.contains(key)) {
			continue;
		}
		if (const fs_node* node = workspace::find_node(ws.fs_root, key); node && !node->is_dir) {
			workspace::open_file(ws, node->path);
		}
	}
	if (ws.explorer_selection_seen != ws.explorer_selection.keys) {
		ws.explorer_selection_seen = ws.explorer_selection.keys;
	}
}

auto gse::ide::spinner_rotation() -> gse::angle {
	constexpr gse::angular_velocity spinner_angular_velocity = gse::radians_per_second(9.6f);
	constexpr gse::angle full_rotation = gse::degrees(360.f);
	return gse::fmod(spinner_angular_velocity * gse::system_clock::now(), full_rotation);
}

auto gse::ide::draw_spinner(const gse::gui::draw_context& ctx, const rectf& rect, const gse::vec4f color, const gse::angle rotation) -> void {
	std::array<gse::gui::symbol::stroke, 9> strokes{};
	constexpr int segments = 9;
	constexpr float radius = 0.34f;
	const gse::angle sweep = gse::degrees(270.f);
	gse::vec2f previous{};
	for (int i = 0; i <= segments; ++i) {
		const float t = static_cast<float>(i) / static_cast<float>(segments);
		const gse::angle a = rotation + sweep * t;
		const gse::vec2f point{ 0.5f + gse::cos(a) * radius, 0.5f + gse::sin(a) * radius };
		if (i > 0) {
			strokes[static_cast<std::size_t>(i - 1)] = { previous, point };
		}
		previous = point;
	}
	gse::gui::symbol::draw(ctx, strokes, rect, {
		.color = color,
		.scale = ctx.style.icon_scale,
		.clip_rect = rect,
	});
}

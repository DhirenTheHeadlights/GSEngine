export module gse.ide.app:chrome;

import std;
import gse;

import gse.ide.workspace;
import gse.ide.git;
import gse.ide.build;
import gse.ide.search;
import gse.ide.project;

import :search_screen;
import :project_screen;

namespace gse::ide {
	auto rebuild_glyph() -> std::span<const gse::gui::symbol::stroke>;

	auto git_status_color(
		git::file_status status
	) -> gse::vec4f;

	struct toggle_settings_request {};

	struct toggle_project_switcher_request {};

	struct quick_search_state {
		search::query_driver driver;
		gse::gui::text_input_state input;
	};

	class editor_screen : public gse::gui::screen {
	public:
		editor_screen(
			gse::channel_writer channels,
			const search::index_state* index,
			gse::shared_view<gse::input::data> input
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

		auto wants_chrome() const -> bool override;

		auto draw_caption(
			gse::gui::builder& ui,
			const gse::rectf& area
		) -> float override;

		auto caption_exclusion_range(
			const gse::gui::draw_context& ctx,
			const gse::rectf& full_rect
		) const -> gse::gui::caption_exclusion override;

	private:
		gse::channel_writer m_channels;
		const search::index_state* m_index = nullptr;
		gse::shared_view<gse::input::data> m_input;
		std::optional<std::string> m_loc_label;
		std::string m_loc_tooltip;
		search::loc_counts m_loc_counts;
	};

	auto format_loc(
		std::uint64_t lines
	) -> std::string;

	auto append_loc_row(
		std::string& text,
		std::string_view label,
		const search::loc_group& group
	) -> void;

	auto describe_loc(
		const search::loc_counts& counts
	) -> std::string;

	auto draw_search_bar(
		gse::gui::builder& ui,
		const gse::input::state& input,
		quick_search_state& state,
		const search::index_state* index,
		gse::channel_writer channels,
		const gse::rectf& search_rect,
		std::string_view id_key
	) -> void;

	auto draw_explorer_panel(
		gse::gui::builder& ui,
		const gse::input::state& input,
		workspace::data& ws,
		quick_search_state& search,
		const search::index_state* index,
		gse::channel_writer channels,
		const git::status_map* git_status,
		std::span<const std::filesystem::path> git_rootless
	) -> void;

	auto explorer_menu_items(
		const workspace::data& w,
		const fs_node& n
	) -> std::vector<gse::gui::menu_item>;

	auto spinner_rotation() -> gse::angle;

	auto draw_spinner(
		const gse::gui::draw_context& ctx,
		const rectf& rect,
		gse::vec4f color,
		gse::angle rotation
	) -> void;
}

gse::ide::editor_screen::editor_screen(gse::channel_writer channels, const search::index_state* index, const gse::shared_view<gse::input::data> input)
	: m_channels(std::move(channels)), m_index(index), m_input(input) {
}

auto gse::ide::editor_screen::title() const -> std::string_view {
	static const std::string value = project::current().valid
		? std::format("{} \xC2\xB7 GSEditor", project::current().name)
		: std::string("GSEditor");
	return value;
}

auto gse::ide::editor_screen::dismissable() const -> bool {
	return false;
}

auto gse::ide::editor_screen::captures_input() const -> bool {
	return false;
}

auto gse::ide::editor_screen::draw_backdrop(gse::gui::draw_context&, gse::vec2f) const -> void {
}

auto gse::ide::editor_screen::wants_chrome() const -> bool {
	return true;
}

auto gse::ide::editor_screen::caption_exclusion_range(const gse::gui::draw_context& ctx, const gse::rectf& full_rect) const -> gse::gui::caption_exclusion {
	if (!ctx.hit_regions) {
		return {};
	}

	const auto span = ctx.hit_regions->right_edge_block_span(full_rect.right(), 2.f * ctx.style.scale_factor);
	if (!span) {
		return {};
	}

	return {
		.y0 = static_cast<int>(std::floor(full_rect.top() - span->second)),
		.y1 = static_cast<int>(std::ceil(full_rect.top() - span->first)),
	};
}

auto gse::ide::format_loc(const std::uint64_t lines) -> std::string {
	std::string digits = std::to_string(lines);
	std::string grouped;
	grouped.reserve(digits.size() + digits.size() / 3);
	for (const auto [index, digit] : std::views::enumerate(digits)) {
		if (index > 0 && (digits.size() - static_cast<std::size_t>(index)) % 3 == 0) {
			grouped.push_back(',');
		}
		grouped.push_back(digit);
	}
	return grouped;
}

auto gse::ide::append_loc_row(std::string& text, const std::string_view label, const search::loc_group& group) -> void {
	if (group.cpp == 0 && group.slang == 0) {
		return;
	}
	text += std::format("\n{}", label);
	if (group.cpp > 0) {
		text += std::format(" \xC2\xB7 {} C++", format_loc(group.cpp));
	}
	if (group.slang > 0) {
		text += std::format(" \xC2\xB7 {} Slang", format_loc(group.slang));
	}
}

auto gse::ide::describe_loc(const search::loc_counts& counts) -> std::string {
	std::string text = std::format("{} indexed source lines", format_loc(search::loc_total(counts)));
	append_loc_row(text, "Engine", counts.engine);
	append_loc_row(text, "Editor", counts.editor);
	append_loc_row(text, project::current().valid ? std::string_view(project::current().name) : std::string_view("Game"), counts.project);
	append_loc_row(text, "Total", {
		.cpp = counts.engine.cpp + counts.editor.cpp + counts.project.cpp,
		.slang = counts.engine.slang + counts.editor.slang + counts.project.slang,
	});
	return text;
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
	const gse::input::state& input = gse::input::current_state(m_input);
	if (!ctx.current_menu) {
		return;
	}

	const bool control_held = input.key_held(gse::key::left_control) || input.key_held(gse::key::right_control);
	const bool shift_held = input.key_held(gse::key::left_shift) || input.key_held(gse::key::right_shift);

	if (control_held && !shift_held && input.key_pressed(gse::key::f)) {
		n.push<search_screen>(m_channels, m_index, m_input);
	}

	if (control_held && shift_held && input.key_pressed(gse::key::p)) {
		n.push<project_screen>(m_channels, m_input);
	}
}

auto gse::ide::editor_screen::draw_caption(gse::gui::builder& ui, const gse::rectf& area) -> float {
	const auto& ctx = ui.ctx;
	const auto& sty = ctx.style;
	const auto text_view = ctx.fonts.text.resolve();

	const float swatch_width = sty.accent_bar_width;
	const float swatch_height = sty.font_size;
	ctx.queue_sprite({
		.rect = gse::rectf::from_position_size(
			{ area.left() + sty.padding, area.center().y() + swatch_height * 0.5f },
			{ swatch_width, swatch_height }
		),
		.color = project::accent(),
		.texture = ctx.blank_texture,
	});

	const float title_x = area.left() + sty.padding + swatch_width + sty.padding * 0.5f;

	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = title(),
		.position = { title_x, area.center().y() + text_view->vertical_center_offset(sty.font_size) },
		.scale = sty.font_size,
		.color = sty.color_text,
		.clip_rect = area,
	});

	const float button_w = area.height() * 1.5f;
	const float controls_width = button_w * 3.f;

	const gse::rectf settings_rect = gse::rectf::from_position_size({ area.right() - button_w, area.top() }, { button_w, area.height() });
	const gse::rectf rebuild_rect = gse::rectf::from_position_size({ area.right() - button_w * 2.f, area.top() }, { button_w, area.height() });
	const gse::rectf project_rect = gse::rectf::from_position_size({ area.right() - button_w * 3.f, area.top() }, { button_w, area.height() });

	if (gse::gui::caption_button(ui, settings_rect, "##chrome_settings", gse::gui::symbol::gear(), sty.color_widget_hovered)) {
		m_channels.push<toggle_settings_request>({});
	}
	if (gse::gui::caption_button(ui, rebuild_rect, "##chrome_rebuild", rebuild_glyph(), sty.color_widget_hovered)) {
		m_channels.push<build_runner::build_request>({
			.target = build_runner::build_target::editor,
		});
	}
	if (gse::gui::caption_button(ui, project_rect, "##chrome_project", gse::gui::symbol::project(), sty.color_widget_hovered)) {
		m_channels.push<toggle_project_switcher_request>({});
	}

	if (!m_index) {
		return controls_width;
	}

	const search::loc_counts counts = m_index->loc.load();
	if (const std::uint64_t loc = search::loc_total(counts)) {
		if (m_loc_counts != counts) {
			m_loc_counts = counts;
			m_loc_label = std::format("{}k LOC", (loc + 500) / 1000);
			m_loc_tooltip = describe_loc(counts);
		}
		const float title_w = text_view->width(title(), sty.font_size);
		const float badge_pad = sty.padding * 0.5f;
		const float badge_height = sty.font_size + sty.padding * 0.5f;
		const float badge_width = text_view->width(*m_loc_label, sty.font_size) + badge_pad * 2.f;
		const gse::rectf badge_rect = gse::rectf::from_position_size(
			{ title_x + title_w + sty.padding, area.center().y() + badge_height * 0.5f },
			{ badge_width, badge_height }
		);
		ctx.queue_sprite({
			.rect = badge_rect,
			.color = sty.color_accent_dim,
			.texture = ctx.blank_texture,
			.corner_radius = badge_height * 0.5f,
		});
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = *m_loc_label,
			.position = { badge_rect.left() + badge_pad, badge_rect.center().y() + text_view->vertical_center_offset(sty.font_size) },
			.scale = sty.font_size,
			.color = sty.color_accent,
			.clip_rect = badge_rect,
		});
		if (ctx.hovers(badge_rect)) {
			ctx.set_tooltip(gse::gui::ids::make("##chrome_loc"), m_loc_tooltip);
		}
	}

	std::string status;
	std::string status_tooltip;
	bool spinning = false;
	gse::vec4f pill_color = sty.color_input_background;
	gse::vec4f pill_fg = sty.color_text_secondary;
	const search::index_phase phase = m_index->phase.load(std::memory_order_acquire);
	if (phase != search::index_phase::idle) {
		const search::index_phase_info info = gse::annotation_from_enum<search::index_phase_info>(phase, {
			.label = "Unknown indexing stage",
			.detail = "No explanation was recorded for this indexing stage.",
		});
		const std::size_t total = m_index->progress_total.load(std::memory_order_acquire);
		const std::size_t done = std::min(m_index->progress_done.load(std::memory_order_acquire), total);
		const gse::time_t<double> started = m_index->phase_started.load(std::memory_order_acquire);
		const double elapsed_seconds = started == gse::time_t<double>{}
			? 0.0
			: (gse::system_clock::now<gse::time_t<double>>() - started).as<gse::seconds>();
		status = std::string(info.label);
		if (total > 0) {
			status += std::format(" {}/{}", done, total);
		}
		status += elapsed_seconds < 10.f
			? std::format(" \xC2\xB7 {:.1f}s", elapsed_seconds)
			: std::format(" \xC2\xB7 {:.0f}s", elapsed_seconds);
		status_tooltip = std::format(
			"{} — {} Progress: {} of {} items. Time in this stage: {:.1f} seconds.",
			info.label,
			info.detail,
			done,
			total,
			elapsed_seconds
		);
		spinning = true;
		pill_color = sty.color_accent_dim;
		pill_fg = sty.color_accent;
	}
	else if (const std::size_t syms = m_index->symbol_count.load(std::memory_order_acquire); syms > 0) {
		status = syms >= 1000 ? std::format("{}k symbols", (syms + 500) / 1000) : std::format("{} symbols", syms);
		status_tooltip = std::format("Semantic index ready. {} symbols are available for highlighting, hover, and navigation.", syms);
	}

	if (status.empty()) {
		return controls_width;
	}

	const float badge_h = sty.font_size + sty.padding * 0.5f;
	const float pad = sty.padding * 0.6f;
	const float spin_w = spinning ? sty.font_size : 0.f;
	const float badge_w = pad + spin_w + text_view->width(status, sty.font_size) + pad;
	const float right_edge = project_rect.left() - sty.padding;
	const gse::rectf status_rect = gse::rectf::from_position_size(
		{ right_edge - badge_w, area.center().y() + badge_h * 0.5f },
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
		.position = { sx, status_rect.center().y() + text_view->vertical_center_offset(sty.font_size) },
		.scale = sty.font_size,
		.color = pill_fg,
		.clip_rect = status_rect,
	});
	if (ctx.hovers(status_rect)) {
		ctx.set_tooltip(gse::gui::ids::make("##semantic_index_status"), status_tooltip);
	}

	return controls_width;
}

auto gse::ide::draw_search_bar(gse::gui::builder& ui, const gse::input::state& input, quick_search_state& state, const search::index_state* index, gse::channel_writer channels, const gse::rectf& search_rect, const std::string_view id_key) -> void {
	const auto& ctx = ui.ctx;
	const auto& sty = ctx.style;
	const auto text_view = ctx.fonts.text.resolve();
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
			.position = { search_rect.left() + pad, search_rect.center().y() + text_view->vertical_center_offset(sty.font_size) },
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
		if (input.key_pressed(gse::key::down)) {
			state.driver.selected = std::min<int>(state.driver.selected + 1, static_cast<int>(state.driver.results.size()) - 1);
		}
		if (input.key_pressed(gse::key::up)) {
			state.driver.selected = std::max(state.driver.selected - 1, 0);
		}
		if (input.key_pressed(gse::key::enter)) {
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
	const float row_h = text_view->line_height(sty.font_size) + pad * 0.5f;
	const gse::rectf list_rect = gse::rectf::from_position_size(
		{ search_rect.left(), search_rect.bottom() - 2.f * sty.scale_factor },
		{ search_rect.width(), row_h * static_cast<float>(state.driver.results.size()) }
	);
	ctx.queue_sprite({
		.rect = list_rect,
		.color = { vec3f(sty.color_menu_body), 1.f },
		.texture = ctx.blank_texture,
		.corner_radius = sty.corner_radius,
	});
	for (std::size_t i = 0; i < state.driver.results.size(); ++i) {
		const gse::rectf row = gse::rectf::from_position_size(
			{ list_rect.left(), list_rect.top() - row_h * static_cast<float>(i) },
			{ list_rect.width(), row_h }
		);
		const bool over = ctx.hovers(row);
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
			.position = { row.left() + pad, row.center().y() + text_view->vertical_center_offset(sty.font_size) },
			.scale = sty.font_size,
			.color = sty.color_text,
			.clip_rect = row,
		});
		if (over && ctx.mouse_pressed_for(row)) {
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
		workspace::create_entry(w, workspace::target(n), false);
	}

	[[= gse::gui::context_action<"New Folder", "create", gse::gui::symbol::folder>{}]]
	auto new_folder(workspace::data& w, const fs_node& n) -> void {
		workspace::create_entry(w, workspace::target(n), true);
	}

	[[= gse::gui::context_action<"Rename", "edit">{}]]
	auto rename(workspace::data& w, const fs_node& n) -> void {
		workspace::rename_entry(w, workspace::target(n));
	}

	[[= gse::gui::context_action<"Reveal in File Explorer", "path", gse::gui::symbol::folder>{}]]
	auto reveal(workspace::data&, const fs_node& n) -> void {
		gse::shell::reveal(n.path);
	}

	[[= gse::gui::context_action<"Copy Path", "path">{}]]
	auto copy_path(workspace::data&, const fs_node& n) -> void {
		gse::window::set_clipboard_text(n.path.generic_display_string());
	}

	[[= gse::gui::context_action<"Delete", "danger", gse::gui::symbol::trash>{}]]
	[[= gse::gui::destructive]]
	auto remove(workspace::data& w, const fs_node& n) -> void {
		workspace::delete_entry(w, workspace::target(n));
	}
}

namespace gse::ide::tab_menu {
	[[= gse::gui::context_action<"Close", "close", gse::gui::symbol::close>{}]]
	auto close(workspace::data& w, gse::id doc_id) -> void {
		workspace::close_document(w, doc_id);
	}

	[[= gse::gui::context_action<"Close Others", "close">{}]]
	auto close_others(workspace::data& w, gse::id doc_id) -> void {
		std::vector<gse::id> ids;
		for (const auto& [id, doc] : w.documents) {
			if (id != doc_id) {
				ids.push_back(id);
			}
		}
		for (const gse::id id : ids) {
			if (!workspace::close_document(w, id)) {
				break;
			}
		}
	}

	[[= gse::gui::context_action<"Reveal in File Explorer", "path", gse::gui::symbol::folder>{}]]
	auto reveal(workspace::data& w, gse::id doc_id) -> void {
		if (const auto it = w.documents.find(doc_id); it != w.documents.end()) {
			gse::shell::reveal(it->second.path);
		}
	}

	[[= gse::gui::context_action<"Copy Path", "path">{}]]
	auto copy_path(workspace::data& w, gse::id doc_id) -> void {
		if (const auto it = w.documents.find(doc_id); it != w.documents.end()) {
			gse::window::set_clipboard_text(it->second.path.generic_display_string());
		}
	}

	[[= gse::gui::context_action<"Close All", "bulk">{}]]
	auto close_all(workspace::data& w, gse::id) -> void {
		std::vector<gse::id> ids;
		for (const auto& [id, doc] : w.documents) {
			ids.push_back(id);
		}
		for (const gse::id id : ids) {
			if (!workspace::close_document(w, id)) {
				break;
			}
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
			^^explorer_menu::reveal,
			^^explorer_menu::copy_path,
			^^explorer_menu::remove
		>();
		return table;
	}

	auto explorer_context_tag() -> gse::id {
		return gse::find_or_generate_id("explorer_context");
	}

	using tab_sig = void(workspace::data&, gse::id);

	auto tab_actions() -> const std::vector<gse::gui::context_action_entry<tab_sig>>& {
		static const auto table = gse::gui::build_context_actions<tab_sig,
			^^tab_menu::close,
			^^tab_menu::close_others,
			^^tab_menu::close_all,
			^^tab_menu::reveal,
			^^tab_menu::copy_path
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

auto gse::ide::explorer_menu_items(const workspace::data& w, const fs_node& n) -> std::vector<gse::gui::menu_item> {
	const auto& actions = explorer_actions();
	std::vector<gse::gui::menu_item> items = gse::gui::to_menu_items(actions);
	const explorer_target target = workspace::target(n);
	for (std::size_t i = 0; i < actions.size(); ++i) {
		if (actions[i].run == &explorer_menu::new_file || actions[i].run == &explorer_menu::new_folder) {
			items[i].enabled = workspace::can_create_entry(w, target);
		}
		else if (actions[i].run == &explorer_menu::rename || actions[i].run == &explorer_menu::remove) {
			items[i].enabled = workspace::can_mutate_entry(w, target);
		}
	}
	return items;
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

auto gse::ide::draw_explorer_panel(gse::gui::builder& ui, const gse::input::state& input, workspace::data& ws, quick_search_state& search, const search::index_state* index, gse::channel_writer channels, const git::status_map* git_status, const std::span<const std::filesystem::path> git_rootless) -> void {
	const auto& ctx = ui.ctx;
	if (ctx.clip_stack.empty()) {
		return;
	}
	const auto text_view = ctx.fonts.text.resolve();

	const gse::rectf body = ctx.clip_stack.back();
	const float pad = ctx.style.padding;
	const float search_height = text_view->line_height(ctx.style.font_size) + pad * 0.8f;
	const float accent_width = std::max(2.f, ctx.style.accent_bar_width);
	const gse::rectf panel = gse::gui::draw::panel_backdrop(ctx, {
		.rect = body,
		.background = ctx.style.color_input_background,
		.accent = gse::gui::panel_accent{
			.edge = gse::gui::panel_edge::right,
			.width = accent_width,
			.color = ctx.style.color_accent,
		},
	});

	const gse::rectf content = panel.inset({ pad, pad });
	const gse::rectf search_rect = gse::rectf::from_position_size(
		content.top_left(),
		{ std::max(0.f, content.width()), search_height }
	);

	draw_search_bar(ui, input, search, index, channels, search_rect, "##explorer_search");
	ctx.layout_cursor.x() = content.left();
	ctx.layout_cursor.y() = search_rect.bottom() - pad;
	if (!ws.explorer_error.empty()) {
		const float error_height = text_view->line_height(ctx.style.font_size);
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = ws.explorer_error,
			.position = { content.left(), ctx.layout_cursor.y() + text_view->vertical_center_offset(ctx.style.font_size) },
			.scale = ctx.style.font_size,
			.color = gse::vec4f{ 0.855f, 0.451f, 0.424f, 1.f },
			.clip_rect = body,
		});
		ctx.layout_cursor.y() -= error_height + pad;
	}

	for (const std::filesystem::path& rootless : git_rootless) {
		const float row_height = text_view->line_height(ctx.style.font_size) + pad * 0.5f;
		const gse::rectf init_rect = gse::rectf::from_position_size(
			{ content.left(), ctx.layout_cursor.y() },
			{ std::max(0.f, content.width()), row_height }
		);
		std::string scoped_label;
		if (git_rootless.size() > 1) {
			scoped_label = std::format("Initialize Git in {}", rootless.filename().generic_display_string());
		}
		const std::string_view label = scoped_label.empty()
			? std::string_view("Initialize Git Repository")
			: std::string_view(scoped_label);
		if (gse::gui::draw::button_in_rect(ui.ctx, label, "##git_init_" + rootless.generic_display_string(), init_rect, ui.hot_widget_id, ui.active_widget_id)) {
			channels.push<git_system::init_request>({ .root = rootless });
		}
		ctx.layout_cursor.y() -= row_height + pad;
	}

	if (ws.fs_root.children.empty()) {
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = "(empty) " + ws.fs_root.path.generic_display_string(),
			.position = { content.left(), ctx.layout_cursor.y() + text_view->vertical_center_offset(ctx.style.font_size) },
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

	std::unordered_set<std::string> open_paths;
	std::string active_path;
	for (const auto& [id, doc] : ws.documents) {
		if (doc.path.empty()) {
			continue;
		}
		std::string key = doc.path.lexically_normal().native_encoded_string();
		if (workspace::active_document_id(ws) == id) {
			active_path = key;
		}
		open_paths.insert(std::move(key));
	}

	const gse::gui::draw::tree_ops<fs_node> ops{
		.children = [&ws](const fs_node& n) -> std::span<const fs_node> {
			if (n.is_dir && !n.loaded) {
				workspace::request_children(ws, n.key);
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
		.custom_draw = [&ui, &ws, &name_action, &input](const fs_node& n, const gse::gui::draw_context& c, const gse::rect_t<gse::vec2f>& row_rect, bool, bool, int) {
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
			if (input.key_pressed(gse::key::escape)) {
				name_action = explorer_name_action::cancel;
			}
			else if (input.key_pressed(gse::key::enter) || input.key_pressed(gse::key::kp_enter)) {
				name_action = explorer_name_action::commit;
			}
			else if (ui.focus_widget_id != input_id) {
				name_action = explorer_name_action::commit;
			}
		},
		.on_context = [&ws](const fs_node& n, const gse::gui::draw_context& c, gse::vec2f pos) {
			c.open_context_menu({
				.position = pos,
				.items = explorer_menu_items(ws, n),
				.target = n.key,
				.tag = explorer_context_tag(),
			});
		},
		.icon = [](const fs_node& n) -> std::span<const gse::gui::symbol::stroke> {
			if (n.glyph) {
				return n.glyph();
			}
			return n.is_dir ? gse::gui::symbol::folder() : gse::gui::symbol::file();
		},
		.label_color = [git_status, &open_paths, &active_path, base = ctx.style.color_text, accent = ctx.style.color_accent](const fs_node& n) -> gse::vec4f {
			if (!n.is_dir && !open_paths.empty()) {
				const std::string key = n.path.lexically_normal().native_encoded_string();
				if (key == active_path) {
					return accent;
				}
				if (open_paths.contains(key)) {
					return gse::lerp(accent, base, 0.45f);
				}
			}
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

	std::vector<std::uint64_t> reveal_expand;
	std::uint64_t reveal_key = 0;
	float reveal_offset = -1.f;
	if (ws.pending_reveal) {
		auto reveal = std::move(*ws.pending_reveal);
		ws.pending_reveal.reset();
		if (reveal.key != 0) {
			reveal_expand = std::move(reveal.expand);
			reveal_key = reveal.key;
			if (!ws.explorer_selection.keys.contains(reveal_key)) {
				ws.explorer_selection.keys.clear();
				ws.explorer_selection.keys.insert(reveal_key);
				ws.explorer_selection_seen.insert(reveal_key);
			}
		}
	}

	const float tree_top = ctx.layout_cursor.y();

	ui.scroll_region({
		.id = "explorer_tree",
	}, [&](gse::gui::builder& b) {
		b.draw<gse::gui::tree<fs_node>>({
			.roots = ws.fs_root.children,
			.ops = ops,
			.options = {
				.open_keys = reveal_expand,
				.reveal_key = reveal_key,
				.reveal_offset = reveal_key != 0 ? &reveal_offset : nullptr,
			},
			.selection = &ws.explorer_selection,
		});
	});

	if (reveal_offset >= 0.f) {
		const float row_height = text_view->line_height(ctx.style.font_size) + ctx.style.padding * 0.5f;
		const float view_height = std::max(0.f, tree_top - content.bottom());
		gse::gui::scroll_state& scroll = ctx.widget_scrolls[gse::hash_combine(gse::gui::ids::current_seed(), gse::stable_id("explorer_tree"))];
		if (reveal_offset < scroll.y.offset || reveal_offset + row_height > scroll.y.offset + view_height) {
			scroll.y.target = std::max(0.f, reveal_offset - view_height * 0.5f + row_height * 0.5f);
		}
	}

	if (name_action == explorer_name_action::cancel) {
		ws.cancel_name_requested = true;
	}
	else if (name_action == explorer_name_action::commit) {
		ws.commit_name_requested = true;
	}

	const auto open_if_file = [&](const std::uint64_t key) {
		if (ws.pending_name && key == ws.pending_name->key) {
			return;
		}
		if (const fs_node* node = workspace::find_node(ws.fs_root, key); node && !node->is_dir) {
			workspace::open_file(ws, node->path);
		}
	};

	if (const std::uint64_t activated = std::exchange(ws.explorer_selection.activated, 0); activated != 0) {
		open_if_file(activated);
	}

	for (const std::uint64_t key : ws.explorer_selection.keys) {
		if (ws.explorer_selection_seen.contains(key)) {
			continue;
		}
		open_if_file(key);
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
		.extent = ctx.style.icon_extent,
		.clip_rect = rect,
	});
}

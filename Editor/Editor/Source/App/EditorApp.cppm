export module gse.ide.app:editor_app;

import std;
import gse;
import gse.gpu;

import gse.ide.workspace;
import gse.ide.git;
import gse.ide.highlight;
import gse.ide.format;
import gse.ide.terminal;
import gse.ide.build;
import gse.ide.analysis;
import gse.ide.diagnostic;
import gse.ide.lint;
import gse.ide.config;
import gse.ide.search;
import gse.ide.graph;
import gse.ide.docs;
import gse.ide.viewport;

import :search_screen;

namespace gse::ide {
	constexpr std::string_view explorer_panel_name = "Explorer";
	constexpr std::string_view code_panel_name = "Code";
	constexpr std::uint32_t viewport_tab_id = std::numeric_limits<std::uint32_t>::max();
	constexpr gse::time editor_layout_save_interval = gse::seconds(30.f);
	constexpr gse::angular_velocity spinner_angular_velocity = gse::radians_per_second(9.6f);
	constexpr gse::angle full_rotation = gse::degrees(360.f);

	auto rebuild_glyph() -> std::span<const gse::gui::symbol::stroke>;

	auto git_status_color(
		git::file_status status
	) -> gse::vec4f;

	struct toggle_settings_request {};


	struct layout_section {
		std::string name;
		std::map<std::string, std::string> values;
	};

	auto editor_layout_path() -> std::filesystem::path;

	auto trim_layout_value(
		std::string_view value
	) -> std::string_view;

	auto layout_section_name(
		std::string_view line
	) -> std::string_view;

	auto parse_layout_sections(
		std::string_view text
	) -> std::vector<layout_section>;

	auto replace_layout_sections(
		layout_store::owner sections,
		std::string_view block
	) -> void;

	auto parse_layout_float(
		const std::string& value,
		float fallback
	) -> float;

	auto parse_layout_uint(
		const std::string& value,
		std::uint32_t fallback
	) -> std::uint32_t;

	auto editor_layout_owner() -> layout_store::owner;

	auto workspace_layout_owner() -> layout_store::owner;

	struct quick_search_state {
		std::string query;
		std::string last_query;
		gse::time changed_at{};
		bool dirty = false;
		gse::gui::text_input_state input;
		std::shared_ptr<search::query_buffer> pending;
		std::vector<search::result> results;
		int selected = -1;
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

	auto draw_code_panel(
		gse::gui::builder& ui,
		workspace::data& ws,
		gse::ide::graph_data& graph,
		const search::index_state* index,
		gse::shared_view<config_system::data> config,
		gse::channel_writer channels,
		gse::gpu::bindless_slot viewport_slot,
		bool game_running
	) -> void;

	auto code_position_at(
		const gse::gui::draw_context& ctx,
		const gse::gui::text_buffer& buffer,
		const gse::gui::text_area_state& view,
		const gse::rectf& rect,
		bool show_line_numbers,
		std::size_t display_tab_width,
		gse::vec2f mouse
	) -> gse::gui::buffer_position;

	auto spinner_rotation() -> gse::angle;

	auto draw_spinner(
		const gse::gui::draw_context& ctx,
		const rectf& rect,
		gse::vec4f color,
		gse::angle rotation
	) -> void;

	auto hover_wrap_lines(
		const gse::gui::draw_context& ctx,
		std::string_view text,
		float max_width,
		float scale
	) -> std::vector<std::string>;

	auto draw_hover_panel(
		const gse::gui::draw_context& ctx,
		const rectf& text_rect,
		hover_state& h,
		std::uint32_t z
	) -> bool;

	auto draw_diagnostic_tooltip(
		const gse::gui::draw_context& ctx,
		const rectf& text_rect,
		std::string_view label,
		std::string_view message,
		gse::vec4f label_color,
		gse::vec2f anchor
	) -> void;

	constexpr int quickfix_edit_kind = 0x5149;
	constexpr int format_edit_kind = 0x464d;

	struct quickfix_popup {
		std::uint32_t document_id = 0;
		std::uint32_t line = 0;
		std::uint32_t start_col = 0;
		gse::vec2f anchor;
		rectf panel;
	};

	struct quickfix_layout {
		rectf panel;
		rectf fix_row;
		rectf fixall_row;
	};

	auto draw_quickfix_tooltip(
		const gse::gui::draw_context& ctx,
		const rectf& text_rect,
		std::string_view label,
		gse::vec4f label_color,
		std::string_view message,
		std::string_view fix_title,
		int fixall_count,
		gse::vec2f anchor,
		gse::vec2f mouse
	) -> quickfix_layout;

	auto apply_quickfix(
		document& doc,
		std::span<const text_edit> edits
	) -> void;

	auto adjust_after_format(
		gse::gui::buffer_position& p,
		std::span<const format::line_edit> edits
	) -> void;

	auto apply_format(
		document& doc,
		const format::options& opts
	) -> void;

	auto format_and_save(
		workspace::data& ws,
		std::uint32_t document_id,
		const format::options& opts,
		bool format_enabled,
		bool force_save,
		gse::channel_writer channels
	) -> void;

	auto apply_diagnostics(
		workspace::data& ws,
		const std::shared_ptr<analysis::diagnostics_check>& check,
		gse::channel_writer channels
	) -> void;

	auto update_diagnostics(
		gse::context& ctx,
		workspace::data& ws,
		gse::shared_view<config_system::data> config
	) -> void;

	auto point_in_triangle(
		gse::vec2f p,
		gse::vec2f a,
		gse::vec2f b,
		gse::vec2f c
	) -> bool;

	auto hover_kept_alive(
		const hover_state& h,
		gse::vec2f mouse
	) -> bool;

	struct panel_code_hit {
		std::uint32_t line = 0;
		std::uint32_t start_col = 0;
		std::uint32_t end_col = 0;
		std::string_view row;
		std::string_view ident;
		bool member_access = false;
	};

	auto hover_panel_code_hit(
		const gse::gui::draw_context& ctx,
		const hover_state& h,
		gse::vec2f mouse
	) -> std::optional<panel_code_hit>;

	auto highlight_code_body(
		std::string_view body,
		const search::index_state* index
	) -> std::vector<gse::gui::text_span>;

	auto resolve_hover_card(
		hover_state& h,
		std::string_view ident,
		std::string_view row,
		std::size_t ident_start,
		bool member_access,
		const search::index_state* index,
		docs::cppref_index& cppref,
		std::string qualified,
		std::string sym_kind,
		std::optional<search::location> def
	) -> void;

	struct module_link {
		std::string name;
		std::uint32_t start_col = 0;
		std::uint32_t end_col = 0;
	};

	auto module_name_at(
		std::string_view row,
		std::uint32_t column
	) -> std::optional<module_link>;

	auto draw_code_line(
		const gse::gui::draw_context& ctx,
		std::string_view text,
		std::span<const gse::gui::text_span> spans,
		std::uint32_t line,
		gse::vec2f origin,
		float scale,
		gse::vec4f fallback,
		const rectf& clip,
		std::uint32_t z
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

auto gse::ide::editor_layout_path() -> std::filesystem::path {
	return config::resource_path / "editor_layout.ini";
}

auto gse::ide::trim_layout_value(const std::string_view value) -> std::string_view {
	std::size_t start = 0;
	while (start < value.size() && (value[start] == ' ' || value[start] == '\t' || value[start] == '\r' || value[start] == '\n')) {
		++start;
	}
	std::size_t end = value.size();
	while (end > start && (value[end - 1] == ' ' || value[end - 1] == '\t' || value[end - 1] == '\r' || value[end - 1] == '\n')) {
		--end;
	}
	return value.substr(start, end - start);
}

auto gse::ide::layout_section_name(const std::string_view line) -> std::string_view {
	const std::string_view trimmed = trim_layout_value(line);
	if (trimmed.size() < 2 || trimmed.front() != '[' || trimmed.back() != ']') {
		return {};
	}
	return trimmed.substr(1, trimmed.size() - 2);
}

auto gse::ide::parse_layout_sections(const std::string_view text) -> std::vector<layout_section> {
	std::vector<layout_section> sections;
	layout_section* current = nullptr;

	std::size_t pos = 0;
	while (pos < text.size()) {
		const std::size_t line_end = text.find('\n', pos);
		const std::string_view line = text.substr(
			pos,
			line_end == std::string_view::npos ? text.size() - pos : line_end - pos
		);
		pos = line_end == std::string_view::npos ? text.size() : line_end + 1;

		const std::string_view trimmed = trim_layout_value(line);
		if (trimmed.empty() || trimmed.front() == '#') {
			continue;
		}

		if (const std::string_view name = layout_section_name(trimmed); !name.empty()) {
			sections.push_back({ .name = std::string(name) });
			current = &sections.back();
			continue;
		}

		if (!current) {
			continue;
		}

		const std::size_t eq = trimmed.find('=');
		if (eq == std::string_view::npos) {
			continue;
		}

		const std::string key(trim_layout_value(trimmed.substr(0, eq)));
		const std::string value(trim_layout_value(trimmed.substr(eq + 1)));
		current->values[key] = value;
	}

	return sections;
}

auto gse::ide::replace_layout_sections(layout_store::owner sections, const std::string_view block) -> void {
	layout_store::submit(editor_layout_path(), std::move(sections), std::string(block));
}

auto gse::ide::parse_layout_float(const std::string& value, const float fallback) -> float {
	try {
		return std::stof(value);
	}
	catch (...) {
		return fallback;
	}
}

auto gse::ide::parse_layout_uint(const std::string& value, const std::uint32_t fallback) -> std::uint32_t {
	try {
		return static_cast<std::uint32_t>(std::stoul(value));
	}
	catch (...) {
		return fallback;
	}
}

auto gse::ide::editor_layout_owner() -> layout_store::owner {
	return {
		.names = { "editor" },
	};
}

auto gse::ide::workspace_layout_owner() -> layout_store::owner {
	return {
		.names = { "workspace" },
		.prefixes = { "document " },
	};
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
			if (!m_loc_label) {
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
		build_runner::start_rebuild();
	}

	if (m_index) {
		std::string status;
		bool spinning = false;
		gse::vec4f pill_color = ctx.style.color_input_background;
		gse::vec4f pill_fg = ctx.style.color_text_secondary;
		const search::index_phase phase = m_index->phase.load(std::memory_order_acquire);
		if (phase != search::index_phase::idle) {
			const std::size_t total = m_index->tus_total.load(std::memory_order_acquire);
			const std::size_t done = std::min(m_index->tus_done.load(std::memory_order_acquire), total);
			switch (phase) {
				case search::index_phase::scanning: status = "Scanning project"; break;
				case search::index_phase::compiling: status = std::format("Indexing {}/{}", done, total); break;
				case search::index_phase::retrying: status = std::format("Retrying {}/{}", done, total); break;
				case search::index_phase::aggregating: status = "Building index"; break;
				default: break;
			}
			spinning = true;
			pill_color = ctx.style.color_accent_dim;
			pill_fg = ctx.style.color_accent;
		}
		else if (const std::size_t syms = m_index->symbol_count.load(std::memory_order_acquire); syms > 0) {
			status = syms >= 1000 ? std::format("{}k symbols", (syms + 500) / 1000) : std::format("{} symbols", syms);
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
		}
	}

	m_channels.push<gse::window_chrome_metrics_request>({
		.caption_height = static_cast<int>(bar_height),
		.controls_width = static_cast<int>(button_w * 5.f),
		.interactive_x0 = 0,
		.interactive_x1 = 0,
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
	gse::gui::draw::text_input_in_rect(ctx, search_id, state.query, state.input, search_rect, ui.hot_widget_id, ui.focus_widget_id);
	const bool focused = ui.focus_widget_id == search_id;

	if (state.query.empty() && !focused) {
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
	if (state.query != state.last_query) {
		if (state.pending) {
			state.pending->cancelled.store(true, std::memory_order_release);
		}
		state.last_query = state.query;
		state.changed_at = now;
		state.dirty = true;
		state.selected = -1;
	}
	if (state.dirty && now - state.changed_at > gse::milliseconds(120)) {
		state.dirty = false;
		if (state.query.empty() || !index) {
			state.results.clear();
			state.pending.reset();
		}
		else {
			if (state.pending) {
				state.pending->cancelled.store(true, std::memory_order_release);
			}
			state.pending = std::make_shared<search::query_buffer>();
			search::engine::submit(state.pending, *index, state.query, search::options{ .max_results = 8 });
		}
	}
	if (state.pending && state.pending->done.load(std::memory_order_acquire)) {
		state.results = std::move(state.pending->results);
		if (state.results.size() > 8) {
			state.results.resize(8);
		}
		state.pending.reset();
	}

	if ((!was_focused && !focused) || state.results.empty()) {
		return;
	}

	if (was_focused && !state.query.empty()) {
		if (ctx.input.key_pressed(gse::key::down)) {
			state.selected = std::min<int>(state.selected + 1, static_cast<int>(state.results.size()) - 1);
		}
		if (ctx.input.key_pressed(gse::key::up)) {
			state.selected = std::max(state.selected - 1, 0);
		}
		if (ctx.input.key_pressed(gse::key::enter)) {
			const int idx = state.selected >= 0 ? state.selected : 0;
			const search::result& r = state.results[static_cast<std::size_t>(idx)];
			channels.push<jump_to_request>({ .path = r.path, .line = r.line, .column = r.column });
			state.query.clear();
			state.last_query.clear();
			state.results.clear();
			ui.focus_widget_id = {};
			return;
		}
	}

	const auto layer = ctx.scoped_layer(gse::render_layer::popup);
	const float row_h = ctx.fonts.text->line_height(sty.font_size) + pad * 0.5f;
	const gse::rectf list_rect = gse::rectf::from_position_size(
		{ search_rect.left(), search_rect.bottom() - 2.f * sty.scale_factor },
		{ search_rect.width(), row_h * static_cast<float>(state.results.size()) }
	);
	ctx.queue_sprite({
		.rect = list_rect,
		.color = { sty.color_menu_body.x(), sty.color_menu_body.y(), sty.color_menu_body.z(), 1.f },
		.texture = ctx.blank_texture,
		.corner_radius = sty.corner_radius,
	});
	const gse::vec2f mouse = ctx.input.mouse_position();
	const bool clicked = ctx.input.mouse_button_pressed(gse::mouse_button::button_1);
	for (std::size_t i = 0; i < state.results.size(); ++i) {
		const gse::rectf row = gse::rectf::from_position_size(
			{ list_rect.left(), list_rect.top() - row_h * static_cast<float>(i) },
			{ list_rect.width(), row_h }
		);
		const bool over = row.contains(mouse);
		if (over) {
			state.selected = static_cast<int>(i);
		}
		if (state.selected == static_cast<int>(i)) {
			ctx.queue_sprite({
				.rect = row,
				.color = sty.color_selection,
				.texture = ctx.blank_texture,
			});
		}
		const search::result& r = state.results[i];
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
			channels.push<jump_to_request>({ .path = r.path, .line = r.line, .column = r.column });
			state.query.clear();
			state.last_query.clear();
			state.results.clear();
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

	ui.scroll_region({ .id = "explorer_tree" }, [&](gse::gui::builder& b) {
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
		
		if (key != ws.last_opened_key) {
			if (const fs_node* node = workspace::find_node(ws.fs_root, key); node && !node->is_dir) {
				workspace::open_file(ws, node->path);
				ws.last_opened_key = key;
			}
		}
	}
}

auto gse::ide::spinner_rotation() -> gse::angle {
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

auto gse::ide::code_position_at(const gse::gui::draw_context& ctx, const gse::gui::text_buffer& buffer, const gse::gui::text_area_state& view, const gse::rectf& rect, const bool show_line_numbers, const std::size_t display_tab_width, const gse::vec2f mouse) -> gse::gui::buffer_position {
	return gse::gui::draw::text_area_position_at(ctx, buffer, view, rect, show_line_numbers, display_tab_width, mouse);
}

auto gse::ide::hover_wrap_lines(const gse::gui::draw_context& ctx, const std::string_view text, const float max_width, const float scale) -> std::vector<std::string> {
	std::vector<std::string> out;
	std::size_t para_start = 0;
	while (para_start <= text.size()) {
		const std::size_t nl = text.find('\n', para_start);
		const std::string_view para = text.substr(para_start, (nl == std::string_view::npos ? text.size() : nl) - para_start);
		std::string current;
		std::size_t i = 0;
		while (i < para.size()) {
			const std::size_t sp = para.find(' ', i);
			const std::string_view word = para.substr(i, (sp == std::string_view::npos ? para.size() : sp) - i);
			i = (sp == std::string_view::npos) ? para.size() : sp + 1;
			if (word.empty()) {
				continue;
			}
			std::string candidate = current.empty() ? std::string(word) : current + " " + std::string(word);
			if (!current.empty() && ctx.fonts.text->width(candidate, scale) > max_width) {
				out.push_back(std::move(current));
				current = std::string(word);
			}
			else {
				current = std::move(candidate);
			}
		}
		if (!current.empty()) {
			out.push_back(std::move(current));
		}
		if (nl == std::string_view::npos) {
			break;
		}
		para_start = nl + 1;
	}
	if (out.size() > 14) {
		out.resize(14);
		out.back() += " ...";
	}
	return out;
}

auto gse::ide::draw_code_line(const gse::gui::draw_context& ctx, const std::string_view text, const std::span<const gse::gui::text_span> spans, const std::uint32_t line, const gse::vec2f origin, const float scale, const gse::vec4f fallback, const rectf& clip, const std::uint32_t z) -> void {
	const std::vector<float> offsets = ctx.fonts.code->caret_offsets(text, scale);
	const float baseline = origin.y() + ctx.fonts.code->vertical_center_offset(scale);
	const std::uint32_t len = static_cast<std::uint32_t>(text.size());
	std::uint32_t pos = 0;
	for (const gse::gui::text_span& sp : spans) {
		if (sp.line != line) {
			continue;
		}
		const std::uint32_t a = std::min<std::uint32_t>(sp.start_col, len);
		const std::uint32_t b = std::min<std::uint32_t>(sp.end_col, len);
		if (a > pos) {
			ctx.queue_text({ .font = ctx.fonts.code, .text = std::string(text.substr(pos, a - pos)), .position = { origin.x() + offsets[pos], baseline }, .scale = scale, .color = fallback, .clip_rect = clip, .z_order = z });
		}
		if (b > a) {
			ctx.queue_text({ .font = ctx.fonts.code, .text = std::string(text.substr(a, b - a)), .position = { origin.x() + offsets[a], baseline }, .scale = scale, .color = sp.color, .clip_rect = clip, .z_order = z });
		}
		if (b > pos) {
			pos = b;
		}
	}
	if (pos < len) {
		ctx.queue_text({ .font = ctx.fonts.code, .text = std::string(text.substr(pos)), .position = { origin.x() + offsets[pos], baseline }, .scale = scale, .color = fallback, .clip_rect = clip, .z_order = z });
	}
}

auto gse::ide::point_in_triangle(const gse::vec2f p, const gse::vec2f a, const gse::vec2f b, const gse::vec2f c) -> bool {
	const float d1 = (p.x() - b.x()) * (a.y() - b.y()) - (a.x() - b.x()) * (p.y() - b.y());
	const float d2 = (p.x() - c.x()) * (b.y() - c.y()) - (b.x() - c.x()) * (p.y() - c.y());
	const float d3 = (p.x() - a.x()) * (c.y() - a.y()) - (c.x() - a.x()) * (p.y() - a.y());
	const bool neg = d1 < 0.f || d2 < 0.f || d3 < 0.f;
	const bool pos = d1 > 0.f || d2 > 0.f || d3 > 0.f;
	return !(neg && pos);
}

auto gse::ide::hover_kept_alive(const hover_state& h, const gse::vec2f mouse) -> bool {
	if (h.panel.width() <= 0.f) {
		return false;
	}
	if (h.panel.contains(mouse)) {
		return true;
	}
	const bool left = h.anchor.x() <= h.panel.left();
	const bool right = h.anchor.x() >= h.panel.right();
	const bool above = h.anchor.y() >= h.panel.top();
	const bool below = h.anchor.y() <= h.panel.bottom();
	gse::vec2f c0;
	gse::vec2f c1;
	if ((left && above) || (right && below)) {
		c0 = h.panel.top_right();
		c1 = h.panel.bottom_left();
	}
	else if ((right && above) || (left && below)) {
		c0 = h.panel.top_left();
		c1 = h.panel.bottom_right();
	}
	else if (left) {
		c0 = h.panel.top_left();
		c1 = h.panel.bottom_left();
	}
	else if (right) {
		c0 = h.panel.top_right();
		c1 = h.panel.bottom_right();
	}
	else if (above) {
		c0 = h.panel.top_left();
		c1 = h.panel.top_right();
	}
	else {
		c0 = h.panel.bottom_left();
		c1 = h.panel.bottom_right();
	}
	return point_in_triangle(mouse, h.anchor, c0, c1);
}

auto gse::ide::hover_panel_code_hit(const gse::gui::draw_context& ctx, const hover_state& h, const gse::vec2f mouse) -> std::optional<panel_code_hit> {
	if (!h.body_is_code || h.code_rect.width() <= 0.f || !h.code_rect.contains(mouse)) {
		return std::nullopt;
	}
	const float fs = ctx.style.font_size;
	const float line_h = ctx.fonts.code->line_height(fs) * 1.25f;
	const float rel = (h.code_rect.top() + h.scroll - mouse.y()) / line_h;
	if (rel < 0.f) {
		return std::nullopt;
	}
	const std::size_t li = static_cast<std::size_t>(rel);
	std::string_view row;
	std::size_t cs = 0;
	std::size_t current = 0;
	bool found = false;
	while (cs <= h.body.size()) {
		const std::size_t nl = h.body.find('\n', cs);
		if (current == li) {
			row = std::string_view(h.body).substr(cs, (nl == std::string::npos ? h.body.size() : nl) - cs);
			found = true;
			break;
		}
		if (nl == std::string::npos) {
			break;
		}
		cs = nl + 1;
		++current;
	}
	if (!found) {
		return std::nullopt;
	}
	const std::vector<float> offsets = ctx.fonts.code->caret_offsets(row, fs);
	const float x = mouse.x() - (h.panel.left() + ctx.style.padding);
	std::size_t col = row.size();
	for (std::size_t j = 0; j + 1 < offsets.size(); ++j) {
		if (x >= offsets[j] && x < offsets[j + 1]) {
			col = j;
			break;
		}
	}
	if (col >= row.size()) {
		return std::nullopt;
	}
	std::size_t a = col;
	std::size_t b = col;
	while (a > 0 && (row[a - 1] == '_' || std::isalnum(static_cast<unsigned char>(row[a - 1])))) {
		--a;
	}
	while (b < row.size() && (row[b] == '_' || std::isalnum(static_cast<unsigned char>(row[b])))) {
		++b;
	}
	if (b == a || std::isdigit(static_cast<unsigned char>(row[a]))) {
		return std::nullopt;
	}
	return panel_code_hit{
		.line = static_cast<std::uint32_t>(li),
		.start_col = static_cast<std::uint32_t>(a),
		.end_col = static_cast<std::uint32_t>(b),
		.row = row,
		.ident = row.substr(a, b - a),
		.member_access = a > 0 && (row[a - 1] == '.' || (a >= 2 && row[a - 1] == '>' && row[a - 2] == '-')),
	};
}

auto gse::ide::highlight_code_body(const std::string_view body, const search::index_state* index) -> std::vector<gse::gui::text_span> {
	syntax_producer::name_kinds kinds;
	if (index) {
		for (const std::string_view name : syntax_producer::identifier_names(body)) {
			if (const std::optional<analysis::semantic_kind> k = index->semantic_kind_of(name)) {
				kinds.emplace(name, *k);
			}
		}
	}
	return syntax_producer::highlight(body, nullptr, kinds.empty() ? nullptr : &kinds);
}

auto gse::ide::resolve_hover_card(hover_state& h, const std::string_view ident, const std::string_view row, const std::size_t ident_start, const bool member_access, const search::index_state* index, docs::cppref_index& cppref, std::string qualified, std::string sym_kind, std::optional<search::location> def) -> void {
	if (qualified.empty()) {
		qualified = docs::expand_qualified(row, ident_start);
	}
	if (!def && index && !member_access) {
		std::string qualifier;
		if (const std::size_t sep = qualified.rfind("::"); sep != std::string::npos) {
			qualifier = qualified.substr(0, sep + 2);
		}
		if (const std::optional<search::hover_hit> decl = index->declaration_of(ident, qualifier)) {
			def = decl->def;
			if (sym_kind.empty()) {
				sym_kind = decl->kind;
			}
			if (!decl->qualified.empty()) {
				qualified = decl->qualified;
			}
		}
	}
	if (const std::optional<docs::cppref_hit> cref = cppref.find(docs::normalize_qualified(qualified))) {
		h.title = qualified;
		h.kind = std::string(cref->kind);
		h.body = std::string(cref->brief);
		h.url = docs::cppref_url(cref->page);
		h.from_cppref = true;
		h.has_card = true;
	}
	else if (def && !def->path.empty()) {
		if (const std::optional<docs::doc_card> card = docs::extract_definition(def->path, def->line, qualified, sym_kind == "function")) {
			h.title = card->title;
			h.kind = sym_kind;
			h.body = card->body;
			h.body_is_code = true;
			h.code_spans = highlight_code_body(h.body, index);
			h.has_card = true;
		}
	}
	if (!h.has_card && !qualified.empty()) {
		h.title = qualified;
		h.kind = sym_kind;
		h.has_card = true;
	}
}

auto gse::ide::module_name_at(const std::string_view row, const std::uint32_t column) -> std::optional<module_link> {
	auto is_ident_start = [](const char ch) {
		return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
	};
	auto is_ident_continue = [&](const char ch) {
		return is_ident_start(ch) || (ch >= '0' && ch <= '9');
	};
	auto skip_ws = [](const std::string_view text, std::size_t pos) {
		while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t')) {
			++pos;
		}
		return pos;
	};
	auto consume_keyword = [&](const std::string_view text, std::size_t& pos, const std::string_view keyword) {
		if (text.substr(pos, keyword.size()) != keyword) {
			return false;
		}
		const std::size_t end = pos + keyword.size();
		if (end < text.size() && is_ident_continue(text[end])) {
			return false;
		}
		pos = end;
		return true;
	};
	auto parse_name = [&](std::size_t pos) -> std::optional<module_link> {
		const std::size_t start = pos;
		bool need_ident = true;
		bool saw_colon = false;
		while (pos < row.size()) {
			const char ch = row[pos];
			if (is_ident_start(ch)) {
				++pos;
				while (pos < row.size() && is_ident_continue(row[pos])) {
					++pos;
				}
				need_ident = false;
				continue;
			}
			if (ch == '.' && !need_ident) {
				++pos;
				need_ident = true;
				continue;
			}
			if (ch == ':' && !saw_colon) {
				++pos;
				saw_colon = true;
				need_ident = true;
				continue;
			}
			break;
		}
		if (pos == start || need_ident || column < start || column >= pos) {
			return std::nullopt;
		}
		const std::string name(row.substr(start, pos - start));
		if (name == ":private") {
			return std::nullopt;
		}
		return module_link{
			.name = name,
			.start_col = static_cast<std::uint32_t>(start),
			.end_col = static_cast<std::uint32_t>(pos),
		};
	};

	std::size_t pos = skip_ws(row, 0);
	if (pos >= row.size() || row[pos] == '/' || row[pos] == '*') {
		return std::nullopt;
	}
	if (consume_keyword(row, pos, "export")) {
		pos = skip_ws(row, pos);
	}
	if (consume_keyword(row, pos, "import")) {
		pos = skip_ws(row, pos);
		return parse_name(pos);
	}
	if (consume_keyword(row, pos, "module")) {
		pos = skip_ws(row, pos);
		if (pos >= row.size() || row[pos] == ';') {
			return std::nullopt;
		}
		return parse_name(pos);
	}
	return std::nullopt;
}

auto gse::ide::draw_hover_panel(const gse::gui::draw_context& ctx, const rectf& text_rect, hover_state& h, const std::uint32_t z) -> bool {
	const auto& sty = ctx.style;
	const float pad = sty.padding;
	const float fs = sty.font_size;
	const float line_h = ctx.fonts.text->line_height(fs) * 1.25f;

	if (h.body_is_code) {
		constexpr std::size_t max_visible = 18;
		const float max_w = 640.f;

		std::vector<std::string> header;
		header.push_back(h.title);
		if (!h.kind.empty()) {
			header.push_back(h.kind);
		}

		std::vector<std::string_view> code;
		std::size_t cs = 0;
		while (cs <= h.body.size()) {
			const std::size_t nl = h.body.find('\n', cs);
			code.push_back(std::string_view(h.body).substr(cs, (nl == std::string::npos ? h.body.size() : nl) - cs));
			if (nl == std::string::npos) {
				break;
			}
			cs = nl + 1;
		}
		const std::size_t visible = std::min(code.size(), max_visible);

		float content_w = 0.f;
		for (const std::string& l : header) {
			content_w = std::max(content_w, ctx.fonts.code->width(l, fs));
		}
		for (const std::string_view l : code) {
			content_w = std::max(content_w, ctx.fonts.code->width(l, fs));
		}
		const float header_rows = static_cast<float>(header.size());
		const float pw = std::min(max_w, content_w) + pad * 2.f;
		const float ph = (header_rows + static_cast<float>(visible)) * line_h + pad * 2.f;

		const float area_top = text_rect.top();
		const float area_bottom = text_rect.top() - text_rect.height();
		float px = h.anchor.x() + 16.f;
		if (px + pw > text_rect.right()) {
			px = text_rect.right() - pw;
		}
		if (px < text_rect.left()) {
			px = text_rect.left();
		}
		float top_y = h.anchor.y() - 16.f;
		if (top_y - ph < area_bottom) {
			top_y = h.anchor.y() + 16.f + ph;
		}
		if (top_y > area_top) {
			top_y = area_top;
		}

		const rectf panel = rectf::from_position_size({ px, top_y }, { pw, ph });
		h.panel = panel;

		const float view_h = static_cast<float>(visible) * line_h;
		const float content_h = static_cast<float>(code.size()) * line_h;
		const float max_scroll = std::max(0.f, content_h - view_h);
		const gse::vec2f wheel = ctx.scroll_delta_for(panel);
		h.scroll = std::clamp(h.scroll - wheel.y() * line_h * 2.f, 0.f, max_scroll);

		const auto scope = ctx.scoped_layer(gse::render_layer::popup);
		ctx.queue_sprite({
			.rect = rectf::from_position_size({ px + 4.f, top_y - 4.f }, { pw, ph }),
			.color = sty.color_shadow,
			.texture = ctx.blank_texture,
			.z_order = z,
			.corner_radius = sty.corner_radius_menu,
		});
		ctx.queue_sprite({
			.rect = panel,
			.color = { sty.color_menu_body.x(), sty.color_menu_body.y(), sty.color_menu_body.z(), 1.f },
			.texture = ctx.blank_texture,
			.z_order = z,
			.corner_radius = sty.corner_radius_menu,
		});

		for (std::size_t i = 0; i < header.size(); ++i) {
			const float center_y = top_y - pad - line_h * (static_cast<float>(i) + 0.5f);
			ctx.queue_text({
				.font = ctx.fonts.code,
				.text = header[i],
				.position = { px + pad, center_y + ctx.fonts.code->vertical_center_offset(fs) },
				.scale = fs,
				.color = i == 0 ? sty.color_text : h.kind_color,
				.clip_rect = panel,
				.z_order = z,
			});
		}

		const float code_top = top_y - pad - header_rows * line_h;
		const rectf code_rect = rectf::from_position_size({ px, code_top }, { pw, view_h });
		h.code_rect = code_rect;
		for (std::size_t li = 0; li < code.size(); ++li) {
			const float center_y = code_top - line_h * (static_cast<float>(li) + 0.5f) + h.scroll;
			if (center_y > code_top + line_h || center_y < code_top - view_h - line_h) {
				continue;
			}
			draw_code_line(ctx, code[li], h.code_spans, static_cast<std::uint32_t>(li), { px + pad, center_y }, fs, sty.color_text, code_rect, z);
		}

		if (max_scroll > 0.f) {
			const float thumb_h = std::max(24.f, view_h * view_h / content_h);
			const float thumb_top = code_top - (h.scroll / max_scroll) * (view_h - thumb_h);
			ctx.queue_sprite({
				.rect = rectf::from_position_size({ px + pw - 4.f, thumb_top }, { 3.f, thumb_h }),
				.color = sty.color_border,
				.texture = ctx.blank_texture,
				.clip_rect = panel,
				.z_order = z,
			});
		}

		return false;
	}

	const float max_w = 460.f;

	std::vector<std::string> lines;
	std::vector<int> roles;
	lines.push_back(h.title);
	roles.push_back(0);
	if (!h.kind.empty()) {
		lines.push_back(h.kind);
		roles.push_back(1);
	}
	if (!h.body.empty() && h.body != h.title) {
		for (std::string& bl : hover_wrap_lines(ctx, h.body, max_w, fs)) {
			lines.push_back(std::move(bl));
			roles.push_back(2);
		}
	}
	if (!h.url.empty()) {
		lines.emplace_back("cppreference");
		roles.push_back(3);
	}

	float content_w = 0.f;
	for (const std::string& l : lines) {
		content_w = std::max(content_w, ctx.fonts.text->width(l, fs));
	}
	const float pw = std::min(max_w, content_w) + pad * 2.f;
	const float ph = line_h * static_cast<float>(lines.size()) + pad * 2.f;

	const float area_top = text_rect.top();
	const float area_bottom = text_rect.top() - text_rect.height();
	float px = h.anchor.x() + 16.f;
	if (px + pw > text_rect.right()) {
		px = text_rect.right() - pw;
	}
	if (px < text_rect.left()) {
		px = text_rect.left();
	}
	float top_y = h.anchor.y() - 16.f;
	if (top_y - ph < area_bottom) {
		top_y = h.anchor.y() + 16.f + ph;
	}
	if (top_y > area_top) {
		top_y = area_top;
	}

	const rectf panel = rectf::from_position_size({ px, top_y }, { pw, ph });
	h.panel = panel;
	const gse::vec2f mouse = ctx.input.mouse_position();
	bool link_clicked = false;

	const auto scope = ctx.scoped_layer(gse::render_layer::popup);
	ctx.queue_sprite({
		.rect = rectf::from_position_size({ px + 4.f, top_y - 4.f }, { pw, ph }),
		.color = sty.color_shadow,
		.texture = ctx.blank_texture,
		.z_order = z,
		.corner_radius = sty.corner_radius_menu,
	});
	ctx.queue_sprite({
		.rect = panel,
		.color = { sty.color_menu_body.x(), sty.color_menu_body.y(), sty.color_menu_body.z(), 1.f },
		.texture = ctx.blank_texture,
		.z_order = z,
		.corner_radius = sty.corner_radius_menu,
	});
	for (std::size_t i = 0; i < lines.size(); ++i) {
		const float center_y = top_y - pad - line_h * (static_cast<float>(i) + 0.5f);
		gse::vec4f col = sty.color_text_secondary;
		if (roles[i] == 0) {
			col = sty.color_text;
		}
		else if (roles[i] == 1) {
			col = h.kind_color;
		}
		else if (roles[i] == 3) {
			const rectf link_rect = rectf::from_position_size({ px + pad, center_y + line_h * 0.5f }, { ctx.fonts.text->width(lines[i], fs), line_h });
			const bool over = link_rect.contains(mouse) && ctx.input_available();
			col = over ? sty.color_text : sty.color_accent;
			if (over && ctx.input.mouse_button_pressed(gse::mouse_button::button_1)) {
				link_clicked = true;
			}
		}
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = lines[i],
			.position = { px + pad, center_y + ctx.fonts.text->vertical_center_offset(fs) },
			.scale = fs,
			.color = col,
			.clip_rect = panel,
			.z_order = z,
		});
	}
	return link_clicked;
}

auto gse::ide::draw_diagnostic_tooltip(const gse::gui::draw_context& ctx, const rectf& text_rect, const std::string_view label, const std::string_view message, const gse::vec4f label_color, const gse::vec2f anchor) -> void {
	const auto& sty = ctx.style;
	const float pad = sty.padding;
	const float fs = sty.font_size;
	const float line_h = ctx.fonts.text->line_height(fs) * 1.25f;
	const float max_w = 460.f;

	std::vector<std::string> lines;
	lines.emplace_back(label);
	for (std::string& wrapped : hover_wrap_lines(ctx, message, max_w, fs)) {
		lines.push_back(std::move(wrapped));
	}

	float content_w = 0.f;
	for (const std::string& l : lines) {
		content_w = std::max(content_w, ctx.fonts.text->width(l, fs));
	}
	const float pw = std::min(max_w, content_w) + pad * 2.f;
	const float ph = line_h * static_cast<float>(lines.size()) + pad * 2.f;

	const float area_top = text_rect.top();
	const float area_bottom = text_rect.top() - text_rect.height();
	float px = anchor.x() + 16.f;
	if (px + pw > text_rect.right()) {
		px = text_rect.right() - pw;
	}
	if (px < text_rect.left()) {
		px = text_rect.left();
	}
	float top_y = anchor.y() - 16.f;
	if (top_y - ph < area_bottom) {
		top_y = anchor.y() + 16.f + ph;
	}
	if (top_y > area_top) {
		top_y = area_top;
	}

	const rectf panel = rectf::from_position_size({ px, top_y }, { pw, ph });
	const auto scope = ctx.scoped_layer(gse::render_layer::popup);
	ctx.queue_sprite({
		.rect = rectf::from_position_size({ px + 4.f, top_y - 4.f }, { pw, ph }),
		.color = sty.color_shadow,
		.texture = ctx.blank_texture,
		.corner_radius = sty.corner_radius_menu,
	});
	ctx.queue_sprite({
		.rect = panel,
		.color = { sty.color_menu_body.x(), sty.color_menu_body.y(), sty.color_menu_body.z(), 1.f },
		.texture = ctx.blank_texture,
		.corner_radius = sty.corner_radius_menu,
	});
	for (std::size_t i = 0; i < lines.size(); ++i) {
		const float center_y = top_y - pad - line_h * (static_cast<float>(i) + 0.5f);
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = lines[i],
			.position = { px + pad, center_y + ctx.fonts.text->vertical_center_offset(fs) },
			.scale = fs,
			.color = i == 0 ? label_color : sty.color_text,
			.clip_rect = panel,
		});
	}
}

auto gse::ide::apply_quickfix(document& doc, const std::span<const text_edit> edits) -> void {
	if (edits.empty()) {
		return;
	}
	doc.view.undo_stack.push_back({ doc.buffer.lines, doc.view.caret, doc.view.anchor });
	doc.view.redo_stack.clear();
	doc.view.last_edit_kind = quickfix_edit_kind;
	if (fix_engine::apply(doc.buffer.lines, edits) == 0) {
		doc.view.undo_stack.pop_back();
		return;
	}
	doc.view.caret = doc.buffer.clamp(doc.view.caret);
	doc.view.anchor = doc.view.caret;
	doc.dirty = true;
	doc.diag_dirty = true;
	doc.highlight_dirty = true;
	doc.last_edit = gse::system_clock::now<gse::time>();
}

auto gse::ide::adjust_after_format(gse::gui::buffer_position& p, const std::span<const format::line_edit> edits) -> void {
	for (const format::line_edit& e : edits) {
		if (e.line != p.line) {
			continue;
		}
		const auto old_len = static_cast<std::uint32_t>(e.expected.size());
		const auto new_len = static_cast<std::uint32_t>(e.replacement.size());
		p.column = p.column >= old_len ? p.column - old_len + new_len : std::min(p.column, new_len);
		break;
	}
}

auto gse::ide::apply_format(document& doc, const format::options& opts) -> void {
	const std::vector<format::line_edit> edits = format::compute(doc.buffer.lines, opts);
	if (edits.empty()) {
		return;
	}
	doc.view.undo_stack.push_back({ doc.buffer.lines, doc.view.caret, doc.view.anchor });
	doc.view.redo_stack.clear();
	doc.view.last_edit_kind = format_edit_kind;
	if (format::apply(doc.buffer.lines, edits) == 0) {
		doc.view.undo_stack.pop_back();
		return;
	}
	adjust_after_format(doc.view.caret, edits);
	adjust_after_format(doc.view.anchor, edits);
	doc.view.caret = doc.buffer.clamp(doc.view.caret);
	doc.view.anchor = doc.buffer.clamp(doc.view.anchor);
	doc.dirty = true;
	doc.diag_dirty = true;
	doc.highlight_dirty = true;
	doc.last_edit = gse::system_clock::now<gse::time>();
}

auto gse::ide::format_and_save(workspace::data& ws, const std::uint32_t document_id, const format::options& opts, const bool format_enabled, const bool force_save, gse::channel_writer channels) -> void {
	const auto it = ws.documents.find(document_id);
	if (it == ws.documents.end()) {
		return;
	}
	document& doc = it->second;
	if (format_enabled && doc.highlightable) {
		apply_format(doc, opts);
	}
	if (force_save || doc.dirty) {
		workspace::save_document(ws, document_id);
		channels.push<git_system::refresh_request>({});
	}
}

auto gse::ide::apply_diagnostics(workspace::data& ws, const std::shared_ptr<analysis::diagnostics_check>& check, gse::channel_writer channels) -> void {
	const auto pending = ws.documents.find(check->document_id);
	if (pending == ws.documents.end()) {
		return;
	}

	document& doc = pending->second;
	doc.analysis_failed = check->failed;
	doc.analysis_crashed = check->crashed;
	doc.analysis_duration = check->duration;
	if (check->crashed) {
		return;
	}

	doc.diagnostics = std::move(check->result);
	doc.lint = std::move(check->lint);
	syntax_producer::set_semantic(doc.syntax, check->tokens, doc.buffer);
	doc.highlight_dirty = true;

	if (check->symbols_complete) {
		channels.push<search::index_merge_request>({
			.path = doc.path,
			.check = check,
		});
	}

	for (const diagnostic& diag : doc.diagnostics) {
		const gse::log::level level = diag.level == severity::error
			? gse::log::level::error
			: (diag.level == severity::warning ? gse::log::level::warning : gse::log::level::info);
		const std::filesystem::path diagnostic_path = diag.file.empty()
			? doc.path
			: diag.file;
		std::error_code relative_ec;
		const std::filesystem::path relative_path = std::filesystem::relative(diagnostic_path, ws.root, relative_ec);
		const std::string where = relative_ec ? diagnostic_path.generic_display_string() : relative_path.generic_display_string();
		gse::log::println(level, gse::log::category::task, "analysis: {}:{}:{}: {}", where, diag.line + 1, diag.start_col + 1, diag.message);
	}
}

auto gse::ide::update_diagnostics(gse::context& ctx, workspace::data& ws, const gse::shared_view<config_system::data> config) -> void {
	for (const analysis::diagnostics_completed& completed : ctx.read_channel<analysis::diagnostics_completed>()) {
		if (!completed.check) {
			continue;
		}
		apply_diagnostics(ws, completed.check, ctx.channels);
		if (ws.diagnostics_pending == completed.check->document_id) {
			ws.diagnostics_pending.reset();
		}
	}

	if (ws.diagnostics_pending) {
		return;
	}

	const gse::time now = gse::system_clock::now<gse::time>();
	auto ready = [now](const document& doc) {
		return doc.highlightable
			&& !doc.path.empty()
			&& doc.diag_dirty
			&& now - doc.last_edit > gse::milliseconds(500);
	};

	auto candidate = ws.documents.end();
	if (const auto active = ws.documents.find(ws.active_document_id); active != ws.documents.end() && ready(active->second)) {
		candidate = active;
	}
	else {
		for (const std::uint32_t document_id : ws.tab_order) {
			const auto document = ws.documents.find(document_id);
			if (document != ws.documents.end() && ready(document->second)) {
				candidate = document;
				break;
			}
		}
	}
	if (candidate == ws.documents.end()) {
		return;
	}

	document& doc = candidate->second;
	if (doc.dirty) {
		format_and_save(ws, candidate->first, {
			.indent_width = config.indent_width,
			.indent_with_spaces = config.indent_with_spaces,
		}, config.format_on_save, false, ctx.channels);
	}

	if (const std::optional<std::filesystem::path> compile_commands = analysis::diagnostics_runner::find_compile_commands(ws.root)) {
		std::error_code plugin_ec;
		const std::filesystem::path plugin = std::filesystem::exists(config::token_plugin, plugin_ec)
			? config::token_plugin
			: std::filesystem::path{};
		ctx.channels.push<analysis::diagnostics_request>({
			.document_id = candidate->first,
			.compile_commands = *compile_commands,
			.file = doc.path,
			.plugin = plugin,
			.lint_hook = &lint::analyze_check,
		});
		ws.diagnostics_pending = candidate->first;
	}
	doc.diag_dirty = false;
}

auto gse::ide::draw_quickfix_tooltip(const gse::gui::draw_context& ctx, const rectf& text_rect, const std::string_view label, const gse::vec4f label_color, const std::string_view message, const std::string_view fix_title, const int fixall_count, const gse::vec2f anchor, const gse::vec2f mouse) -> quickfix_layout {
	const auto& sty = ctx.style;
	const float pad = sty.padding;
	const float fs = sty.font_size;
	const float line_h = ctx.fonts.text->line_height(fs) * 1.25f;
	const float max_w = 460.f;

	std::vector<std::string> info;
	info.emplace_back(label);
	for (std::string& wrapped : hover_wrap_lines(ctx, message, max_w, fs)) {
		info.push_back(std::move(wrapped));
	}

	const std::string fix_line = std::string(fix_title);
	const bool has_fixall = fixall_count > 1;
	const std::string fixall_line = has_fixall ? "Fix all in file  (" + std::to_string(fixall_count) + ")" : std::string{};

	float content_w = ctx.fonts.text->width(fix_line, fs);
	for (const std::string& l : info) {
		content_w = std::max(content_w, ctx.fonts.text->width(l, fs));
	}
	if (has_fixall) {
		content_w = std::max(content_w, ctx.fonts.text->width(fixall_line, fs));
	}

	const std::size_t action_rows = has_fixall ? 2u : 1u;
	const float info_h = line_h * static_cast<float>(info.size());
	const float sep_h = line_h * 0.35f;
	const float actions_h = line_h * static_cast<float>(action_rows);
	const float pw = std::min(max_w, content_w) + pad * 2.f;
	const float ph = info_h + sep_h + actions_h + pad * 2.f;

	const float area_top = text_rect.top();
	const float area_bottom = text_rect.top() - text_rect.height();
	float px = anchor.x() - 8.f;
	if (px + pw > text_rect.right()) {
		px = text_rect.right() - pw;
	}
	if (px < text_rect.left()) {
		px = text_rect.left();
	}
	float top_y = anchor.y() + 6.f;
	if (top_y > area_top) {
		top_y = area_top;
	}
	if (top_y - ph < area_bottom) {
		top_y = area_bottom + ph;
	}

	const rectf panel = rectf::from_position_size({ px, top_y }, { pw, ph });
	const auto scope = ctx.scoped_layer(gse::render_layer::popup);
	ctx.queue_sprite({
		.rect = rectf::from_position_size({ px + 4.f, top_y - 4.f }, { pw, ph }),
		.color = sty.color_shadow,
		.texture = ctx.blank_texture,
		.corner_radius = sty.corner_radius_menu,
	});
	ctx.queue_sprite({
		.rect = panel,
		.color = { sty.color_menu_body.x(), sty.color_menu_body.y(), sty.color_menu_body.z(), 1.f },
		.texture = ctx.blank_texture,
		.corner_radius = sty.corner_radius_menu,
	});

	for (std::size_t i = 0; i < info.size(); ++i) {
		const float center_y = top_y - pad - line_h * (static_cast<float>(i) + 0.5f);
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = info[i],
			.position = { px + pad, center_y + ctx.fonts.text->vertical_center_offset(fs) },
			.scale = fs,
			.color = i == 0 ? label_color : sty.color_text,
			.clip_rect = panel,
		});
	}

	auto action_rect = [&](const std::size_t idx) -> rectf {
		const float row_top = top_y - pad - info_h - sep_h - line_h * static_cast<float>(idx);
		return rectf::from_position_size({ px + 2.f, row_top }, { pw - 4.f, line_h });
	};
	auto draw_action = [&](const rectf row, const std::string& text, const gse::vec4f color) {
		if (row.contains(mouse)) {
			ctx.queue_sprite({
				.rect = row,
				.color = sty.color_input_background,
				.texture = ctx.blank_texture,
				.corner_radius = sty.corner_radius_menu,
			});
		}
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = text,
			.position = { px + pad, row.top() - line_h * 0.5f + ctx.fonts.text->vertical_center_offset(fs) },
			.scale = fs,
			.color = color,
			.clip_rect = panel,
		});
	};

	quickfix_layout layout;
	layout.panel = panel;
	layout.fix_row = action_rect(0);
	draw_action(layout.fix_row, fix_line, sty.color_text);
	if (has_fixall) {
		layout.fixall_row = action_rect(1);
		draw_action(layout.fixall_row, fixall_line, sty.color_text_secondary);
	}
	return layout;
}

auto gse::ide::draw_code_panel(gse::gui::builder& ui, workspace::data& ws, gse::ide::graph_data& graph, const search::index_state* index, const gse::shared_view<config_system::data> config, gse::channel_writer channels, const gse::gpu::bindless_slot viewport_slot, const bool game_running) -> void {
	const auto& ctx = ui.ctx;
	if (ctx.clip_stack.empty()) {
		return;
	}
	const gse::rectf body = ctx.clip_stack.back();
	const float pad = ctx.style.padding;
	const float font_sz = ctx.style.font_size;

	const std::uint32_t analyzing_id = ws.diagnostics_pending.value_or(0);

	std::erase_if(ws.tab_order, [&](const std::uint32_t id) {
		return !ws.documents.contains(id);
	});
	std::vector<std::uint32_t> added;
	for (const auto& [id, doc] : ws.documents) {
		if (std::ranges::find(ws.tab_order, id) == ws.tab_order.end()) {
			added.push_back(id);
		}
	}
	std::ranges::sort(added);
	ws.tab_order.insert(ws.tab_order.end(), added.begin(), added.end());

	if (ws.active_document_id != viewport_tab_id && !ws.documents.contains(ws.active_document_id) && !ws.tab_order.empty()) {
		ws.active_document_id = ws.tab_order.front();
	}
	if (ws.active_document_id != viewport_tab_id && !ws.documents.contains(ws.active_document_id) && ws.tab_order.empty()) {
		ws.active_document_id = viewport_tab_id;
	}

	const float row_h = std::min(ctx.fonts.text->line_height(font_sz) + pad, body.height());
	const float tab_gap = 2.f * ctx.style.scale_factor;

	const std::string game_caption = "Game";
	const float game_width = std::clamp(ctx.fonts.text->width(game_caption, font_sz) + pad * 3.f, 64.f * ctx.style.scale_factor, 220.f * ctx.style.scale_factor);
	const float game_divider_width = ctx.style.accent_bar_width;
	const float document_area_width = std::max(0.f, body.width() - game_width - game_divider_width - tab_gap);

	std::vector<gse::gui::tab_desc> tab_descs;
	tab_descs.reserve(ws.tab_order.size());
	for (const std::uint32_t id : ws.tab_order) {
		const document& doc = ws.documents.at(id);
		tab_descs.push_back({
			.id = id,
			.caption = doc.tab_name,
			.dirty = doc.dirty,
			.busy = id == analyzing_id,
		});
	}

	const gse::gui::tab_strip_metrics metrics = gse::gui::tab_strip_measure(ctx.fonts.text, ctx.style, tab_descs, document_area_width);
	ws.tab_strip.visible_rows = std::clamp(ws.tab_strip.visible_rows, 1u, std::max(1u, metrics.required_rows));

	const bool overflow = metrics.content_extent > document_area_width;
	const float tab_scrollbar_h = 6.f * ctx.style.scale_factor;
	const float tab_bar_h = std::min(
		body.height(),
		ws.tab_strip.visible_rows * row_h + static_cast<float>(ws.tab_strip.visible_rows - 1) * tab_gap + (ws.tab_strip.visible_rows == 1 && overflow ? tab_scrollbar_h + tab_gap : 0.f)
	);
	const gse::rectf tab_bar = gse::rectf::from_position_size(
		{ body.left(), body.top() },
		{ body.width(), tab_bar_h }
	);
	const gse::rectf document_tab_bar = gse::rectf::from_position_size(
		{ tab_bar.left(), tab_bar.top() },
		{ document_area_width, tab_bar.height() }
	);
	ctx.queue_sprite({
		.rect = tab_bar,
		.color = ctx.style.color_input_background,
		.texture = ctx.blank_texture,
	});

	const gse::vec2f mouse = ctx.input.mouse_position();

	const gse::gui::tab_strip_result doc_tabs = gse::gui::tab_strip(ctx, {
		.area = document_tab_bar,
		.tabs = tab_descs,
		.active = ws.active_document_id,
		.orientation = gse::gui::tab_orientation::horizontal,
		.overflow = gse::gui::tab_overflow::wrap,
		.allow_reorder = true,
	}, ws.tab_strip);

	std::uint32_t close_requested = 0;
	if (doc_tabs.activated != 0) {
		ws.active_document_id = static_cast<std::uint32_t>(doc_tabs.activated);
	}
	if (doc_tabs.close_requested != 0) {
		close_requested = static_cast<std::uint32_t>(doc_tabs.close_requested);
	}
	if (doc_tabs.context_requested != 0) {
		ws.active_document_id = static_cast<std::uint32_t>(doc_tabs.context_requested);
		ctx.open_context_menu({
			.position = doc_tabs.context_position,
			.items = gse::gui::to_menu_items(tab_actions()),
			.target = static_cast<std::uint32_t>(doc_tabs.context_requested),
			.tag = tab_context_tag(),
		});
	}
	if (doc_tabs.reorder_id != 0) {
		const auto moved = static_cast<std::uint32_t>(doc_tabs.reorder_id);
		if (const auto cur = std::ranges::find(ws.tab_order, moved); cur != ws.tab_order.end()) {
			ws.tab_order.erase(cur);
			ws.tab_order.insert(ws.tab_order.begin() + static_cast<std::ptrdiff_t>(std::min(doc_tabs.reorder_to, ws.tab_order.size())), moved);
		}
	}

	const gse::rectf game_tab = gse::rectf::from_position_size(
		{ tab_bar.right() - game_width, tab_bar.top() },
		{ game_width, tab_bar_h }
	);
	const gse::rectf game_divider = gse::rectf::from_position_size(
		{ game_tab.left() - game_divider_width, tab_bar.top() },
		{ game_divider_width, tab_bar_h }
	);

	if (ctx.input.mouse_button_pressed(gse::mouse_button::button_1) && ctx.input_available() && game_tab.contains(mouse)) {
		ws.active_document_id = viewport_tab_id;
	}

	const bool game_active = ws.active_document_id == viewport_tab_id;
	const bool game_hovered = game_tab.contains(mouse) && ctx.input_available();
	ctx.queue_sprite({
		.rect = game_divider,
		.color = ctx.style.color_accent,
		.texture = ctx.blank_texture,
	});
	ctx.queue_sprite({
		.rect = game_tab,
		.color = game_active ? ctx.style.color_widget_background : (game_hovered ? ctx.style.color_widget_hovered : ctx.style.color_input_background),
		.texture = ctx.blank_texture,
	});
	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = game_caption,
		.position = { game_tab.left() + pad, game_tab.center().y() + ctx.fonts.text->vertical_center_offset(font_sz) },
		.scale = font_sz,
		.color = game_active ? ctx.style.color_text : ctx.style.color_text_secondary,
		.clip_rect = game_tab,
	});
	ctx.queue_sprite({
		.rect = gse::rectf::from_position_size(
			{ tab_bar.left(), tab_bar.bottom() },
			{ tab_bar.width(), ctx.style.accent_bar_width }
		),
		.color = ctx.style.color_accent,
		.texture = ctx.blank_texture,
	});

	if (close_requested != 0) {
		workspace::close_document(ws, close_requested);
	}

	const float requested_status_h = ctx.fonts.text->line_height(font_sz) + pad * 0.5f;
	const float content_h = std::max(0.f, body.height() - tab_bar_h);
	const float status_h = std::min(requested_status_h, content_h);
	const float text_h = content_h - status_h;
	const gse::rectf text_rect = gse::rectf::from_position_size(
		{ body.left(), body.top() - tab_bar_h },
		{ body.width(), text_h }
	);
	const gse::rectf status_rect = gse::rectf::from_position_size(
		{ body.left(), body.bottom() + status_h },
		{ body.width(), status_h }
	);

	if (ws.active_document_id == viewport_tab_id) {
		const gse::rectf view_rect = gse::rectf::from_position_size(
			{ body.left(), body.top() - tab_bar_h },
			{ body.width(), content_h }
		);
		const bool showing_graph = !game_running || ws.show_game_graph;
		if (showing_graph) {
			gse::ide::draw_graph(ui, view_rect, graph, index, channels);
		}
		else {
			ctx.queue_sprite({
				.rect = view_rect,
				.color = { 1.f, 1.f, 1.f, 1.f },
				.image_slot = viewport_slot,
			});
		}

		const bool building = build_runner::in_progress();
		const float button_w = 150.f;
		const float button_h = ctx.fonts.text->line_height(font_sz) + pad;
		const gse::rectf button_rect = gse::rectf::from_position_size(
			{ view_rect.right() - button_w - pad, view_rect.top() - pad },
			{ button_w, button_h }
		);
		const bool button_hovered = button_rect.contains(mouse) && ctx.input_available() && !building;
		if (button_hovered && ctx.mouse_pressed_for(button_rect)) {
			if (game_running) {
				ws.show_game_graph = !ws.show_game_graph;
			}
			else {
				build_runner::start_build_and_run_game();
			}
		}
		const std::string button_label = game_running
			? (ws.show_game_graph ? "View Game" : "View Graph")
			: (building ? "Building..." : "Build & Run");
		ctx.queue_sprite({
			.rect = button_rect,
			.color = building ? ctx.style.color_input_background : (button_hovered ? ctx.style.color_button_hovered : ctx.style.color_button_background),
			.texture = ctx.blank_texture,
			.corner_radius = ctx.style.corner_radius,
		});
		const float button_text_w = ctx.fonts.text->width(button_label, font_sz);
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = button_label,
			.position = { button_rect.center().x() - button_text_w * 0.5f, button_rect.center().y() + ctx.fonts.text->vertical_center_offset(font_sz) },
			.scale = font_sz,
			.color = building ? ctx.style.color_text_secondary : ctx.style.color_text,
			.clip_rect = button_rect,
		});
		return;
	}

	const auto it = ws.documents.find(ws.active_document_id);
	if (it == ws.documents.end()) {
		if (text_rect.width() <= 0.f || text_rect.height() <= 0.f) {
			return;
		}
		const std::string label = "Open a file from the explorer";
		const float w = ctx.fonts.text->width(label, font_sz);
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = label,
			.position = { text_rect.center().x() - w * 0.5f, text_rect.center().y() + ctx.fonts.text->vertical_center_offset(font_sz) },
			.scale = font_sz,
			.color = ctx.style.color_text_secondary,
			.clip_rect = text_rect,
		});
		return;
	}
	document& doc = it->second;

	{
		const bool analyzing = ws.diagnostics_pending == ws.active_document_id;
		int errors = 0;
		int warnings = 0;
		for (const auto& diagnostic : doc.diagnostics) {
			if (!diagnostic.file.empty() && diagnostic.file != doc.path) {
				continue;
			}
			if (diagnostic.level == severity::error) {
				++errors;
			}
			else if (diagnostic.level == severity::warning) {
				++warnings;
			}
		}

		ctx.queue_sprite({
			.rect = status_rect,
			.color = ctx.style.color_input_background,
			.texture = ctx.blank_texture,
		});

		float text_left = status_rect.left() + pad;
		std::string status_text;
		gse::vec4f status_color = ctx.style.color_text_secondary;

		if (analyzing) {
			const float glyph_size = status_rect.height() * 0.62f;
			const gse::rectf spin_rect = gse::rectf::from_position_size(
				{ status_rect.left() + pad, status_rect.center().y() + glyph_size * 0.5f },
				{ glyph_size, glyph_size }
			);
			draw_spinner(ctx, spin_rect, ctx.style.color_text_secondary, spinner_rotation());
			text_left = spin_rect.right() + pad * 0.5f;
			status_text = "analyzing...";
		}
		else if (doc.analysis_failed) {
			status_text = "analysis unavailable - unbuilt dependencies";
			status_color = gse::vec4f{ 0.76f, 0.46f, 0.73f, 1.f };
		}
		else if (doc.analysis_crashed) {
			status_text = "analysis error - analyzer exited abnormally";
			status_color = gse::vec4f{ 0.855f, 0.451f, 0.424f, 1.f };
		}
		else if (doc.highlightable) {
			const int ms = static_cast<int>(doc.analysis_duration / gse::milliseconds(1.f));
			const std::string prefix = ms > 0 ? "Analyzed in " + std::to_string(ms) + " ms - " : "";
			if (errors > 0) {
				status_text = prefix + std::to_string(errors) + (errors == 1 ? " error" : " errors") + (warnings > 0 ? ", " + std::to_string(warnings) + (warnings == 1 ? " warning" : " warnings") : "");
				status_color = gse::vec4f{ 0.855f, 0.451f, 0.424f, 1.f };
			}
			else if (warnings > 0) {
				status_text = prefix + std::to_string(warnings) + (warnings == 1 ? " warning" : " warnings");
				status_color = gse::vec4f{ 0.71f, 0.57f, 0.11f, 1.f };
			}
			else {
				status_text = prefix + "no issues";
				status_color = gse::vec4f{ 0.48f, 0.65f, 0.29f, 1.f };
			}
		}

		if (!status_text.empty()) {
			ctx.queue_text({
				.font = ctx.fonts.text,
				.text = status_text,
				.position = { text_left, status_rect.center().y() + ctx.fonts.text->vertical_center_offset(font_sz) },
				.scale = font_sz,
				.color = status_color,
				.clip_rect = status_rect,
			});
		}
	}

	if (text_rect.width() <= 0.f || text_rect.height() <= 0.f) {
		return;
	}

	syntax_producer::poll(doc.syntax);

	if (doc.highlightable && doc.highlight_dirty && !doc.syntax.pending && gse::system_clock::now<gse::time>() - doc.last_edit > gse::milliseconds(120)) {
		syntax_producer::rebuild(doc.syntax, doc.buffer);
		doc.highlight_dirty = false;
	}

	struct diag_region {
		std::uint32_t line = 0;
		std::uint32_t byte_start = 0;
		std::uint32_t byte_end = 0;
		gse::vec4f color;
		std::string_view message;
		severity level = severity::error;
		const diagnostic* diag = nullptr;
	};

	std::vector<gse::gui::text_underline> underlines;
	std::vector<gse::gui::text_fade> fades;
	std::vector<diag_region> diag_regions;
	if ((!doc.diagnostics.empty() || !doc.lint.empty()) && !doc.analysis_failed) {
		const std::size_t line_count = doc.buffer.line_count();
		auto display_to_byte = [](const std::string_view row, const std::uint32_t display_col) -> std::uint32_t {
			constexpr std::size_t gcc_tab_width = 8;
			std::size_t display = 0;
			for (std::size_t i = 0; i < row.size(); ++i) {
				if (display >= display_col) {
					return static_cast<std::uint32_t>(i);
				}
				display += row[i] == '\t' ? gcc_tab_width - display % gcc_tab_width : 1;
			}
			return static_cast<std::uint32_t>(row.size());
		};
		underlines.reserve(doc.diagnostics.size() + doc.lint.size());
		diag_regions.reserve(doc.diagnostics.size() + doc.lint.size());
		const std::array<std::span<const diagnostic>, 2> diag_sources{ doc.diagnostics, doc.lint };
		for (const std::span<const diagnostic> diag_group : diag_sources)
		for (const auto& diagnostic : diag_group) {
			if (!diagnostic.file.empty() && diagnostic.file != doc.path) {
				continue;
			}
			if (diagnostic.line >= line_count) {
				continue;
			}
			const gse::vec4f color = diagnostic.level == severity::warning
				? gse::vec4f{ 0.71f, 0.57f, 0.11f, 1.f }
				: (diagnostic.level == severity::hint ? gse::vec4f{ 0.45f, 0.51f, 0.58f, 0.9f } : diagnostic.level == severity::note ? gse::vec4f{ 0.43f, 0.50f, 0.64f, 1.f } : gse::vec4f{ 0.855f, 0.451f, 0.424f, 1.f });
			const bool faded = diagnostic.source == diagnostic_source::lint && diagnostic.level == severity::hint;
			const std::uint32_t last_line = std::min<std::uint32_t>(std::max(diagnostic.end_line, diagnostic.line), static_cast<std::uint32_t>(line_count) - 1);
			for (std::uint32_t seg_line = diagnostic.line; seg_line <= last_line; ++seg_line) {
				const std::string_view row = doc.buffer.line(seg_line);
				const std::uint32_t byte_start = seg_line == diagnostic.line ? display_to_byte(row, diagnostic.start_col) : 0;
				std::uint32_t byte_end = seg_line == last_line ? display_to_byte(row, diagnostic.end_col) : static_cast<std::uint32_t>(row.size());
				if (seg_line == diagnostic.line && seg_line == last_line) {
					byte_end = std::max(byte_start + 1, byte_end);
				}
				if (byte_end <= byte_start) {
					continue;
				}
				if (faded) {
					fades.push_back({ .line = seg_line, .start_col = byte_start, .end_col = byte_end, .alpha = 0.45f });
				}
				else {
					underlines.push_back({ .line = seg_line, .start_col = byte_start, .end_col = byte_end, .color = color });
				}
				diag_regions.push_back({ .line = seg_line, .byte_start = byte_start, .byte_end = byte_end, .color = color, .message = diagnostic.message, .level = diagnostic.level, .diag = &diagnostic });
			}
		}
	}

	if (doc.pending_center_line) {
		const float line_h = ctx.fonts.code->line_height(font_sz) * 1.25f;
		const float view_h = std::max(0.f, text_rect.height() - pad * 2.f);
		const float content_h = static_cast<float>(doc.buffer.line_count()) * line_h;
		const float caret_y = static_cast<float>(*doc.pending_center_line) * line_h;
		const float centered = caret_y - view_h * 0.5f + line_h * 0.5f;
		const float target = std::clamp(centered, 0.f, std::max(0.f, content_h - view_h));
		doc.view.scroll.y.offset = target;
		doc.view.scroll.y.target = target;
		doc.pending_center_line.reset();
	}

	const std::size_t display_tab_width = static_cast<std::size_t>(std::max(1, config.indent_width));
	const bool goto_ctrl = ctx.input.key_held(gse::key::left_control) || ctx.input.key_held(gse::key::right_control);
	std::optional<search::location> link_target;
	if (goto_ctrl && index && text_rect.contains(mouse) && ctx.input_available() && !doc.buffer.lines.empty()) {
		const gse::gui::buffer_position hover = code_position_at(ctx, doc.buffer, doc.view, text_rect, config.show_line_numbers, display_tab_width, mouse);
		const std::string_view row = doc.buffer.line(hover.line);
		if (const std::optional<module_link> mod = module_name_at(row, hover.column)) {
			link_target = index->module_definition(mod->name, doc.path);
			if (link_target) {
				underlines.push_back({ .line = hover.line, .start_col = mod->start_col, .end_col = mod->end_col, .color = ctx.style.color_text_secondary });
			}
		}
		else {
			std::size_t a = std::min<std::size_t>(hover.column, row.size());
			std::size_t b = a;
			while (a > 0 && (row[a - 1] == '_' || std::isalnum(static_cast<unsigned char>(row[a - 1])))) {
				--a;
			}
			while (b < row.size() && (row[b] == '_' || std::isalnum(static_cast<unsigned char>(row[b])))) {
				++b;
			}
			const bool member_access = a > 0 && (row[a - 1] == '.' || (a >= 2 && row[a - 1] == '>' && row[a - 2] == '-'));
			if (b > a && !std::isdigit(static_cast<unsigned char>(row[a]))) {
				link_target = index->definition_at(doc.path, hover.line, hover.column);
				if (!link_target && !member_access) {
					std::string qualifier;
					std::size_t qp = a;
					while (qp >= 2 && row[qp - 1] == ':' && row[qp - 2] == ':') {
						std::size_t qs = qp - 2;
						while (qs > 0 && (row[qs - 1] == '_' || std::isalnum(static_cast<unsigned char>(row[qs - 1])))) {
							--qs;
						}
						if (qs == qp - 2) {
							break;
						}
						qualifier.insert(0, std::string(row.substr(qs, qp - qs)));
						qp = qs;
					}
					link_target = index->symbol_definition(row.substr(a, b - a), qualifier, doc.path);
				}
				if (link_target) {
					underlines.push_back({ .line = hover.line, .start_col = static_cast<std::uint32_t>(a), .end_col = static_cast<std::uint32_t>(b), .color = ctx.style.color_text_secondary });
				}
			}
		}
	}
	static std::optional<quickfix_popup> qf;
	if (link_target.has_value()) {
		channels.push<gse::set_cursor_shape_request>({ .shape = gse::cursor_shape::hand });
	}
	const bool goto_click = link_target.has_value() && ctx.mouse_pressed_for(text_rect);

	if (!ws.cppref.loaded) {
		ws.cppref.load(config::cppref_index);
	}
	std::optional<std::size_t> hovered_diag;
	if (!goto_ctrl && !diag_regions.empty() && text_rect.contains(mouse) && ctx.input_available() && !doc.buffer.lines.empty()) {
		const float diag_line_h = ctx.fonts.code->line_height(font_sz) * 1.25f;
		const float content_top = text_rect.top() - pad + doc.view.scroll.y.offset;
		const float row_rel = (content_top - mouse.y()) / diag_line_h;
		if (row_rel >= 0.f && row_rel < static_cast<float>(doc.buffer.line_count())) {
			const gse::gui::buffer_position dp = code_position_at(ctx, doc.buffer, doc.view, text_rect, config.show_line_numbers, display_tab_width, mouse);
			for (std::size_t di = 0; di < diag_regions.size(); ++di) {
				const diag_region& dr = diag_regions[di];
				if (dr.line == dp.line && dp.column >= dr.byte_start && dp.column < dr.byte_end) {
					hovered_diag = di;
					break;
				}
			}
		}
	}
	int deepest_alive = -1;
	for (std::size_t i = 0; i < ws.hover_stack.size(); ++i) {
		if (ws.hover_stack[i].has_card && hover_kept_alive(ws.hover_stack[i], mouse)) {
			deepest_alive = static_cast<int>(i);
		}
	}
	const bool panel_alive = !hovered_diag && deepest_alive >= 0;
	if (!goto_ctrl && !hovered_diag && !panel_alive && text_rect.contains(mouse) && ctx.input_available() && !doc.buffer.lines.empty()) {
		const gse::gui::buffer_position hp = code_position_at(ctx, doc.buffer, doc.view, text_rect, config.show_line_numbers, display_tab_width, mouse);
		const std::string_view row = doc.buffer.line(hp.line);
		std::size_t a = std::min<std::size_t>(hp.column, row.size());
		std::size_t b = a;
		while (a > 0 && (row[a - 1] == '_' || std::isalnum(static_cast<unsigned char>(row[a - 1])))) {
			--a;
		}
		while (b < row.size() && (row[b] == '_' || std::isalnum(static_cast<unsigned char>(row[b])))) {
			++b;
		}
		const bool member_access = a > 0 && (row[a - 1] == '.' || (a >= 2 && row[a - 1] == '>' && row[a - 2] == '-'));
		if (b > a && !std::isdigit(static_cast<unsigned char>(row[a]))) {
			const std::string_view ident = row.substr(a, b - a);
			if (ws.hover_stack.empty()) {
				ws.hover_stack.emplace_back();
			}
			else if (ws.hover_stack.size() > 1) {
				ws.hover_stack.resize(1);
			}
			hover_state& hv = ws.hover_stack.front();
			if (hv.line != hp.line || hv.column != static_cast<std::uint32_t>(a) || std::string_view(hv.ident) != ident) {
				hv = {};
				hv.line = hp.line;
				hv.column = static_cast<std::uint32_t>(a);
				hv.ident = std::string(ident);
				hv.since = gse::system_clock::now<gse::time>();
			}
			else if (!hv.resolved && gse::system_clock::now<gse::time>() - hv.since > gse::milliseconds(350)) {
				hv.resolved = true;
				std::string qualified;
				std::string sym_kind;
				std::optional<search::location> def;
				if (index) {
					if (const std::optional<search::hover_hit> hit = index->symbol_at(doc.path, hp.line, hp.column)) {
						const std::string_view q = hit->qualified;
						const std::size_t qsep = q.rfind("::");
						const std::string_view last = qsep == std::string_view::npos ? q : q.substr(qsep + 2);
						if (last == ident) {
							qualified = hit->qualified;
							sym_kind = hit->kind;
							if (!hit->def.path.empty()) {
								def = hit->def;
							}
						}
					}
				}
				resolve_hover_card(hv, ident, row, a, member_access, index, ws.cppref, std::move(qualified), std::move(sym_kind), def);
				if (hv.kind.empty() && doc.syntax.semantic) {
					const syntax_producer::semantic_data& sem = *doc.syntax.semantic;
					const std::uint64_t key = (static_cast<std::uint64_t>(hp.line) << 32) | static_cast<std::uint32_t>(a);
					if (const auto it = sem.at.find(key); it != sem.at.end()) {
						hv.kind = std::format("{}", it->second);
					}
				}
				hv.kind_color = ctx.style.color_text_secondary;
				for (const gse::gui::text_span& sp : doc.syntax.spans) {
					if (sp.line == hp.line && static_cast<std::uint32_t>(a) >= sp.start_col && static_cast<std::uint32_t>(a) < sp.end_col) {
						hv.kind_color = sp.color;
						break;
					}
				}
				hv.anchor = mouse;
			}
		}
		else {
			ws.hover_stack.clear();
		}
	}
	else if (!panel_alive) {
		ws.hover_stack.clear();
	}
	if (!goto_ctrl && !hovered_diag && deepest_alive >= 0 && ctx.input_available()) {
		const std::size_t k = static_cast<std::size_t>(deepest_alive);
		if (ws.hover_stack.size() > k + 2) {
			ws.hover_stack.resize(k + 2);
		}
		if (ws.hover_stack.size() == k + 2 && ws.hover_stack[k + 1].has_card && ws.hover_stack[k + 1].panel.width() > 0.f) {
			ws.hover_stack.resize(k + 1);
		}
		if (const std::optional<panel_code_hit> hit = hover_panel_code_hit(ctx, ws.hover_stack[k], mouse)) {
			const std::string ident(hit->ident);
			const std::string row_text(hit->row);
			if (ws.hover_stack.size() == k + 1) {
				ws.hover_stack.emplace_back();
			}
			hover_state& child = ws.hover_stack[k + 1];
			if (child.line != hit->line || child.column != hit->start_col || child.ident != ident) {
				child = {};
				child.line = hit->line;
				child.column = hit->start_col;
				child.ident = ident;
				child.since = gse::system_clock::now<gse::time>();
			}
			else if (!child.resolved && gse::system_clock::now<gse::time>() - child.since > gse::milliseconds(350)) {
				child.resolved = true;
				resolve_hover_card(child, ident, row_text, hit->start_col, hit->member_access, index, ws.cppref, {}, {}, std::nullopt);
				child.kind_color = ctx.style.color_text_secondary;
				for (const gse::gui::text_span& sp : ws.hover_stack[k].code_spans) {
					if (sp.line == hit->line && hit->start_col >= sp.start_col && hit->start_col < sp.end_col) {
						child.kind_color = sp.color;
						break;
					}
				}
				child.anchor = mouse;
			}
		}
		else if (ws.hover_stack.size() == k + 2 && !ws.hover_stack[k + 1].has_card) {
			ws.hover_stack.resize(k + 1);
		}
	}
	if (hovered_diag) {
		ws.hover_stack.clear();
		const diag_region& dr = diag_regions[*hovered_diag];
		if (dr.diag && dr.diag->fix.has_value()) {
			if (!qf || qf->document_id != ws.active_document_id || qf->line != dr.diag->line || qf->start_col != dr.diag->start_col) {
				qf = quickfix_popup{ .document_id = ws.active_document_id, .line = dr.diag->line, .start_col = dr.diag->start_col, .anchor = mouse };
			}
		}
		else {
			qf.reset();
			const std::string_view diag_label = gse::enum_to_string(dr.level);
			draw_diagnostic_tooltip(ctx, text_rect, diag_label, dr.message, dr.color, mouse);
		}
	}
	else if (!(qf && qf->document_id == ws.active_document_id && qf->panel.width() > 0.f && qf->panel.contains(mouse))) {
		qf.reset();
	}

	if (qf && qf->document_id == ws.active_document_id) {
		ws.hover_stack.clear();
		const diagnostic* found = nullptr;
		for (const diagnostic& d : doc.lint) {
			if (d.fix && d.line == qf->line && d.start_col == qf->start_col) {
				found = &d;
				break;
			}
		}
		if (found) {
			int fixall_count = 0;
			for (const diagnostic& d : doc.lint) {
				if (d.fix && d.rule == found->rule) {
					++fixall_count;
				}
			}
			const std::string_view qlabel = gse::enum_to_string(found->level);
			const quickfix_layout layout = draw_quickfix_tooltip(ctx, text_rect, qlabel, gse::vec4f{ 0.55f, 0.61f, 0.68f, 1.f }, found->message, found->fix->title, fixall_count, qf->anchor, mouse);
			qf->panel = layout.panel;
			if (ctx.input_available() && ctx.mouse_pressed_for(text_rect)) {
				if (layout.fix_row.contains(mouse)) {
					apply_quickfix(doc, found->fix->edits);
					qf.reset();
				}
				else if (layout.fixall_row.width() > 0.f && layout.fixall_row.contains(mouse)) {
					apply_quickfix(doc, fix_engine::rule_edits(doc.lint, found->rule));
					qf.reset();
				}
			}
		}
		else {
			qf.reset();
		}
	}
	else if (!hovered_diag && !ws.hover_stack.empty()) {
		for (std::size_t i = 0; i < ws.hover_stack.size(); ++i) {
			hover_state& hv = ws.hover_stack[i];
			if (!hv.has_card) {
				continue;
			}
			if (draw_hover_panel(ctx, text_rect, hv, ctx.current_z_order + 1 + static_cast<std::uint32_t>(i)) && !hv.url.empty()) {
				analysis::process::open_url(hv.url.c_str());
			}
		}
	}

	const gse::id text_id = gse::gui::ids::make("##doc_text_" + std::to_string(ws.active_document_id));
	doc.view.context_menu_tag = editor_text_context_tag();
	const bool edited = gse::gui::draw::text_area_in_rect(
		ctx,
		text_id,
		doc.buffer,
		doc.view,
		doc.highlightable ? std::span<const gse::gui::text_span>(doc.syntax.spans) : std::span<const gse::gui::text_span>{},
		underlines,
		fades,
		text_rect,
		false,
		config.show_line_numbers,
		display_tab_width,
		config.indent_with_spaces,
		true,
		config.caret_blink,
		ui.hot_widget_id,
		ui.focus_widget_id
	);
	if (edited) {
		doc.dirty = true;
		doc.highlight_dirty = true;
		doc.diag_dirty = true;
		doc.last_edit = gse::system_clock::now<gse::time>();
	}

	if (goto_click && link_target) {
		workspace::jump_to(ws, link_target->path, link_target->line, link_target->column);
		return;
	}

	const format::options format_opts = {
		.indent_width = config.indent_width,
		.indent_with_spaces = config.indent_with_spaces,
	};

	const bool ctrl = ctx.input.key_held(gse::key::left_control) || ctx.input.key_held(gse::key::right_control);
	if (ui.focus_widget_id == text_id && ctrl && ctx.input.key_pressed(gse::key::s)) {
		format_and_save(ws, ws.active_document_id, format_opts, config.format_on_save, true, channels);
		doc.diag_dirty = true;
		doc.last_edit = {};
	}

	const bool shift = ctx.input.key_held(gse::key::left_shift) || ctx.input.key_held(gse::key::right_shift);
	const bool alt = ctx.input.key_held(gse::key::left_alt) || ctx.input.key_held(gse::key::right_alt);
	if (ui.focus_widget_id == text_id && shift && alt && ctx.input.key_pressed(gse::key::f) && doc.highlightable) {
		apply_format(doc, format_opts);
	}
}

export namespace gse::ide::editor_app {
	struct [[= gse::system_state<"Editor">{}]] data {
		bool initialized = false;
		bool screen_pushed = false;
		float explorer_ratio = 0.22f;
		float terminal_ratio = 0.22f;
		bool resizing_explorer = false;
		bool resizing_terminal = false;
		gse::cursor_shape frame_cursor = gse::cursor_shape::arrow;
		gse::clock save_clock;
	};

	[[= gse::system_run<>{}]]
	auto run(
		gse::context& ctx,
		data& d,
		gse::shared_view<search_system::data> search_d,
		gse::shared_view<gse::input::data> input_d,
		const gse::save::registry& save_reg
	) -> gse::async::task<>;

	[[= gse::system_shutdown{}]]
	auto shutdown(
		data& d
	) -> void;
}

export namespace gse::ide::workspace_system {
	struct [[= gse::system_state<"Workspace">{}]] data {
		workspace::data ws;
		gse::ide::graph_data graph;
		bool graph_from_game = false;
		std::uint32_t graph_game_gen = 0;
		quick_search_state search;
		git::status_snapshot git_status;
		bool initialized = false;
		gse::clock save_clock;
	};

	[[= gse::system_run<>{}]]
	auto run(
		gse::context& ctx,
		data& d,
		const scheduler& sched,
		gse::shared_view<config_system::data> config_d,
		gse::shared_view<search_system::data> search_d,
		gse::shared_view<gse::input::data> input_d,
		gse::shared_view<viewport::data> viewport_d
	) -> gse::async::task<>;

	[[= gse::system_shutdown{}]]
	auto shutdown(
		data& d
	) -> void;
}

namespace gse::ide {
	auto load_editor_layout(
		editor_app::data& d
	) -> void;

	auto save_editor_layout(
		const editor_app::data& d
	) -> void;

	auto load_workspace_layout(
		workspace::data& ws
	) -> void;

	auto save_workspace_layout(
		const workspace::data& ws
	) -> void;
}

auto gse::ide::load_editor_layout(editor_app::data& d) -> void {
	const std::vector<layout_section> sections = parse_layout_sections(layout_store::read(editor_layout_path()));
	for (const layout_section& section : sections) {
		if (section.name != "editor") {
			continue;
		}
		if (const auto it = section.values.find("explorer_ratio"); it != section.values.end()) {
			d.explorer_ratio = std::clamp(parse_layout_float(it->second, d.explorer_ratio), 0.05f, 0.95f);
		}
		if (const auto it = section.values.find("terminal_ratio"); it != section.values.end()) {
			d.terminal_ratio = std::clamp(parse_layout_float(it->second, d.terminal_ratio), 0.05f, 0.95f);
		}
		return;
	}
}

auto gse::ide::save_editor_layout(const editor_app::data& d) -> void {
	std::string out;
	out.append("[editor]\n");
	out.append(std::format("explorer_ratio = {}\n", d.explorer_ratio));
	out.append(std::format("terminal_ratio = {}\n", d.terminal_ratio));
	replace_layout_sections(editor_layout_owner(), out);
}

auto gse::ide::load_workspace_layout(workspace::data& ws) -> void {
	const std::vector<layout_section> sections = parse_layout_sections(layout_store::read(editor_layout_path()));
	std::string active;
	std::filesystem::path active_path;
	std::unordered_map<std::string, std::uint32_t> id_by_path;

	for (const layout_section& section : sections) {
		if (section.name != "workspace") {
			continue;
		}
		if (const auto it = section.values.find("active"); it != section.values.end()) {
			active = it->second;
		}
		if (const auto it = section.values.find("active_path"); it != section.values.end()) {
			active_path = it->second;
		}
		break;
	}

	for (const layout_section& section : sections) {
		if (!section.name.starts_with("document ")) {
			continue;
		}

		const auto path_it = section.values.find("path");
		if (path_it == section.values.end() || path_it->second.empty()) {
			continue;
		}

		const std::filesystem::path path = path_it->second;
		std::error_code exists_ec;
		if (!std::filesystem::exists(path, exists_ec)) {
			continue;
		}

		const std::uint32_t id = workspace::open_file(ws, path);
		if (std::ranges::find(ws.tab_order, id) == ws.tab_order.end()) {
			ws.tab_order.push_back(id);
		}

		document& doc = ws.documents.at(id);
		const std::uint32_t line = section.values.contains("line") ? parse_layout_uint(section.values.at("line"), 0) : 0;
		const std::uint32_t column = section.values.contains("column") ? parse_layout_uint(section.values.at("column"), 0) : 0;
		doc.view.caret = doc.buffer.clamp({ .line = line, .column = column });
		doc.view.anchor = doc.view.caret;

		if (const auto it = section.values.find("scroll_x"); it != section.values.end()) {
			doc.view.scroll.x.offset = std::max(0.f, parse_layout_float(it->second, 0.f));
			doc.view.scroll.x.target = doc.view.scroll.x.offset;
		}
		if (const auto it = section.values.find("scroll_y"); it != section.values.end()) {
			doc.view.scroll.y.offset = std::max(0.f, parse_layout_float(it->second, 0.f));
			doc.view.scroll.y.target = doc.view.scroll.y.offset;
		}

		id_by_path.emplace(doc.path.generic_native_encoded_string(), id);
	}

	if (active == "game") {
		ws.active_document_id = viewport_tab_id;
	}
	else if (!active_path.empty()) {
		std::error_code ec;
		const std::filesystem::path canonical = std::filesystem::weakly_canonical(active_path, ec);
		const std::string key = (ec ? active_path : canonical).generic_native_encoded_string();
		if (const auto it = id_by_path.find(key); it != id_by_path.end()) {
			ws.active_document_id = it->second;
		}
	}
	else if (!ws.tab_order.empty()) {
		ws.active_document_id = ws.tab_order.front();
	}

	ws.tab_strip.dragging = 0;
}

auto gse::ide::save_workspace_layout(const workspace::data& ws) -> void {
	std::vector<std::uint32_t> order = ws.tab_order;
	std::erase_if(order, [&](const std::uint32_t id) {
		const auto it = ws.documents.find(id);
		return it == ws.documents.end() || it->second.path.empty();
	});

	std::vector<std::uint32_t> added;
	for (const auto& [id, doc] : ws.documents) {
		if (!doc.path.empty() && std::ranges::find(order, id) == order.end()) {
			added.push_back(id);
		}
	}
	std::ranges::sort(added);
	order.insert(order.end(), added.begin(), added.end());

	std::string out;
	out.append("[workspace]\n");
	if (ws.active_document_id == viewport_tab_id) {
		out.append("active = game\n");
		out.append("active_path = \n");
	}
	else if (const auto it = ws.documents.find(ws.active_document_id); it != ws.documents.end() && !it->second.path.empty()) {
		out.append("active = document\n");
		out.append(std::format("active_path = {}\n", it->second.path.generic_native_encoded_string()));
	}
	else {
		out.append("active = none\n");
		out.append("active_path = \n");
	}

	std::size_t index = 0;
	for (const std::uint32_t id : order) {
		const document& doc = ws.documents.at(id);
		const gse::gui::buffer_position caret = doc.buffer.clamp(doc.view.caret);
		out.push_back('\n');
		out.append(std::format("[document {}]\n", index));
		out.append(std::format("path = {}\n", doc.path.generic_native_encoded_string()));
		out.append(std::format("line = {}\n", caret.line));
		out.append(std::format("column = {}\n", caret.column));
		out.append(std::format("scroll_x = {}\n", std::max(0.f, doc.view.scroll.x.offset)));
		out.append(std::format("scroll_y = {}\n", std::max(0.f, doc.view.scroll.y.offset)));
		++index;
	}

	replace_layout_sections(workspace_layout_owner(), out);
}

auto gse::ide::editor_app::run(
	gse::context& ctx,
	data& d,
	const gse::shared_view<search_system::data> search_d,
	const gse::shared_view<gse::input::data> input_d,
	const gse::save::registry& save_reg
) -> gse::async::task<> {
	if (!d.initialized) {
		build_runner::cleanup_backup();
		load_editor_layout(d);
		d.save_clock.reset();
		d.initialized = true;
	}

	if (!d.screen_pushed && search_d.index) {
		ctx.channels.push<gse::gui::push_screen_request>({
			.factory = [channels = ctx.channels, index = search_d.index] {
				return std::make_unique<editor_screen>(channels, index);
			},
		});
		d.screen_pushed = true;
	}
	for ([[maybe_unused]] const auto& req : ctx.read_channel<toggle_settings_request>()) {
		ctx.channels.push<gse::gui::push_screen_request>({
			.factory = [save = &save_reg, channels = ctx.channels] {
				return std::make_unique<gse::gui::settings_screen>(*save, channels, gse::gui::settings_screen_config{ .opaque = true });
			},
		});
	}

	const gse::input::state& input = gse::input::current_state(input_d);
	const gse::vec2f mouse = input.mouse_position();
	const bool pressed = input.mouse_button_pressed(gse::mouse_button::button_1);
	const bool held = input.mouse_button_held(gse::mouse_button::button_1);

	ctx.channels.push<gse::settings::annotated_change_request<gse::gui::data>>({
		.apply = [&d, mouse, pressed, held](gse::gui::data& s) {
			gse::gui::menu* explorer = editor_menu(s, explorer_panel_name);
			gse::gui::menu* code = editor_menu(s, code_panel_name);
			gse::gui::menu* term = editor_menu(s, terminal::panel_name);
			if (!explorer || !code || !term || explorer == code || explorer == term || code == term) {
				return;
			}

			const gse::vec2f vp = s.previous_viewport_size;
			if (vp.x() <= 0.f || vp.y() <= 0.f) {
				return;
			}

			const gse::gui::style sty = gse::gui::apply_scale(s, gse::gui::style::from_theme(s.current_theme), vp.y());
			const float inset = s.reserve_top_bar ? sty.title_bar_height : 0.f;
			const float top = vp.y() - inset;
			if (top <= 0.f) {
				return;
			}

			const float min_explorer_width = 180.f * sty.scale_factor;
			const float min_code_width = 320.f * sty.scale_factor;
			const float min_terminal_height = 120.f * sty.scale_factor;
			const float min_main_height = 180.f * sty.scale_factor;
			if (vp.x() < min_explorer_width + min_code_width || top < min_main_height + min_terminal_height) {
				return;
			}
			const float hit_width = std::max(6.f * sty.scale_factor, sty.resize_border_thickness);
			const bool blocked = s.menu_stack.captures_input() || s.context_menu.open;

			const float divider_thickness = hit_width * 2.f;
			const gse::rectf frame = gse::rectf::from_position_size({ 0.f, top }, { vp.x(), top });

			const gse::gui::layout::split_params vertical_split{
				.container = frame,
				.axis = gse::gui::layout::split_axis::rows,
				.ratio = 1.f - d.terminal_ratio,
				.min_first = min_main_height,
				.min_second = min_terminal_height,
				.divider_thickness = divider_thickness,
			};
			const gse::rectf main_area = gse::gui::layout::resolve_split(vertical_split).first;

			const gse::gui::layout::split_params horizontal_split{
				.container = main_area,
				.axis = gse::gui::layout::split_axis::columns,
				.ratio = d.explorer_ratio,
				.min_first = min_explorer_width,
				.min_second = min_code_width,
				.divider_thickness = divider_thickness,
			};

			const gse::gui::layout::split_result columns = gse::gui::layout::update_split(
				horizontal_split,
				{ .mouse = mouse, .pressed = pressed, .held = held, .blocked = blocked },
				d.resizing_explorer
			);
			d.explorer_ratio = columns.ratio;

			const gse::gui::layout::split_result rows = gse::gui::layout::update_split(
				vertical_split,
				{ .mouse = mouse, .pressed = pressed, .held = held, .blocked = blocked || d.resizing_explorer },
				d.resizing_terminal
			);
			d.terminal_ratio = 1.f - rows.ratio;

			const gse::gui::layout::split_result final_rows = gse::gui::layout::resolve_split({
				.container = frame,
				.axis = gse::gui::layout::split_axis::rows,
				.ratio = 1.f - d.terminal_ratio,
				.min_first = min_main_height,
				.min_second = min_terminal_height,
				.divider_thickness = divider_thickness,
			});
			const gse::gui::layout::split_result final_columns = gse::gui::layout::resolve_split({
				.container = final_rows.first,
				.axis = gse::gui::layout::split_axis::columns,
				.ratio = d.explorer_ratio,
				.min_first = min_explorer_width,
				.min_second = min_code_width,
				.divider_thickness = divider_thickness,
			});
			d.terminal_ratio = 1.f - final_rows.ratio;
			d.explorer_ratio = final_columns.ratio;

			gse::cursor_shape want_cursor = gse::cursor_shape::arrow;
			if (!blocked) {
				if (final_columns.divider.contains(mouse) || d.resizing_explorer) {
					want_cursor = gse::cursor_shape::resize_ew;
				}
				else if (final_rows.divider.contains(mouse) || d.resizing_terminal) {
					want_cursor = gse::cursor_shape::resize_ns;
				}
			}
			d.frame_cursor = want_cursor;

			explorer->rect = final_columns.first;
			explorer->swap_parent(gse::id());
			explorer->docked_to = gse::gui::dock::location::none;
			explorer->fixed = true;
			explorer->bare = true;

			code->rect = final_columns.second;
			code->swap_parent(gse::id());
			code->docked_to = gse::gui::dock::location::none;
			code->fixed = true;
			code->bare = true;

			term->rect = final_rows.second;
			term->swap_parent(gse::id());
			term->docked_to = gse::gui::dock::location::none;
			term->fixed = true;
			term->bare = true;

			gse::gui::clear_menu_interaction(s);
		},
	});

	if (d.frame_cursor != gse::cursor_shape::arrow) {
		ctx.channels.push<gse::set_cursor_shape_request>({ .shape = d.frame_cursor });
	}

	if (d.save_clock.elapsed() > editor_layout_save_interval) {
		save_editor_layout(d);
		d.save_clock.reset();
	}

	return {};
}

auto gse::ide::workspace_system::run(
	gse::context& ctx,
	data& d,
	const gse::scheduler& sched,
	const gse::shared_view<config_system::data> config_d,
	const gse::shared_view<search_system::data> search_d,
	const gse::shared_view<gse::input::data> input_d,
	const gse::shared_view<viewport::data> viewport_d
) -> gse::async::task<> {
	if (!d.initialized) {
		d.ws.root = gse::config::root_dir;
		d.ws.fs_root.path = d.ws.root;
		d.ws.fs_root.name = d.ws.root.filename().display_string();
		d.ws.fs_root.is_dir = true;
		load_workspace_layout(d.ws);
		d.save_clock.reset();
		d.initialized = true;
	}

	const std::filesystem::path game_graph_file = gse::ide::build_runner::game_graph_path();
	const std::uint32_t game_gen = gse::ide::build_runner::game_build_generation();
	if (!game_graph_file.empty() && d.graph_game_gen != game_gen) {
		gse::ide::graph_data loaded = gse::ide::build_graph_from_file(game_graph_file);
		if (loaded.built) {
			d.graph = std::move(loaded);
			d.graph_from_game = true;
			d.graph_game_gen = game_gen;
		}
	}
	if (!d.graph.built && sched.all_settled()) {
		d.graph = gse::ide::build_graph(sched);
	}

	workspace::data* ws = &d.ws;
	const auto config = config_d;
	const gse::input::state& input = gse::input::current_state(input_d);
	for (const git::status_updated& update : ctx.read_channel<git::status_updated>()) {
		d.git_status = update.status;
	}
	update_diagnostics(ctx, d.ws, config_d);
	d.ws.watcher.poll();
	if (input.mouse_button_pressed(gse::mouse_button::button_4)) {
		workspace::go_back(d.ws);
	}
	if (input.mouse_button_pressed(gse::mouse_button::button_5)) {
		workspace::go_forward(d.ws);
	}

	ctx.channels.push<gse::gui::menu_content>({
		.menu = std::string(explorer_panel_name),
		.layer = gse::render_layer::content,
		.build = [ws, search = &d.search, index = search_d.index, channels = ctx.channels, git_status = d.git_status](gse::gui::builder& b) {
			draw_explorer_panel(b, *ws, *search, index, channels, git_status.get());
		},
	});

	const gse::gpu::bindless_slot viewport_slot = viewport_d.ready ? viewport_d.display_slot : gse::gpu::bindless_slot{};
	const bool game_running = viewport_d.imported_ready;

	ctx.channels.push<gse::gui::menu_content>({
		.menu = std::string(code_panel_name),
		.layer = gse::render_layer::content,
		.build = [ws, graph = &d.graph, index = search_d.index, config, channels = ctx.channels, viewport_slot, game_running](gse::gui::builder& b) {
			draw_code_panel(b, *ws, *graph, index, config, channels, viewport_slot, game_running);
		},
	});

	for (const auto& req : ctx.read_channel<jump_to_request>()) {
		workspace::jump_to(d.ws, req.path, req.line, req.column);
	}

	for (const auto& res : ctx.read_channel<gse::gui::context_menu_result>()) {
		if (res.tag == explorer_context_tag()) {
			const fs_node* node = workspace::find_node(d.ws.fs_root, res.target);
			const auto& table = explorer_actions();
			if (node && res.action_id < table.size() && table[res.action_id].run) {
				table[res.action_id].run(d.ws, *node);
			}
		}
		else if (res.tag == tab_context_tag()) {
			const auto& table = tab_actions();
			if (res.action_id < table.size() && table[res.action_id].run) {
				table[res.action_id].run(d.ws, static_cast<std::uint32_t>(res.target));
			}
		}
		else if (res.tag == editor_text_context_tag()) {
			if (const auto doc = d.ws.documents.find(d.ws.active_document_id); doc != d.ws.documents.end()) {
				doc->second.view.pending_action = static_cast<gse::gui::text_edit_action>(res.action_id);
			}
		}
	}

	if (d.save_clock.elapsed() > editor_layout_save_interval) {
		save_workspace_layout(d.ws);
		d.save_clock.reset();
	}

	return {};
}

auto gse::ide::editor_app::shutdown(data& d) -> void {
	save_editor_layout(d);
}

auto gse::ide::workspace_system::shutdown(data& d) -> void {
	save_workspace_layout(d.ws);
}

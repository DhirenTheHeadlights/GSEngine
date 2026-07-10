export module gse.ide.app:editor_app;

import std;
import gse;
import gse.gpu;

import gse.ide.workspace;
import gse.ide.highlight;
import gse.ide.terminal;
import gse.ide.build;
import gse.ide.analysis;
import gse.ide.config;
import gse.ide.search;
import gse.ide.docs;
import gse.ide.viewport;

import :search_screen;

namespace gse::ide {
	constexpr std::string_view explorer_panel_name = "Explorer";
	constexpr std::string_view code_panel_name = "Code";
	constexpr std::uint32_t viewport_tab_id = std::numeric_limits<std::uint32_t>::max();
	constexpr gse::time editor_layout_save_interval = gse::seconds(30.f);

	auto rebuild_glyph() -> std::span<const gse::gui::symbol::stroke>;

	struct toggle_settings_request {};

	using ui_rect = gse::rect_t<gse::vec2f>;

	struct layout_section {
		std::string name;
		std::map<std::string, std::string> values;
	};

	using layout_section_filter = bool (*)(std::string_view);

	auto editor_layout_path() -> std::filesystem::path;

	auto trim_layout_value(
		std::string_view value
	) -> std::string_view;

	auto layout_section_name(
		std::string_view line
	) -> std::string_view;

	auto read_layout_file(
		const std::filesystem::path& path
	) -> std::string;

	auto write_layout_file(
		const std::filesystem::path& path,
		std::string_view text
	) -> void;

	auto parse_layout_sections(
		std::string_view text
	) -> std::vector<layout_section>;

	auto remove_layout_sections(
		std::string_view text,
		layout_section_filter remove
	) -> std::string;

	auto replace_layout_sections(
		layout_section_filter remove,
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

	auto editor_layout_section(
		std::string_view line
	) -> bool;

	auto workspace_layout_section(
		std::string_view line
	) -> bool;

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
			const gse::ide::ui_rect& rect,
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
		const gse::ide::ui_rect& search_rect,
		std::string_view id_key
	) -> void;

	auto draw_explorer_panel(
		gse::gui::builder& ui,
		workspace::data& ws,
		quick_search_state& search,
		const search::index_state* index,
		gse::channel_writer channels
	) -> void;

	auto draw_code_panel(
		gse::gui::builder& ui,
		workspace::data& ws,
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
		const gse::ide::ui_rect& rect,
		bool show_line_numbers,
		std::size_t display_tab_width,
		gse::vec2f mouse
	) -> gse::gui::buffer_position;

	auto draw_spinner(
		const gse::gui::draw_context& ctx,
		const ui_rect& rect,
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
		const ui_rect& text_rect,
		hover_state& h
	) -> bool;

	auto draw_diagnostic_tooltip(
		const gse::gui::draw_context& ctx,
		const ui_rect& text_rect,
		std::string_view label,
		std::string_view message,
		gse::vec4f label_color,
		gse::vec2f anchor
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
		const ui_rect& clip
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

auto gse::ide::editor_screen::chrome_button(gse::gui::builder& ui, const gse::ide::ui_rect& rect, const std::string& key, const std::span<const gse::gui::symbol::stroke> glyph, const gse::vec4f hover_color) -> bool {
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

auto gse::ide::read_layout_file(const std::filesystem::path& path) -> std::string {
	std::ifstream file(path);
	if (!file) {
		return {};
	}

	std::ostringstream oss;
	oss << file.rdbuf();
	return oss.str();
}

auto gse::ide::write_layout_file(const std::filesystem::path& path, const std::string_view text) -> void {
	if (const auto parent = path.parent_path(); !parent.empty() && !std::filesystem::exists(parent)) {
		std::filesystem::create_directories(parent);
	}

	std::ofstream file(path);
	file << text;
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

auto gse::ide::remove_layout_sections(const std::string_view text, const layout_section_filter remove) -> std::string {
	std::string out;
	std::string section;
	bool keep_section = true;

	auto flush = [&] {
		if (keep_section) {
			out.append(section);
		}
		section.clear();
	};

	std::size_t pos = 0;
	while (pos < text.size()) {
		const std::size_t line_end = text.find('\n', pos);
		const std::size_t next = line_end == std::string_view::npos ? text.size() : line_end + 1;
		const std::string_view line(text.data() + pos, next - pos);
		const std::string_view name = layout_section_name(line);

		if (!name.empty()) {
			flush();
			keep_section = !remove(line);
		}

		section.append(line);
		pos = next;
	}

	flush();
	return out;
}

auto gse::ide::replace_layout_sections(const layout_section_filter remove, const std::string_view block) -> void {
	const std::filesystem::path path = editor_layout_path();
	std::string content = remove_layout_sections(read_layout_file(path), remove);

	if (!block.empty()) {
		if (!content.empty() && content.back() != '\n') {
			content.push_back('\n');
		}
		if (!content.empty()) {
			content.push_back('\n');
		}
		content.append(block);
		if (!content.empty() && content.back() != '\n') {
			content.push_back('\n');
		}
	}

	write_layout_file(path, content);
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

auto gse::ide::editor_layout_section(const std::string_view line) -> bool {
	return layout_section_name(line) == "editor";
}

auto gse::ide::workspace_layout_section(const std::string_view line) -> bool {
	const std::string_view name = layout_section_name(line);
	return name == "workspace" || name.starts_with("document ");
}

auto gse::ide::editor_screen::build(gse::gui::builder& ui, gse::gui::nav& n) -> void {
	const auto& ctx = ui.ctx;
	if (!ctx.current_menu) {
		return;
	}

	if ((ctx.input.key_held(gse::key::left_control) || ctx.input.key_held(gse::key::right_control)) && ctx.input.key_pressed(gse::key::f)) {
		n.push<search_screen>(m_channels, m_index);
	}

	const gse::ide::ui_rect screen_rect = ctx.current_menu->rect;
	const float bar_height = ctx.style.title_bar_height;

	const gse::ide::ui_rect bar_rect = gse::ide::ui_rect::from_position_size(
		{ screen_rect.left(), screen_rect.top() },
		{ screen_rect.width(), bar_height }
	);

	ctx.queue_sprite({
		.rect = bar_rect,
		.color = ctx.style.color_input_background,
		.texture = ctx.blank_texture,
	});

	ctx.queue_text({
		.font = ctx.font,
		.text = std::string(title()),
		.position = { bar_rect.left() + ctx.style.padding, bar_rect.center().y() + ctx.font->vertical_center_offset(ctx.style.font_size) },
		.scale = ctx.style.font_size,
		.color = ctx.style.color_text,
		.clip_rect = bar_rect,
	});

	if (m_index) {
		if (const std::uint64_t loc = m_index->cpp_loc.load(std::memory_order_acquire)) {
			if (!m_loc_label) {
				m_loc_label = std::format("{}k LOC", (loc + 500) / 1000);
			}
			const float title_w = ctx.font->width(title(), ctx.style.font_size);
			const float badge_pad = ctx.style.padding * 0.5f;
			const float badge_height = ctx.style.font_size + ctx.style.padding * 0.5f;
			const float badge_width = ctx.font->width(*m_loc_label, ctx.style.font_size) + badge_pad * 2.f;
			const gse::ide::ui_rect badge_rect = gse::ide::ui_rect::from_position_size(
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
				.font = ctx.font,
				.text = *m_loc_label,
				.position = { badge_rect.left() + badge_pad, badge_rect.center().y() + ctx.font->vertical_center_offset(ctx.style.font_size) },
				.scale = ctx.style.font_size,
				.color = ctx.style.color_accent,
				.clip_rect = badge_rect,
			});
		}
	}

	const float button_w = bar_height * 1.5f;
	auto button_slot = [&](const int from_right) -> gse::ide::ui_rect {
		return gse::ide::ui_rect::from_position_size(
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
			static std::uint64_t index_spin = 0;
			++index_spin;
			const float badge_h = ctx.style.font_size + ctx.style.padding * 0.5f;
			const float pad = ctx.style.padding * 0.6f;
			const float spin_w = spinning ? ctx.style.font_size : 0.f;
			const float badge_w = pad + spin_w + ctx.font->width(status, ctx.style.font_size) + pad;
			const float right_edge = bar_rect.right() - button_w * 5.f - ctx.style.padding;
			const gse::ide::ui_rect status_rect = gse::ide::ui_rect::from_position_size(
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
				draw_spinner(ctx, gse::ide::ui_rect::from_position_size({ sx, status_rect.center().y() + spin_w * 0.5f }, { spin_w, spin_w }), pill_fg, gse::radians(static_cast<float>(index_spin) * 0.16f));
				sx += spin_w;
			}
			ctx.queue_text({
				.font = ctx.font,
				.text = status,
				.position = { sx, status_rect.center().y() + ctx.font->vertical_center_offset(ctx.style.font_size) },
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

auto gse::ide::draw_search_bar(gse::gui::builder& ui, quick_search_state& state, const search::index_state* index, gse::channel_writer channels, const gse::ide::ui_rect& search_rect, const std::string_view id_key) -> void {
	const auto& ctx = ui.ctx;
	const auto& sty = ctx.style;
	const float pad = sty.padding;

	const gse::id search_id = gse::gui::ids::make(id_key);
	const bool was_focused = ui.focus_widget_id == search_id;

	ctx.queue_sprite({
		.rect = gse::ide::ui_rect::from_position_size(
			{ search_rect.left() - 1.5f, search_rect.top() + 1.5f },
			{ search_rect.width() + 3.f, search_rect.height() + 3.f }
		),
		.color = was_focused ? sty.color_accent : sty.color_accent_dim,
		.texture = ctx.blank_texture,
		.corner_radius = sty.corner_radius,
	});
	gse::gui::draw::text_input_in_rect(ctx, search_id, state.query, state.input, search_rect, ui.hot_widget_id, ui.focus_widget_id);
	const bool focused = ui.focus_widget_id == search_id;

	if (state.query.empty() && !focused) {
		ctx.queue_text({
			.font = ctx.font,
			.text = "Search...",
			.position = { search_rect.left() + pad, search_rect.center().y() + ctx.font->vertical_center_offset(sty.font_size) },
			.scale = sty.font_size,
			.color = sty.color_text_secondary,
			.clip_rect = search_rect,
		});
	}

	const gse::time now = gse::system_clock::now<gse::time>();
	if (state.query != state.last_query) {
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
	const float row_h = ctx.font->line_height(sty.font_size) + pad * 0.5f;
	const gse::ide::ui_rect list_rect = gse::ide::ui_rect::from_position_size(
		{ search_rect.left(), search_rect.bottom() - 2.f },
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
		const gse::ide::ui_rect row = gse::ide::ui_rect::from_position_size(
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
			.font = ctx.font,
			.text = r.display,
			.position = { row.left() + pad, row.center().y() + ctx.font->vertical_center_offset(sty.font_size) },
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
				.rect = ui_rect::from_position_size(
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

auto gse::ide::draw_explorer_panel(gse::gui::builder& ui, workspace::data& ws, quick_search_state& search, const search::index_state* index, gse::channel_writer channels) -> void {
	const auto& ctx = ui.ctx;
	if (ctx.clip_stack.empty()) {
		return;
	}

	const gse::ide::ui_rect body = ctx.clip_stack.back();
	const float pad = ctx.style.padding;
	const float search_height = ctx.font->line_height(ctx.style.font_size) + pad * 0.8f;
	const float accent_width = std::max(2.f, ctx.style.accent_bar_width);
	const gse::vec4f explorer_bg{ 0.045f, 0.06f, 0.095f, 0.98f };
	const gse::vec4f explorer_accent{ 0.62f, 0.34f, 1.0f, 1.0f };

	ctx.queue_sprite({
		.rect = body,
		.color = explorer_bg,
		.texture = ctx.blank_texture,
	});

	ctx.queue_sprite({
		.rect = gse::ide::ui_rect::from_position_size(
			{ body.right() - accent_width, body.top() },
			{ accent_width, body.height() }
		),
		.color = explorer_accent,
		.texture = ctx.blank_texture,
	});

	const gse::ide::ui_rect content = body.inset({ pad, pad });
	const gse::ide::ui_rect search_rect = gse::ide::ui_rect::from_position_size(
		content.top_left(),
		{ std::max(0.f, content.width() - accent_width), search_height }
	);

	draw_search_bar(ui, search, index, channels, search_rect, "##explorer_search");
	ctx.layout_cursor.x() = content.left();
	ctx.layout_cursor.y() = search_rect.bottom() - pad;

	if (!ws.fs_root.loaded) {
		workspace::load_children(ws.fs_root);
	}

	if (ws.fs_root.children.empty()) {
		ctx.queue_text({
			.font = ctx.font,
			.text = "(empty) " + ws.fs_root.path.display_string(),
			.position = { content.left(), ctx.layout_cursor.y() + ctx.font->vertical_center_offset(ctx.style.font_size) },
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

auto gse::ide::draw_spinner(const gse::gui::draw_context& ctx, const ui_rect& rect, const gse::vec4f color, const gse::angle rotation) -> void {
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

auto gse::ide::code_position_at(const gse::gui::draw_context& ctx, const gse::gui::text_buffer& buffer, const gse::gui::text_area_state& view, const gse::ide::ui_rect& rect, const bool show_line_numbers, std::size_t display_tab_width, const gse::vec2f mouse) -> gse::gui::buffer_position {
	const float scale = ctx.style.font_size;
	const float pad = ctx.style.padding;
	const float line_h = ctx.font->line_height(scale) * 1.25f;
	const std::size_t line_digits = std::max<std::size_t>(2, std::to_string(std::max<std::size_t>(1, buffer.line_count())).size());
	const float gutter_width = show_line_numbers ? ctx.font->width(std::string(line_digits, '0'), scale) + pad * 2.f : 0.f;
	const float left_inset = show_line_numbers ? gutter_width : pad;
	const float text_x = rect.left() + left_inset - view.scroll.x.offset;
	const float top_y = rect.top() - pad + view.scroll.y.offset;
	display_tab_width = std::clamp<std::size_t>(display_tab_width, 1, 16);

	const int line_count = static_cast<int>(buffer.line_count());
	const auto picked_line = static_cast<std::uint32_t>(std::clamp(static_cast<int>((top_y - mouse.y()) / line_h), 0, std::max(0, line_count - 1)));
	const std::string_view line = buffer.line(picked_line);

	std::string expanded;
	std::vector<std::size_t> col_to_expanded(line.size() + 1);
	std::size_t display_col = 0;
	for (std::size_t i = 0; i < line.size(); ++i) {
		col_to_expanded[i] = expanded.size();
		if (line[i] == '\t') {
			const std::size_t tab = display_tab_width - display_col % display_tab_width;
			expanded.append(tab, ' ');
			display_col += tab;
		}
		else {
			expanded.push_back(line[i]);
			++display_col;
		}
	}
	col_to_expanded[line.size()] = expanded.size();
	const std::vector<float> expanded_offsets = ctx.font->caret_offsets(expanded, scale);

	int picked_col = 0;
	float best_dx = std::numeric_limits<float>::max();
	for (int k = 0; k <= static_cast<int>(line.size()); ++k) {
		const float x = text_x + expanded_offsets[col_to_expanded[static_cast<std::size_t>(k)]];
		if (const float dx = std::abs(x - mouse.x()); dx < best_dx) {
			best_dx = dx;
			picked_col = k;
		}
	}
	return buffer.clamp({ .line = picked_line, .column = static_cast<std::uint32_t>(picked_col) });
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
			if (!current.empty() && ctx.font->width(candidate, scale) > max_width) {
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

auto gse::ide::draw_code_line(const gse::gui::draw_context& ctx, const std::string_view text, const std::span<const gse::gui::text_span> spans, const std::uint32_t line, const gse::vec2f origin, const float scale, const gse::vec4f fallback, const ui_rect& clip) -> void {
	const std::vector<float> offsets = ctx.font->caret_offsets(text, scale);
	const float baseline = origin.y() + ctx.font->vertical_center_offset(scale);
	const std::uint32_t len = static_cast<std::uint32_t>(text.size());
	std::uint32_t pos = 0;
	for (const gse::gui::text_span& sp : spans) {
		if (sp.line != line) {
			continue;
		}
		const std::uint32_t a = std::min<std::uint32_t>(sp.start_col, len);
		const std::uint32_t b = std::min<std::uint32_t>(sp.end_col, len);
		if (a > pos) {
			ctx.queue_text({ .font = ctx.font, .text = std::string(text.substr(pos, a - pos)), .position = { origin.x() + offsets[pos], baseline }, .scale = scale, .color = fallback, .clip_rect = clip });
		}
		if (b > a) {
			ctx.queue_text({ .font = ctx.font, .text = std::string(text.substr(a, b - a)), .position = { origin.x() + offsets[a], baseline }, .scale = scale, .color = sp.color, .clip_rect = clip });
		}
		if (b > pos) {
			pos = b;
		}
	}
	if (pos < len) {
		ctx.queue_text({ .font = ctx.font, .text = std::string(text.substr(pos)), .position = { origin.x() + offsets[pos], baseline }, .scale = scale, .color = fallback, .clip_rect = clip });
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

auto gse::ide::draw_hover_panel(const gse::gui::draw_context& ctx, const ui_rect& text_rect, hover_state& h) -> bool {
	const auto& sty = ctx.style;
	const float pad = sty.padding;
	const float fs = sty.font_size;
	const float line_h = ctx.font->line_height(fs) * 1.25f;

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
			content_w = std::max(content_w, ctx.font->width(l, fs));
		}
		for (const std::string_view l : code) {
			content_w = std::max(content_w, ctx.font->width(l, fs));
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

		const ui_rect panel = ui_rect::from_position_size({ px, top_y }, { pw, ph });
		h.panel = panel;

		const float view_h = static_cast<float>(visible) * line_h;
		const float content_h = static_cast<float>(code.size()) * line_h;
		const float max_scroll = std::max(0.f, content_h - view_h);
		const gse::vec2f wheel = ctx.scroll_delta_for(panel);
		h.scroll = std::clamp(h.scroll - wheel.y() * line_h * 2.f, 0.f, max_scroll);

		const auto scope = ctx.scoped_layer(gse::render_layer::popup);
		ctx.queue_sprite({
			.rect = ui_rect::from_position_size({ px + 4.f, top_y - 4.f }, { pw, ph }),
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

		for (std::size_t i = 0; i < header.size(); ++i) {
			const float center_y = top_y - pad - line_h * (static_cast<float>(i) + 0.5f);
			ctx.queue_text({
				.font = ctx.font,
				.text = header[i],
				.position = { px + pad, center_y + ctx.font->vertical_center_offset(fs) },
				.scale = fs,
				.color = i == 0 ? sty.color_text : h.kind_color,
				.clip_rect = panel,
			});
		}

		const float code_top = top_y - pad - header_rows * line_h;
		const ui_rect code_rect = ui_rect::from_position_size({ px, code_top }, { pw, view_h });
		for (std::size_t li = 0; li < code.size(); ++li) {
			const float center_y = code_top - line_h * (static_cast<float>(li) + 0.5f) + h.scroll;
			if (center_y > code_top + line_h || center_y < code_top - view_h - line_h) {
				continue;
			}
			draw_code_line(ctx, code[li], h.code_spans, static_cast<std::uint32_t>(li), { px + pad, center_y }, fs, sty.color_text, code_rect);
		}

		if (max_scroll > 0.f) {
			const float thumb_h = std::max(24.f, view_h * view_h / content_h);
			const float thumb_top = code_top - (h.scroll / max_scroll) * (view_h - thumb_h);
			ctx.queue_sprite({
				.rect = ui_rect::from_position_size({ px + pw - 4.f, thumb_top }, { 3.f, thumb_h }),
				.color = sty.color_border,
				.texture = ctx.blank_texture,
				.clip_rect = panel,
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
		content_w = std::max(content_w, ctx.font->width(l, fs));
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

	const ui_rect panel = ui_rect::from_position_size({ px, top_y }, { pw, ph });
	h.panel = panel;
	const gse::vec2f mouse = ctx.input.mouse_position();
	bool link_clicked = false;

	const auto scope = ctx.scoped_layer(gse::render_layer::popup);
	ctx.queue_sprite({
		.rect = ui_rect::from_position_size({ px + 4.f, top_y - 4.f }, { pw, ph }),
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
		gse::vec4f col = sty.color_text_secondary;
		if (roles[i] == 0) {
			col = sty.color_text;
		}
		else if (roles[i] == 1) {
			col = h.kind_color;
		}
		else if (roles[i] == 3) {
			const ui_rect link_rect = ui_rect::from_position_size({ px + pad, center_y + line_h * 0.5f }, { ctx.font->width(lines[i], fs), line_h });
			const bool over = link_rect.contains(mouse) && ctx.input_available();
			col = over ? sty.color_text : sty.color_accent;
			if (over && ctx.input.mouse_button_pressed(gse::mouse_button::button_1)) {
				link_clicked = true;
			}
		}
		ctx.queue_text({
			.font = ctx.font,
			.text = lines[i],
			.position = { px + pad, center_y + ctx.font->vertical_center_offset(fs) },
			.scale = fs,
			.color = col,
			.clip_rect = panel,
		});
	}
	return link_clicked;
}

auto gse::ide::draw_diagnostic_tooltip(const gse::gui::draw_context& ctx, const ui_rect& text_rect, const std::string_view label, const std::string_view message, const gse::vec4f label_color, const gse::vec2f anchor) -> void {
	const auto& sty = ctx.style;
	const float pad = sty.padding;
	const float fs = sty.font_size;
	const float line_h = ctx.font->line_height(fs) * 1.25f;
	const float max_w = 460.f;

	std::vector<std::string> lines;
	lines.emplace_back(label);
	for (std::string& wrapped : hover_wrap_lines(ctx, message, max_w, fs)) {
		lines.push_back(std::move(wrapped));
	}

	float content_w = 0.f;
	for (const std::string& l : lines) {
		content_w = std::max(content_w, ctx.font->width(l, fs));
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

	const ui_rect panel = ui_rect::from_position_size({ px, top_y }, { pw, ph });
	const auto scope = ctx.scoped_layer(gse::render_layer::popup);
	ctx.queue_sprite({
		.rect = ui_rect::from_position_size({ px + 4.f, top_y - 4.f }, { pw, ph }),
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
			.font = ctx.font,
			.text = lines[i],
			.position = { px + pad, center_y + ctx.font->vertical_center_offset(fs) },
			.scale = fs,
			.color = i == 0 ? label_color : sty.color_text,
			.clip_rect = panel,
		});
	}
}

auto gse::ide::draw_code_panel(gse::gui::builder& ui, workspace::data& ws, const search::index_state* index, const gse::shared_view<config_system::data> config, gse::channel_writer channels, const gse::gpu::bindless_slot viewport_slot, const bool game_running) -> void {
	const auto& ctx = ui.ctx;
	if (ctx.clip_stack.empty()) {
		return;
	}
	const gse::ide::ui_rect body = ctx.clip_stack.back();
	const float pad = ctx.style.padding;
	const float font_sz = ctx.style.font_size;

	static std::uint64_t spin_frame = 0;
	++spin_frame;
	const gse::angle spin_rotation = gse::radians(static_cast<float>(spin_frame) * 0.16f);
	const bool analyzing = ws.diagnostics_pending && !ws.diagnostics_pending->done.load(std::memory_order_acquire);
	const std::uint32_t analyzing_id = analyzing ? ws.diagnostics_pending->document_id : 0;

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

	const float row_h = std::min(ctx.font->line_height(font_sz) + pad, body.height());
	const float tab_gap = 2.f;
	const float close_w = ctx.font->width("x", font_sz);

	struct tab_slot {
		std::uint32_t id;
		std::string caption;
		gse::ide::ui_rect rect;
		gse::ide::ui_rect close_rect;
	};

	std::vector<std::pair<std::uint32_t, std::string>> captions;
	captions.reserve(ws.tab_order.size());
	for (const std::uint32_t id : ws.tab_order) {
		const document& doc = ws.documents.at(id);
		captions.emplace_back(id, doc.dirty ? doc.tab_name + " *" : doc.tab_name);
	}

	const std::string game_caption = "Game";
	const float game_width = std::clamp(ctx.font->width(game_caption, font_sz) + pad * 3.f, 64.f, 220.f);
	const float game_divider_width = ctx.style.accent_bar_width;
	const float document_area_width = std::max(0.f, body.width() - game_width - game_divider_width - tab_gap);

	std::vector<float> widths;
	widths.reserve(captions.size());
	float total_tab_width = 0.f;
	for (const auto& [_, caption] : captions) {
		const float w = std::clamp(ctx.font->width(caption, font_sz) + pad * 3.f + close_w, 64.f, 220.f);
		widths.push_back(w);
		total_tab_width += w;
	}
	total_tab_width += tab_gap * static_cast<float>(captions.empty() ? 0 : captions.size() - 1);

	const float available_width = document_area_width;
	std::uint32_t required_rows = 1;
	float row_width = 0.f;
	for (const float w : widths) {
		if (row_width > 0.f && row_width + tab_gap + w > available_width) {
			++required_rows;
			row_width = w;
		}
		else {
			row_width += (row_width > 0.f ? tab_gap : 0.f) + w;
		}
	}
	ws.tab_visible_rows = std::clamp(ws.tab_visible_rows, 1u, std::max(1u, required_rows));

	const bool overflow = total_tab_width > available_width;
	constexpr float tab_scrollbar_h = 6.f;
	const float tab_bar_h = std::min(
		body.height(),
		ws.tab_visible_rows * row_h + static_cast<float>(ws.tab_visible_rows - 1) * tab_gap + (ws.tab_visible_rows == 1 && overflow ? tab_scrollbar_h + tab_gap : 0.f)
	);
	const gse::ide::ui_rect tab_bar = gse::ide::ui_rect::from_position_size(
		{ body.left(), body.top() },
		{ body.width(), tab_bar_h }
	);
	const gse::ide::ui_rect document_tab_bar = gse::ide::ui_rect::from_position_size(
		{ tab_bar.left(), tab_bar.top() },
		{ document_area_width, tab_bar.height() }
	);
	ctx.queue_sprite({
		.rect = tab_bar,
		.color = ctx.style.color_input_background,
		.texture = ctx.blank_texture,
	});

	std::vector<tab_slot> slots;
	const gse::vec2f mouse = ctx.input.mouse_position();
	if (overflow && tab_bar.contains(mouse) && !ctx.is_scroll_consumed()) {
		const gse::vec2f wheel = ctx.input.scroll_delta();
		const bool shift = ctx.input.key_held(gse::key::left_shift) || ctx.input.key_held(gse::key::right_shift);
		if (std::abs(wheel.y()) > 0.001f && !shift) {
			if (wheel.y() > 0.f) {
				if (ws.tab_visible_rows > 1) {
					--ws.tab_visible_rows;
				}
			}
			else {
				++ws.tab_visible_rows;
			}
			ctx.consume_scroll();
		}
		else if (std::abs(wheel.x()) > 0.001f || (shift && std::abs(wheel.y()) > 0.001f)) {
			ws.tab_scroll_x.offset -= (wheel.x() + wheel.y()) * 80.f;
			ws.tab_scroll_x.target = ws.tab_scroll_x.offset;
			ctx.consume_scroll();
		}
	}

	slots.reserve(captions.size());
	if (ws.tab_visible_rows == 1) {
		const float max_scroll = std::max(0.f, total_tab_width - available_width);
		ws.tab_scroll_x.offset = std::clamp(ws.tab_scroll_x.offset, 0.f, max_scroll);
		ws.tab_scroll_x.target = std::clamp(ws.tab_scroll_x.target, 0.f, max_scroll);
		if (ws.tab_scroll_active_id != ws.active_document_id) {
			if (const auto active = std::ranges::find_if(captions, [&](const auto& e) { return e.first == ws.active_document_id; }); active != captions.end()) {
				const std::size_t active_index = static_cast<std::size_t>(std::distance(captions.begin(), active));
				float active_left = 0.f;
				for (std::size_t i = 0; i < active_index; ++i) {
					active_left += widths[i] + tab_gap;
				}
				const float active_right = active_left + widths[active_index];
				if (active_left < ws.tab_scroll_x.offset) {
					ws.tab_scroll_x.offset = active_left;
				}
				else if (active_right > ws.tab_scroll_x.offset + available_width) {
					ws.tab_scroll_x.offset = active_right - available_width;
				}
				ws.tab_scroll_x.offset = std::clamp(ws.tab_scroll_x.offset, 0.f, max_scroll);
				ws.tab_scroll_x.target = ws.tab_scroll_x.offset;
			}
		}
		ws.tab_scroll_active_id = ws.active_document_id;

		float x = tab_bar.left() - ws.tab_scroll_x.offset;
		for (std::size_t i = 0; i < captions.size(); ++i) {
			const auto& [id, caption] = captions[i];
			const gse::ide::ui_rect tab_rect = gse::ide::ui_rect::from_position_size({ x, tab_bar.top() }, { widths[i], row_h });
			const gse::ide::ui_rect close_rect = id == viewport_tab_id ? gse::ide::ui_rect{} : gse::ide::ui_rect::from_position_size(
				{ tab_rect.right() - close_w - pad, tab_rect.center().y() + close_w * 0.5f },
				{ close_w, close_w }
			);
			slots.push_back({ id, caption, tab_rect, close_rect });
			x += widths[i] + tab_gap;
		}
	}
	else {
		ws.tab_scroll_x.offset = 0.f;
		ws.tab_scroll_x.target = 0.f;
		float x = document_tab_bar.left();
		float y = document_tab_bar.top();
		std::uint32_t row = 0;
		for (std::size_t i = 0; i < captions.size(); ++i) {
			if (x > document_tab_bar.left() && x + widths[i] > document_tab_bar.right()) {
				++row;
				x = document_tab_bar.left();
				y -= row_h + tab_gap;
			}
			if (row >= ws.tab_visible_rows) {
				break;
			}
			const auto& [id, caption] = captions[i];
			const gse::ide::ui_rect tab_rect = gse::ide::ui_rect::from_position_size({ x, y }, { widths[i], row_h });
			const gse::ide::ui_rect close_rect = id == viewport_tab_id ? gse::ide::ui_rect{} : gse::ide::ui_rect::from_position_size(
				{ tab_rect.right() - close_w - pad, tab_rect.center().y() + close_w * 0.5f },
				{ close_w, close_w }
			);
			slots.push_back({ id, caption, tab_rect, close_rect });
			x += widths[i] + tab_gap;
		}
	}

	const gse::ide::ui_rect game_tab = gse::ide::ui_rect::from_position_size(
		{ tab_bar.right() - game_width, tab_bar.top() },
		{ game_width, tab_bar_h }
	);
	const gse::ide::ui_rect game_divider = gse::ide::ui_rect::from_position_size(
		{ game_tab.left() - game_divider_width, tab_bar.top() },
		{ game_divider_width, tab_bar_h }
	);

	if (ws.tab_visible_rows == 1) {
		const float max_scroll = std::max(0.f, total_tab_width - available_width);
		if (max_scroll > 0.f && available_width > 0.f) {
			const gse::ide::ui_rect track_rect = gse::ide::ui_rect::from_position_size(
				{ tab_bar.left(), tab_bar.bottom() + tab_scrollbar_h + 1.f },
				{ available_width, tab_scrollbar_h }
			);
			const gse::gui::scroll_bar_result bar = gse::gui::update_scroll_bar(ws.tab_scroll_x, {
				.track_rect = track_rect,
				.visible_extent = available_width,
				.content_extent = total_tab_width,
				.horizontal = true,
				.mouse = mouse,
				.mouse_pressed = ctx.input.mouse_button_pressed(gse::mouse_button::button_1) && ctx.input_available(),
				.mouse_held = ctx.input.mouse_button_held(gse::mouse_button::button_1),
				.min_thumb = 24.f,
			});
			if (bar.used_press) {
				ctx.consume_press(gse::mouse_button::button_1);
			}

			gse::vec4f track_color = ctx.style.color_widget_background;
			track_color.w() *= 0.4f;
			ctx.queue_sprite({
				.rect = bar.track_rect,
				.color = track_color,
				.texture = ctx.blank_texture,
			});
			ctx.queue_sprite({
				.rect = bar.thumb_rect,
				.color = bar.held || bar.hovered ? ctx.style.color_widget_hovered : ctx.style.color_widget_background,
				.texture = ctx.blank_texture,
			});
		}
	}

	std::uint32_t close_requested = 0;

	if (ctx.input.mouse_button_pressed(gse::mouse_button::button_1) && ctx.input_available()) {
		if (game_tab.contains(mouse)) {
			ws.active_document_id = viewport_tab_id;
			ws.dragging_tab = 0;
		}
		for (const tab_slot& s : slots) {
			const gse::ide::ui_rect visible = s.rect.intersection(document_tab_bar);
			if (visible.width() <= 0.f || visible.height() <= 0.f) {
				continue;
			}
			if (s.close_rect.contains(mouse) && document_tab_bar.contains(mouse)) {
				close_requested = s.id;
				break;
			}
			if (visible.contains(mouse)) {
				ws.active_document_id = s.id;
				ws.dragging_tab = s.id;
				break;
			}
		}
	}

	if (ctx.input.mouse_button_pressed(gse::mouse_button::button_2) && ctx.input_available()) {
		for (const tab_slot& s : slots) {
			const gse::ide::ui_rect visible = s.rect.intersection(document_tab_bar);
			if (visible.contains(mouse)) {
				ws.active_document_id = s.id;
				ctx.open_context_menu({
					.position = mouse,
					.items = gse::gui::to_menu_items(tab_actions()),
					.target = s.id,
					.tag = tab_context_tag(),
				});
				break;
			}
		}
	}

	if (ws.dragging_tab != 0 && ctx.input.mouse_button_held(gse::mouse_button::button_1)) {
		if (const auto cur = std::ranges::find(ws.tab_order, ws.dragging_tab); cur != ws.tab_order.end()) {
			const auto cur_idx = static_cast<std::size_t>(std::distance(ws.tab_order.begin(), cur));
			std::size_t target = 0;
			for (const tab_slot& s : slots) {
				if (s.id != ws.dragging_tab && mouse.x() > s.rect.center().x()) {
					++target;
				}
			}
			target = std::min(target, ws.tab_order.size() - 1);
			if (target != cur_idx) {
				ws.tab_order.erase(ws.tab_order.begin() + static_cast<std::ptrdiff_t>(cur_idx));
				ws.tab_order.insert(ws.tab_order.begin() + static_cast<std::ptrdiff_t>(target), ws.dragging_tab);
			}
		}
	}
	else {
		ws.dragging_tab = 0;
	}

	for (const tab_slot& s : slots) {
		const gse::ide::ui_rect visible = s.rect.intersection(document_tab_bar);
		if (visible.width() <= 0.f || visible.height() <= 0.f) {
			continue;
		}
		const bool active = s.id == ws.active_document_id;
		const bool hovered = visible.contains(mouse) && ctx.input_available();
		const bool close_hovered = s.close_rect.contains(mouse) && document_tab_bar.contains(mouse) && ctx.input_available();

		ctx.queue_sprite({
			.rect = s.rect,
			.color = active ? ctx.style.color_widget_background : (hovered ? ctx.style.color_widget_hovered : ctx.style.color_input_background),
			.texture = ctx.blank_texture,
			.clip_rect = visible,
			.corner_radius = ctx.style.corner_radius,
		});
		ctx.queue_text({
			.font = ctx.font,
			.text = s.caption,
			.position = { s.rect.left() + pad, s.rect.center().y() + ctx.font->vertical_center_offset(font_sz) },
			.scale = font_sz,
			.color = active ? ctx.style.color_text : ctx.style.color_text_secondary,
			.clip_rect = visible,
		});
		if (s.id == analyzing_id) {
			draw_spinner(ctx, s.close_rect, ctx.style.color_text_secondary, spin_rotation);
		}
		else if (s.id != viewport_tab_id) {
			gse::gui::symbol::draw(ctx, gse::gui::symbol::close(), s.close_rect, {
				.color = close_hovered ? ctx.style.color_text : ctx.style.color_text_secondary,
				.scale = ctx.style.icon_scale,
				.clip_rect = visible,
			});
		}
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
		.font = ctx.font,
		.text = game_caption,
		.position = { game_tab.left() + pad, game_tab.center().y() + ctx.font->vertical_center_offset(font_sz) },
		.scale = font_sz,
		.color = game_active ? ctx.style.color_text : ctx.style.color_text_secondary,
		.clip_rect = game_tab,
	});
	ctx.queue_sprite({
		.rect = gse::ide::ui_rect::from_position_size(
			{ tab_bar.left(), tab_bar.bottom() },
			{ tab_bar.width(), ctx.style.accent_bar_width }
		),
		.color = ctx.style.color_accent,
		.texture = ctx.blank_texture,
	});

	if (close_requested != 0) {
		workspace::close_document(ws, close_requested);
	}

	const float requested_status_h = ctx.font->line_height(font_sz) + pad * 0.5f;
	const float content_h = std::max(0.f, body.height() - tab_bar_h);
	const float status_h = std::min(requested_status_h, content_h);
	const float text_h = content_h - status_h;
	const gse::ide::ui_rect text_rect = gse::ide::ui_rect::from_position_size(
		{ body.left(), body.top() - tab_bar_h },
		{ body.width(), text_h }
	);
	const gse::ide::ui_rect status_rect = gse::ide::ui_rect::from_position_size(
		{ body.left(), body.bottom() + status_h },
		{ body.width(), status_h }
	);

	if (ws.active_document_id == viewport_tab_id) {
		const gse::ide::ui_rect view_rect = gse::ide::ui_rect::from_position_size(
			{ body.left(), body.top() - tab_bar_h },
			{ body.width(), content_h }
		);
		if (!game_running) {
			ctx.queue_sprite({
				.rect = view_rect,
				.color = ctx.style.color_panel_alt,
				.texture = ctx.blank_texture,
			});

			const bool building = build_runner::in_progress();
			const float button_w = std::min(220.f, std::max(120.f, view_rect.width() - pad * 4.f));
			const float button_h = ctx.font->line_height(font_sz) + pad;
			const gse::ide::ui_rect run_rect = gse::ide::ui_rect::from_position_size(
				{ view_rect.center().x() - button_w * 0.5f, view_rect.center().y() + button_h * 0.5f },
				{ button_w, button_h }
			);
			const bool hovered = run_rect.contains(mouse) && ctx.input_available() && !building;
			if (hovered && ctx.mouse_pressed_for(run_rect)) {
				build_runner::start_build_and_run_game();
			}

			ctx.queue_sprite({
				.rect = run_rect,
				.color = building ? ctx.style.color_input_background : (hovered ? ctx.style.color_button_hovered : ctx.style.color_button_background),
				.texture = ctx.blank_texture,
				.corner_radius = ctx.style.corner_radius,
			});
			const std::string button_label = building ? "Building..." : "Build and Run Game";
			const float button_text_w = ctx.font->width(button_label, font_sz);
			ctx.queue_text({
				.font = ctx.font,
				.text = button_label,
				.position = { run_rect.center().x() - button_text_w * 0.5f, run_rect.center().y() + ctx.font->vertical_center_offset(font_sz) },
				.scale = font_sz,
				.color = building ? ctx.style.color_text_secondary : ctx.style.color_text,
				.clip_rect = run_rect,
			});

			const std::string title = "Game is not running";
			const float title_w = ctx.font->width(title, font_sz);
			const float line_h = ctx.font->line_height(font_sz);
			ctx.queue_text({
				.font = ctx.font,
				.text = title,
				.position = { view_rect.center().x() - title_w * 0.5f, run_rect.top() + line_h + pad + ctx.font->vertical_center_offset(font_sz) },
				.scale = font_sz,
				.color = ctx.style.color_text,
				.clip_rect = view_rect,
			});
			return;
		}
		ctx.queue_sprite({
			.rect = view_rect,
			.color = { 1.f, 1.f, 1.f, 1.f },
			.image_slot = viewport_slot,
		});
		return;
	}

	const auto it = ws.documents.find(ws.active_document_id);
	if (it == ws.documents.end()) {
		if (text_rect.width() <= 0.f || text_rect.height() <= 0.f) {
			return;
		}
		const std::string label = "Open a file from the explorer";
		const float w = ctx.font->width(label, font_sz);
		ctx.queue_text({
			.font = ctx.font,
			.text = label,
			.position = { text_rect.center().x() - w * 0.5f, text_rect.center().y() + ctx.font->vertical_center_offset(font_sz) },
			.scale = font_sz,
			.color = ctx.style.color_text_secondary,
			.clip_rect = text_rect,
		});
		return;
	}
	document& doc = it->second;

	if (ws.diagnostics_pending && ws.diagnostics_pending->done.load(std::memory_order_acquire)) {
		if (const auto pending = ws.documents.find(ws.diagnostics_pending->document_id); pending != ws.documents.end()) {
			pending->second.analysis_failed = ws.diagnostics_pending->failed;
			pending->second.analysis_crashed = ws.diagnostics_pending->crashed;
			pending->second.analysis_duration = ws.diagnostics_pending->duration;
			if (!ws.diagnostics_pending->crashed) {
				pending->second.diagnostics = std::move(ws.diagnostics_pending->result);
				syntax_producer::set_semantic(pending->second.syntax, ws.diagnostics_pending->tokens, ws.diagnostics_pending->type_names, ws.diagnostics_pending->template_params, pending->second.buffer);
				pending->second.highlight_dirty = true;

				if (ws.diagnostics_pending->symbols_complete) {
					channels.push<search::index_merge_request>({ .path = pending->second.path, .check = ws.diagnostics_pending });
				}

				for (const analysis::gcc_diagnostic& diagnostic : pending->second.diagnostics) {
				const gse::log::level lvl = diagnostic.level == analysis::severity::error
					? gse::log::level::error
					: (diagnostic.level == analysis::severity::warning ? gse::log::level::warning : gse::log::level::info);
				const std::string where = diagnostic.file.empty()
					? pending->second.tab_name
					: std::filesystem::path(diagnostic.file).filename().generic_display_string();
				gse::log::println(lvl, gse::log::category::task, "analysis: {}:{}:{}: {}", where, diagnostic.line + 1, diagnostic.start_col + 1, diagnostic.message);
			}
		}
		}
		ws.diagnostics_pending.reset();
	}

	{
		int errors = 0;
		int warnings = 0;
		const std::string current_file = doc.path.filename().display_string();
		for (const auto& diagnostic : doc.diagnostics) {
			if (!diagnostic.file.empty() && std::filesystem::path(diagnostic.file).filename().display_string() != current_file) {
				continue;
			}
			if (diagnostic.level == analysis::severity::error) {
				++errors;
			}
			else if (diagnostic.level == analysis::severity::warning) {
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
			const gse::ide::ui_rect spin_rect = gse::ide::ui_rect::from_position_size(
				{ status_rect.left() + pad, status_rect.center().y() + glyph_size * 0.5f },
				{ glyph_size, glyph_size }
			);
			draw_spinner(ctx, spin_rect, ctx.style.color_text_secondary, spin_rotation);
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
				.font = ctx.font,
				.text = status_text,
				.position = { text_left, status_rect.center().y() + ctx.font->vertical_center_offset(font_sz) },
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
		analysis::severity level = analysis::severity::error;
	};

	std::vector<gse::gui::text_underline> underlines;
	std::vector<diag_region> diag_regions;
	if (!doc.diagnostics.empty() && !doc.analysis_failed) {
		const std::string doc_name = doc.path.filename().display_string();
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
		underlines.reserve(doc.diagnostics.size());
		diag_regions.reserve(doc.diagnostics.size());
		for (const auto& diagnostic : doc.diagnostics) {
			if (!diagnostic.file.empty() && std::filesystem::path(diagnostic.file).filename().display_string() != doc_name) {
				continue;
			}
			if (diagnostic.line >= line_count) {
				continue;
			}
			const std::string_view row = doc.buffer.line(diagnostic.line);
			const std::uint32_t byte_start = display_to_byte(row, diagnostic.start_col);
			const std::uint32_t byte_end = std::max(byte_start + 1, display_to_byte(row, diagnostic.end_col));
			const gse::vec4f color = diagnostic.level == analysis::severity::warning
				? gse::vec4f{ 0.71f, 0.57f, 0.11f, 1.f }
				: (diagnostic.level == analysis::severity::note ? gse::vec4f{ 0.43f, 0.50f, 0.64f, 1.f } : gse::vec4f{ 0.855f, 0.451f, 0.424f, 1.f });
			underlines.push_back({ .line = diagnostic.line, .start_col = byte_start, .end_col = byte_end, .color = color });
			diag_regions.push_back({ .line = diagnostic.line, .byte_start = byte_start, .byte_end = byte_end, .color = color, .message = diagnostic.message, .level = diagnostic.level });
		}
	}

	if (doc.pending_center_line) {
		const float line_h = ctx.font->line_height(font_sz) * 1.25f;
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
	static gse::cursor_shape last_cursor = gse::cursor_shape::arrow;
	const gse::cursor_shape want_cursor = link_target.has_value() ? gse::cursor_shape::hand : gse::cursor_shape::arrow;
	if (want_cursor != last_cursor) {
		channels.push<gse::set_cursor_shape_request>({ .shape = want_cursor });
		last_cursor = want_cursor;
	}
	const bool goto_click = link_target.has_value() && ctx.mouse_pressed_for(text_rect);

	if (!ws.cppref.loaded) {
		ws.cppref.load(config::cppref_index);
	}
	std::optional<std::size_t> hovered_diag;
	if (!goto_ctrl && !diag_regions.empty() && text_rect.contains(mouse) && ctx.input_available() && !doc.buffer.lines.empty()) {
		const float diag_line_h = ctx.font->line_height(font_sz) * 1.25f;
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
	const bool panel_alive = !hovered_diag && ws.hover.has_card && hover_kept_alive(ws.hover, mouse);
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
			if (ws.hover.line != hp.line || ws.hover.column != static_cast<std::uint32_t>(a) || std::string_view(ws.hover.ident) != ident) {
				ws.hover = {};
				ws.hover.line = hp.line;
				ws.hover.column = static_cast<std::uint32_t>(a);
				ws.hover.ident = std::string(ident);
				ws.hover.since = gse::system_clock::now<gse::time>();
			}
			else if (!ws.hover.resolved && gse::system_clock::now<gse::time>() - ws.hover.since > gse::milliseconds(350)) {
				ws.hover.resolved = true;
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
				if (qualified.empty()) {
					qualified = docs::expand_qualified(row, a);
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
				if (const std::optional<docs::cppref_hit> cref = ws.cppref.find(docs::normalize_qualified(qualified))) {
					ws.hover.title = qualified;
					ws.hover.kind = std::string(cref->kind);
					ws.hover.body = std::string(cref->brief);
					ws.hover.url = docs::cppref_url(cref->page);
					ws.hover.from_cppref = true;
					ws.hover.has_card = true;
				}
				else if (def && !def->path.empty()) {
					if (const std::optional<docs::doc_card> card = docs::extract_definition(def->path, def->line, qualified, sym_kind == "function")) {
						ws.hover.title = card->title;
						ws.hover.kind = sym_kind;
						ws.hover.body = card->body;
						ws.hover.body_is_code = true;
						ws.hover.code_spans = syntax_producer::highlight(ws.hover.body, nullptr);
						ws.hover.has_card = true;
					}
				}
				if (!ws.hover.has_card && !qualified.empty()) {
					ws.hover.title = qualified;
					ws.hover.kind = sym_kind;
					ws.hover.has_card = true;
				}
				if (ws.hover.kind.empty() && doc.syntax.semantic) {
					const syntax_producer::semantic_data& sem = *doc.syntax.semantic;
					const std::uint64_t key = (static_cast<std::uint64_t>(hp.line) << 32) | static_cast<std::uint32_t>(a);
					if (const auto it = sem.at.find(key); it != sem.at.end()) {
						ws.hover.kind = std::format("{}", it->second);
					}
					else if (sem.template_params.contains(ident)) {
						ws.hover.kind = "template parameter";
					}
					else if (sem.types.contains(ident)) {
						ws.hover.kind = "type";
					}
					else if (sem.enums.contains(ident)) {
						ws.hover.kind = "enum member";
					}
					else if (const auto nit = sem.names.find(ident); nit != sem.names.end()) {
						ws.hover.kind = std::format("{}", nit->second);
					}
				}
				ws.hover.kind_color = ctx.style.color_text_secondary;
				for (const gse::gui::text_span& sp : doc.syntax.spans) {
					if (sp.line == hp.line && static_cast<std::uint32_t>(a) >= sp.start_col && static_cast<std::uint32_t>(a) < sp.end_col) {
						ws.hover.kind_color = sp.color;
						break;
					}
				}
				ws.hover.anchor = mouse;
			}
		}
		else {
			ws.hover = {};
		}
	}
	else if (!panel_alive) {
		ws.hover = {};
	}
	if (hovered_diag) {
		ws.hover = {};
		const diag_region& dr = diag_regions[*hovered_diag];
		const std::string_view diag_label = dr.level == analysis::severity::warning ? "warning" : dr.level == analysis::severity::note ? "note" : "error";
		draw_diagnostic_tooltip(ctx, text_rect, diag_label, dr.message, dr.color, mouse);
	}
	else if (ws.hover.has_card) {
		if (draw_hover_panel(ctx, text_rect, ws.hover) && !ws.hover.url.empty()) {
			analysis::process::open_url(ws.hover.url.c_str());
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
	}

	if (doc.highlightable && !doc.path.empty() && doc.diag_dirty
		&& gse::system_clock::now<gse::time>() - doc.last_edit > gse::milliseconds(500)
		&& (!ws.diagnostics_pending || ws.diagnostics_pending->done.load(std::memory_order_acquire))) {
		if (doc.dirty) {
			workspace::save_document(ws, ws.active_document_id);
		}
		if (const std::optional<std::filesystem::path> compile_commands = analysis::diagnostics_runner::find_compile_commands(ws.root)) {
			const std::shared_ptr<analysis::diagnostics_check> check = std::make_shared<analysis::diagnostics_check>();
			check->document_id = ws.active_document_id;
			std::error_code plugin_ec;
			const std::filesystem::path plugin = std::filesystem::exists(config::token_plugin, plugin_ec) ? config::token_plugin : std::filesystem::path{};
			analysis::diagnostics_runner::start(check, *compile_commands, doc.path, plugin);
			ws.diagnostics_pending = check;
		}
		doc.diag_dirty = false;
	}

	const bool ctrl = ctx.input.key_held(gse::key::left_control) || ctx.input.key_held(gse::key::right_control);
	if (ui.focus_widget_id == text_id && ctrl && ctx.input.key_pressed(gse::key::s)) {
		workspace::save_document(ws, ws.active_document_id);
		doc.diag_dirty = true;
		doc.last_edit = {};
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
		quick_search_state search;
		bool initialized = false;
		gse::clock save_clock;
	};

	[[= gse::system_run<>{}]]
	auto run(
		gse::context& ctx,
		data& d,
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
	const std::vector<layout_section> sections = parse_layout_sections(read_layout_file(editor_layout_path()));
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
	replace_layout_sections(editor_layout_section, out);
}

auto gse::ide::load_workspace_layout(workspace::data& ws) -> void {
	const std::vector<layout_section> sections = parse_layout_sections(read_layout_file(editor_layout_path()));
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

	ws.dragging_tab = 0;
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

	replace_layout_sections(workspace_layout_section, out);
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

			const float inset = s.reserve_top_bar ? s.fstate.sty.title_bar_height : 0.f;
			const float top = vp.y() - inset;
			if (top <= 0.f) {
				return;
			}

			const float min_explorer_width = 180.f;
			const float min_code_width = 320.f;
			const float min_terminal_height = 120.f;
			const float min_main_height = 180.f;
			const float hit_width = std::max(6.f, s.fstate.sty.resize_border_thickness);
			const bool blocked = s.menu_stack.captures_input() || s.context_menu.open;

			float explorer_width = std::clamp(
				vp.x() * d.explorer_ratio,
				min_explorer_width,
				std::max(min_explorer_width, vp.x() - min_code_width)
			);
			float terminal_height = std::clamp(
				top * d.terminal_ratio,
				min_terminal_height,
				std::max(min_terminal_height, top - min_main_height)
			);

			d.explorer_ratio = explorer_width / vp.x();
			d.terminal_ratio = terminal_height / top;

			const bool over_explorer_divider =
				std::abs(mouse.x() - explorer_width) <= hit_width &&
				mouse.y() >= terminal_height &&
				mouse.y() <= top;
			const bool over_terminal_divider =
				std::abs(mouse.y() - terminal_height) <= hit_width &&
				mouse.x() >= 0.f &&
				mouse.x() <= vp.x();

			if (!held) {
				d.resizing_explorer = false;
				d.resizing_terminal = false;
			}
			else if (!blocked && pressed) {
				d.resizing_explorer = over_explorer_divider;
				d.resizing_terminal = !d.resizing_explorer && over_terminal_divider;
			}

			if (d.resizing_explorer) {
				const float next_width = std::clamp(
					mouse.x(),
					min_explorer_width,
					std::max(min_explorer_width, vp.x() - min_code_width)
				);
				d.explorer_ratio = next_width / vp.x();
			}
			if (d.resizing_terminal) {
				const float next_height = std::clamp(
					mouse.y(),
					min_terminal_height,
					std::max(min_terminal_height, top - min_main_height)
				);
				d.terminal_ratio = next_height / top;
			}

			explorer_width = std::clamp(
				vp.x() * d.explorer_ratio,
				min_explorer_width,
				std::max(min_explorer_width, vp.x() - min_code_width)
			);
			terminal_height = std::clamp(
				top * d.terminal_ratio,
				min_terminal_height,
				std::max(min_terminal_height, top - min_main_height)
			);
			const float main_height = top - terminal_height;

			explorer->rect = gse::ide::ui_rect::from_position_size({ 0.f, top }, { explorer_width, main_height });
			explorer->swap_parent(gse::id());
			explorer->docked_to = gse::gui::dock::location::none;
			explorer->fixed = true;
			explorer->bare = true;

			code->rect = gse::ide::ui_rect::from_position_size({ explorer_width, top }, { vp.x() - explorer_width, main_height });
			code->swap_parent(gse::id());
			code->docked_to = gse::gui::dock::location::none;
			code->fixed = true;
			code->bare = true;

			term->rect = gse::ide::ui_rect::from_position_size({ 0.f, terminal_height }, { vp.x(), terminal_height });
			term->swap_parent(gse::id());
			term->docked_to = gse::gui::dock::location::none;
			term->fixed = true;
			term->bare = true;

			gse::gui::clear_menu_interaction(s);
		},
	});

	if (d.save_clock.elapsed() > editor_layout_save_interval) {
		save_editor_layout(d);
		d.save_clock.reset();
	}

	return {};
}

auto gse::ide::workspace_system::run(gse::context& ctx, data& d, const gse::shared_view<config_system::data> config_d, const gse::shared_view<search_system::data> search_d, const gse::shared_view<gse::input::data> input_d, const gse::shared_view<viewport::data> viewport_d) -> gse::async::task<> {
	if (!d.initialized) {
		d.ws.root = gse::config::root_dir;
		d.ws.fs_root.path = d.ws.root;
		d.ws.fs_root.name = d.ws.root.filename().display_string();
		d.ws.fs_root.is_dir = true;
		load_workspace_layout(d.ws);
		d.save_clock.reset();
		d.initialized = true;
	}

	workspace::data* ws = &d.ws;
	const auto config = config_d;
	const gse::input::state& input = gse::input::current_state(input_d);
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
		.build = [ws, search = &d.search, index = search_d.index, channels = ctx.channels](gse::gui::builder& b) {
			draw_explorer_panel(b, *ws, *search, index, channels);
		},
	});

	const gse::gpu::bindless_slot viewport_slot = viewport_d.ready ? viewport_d.display_slot : gse::gpu::bindless_slot{};
	const bool game_running = viewport_d.imported_ready;

	ctx.channels.push<gse::gui::menu_content>({
		.menu = std::string(code_panel_name),
		.layer = gse::render_layer::content,
		.build = [ws, index = search_d.index, config, channels = ctx.channels, viewport_slot, game_running](gse::gui::builder& b) {
			draw_code_panel(b, *ws, index, config, channels, viewport_slot, game_running);
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

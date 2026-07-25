module gse.ide.terminal;

import std;
import gse;
import gse.ide.build;
import gse.ide.navigation;
import gse.win32;

namespace gse::ide::terminal {
	constexpr std::size_t max_lines = 8192;

	struct link_hit {
		std::filesystem::path path;
		std::uint32_t line = 0;
		std::uint32_t column = 0;
		std::uint32_t start_col = 0;
		std::uint32_t end_col = 0;
	};

	auto path_link_at(std::string_view row, std::uint32_t column) -> std::optional<link_hit>;

	auto level_color(const gui::style& sty, log::level lvl) -> vec4f;

	auto ellipsize_path(std::string_view path) -> std::string;

	auto header_button(gui::builder& ui, const rectf& rect, std::span<const gui::symbol::stroke> glyph, std::string_view label, bool enabled) -> bool;

	auto confirm_button(gui::builder& ui, const rectf& rect, std::string_view label, bool danger) -> bool;

	auto draw_close_confirm(gui::builder& ui, data& d, const rectf& body) -> void;

	auto widen(const std::string& s) -> std::wstring;

	auto run_command(command_runner& runner, const std::string& command, const std::wstring& cwd) -> void;

	auto draw_instance(gui::builder& ui, data& d, instance& inst, const rectf& area, channel_writer channels, bool building) -> void;
}

auto gse::ide::terminal::level_color(const gui::style& sty, const log::level lvl) -> vec4f {
	switch (lvl) {
		case log::level::debug:
			return sty.color_text_disabled;
		case log::level::info:
			return sty.color_text;
		case log::level::warning:
			return vec4f{ 0.71f, 0.57f, 0.11f, 1.f };
		case log::level::error:
			return vec4f{ 0.855f, 0.451f, 0.424f, 1.f };
		case log::level::fatal:
			return vec4f{ 0.94f, 0.30f, 0.30f, 1.f };
	}
	return sty.color_text;
}

auto gse::ide::terminal::ellipsize_path(const std::string_view path) -> std::string {
	std::vector<std::string_view> parts;
	std::size_t start = 0;
	for (std::size_t i = 0; i <= path.size(); ++i) {
		if (i == path.size() || path[i] == '\\' || path[i] == '/') {
			if (i > start) {
				parts.push_back(path.substr(start, i - start));
			}
			start = i + 1;
		}
	}
	if (parts.size() <= 3) {
		return std::string(path);
	}
	return std::format("{}\\...\\{}\\{}", parts.front(), parts[parts.size() - 2], parts.back());
}

auto gse::ide::terminal::path_link_at(const std::string_view row, const std::uint32_t column) -> std::optional<link_hit> {
	auto is_link_char = [](const char c) -> bool {
		return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.' || c == '-' || c == '/' || c == '\\' || c == ':' || c == '~' || c == '+';
	};

	if (row.empty()) {
		return std::nullopt;
	}
	std::size_t a = std::min<std::size_t>(column, row.size());
	if (a == row.size() || !is_link_char(row[a])) {
		if (a == 0 || !is_link_char(row[a - 1])) {
			return std::nullopt;
		}
		--a;
	}
	std::size_t b = a;
	while (a > 0 && is_link_char(row[a - 1])) {
		--a;
	}
	while (b < row.size() && is_link_char(row[b])) {
		++b;
	}

	const std::string token(row.substr(a, b - a));

	std::uint32_t peeled[2] = { 0, 0 };
	std::size_t peeled_count = 0;
	std::size_t end = token.size();
	while (end > 0 && token[end - 1] == ':') {
		--end;
	}
	while (peeled_count < 2) {
		std::size_t digits = end;
		while (digits > 0 && std::isdigit(static_cast<unsigned char>(token[digits - 1]))) {
			--digits;
		}
		if (digits == end || digits == 0 || token[digits - 1] != ':') {
			break;
		}
		std::uint32_t value = 0;
		std::from_chars(token.data() + digits, token.data() + end, value);
		peeled[peeled_count++] = value;
		end = digits - 1;
	}

	const std::string_view path_str = std::string_view(token).substr(0, end);
	if (path_str.empty()) {
		return std::nullopt;
	}

	const std::uint32_t line = peeled_count == 2 ? peeled[1] : (peeled_count == 1 ? peeled[0] : 0);
	const std::uint32_t col = peeled_count == 2 ? peeled[0] : 0;

	const std::filesystem::path candidate(path_str);
	std::filesystem::path resolved = candidate.is_absolute() ? candidate : gse::config::root_dir() / candidate;
	std::error_code ec;
	if (!std::filesystem::is_regular_file(resolved, ec)) {
		return std::nullopt;
	}

	return link_hit{
		.path = std::move(resolved),
		.line = line > 0 ? line - 1 : 0,
		.column = col > 0 ? col - 1 : 0,
		.start_col = static_cast<std::uint32_t>(a),
		.end_col = static_cast<std::uint32_t>(b),
	};
}

auto gse::ide::terminal::ring_sink::push(const log::level lvl, std::string text) -> void {
	std::lock_guard lock(m_mutex);
	m_lines.push_back({ .seq = m_next++, .lvl = lvl, .text = std::move(text) });
	while (m_lines.size() > max_lines) {
		m_lines.pop_front();
	}
}

auto gse::ide::terminal::ring_sink::write(const log::record& rec) -> void {
	push(rec.lvl, std::format("[{}] {}{}", rec.cat, rec.prefix, rec.message));
}

auto gse::ide::terminal::ring_sink::write_raw(const std::string_view text) -> void {
	push(log::level::info, std::string(text));
}

auto gse::ide::terminal::ring_sink::flush() -> void {
}

auto gse::ide::terminal::ring_sink::drain(std::uint64_t& cursor, std::vector<line>& out) -> void {
	std::lock_guard lock(m_mutex);
	for (const line& l : m_lines) {
		if (l.seq >= cursor) {
			out.push_back(l);
		}
	}
	cursor = m_next;
}

auto gse::ide::terminal::ring_sink::sequence() -> std::uint64_t {
	std::lock_guard lock(m_mutex);
	return m_next;
}

auto gse::ide::terminal::init(data& d) -> async::task<> {
	register_sink(d);
	return {};
}

auto gse::ide::terminal::run(context& ctx, data& d, const shared_view<build_runner::data> build_d) -> async::task<> {
	for (const build_runner::stream_opened& opened : ctx.read_channel<build_runner::stream_opened>()) {
		d.instances.push_back({
			.name = opened.name,
			.cursor = d.sink ? d.sink->sequence() : 0,
			.runner = opened.stream,
			.interactive = false,
		});
		d.active = d.instances.size() - 1;
	}
	const bool building = build_d.building;
	ctx.channels.push<gui::menu_content>({
		.menu = std::string(panel_name),
		.layer = render_layer::content,
		.build = [d = &d, channels = ctx.channels, building](gui::builder& b) {
			draw_panel(b, *d, channels, building);
		},
	});
	return {};
}

auto gse::ide::terminal::register_sink(data& d) -> void {
	auto s = std::make_unique<ring_sink>();
	d.sink = s.get();
	log::add_sink(std::move(s));
	d.instances.push_back({ .name = "Log", .follows_log = true });
}

auto gse::ide::terminal::widen(const std::string& s) -> std::wstring {
	if (s.empty()) {
		return {};
	}
	const int length = win32::MultiByteToWideChar(win32::cp_utf8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
	std::wstring out(static_cast<std::size_t>(length), L'\0');
	win32::MultiByteToWideChar(win32::cp_utf8, 0, s.data(), static_cast<int>(s.size()), out.data(), length);
	return out;
}

auto gse::ide::terminal::run_command(command_runner& runner, const std::string& command, const std::wstring& cwd) -> void {
	spawn::run_capture(runner, L"cmd.exe /c " + widen(command), cwd, {});
	spawn::close_process(runner);
}

auto gse::ide::terminal::header_button(gui::builder& ui, const rectf& rect, const std::span<const gui::symbol::stroke> glyph, const std::string_view label, const bool enabled) -> bool {
	const gui::draw_context& ctx = ui.ctx;
	const gui::style& sty = ctx.style;
	const float pad = sty.padding;

	const vec2f mouse = ctx.input.mouse_position();
	const bool hovered = enabled && rect.contains(mouse) && ctx.input_available();
	const bool clicked = hovered && ctx.input.mouse_button_pressed(mouse_button::button_1);

	ctx.queue_sprite({
		.rect = rect,
		.color = hovered ? sty.color_widget_hovered : sty.color_input_background,
		.texture = ctx.blank_texture,
	});

	const vec4f fg = enabled ? sty.color_text : sty.color_text_secondary;
	const float glyph_size = rect.height();
	const float glyph_left = label.empty() ? rect.center().x() - glyph_size * 0.5f : rect.left() + pad * 0.5f;
	const rectf glyph_rect = rectf::from_position_size(
		{ glyph_left, rect.top() },
		{ glyph_size, glyph_size }
	);
	gui::symbol::draw(ctx, glyph, glyph_rect, {
		.color = fg,
		.scale = 0.55f,
		.clip_rect = rect,
	});
	if (!label.empty()) {
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = std::string(label),
			.position = { glyph_rect.right(), rect.center().y() + ctx.fonts.text->vertical_center_offset(sty.font_size) },
			.scale = sty.font_size,
			.color = fg,
			.clip_rect = rect,
		});
	}

	return clicked;
}

auto gse::ide::terminal::draw_instance(gui::builder& ui, data& d, instance& inst, const rectf& area, channel_writer channels, const bool building) -> void {
	const gui::draw_context& ctx = ui.ctx;
	const id input_id = gui::ids::make("##term_input_" + inst.name);
	const std::string prompt = ellipsize_path(gse::config::root_dir().display_string()) + "> ";

	std::vector<line> fresh;
	if (inst.follows_log) {
		d.sink->drain(inst.cursor, fresh);
	}
	if (inst.runner) {
		std::lock_guard lock(inst.runner->mutex);
		for (std::string& l : inst.runner->lines) {
			fresh.push_back({ .seq = 0, .lvl = log::level::info, .text = std::move(l) });
		}
		inst.runner->lines.clear();
	}

	const bool busy = inst.runner && inst.runner->running.load(std::memory_order_acquire);
	if (inst.interactive && ui.focus_widget_id == input_id && ctx.key_pressed_for(key::enter) && !inst.input.empty() && !busy) {
		fresh.push_back({ .seq = 0, .lvl = log::level::info, .text = prompt + inst.input });
		if (!inst.runner) {
			inst.runner = std::make_shared<command_runner>();
		}
		inst.runner->running.store(true, std::memory_order_release);
		std::thread([r = inst.runner, cmd = inst.input, cwd = gse::config::root_dir().wstring()] {
			run_command(*r, cmd, cwd);
		}).detach();
		inst.input.clear();
		inst.input_state = {};
	}

	if (!fresh.empty()) {
		for (const line& l : fresh) {
			std::size_t start = 0;
			while (true) {
				const std::size_t nl = l.text.find('\n', start);
				const std::string_view piece = std::string_view(l.text).substr(start, nl == std::string::npos ? std::string::npos : nl - start);
				inst.buffer.lines.emplace_back(piece);
				inst.line_levels.push_back(l.lvl);
				if (nl == std::string::npos) {
					break;
				}
				start = nl + 1;
			}
		}

		while (inst.buffer.lines.size() > max_lines) {
			inst.buffer.lines.erase(inst.buffer.lines.begin());
			inst.line_levels.erase(inst.line_levels.begin());
		}

		inst.spans.clear();
		inst.spans.reserve(inst.buffer.lines.size());
		for (std::size_t i = 0; i < inst.buffer.lines.size(); ++i) {
			inst.spans.push_back({
				.line = static_cast<std::uint32_t>(i),
				.start_col = 0,
				.end_col = static_cast<std::uint32_t>(inst.buffer.lines[i].size()),
				.color = level_color(ctx.style, inst.line_levels[i]),
			});
		}

		inst.view.scroll.y.target = std::numeric_limits<float>::max();
	}

	if (inst.buffer.lines.empty()) {
		inst.buffer.lines.emplace_back();
		inst.line_levels.push_back(log::level::info);
	}

	const float pad = ctx.style.padding;
	const float input_h = inst.interactive ? ctx.fonts.code->line_height(ctx.style.font_size) + pad : 0.f;

	const rectf log_rect = rectf::from_position_size(
		{ area.left(), area.top() },
		{ area.width(), std::max(0.f, area.height() - input_h) }
	);

	std::vector<gui::text_underline> underlines;
	std::optional<link_hit> link;
	const vec2f mouse = ctx.input.mouse_position();
	const bool goto_ctrl = ctx.input.key_held(key::left_control) || ctx.input.key_held(key::right_control);
	if (goto_ctrl && log_rect.contains(mouse) && ctx.input_available() && !inst.buffer.lines.empty()) {
		const gui::buffer_position hover = gui::draw::text_area_position_at(ctx, inst.buffer, inst.view, log_rect, false, 4, mouse);
		if (const std::optional<link_hit> hit = path_link_at(inst.buffer.line(hover.line), hover.column)) {
			link = hit;
			underlines.push_back({ .line = hover.line, .start_col = hit->start_col, .end_col = hit->end_col, .color = ctx.style.color_text_secondary });
		}
	}
	if (link) {
		channels.push<set_cursor_shape_request>({ .shape = cursor_shape::hand });
	}
	const bool goto_click = link.has_value() && ctx.mouse_pressed_for(log_rect);

	gui::draw::text_area_in_rect(
		ctx,
		gui::ids::make("##term_log_" + inst.name),
		inst.buffer,
		inst.view,
		std::span<const gui::text_span>(inst.spans),
		underlines,
		{},
		log_rect,
		true,
		false,
		4,
		false,
		false,
		time{},
		ui.hot_widget_id,
		ui.focus_widget_id
	);

	if (goto_click && link) {
		channels.push<jump_to_request>({ .path = link->path, .line = link->line, .column = link->column });
	}

	if (!inst.interactive) {
		return;
	}

	const rectf input_rect = rectf::from_position_size(
		{ area.left(), area.bottom() + input_h },
		{ area.width(), input_h }
	);

	ctx.queue_sprite({
		.rect = input_rect,
		.color = ctx.style.color_input_background,
		.texture = ctx.blank_texture,
	});

	const float prompt_width = ctx.fonts.code->width(prompt, ctx.style.font_size) + pad;
	ctx.queue_text({
		.font = ctx.fonts.code,
		.text = prompt,
		.position = { input_rect.left() + pad, input_rect.center().y() + ctx.fonts.code->vertical_center_offset(ctx.style.font_size) },
		.scale = ctx.style.font_size,
		.color = ctx.style.color_accent,
		.clip_rect = input_rect,
	});

	const rectf run_btn = rectf::from_position_size(
		{ input_rect.right() - input_h - pad, input_rect.top() },
		{ input_h, input_h }
	);
	if (header_button(ui, run_btn, gui::symbol::play(), "", !building)) {
		channels.push<build_runner::build_request>({
			.target = build_runner::build_target::game,
			.run_after = true,
		});
	}

	const std::string build_label = building ? "Building..." : "Build Game";
	const float build_w = ctx.fonts.text->width(build_label, ctx.style.font_size) + input_h + pad * 1.5f;
	const rectf build_btn = rectf::from_position_size(
		{ run_btn.left() - build_w, input_rect.top() },
		{ build_w, input_h }
	);
	if (header_button(ui, build_btn, gui::symbol::hammer(), build_label, !building)) {
		channels.push<build_runner::build_request>({
			.target = build_runner::build_target::game,
		});
	}

	const rectf input_box = rectf::from_position_size(
		{ input_rect.left() + prompt_width, input_rect.top() },
		{ std::max(0.f, build_btn.left() - pad - input_rect.left() - prompt_width), input_h }
	);

	gui::draw::text_input_in_rect(
		ctx,
		input_id,
		inst.input,
		inst.input_state,
		input_box,
		ui.hot_widget_id,
		ui.focus_widget_id,
		ctx.fonts.code
	);

	ctx.queue_sprite({
		.rect = rectf::from_position_size({ input_rect.left(), input_rect.top() }, { input_rect.width(), ctx.style.accent_bar_width }),
		.color = ctx.style.color_accent,
		.texture = ctx.blank_texture,
	});

	if (ctx.input.mouse_button_pressed(mouse_button::button_1) && ctx.input_available() && input_rect.contains(ctx.input.mouse_position())) {
		ui.focus_widget_id = input_id;
	}
}

auto gse::ide::terminal::confirm_button(gui::builder& ui, const rectf& rect, const std::string_view label, const bool danger) -> bool {
	const gui::draw_context& ctx = ui.ctx;
	const gui::style& sty = ctx.style;
	const vec2f mouse = ctx.input.mouse_position();
	const bool hovered = rect.contains(mouse) && ctx.input_available();
	const bool clicked = hovered && ctx.input.mouse_button_pressed(mouse_button::button_1);

	const vec4f base = danger ? vec4f{ 0.62f, 0.22f, 0.22f, 1.f } : sty.color_input_background;
	const vec4f hot = danger ? vec4f{ 0.78f, 0.28f, 0.28f, 1.f } : sty.color_widget_hovered;
	ctx.queue_sprite({
		.rect = rect,
		.color = hovered ? hot : base,
		.texture = ctx.blank_texture,
		.corner_radius = sty.corner_radius,
	});
	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = std::string(label),
		.position = { rect.center().x() - ctx.fonts.text->width(label, sty.font_size) * 0.5f, rect.center().y() + ctx.fonts.text->vertical_center_offset(sty.font_size) },
		.scale = sty.font_size,
		.color = sty.color_text,
		.clip_rect = rect,
	});
	return clicked;
}

auto gse::ide::terminal::draw_close_confirm(gui::builder& ui, data& d, const rectf& body) -> void {
	if (!d.pending_close || *d.pending_close >= d.instances.size()) {
		d.pending_close.reset();
		return;
	}

	const gui::draw_context& ctx = ui.ctx;
	const gui::style& sty = ctx.style;
	const float pad = sty.padding;
	const float fs = sty.font_size;
	const float line_h = ctx.fonts.text->line_height(fs) * 1.25f;
	const float btn_h = ctx.fonts.text->line_height(fs) + pad;

	const std::string title = "Kill running process?";
	const std::string message = std::format("\"{}\" is still running.", d.instances[*d.pending_close].name);

	const auto scope = ctx.scoped_layer(render_layer::modal);
	ctx.register_hit_region(render_layer::modal, body);
	ctx.queue_sprite({
		.rect = body,
		.color = { 0.f, 0.f, 0.f, 0.45f },
		.texture = ctx.blank_texture,
	});

	const float content_w = std::max({ ctx.fonts.text->width(title, fs), ctx.fonts.text->width(message, fs), 220.f });
	const float dialog_w = content_w + pad * 4.f;
	const float dialog_h = line_h * 2.f + btn_h + pad * 4.f;
	const vec2f center = body.center();
	const rectf dialog = rectf::from_position_size(
		{ center.x() - dialog_w * 0.5f, center.y() + dialog_h * 0.5f },
		{ dialog_w, dialog_h }
	);

	ctx.queue_sprite({
		.rect = rectf::from_position_size({ dialog.left() + 4.f, dialog.top() - 4.f }, { dialog_w, dialog_h }),
		.color = sty.color_shadow,
		.texture = ctx.blank_texture,
		.corner_radius = sty.corner_radius_menu,
	});
	ctx.queue_sprite({
		.rect = dialog,
		.color = { sty.color_menu_body.x(), sty.color_menu_body.y(), sty.color_menu_body.z(), 1.f },
		.texture = ctx.blank_texture,
		.corner_radius = sty.corner_radius_menu,
	});

	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = title,
		.position = { dialog.left() + pad * 2.f, dialog.top() - pad * 2.f - line_h * 0.5f + ctx.fonts.text->vertical_center_offset(fs) },
		.scale = fs,
		.color = sty.color_text,
		.clip_rect = dialog,
	});
	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = message,
		.position = { dialog.left() + pad * 2.f, dialog.top() - pad * 2.f - line_h * 1.5f + ctx.fonts.text->vertical_center_offset(fs) },
		.scale = fs,
		.color = sty.color_text_secondary,
		.clip_rect = dialog,
	});

	const float btn_w = (dialog_w - pad * 3.f) * 0.5f;
	const rectf cancel_btn = rectf::from_position_size({ dialog.left() + pad, dialog.bottom() + pad + btn_h }, { btn_w, btn_h });
	const rectf kill_btn = rectf::from_position_size({ cancel_btn.right() + pad, dialog.bottom() + pad + btn_h }, { btn_w, btn_h });

	bool cancel = confirm_button(ui, cancel_btn, "Cancel", false);
	const bool kill = confirm_button(ui, kill_btn, "Kill", true);
	if (ctx.key_pressed_for(key::escape)) {
		ctx.consume_key_press(key::escape);
		cancel = true;
	}

	if (kill) {
		instance& inst = d.instances[*d.pending_close];
		if (inst.runner) {
			spawn::terminate_process(*inst.runner);
		}
		d.instances.erase(d.instances.begin() + static_cast<std::ptrdiff_t>(*d.pending_close));
		d.pending_close.reset();
		if (d.active >= d.instances.size()) {
			d.active = d.instances.empty() ? 0 : d.instances.size() - 1;
		}
	}
	else if (cancel) {
		d.pending_close.reset();
	}
}

auto gse::ide::terminal::draw_panel(gui::builder& ui, data& d, channel_writer channels, const bool building) -> void {
	const gui::draw_context& ctx = ui.ctx;
	if (!d.sink || ctx.clip_stack.empty()) {
		return;
	}

	if (d.instances.empty()) {
		d.instances.push_back({ .name = "Log", .follows_log = true });
	}

	const gui::style& sty = ctx.style;
	const rectf body = ctx.clip_stack.back();

	const vec2f mouse = ctx.input.mouse_position();
	const bool pressed = ctx.input.mouse_button_pressed(mouse_button::button_1) && ctx.input_available();
	const bool held = ctx.input.mouse_button_held(mouse_button::button_1);

	const float divider_thickness = std::max(6.f, sty.resize_border_thickness) * 2.f;
	const gui::layout::split_params split{
		.container = body,
		.axis = gui::layout::split_axis::columns,
		.ratio = body.width() > 0.f ? std::clamp((body.width() - d.strip_width) / body.width(), 0.f, 1.f) : 0.8f,
		.min_first = 200.f,
		.min_second = 100.f,
		.divider_thickness = divider_thickness,
	};
	const bool resize_blocked = ctx.hit_regions && ctx.hit_regions->is_resize_blocked(mouse);
	const gui::layout::split_result panels = gui::layout::update_split(
		split,
		{ .mouse = mouse, .pressed = pressed, .held = held, .blocked = resize_blocked },
		d.resizing_strip
	);
	const rectf content = panels.first;
	const rectf strip = panels.second;
	d.strip_width = strip.width();

	if ((panels.divider.contains(mouse) && !resize_blocked) || d.resizing_strip) {
		channels.push<set_cursor_shape_request>({ .shape = cursor_shape::resize_ew });
	}

	gui::draw::panel_backdrop(ctx, {
		.rect = strip,
		.background = sty.color_panel_alt,
	});

	std::vector<gui::tab_desc> tab_descs;
	tab_descs.reserve(d.instances.size());
	for (std::size_t i = 0; i < d.instances.size(); ++i) {
		const instance& inst = d.instances[i];
		tab_descs.push_back({
			.id = i + 1,
			.caption = inst.name,
			.busy = inst.runner && inst.runner->running.load(std::memory_order_acquire),
			.closeable = d.instances.size() > 1,
		});
	}

	const gui::tab_strip_result tabs = gui::tab_strip(ctx, {
		.area = strip,
		.tabs = tab_descs,
		.active = d.active + 1,
		.orientation = gui::tab_orientation::vertical,
		.show_add = true,
	}, d.tab_strip);

	std::size_t close_requested = std::numeric_limits<std::size_t>::max();
	if (tabs.activated != 0) {
		d.active = tabs.activated - 1;
	}
	if (tabs.close_requested != 0) {
		close_requested = tabs.close_requested - 1;
	}
	if (tabs.add_requested) {
		d.instances.push_back({
			.name = std::format("Terminal {}", d.next_number++),
			.cursor = d.sink->sequence(),
		});
		d.active = d.instances.size() - 1;
	}

	if (close_requested != std::numeric_limits<std::size_t>::max() && !d.pending_close) {
		instance& closing = d.instances[close_requested];
		if (closing.runner && closing.runner->running.load(std::memory_order_acquire)) {
			d.pending_close = close_requested;
		}
		else {
			d.instances.erase(d.instances.begin() + static_cast<std::ptrdiff_t>(close_requested));
		}
	}

	if (d.active >= d.instances.size()) {
		d.active = d.instances.empty() ? 0 : d.instances.size() - 1;
	}

	draw_instance(ui, d, d.instances[d.active], content, channels, building);

	draw_close_confirm(ui, d, body);
}
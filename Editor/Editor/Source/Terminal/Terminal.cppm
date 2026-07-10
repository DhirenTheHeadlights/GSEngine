export module gse.ide.terminal:terminal_panel;

import std;
import gse;
import gse.ide.build;
import gse.win32;

export namespace gse::ide::terminal {
	constexpr std::string_view panel_name = "Terminal";

	using command_runner = gse::ide::build_runner::output_stream;

	struct line {
		std::uint64_t seq;
		gse::log::level lvl;
		std::string text;
	};

	class ring_sink : public gse::log::sink {
	public:
		auto write(const gse::log::record& rec) -> void override;

		auto write_raw(std::string_view text) -> void override;

		auto flush() -> void override;

		auto drain(std::uint64_t& cursor, std::vector<line>& out) -> void;

		auto sequence() -> std::uint64_t;

	private:
		auto push(gse::log::level lvl, std::string text) -> void;

		std::mutex m_mutex;
		std::deque<line> m_lines;
		std::uint64_t m_next = 0;
	};

	struct instance {
		std::string name;
		bool follows_log = false;
		std::uint64_t cursor = 0;
		gse::gui::text_buffer buffer;
		std::vector<gse::log::level> line_levels;
		std::vector<gse::gui::text_span> spans;
		gse::gui::text_area_state view;
		std::string input;
		gse::gui::text_input_state input_state;
		std::shared_ptr<command_runner> runner;
		bool interactive = true;
	};

	struct [[= gse::system_state<"Terminal">{}]] data {
		ring_sink* sink = nullptr;
		std::vector<instance> instances;
		std::size_t active = 0;
		std::uint32_t next_number = 1;
		float tab_scroll_y = 0.f;
		std::size_t tab_scroll_active = static_cast<std::size_t>(-1);
	};

	[[= gse::system_init{}]]
	auto init(data& d) -> gse::async::task<>;

	[[= gse::system_run<>{}]]
	auto run(gse::context& ctx, data& d) -> gse::async::task<>;

	auto register_sink(data& d) -> void;

	auto draw_panel(gse::gui::builder& ui, data& d) -> void;
}

namespace gse::ide::terminal {
	using ui_rect = gse::rect_t<gse::vec2f>;

	constexpr std::size_t max_lines = 8192;
	constexpr float tab_strip_width = 140.f;

	auto level_color(const gse::gui::style& sty, gse::log::level lvl) -> gse::vec4f;

	auto ellipsize_path(std::string_view path) -> std::string;

	auto header_button(gse::gui::builder& ui, const ui_rect& rect, std::span<const gse::gui::symbol::stroke> glyph, std::string_view label, bool enabled) -> bool;

	auto widen(const std::string& s) -> std::wstring;

	auto run_command(command_runner& runner, const std::string& command, const std::wstring& cwd) -> void;

	auto draw_instance(gse::gui::builder& ui, data& d, instance& inst, const ui_rect& area) -> void;
}

auto gse::ide::terminal::level_color(const gse::gui::style& sty, const gse::log::level lvl) -> gse::vec4f {
	switch (lvl) {
		case gse::log::level::debug:
			return sty.color_text_disabled;
		case gse::log::level::info:
			return sty.color_text;
		case gse::log::level::warning:
			return gse::vec4f{ 0.71f, 0.57f, 0.11f, 1.f };
		case gse::log::level::error:
			return gse::vec4f{ 0.855f, 0.451f, 0.424f, 1.f };
		case gse::log::level::fatal:
			return gse::vec4f{ 0.94f, 0.30f, 0.30f, 1.f };
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

auto gse::ide::terminal::ring_sink::push(const gse::log::level lvl, std::string text) -> void {
	std::lock_guard lock(m_mutex);
	m_lines.push_back({ .seq = m_next++, .lvl = lvl, .text = std::move(text) });
	while (m_lines.size() > max_lines) {
		m_lines.pop_front();
	}
}

auto gse::ide::terminal::ring_sink::write(const gse::log::record& rec) -> void {
	push(rec.lvl, std::format("[{}] {}{}", rec.cat, rec.prefix, rec.message));
}

auto gse::ide::terminal::ring_sink::write_raw(const std::string_view text) -> void {
	push(gse::log::level::info, std::string(text));
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

auto gse::ide::terminal::init(data& d) -> gse::async::task<> {
	register_sink(d);
	return {};
}

auto gse::ide::terminal::run(gse::context& ctx, data& d) -> gse::async::task<> {
	for (auto& [name, stream] : gse::ide::build_runner::take_new_streams()) {
		d.instances.push_back({
			.name = name,
			.cursor = d.sink ? d.sink->sequence() : 0,
			.runner = std::move(stream),
			.interactive = false,
		});
		d.active = d.instances.size() - 1;
	}
	ctx.channels.push<gse::gui::menu_content>({
		.menu = std::string(panel_name),
		.layer = gse::render_layer::content,
		.build = [d = &d](gse::gui::builder& b) {
			draw_panel(b, *d);
		},
	});
	return {};
}

auto gse::ide::terminal::register_sink(data& d) -> void {
	auto s = std::make_unique<ring_sink>();
	d.sink = s.get();
	gse::log::add_sink(std::move(s));
	d.instances.push_back({ .name = "Log", .follows_log = true });
}

auto gse::ide::terminal::widen(const std::string& s) -> std::wstring {
	if (s.empty()) {
		return {};
	}
	const int length = gse::win32::MultiByteToWideChar(gse::win32::cp_utf8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
	std::wstring out(static_cast<std::size_t>(length), L'\0');
	gse::win32::MultiByteToWideChar(gse::win32::cp_utf8, 0, s.data(), static_cast<int>(s.size()), out.data(), length);
	return out;
}

auto gse::ide::terminal::run_command(command_runner& runner, const std::string& command, const std::wstring& cwd) -> void {
	gse::win32::SECURITY_ATTRIBUTES attributes{
		.nLength = sizeof(gse::win32::SECURITY_ATTRIBUTES),
		.bInheritHandle = 1,
	};

	gse::win32::HANDLE read_end = nullptr;
	gse::win32::HANDLE write_end = nullptr;
	if (!gse::win32::CreatePipe(&read_end, &write_end, &attributes, 0)) {
		return;
	}
	gse::win32::SetHandleInformation(read_end, gse::win32::handle_flag_inherit, 0);

	const std::wstring wide_command = L"cmd.exe /c " + widen(command);
	std::vector<wchar_t> command_buffer(wide_command.begin(), wide_command.end());
	command_buffer.push_back(0);

	gse::win32::STARTUPINFOW startup{
		.cb = sizeof(gse::win32::STARTUPINFOW),
		.dwFlags = gse::win32::startf_use_std_handles,
		.hStdOutput = write_end,
		.hStdError = write_end,
	};

	gse::win32::PROCESS_INFORMATION process{};
	const int spawned = gse::win32::CreateProcessW(nullptr, command_buffer.data(), nullptr, nullptr, 1, gse::win32::create_no_window, nullptr, cwd.empty() ? nullptr : cwd.c_str(), &startup, &process);

	gse::win32::CloseHandle(write_end);

	if (!spawned) {
		std::lock_guard lock(runner.mutex);
		runner.lines.push_back("failed to launch: " + command);
		gse::win32::CloseHandle(read_end);
		return;
	}

	std::string pending;
	std::array<char, 4096> chunk{};
	gse::win32::DWORD received = 0;
	while (gse::win32::ReadFile(read_end, chunk.data(), static_cast<gse::win32::DWORD>(chunk.size()), &received, nullptr) && received > 0) {
		pending.append(chunk.data(), received);
		for (std::size_t newline = pending.find('\n'); newline != std::string::npos; newline = pending.find('\n')) {
			std::string text = pending.substr(0, newline);
			if (!text.empty() && text.back() == '\r') {
				text.pop_back();
			}
			std::lock_guard lock(runner.mutex);
			runner.lines.push_back(std::move(text));
		}
	}
	if (!pending.empty()) {
		std::lock_guard lock(runner.mutex);
		runner.lines.push_back(std::move(pending));
	}

	gse::win32::CloseHandle(read_end);
	gse::win32::WaitForSingleObject(process.hProcess, gse::win32::infinite);
	gse::win32::CloseHandle(process.hProcess);
	gse::win32::CloseHandle(process.hThread);
}

auto gse::ide::terminal::header_button(gse::gui::builder& ui, const ui_rect& rect, const std::span<const gse::gui::symbol::stroke> glyph, const std::string_view label, const bool enabled) -> bool {
	const gse::gui::draw_context& ctx = ui.ctx;
	const gse::gui::style& sty = ctx.style;
	const float pad = sty.padding;

	const gse::vec2f mouse = ctx.input.mouse_position();
	const bool hovered = enabled && rect.contains(mouse) && ctx.input_available();
	const bool clicked = hovered && ctx.input.mouse_button_pressed(gse::mouse_button::button_1);

	ctx.queue_sprite({
		.rect = rect,
		.color = hovered ? sty.color_widget_hovered : sty.color_input_background,
		.texture = ctx.blank_texture,
	});

	const gse::vec4f fg = enabled ? sty.color_text : sty.color_text_secondary;
	const float glyph_size = rect.height();
	const float glyph_left = label.empty() ? rect.center().x() - glyph_size * 0.5f : rect.left() + pad * 0.5f;
	const ui_rect glyph_rect = ui_rect::from_position_size(
		{ glyph_left, rect.top() },
		{ glyph_size, glyph_size }
	);
	gse::gui::symbol::draw(ctx, glyph, glyph_rect, {
		.color = fg,
		.scale = 0.55f,
		.clip_rect = rect,
	});
	if (!label.empty()) {
		ctx.queue_text({
			.font = ctx.font,
			.text = std::string(label),
			.position = { glyph_rect.right(), rect.center().y() + ctx.font->vertical_center_offset(sty.font_size) },
			.scale = sty.font_size,
			.color = fg,
			.clip_rect = rect,
		});
	}

	return clicked;
}

auto gse::ide::terminal::draw_instance(gse::gui::builder& ui, data& d, instance& inst, const ui_rect& area) -> void {
	const gse::gui::draw_context& ctx = ui.ctx;
	const gse::id input_id = gse::gui::ids::make("##term_input_" + inst.name);
	const std::string prompt = ellipsize_path(gse::config::root_dir.display_string()) + "> ";

	std::vector<line> fresh;
	if (inst.follows_log) {
		d.sink->drain(inst.cursor, fresh);
	}
	if (inst.runner) {
		std::lock_guard lock(inst.runner->mutex);
		for (std::string& l : inst.runner->lines) {
			fresh.push_back({ .seq = 0, .lvl = gse::log::level::info, .text = std::move(l) });
		}
		inst.runner->lines.clear();
	}

	const bool busy = inst.runner && inst.runner->running.load(std::memory_order_acquire);
	if (inst.interactive && ui.focus_widget_id == input_id && ctx.key_pressed_for(gse::key::enter) && !inst.input.empty() && !busy) {
		fresh.push_back({ .seq = 0, .lvl = gse::log::level::info, .text = prompt + inst.input });
		if (!inst.runner) {
			inst.runner = std::make_shared<command_runner>();
		}
		inst.runner->running.store(true, std::memory_order_release);
		std::thread([r = inst.runner, cmd = inst.input, cwd = gse::config::root_dir.wstring()] {
			run_command(*r, cmd, cwd);
			r->running.store(false, std::memory_order_release);
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
		inst.line_levels.push_back(gse::log::level::info);
	}

	const float pad = ctx.style.padding;
	const float input_h = inst.interactive ? ctx.font->line_height(ctx.style.font_size) + pad : 0.f;

	const ui_rect log_rect = ui_rect::from_position_size(
		{ area.left(), area.top() },
		{ area.width(), std::max(0.f, area.height() - input_h) }
	);

	gse::gui::draw::text_area_in_rect(
		ctx,
		gse::gui::ids::make("##term_log_" + inst.name),
		inst.buffer,
		inst.view,
		std::span<const gse::gui::text_span>(inst.spans),
		{},
		log_rect,
		true,
		false,
		4,
		false,
		false,
		gse::time{},
		ui.hot_widget_id,
		ui.focus_widget_id
	);

	if (!inst.interactive) {
		return;
	}

	const ui_rect input_rect = ui_rect::from_position_size(
		{ area.left(), area.bottom() + input_h },
		{ area.width(), input_h }
	);

	ctx.queue_sprite({
		.rect = input_rect,
		.color = ctx.style.color_input_background,
		.texture = ctx.blank_texture,
	});

	const float prompt_width = ctx.font->width(prompt, ctx.style.font_size) + pad;
	ctx.queue_text({
		.font = ctx.font,
		.text = prompt,
		.position = { input_rect.left() + pad, input_rect.center().y() + ctx.font->vertical_center_offset(ctx.style.font_size) },
		.scale = ctx.style.font_size,
		.color = ctx.style.color_accent,
		.clip_rect = input_rect,
	});

	const bool building = build_runner::in_progress();

	const ui_rect run_btn = ui_rect::from_position_size(
		{ input_rect.right() - input_h - pad, input_rect.top() },
		{ input_h, input_h }
	);
	if (header_button(ui, run_btn, gse::gui::symbol::play(), "", !building)) {
		build_runner::start_build_and_run_game();
	}

	const std::string build_label = building ? "Building..." : "Build Game";
	const float build_w = ctx.font->width(build_label, ctx.style.font_size) + input_h + pad * 1.5f;
	const ui_rect build_btn = ui_rect::from_position_size(
		{ run_btn.left() - build_w, input_rect.top() },
		{ build_w, input_h }
	);
	if (header_button(ui, build_btn, gse::gui::symbol::hammer(), build_label, !building)) {
		build_runner::start_build_game();
	}

	const ui_rect input_box = ui_rect::from_position_size(
		{ input_rect.left() + prompt_width, input_rect.top() },
		{ std::max(0.f, build_btn.left() - pad - input_rect.left() - prompt_width), input_h }
	);

	gse::gui::draw::text_input_in_rect(
		ctx,
		input_id,
		inst.input,
		inst.input_state,
		input_box,
		ui.hot_widget_id,
		ui.focus_widget_id
	);

	ctx.queue_sprite({
		.rect = ui_rect::from_position_size({ input_rect.left(), input_rect.top() }, { input_rect.width(), ctx.style.accent_bar_width }),
		.color = ctx.style.color_accent,
		.texture = ctx.blank_texture,
	});

	if (ctx.input.mouse_button_pressed(gse::mouse_button::button_1) && ctx.input_available() && input_rect.contains(ctx.input.mouse_position())) {
		ui.focus_widget_id = input_id;
	}
}

auto gse::ide::terminal::draw_panel(gse::gui::builder& ui, data& d) -> void {
	const gse::gui::draw_context& ctx = ui.ctx;
	if (!d.sink || ctx.clip_stack.empty()) {
		return;
	}

	if (d.instances.empty()) {
		d.instances.push_back({ .name = "Log", .follows_log = true });
	}

	const gse::gui::style& sty = ctx.style;
	const ui_rect body = ctx.clip_stack.back();
	const float pad = sty.padding;
	const float tab_h = ctx.font->line_height(sty.font_size) + pad;

	const ui_rect strip = ui_rect::from_position_size(
		{ body.right() - tab_strip_width, body.top() },
		{ tab_strip_width, body.height() }
	);
	const ui_rect content = ui_rect::from_position_size(
		{ body.left(), body.top() },
		{ std::max(0.f, body.width() - tab_strip_width), body.height() }
	);

	ctx.queue_sprite({
		.rect = strip,
		.color = sty.color_panel_alt,
		.texture = ctx.blank_texture,
	});

	const gse::vec2f mouse = ctx.input.mouse_position();
	const bool clicked = ctx.input.mouse_button_pressed(gse::mouse_button::button_1) && ctx.input_available();
	std::size_t close_requested = std::numeric_limits<std::size_t>::max();

	const float tab_content_height = tab_h * static_cast<float>(d.instances.size() + 1);
	const float max_tab_scroll = std::max(0.f, tab_content_height - strip.height());
	if (strip.contains(mouse) && !ctx.is_scroll_consumed()) {
		const gse::vec2f wheel = ctx.input.scroll_delta();
		if (std::abs(wheel.y()) > 0.001f) {
			d.tab_scroll_y = std::clamp(d.tab_scroll_y - wheel.y() * tab_h, 0.f, max_tab_scroll);
			ctx.consume_scroll();
		}
	}
	d.tab_scroll_y = std::clamp(d.tab_scroll_y, 0.f, max_tab_scroll);

	if (d.tab_scroll_active != d.active) {
		const float active_top_target = static_cast<float>(d.active) * tab_h;
		if (active_top_target < d.tab_scroll_y) {
			d.tab_scroll_y = active_top_target;
		}
		else if (active_top_target + tab_h > d.tab_scroll_y + strip.height()) {
			d.tab_scroll_y = active_top_target + tab_h - strip.height();
		}
		d.tab_scroll_active = d.active;
	}
	d.tab_scroll_y = std::clamp(d.tab_scroll_y, 0.f, max_tab_scroll);

	float active_top = strip.top();
	float y = strip.top() + d.tab_scroll_y;
	for (std::size_t i = 0; i < d.instances.size(); ++i) {
		const ui_rect tab = ui_rect::from_position_size({ strip.left(), y }, { tab_strip_width, tab_h });
		const ui_rect visible_tab = tab.intersection(strip);
		if (visible_tab.height() <= 0.f) {
			y -= tab_h;
			continue;
		}
		const bool is_active = i == d.active;
		const bool hovered = visible_tab.contains(mouse) && ctx.input_available();
		const bool closable = d.instances.size() > 1;
		const ui_rect close_rect = ui_rect::from_position_size({ tab.right() - tab_h, tab.top() }, { tab_h, tab_h });

		ctx.queue_sprite({
			.rect = visible_tab,
			.color = is_active ? sty.color_widget_background : (hovered ? sty.color_widget_hovered : sty.color_input_background),
			.texture = ctx.blank_texture,
		});
		if (is_active) {
			active_top = tab.top();
		}
		ctx.queue_text({
			.font = ctx.font,
			.text = d.instances[i].name,
			.position = { tab.left() + pad, tab.center().y() + ctx.font->vertical_center_offset(sty.font_size) },
			.scale = sty.font_size,
			.color = is_active ? sty.color_text : sty.color_text_secondary,
			.clip_rect = visible_tab,
		});
		if (closable) {
			const bool close_hovered = close_rect.contains(mouse) && strip.contains(mouse) && ctx.input_available();
			gse::gui::symbol::draw(ctx, gse::gui::symbol::close(), close_rect, {
				.color = close_hovered ? sty.color_text : sty.color_text_secondary,
				.scale = 0.4f,
				.clip_rect = visible_tab,
			});
		}

		if (clicked && visible_tab.contains(mouse)) {
			if (closable && close_rect.contains(mouse)) {
				close_requested = i;
			}
			else {
				d.active = i;
			}
		}

		y -= tab_h;
	}

	const ui_rect plus = ui_rect::from_position_size({ strip.left(), y }, { tab_strip_width, tab_h });
	const ui_rect visible_plus = plus.intersection(strip);
	const bool plus_hovered = visible_plus.contains(mouse) && ctx.input_available();
	ctx.queue_sprite({
		.rect = visible_plus,
		.color = plus_hovered ? sty.color_widget_hovered : sty.color_input_background,
		.texture = ctx.blank_texture,
	});
	const std::string plus_label = "+";
	ctx.queue_text({
		.font = ctx.font,
		.text = plus_label,
		.position = { plus.center().x() - ctx.font->width(plus_label, sty.font_size) * 0.5f, plus.center().y() + ctx.font->vertical_center_offset(sty.font_size) },
		.scale = sty.font_size,
		.color = plus_hovered ? sty.color_text : sty.color_text_secondary,
		.clip_rect = visible_plus,
	});
	if (clicked && visible_plus.contains(mouse)) {
		d.instances.push_back({
			.name = std::format("Terminal {}", d.next_number++),
			.cursor = d.sink->sequence(),
		});
		d.active = d.instances.size() - 1;
	}

	ctx.queue_sprite({
		.rect = ui_rect::from_position_size({ strip.left(), active_top }, { sty.accent_bar_width, std::max(0.f, active_top - strip.bottom()) }),
		.color = sty.color_accent,
		.texture = ctx.blank_texture,
	});

	if (close_requested != std::numeric_limits<std::size_t>::max()) {
		d.instances.erase(d.instances.begin() + static_cast<std::ptrdiff_t>(close_requested));
	}

	if (d.active >= d.instances.size()) {
		d.active = d.instances.empty() ? 0 : d.instances.size() - 1;
	}

	draw_instance(ui, d, d.instances[d.active], content);
}

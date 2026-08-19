module gse.ide.terminal;

import std;
import gse;
import gse.ide.build;
import gse.ide.config;
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

	auto path_link_at(
		std::string_view row,
		std::uint32_t column
	) -> std::optional<link_hit>;

	auto level_color(
		const gui::style& sty,
		log::level lvl
	) -> vec4f;

	auto draw_close_confirm(
		gui::builder& ui,
		data& d,
		const rectf& body
	) -> void;

	auto widen(
		const std::string& s
	) -> std::wstring;

	auto run_command(
		command_runner& runner,
		const std::string& command,
		const std::wstring& cwd
	) -> void;

	auto make_instance(
		data& d,
		std::string name
	) -> instance;

	auto find_instance(
		data& d,
		id instance_id
	) -> instance*;

	auto erase_instance(
		data& d,
		id instance_id
	) -> void;

	auto close_slot(
		data& d,
		build_runner::stream_slot slot
	) -> void;

	auto append_lines(
		instance& inst,
		std::span<const line> lines,
		const gui::style& style
	) -> void;

	auto draw_instance(
		gui::builder& ui,
		const input::state& input,
		data& d,
		instance& inst,
		const rectf& area,
		channel_write<agent::start_request, build_runner::build_request, gui::menu_content, jump_to_request, set_cursor_shape_request> channels,
		bool building
	) -> void;
}

auto gse::ide::terminal::level_color(const gui::style& sty, const log::level lvl) -> vec4f {
	switch (lvl) {
		case log::level::debug:
			return sty.color_text_disabled;
		case log::level::info:
			return sty.color_text;
		case log::level::warning:
			return sty.color_warning;
		case log::level::error:
			return sty.color_error;
		case log::level::fatal:
			return sty.color_fatal;
	}
	return sty.color_text;
}

auto gse::ide::terminal::make_instance(data& d, std::string name) -> instance {
	const id instance_id = generate_temp_id(hash_combine(stable_id("terminal_instance"), d.next_id++));
	return {
		.instance_id = instance_id,
		.input_id = generate_temp_id(hash_combine(instance_id.number(), stable_id("input"))),
		.log_id = generate_temp_id(hash_combine(instance_id.number(), stable_id("log"))),
		.name = std::move(name),
	};
}

auto gse::ide::terminal::find_instance(data& d, const id instance_id) -> instance* {
	const auto found = std::ranges::find(d.instances, instance_id, &instance::instance_id);
	return found == d.instances.end() ? nullptr : &*found;
}

auto gse::ide::terminal::erase_instance(data& d, const id instance_id) -> void {
	const auto found = std::ranges::find(d.instances, instance_id, &instance::instance_id);
	if (found == d.instances.end()) {
		return;
	}
	const std::size_t index = static_cast<std::size_t>(std::distance(d.instances.begin(), found));
	const bool was_active = d.active == instance_id;
	d.instances.erase(found);
	if (d.instances.empty()) {
		d.active.reset();
	}
	else if (was_active) {
		d.active = d.instances[std::min(index, d.instances.size() - 1)].instance_id;
	}
}

auto gse::ide::terminal::close_slot(data& d, const build_runner::stream_slot slot) -> void {
	if (slot == build_runner::stream_slot::none) {
		return;
	}
	const auto found = std::ranges::find(d.instances, slot, &instance::slot);
	if (found == d.instances.end()) {
		return;
	}
	if (found->runner && found->runner->running.load(std::memory_order_acquire)) {
		spawn::terminate_process(*found->runner);
	}
	erase_instance(d, found->instance_id);
}

auto gse::ide::terminal::append_lines(instance& inst, const std::span<const line> lines, const gui::style& style) -> void {
	for (const line& l : lines) {
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

	if (inst.buffer.lines.size() > max_lines) {
		const std::size_t overflow = inst.buffer.lines.size() - max_lines;
		inst.buffer.lines.erase(inst.buffer.lines.begin(), inst.buffer.lines.begin() + static_cast<std::ptrdiff_t>(overflow));
		inst.line_levels.erase(inst.line_levels.begin(), inst.line_levels.begin() + static_cast<std::ptrdiff_t>(overflow));
	}

	assert(inst.buffer.lines.size() == inst.line_levels.size(), "terminal buffer line metadata diverged");
	inst.spans.clear();
	inst.spans.reserve(inst.buffer.lines.size());
	for (std::size_t i = 0; i < inst.buffer.lines.size(); ++i) {
		inst.spans.push_back({
			.line = static_cast<std::uint32_t>(i),
			.start_col = 0,
			.end_col = static_cast<std::uint32_t>(inst.buffer.lines[i].size()),
			.color = level_color(style, inst.line_levels[i]),
		});
	}
	inst.view.scroll.y.target = std::numeric_limits<float>::max();
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
	std::error_code ec;
	std::filesystem::path resolved;
	if (candidate.is_absolute()) {
		resolved = candidate;
	}
	else {
		for (const ide::config::worktree& tree : ide::config::worktrees()) {
			std::filesystem::path attempt = tree.project_root / candidate;
			if (std::filesystem::is_regular_file(attempt, ec)) {
				resolved = std::move(attempt);
				break;
			}
		}
	}

	if (resolved.empty() || !std::filesystem::is_regular_file(resolved, ec)) {
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

gse::ide::terminal::ring_sink::~ring_sink() = default;

auto gse::ide::terminal::ring_sink::push(const log::level lvl, std::string text) -> void {
	std::lock_guard lock(m_mutex);
	m_lines.push_back({
		.seq = m_next++,
		.lvl = lvl,
		.text = std::move(text),
	});
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

auto gse::ide::terminal::ring_sink::flush() -> void {}

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
	d.prompt = ellipsize_path(ide::config::project_root().generic_display_string()) + "> ";
	register_sink(d);
	return {};
}

auto gse::ide::terminal::run(context& ctx, data& d, const channel_read<build_runner::stream_opened> stream_in, const channel_write<agent::start_request, build_runner::build_request, gui::menu_content, jump_to_request, set_cursor_shape_request> ui_out, const shared_view<input::data> input_d, const shared_view<build_runner::data> build_d) -> async::task<> {
	for (const build_runner::stream_opened& opened : stream_in.of<build_runner::stream_opened>()) {
		close_slot(d, opened.slot);
		instance inst = make_instance(d, opened.name);
		inst.cursor = d.sink ? d.sink->sequence() : 0;
		inst.runner = opened.stream;
		inst.interactive = false;
		inst.slot = opened.slot;
		d.active = inst.instance_id;
		d.instances.push_back(std::move(inst));
	}
	const bool building = build_d.building;
	const input::state input_snapshot = input::current_state(input_d);
	ui_out.push<gui::menu_content>({
		.menu = std::string(panel_name),
		.layer = render_layer::content,
		.build = [d = &d, channels = ui_out, input_snapshot, building](gui::builder& b) {
			draw_panel(b, input_snapshot, *d, channels, building);
		},
	});
	return {};
}

auto gse::ide::terminal::shutdown(data& d) -> void {
	for (instance& inst : d.instances) {
		if (inst.interactive && inst.runner && inst.runner->running.load(std::memory_order_acquire)) {
			spawn::terminate_process(*inst.runner);
		}
	}
	d.instances.clear();
}

auto gse::ide::terminal::register_sink(data& d) -> void {
	auto s = std::make_unique<ring_sink>();
	d.sink = s.get();
	log::add_sink(std::move(s));
	instance log_instance = make_instance(d, "Log");
	log_instance.follows_log = true;
	d.active = log_instance.instance_id;
	d.instances.push_back(std::move(log_instance));
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

auto gse::ide::terminal::draw_instance(gui::builder& ui, const input::state& input, data& d, instance& inst, const rectf& area, channel_write<agent::start_request, build_runner::build_request, gui::menu_content, jump_to_request, set_cursor_shape_request> channels, const bool building) -> void {
	const gui::draw_context& ctx = ui.ctx;
	const auto text_view = ctx.fonts.text.resolve();
	const auto code_view = ctx.fonts.code.resolve();

	d.fresh.clear();
	if (inst.follows_log) {
		d.sink->drain(inst.cursor, d.fresh);
	}
	if (inst.runner) {
		std::lock_guard lock(inst.runner->mutex);
		for (std::string& l : inst.runner->lines) {
			d.fresh.push_back({
				.seq = 0,
				.lvl = log::level::info,
				.text = std::move(l),
			});
		}
		inst.runner->lines.clear();
	}

	const bool busy = inst.runner && inst.runner->running.load(std::memory_order_acquire);
	if (inst.interactive && ui.focus_widget_id == inst.input_id && ctx.key_pressed_for(key::enter) && !inst.input.empty() && !busy) {
		d.fresh.push_back({
			.seq = 0,
			.lvl = log::level::info,
			.text = d.prompt + inst.input,
		});
		constexpr std::string_view agent_prefix = "agent ";
		if (std::string_view(inst.input).starts_with(agent_prefix)) {
			channels.push<agent::start_request>({
				.prompt = inst.input.substr(agent_prefix.size()),
				.cwd = ide::config::project_root(),
			});
		}
		else {
			if (!inst.runner) {
				inst.runner = std::make_shared<command_runner>();
			}
			inst.runner->terminated.store(false, std::memory_order_release);
			inst.runner->running.store(true, std::memory_order_release);
			inst.worker = std::jthread([r = inst.runner, cmd = inst.input, cwd = ide::config::project_root().wstring()] {
				run_command(*r, cmd, cwd);
			});
		}

		inst.input.clear();
		inst.input_state = {};
	}

	if (!d.fresh.empty()) {
		append_lines(inst, d.fresh, ctx.style);
	}

	if (inst.buffer.lines.empty()) {
		inst.buffer.lines.emplace_back();
		inst.line_levels.push_back(log::level::info);
	}

	const float pad = ctx.style.padding;
	const float input_h = inst.interactive ? code_view->line_height(ctx.style.font_size) + pad : 0.f;

	const rectf log_rect = rectf::from_position_size(
		{ area.left(), area.top() },
		{ area.width(), std::max(0.f, area.height() - input_h) }
	);

	d.underlines.clear();
	std::optional<link_hit> link;
	const vec2f mouse = input.mouse_position();
	const bool goto_ctrl = input.key_held(key::left_control) || input.key_held(key::right_control);
	if (goto_ctrl && ctx.hovers(log_rect) && !inst.buffer.lines.empty()) {
		const gui::buffer_position hover = gui::draw::text_area_position_at(ctx, inst.buffer, inst.view, log_rect, false, 4, mouse);
		if (const std::optional<link_hit> hit = path_link_at(inst.buffer.line(hover.line), hover.column)) {
			link = hit;
			d.underlines.push_back({
				.line = hover.line,
				.start_col = hit->start_col,
				.end_col = hit->end_col,
				.color = ctx.style.color_text_secondary,
			});
		}
	}
	if (link) {
		channels.push<set_cursor_shape_request>({
			.shape = cursor_shape::hand,
		});
	}
	const bool goto_click = link.has_value() && ctx.mouse_pressed_for(log_rect);

	gui::draw::text_area_in_rect(
		ctx,
		inst.log_id,
		{
			.buffer = inst.buffer,
			.state = inst.view,
			.spans = inst.spans,
			.underlines = d.underlines,
			.rect = log_rect,
			.read_only = true,
			.blink_interval = time{},
		},
		ui.hot_widget_id,
		ui.focus_widget_id
	);

	if (goto_click && link) {
		channels.push<jump_to_request>({
			.path = link->path,
			.line = link->line,
			.column = link->column,
		});
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

	const float prompt_width = code_view->width(d.prompt, ctx.style.font_size) + pad;
	ctx.queue_text({
		.font = ctx.fonts.code,
		.text = d.prompt,
		.position = { input_rect.left() + pad, input_rect.center().y() + code_view->vertical_center_offset(ctx.style.font_size) },
		.scale = ctx.style.font_size,
		.color = ctx.style.color_accent,
		.clip_rect = input_rect,
	});

	const rectf run_btn = rectf::from_position_size(
		{ input_rect.right() - input_h - pad, input_rect.top() },
		{ input_h, input_h }
	);
	if (gui::draw::button_in_rect(ctx, {
		.rect = run_btn,
		.glyph = gui::symbol::play(),
		.key = "##terminal_run",
		.enabled = !building,
	}, ui.hot_widget_id, ui.active_widget_id)) {
		channels.push<build_runner::build_request>({
			.target = build_runner::build_target::game,
			.run_after = true,
		});
	}

	const std::string_view build_label = building ? "Building..." : "Build Game";
	const float build_w = text_view->width(build_label, ctx.style.font_size) + input_h + pad * 1.5f;
	const rectf build_btn = rectf::from_position_size(
		{ run_btn.left() - build_w, input_rect.top() },
		{ build_w, input_h }
	);
	if (gui::draw::button_in_rect(ctx, {
		.rect = build_btn,
		.label = build_label,
		.glyph = gui::symbol::hammer(),
		.key = "##terminal_build",
		.enabled = !building,
	}, ui.hot_widget_id, ui.active_widget_id)) {
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
		inst.input_id,
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

	if (ctx.mouse_pressed_for(input_rect)) {
		ui.focus_widget_id = inst.input_id;
	}
}

auto gse::ide::terminal::draw_close_confirm(gui::builder& ui, data& d, const rectf& body) -> void {
	instance* closing = d.pending_close ? find_instance(d, *d.pending_close) : nullptr;
	if (!closing) {
		d.pending_close.reset();
		return;
	}
	const auto text_view = ui.ctx.fonts.text.resolve();

	const gui::draw_context& ctx = ui.ctx;
	const gui::style& sty = ctx.style;
	const float pad = sty.padding;
	const float fs = sty.font_size;
	const float line_h = text_view->line_height(fs) * 1.25f;
	const float btn_h = text_view->line_height(fs) + pad;

	constexpr std::string_view title = "Kill running process?";
	const std::string message = std::format("\"{}\" is still running.", closing->name);

	const auto scope = ctx.scoped_layer(render_layer::modal);
	ctx.register_hit_region(render_layer::modal, body);
	ctx.queue_sprite({
		.rect = body,
		.color = { 0.f, 0.f, 0.f, 0.45f },
		.texture = ctx.blank_texture,
	});

	const float content_w = std::max({ text_view->width(title, fs), text_view->width(message, fs), 220.f });
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
		.color = { vec3f(sty.color_menu_body), 1.f },
		.texture = ctx.blank_texture,
		.corner_radius = sty.corner_radius_menu,
	});

	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = title,
		.position = { dialog.left() + pad * 2.f, dialog.top() - pad * 2.f - line_h * 0.5f + text_view->vertical_center_offset(fs) },
		.scale = fs,
		.color = sty.color_text,
		.clip_rect = dialog,
	});
	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = message,
		.position = { dialog.left() + pad * 2.f, dialog.top() - pad * 2.f - line_h * 1.5f + text_view->vertical_center_offset(fs) },
		.scale = fs,
		.color = sty.color_text_secondary,
		.clip_rect = dialog,
	});

	const float btn_w = (dialog_w - pad * 3.f) * 0.5f;
	const rectf cancel_btn = rectf::from_position_size({ dialog.left() + pad, dialog.bottom() + pad + btn_h }, { btn_w, btn_h });
	const rectf kill_btn = rectf::from_position_size({ cancel_btn.right() + pad, dialog.bottom() + pad + btn_h }, { btn_w, btn_h });

	bool cancel = gui::draw::button_in_rect(ctx, {
		.rect = cancel_btn,
		.label = "Cancel",
		.key = "##terminal_close_cancel",
	}, ui.hot_widget_id, ui.active_widget_id);
	const bool kill = gui::draw::button_in_rect(ctx, {
		.rect = kill_btn,
		.label = "Kill",
		.key = "##terminal_close_kill",
		.danger = true,
	}, ui.hot_widget_id, ui.active_widget_id);
	if (ctx.key_pressed_for(key::escape)) {
		ctx.consume_key_press(key::escape);
		cancel = true;
	}

	if (kill) {
		if (closing->runner) {
			spawn::terminate_process(*closing->runner);
		}
		erase_instance(d, closing->instance_id);
		d.pending_close.reset();
	}
	else if (cancel) {
		d.pending_close.reset();
	}
}

auto gse::ide::terminal::draw_panel(gui::builder& ui, const input::state& input, data& d, channel_write<agent::start_request, build_runner::build_request, gui::menu_content, jump_to_request, set_cursor_shape_request> channels, const bool building) -> void {
	const gui::draw_context& ctx = ui.ctx;
	if (!d.sink || ctx.clip_stack.empty()) {
		return;
	}

	if (d.instances.empty()) {
		instance log_instance = make_instance(d, "Log");
		log_instance.follows_log = true;
		d.active = log_instance.instance_id;
		d.instances.push_back(std::move(log_instance));
	}

	const gui::style& sty = ctx.style;
	const rectf body = ctx.clip_stack.back();

	const vec2f mouse = input.mouse_position();
	const bool pressed = input.mouse_button_pressed(mouse_button::button_1) && ctx.input_available();
	const bool held = input.mouse_button_held(mouse_button::button_1);

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
		{
			.mouse = mouse,
			.pressed = pressed,
			.held = held,
			.blocked = resize_blocked,
		},
		d.resizing_strip
	);
	const rectf content = panels.first;
	const rectf strip = panels.second;
	d.strip_width = strip.width();

	if ((panels.divider.contains(mouse) && !resize_blocked) || d.resizing_strip) {
		channels.push<set_cursor_shape_request>({
			.shape = cursor_shape::resize_ew,
		});
	}

	gui::draw::panel_backdrop(ctx, {
		.rect = strip,
		.background = sty.color_panel_alt,
	});

	d.tab_descs.clear();
	d.tab_descs.reserve(d.instances.size());
	for (const instance& inst : d.instances) {
		d.tab_descs.push_back({
			.tab_id = inst.instance_id,
			.caption = inst.name,
			.busy = inst.runner && inst.runner->running.load(std::memory_order_acquire),
			.closeable = !inst.follows_log,
		});
	}

	const gui::tab_strip_result tabs = gui::tab_strip(ctx, {
		.area = strip,
		.tabs = d.tab_descs,
		.active = d.active,
		.orientation = gui::tab_orientation::vertical,
		.show_add = true,
	}, d.tab_strip);

	if (tabs.activated.exists()) {
		d.active = tabs.activated;
	}
	if (tabs.add_requested) {
		instance inst = make_instance(d, std::format("Terminal {}", d.next_number++));
		inst.cursor = d.sink->sequence();
		d.active = inst.instance_id;
		d.instances.push_back(std::move(inst));
	}

	if (tabs.close_requested.exists() && !d.pending_close) {
		instance* closing = find_instance(d, tabs.close_requested);
		if (closing && closing->runner && closing->runner->running.load(std::memory_order_acquire)) {
			d.pending_close = closing->instance_id;
		}
		else if (closing) {
			erase_instance(d, closing->instance_id);
		}
	}

	instance* active = find_instance(d, d.active);
	if (!active) {
		d.active = d.instances.front().instance_id;
		active = &d.instances.front();
	}

	draw_instance(ui, input, d, *active, content, channels, building);

	draw_close_confirm(ui, d, body);
}

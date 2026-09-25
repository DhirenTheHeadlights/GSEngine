module gse.ide.terminal;

import gse;
import gse.ide.build;
import gse.ide.config;
import gse.ide.navigation;
import gse.win32;
import std;

namespace gse::ide::terminal {
	constexpr std::size_t max_lines = 8192;
	constexpr std::size_t max_offers = 16;
	constexpr float profile_picker_width = 210.f;
	constexpr float profile_editor_min_width = 260.f;

	constexpr std::string_view profile_label_name = "Name";
	constexpr std::string_view profile_label_config = "Configuration";
	constexpr std::string_view profile_label_clients = "Clients";
	constexpr std::string_view profile_label_server = "Dedicated server";
	constexpr std::string_view profile_label_attached = "Attached to the editor";
	constexpr std::string_view profile_label_port = "Server port";

	constexpr std::array<std::string_view, 6> profile_field_labels = {
		profile_label_name,
		profile_label_config,
		profile_label_clients,
		profile_label_server,
		profile_label_attached,
		profile_label_port,
	};

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

	auto close_kind(
		data& d,
		build_runner::stream_kind kind
	) -> void;

	auto append_lines(
		instance& inst,
		std::span<const line> lines,
		const gui::draw_context& ctx
	) -> void;

	auto offer_site(
		const agent::blame_offer& offer
	) -> std::string;

	auto offer_pending_line(
		const agent::blame_offer& offer
	) -> std::string;

	auto offer_sent_line(
		const agent::blame_offer& offer
	) -> std::string;

	auto take_offers(
		data& d,
		instance& inst,
		const gui::draw_context& ctx
	) -> void;

	auto dispatch_at(
		instance& inst,
		std::uint32_t buffer_line
	) -> dispatch_marker*;

	auto draw_instance(
		gui::builder& ui,
		data& d,
		instance& inst,
		const rectf& area,
		channel_write<agent::start_request, agent::dispatch_request, build_runner::build_request, build_runner::select_profile_request, build_runner::edit_profiles_request, package_sdk::request, gui::menu_content, jump_to_request, set_cursor_shape_request> channels,
		bool building
	) -> void;

	auto draw_build_row(
		gui::builder& ui,
		data& d,
		const rectf& input_rect,
		channel_write<agent::start_request, agent::dispatch_request, build_runner::build_request, build_runner::select_profile_request, build_runner::edit_profiles_request, package_sdk::request, gui::menu_content, jump_to_request, set_cursor_shape_request> channels,
		bool building
	) -> rectf;

	auto draw_profile_editor(
		gui::builder& ui,
		data& d,
		const rectf& anchor,
		const rectf& opener,
		channel_write<agent::start_request, agent::dispatch_request, build_runner::build_request, build_runner::select_profile_request, build_runner::edit_profiles_request, package_sdk::request, gui::menu_content, jump_to_request, set_cursor_shape_request> channels
	) -> void;

	auto sync_config_options(
		data& d
	) -> void;

	auto config_option_index(
		std::string_view config
	) -> std::size_t;

	auto config_for_option(
		std::size_t index
	) -> std::string;

	struct editor_metrics {
		float pad = 0.f;
		float row_h = 0.f;
		float advance = 0.f;
	};

	auto profile_editor_row(
		const rectf& body,
		const editor_metrics& metrics,
		std::size_t index
	) -> rectf;

	auto profile_editor_field(
		const gui::draw_context& ctx,
		const rectf& row,
		std::string_view label
	) -> rectf;
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
		.tail_id = generate_temp_id(hash_combine(instance_id.number(), stable_id("tail"))),
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

auto gse::ide::terminal::close_kind(data& d, const build_runner::stream_kind kind) -> void {
	if (kind == build_runner::stream_kind::none) {
		return;
	}
	while (true) {
		const auto found = std::ranges::find(d.instances, kind, &instance::kind);
		if (found == d.instances.end()) {
			return;
		}
		if (found->runner && found->runner->running.load(std::memory_order_acquire)) {
			spawn::terminate_process(*found->runner);
		}
		erase_instance(d, found->instance_id);
	}
}

auto gse::ide::terminal::append_lines(instance& inst, const std::span<const line> lines, const gui::draw_context& ctx) -> void {
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

		std::erase_if(inst.dispatches, [overflow](const dispatch_marker& marker) {
			return marker.line < overflow;
		});
		for (dispatch_marker& marker : inst.dispatches) {
			marker.line -= static_cast<std::uint32_t>(overflow);
		}

		const float dropped = static_cast<float>(overflow) * gui::text_area_line_height(ctx);
		inst.view.scroll.y.offset = std::max(0.f, inst.view.scroll.y.offset - dropped);
		inst.view.scroll.y.target = std::max(0.f, inst.view.scroll.y.target - dropped);
	}

	assert(inst.buffer.lines.size() == inst.line_levels.size(), "terminal buffer line metadata diverged");
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
}

auto gse::ide::terminal::offer_site(const agent::blame_offer& offer) -> std::string {
	const std::string head = std::format("{}:{}", offer.file.filename().generic_display_string(), offer.line);
	return offer.extra == 0
		? head
		: std::format("{} +{}", head, offer.extra);
}

auto gse::ide::terminal::offer_pending_line(const agent::blame_offer& offer) -> std::string {
	const std::string site = offer_site(offer);
	return offer.session == 0
		? std::format("  -> no chat owns {} - start one to fix it", site)
		: std::format("  -> send {} to \"{}\"", site, offer.session_name);
}

auto gse::ide::terminal::offer_sent_line(const agent::blame_offer& offer) -> std::string {
	const std::string site = offer_site(offer);
	return offer.session == 0
		? std::format("  -- started a chat to fix {}", site)
		: std::format("  -- sent {} to \"{}\"", site, offer.session_name);
}

auto gse::ide::terminal::take_offers(data& d, instance& inst, const gui::draw_context& ctx) -> void {
	if (d.offers.empty()) {
		return;
	}

	std::vector<agent::blame_offer> mine;
	std::vector<agent::blame_offer> rest;
	for (agent::blame_offer& offer : d.offers) {
		(offer.kind == inst.kind ? mine : rest).push_back(std::move(offer));
	}
	d.offers = std::move(rest);
	if (mine.empty()) {
		return;
	}

	std::vector<line> rows;
	rows.reserve(mine.size());
	for (const agent::blame_offer& offer : mine) {
		rows.push_back({
			.seq = 0,
			.lvl = log::level::warning,
			.text = offer_pending_line(offer),
		});
	}
	append_lines(inst, rows, ctx);

	auto placed = static_cast<std::uint32_t>(inst.buffer.lines.size() - mine.size());
	for (agent::blame_offer& offer : mine) {
		inst.dispatches.push_back({
			.line = placed++,
			.offer = std::move(offer),
		});
	}
}

auto gse::ide::terminal::dispatch_at(instance& inst, const std::uint32_t buffer_line) -> dispatch_marker* {
	const auto found = std::ranges::find(inst.dispatches, buffer_line, &dispatch_marker::line);
	return found == inst.dispatches.end() ? nullptr : &*found;
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
		for (const config::worktree& tree : config::worktrees()) {
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
	{
		std::lock_guard _(m_mutex);
		m_lines.push_back({
			.seq = m_next++,
			.lvl = lvl,
			.text = std::move(text),
		});
		while (m_lines.size() > max_lines) {
			m_lines.pop_front();
		}
	}

	frame_demand::request_redraw();
}

auto gse::ide::terminal::ring_sink::write(const log::record& rec) -> void {
	push(rec.lvl, std::format("[{}] {}{}", rec.cat, rec.prefix, rec.message));
}

auto gse::ide::terminal::ring_sink::write_raw(const std::string_view text) -> void {
	push(log::level::info, std::string(text));
}

auto gse::ide::terminal::ring_sink::flush() -> void {}

auto gse::ide::terminal::ring_sink::drain(std::uint64_t& cursor, std::vector<line>& out) -> void {
	std::lock_guard _(m_mutex);
	for (const line& l : m_lines) {
		if (l.seq >= cursor) {
			out.push_back(l);
		}
	}
	cursor = m_next;
}

auto gse::ide::terminal::ring_sink::sequence() -> std::uint64_t {
	std::lock_guard _(m_mutex);
	return m_next;
}

auto gse::ide::terminal::init(data& d) -> async::task<> {
	d.prompt = ellipsize_path(config::project_root().generic_display_string()) + "> ";
	register_sink(d);
	return {};
}

auto gse::ide::terminal::run(context& ctx, data& d, const channel_read<build_runner::stream_opened, build_runner::attached_fatal_reported, agent::blame_offer> stream_in, const channel_write<agent::start_request, agent::dispatch_request, build_runner::build_request, build_runner::select_profile_request, build_runner::edit_profiles_request, package_sdk::request, gui::menu_content, jump_to_request, set_cursor_shape_request> ui_out, const shared_view<build_runner::data> build_d) -> async::task<> {
	const auto opened_streams = stream_in.of<build_runner::stream_opened>();

	for (const build_runner::stream_opened& opened : opened_streams) {
		close_kind(d, opened.kind);
	}

	for (const build_runner::stream_opened& opened : opened_streams) {
		instance inst = make_instance(d, opened.name);
		inst.cursor = d.sink ? d.sink->sequence() : 0;
		inst.runner = opened.stream;
		inst.interactive = false;
		inst.kind = opened.kind;
		inst.attached_instance = opened.attached_instance;
		d.active = inst.instance_id;
		d.instances.push_back(std::move(inst));
	}

	for (const build_runner::attached_fatal_reported& fatal : stream_in.of<build_runner::attached_fatal_reported>()) {
		const auto tab = std::ranges::find_if(d.instances, [&fatal](const instance& inst) {
			return inst.attached_instance == fatal.instance;
		});
		if (tab == d.instances.end()) {
			continue;
		}
		tab->fatal = fatal;
		d.active = tab->instance_id;
	}

	for (const agent::blame_offer& offer : stream_in.of<agent::blame_offer>()) {
		d.offers.push_back(offer);
	}
	if (d.offers.size() > max_offers) {
		d.offers.erase(d.offers.begin(), d.offers.begin() + static_cast<std::ptrdiff_t>(d.offers.size() - max_offers));
	}
	if (!d.editing_profiles) {
		if (d.profiles != build_d.profiles) {
			d.profiles = build_d.profiles;
		}
		d.active_profile = build_d.active_profile;
	}

	const bool building = build_d.building;
	ui_out.push<gui::menu_content>({
		.menu = std::string(panel_name),
		.layer = render_layer::content,
		.build = [d = &d, channels = ui_out, building](gui::builder& b) {
			draw_panel(b, *d, channels, building);
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

auto gse::ide::terminal::draw_instance(gui::builder& ui, data& d, instance& inst, const rectf& area, channel_write<agent::start_request, agent::dispatch_request, build_runner::build_request, build_runner::select_profile_request, build_runner::edit_profiles_request, package_sdk::request, gui::menu_content, jump_to_request, set_cursor_shape_request> channels, const bool building) -> void {
	const gui::draw_context& ctx = ui.ctx;
	const auto _ = ctx.fonts.text.resolve();
	const auto code_view = ctx.fonts.code.resolve();

	d.fresh.clear();
	if (inst.follows_log) {
		d.sink->drain(inst.cursor, d.fresh);
	}
	if (inst.runner) {
		std::lock_guard _(inst.runner->mutex);
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
				.cwd = config::project_root(),
			});
			d.fresh.push_back({
				.seq = 0,
				.lvl = log::level::info,
				.text = std::format("started a session in the {} panel", agent::panel_name),
			});
		}
		else {
			if (!inst.runner) {
				inst.runner = std::make_shared<command_runner>();
			}
			inst.runner->terminated.store(false, std::memory_order_release);
			inst.runner->running.store(true, std::memory_order_release);
			inst.worker = task::spawn(log::thread_role::terminal, [r = inst.runner, cmd = inst.input, cwd = config::project_root().wstring()](const std::stop_token&) {
				run_command(*r, cmd, cwd);
			});
		}

		inst.input.clear();
		inst.input_state = {};
	}

	if (!d.fresh.empty()) {
		append_lines(inst, d.fresh, ctx);
	}
	take_offers(d, inst, ctx);

	if (inst.buffer.lines.empty()) {
		inst.buffer.lines.emplace_back();
		inst.line_levels.push_back(log::level::info);
	}

	const float pad = ctx.style.padding;
	const float input_h = inst.interactive ? code_view->line_height(ctx.style.font_size) + pad : 0.f;
	const float accent_h = inst.interactive ? ctx.style.accent_bar_width : 0.f;
	const float line_h = code_view->line_height(ctx.style.font_size);
	const float text_width = std::max(0.f, area.width() - pad * 2.f);
	const std::vector<std::string_view> comment_lines = inst.fatal
		? code_view->wrap(inst.fatal->comment, text_width, ctx.style.font_size)
		: std::vector<std::string_view>{};
	const float banner_h = inst.fatal ? line_h * static_cast<float>(comment_lines.size() + 2) + pad * 2.f : 0.f;

	if (inst.fatal) {
		const rectf banner = rectf::from_position_size(
			{ area.left(), area.top() },
			{ area.width(), banner_h }
		);
		ui.draw<gui::panel_backdrop>({
			.rect = banner,
			.background = ctx.style.color_panel_alt,
		});

		const float text_x = banner.left() + pad;
		const float center = code_view->vertical_center_offset(ctx.style.font_size);
		float row_y = banner.top() - pad - line_h;

		ctx.queue_text({
			.font = ctx.fonts.code,
			.text = ctx.intern(std::format("Assertion failed in {}", inst.fatal->function)),
			.position = { text_x, row_y + center },
			.scale = ctx.style.font_size,
			.color = ctx.style.color_error,
			.clip_rect = banner,
		});
		row_y -= line_h;

		for (const std::string_view comment_line : comment_lines) {
			ctx.queue_text({
				.font = ctx.fonts.code,
				.text = comment_line,
				.position = { text_x, row_y + center },
				.scale = ctx.style.font_size,
				.color = ctx.style.color_text,
				.clip_rect = banner,
			});
			row_y -= line_h;
		}

		ctx.queue_text({
			.font = ctx.fonts.code,
			.text = ctx.intern(std::format("{}:{}", inst.fatal->file, inst.fatal->line)),
			.position = { text_x, row_y + center },
			.scale = ctx.style.font_size,
			.color = ctx.style.color_text_secondary,
			.clip_rect = banner,
		});
	}

	const rectf log_rect = rectf::from_position_size(
		{ area.left(), area.top() - banner_h },
		{ area.width(), std::max(0.f, area.height() - banner_h - input_h - accent_h) }
	);

	const gui::interaction::press tail_press = ui.draw<gui::follow_tail>({
		.area = log_rect,
		.state = inst.view,
		.widget_id = inst.tail_id,
	});

	d.underlines.clear();
	std::optional<link_hit> link;
	dispatch_marker* offered = nullptr;
	const vec2f mouse = ctx.mouse_position();
	const bool goto_ctrl = ctx.key_held(key::left_control) || ctx.key_held(key::right_control);
	if ((goto_ctrl || !inst.dispatches.empty()) && ctx.hovers(log_rect) && !tail_press.hovered && !inst.buffer.lines.empty()) {
		const gui::buffer_position hover = gui::text_area_position_at(ctx, {
			.buffer = inst.buffer,
			.state = inst.view,
			.rect = log_rect,
			.spans = inst.spans,
		}, mouse);
		const std::optional<link_hit> hit = goto_ctrl ? path_link_at(inst.buffer.line(hover.line), hover.column) : std::nullopt;
		if (hit) {
			link = hit;
			d.underlines.push_back({
				.line = hover.line,
				.start_col = hit->start_col,
				.end_col = hit->end_col,
				.color = ctx.style.color_text_secondary,
			});
		}
		else if (dispatch_marker* marker = dispatch_at(inst, hover.line)) {
			offered = marker;
			d.underlines.push_back({
				.line = marker->line,
				.start_col = 0,
				.end_col = static_cast<std::uint32_t>(inst.buffer.line(marker->line).size()),
				.color = ctx.style.color_warning,
			});
		}
	}
	if (link || offered) {
		channels.push<set_cursor_shape_request>({
			.shape = cursor_shape::hand,
		});
	}
	const bool acted = (link || offered) && ctx.clicked_in_rect(log_rect);
	const bool goto_click = acted && link.has_value();
	const bool dispatch_click = acted && offered != nullptr;

	ui.draw<gui::text_area>({
		.buffer = inst.buffer,
		.state = inst.view,
		.widget_id = inst.log_id,
		.spans = inst.spans,
		.underlines = d.underlines,
		.rect = log_rect,
		.read_only = true,
		.follow_tail = true,
		.blink_interval = time{},
	});

	if (goto_click && link) {
		channels.push<jump_to_request>({
			.path = link->path,
			.line = link->line,
			.column = link->column,
		});
	}

	if (dispatch_click && offered) {
		const std::uint32_t dispatched = offered->offer.session;
		channels.push<agent::dispatch_request>({
			.session = dispatched,
		});

		for (const dispatch_marker& marker : inst.dispatches) {
			if (marker.offer.session != dispatched) {
				continue;
			}
			inst.buffer.lines[marker.line] = offer_sent_line(marker.offer);
			inst.spans[marker.line].end_col = static_cast<std::uint32_t>(inst.buffer.line(marker.line).size());
		}
		std::erase_if(inst.dispatches, [dispatched](const dispatch_marker& marker) {
			return marker.offer.session == dispatched;
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

	const rectf build_row = draw_build_row(ui, d, input_rect, channels, building);

	const rectf input_box = rectf::from_position_size(
		{ input_rect.left() + prompt_width, input_rect.top() },
		{ std::max(0.f, build_row.left() - pad - input_rect.left() - prompt_width), input_h }
	);

	ui.draw<gui::text_input>({
		.buffer = inst.input,
		.state = inst.input_state,
		.rect = input_box,
		.widget_id = inst.input_id,
		.font = ctx.fonts.code,
	});

	ctx.queue_sprite({
		.rect = rectf::from_position_size({ input_rect.left(), input_rect.top() + accent_h }, { input_rect.width(), accent_h }),
		.color = ctx.style.color_accent,
		.texture = ctx.blank_texture,
	});

	if (ctx.mouse_pressed_for(input_rect)) {
		ui.focus_widget_id = inst.input_id;
	}
}

auto gse::ide::terminal::sync_config_options(data& d) -> void {
	const std::span<const build_runner::build_config> configs = build_runner::build_configs();
	if (d.config_options.size() == configs.size() + 1) {
		return;
	}

	const std::string_view active = build_runner::active_build_config();
	d.config_options.clear();
	d.config_options.reserve(configs.size() + 1);
	d.config_options.push_back(active.empty()
		? std::string("Editor's configuration")
		: std::format("Editor's configuration ({})", build_runner::build_config_label(active)));
	for (const build_runner::build_config& option : configs) {
		d.config_options.push_back(option.label);
	}
}

auto gse::ide::terminal::config_option_index(const std::string_view config) -> std::size_t {
	if (config.empty()) {
		return 0;
	}
	const std::span<const build_runner::build_config> configs = build_runner::build_configs();
	const auto found = std::ranges::find(configs, config, &build_runner::build_config::name);
	return found != configs.end() ? static_cast<std::size_t>(std::ranges::distance(configs.begin(), found)) + 1 : 0;
}

auto gse::ide::terminal::config_for_option(const std::size_t index) -> std::string {
	const std::span<const build_runner::build_config> configs = build_runner::build_configs();
	if (index == 0 || index > configs.size()) {
		return {};
	}
	return configs[index - 1].name;
}

auto gse::ide::terminal::profile_editor_row(const rectf& body, const editor_metrics& metrics, const std::size_t index) -> rectf {
	return rectf::from_position_size(
		{ body.left() + metrics.pad, body.top() - metrics.pad - metrics.advance * static_cast<float>(index) },
		{ body.width() - metrics.pad * 2.f, metrics.row_h }
	);
}

auto gse::ide::terminal::profile_editor_field(const gui::draw_context& ctx, const rectf& row, const std::string_view label) -> rectf {
	const auto text_view = ctx.fonts.text.resolve();
	const std::array<rectf, 2> cells = gui::layout::split_horizontal<2>(row, {
		gui::layout::size_spec::ratio(ctx.style.label_column_ratio),
		gui::layout::size_spec::flex(1.f),
	}, 0.f);

	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = label,
		.position = { cells[0].left(), cells[0].center().y() + text_view->vertical_center_offset(ctx.style.font_size) },
		.scale = ctx.style.font_size,
		.color = ctx.style.color_text,
		.clip_rect = cells[0],
	});

	return cells[1];
}

auto gse::ide::terminal::draw_build_row(gui::builder& ui, data& d, const rectf& input_rect, const channel_write<agent::start_request, agent::dispatch_request, build_runner::build_request, build_runner::select_profile_request, build_runner::edit_profiles_request, package_sdk::request, gui::menu_content, jump_to_request, set_cursor_shape_request> channels, const bool building) -> rectf {
	const gui::draw_context& ctx = ui.ctx;
	const auto text_view = ctx.fonts.text.resolve();
	const float pad = ctx.style.padding;
	const float row_h = input_rect.height();

	const build_runner::build_profile* active = build_runner::profile_for(d.profiles, d.active_profile);
	const bool ready = !building && active != nullptr;

	const rectf run_btn = rectf::from_position_size(
		{ input_rect.right() - row_h - pad, input_rect.top() },
		{ row_h, row_h }
	);
	if (ui.draw<gui::button>({
		.rect = run_btn,
		.key = "##terminal_run",
		.glyph = gui::symbol::play(),
		.enabled = ready,
	})) {
		channels.push<build_runner::build_request>(build_runner::request_for_profile(*active, true));
	}

	const std::string_view build_label = building ? "Building..." : "Build";
	const float build_w = text_view->width(build_label, ctx.style.font_size) + row_h + pad * 1.5f;
	const rectf build_btn = rectf::from_position_size(
		{ run_btn.left() - build_w - pad, input_rect.top() },
		{ build_w, row_h }
	);
	if (ui.draw<gui::button>({
		.text = build_label,
		.rect = build_btn,
		.key = "##terminal_build",
		.glyph = gui::symbol::hammer(),
		.enabled = ready,
	})) {
		channels.push<build_runner::build_request>(build_runner::request_for_profile(*active, false));
	}

	d.profile_options.clear();
	d.profile_options.reserve(d.profiles.size());
	for (const build_runner::build_profile& profile : d.profiles) {
		d.profile_options.push_back(build_runner::profile_label(profile));
	}

	float widest = 0.f;
	for (const std::string& option : d.profile_options) {
		widest = std::max(widest, text_view->width(option, ctx.style.font_size));
	}

	const rectf edit_btn = rectf::from_position_size(
		{ build_btn.left() - row_h - pad, input_rect.top() },
		{ row_h, row_h }
	);
	if (ui.draw<gui::button>({
		.rect = edit_btn,
		.key = "##terminal_profiles",
		.glyph = gui::symbol::gear(),
	})) {
		d.editing_profiles = !d.editing_profiles;
		d.editing_index = build_runner::profile_index(d.profiles, d.active_profile);
		d.profile_name_state = {};
		d.profile_dropdown.open_dropdown_id.reset();
	}

	const std::string_view package_label = "Package SDK";
	const float package_w = text_view->width(package_label, ctx.style.font_size) + pad * 2.f;
	const rectf package_btn = rectf::from_position_size(
		{ edit_btn.left() - package_w - pad, input_rect.top() },
		{ package_w, row_h }
	);
	if (ui.draw<gui::button>({
		.text = package_label,
		.rect = package_btn,
		.key = "##terminal_package_sdk",
		.enabled = !building,
	})) {
		channels.push<package_sdk::request>({});
	}

	const float picker_w = std::min(widest + ctx.style.icon_extent + pad * 3.f, profile_picker_width);
	const rectf picker = rectf::from_position_size(
		{ package_btn.left() - picker_w - pad, input_rect.top() },
		{ picker_w, row_h }
	);

	std::size_t selected = build_runner::profile_index(d.profiles, d.active_profile);
	const gui::dropdown_result picked = ui.draw<gui::dropdown<>>({
		.name = "terminal.profile",
		.current_index = selected,
		.options = d.profile_options,
		.state = d.profile_dropdown,
		.rect = picker,
	});

	if (picked.changed && picked.new_index < d.profiles.size()) {
		d.active_profile = d.profiles[picked.new_index].name;
		channels.push<build_runner::select_profile_request>({
			.name = d.active_profile,
		});
	}

	if (d.editing_profiles) {
		draw_profile_editor(ui, d, picker, edit_btn, channels);
	}

	return picker;
}

auto gse::ide::terminal::draw_profile_editor(gui::builder& ui, data& d, const rectf& anchor, const rectf& opener, const channel_write<agent::start_request, agent::dispatch_request, build_runner::build_request, build_runner::select_profile_request, build_runner::edit_profiles_request, package_sdk::request, gui::menu_content, jump_to_request, set_cursor_shape_request> channels) -> void {
	if (d.profiles.empty()) {
		d.editing_profiles = false;
		return;
	}

	const gui::draw_context& ctx = ui.ctx;
	const auto text_view = ctx.fonts.text.resolve();
	const float pad = ctx.style.padding;
	const editor_metrics metrics{
		.pad = pad,
		.row_h = text_view->line_height(ctx.style.font_size) + pad * ctx.style.widget_height_padding,
		.advance = text_view->line_height(ctx.style.font_size) + pad * ctx.style.widget_height_padding + pad,
	};

	sync_config_options(d);
	d.editing_index = std::min(d.editing_index, d.profiles.size() - 1);
	build_runner::build_profile& edited = d.profiles[d.editing_index];

	const std::size_t form_rows = edited.session.dedicated_server ? 8 : 7;
	const std::size_t rows = d.profiles.size() + form_rows;
	const float height = metrics.advance * static_cast<float>(rows) + pad;

	float widest_label = 0.f;
	for (const std::string_view label : profile_field_labels) {
		widest_label = std::max(widest_label, text_view->width(label, ctx.style.font_size));
	}
	const float label_driven = (widest_label + pad) / ctx.style.label_column_ratio + pad * 2.f;
	const float width = std::max({ profile_editor_min_width, anchor.width(), label_driven });

	const rectf body = rectf::from_position_size(
		{ anchor.left(), anchor.top() + pad + height },
		{ width, height }
	);

	const auto _ = ctx.scoped_layer(render_layer::modal);
	ctx.register_hit_region(render_layer::modal, body);

	ctx.queue_sprite({
		.rect = body.inset({ -1.f, -1.f }),
		.color = ctx.style.color_border,
		.texture = ctx.blank_texture,
	});
	ctx.queue_sprite({
		.rect = body,
		.color = ctx.style.color_panel_alt,
		.texture = ctx.blank_texture,
	});

	std::size_t row = 0;
	const rectf title_row = profile_editor_row(body, metrics, row++);
	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = "Build Profiles",
		.position = { title_row.left(), title_row.center().y() + text_view->vertical_center_offset(ctx.style.font_size) },
		.scale = ctx.style.font_size,
		.color = ctx.style.color_text_secondary,
		.clip_rect = title_row,
	});

	const rectf close_btn = rectf::from_position_size(
		{ title_row.right() - metrics.row_h, title_row.top() },
		{ metrics.row_h, metrics.row_h }
	);
	if (ui.draw<gui::button>({
		.rect = close_btn,
		.key = "##profile_editor_close",
		.glyph = gui::symbol::close(),
	})) {
		d.editing_profiles = false;
	}

	for (std::size_t i = 0; i < d.profiles.size(); ++i) {
		const rectf entry = profile_editor_row(body, metrics, row++);
		const gui::interaction::press pressed = gui::interaction::press_in_rect(
			ctx,
			ui.hot_widget_id,
			ui.active_widget_id,
			gui::ids::make_from_key(stable_id(std::format("##profile_row{}", i))),
			entry
		);
		if (pressed.activated) {
			d.editing_index = i;
			d.profile_name_state = {};
		}

		if (i == d.editing_index || pressed.hovered) {
			ctx.queue_sprite({
				.rect = entry,
				.color = i == d.editing_index ? ctx.style.color_accent_dim : ctx.style.color_widget_hovered,
				.texture = ctx.blank_texture,
			});
		}
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = build_runner::profile_label(d.profiles[i]),
			.position = { entry.left() + pad, entry.center().y() + text_view->vertical_center_offset(ctx.style.font_size) },
			.scale = ctx.style.font_size,
			.color = d.profiles[i].name == d.active_profile ? ctx.style.color_accent : ctx.style.color_text,
			.clip_rect = entry,
		});
	}

	const rectf actions = profile_editor_row(body, metrics, row++);
	const std::array<rectf, 3> action_cells = gui::layout::split_horizontal<3>(actions, {
		gui::layout::size_spec::flex(1.f),
		gui::layout::size_spec::flex(1.f),
		gui::layout::size_spec::flex(1.f),
	}, pad * 0.5f);

	bool changed = false;
	if (ui.draw<gui::button>({
		.text = "New",
		.rect = action_cells[0],
		.key = "##profile_new",
		.glyph = gui::symbol::plus(),
	})) {
		d.profiles.push_back({
			.name = build_runner::unique_profile_name(d.profiles, "Profile"),
		});
		d.editing_index = d.profiles.size() - 1;
		d.profile_name_state = {};
		changed = true;
	}
	if (ui.draw<gui::button>({
		.text = "Duplicate",
		.rect = action_cells[1],
		.key = "##profile_duplicate",
	})) {
		build_runner::build_profile copy = d.profiles[d.editing_index];
		copy.name = build_runner::unique_profile_name(d.profiles, copy.name);
		d.profiles.push_back(std::move(copy));
		d.editing_index = d.profiles.size() - 1;
		d.profile_name_state = {};
		changed = true;
	}
	if (ui.draw<gui::button>({
		.text = "Delete",
		.rect = action_cells[2],
		.key = "##profile_delete",
		.glyph = gui::symbol::trash(),
		.enabled = d.profiles.size() > 1,
		.role = gui::button_role::danger,
	})) {
		const bool was_active = d.profiles[d.editing_index].name == d.active_profile;
		d.profiles.erase(d.profiles.begin() + static_cast<std::ptrdiff_t>(d.editing_index));
		d.editing_index = std::min(d.editing_index, d.profiles.size() - 1);
		if (was_active) {
			d.active_profile = d.profiles[d.editing_index].name;
		}
		d.profile_name_state = {};
		changed = true;
	}

	if (changed) {
		channels.push<build_runner::edit_profiles_request>({
			.profiles = d.profiles,
			.active = d.active_profile,
		});
		return;
	}

	const std::string previous_name = edited.name;
	ui.draw<gui::text_input>({
		.name = "terminal.profile.name",
		.buffer = edited.name,
		.state = d.profile_name_state,
		.rect = profile_editor_field(ctx, profile_editor_row(body, metrics, row++), profile_label_name),
	});
	if (edited.name != previous_name) {
		const build_runner::build_profile* clash = build_runner::profile_for(d.profiles, edited.name);
		if (edited.name.empty() || (clash != nullptr && clash != &edited)) {
			edited.name = previous_name;
		}
		else {
			if (previous_name == d.active_profile) {
				d.active_profile = edited.name;
			}
			changed = true;
		}
	}

	std::size_t config_index = config_option_index(edited.config);
	const gui::dropdown_result config_picked = ui.draw<gui::dropdown<>>({
		.name = "terminal.profile.config",
		.current_index = config_index,
		.options = d.config_options,
		.state = d.config_dropdown,
		.rect = profile_editor_field(ctx, profile_editor_row(body, metrics, row++), profile_label_config),
	});
	if (config_picked.changed) {
		edited.config = config_for_option(config_picked.new_index);
		changed = true;
	}

	int clients = edited.session.clients;
	const int previous_clients = clients;
	const std::string clients_label = std::to_string(clients);
	ui.draw<gui::slider<int>>({
		.name = "terminal.profile.clients",
		.value = clients,
		.min = 1,
		.max = build_runner::max_profile_clients,
		.rect = profile_editor_field(ctx, profile_editor_row(body, metrics, row++), profile_label_clients),
		.value_label = clients_label,
	});
	if (clients != previous_clients) {
		edited.session.clients = static_cast<std::uint8_t>(clients);
		changed = true;
	}

	changed |= ui.draw<gui::toggle>({
		.name = profile_label_server,
		.value = edited.session.dedicated_server,
		.rect = profile_editor_row(body, metrics, row++),
	});
	changed |= ui.draw<gui::toggle>({
		.name = profile_label_attached,
		.value = edited.session.attached,
		.rect = profile_editor_row(body, metrics, row++),
	});

	if (edited.session.dedicated_server) {
		int port = edited.session.base_port;
		const int previous_port = port;
		const std::string port_label = std::to_string(port);
		ui.draw<gui::slider<int>>({
			.name = "terminal.profile.port",
			.value = port,
			.min = 1024,
			.max = 65535,
			.rect = profile_editor_field(ctx, profile_editor_row(body, metrics, row++), profile_label_port),
			.value_label = port_label,
		});
		if (port != previous_port) {
			edited.session.base_port = static_cast<std::uint16_t>(port);
			changed = true;
		}
	}

	if (changed) {
		channels.push<build_runner::edit_profiles_request>({
			.profiles = d.profiles,
			.active = d.active_profile,
		});
	}

	const std::array<rectf, 2> keep_open = { anchor, opener };
	if (gui::interaction::dismissed_by_outside_press(ctx, {
		.body = body,
		.keep_open = keep_open,
		.suppressed = d.config_dropdown.open_dropdown_id.exists(),
	})) {
		d.editing_profiles = false;
	}
}

auto gse::ide::terminal::draw_close_confirm(gui::builder& ui, data& d, const rectf& body) -> void {
	instance* closing = d.pending_close ? find_instance(d, *d.pending_close) : nullptr;
	if (!closing) {
		d.pending_close.reset();
		return;
	}

	const gui::confirm_result result = ui.draw<gui::confirm_dialog>({
		.body = body,
		.title = "Kill running process?",
		.message = std::format("\"{}\" is still running.", closing->name),
		.confirm_label = "Kill",
		.key = "##terminal_close",
	});

	if (result == gui::confirm_result::confirmed) {
		if (closing->runner) {
			spawn::terminate_process(*closing->runner);
		}
		erase_instance(d, closing->instance_id);
		d.pending_close.reset();
	}
	else if (result == gui::confirm_result::cancelled) {
		d.pending_close.reset();
	}
}

auto gse::ide::terminal::draw_panel(gui::builder& ui, data& d, channel_write<agent::start_request, agent::dispatch_request, build_runner::build_request, build_runner::select_profile_request, build_runner::edit_profiles_request, package_sdk::request, gui::menu_content, jump_to_request, set_cursor_shape_request> channels, const bool building) -> void {
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

	const vec2f mouse = ctx.mouse_position();
	const bool pressed = ctx.mouse_pressed(mouse_button::button_1) && ctx.input_available();
	const bool held = ctx.mouse_held(mouse_button::button_1);

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

	if ((panels.divider.contains(mouse) && !resize_blocked) || d.resizing_strip.dragging) {
		channels.push<set_cursor_shape_request>({
			.shape = cursor_shape::resize_ew,
		});
	}

	ui.draw<gui::panel_backdrop>({
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
			.pinned = inst.follows_log,
		});
	}

	const gui::tab_strip_result tabs = gui::tab_strip(ctx, {
		.area = strip,
		.tabs = d.tab_descs,
		.active = d.active,
		.orientation = gui::tab_orientation::vertical,
		.allow_reorder = true,
		.show_add = true,
	}, d.tab_strip);

	if (tabs.activated.exists()) {
		d.active = tabs.activated;
	}
	if (const auto from = tabs.reorder_id.exists() ? std::ranges::find(d.instances, tabs.reorder_id, &instance::instance_id) : d.instances.end(); from != d.instances.end()) {
		const auto to = d.instances.begin() + static_cast<std::ptrdiff_t>(std::min(tabs.reorder_to, d.instances.size() - 1));
		if (from < to) {
			std::rotate(from, from + 1, to + 1);
		}
		else if (to < from) {
			std::rotate(to, from, from + 1);
		}
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

	draw_instance(ui, d, *active, content, channels, building);

	draw_close_confirm(ui, d, body);
}
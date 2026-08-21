module gse.ide.agent:panel_impl;

import std;
import gse;
import gse.win32;

import gse.ide.config;
import gse.ide.navigation;

import :blame;
import :chats;
import :layout;
import :model;
import :panel;
import :session;
import :stream;

auto gse::ide::agent::input_text(const gui::text_buffer& buffer) -> std::string {
	std::string text;
	for (std::size_t i = 0; i < buffer.lines.size(); ++i) {
		if (i > 0) {
			text += '\n';
		}
		text += buffer.lines[i];
	}
	return text;
}

auto gse::ide::agent::input_empty(const gui::text_buffer& buffer) -> bool {
	return std::ranges::all_of(buffer.lines, [](const std::string& line) {
		return line.empty();
	});
}

auto gse::ide::agent::reset_input(session& s) -> void {
	s.draft.lines.assign(1, {});
	s.draft_state = {};
	s.attachments.clear();
}

auto gse::ide::agent::fill_input(session& s, const std::string_view text) -> void {
	s.draft.lines.clear();
	for (std::size_t at = 0; ; ) {
		const std::size_t end = text.find('\n', at);
		if (end == std::string_view::npos) {
			s.draft.lines.emplace_back(text.substr(at));
			break;
		}
		s.draft.lines.emplace_back(text.substr(at, end - at));
		at = end + 1;
	}

	const gui::buffer_position end_of_text = {
		.line = static_cast<std::uint32_t>(s.draft.lines.size() - 1),
		.column = static_cast<std::uint32_t>(s.draft.lines.back().size()),
	};
	s.draft_state = {
		.caret = end_of_text,
		.anchor = end_of_text,
	};
}

auto gse::ide::agent::attach_image(data& d, const shared_view<asset::data> assets, window::clipboard_image pasted) -> void {
	session* target = active_session(d);
	if (!target) {
		return;
	}

	if (!pasted.path.empty() && image::dimensions(pasted.path).x() == 0) {
		return;
	}

	image::data decoded = pasted.path.empty()
		? image::data{
			.size = pasted.size,
			.channels = 4,
			.pixels = std::move(pasted.pixels),
		}
		: image::load_rgba(pasted.path);

	if (decoded.pixels.empty() || decoded.size.x() == 0 || decoded.size.y() == 0) {
		return;
	}

	const std::uint32_t index = d.next_attachment++;
	std::filesystem::path path = pasted.path;

	if (!sendable_encoding(path)) {
		path = std::filesystem::temp_directory_path()
			/ std::format("gse_agent_paste_{}_{}.png", win32::GetCurrentProcessId(), index);
		if (!image::write_png(path, decoded.size.x(), decoded.size.y(), 4, decoded.pixels.data())) {
			return;
		}
	}

	target->attachments.push_back({
		.path = std::move(path),
		.size = decoded.size,
		.preview = asset::queue<texture>(
			assets,
			std::format("agent_paste_{}", index),
			decoded.pixels,
			decoded.size,
			4u,
			texture::profile::generic_clamp_to_edge
		),
	});
}

auto gse::ide::agent::agent_context_tag() -> gse::id {
	return find_or_generate_id("agent_transcript_context");
}

auto gse::ide::agent::draw_session_info(const gui::draw_context& ctx, data& d, const rectf& body) -> void {
	if (!d.info_open) {
		return;
	}

	const session* s = active_session(d);
	if (!s) {
		d.info_open = false;
		return;
	}

	const gui::style& sty = ctx.style;
	const auto text_view = ctx.fonts.text.resolve();
	const float pad = sty.padding;
	const float fs = sty.font_size;
	const float line_h = text_view->line_height(fs) * 1.35f;

	const std::string link = link_label(d, *s);
	const std::array<std::pair<std::string_view, std::string>, 8> rows = { {
		{ "status", !link.empty() ? link : !s->info.failure.empty() ? s->info.failure : s->running ? "running" : "exited" },
		{ "build", unbuilt_label(*s) },
		{ "broke", blame_label(*s) },
		{ "model", s->info.model.empty() ? "-" : s->info.model },
		{ "session", s->info.agent_id.empty() ? "-" : s->info.agent_id },
		{ "turns", std::format("{}", s->info.turns) },
		{ "api time", std::format("{:.1f}s", s->info.api_time.as<seconds>()) },
		{ "api equivalent", std::format("${:.4f}", s->info.cost) },
	} };

	float label_w = 0.f;
	float value_w = 0.f;
	for (const auto& [label, value] : rows) {
		label_w = std::max(label_w, text_view->width(label, fs));
		value_w = std::max(value_w, text_view->width(value, fs));
	}

	const float pw = std::min(body.width(), pad * 3.f + label_w + value_w);
	const float ph = line_h * static_cast<float>(rows.size()) + pad * 2.f;

	float px = d.info_anchor.left();
	if (px + pw > body.right()) {
		px = body.right() - pw;
	}
	px = std::max(px, body.left());
	const float top_y = std::min(body.top(), d.info_anchor.bottom() - pad * 0.5f);

	const rectf panel = rectf::from_position_size({ px, top_y }, { pw, ph });
	const auto scope = ctx.scoped_layer(render_layer::popup);

	ctx.queue_sprite({
		.rect = rectf::from_position_size({ px + 4.f * sty.scale_factor, top_y - 4.f * sty.scale_factor }, { pw, ph }),
		.color = sty.color_shadow,
		.texture = ctx.blank_texture,
		.corner_radius = sty.corner_radius_menu,
	});
	ctx.queue_sprite({
		.rect = panel,
		.color = { vec3f(sty.color_menu_body), 1.f },
		.texture = ctx.blank_texture,
		.corner_radius = sty.corner_radius_menu,
	});

	for (std::size_t i = 0; i < rows.size(); ++i) {
		const float center_y = top_y - pad - line_h * (static_cast<float>(i) + 0.5f) + text_view->vertical_center_offset(fs);
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = rows[i].first,
			.position = { px + pad, center_y },
			.scale = fs,
			.color = sty.color_text_secondary,
			.clip_rect = panel,
		});
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = rows[i].second,
			.position = { panel.right() - pad - text_view->width(rows[i].second, fs), center_y },
			.scale = fs,
			.color = sty.color_text,
			.clip_rect = panel,
		});
	}

	ctx.register_hit_region(render_layer::popup, panel);

	const vec2f mouse = ctx.mouse_position();
	if (ctx.mouse_pressed() && !panel.contains(mouse) && !d.info_anchor.contains(mouse)) {
		d.info_open = false;
	}
}

auto gse::ide::agent::history_label(const data& d, const past_chat& chat) -> std::string_view {
	const auto open = std::ranges::find_if(d.sessions, [&](const session& s) {
		return s.info.agent_id == chat.agent_id;
	});
	if (open != d.sessions.end() && !open->name.empty()) {
		return open->name;
	}

	if (const auto named = d.chat_names.find(chat.agent_id); named != d.chat_names.end() && !named->second.empty()) {
		return named->second;
	}

	return chat.summary.empty() ? std::string_view(chat.agent_id) : std::string_view(chat.summary);
}

auto gse::ide::agent::draw_history(gui::builder& ui, data& d, const rectf& body) -> void {
	if (!d.history_open) {
		return;
	}

	gui::draw_context& ctx = ui.ctx;
	const gui::style& sty = ctx.style;
	const auto text_view = ctx.fonts.text.resolve();
	const float pad = sty.padding;
	const float fs = sty.font_size;
	const float row_h = text_view->line_height(fs) + pad;

	const float pw = std::min(body.width(), std::max(body.width() * 0.5f, fs * 24.f));
	const float rows = static_cast<float>(std::min(d.history.size(), history_visible_rows));
	const float ph = std::min(body.height(), row_h * std::max(rows, 1.f) + pad * 2.f);

	float px = d.history_anchor.left();
	if (px + pw > body.right()) {
		px = body.right() - pw;
	}
	px = std::max(px, body.left());
	const float top_y = std::min(body.top(), d.history_anchor.bottom() - pad * 0.5f);

	const rectf panel = rectf::from_position_size({ px, top_y }, { pw, ph });
	const auto scope = ctx.scoped_layer(render_layer::popup);

	ctx.queue_sprite({
		.rect = rectf::from_position_size({ px + 4.f * sty.scale_factor, top_y - 4.f * sty.scale_factor }, { pw, ph }),
		.color = sty.color_shadow,
		.texture = ctx.blank_texture,
		.corner_radius = sty.corner_radius_menu,
	});
	ctx.queue_sprite({
		.rect = panel,
		.color = { vec3f(sty.color_menu_body), 1.f },
		.texture = ctx.blank_texture,
		.corner_radius = sty.corner_radius_menu,
	});
	ctx.register_hit_region(render_layer::popup, panel);

	if (d.history.empty()) {
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = "no past chats for this project",
			.position = { panel.left() + pad, panel.top() - pad - row_h * 0.5f + text_view->vertical_center_offset(fs) },
			.scale = fs,
			.color = sty.color_text_secondary,
			.clip_rect = panel,
		});
	}

	const rectf list = rectf::from_position_size(
		{ panel.left() + pad, panel.top() - pad },
		{ panel.width() - pad * 2.f, std::max(0.f, panel.height() - pad * 2.f) }
	);
	ctx.layout_cursor = { list.left(), list.top() };

	const past_chat* chosen = nullptr;
	ui.scroll_region({
		.id = "##agent_history_list",
		.size = list.size(),
	}, [&](gui::builder& b) {
		gui::draw_context& c = b.ctx;
		const vec2f mouse = c.mouse_position();
		const rectf clip = c.current_clip().value_or(list);
		for (past_chat& chat : d.history) {
			const rectf row = rectf::from_position_size(
				{ list.left(), c.layout_cursor.y() },
				{ list.width(), row_h }
			);
			c.layout_cursor.y() -= row_h;
			if (!row.intersects(clip)) {
				continue;
			}
			if (!chat.summarized) {
				chat.summarized = true;
				chat.summary = chat_summary(chat.path);
			}

			const bool over = clip.contains(mouse) && c.hovers(row);
			if (over) {
				c.queue_sprite({
					.rect = row,
					.color = sty.color_widget_hovered,
					.texture = c.blank_texture,
					.clip_rect = clip,
					.corner_radius = sty.corner_radius,
				});
			}
			c.queue_text({
				.font = c.fonts.text,
				.text = history_label(d, chat),
				.position = { row.left() + pad * 0.5f, row.top() - row_h * 0.5f + text_view->vertical_center_offset(fs) },
				.scale = fs,
				.color = over ? sty.color_text : sty.color_text_secondary,
				.clip_rect = clip,
			});
			if (over && c.mouse_pressed_for(row)) {
				chosen = &chat;
			}
		}
	});

	if (chosen) {
		restore_chat(d, *chosen);
		d.history_open = false;
		return;
	}

	const vec2f mouse = ctx.mouse_position();
	if (ctx.mouse_pressed() && !panel.contains(mouse) && !d.history_anchor.contains(mouse)) {
		d.history_open = false;
	}
}

auto gse::ide::agent::session_tab_id(const std::uint32_t session_id) -> gse::id {
	return gui::ids::make(std::format("##agent_tab_{}", session_id));
}

auto gse::ide::agent::draw_session_tabs(gui::builder& ui, data& d, const rectf& body) -> float {
	const gui::draw_context& ctx = ui.ctx;
	const gui::style& sty = ctx.style;
	const float pad = sty.padding;

	std::vector<gui::tab_desc> descs;
	descs.reserve(d.sessions.size());
	for (const session& s : d.sessions) {
		descs.push_back({
			.tab_id = session_tab_id(s.id),
			.caption = s.name,
			.busy = s.think_clock.has_value(),
			.warning = s.stale,
			.error = !s.blame.empty() || !s.info.failure.empty(),
			.dimmed = !s.running,
		});
	}

	const float row_extent = gui::tab_strip_row_extent(ctx.fonts.text, sty);
	const float button_extent = row_extent * 0.7f;
	const float tab_width = std::max(0.f, body.width() - pad * 4.f - button_extent * 2.f);
	const gui::tab_strip_metrics metrics = gui::tab_strip_measure(sty, {
		.font = ctx.fonts.text,
		.tabs = descs,
		.available_extent = tab_width,
		.show_add = true,
	});
	const float strip_h = std::max(sty.font_size * 2.f, gui::tab_strip_extent(metrics, 1));
	const rectf strip = rectf::from_position_size(body.top_left(), { body.width(), strip_h });

	ctx.queue_sprite({
		.rect = strip,
		.color = sty.color_panel_alt,
		.texture = ctx.blank_texture,
	});

	const float button_top = strip.top() - row_extent * 0.5f + button_extent * 0.5f;
	const rectf info = rectf::from_position_size({ strip.right() - pad - button_extent, button_top }, { button_extent, button_extent });
	const rectf history = rectf::from_position_size({ info.left() - pad * 0.5f - button_extent, button_top }, { button_extent, button_extent });

	const rectf tab_area = rectf::from_position_size(
		{ strip.left() + pad, strip.top() },
		{ tab_width, strip.height() }
	);

	const std::uint32_t renaming_before = d.renaming;
	const gui::tab_strip_result tabs = gui::tab_strip(ctx, {
		.area = tab_area,
		.tabs = descs,
		.active = session_tab_id(d.active),
		.allow_reorder = true,
		.show_add = true,
		.renaming = d.renaming != 0 ? session_tab_id(d.renaming) : gse::id{},
	}, d.tab_strip);

	const auto session_of = [&](const gse::id tab_id) -> session* {
		const auto found = std::ranges::find_if(d.sessions, [&](const session& s) {
			return session_tab_id(s.id) == tab_id;
		});
		return found == d.sessions.end() ? nullptr : &*found;
	};

	if (session* activated = tabs.activated.exists() ? session_of(tabs.activated) : nullptr) {
		const bool second_click = gui::interaction::register_click(d.tab_click, ctx.mouse_position()) >= 2;
		if (second_click && activated->id == d.active) {
			d.renaming = activated->id;
			activated->name_state.anchor = 0;
			activated->name_state.caret = static_cast<int>(activated->name.size());
			ui.focus_widget_id = gui::ids::make(std::format("##agent_name_{}", activated->id));
		}
		d.active = activated->id;
	}

	if (const auto from = tabs.reorder_id.exists() ? std::ranges::find_if(d.sessions, [&](const session& s) {
		return session_tab_id(s.id) == tabs.reorder_id;
	}) : d.sessions.end(); from != d.sessions.end()) {
		const auto to = d.sessions.begin() + static_cast<std::ptrdiff_t>(std::min(tabs.reorder_to, d.sessions.size() - 1));
		if (from < to) {
			std::rotate(from, from + 1, to + 1);
		}
		else if (to < from) {
			std::rotate(to, from, from + 1);
		}
	}

	if (const session* closing = tabs.close_requested.exists() ? session_of(tabs.close_requested) : nullptr) {
		request_close(d, closing->id);
	}

	if (tabs.add_requested) {
		create_session(d, config::primary().project_root);
	}

	session* renaming = d.renaming != 0 && tabs.renaming_rect.width() > 0.f ? session_of(session_tab_id(d.renaming)) : nullptr;
	if (renaming) {
		const gse::id input_id = gui::ids::make(std::format("##agent_name_{}", renaming->id));
		gui::draw::text_input_in_rect(
			ctx,
			input_id,
			renaming->name,
			renaming->name_state,
			tabs.renaming_rect,
			ui.hot_widget_id,
			ui.focus_widget_id,
			ctx.fonts.text
		);

		if (ui.focus_widget_id != input_id) {
			if (renaming->name.empty()) {
				renaming->name = std::format("Agent {}", renaming->id);
			}
			d.renaming = 0;
		}
	}
	else if (d.renaming != 0 && d.renaming == renaming_before) {
		if (ui.focus_widget_id == gui::ids::make(std::format("##agent_name_{}", d.renaming))) {
			ui.focus_widget_id.reset();
		}
		d.renaming = 0;
	}

	if (gui::caption_button(ui, history, "##agent_history", gui::symbol::chevron_down(), sty.color_widget_hovered)) {
		d.history_open = !d.history_open;
		if (d.history_open) {
			d.history = past_chats(config::primary().project_root);
		}
	}
	d.history_anchor = history;

	session* s = active_session(d);
	if (!s) {
		d.info_anchor = {};
		d.info_open = false;
		return strip_h;
	}

	if (gui::caption_button(ui, info, std::format("##agent_info_{}", s->id), gui::symbol::info(), sty.color_widget_hovered)) {
		d.info_open = !d.info_open;
	}
	d.info_anchor = info;
	return strip_h;
}

auto gse::ide::agent::draw_close_confirm(gui::builder& ui, data& d, const rectf& body) -> void {
	if (d.pending_close == 0) {
		return;
	}

	const auto closing = std::ranges::find(d.sessions, d.pending_close, &session::id);
	if (closing == d.sessions.end()) {
		d.pending_close = 0;
		return;
	}

	const gui::draw::confirm_result result = gui::draw::confirm_dialog(ui, {
		.body = body,
		.title = "Stop the agent and close the tab?",
		.message = std::format("\"{}\" is still running, and its transcript will be discarded.", closing->name),
		.confirm_label = "Close",
		.key = "##agent_close",
	});

	if (result == gui::draw::confirm_result::confirmed) {
		erase_session(d, d.pending_close);
	}
	else if (result == gui::draw::confirm_result::cancelled) {
		d.pending_close = 0;
	}
}

auto gse::ide::agent::draw_transcript(gui::builder& ui, data& d, const rectf& area, const vec2f mouse, const channel_write<gui::menu_content, jump_to_request, set_cursor_shape_request> jump_out) -> void {
	const gui::draw_context& ctx = ui.ctx;
	const gui::style& sty = ctx.style;
	const float pad = sty.padding;

	session* s = active_session(d);
	if (!s || s->rows.empty()) {
		float y = area.top() - pad - sty.font_size;
		const std::string hint = s
			? "Type a prompt below to start claude in " + s->cwd.generic_display_string() + "."
			: "No agents yet. Type a prompt below, or press + to add a session, in " + config::primary().name + ".";
		for (const std::string_view line : ctx.fonts.text.resolve()->wrap(hint, area.width() - pad * 2.f, sty.font_size)) {
			ctx.queue_text({
				.font = ctx.fonts.text,
				.text = line,
				.position = { area.left() + pad, y },
				.scale = sty.font_size,
				.color = sty.color_text_secondary,
				.clip_rect = area,
			});
			y -= sty.font_size * 1.45f;
		}
		return;
	}

	const auto code_view = ctx.fonts.code.resolve();
	const float advance = code_view->width("0", sty.font_size);
	const transcript_metrics metrics = {
		.face = *code_view,
		.width = std::max(0.f, area.width() - pad * 2.f - gui::scroll_config{}.scrollbar_width),
		.scale = sty.font_size,
	};
	sync_transcript(*s, sty, metrics);

	if (s->buffer.lines.empty()) {
		s->buffer.lines.emplace_back();
		s->line_rows.push_back(0);
	}

	const gui::interaction::press tail_press = gui::draw::follow_tail_button(ui, area, s->view, gui::ids::make("##agent_follow_tail"));

	const gui::buffer_position at = gui::draw::text_area_position_at(ctx, s->buffer, s->view, area, false, transcript_tab_width, mouse, s->blocks);
	const auto hovered = static_cast<std::uint32_t>(std::min<std::size_t>(at.line, s->line_rows.size() - 1));
	const std::string_view hovered_text = s->buffer.line(hovered);
	const link_marker* hit = ctx.hovers(area) && !tail_press.hovered && at.column < hovered_text.size() ? link_at(*s, hovered) : nullptr;
	const std::optional<std::uint32_t> link = hit ? std::optional(hit->row) : std::nullopt;

	std::vector<gui::text_underline> underlines;
	if (link) {
		const std::size_t start = hovered_text.find_first_not_of(' ');
		underlines.push_back({
			.line = hovered,
			.start_col = static_cast<std::uint32_t>(start == std::string_view::npos ? 0 : start),
			.end_col = static_cast<std::uint32_t>(hovered_text.size()),
			.color = sty.color_file,
		});
		jump_out.push<set_cursor_shape_request>({
			.shape = cursor_shape::hand,
		});
	}

	if (ctx.hovers(area) && ctx.mouse_pressed_for(area, mouse_button::button_2)) {
		const auto row = s->line_rows[hovered];
		const bool mine = row < s->rows.size() && s->rows[row].kind == row_kind::user;
		if (const std::optional<std::uint32_t> anchor = mine ? rewind_anchor(*s, row) : std::nullopt; anchor) {
			ctx.open_context_menu({
				.position = mouse,
				.items = {
					{
						.label = "Rewind to this prompt",
						.action_id = 0,
						.destructive = true,
					},
				},
				.target = (static_cast<std::uint64_t>(s->id) << 32) | row,
				.tag = agent_context_tag(),
			});
		}
	}

	if (ctx.mouse_pressed_for(area)) {
		const auto group = std::ranges::find(s->groups, hovered, &group_marker::line);

		if (group != s->groups.end() && group->rows > 1) {
			toggle_group(*s, static_cast<std::size_t>(std::distance(s->groups.begin(), group)));
			sync_transcript(*s, sty, metrics);
		}
		else if (link) {
			const transcript_row& owner = s->rows[std::min<std::size_t>(*link, s->rows.size() - 1)];
			jump_out.push<jump_to_request>({
				.path = owner.file,
				.line = jump_line_for(owner),
				.column = 0,
			});
		}
	}

	gui::draw::text_area_in_rect(
		ctx,
		s->log_id,
		{
			.buffer = s->buffer,
			.state = s->view,
			.spans = s->spans,
			.underlines = underlines,
			.blocks = s->blocks,
			.rect = area,
			.read_only = true,
			.follow_tail = true,
			.indent_width = transcript_tab_width,
			.blink_interval = time{},
		},
		ui.hot_widget_id,
		ui.focus_widget_id
	);

	draw_diff_bars(ctx, *s, area, advance);
}

auto gse::ide::agent::draw_attachments(gui::builder& ui, session& s, const rectf& area) -> void {
	const gui::draw_context& ctx = ui.ctx;
	const gui::style& sty = ctx.style;
	const float pad = sty.padding;

	ctx.queue_sprite({
		.rect = area,
		.color = sty.color_input_background,
		.texture = ctx.blank_texture,
	});

	const auto text_view = ctx.fonts.text.resolve();
	const float thumb = std::max(0.f, area.height() - pad * 2.f);
	const float close_extent = thumb * 0.35f;
	float x = area.left() + pad;
	std::size_t removed = s.attachments.size();

	for (std::size_t i = 0; i < s.attachments.size(); ++i) {
		const attachment& a = s.attachments[i];
		const float aspect = static_cast<float>(a.size.x()) / static_cast<float>(std::max(1u, a.size.y()));
		const float width = std::clamp(thumb * aspect, thumb * 0.5f, thumb * 2.5f);
		const rectf frame = rectf::from_position_size({ x, area.top() - pad }, { width, thumb });

		if (frame.right() > area.right() - pad) {
			ctx.queue_text({
				.font = ctx.fonts.text,
				.text = ctx.intern(std::format("+{}", s.attachments.size() - i)),
				.position = { x, area.center().y() + text_view->vertical_center_offset(sty.font_size) },
				.scale = sty.font_size,
				.color = sty.color_text_secondary,
				.clip_rect = area,
			});
			break;
		}

		ctx.queue_sprite({
			.rect = frame,
			.color = sty.color_tab_background,
			.texture = ctx.blank_texture,
			.clip_rect = area,
			.corner_radius = sty.corner_radius,
		});
		ctx.queue_sprite({
			.rect = frame,
			.texture = a.preview,
			.clip_rect = area,
			.corner_radius = sty.corner_radius,
		});

		const rectf close = rectf::from_position_size(
			{ frame.right() - close_extent, frame.top() },
			{ close_extent, close_extent }
		);
		if (gui::caption_button(ui, close, std::format("##agent_attach_{}", i), gui::symbol::close(), sty.color_tab_hovered)) {
			removed = i;
		}

		x = frame.right() + pad;
	}

	if (removed < s.attachments.size()) {
		s.attachments.erase(s.attachments.begin() + static_cast<std::ptrdiff_t>(removed));
	}
}

auto gse::ide::agent::draw_input(gui::builder& ui, session& s, const rectf& area) -> void {
	const gui::draw_context& ctx = ui.ctx;
	const gui::style& sty = ctx.style;
	const float pad = sty.padding;

	const id input_id = gui::ids::make("##agent_input");
	const bool focused = ui.focus_widget_id == input_id;
	const bool ctrl = ctx.key_held(key::left_control) || ctx.key_held(key::right_control);
	const bool shift = ctx.key_held(key::left_shift) || ctx.key_held(key::right_shift);
	const bool enter = focused && !ctrl && !shift && ctx.key_pressed_for(key::enter);
	const bool submit = enter && (!input_empty(s.draft) || !s.attachments.empty());

	if (focused && ctrl && ctx.key_pressed(key::v)) {
		window::request_clipboard_image();
	}

	ctx.queue_sprite({
		.rect = area,
		.color = sty.color_input_background,
		.texture = ctx.blank_texture,
	});

	const auto code_view = ctx.fonts.code.resolve();
	constexpr std::string_view marker = ">";
	const float marker_width = code_view->width(marker, sty.font_size) + pad;
	const float line_h = gui::draw::text_area_line_height(ctx, ctx.fonts.code);
	const float first_row_center = area.top() - pad - line_h * 0.5f;

	ctx.queue_text({
		.font = ctx.fonts.code,
		.text = marker,
		.position = { area.left() + pad, first_row_center + code_view->vertical_center_offset(sty.font_size) },
		.scale = sty.font_size,
		.color = sty.color_accent,
		.clip_rect = area,
	});

	const float button_extent = sty.font_size * 1.4f;
	const rectf stop = rectf::from_position_size(
		{ area.right() - pad - button_extent, first_row_center + button_extent * 0.5f },
		{ button_extent, button_extent }
	);
	const rectf box = rectf::from_position_size(
		{ area.left() + pad + marker_width, area.top() },
		{ std::max(0.f, area.width() - pad * 3.f - marker_width - button_extent), area.height() }
	);
	gui::draw::text_area_in_rect(
		ctx,
		input_id,
		{
			.buffer = s.draft,
			.state = s.draft_state,
			.rect = box,
			.font = ctx.fonts.code,
		},
		ui.hot_widget_id,
		ui.focus_widget_id
	);

	const bool busy = s.running && s.think_clock.has_value();

	if (gui::caption_button(ui, stop, "##agent_stop", gui::symbol::stop(), sty.color_tab_hovered, busy) && busy) {
		interrupt_session(s);
	}

	if (!submit) {
		return;
	}

	const std::string prompt = input_text(s.draft);
	const std::vector<attachment> attachments = std::move(s.attachments);
	reset_input(s);

	if (!s.running && !launch_session(s)) {
		append_row(s, {
			.kind = row_kind::failure,
			.text = "failed to launch 'claude' - is it on PATH?",
		});
		return;
	}

	send_to_session(s, prompt, attachments);
}

auto gse::ide::agent::context_window_for(const session_info& info) -> std::int64_t {
	const bool large = info.model.contains("[1m]")
		|| info.model.contains("opus-5")
		|| info.context_used > base_context_window;
	return large ? large_context_window : base_context_window;
}

auto gse::ide::agent::message_tokens(session& s) -> std::int64_t {
	if (s.counted_rows > s.rows.size()) {
		s.counted_rows = 0;
		s.message_chars = 0;
	}
	for (; s.counted_rows < s.rows.size(); ++s.counted_rows) {
		const transcript_row& row = s.rows[s.counted_rows];
		if (row.kind == row_kind::user || row.kind == row_kind::text) {
			s.message_chars += static_cast<std::int64_t>(row.text.size());
		}
	}
	return static_cast<std::int64_t>(static_cast<float>(s.message_chars) / chars_per_token);
}

auto gse::ide::agent::draw_context_bar(const gui::draw_context& ctx, session& s, const rectf& area) -> void {
	const gui::style& sty = ctx.style;

	ctx.queue_sprite({
		.rect = area,
		.color = sty.color_widget_background,
		.texture = ctx.blank_texture,
	});

	const std::int64_t used = s.info.context_used;
	if (used <= 0) {
		return;
	}

	const std::int64_t window = context_window_for(s.info);
	const std::int64_t system = std::min(s.info.context_base, used);
	const std::int64_t messages = std::min(message_tokens(s), used - system);
	const std::array<std::pair<std::int64_t, vec4f>, 3> slices = { {
		{ system, sty.color_file },
		{ messages, sty.color_accent },
		{ used - system - messages, sty.color_folder },
	} };

	float x = area.left();
	for (const auto& [tokens, color] : slices) {
		const float width = std::min(
			area.width() * static_cast<float>(tokens) / static_cast<float>(window),
			area.right() - x
		);
		if (width <= 0.f) {
			continue;
		}
		ctx.queue_sprite({
			.rect = rectf::from_position_size({ x, area.top() }, { width, area.height() }),
			.color = color,
			.texture = ctx.blank_texture,
			.clip_rect = area,
		});
		x += width;
	}
}

auto gse::ide::agent::activity_label(const session& s) -> std::string {
	const time subsecond_limit = seconds(10.f);
	const time elapsed = s.think_clock->elapsed();
	return elapsed < subsecond_limit
		? std::format("{} \xC2\xB7 {:.1f:s}", s.action, elapsed)
		: std::format("{} \xC2\xB7 {:.0f:s}", s.action, elapsed);
}

auto gse::ide::agent::draw_activity(const gui::draw_context& ctx, const session& s, const rectf& area) -> void {
	const gui::style& sty = ctx.style;
	const auto text_view = ctx.fonts.text.resolve();
	const float pad = sty.padding;
	const float spin_extent = sty.font_size;

	ctx.queue_sprite({
		.rect = area,
		.color = sty.color_panel_alt,
		.texture = ctx.blank_texture,
	});

	const rectf spin = rectf::from_position_size(
		{ area.left() + pad, area.center().y() + spin_extent * 0.5f },
		{ spin_extent, spin_extent }
	);
	gui::symbol::spinner(ctx, spin, gui::symbol::spinner_rotation(), {
		.color = sty.color_accent,
		.extent = sty.icon_extent,
		.clip_rect = area,
	});

	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = activity_label(s),
		.position = { spin.right() + pad * 0.5f, area.center().y() + text_view->vertical_center_offset(sty.font_size) },
		.scale = sty.font_size,
		.color = sty.color_text_secondary,
		.clip_rect = area,
	});
}

auto gse::ide::agent::draw_panel(gui::builder& ui, data& d, const vec2f mouse, const channel_write<gui::menu_content, jump_to_request, set_cursor_shape_request> jump_out) -> void {
	const gui::draw_context& ctx = ui.ctx;
	if (ctx.clip_stack.empty()) {
		return;
	}

	const gui::style& sty = ctx.style;
	const rectf body = ctx.clip_stack.back();
	const float strip_h = draw_session_tabs(ui, d, body);
	session* shown = active_session(d);
	const float input_line_h = gui::draw::text_area_line_height(ctx, ctx.fonts.code);
	const auto input_rows = static_cast<float>(std::clamp<std::size_t>(shown ? shown->draft.line_count() : 1, 1, max_input_rows));
	const float input_h = std::max(sty.font_size * 2.f, input_rows * input_line_h + sty.padding * 2.f);
	const float attachments_h = !shown || shown->attachments.empty() ? 0.f : sty.font_size * 4.5f;
	const float activity_h = shown && shown->think_clock ? sty.font_size * 1.75f : 0.f;

	const float context_h = std::max(2.f, std::round(3.f * sty.scale_factor));

	const rectf context_area = rectf::from_position_size({ body.left(), body.bottom() + context_h }, { body.width(), context_h });
	const rectf input_area = rectf::from_position_size({ body.left(), context_area.top() + input_h }, { body.width(), input_h });
	const rectf attachments_area = rectf::from_position_size(
		{ body.left(), input_area.top() + attachments_h },
		{ body.width(), attachments_h }
	);
	const rectf activity_area = rectf::from_position_size(
		{ body.left(), attachments_area.top() + activity_h },
		{ body.width(), activity_h }
	);
	const rectf transcript = rectf::from_position_size(
		{ body.left(), body.top() - strip_h },
		{ body.width(), std::max(0.f, body.height() - strip_h - context_h - input_h - attachments_h - activity_h) }
	);

	if (attachments_h > 0.f) {
		draw_attachments(ui, *shown, attachments_area);
	}
	if (activity_h > 0.f) {
		draw_activity(ctx, *shown, activity_area);
	}
	if (shown) {
		draw_input(ui, *shown, input_area);
		draw_context_bar(ctx, *shown, context_area);
	}
	draw_transcript(ui, d, transcript, mouse, jump_out);
	draw_session_info(ctx, d, body);
	draw_history(ui, d, body);
	draw_close_confirm(ui, d, body);
}

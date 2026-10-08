module gse.ide.agent:panel_impl;

import gse;
import gse.ide.config;
import gse.ide.navigation;
import std;

import :blame;
import :chats;
import :layout;
import :model;
import :panel;
import :phase;
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
	s.attachments = {};
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

auto gse::ide::agent::agent_context_tag() -> id {
	return find_or_generate_id("agent_transcript_context");
}

auto gse::ide::agent::local_time_label(const std::int64_t unix_seconds) -> std::string {
	const std::time_t stamp = unix_seconds;
	const std::tm* local = std::localtime(&stamp);
	if (!local) {
		return {};
	}

	std::array<char, 32> out{};
	const std::size_t written = std::strftime(out.data(), out.size(), "%Y-%m-%d %H:%M:%S", local);
	return std::string(out.data(), written);
}

auto gse::ide::agent::usage_label(const usage_window& window) -> std::string {
	const std::int64_t remaining = window.resets_at - unix_now();
	if (remaining <= 0) {
		return std::format("{:.0f}%", window.utilization);
	}
	return remaining >= 3600
		? std::format("{:.0f}% \xC2\xB7 resets in {}h {}m", window.utilization, remaining / 3600, remaining % 3600 / 60)
		: std::format("{:.0f}% \xC2\xB7 resets in {}m", window.utilization, remaining / 60);
}

auto gse::ide::agent::tool_output_label(const session_info& info) -> std::string {
	if (info.tool_bytes <= bytes(0.)) {
		return "-";
	}

	const auto size_label = [](const byte_count size) {
		if (size >= mebibytes(1.0)) {
			return std::format("{:.1f:MiB}", size);
		}
		if (size >= kibibytes(1.0)) {
			return std::format("{:.0f:KiB}", size);
		}
		return std::format("{:.0f:B}", size);
	};

	if (info.tool_peak <= bytes(0.)) {
		return size_label(info.tool_bytes);
	}
	return std::format("{} \xC2\xB7 biggest {} from {}", size_label(info.tool_bytes), size_label(info.tool_peak), info.tool_peak_name);
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
	std::vector<std::pair<std::string, std::string>> rows = {
		{ "status", !link.empty() ? link : !s->info.failure.empty() ? s->info.failure : s->running ? "running" : "exited" },
		{ "activity", status_label(d, *s) },
		{ "my edits", exe_label(*s) },
		{ "broke", blame_label(*s) },
		{ "model", s->info.model.empty() ? "-" : s->info.model },
		{ "session", s->info.agent_id.empty() ? "-" : s->info.agent_id },
		{ "turns", std::format("{}", s->info.turns) },
		{ "api time", std::format("{:.1f}s", s->info.api_time.as<seconds>()) },
		{ "api equivalent", std::format("${:.4f}", s->info.cost) },
		{ "tool output", tool_output_label(s->info) },
	};

	if (const std::size_t asleep = hibernating_count(d); asleep > 0) {
		rows.emplace_back("observers", std::format("{} chat(s) waiting for a build", asleep));
	}

	rows.emplace_back("account usage", d.usage.windows.empty() && d.usage.error.empty() ? "fetching..." : d.usage.error);
	for (const usage_window& window : d.usage.windows) {
		rows.emplace_back(window.label, usage_label(window));
	}

	if (d.sessions.size() > 1) {
		rows.emplace_back("all chats", "");
		for (const session& other : d.sessions) {
			rows.emplace_back(
				other.name.empty() ? std::string("unnamed") : other.name,
				other.stale ? status_label(d, other) + " \xC2\xB7 unbuilt" : status_label(d, other)
			);
		}
	}

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
	const auto _ = ctx.scoped_layer(render_layer::popup);

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

	const std::array<rectf, 1> keep_open = { d.info_anchor };
	if (gui::interaction::dismissed_by_outside_press(ctx, {
		.body = panel,
		.keep_open = keep_open,
	})) {
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
	const auto _ = ctx.scoped_layer(render_layer::popup);

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
			if (c.clicked_in_rect(row)) {
				chosen = &chat;
			}
		}
	});

	if (chosen) {
		restore_chat(d, *chosen);
		d.history_open = false;
		d.overview_active = false;
		return;
	}

	const vec2f mouse = ctx.mouse_position();
	if (ctx.mouse_pressed() && !panel.contains(mouse) && !d.history_anchor.contains(mouse)) {
		d.history_open = false;
	}
}

auto gse::ide::agent::session_tab_id(const std::uint32_t session_id) -> id {
	return gui::ids::make(std::format("##agent_tab_{}", session_id));
}

auto gse::ide::agent::overview_tab_id() -> id {
	return gui::ids::make("##agent_tab_overview");
}

auto gse::ide::agent::overview_task(const session& s) -> std::string {
	if (s.hibernating && !s.wake_prompt.empty()) {
		return "on wake: " + std::string(first_line(s.wake_prompt));
	}

	for (const transcript_row& row : s.rows | std::views::reverse) {
		if (row.kind != row_kind::user) {
			continue;
		}
		if (const std::string_view head = first_line(row.text); !head.empty() && !head.starts_with('<')) {
			return std::string(head);
		}
	}
	return {};
}

auto gse::ide::agent::draw_overview(gui::builder& ui, data& d, const rectf& area) -> void {
	gui::draw_context& ctx = ui.ctx;
	const gui::style& sty = ctx.style;
	const auto text_view = ctx.fonts.text.resolve();
	const float pad = sty.padding;
	const float fs = sty.font_size;
	const float line_h = text_view->line_height(fs) * 1.3f;
	const float _ = line_h * 3.f + pad;

	if (d.sessions.empty()) {
		constexpr std::string_view empty = "No agents yet - open a chat with the + tab";
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = empty,
			.position = {
				area.center().x() - text_view->width(empty, fs) * 0.5f,
				area.center().y() + text_view->vertical_center_offset(fs),
			},
			.scale = fs,
			.color = sty.color_text_secondary,
			.clip_rect = area,
		});
		return;
	}

	const rectf list = rectf::from_position_size(
		{ area.left() + pad, area.top() - pad },
		{ area.width() - pad * 2.f, std::max(0.f, area.height() - pad * 2.f) }
	);
	ctx.layout_cursor = { list.left(), list.top() };

	std::uint32_t chosen = 0;
	std::string cancelled;
	std::string forced;
	ui.scroll_region({
		.id = "##agent_overview_list",
		.size = list.size(),
	}, [&](gui::builder& b) {
		gui::draw_context& c = b.ctx;
		const vec2f mouse = c.mouse_position();
		const rectf clip = c.current_clip().value_or(list);

		const auto next_row = [&](const float height) {
			const rectf row = rectf::from_position_size(
				{ list.left(), c.layout_cursor.y() },
				{ list.width(), height }
			);
			c.layout_cursor.y() -= height;
			return row;
		};

		const auto draw_build_row = [&](const queued_build& queued, const bool active) {
			const rectf row = next_row(line_h * 2.f + pad * 1.5f);
			if (!row.intersects(clip)) {
				return;
			}

			c.queue_sprite({
				.rect = row,
				.color = sty.color_panel_alt,
				.texture = c.blank_texture,
				.clip_rect = clip,
				.corner_radius = sty.corner_radius,
			});

			const std::string label = queue_label(queued);
			const std::string elapsed = std::format("{:.0f:s}", system_clock::now<time>() - queued.requested);
			const float text_left = row.left() + pad;
			float baseline = row.top() - pad * 0.75f - line_h * 0.5f + text_view->vertical_center_offset(fs);

			c.queue_text({
				.font = c.fonts.text,
				.text = label,
				.position = { text_left, baseline },
				.scale = fs,
				.color = sty.color_text,
				.clip_rect = clip,
			});
			c.queue_text({
				.font = c.fonts.text,
				.text = elapsed,
				.position = { row.right() - pad - text_view->width(elapsed, fs), baseline },
				.scale = fs,
				.color = sty.color_text_secondary,
				.clip_rect = clip,
			});

			const float controls_top = baseline - text_view->vertical_center_offset(fs) - line_h * 0.5f;
			baseline -= line_h;

			const std::string status = active ? std::string("building") : hold_label(d, queued);
			c.queue_text({
				.font = c.fonts.text,
				.text = status,
				.position = { text_left, baseline },
				.scale = fs,
				.color = active ? sty.color_folder : sty.color_warning,
				.clip_rect = clip,
			});

			c.queue_sprite({
				.rect = rectf::from_position_size({ row.left(), row.bottom() }, { row.width(), 1.f }),
				.color = sty.color_separator,
				.texture = c.blank_texture,
				.clip_rect = clip,
			});

			if (active) {
				return;
			}

			constexpr std::string_view cancel_text = "Cancel";
			constexpr std::string_view force_text = "Build now";
			const float cancel_w = text_view->width(cancel_text, fs) + pad * 2.f;
			const float force_w = text_view->width(force_text, fs) + pad * 2.f;
			const rectf cancel_rect = rectf::from_position_size({ row.right() - pad - cancel_w, controls_top }, { cancel_w, line_h });
			const rectf force_rect = rectf::from_position_size({ cancel_rect.left() - pad * 0.5f - force_w, controls_top }, { force_w, line_h });

			if (b.draw<gui::button>({
				.text = force_text,
				.rect = force_rect,
				.key = std::format("##agent_queue_force_{}", queued.id),
			})) {
				forced = queued.id;
			}
			if (b.draw<gui::button>({
				.text = cancel_text,
				.rect = cancel_rect,
				.key = std::format("##agent_queue_cancel_{}", queued.id),
				.role = gui::button_role::danger,
			})) {
				cancelled = queued.id;
			}
		};

		for (const queued_build& queued : d.inbox_active) {
			draw_build_row(queued, true);
		}
		for (const queued_build& queued : d.inbox_queue) {
			draw_build_row(queued, false);
		}

		for (const session& s : d.sessions) {
			const std::string task = overview_task(s);
			const bool busy = is_busy(s) && build_wait_for(d, s) == build_wait::none;

			std::string footer = s.info.model;
			if (s.stale || !s.blame.empty()) {
				footer += footer.empty() ? exe_label(s) : " \xC2\xB7 " + exe_label(s);
			}
			if (!s.blame.empty()) {
				footer += " \xC2\xB7 broke " + blame_label(s);
			}

			const float text_lines = 2.f + (busy || !task.empty() ? 1.f : 0.f) + (footer.empty() ? 0.f : 1.f);

			const rectf row = next_row(text_lines * line_h + pad * 1.5f);
			if (!row.intersects(clip)) {
				continue;
			}

			const bool over = clip.contains(mouse) && c.hovers(row);
			if (over || s.id == d.active) {
				c.queue_sprite({
					.rect = row,
					.color = over ? sty.color_widget_hovered : sty.color_panel_alt,
					.texture = c.blank_texture,
					.clip_rect = clip,
					.corner_radius = sty.corner_radius,
				});
			}

			const std::string status = status_label(d, s);
			const float status_w = text_view->width(status, fs);
			const float text_left = row.left() + pad;
			const float status_left = row.right() - pad - status_w;
			float baseline = row.top() - pad * 0.75f - line_h * 0.5f + text_view->vertical_center_offset(fs);

			const rectf body_clip = clip.intersection(rectf::from_position_size(
				{ text_left, row.top() },
				{ std::max(0.f, row.width() - pad * 2.f), row.height() }
			));
			const rectf name_clip = body_clip.intersection(rectf::from_position_size(
				{ text_left, row.top() },
				{ std::max(0.f, status_left - pad - text_left), row.height() }
			));

			c.queue_text({
				.font = c.fonts.text,
				.text = s.name.empty() ? std::string_view("unnamed") : std::string_view(s.name),
				.position = { text_left, baseline },
				.scale = fs,
				.color = s.running ? sty.color_text : sty.color_text_secondary,
				.clip_rect = name_clip,
			});
			c.queue_text({
				.font = c.fonts.text,
				.text = status,
				.position = { status_left, baseline },
				.scale = fs,
				.color = sty.*style_of(state_of(d, s)).color,
				.clip_rect = clip,
			});

			baseline -= line_h;
			draw_phase_pipeline(c, s, rectf::from_position_size(
				{ text_left, baseline - text_view->vertical_center_offset(fs) + line_h * 0.5f },
				{ std::max(0.f, row.width() - pad * 2.f), line_h }
			), body_clip);

			if (busy) {
				baseline -= line_h;
				draw_activity_line(c, d, s, rectf::from_position_size(
					{ text_left, baseline - text_view->vertical_center_offset(fs) + line_h * 0.5f },
					{ std::max(0.f, row.width() - pad * 2.f), line_h }
				), body_clip);
			}
			else if (!task.empty()) {
				baseline -= line_h;
				c.queue_text({
					.font = c.fonts.text,
					.text = task,
					.position = { text_left, baseline },
					.scale = fs,
					.color = sty.color_text_secondary,
					.clip_rect = body_clip,
				});
			}
			if (!footer.empty()) {
				baseline -= line_h;
				c.queue_text({
					.font = c.fonts.text,
					.text = footer,
					.position = { text_left, baseline },
					.scale = fs,
					.color = !s.blame.empty() ? sty.color_error : s.stale ? sty.color_warning : sty.color_text_secondary,
					.clip_rect = body_clip,
				});
			}

			c.queue_sprite({
				.rect = rectf::from_position_size({ row.left(), row.bottom() }, { row.width(), 1.f }),
				.color = sty.color_separator,
				.texture = c.blank_texture,
				.clip_rect = clip,
			});

			if (c.clicked_in_rect(row)) {
				chosen = s.id;
			}
		}
	});

	if (!cancelled.empty()) {
		cancel_queued(d, cancelled);
	}
	if (!forced.empty()) {
		force_queued(d, forced);
	}

	if (chosen != 0) {
		d.active = chosen;
		d.overview_active = false;
	}
}

auto gse::ide::agent::draw_session_tabs(gui::builder& ui, data& d, const rectf& body) -> float {
	const gui::draw_context& ctx = ui.ctx;
	const gui::style& sty = ctx.style;
	const float pad = sty.padding;

	std::vector<gui::tab_desc> descs;
	descs.reserve(d.sessions.size() + 1);
	descs.push_back({
		.tab_id = overview_tab_id(),
		.caption = "Agents",
		.dirty = hibernating_count(d) > 0,
		.closeable = false,
		.pinned = true,
	});
	for (const session& s : d.sessions) {
		descs.push_back({
			.tab_id = session_tab_id(s.id),
			.caption = s.name,
			.dirty = s.hibernating || s.gate.has_value() || build_wait_for(d, s) != build_wait::none,
			.busy = is_busy(s),
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
		.active = d.overview_active ? overview_tab_id() : session_tab_id(d.active),
		.allow_reorder = true,
		.show_add = true,
		.renaming = d.renaming != 0 ? session_tab_id(d.renaming) : id{},
	}, d.tab_strip);

	const auto session_of = [&](const id tab_id) -> session* {
		const auto found = std::ranges::find_if(d.sessions, [&](const session& s) {
			return session_tab_id(s.id) == tab_id;
		});
		return found == d.sessions.end() ? nullptr : &*found;
	};

	if (tabs.activated.exists() && tabs.activated == overview_tab_id()) {
		d.overview_active = true;
	}
	else if (session* activated = tabs.activated.exists() ? session_of(tabs.activated) : nullptr) {
		d.overview_active = false;
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
		const std::size_t dropped_at = tabs.reorder_to > 0 ? tabs.reorder_to - 1 : 0;
		const auto to = d.sessions.begin() + static_cast<std::ptrdiff_t>(std::min(dropped_at, d.sessions.size() - 1));
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
		d.naming_new_chat = true;
		d.new_chat_name.clear();
		d.new_chat_name_state = {};
	}

	session* renaming = d.renaming != 0 && tabs.renaming_rect.width() > 0.f ? session_of(session_tab_id(d.renaming)) : nullptr;
	if (renaming) {
		const id input_id = gui::ids::make(std::format("##agent_name_{}", renaming->id));
		ui.draw<gui::text_input>({
			.buffer = renaming->name,
			.state = renaming->name_state,
			.rect = tabs.renaming_rect,
			.widget_id = input_id,
			.font = ctx.fonts.text,
		});

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

	const gui::confirm_result result = ui.draw<gui::confirm_dialog>({
		.body = body,
		.title = "Stop the agent and close the tab?",
		.message = std::format("\"{}\" is still running, and its transcript will be discarded.", closing->name),
		.confirm_label = "Close",
		.key = "##agent_close",
	});

	if (result == gui::confirm_result::confirmed) {
		erase_session(d, d.pending_close);
	}
	else if (result == gui::confirm_result::cancelled) {
		d.pending_close = 0;
	}
}

auto gse::ide::agent::draw_new_chat_prompt(gui::builder& ui, data& d, const rectf& body) -> void {
	if (!d.naming_new_chat) {
		return;
	}

	const gui::prompt_result result = ui.draw<gui::prompt_dialog>({
		.body = body,
		.title = "Name this task",
		.message = "The chat scopes it first, and you approve the plan before anything is written.",
		.value = d.new_chat_name,
		.state = d.new_chat_name_state,
		.submit_label = "Start",
		.key = "##agent_new_chat",
	});

	if (result == gui::prompt_result::pending) {
		return;
	}

	d.naming_new_chat = false;
	if (result == gui::prompt_result::submitted) {
		session& started = create_session(d, config::primary().project_root, task_phase::scope);
		started.name = d.new_chat_name;
		d.overview_active = false;
	}
	d.new_chat_name.clear();
	d.new_chat_name_state = {};
}

auto gse::ide::agent::draw_transcript_hint(const gui::draw_context& ctx, const std::string& hint, const rectf& area) -> void {
	const gui::style& sty = ctx.style;
	const float pad = sty.padding;

	float y = area.top() - pad - sty.font_size;
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
}

auto gse::ide::agent::draw_transcript(gui::builder& ui, data& d, transcript_view& v, const rectf& area, const channel_write<gui::menu_content, jump_to_request, set_cursor_shape_request> jump_out) -> void {
	const gui::draw_context& ctx = ui.ctx;
	const vec2f mouse = ctx.mouse_position();
	const gui::style& sty = ctx.style;
	const float pad = sty.padding;

	session* s = active_session(d);
	if (!s) {
		return;
	}
	if (!std::ranges::any_of(s->rows, [&](const transcript_row& row) { return shows(v.filter, row); })) {
		draw_transcript_hint(ctx, v.filter == row_filter::changes
			? std::string("No file edits yet.")
			: "Type a prompt below to start claude in " + s->cwd.generic_display_string() + ".", area);
		return;
	}

	const auto code_view = ctx.fonts.code.resolve();
	const auto body_view = ctx.fonts.text.resolve();
	const float advance = code_view->width("0", sty.font_size);
	const transcript_metrics metrics = {
		.fonts = ctx.fonts,
		.face = *code_view,
		.body = *body_view,
		.width = std::max(0.f, area.width() - pad * 2.f - gui::scroll_config{}.scrollbar_width),
		.scale = sty.font_size,
	};
	sync_transcript(*s, v, sty, metrics);

	if (v.buffer.lines.empty()) {
		v.buffer.lines.emplace_back();
		v.line_rows.push_back(0);
	}

	const gui::interaction::press tail_press = ui.draw<gui::follow_tail>({
		.area = area,
		.state = v.state,
		.widget_id = generate_temp_id(hash_combine(v.log_id.number(), stable_id("agent_follow_tail"))),
	});

	const gui::buffer_position at = gui::text_area_position_at(ctx, {
		.buffer = v.buffer,
		.state = v.state,
		.rect = area,
		.spans = v.spans,
		.stops = v.stops,
		.blocks = v.blocks,
		.indent_width = transcript_tab_width,
	}, mouse);
	const auto hovered = static_cast<std::uint32_t>(std::min<std::size_t>(at.line, v.line_rows.size() - 1));
	const std::string_view hovered_text = v.buffer.line(hovered);
	const link_marker* hit = ctx.hovers(area) && !tail_press.hovered && at.column < hovered_text.size() ? link_at(v, hovered) : nullptr;
	const std::optional<std::uint32_t> link = hit ? std::optional(hit->row) : std::nullopt;
	const group_marker* toggle = ctx.hovers(area) && !tail_press.hovered ? marker_at(v, hovered) : nullptr;

	std::vector<gui::text_underline> underlines;
	if (link || toggle) {
		const std::size_t start = hovered_text.find_first_not_of(' ');
		underlines.push_back({
			.line = hovered,
			.start_col = static_cast<std::uint32_t>(start == std::string_view::npos ? 0 : start),
			.end_col = static_cast<std::uint32_t>(hovered_text.size()),
			.color = link ? sty.color_file : sty.color_accent,
		});
		jump_out.push<set_cursor_shape_request>({
			.shape = cursor_shape::hand,
		});
	}

	const auto context_row = v.line_rows[hovered];
	if (const group_marker* group = toggle ? group_at(v, hovered) : nullptr) {
		ctx.set_tooltip(gui::ids::make(std::format("##agent_group_{}_{}", s->id, group->row)), group_detail(*s, *group));
	}
	else if (ctx.hovers(area) && !tail_press.hovered && context_row < s->rows.size() && !hovered_text.empty() && s->rows[context_row].stamped > 0) {
		ctx.set_tooltip(gui::ids::make(std::format("##agent_row_{}_{}", s->id, context_row)), local_time_label(s->rows[context_row].stamped));
	}
	const bool context_mine = ctx.hovers(area) && context_row < s->rows.size() && s->rows[context_row].kind == row_kind::user;
	const std::optional<std::uint32_t> rewind = context_mine ? rewind_anchor(*s, context_row) : std::nullopt;

	if (rewind && ctx.mouse_pressed_for(area, mouse_button::button_2)) {
		ctx.open_context_menu({
			.position = mouse,
			.items = {
				{
					.label = "Rewind to this prompt",
					.action_id = 0,
					.destructive = true,
				},
			},
			.target = (static_cast<std::uint64_t>(s->id) << 32) | context_row,
			.tag = agent_context_tag(),
		});
	}

	if (ctx.clicked_in_rect(area)) {
		if (toggle) {
			toggle_marker(*s, v, *toggle);
			sync_transcript(*s, v, sty, metrics);
		}
		else if (link) {
			const transcript_row& owner = s->rows[std::min<std::size_t>(*link, s->rows.size() - 1)];
			const bool edit = !owner.added.empty() || !owner.removed.empty();

			if (edit && v.filter != row_filter::changes) {
				s->changes_open = true;
				s->changes_focus = *link;
			}
			else {
				const std::uint32_t first = jump_line_for(owner);
				jump_out.push<jump_to_request>({
					.path = owner.file,
					.line = first,
					.column = 0,
					.end_line = owner.added.empty() ? first : first + static_cast<std::uint32_t>(owner.added.size()) - 1,
					.end_column = ~0u,
				});
			}
		}
	}

	const std::optional<std::uint32_t> stale_diff = update_diff_scroll(ctx, v, area, advance);

	ui.draw<gui::text_area>({
		.buffer = v.buffer,
		.state = v.state,
		.widget_id = v.log_id,
		.spans = v.spans,
		.underlines = underlines,
		.blocks = v.blocks,
		.stops = v.stops,
		.rules = v.rules,
		.rect = area,
		.read_only = true,
		.follow_tail = true,
		.indent_width = transcript_tab_width,
		.blink_interval = time{},
	});

	if (stale_diff) {
		relayout_from(v, *stale_diff);
	}

	if (v.filter == row_filter::changes && s->changes_focus) {
		const auto found = std::ranges::find(v.line_rows, *s->changes_focus);
		if (found != v.line_rows.end()) {
			const gui::text_area_layout placement = gui::text_area_layout_of(ctx, {
				.buffer = v.buffer,
				.state = v.state,
				.rect = area,
				.spans = v.spans,
				.stops = v.stops,
				.blocks = v.blocks,
				.indent_width = transcript_tab_width,
			});
			v.state.scroll.y.offset = placement.line_top(static_cast<std::uint32_t>(std::distance(v.line_rows.begin(), found)));
			v.state.scroll.y.target = v.state.scroll.y.offset;
			v.state.tail_pinned = false;
		}
		s->changes_focus.reset();
	}
}

auto gse::ide::agent::draw_model_controls(gui::builder& ui, session& s, const rectf& model_rect, const rectf& effort_rect) -> void {
	const gui::draw_context& ctx = ui.ctx;

	const std::span<const model_option> catalogue = available_models();
	std::vector<std::string_view> models;
	models.reserve(catalogue.size() + 1);
	models.emplace_back("default");
	std::size_t current = 0;
	for (const model_option& option : catalogue) {
		if (option.value == s.model_id) {
			current = models.size();
		}
		models.push_back(option.label);
	}

	const gui::dropdown_result picked = ui.draw<gui::dropdown<std::string_view>>({
		.name = "##agent_model",
		.current_index = current,
		.options = models,
		.state = s.model_dropdown,
		.rect = model_rect,
	});

	const std::span<const agent_effort> levels = enum_values<agent_effort>();
	auto level = static_cast<int>(s.requested_effort);
	const std::string_view effort_text = s.requested_effort == agent_effort::inherit
		? std::string_view("auto")
		: enum_to_string(s.requested_effort);

	ui.draw<gui::slider<int>>({
		.name = "##agent_effort",
		.value = level,
		.min = 0,
		.max = static_cast<int>(levels.size()) - 1,
		.rect = effort_rect,
		.value_label = effort_text,
	});

	if (picked.changed && picked.new_index < models.size()) {
		s.model_id = picked.new_index == 0 ? std::string{} : catalogue[picked.new_index - 1].value;
	}
	s.requested_effort = static_cast<agent_effort>(std::clamp<int>(level, 0, static_cast<int>(levels.size()) - 1));
}

auto gse::ide::agent::draw_input(gui::builder& ui, session& s, const input_layout& layout) -> void {
	const rectf& area = layout.area;
	const rectf& body = layout.body;
	const gui::draw_context& ctx = ui.ctx;
	const gui::style& sty = ctx.style;
	const float pad = sty.padding;

	const id input_id = gui::ids::make("##agent_input");
	const bool focused = ui.focus_widget_id == input_id;
	const bool ctrl = ctx.key_held(key::left_control) || ctx.key_held(key::right_control);
	const bool shift = ctx.key_held(key::left_shift) || ctx.key_held(key::right_shift);
	const bool enter = focused && !s.attachments.viewing && !ctrl && !shift && ctx.key_pressed_for(key::enter);
	const bool submit = enter && (!input_empty(s.draft) || !s.attachments.items.empty());

	ctx.queue_sprite({
		.rect = area,
		.color = sty.color_input_background,
		.texture = ctx.blank_texture,
	});

	const auto code_view = ctx.fonts.code.resolve();
	const auto text_view = ctx.fonts.text.resolve();
	constexpr std::string_view marker = ">";
	const float marker_width = text_view->width(marker, sty.font_size) + pad;
	const float line_h = gui::text_area_line_height(ctx, ctx.fonts.text);
	const float first_row_center = area.top() - pad - line_h * 0.5f;

	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = marker,
		.position = { area.left() + pad, first_row_center + text_view->vertical_center_offset(sty.font_size) },
		.scale = sty.font_size,
		.color = sty.color_accent,
		.clip_rect = area,
	});

	const float button_extent = sty.font_size * 1.4f;
	const bool gated = s.gate.has_value() && !s.pending_transition;
	const phase_policy granted = gated ? policy_of(next_phase(s, s.gate->findings)) : phase_policy{};
	const std::string_view approve_text = granted.enter_label;
	const float action_width = gated ? text_view->width(approve_text, sty.font_size) + pad * 2.f : button_extent;
	const rectf stop = rectf::from_position_size(
		{ area.right() - pad - action_width, first_row_center + button_extent * 0.5f },
		{ action_width, button_extent }
	);

	float widest_model = text_view->width("default", sty.font_size);
	for (const model_option& option : available_models()) {
		widest_model = std::max(widest_model, text_view->width(option.label, sty.font_size));
	}
	float widest_effort = code_view->width("auto", sty.font_size);
	for (const agent_effort level : enum_values<agent_effort>()) {
		if (level != agent_effort::inherit) {
			widest_effort = std::max(widest_effort, code_view->width(enum_to_string(level), sty.font_size));
		}
	}

	const float model_width = widest_model + sty.font_size + pad * 3.f;
	const float effort_width = widest_effort + pad * 2.f;
	const float controls_width = model_width + effort_width + pad * 2.f;
	const float text_minimum = sty.font_size * 12.f;
	const bool show_controls = area.width() - pad * 3.f - marker_width - action_width - controls_width >= text_minimum;

	const rectf model_rect = rectf::from_position_size(
		{ stop.left() - pad - model_width, first_row_center + button_extent * 0.5f },
		{ model_width, button_extent }
	);
	const rectf effort_rect = rectf::from_position_size(
		{ model_rect.left() - pad - effort_width, first_row_center + button_extent * 0.5f },
		{ effort_width, button_extent }
	);

	const float reserved = pad * 3.f + marker_width + action_width + (show_controls ? controls_width : 0.f);
	const float strip_extent = gui::image_strip_extent(ctx, s.attachments);
	const rectf strip = rectf::from_position_size(
		{ area.left() + pad, area.bottom() + strip_extent },
		{ std::max(0.f, area.width() - pad * 2.f), strip_extent }
	);
	const rectf box = rectf::from_position_size(
		{ area.left() + pad + marker_width, area.top() },
		{ std::max(0.f, area.width() - reserved), std::max(0.f, area.height() - strip_extent) }
	);
	ui.draw<gui::text_area>({
		.buffer = s.draft,
		.state = s.draft_state,
		.widget_id = input_id,
		.rect = box,
		.images = gui::image_strip_params{
			.attachments = s.attachments,
			.strip = strip,
			.viewer_host = body,
		},
		.font = ctx.fonts.text,
	});

	const bool busy = is_busy(s);

	if (show_controls) {
		draw_model_controls(ui, s, model_rect, effort_rect);
	}

	if (gated) {
		if (ui.draw<gui::button>({
			.text = approve_text,
			.rect = stop,
			.key = std::format("##agent_phase_approve_{}", s.id),
		})) {
			advance_phase(s, input_text(s.draft));
			reset_input(s);
		}
	}
	else if (gui::caption_button(ui, stop, "##agent_stop", gui::symbol::stop(), sty.color_tab_hovered, busy) && busy) {
		interrupt_session(s);
	}

	if (!submit) {
		return;
	}

	const std::string prompt = input_text(s.draft);
	const std::vector<gui::image_attachment> attachments = s.attachments.take_items();
	reset_input(s);

	if (current_phase(s) == task_phase::settled) {
		restart_task(s);
	}

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

auto gse::ide::agent::activity_label(const data& d, const session& s) -> std::string {
	const time subsecond_limit = seconds(10.f);
	const time elapsed = s.think_clock->elapsed();
	const build_wait wait = build_wait_for(d, s);
	const std::string action = wait == build_wait::none ? s.action : status_label(d, s);
	return elapsed < subsecond_limit
		? std::format("{} \xC2\xB7 {:.1f:s}", action, elapsed)
		: std::format("{} \xC2\xB7 {:.0f:s}", action, elapsed);
}

auto gse::ide::agent::draw_activity_line(const gui::draw_context& ctx, const data& d, const session& s, const rectf& area, const rectf& clip) -> void {
	const gui::style& sty = ctx.style;
	const auto text_view = ctx.fonts.text.resolve();
	const float pad = sty.padding;
	const float spin_extent = sty.font_size;

	const rectf spin = rectf::from_position_size(
		{ area.left(), area.center().y() + spin_extent * 0.5f },
		{ spin_extent, spin_extent }
	);
	gui::symbol::spinner(ctx, spin, {
		.color = sty.color_accent,
		.extent = sty.icon_extent,
		.clip_rect = clip,
	});

	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = activity_label(d, s),
		.position = { spin.right() + pad * 0.5f, area.center().y() + text_view->vertical_center_offset(sty.font_size) },
		.scale = sty.font_size,
		.color = sty.color_text_secondary,
		.clip_rect = clip,
	});
}

auto gse::ide::agent::draw_activity(const gui::draw_context& ctx, const data& d, const session& s, const rectf& area) -> void {
	const gui::style& sty = ctx.style;
	const float pad = sty.padding;

	ctx.queue_sprite({
		.rect = area,
		.color = sty.color_panel_alt,
		.texture = ctx.blank_texture,
	});

	draw_activity_line(ctx, d, s, rectf::from_position_size(
		{ area.left() + pad, area.top() },
		{ std::max(0.f, area.width() - pad), area.height() }
	), area);
}

auto gse::ide::agent::draw_phase_pipeline(const gui::draw_context& ctx, const session& s, const rectf& area, const rectf& clip) -> float {
	const gui::style& sty = ctx.style;
	const auto text_view = ctx.fonts.text.resolve();
	const float fs = sty.font_size;
	const float baseline = area.center().y() + text_view->vertical_center_offset(fs);
	constexpr std::string_view separator = " \xC2\xB7 ";

	float x = area.left();
	bool first = true;
	for (const phase_step& step : phase_timeline(s)) {
		if (!first) {
			ctx.queue_text({
				.font = ctx.fonts.text,
				.text = separator,
				.position = { x, baseline },
				.scale = fs,
				.color = sty.color_text_disabled,
				.clip_rect = clip,
			});
			x += text_view->width(separator, fs);
		}
		first = false;

		const phase_policy policy = policy_of(step.phase);
		const std::string label = step.cycles > 1
			? std::format("{} x{}", std::string_view(policy.label), step.cycles)
			: std::string(policy.label);
		const float label_w = text_view->width(label, fs);

		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = label,
			.position = { x, baseline },
			.scale = fs,
			.color = step.current ? sty.*policy.color : step.visited ? sty.color_text_secondary : sty.color_text_disabled,
			.clip_rect = clip,
		});

		if (step.current) {
			ctx.queue_sprite({
				.rect = rectf::from_position_size(
					{ x, area.center().y() - fs * 0.5f },
					{ label_w, std::max(1.f, std::round(sty.scale_factor)) }
				),
				.color = sty.*policy.color,
				.texture = ctx.blank_texture,
				.clip_rect = clip,
			});
		}
		x += label_w;
	}

	return x - area.left();
}

auto gse::ide::agent::draw_phase(gui::builder& ui, session& s, const rectf& area) -> void {
	const gui::draw_context& ctx = ui.ctx;
	const gui::style& sty = ctx.style;
	const auto text_view = ctx.fonts.text.resolve();
	const float pad = sty.padding;
	const float fs = sty.font_size;

	ctx.queue_sprite({
		.rect = area,
		.color = sty.color_panel_alt,
		.texture = ctx.blank_texture,
	});

	const float reserved = draw_view_toggles(ui, s, area);
	const rectf pipeline = rectf::from_position_size(
		{ area.left() + pad, area.top() },
		{ std::max(0.f, area.width() - pad - reserved), area.height() }
	);
	const float used = draw_phase_pipeline(ctx, s, pipeline, pipeline);

	const std::string waiting = s.pending_transition
		? std::format("approved \xC2\xB7 finishing the turn before {}", std::string_view(policy_of(next_phase(s, s.pending_transition->findings)).label))
		: s.gate
		? s.gate->findings > 0
			? std::format("reported {} finding(s)", s.gate->findings)
			: std::string("reported done")
		: std::string{};
	if (waiting.empty()) {
		return;
	}

	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = waiting,
		.position = { pipeline.left() + used + pad * 2.f, area.center().y() + text_view->vertical_center_offset(fs) },
		.scale = fs,
		.color = sty.color_text_secondary,
		.clip_rect = pipeline,
	});
}

auto gse::ide::agent::shown_view(session& s) -> transcript_view& {
	return s.mode == transcript_mode::digest ? s.digest : s.full;
}

auto gse::ide::agent::draw_view_toggles(gui::builder& ui, session& s, const rectf& area) -> float {
	const gui::style& sty = ui.ctx.style;
	const float pad = sty.padding;
	const auto text_view = ui.ctx.fonts.text.resolve();

	constexpr std::string_view diffs_label = "Diffs";
	const std::string_view mode_label = s.mode == transcript_mode::digest ? "Summary" : "Transcript";

	const float diffs_w = text_view->width(diffs_label, sty.font_size) + pad * 2.f;
	const float mode_w = text_view->width("Transcript", sty.font_size) + pad * 2.f;
	const float height = std::min(area.height(), sty.font_size * 1.4f);
	const float top = area.center().y() + height * 0.5f;

	const rectf diffs = rectf::from_position_size({ area.right() - pad - diffs_w, top }, { diffs_w, height });
	const rectf mode = rectf::from_position_size({ diffs.left() - pad * 0.5f - mode_w, top }, { mode_w, height });

	if (ui.draw<gui::button>({
		.text = mode_label,
		.rect = mode,
		.key = std::format("##agent_view_mode_{}", s.id),
		.role = s.mode == transcript_mode::digest ? gui::button_role::accent : gui::button_role::ghost,
	})) {
		s.mode = s.mode == transcript_mode::digest ? transcript_mode::transcript : transcript_mode::digest;
	}

	if (ui.draw<gui::button>({
		.text = diffs_label,
		.rect = diffs,
		.key = std::format("##agent_view_diffs_{}", s.id),
		.role = s.changes_open ? gui::button_role::accent : gui::button_role::ghost,
	})) {
		s.changes_open = !s.changes_open;
	}

	return area.right() - mode.left() + pad;
}

auto gse::ide::agent::draw_panel(gui::builder& ui, data& d, const channel_write<gui::menu_content, jump_to_request, set_cursor_shape_request> jump_out) -> void {
	const gui::draw_context& ctx = ui.ctx;
	if (ctx.clip_stack.empty()) {
		return;
	}

	const gui::style& sty = ctx.style;
	const rectf body = ctx.clip_stack.back();
	const float strip_h = draw_session_tabs(ui, d, body);

	if (d.overview_active) {
		draw_overview(ui, d, rectf::from_position_size(
			{ body.left(), body.top() - strip_h },
			{ body.width(), std::max(0.f, body.height() - strip_h) }
		));
		draw_session_info(ctx, d, body);
		draw_history(ui, d, body);
		draw_close_confirm(ui, d, body);
		draw_new_chat_prompt(ui, d, body);
		return;
	}

	session* shown = active_session(d);
	const float input_line_h = gui::text_area_line_height(ctx, ctx.fonts.text);
	const auto input_rows = static_cast<float>(std::clamp<std::size_t>(shown ? shown->draft.line_count() : 1, 1, max_input_rows));
	const float attachments_h = shown ? gui::image_strip_extent(ctx, shown->attachments) : 0.f;
	const float input_h = std::max(sty.font_size * 2.f, input_rows * input_line_h + sty.padding * 2.f) + attachments_h;
	const float activity_h = shown && is_busy(*shown) ? sty.font_size * 1.75f : 0.f;
	const float phase_h = shown ? sty.font_size * 1.75f : 0.f;

	const float context_h = std::max(2.f, std::round(3.f * sty.scale_factor));

	const rectf context_area = rectf::from_position_size({ body.left(), body.bottom() + context_h }, { body.width(), context_h });
	const rectf input_area = rectf::from_position_size({ body.left(), context_area.top() + input_h }, { body.width(), input_h });
	const rectf activity_area = rectf::from_position_size(
		{ body.left(), input_area.top() + activity_h },
		{ body.width(), activity_h }
	);
	const rectf phase_area = rectf::from_position_size(
		{ body.left(), activity_area.top() + phase_h },
		{ body.width(), phase_h }
	);
	const rectf transcript = rectf::from_position_size(
		{ body.left(), body.top() - strip_h },
		{ body.width(), std::max(0.f, body.height() - strip_h - context_h - input_h - activity_h - phase_h) }
	);

	if (activity_h > 0.f) {
		draw_activity(ctx, d, *shown, activity_area);
	}
	if (shown) {
		draw_phase(ui, *shown, phase_area);
		draw_input(ui, *shown, { .area = input_area, .body = body });
		draw_context_bar(ctx, *shown, context_area);
	}

	if (!shown) {
		draw_transcript_hint(ctx, "No agents yet. Type a prompt below, or press + to add a session, in " + config::primary().name + ".", transcript);
	}
	else if (!shown->changes_open) {
		draw_transcript(ui, d, shown_view(*shown), transcript, jump_out);
	}
	else {
		const vec2f mouse = ctx.mouse_position();
		const float minimum = sty.font_size * 14.f;
		const float divider_thickness = std::max(6.f, sty.resize_border_thickness) * 2.f;
		const bool blocked = ctx.hit_regions && ctx.hit_regions->is_resize_blocked(mouse);
		const gui::layout::split_result split = gui::layout::update_split({
			.container = transcript,
			.axis = gui::layout::split_axis::columns,
			.ratio = shown->changes_ratio,
			.min_first = minimum,
			.min_second = minimum,
			.divider_thickness = divider_thickness,
		}, {
			.mouse = mouse,
			.pressed = ctx.mouse_pressed(mouse_button::button_1) && ctx.input_available(),
			.held = ctx.mouse_held(mouse_button::button_1),
			.blocked = blocked,
		}, shown->changes_drag);

		if (shown->changes_drag.dragging && transcript.width() >= minimum * 2.f + divider_thickness) {
			shown->changes_ratio = split.ratio;
		}

		if ((split.divider.contains(mouse) && !blocked) || shown->changes_drag.dragging) {
			jump_out.push<set_cursor_shape_request>({
				.shape = cursor_shape::resize_ew,
			});
		}

		draw_transcript(ui, d, shown_view(*shown), split.first, jump_out);
		draw_transcript(ui, d, shown->changes, split.second, jump_out);
	}
	draw_session_info(ctx, d, body);
	draw_history(ui, d, body);
	draw_close_confirm(ui, d, body);
	draw_new_chat_prompt(ui, d, body);
}
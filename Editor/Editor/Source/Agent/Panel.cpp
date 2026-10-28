module gse.ide.agent:panel_impl;

import gse;
import gse.ide.build;
import gse.ide.config;
import gse.ide.navigation;
import std;

import :blame;
import :canvas;
import :chats;
import :chrome;
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

auto gse::ide::agent::draw_session_info(gui::builder& ui, data& d, const rectf& body) -> void {
	const gui::draw_context& ctx = ui.ctx;
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
		{ "status", !link.empty() ? link : !s->info.failure.empty() ? s->info.failure : s->running ? "running" : "ready" },
		{ "activity", plan_chrome(d, *s).status },
		{ "my edits", exe_label(*s) },
		{ "broke", blame_label(*s) },
		{ "unclaimed", d.unclaimed.empty() ? std::string("-") : std::format("{} error(s) in {}", d.unclaimed.size(), d.unclaimed_tree) },
		{ "model", s->info.model.empty() ? "-" : s->info.model },
		{ "session", s->info.agent_id.empty() ? "-" : s->info.agent_id },
		{ "turns", std::format("{}", s->info.turns) },
		{ "api time", std::format("{:.1f:s}", s->info.api_time) },
		{ "api equivalent", std::format("${:.4f}", s->info.cost) },
		{ "tool output", tool_output_label(s->info) },
	};

	if (const std::size_t asleep = hibernating_count(d); asleep > 0) {
		rows.emplace_back("observers", std::format("{} chat(s) waiting for a build", asleep));
	}

	const gui::motion_stats motion = ctx.motion_counts();
	rows.emplace_back("ui motion", std::format("{} tracked \xC2\xB7 {} moving", motion.live, motion.animating));

	rows.emplace_back("account usage", d.usage.windows.empty() && d.usage.error.empty() ? "fetching..." : d.usage.error);
	for (const usage_window& window : d.usage.windows) {
		rows.emplace_back(window.label, usage_label(window));
	}

	if (std::ranges::count_if(d.sessions, is_chat) > 1) {
		rows.emplace_back("all chats", "");
		for (const session& other : d.sessions | std::views::filter(is_chat)) {
			rows.emplace_back(
				other.name.empty() ? std::string("unnamed") : other.name,
				other.stale ? plan_chrome(d, other).status + " \xC2\xB7 unbuilt" : plan_chrome(d, other).status
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
	const float ph = line_h * static_cast<float>(rows.size() + 1) + pad * 2.f;

	float px = d.info_anchor.left();
	if (px + pw > body.right()) {
		px = body.right() - pw;
	}
	px = std::max(px, body.left());
	const float top_y = std::min(body.top(), d.info_anchor.bottom() - pad * 0.5f);

	const rectf panel = rectf::from_position_size({ px, top_y }, { pw, ph });
	const auto _ = ctx.scoped_layer(render_layer::popup);

	gui::draw::fill_surface(
		ctx,
		rectf::from_position_size({ px + 4.f * sty.scale_factor, top_y - 4.f * sty.scale_factor }, { pw, ph }),
		sty.modal.shadow,
		{}
	);
	gui::draw::fill_surface(ctx, panel, sty.modal.panel, {});

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

	if (ui.draw<gui::button>({
		.text = "Change Claude token...",
		.rect = rectf::from_position_size({ px + pad, top_y - pad - line_h * static_cast<float>(rows.size()) }, { pw - pad * 2.f, line_h }),
		.key = "##agent_change_token",
		.role = gui::button_role::ghost,
	})) {
		d.info_open = false;
		d.token_prompt_open = true;
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

	gui::draw::fill_surface(
		ctx,
		rectf::from_position_size({ px + 4.f * sty.scale_factor, top_y - 4.f * sty.scale_factor }, { pw, ph }),
		sty.modal.shadow,
		{}
	);
	gui::draw::fill_surface(ctx, panel, sty.modal.panel, {});
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
					.color = sty.list_item.surfaces.hot.fill,
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

auto gse::ide::agent::overview_task(const chrome_plan& plan, const session& s) -> std::string {
	if (!plan.wake_task.empty()) {
		return plan.wake_task;
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

	if (std::ranges::none_of(d.sessions, is_chat)) {
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
				.color = sty.panel_alt.fill,
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
				.color = sty.separator_line.fill,
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

		for (const session& s : d.sessions | std::views::filter(is_chat)) {
			const chrome_plan plan = plan_chrome(d, s);
			const std::string task = overview_task(plan, s);
			const bool busy = plan.overview_activity;

			std::string footer = s.info.model;
			if (!plan.footer.empty()) {
				footer += footer.empty() ? plan.footer : " \xC2\xB7 " + plan.footer;
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
					.color = over ? sty.list_item.surfaces.hot.fill : sty.panel_alt.fill,
					.texture = c.blank_texture,
					.clip_rect = clip,
					.corner_radius = sty.corner_radius,
				});
			}

			const std::string& status = plan.status;
			const float status_w = text_view->width(status, fs);
			const float text_left = row.left() + pad;
			const float name_left = text_left + sty.icon_extent + pad * 0.5f;
			const float status_left = row.right() - pad - status_w;
			float baseline = row.top() - pad * 0.75f - line_h * 0.5f + text_view->vertical_center_offset(fs);

			const rectf body_clip = clip.intersection(rectf::from_position_size(
				{ text_left, row.top() },
				{ std::max(0.f, row.width() - pad * 2.f), row.height() }
			));
			const rectf name_clip = body_clip.intersection(rectf::from_position_size(
				{ name_left, row.top() },
				{ std::max(0.f, status_left - pad - name_left), row.height() }
			));

			const kind_style kind = style_of(kind_of(s));
			gui::symbol::draw(c, kind.symbol(), rectf::from_position_size(
				{ text_left, baseline - text_view->vertical_center_offset(fs) + sty.icon_extent * 0.5f },
				{ sty.icon_extent, sty.icon_extent }
			), {
				.color = sty.*kind.color,
				.extent = sty.icon_extent,
				.clip_rect = body_clip,
			});

			c.queue_text({
				.font = c.fonts.text,
				.text = s.name.empty() ? std::string_view("unnamed") : std::string_view(s.name),
				.position = { name_left, baseline },
				.scale = fs,
				.color = s.running ? sty.color_text : sty.color_text_secondary,
				.clip_rect = name_clip,
			});
			c.queue_text({
				.font = c.fonts.text,
				.text = status,
				.position = { status_left, baseline },
				.scale = fs,
				.color = sty.*plan.status_color,
				.clip_rect = clip,
			});

			baseline -= line_h;
			draw_phase_pipeline(c, plan.steps, generate_temp_id(hash_of(s.id, "agent_phase_underline")), rectf::from_position_size(
				{ text_left, baseline - text_view->vertical_center_offset(fs) + line_h * 0.5f },
				{ std::max(0.f, row.width() - pad * 2.f), line_h }
			), body_clip);

			if (busy) {
				baseline -= line_h;
				draw_activity_line(c, plan.activity, plan.activity_color, 1.f, rectf::from_position_size(
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
					.color = plan.tab.error ? sty.color_error : plan.tab.warning ? sty.color_warning : sty.color_text_secondary,
					.clip_rect = body_clip,
				});
			}

			c.queue_sprite({
				.rect = rectf::from_position_size({ row.left(), row.bottom() }, { row.width(), 1.f }),
				.color = sty.separator_line.fill,
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

auto gse::ide::agent::chat_at(data& d, const std::size_t index) -> std::vector<session>::iterator {
	auto found = d.sessions.end();
	std::size_t seen = 0;
	for (auto at = d.sessions.begin(); at != d.sessions.end(); ++at) {
		if (!is_chat(*at)) {
			continue;
		}
		found = at;
		if (seen++ == index) {
			break;
		}
	}
	return found;
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
		.dirty = hibernating_count(d) > 0 || std::ranges::any_of(d.sessions | std::views::filter(is_chat), awaiting_answer),
		.closeable = false,
		.pinned = true,
	});
	for (const session& s : d.sessions | std::views::filter(is_chat)) {
		const kind_style kind = style_of(kind_of(s));
		const chrome_tab tab = plan_chrome(d, s).tab;
		descs.push_back({
			.tab_id = session_tab_id(s.id),
			.caption = s.name,
			.icon = kind.symbol(),
			.icon_color = sty.*kind.color,
			.dirty = tab.dirty,
			.busy = tab.busy,
			.warning = tab.warning,
			.error = tab.error,
			.dimmed = tab.dimmed,
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
		.color = sty.panel_alt.fill,
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
		const auto to = chat_at(d, tabs.reorder_to > 0 ? tabs.reorder_to - 1 : 0);
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

	if (gui::caption_button(ui, history, "##agent_history", gui::symbol::chevron_down(), sty.caption_button)) {
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

	if (gui::caption_button(ui, info, std::format("##agent_info_{}", s->id), gui::symbol::info(), sty.caption_button)) {
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

auto gse::ide::agent::draw_transcript(gui::builder& ui, session& s, transcript_view& v, const rectf& area, const rectf& host, const channel_write<gui::menu_content, jump_to_request, set_cursor_shape_request> jump_out) -> void {
	const gui::draw_context& ctx = ui.ctx;
	const vec2f mouse = ctx.mouse_position();
	const gui::style& sty = ctx.style;
	const float pad = sty.padding;

	if (!std::ranges::any_of(s.rows, [&](const transcript_row& row) { return shows(v.filter, row); })) {
		draw_transcript_hint(ctx, v.filter == row_filter::changes
			? std::string("No file edits yet.")
			: "Type a prompt below to start claude in " + s.cwd.generic_display_string() + ".", area);
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
	sync_transcript(s, v, sty, metrics);

	if (v.buffer.lines.empty()) {
		v.buffer.lines.emplace_back();
		v.line_rows.push_back(0);
	}

	const gui::interaction::press tail_press = ui.draw<gui::follow_tail>({
		.area = area,
		.state = v.state,
		.widget_id = generate_temp_id(hash_of(v.log_id.number(), "agent_follow_tail")),
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
	const bool scanning = ctx.hovers(area) && !tail_press.hovered && !link && !toggle;
	const target_marker* target = scanning ? target_at(v, hovered, at.column) : nullptr;
	const std::optional<link_hit> path = target
		? resolve_link(target->target, s.cwd)
		: scanning ? path_link_at(hovered_text, at.column, s.cwd) : std::nullopt;

	std::vector<gui::text_underline> underlines;
	if (path) {
		underlines.push_back({
			.line = hovered,
			.start_col = target ? target->start_col : path->start_col,
			.end_col = target ? target->end_col : path->end_col,
			.color = sty.color_file,
		});
		jump_out.push<set_cursor_shape_request>({
			.shape = cursor_shape::hand,
		});
	}
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
		ctx.set_tooltip(gui::ids::make(std::format("##agent_group_{}_{}", s.id, group->row)), group_detail(s, *group));
	}
	else if (ctx.hovers(area) && !tail_press.hovered && context_row < s.rows.size() && !hovered_text.empty() && s.rows[context_row].stamped > 0) {
		ctx.set_tooltip(gui::ids::make(std::format("##agent_row_{}_{}", s.id, context_row)), local_time_label(s.rows[context_row].stamped));
	}
	const bool context_mine = ctx.hovers(area) && context_row < s.rows.size() && s.rows[context_row].kind == row_kind::user;
	const std::optional<std::uint32_t> rewind = context_mine ? rewind_anchor(s, context_row) : std::nullopt;

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
			.target = (static_cast<std::uint64_t>(s.id) << 32) | context_row,
			.tag = agent_context_tag(),
		});
	}

	if (ctx.clicked_in_rect(area)) {
		if (path) {
			jump_out.push<jump_to_request>({
				.path = path->path,
				.line = path->line,
				.column = path->column,
			});
		}
		else if (toggle) {
			toggle_marker(s, v, *toggle);
			sync_transcript(s, v, sty, metrics);
		}
		else if (link) {
			const transcript_row& owner = s.rows[std::min<std::size_t>(*link, s.rows.size() - 1)];
			const bool edit = !owner.added.empty() || !owner.removed.empty();

			if (edit && v.filter != row_filter::changes) {
				s.changes_open = true;
				s.changes_focus = *link;
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

	draw_canvases(ui, s, v, area, host);

	if (stale_diff) {
		relayout_from(v, *stale_diff);
	}

	if (v.filter == row_filter::changes && s.changes_focus) {
		const auto found = std::ranges::find(v.line_rows, *s.changes_focus);
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
		s.changes_focus.reset();
	}
}

auto gse::ide::agent::draw_canvases(gui::builder& ui, const session& s, transcript_view& v, const rectf& area, const rectf& host) -> void {
	const gui::draw_context& ctx = ui.ctx;
	const auto placed = [](const canvas_view& view) {
		return view.first_line != unplaced_line;
	};

	if (std::ranges::any_of(v.canvases, placed)) {
		const gui::text_area_layout geometry = gui::text_area_layout_of(ctx, {
			.buffer = v.buffer,
			.state = v.state,
			.rect = area,
			.spans = v.spans,
			.stops = v.stops,
			.blocks = v.blocks,
			.indent_width = transcript_tab_width,
		});

		for (canvas_view& view : v.canvases | std::views::filter(placed)) {
			view.tally = {};
			const canvas_record* record = canvas_named(s, s.rows[view.row].text);
			if (!record) {
				continue;
			}

			const rectf canvas = rectf::from_position_size(
				{ geometry.text_left, geometry.top - geometry.line_top(view.first_line) },
				{ std::min(record->size[0], geometry.view_width), record->size[1] }
			);
			if (!canvas.intersects(area)) {
				continue;
			}

			const id canvas_id = gui::ids::make_from_key(hash_of(s.id, record->name));
			const rectf visible = canvas.intersection(area);
			const gui::interaction::press open = gui::interaction::press_in_rect(ctx, ui.hot_widget_id, ui.active_widget_id, canvas_id, {
				.rect = visible,
			});

			view.tally = draw_canvas({
				.ctx = ctx,
				.area = canvas,
				.clip = visible,
				.canvas = canvas_id,
			}, record->steps);

			if (open.activated) {
				v.viewing_canvas = view.row;
			}
		}
	}

	if (!v.viewing_canvas) {
		return;
	}
	const canvas_record* viewed = canvas_named(s, s.rows[*v.viewing_canvas].text);
	if (!viewed) {
		v.viewing_canvas.reset();
		return;
	}
	draw_canvas_viewer(ui, *viewed, v, host, gui::ids::make_from_key(hash_of(s.id, viewed->name)));
}

auto gse::ide::agent::draw_canvas_viewer(gui::builder& ui, const canvas_record& record, transcript_view& v, const rectf& host, const id canvas) -> void {
	const gui::draw_context& ctx = ui.ctx;
	const gui::style& sty = ctx.style;
	const float pad = sty.padding;
	const auto text_view = ctx.fonts.text.resolve();

	const float header = text_view->line_height(sty.font_size) + pad * 2.f;
	const rectf body = host.inset({ host.width() * 0.08f, host.height() * 0.08f });
	const rectf content = rectf::from_position_size(
		{ body.left() + pad, body.top() - header },
		{ std::max(0.f, body.width() - pad * 2.f), std::max(0.f, body.height() - header - pad) }
	);

	const float scale = std::min(content.width() / record.size[0], content.height() / record.size[1]);
	const vec2f fitted_size = record.size * scale;
	const rectf fitted = rectf::from_position_size(
		{ content.center().x() - fitted_size[0] * 0.5f, content.center().y() + fitted_size[1] * 0.5f },
		fitted_size
	);

	const auto _ = ctx.scoped_layer(render_layer::modal);
	ctx.register_hit_region(render_layer::modal, host);
	gui::draw::fill_surface(ctx, host, sty.modal.scrim, {});
	gui::draw::fill_surface(
		ctx,
		rectf::from_position_size({ body.left() + sty.modal_shadow_offset, body.top() - sty.modal_shadow_offset }, { body.width(), body.height() }),
		sty.modal.shadow,
		{}
	);
	gui::draw::fill_surface(ctx, body, sty.modal.panel, {});

	draw_canvas({
		.ctx = ctx,
		.area = fitted,
		.clip = content,
		.canvas = canvas,
		.scale = scale,
	}, record.steps);

	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = ctx.intern(std::format("{}  {}", record.name, record.size)),
		.position = { body.left() + pad, body.top() - header * 0.5f + text_view->vertical_center_offset(sty.font_size) },
		.scale = sty.font_size,
		.color = sty.color_text_secondary,
		.clip_rect = body,
	});

	const float chip_extent = header * 0.5f;
	const rectf close = rectf::from_position_size(
		{ body.right() - pad - chip_extent, body.top() - pad * 0.5f },
		{ chip_extent, chip_extent }
	);
	const bool closed = gui::caption_button(ui, close, std::format("##canvas_viewer_{}", canvas.number()), gui::symbol::close(), sty.caption_button);
	const bool dismissed = gui::interaction::dismissed_by_outside_press(ctx, { .body = body });

	if (closed || dismissed || ctx.key_pressed_for(key::escape)) {
		v.viewing_canvas.reset();
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
		.name = std::format("##agent_model_{}", s.id),
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
		.name = std::format("##agent_effort_{}", s.id),
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

auto gse::ide::agent::question_prompt(const pending_question& asked) -> std::string {
	std::string out = asked.multi ? "Pick any that apply, or type instead" : "Pick one, or type instead";
	if (!asked.context.empty()) {
		out += " \xC2\xB7 " + asked.context;
	}
	return out;
}

auto gse::ide::agent::wrapped_block(const gui::draw_context& ctx, const std::string_view text, const float width, const std::size_t limit) -> std::vector<std::string> {
	std::vector<std::string> out;
	for (const std::string_view line : ctx.fonts.text.resolve()->wrap(text, width, ctx.style.font_size)) {
		if (out.size() >= limit) {
			return out;
		}
		out.emplace_back(line);
	}
	return out;
}

auto gse::ide::agent::layout_question(const gui::draw_context& ctx, const pending_question& asked, const float width, const float room) -> question_block {
	question_block laid;
	const gui::style& sty = ctx.style;
	const float pad = sty.padding;

	laid.line_h = gui::text_area_line_height(ctx, ctx.fonts.text);
	laid.body = std::max(sty.font_size, width - pad * 2.f);
	laid.label = std::max(sty.font_size, laid.body - gui::checkbox_extent(sty) - pad * 3.f);

	const std::string prompt = question_prompt(asked);
	for (const std::size_t limit : { max_question_lines, min_question_lines }) {
		laid.head = wrapped_block(ctx, asked.question, laid.body, limit);
		laid.hint = wrapped_block(ctx, prompt, laid.body, limit);
		laid.options.clear();
		laid.extent = pad * 2.5f + static_cast<float>(laid.head.size() + laid.hint.size()) * laid.line_h;

		const float fits = room - laid.line_h;
		for (const question_option& option : asked.options) {
			std::vector<std::string> lines = wrapped_block(ctx, option.label, laid.label, std::min(limit, max_option_lines));
			const float row = static_cast<float>(lines.size()) * laid.line_h + pad * 1.5f;
			if (laid.extent + row > fits && !laid.options.empty()) {
				break;
			}
			laid.extent += row;
			laid.options.push_back(std::move(lines));
		}

		if (laid.options.size() == asked.options.size()) {
			return laid;
		}
	}

	laid.hint.push_back(std::format(
		"{} more option(s) do not fit here - they are in the transcript above, and the box answers in your own words",
		asked.options.size() - laid.options.size()
	));
	laid.extent += laid.line_h;
	return laid;
}

auto gse::ide::agent::question_extent(const gui::draw_context& ctx, const session& s, const chrome_plan& plan, const question_block& laid) -> float {
	const rectf target = rectf::from_position_size({ 0.f, 0.f }, { 0.f, plan.question_shown ? laid.extent : 0.f });
	return ctx.animated_rect(generate_temp_id(hash_of(s.id, "agent_question")), target).height();
}

auto gse::ide::agent::question_option_rows(const gui::draw_context& ctx, const question_block& laid, const rectf& area) -> std::vector<rectf> {
	const float pad = ctx.style.padding;
	std::vector<rectf> rows;
	rows.reserve(laid.options.size());

	float cursor = area.top() - pad - static_cast<float>(laid.head.size() + laid.hint.size()) * laid.line_h - pad * 0.5f;
	for (const std::vector<std::string>& lines : laid.options) {
		const float row_h = static_cast<float>(lines.size()) * laid.line_h + pad;
		rows.push_back(rectf::from_position_size({ area.left() + pad, cursor }, { laid.body, row_h }));
		cursor -= row_h + pad * 0.5f;
	}
	return rows;
}

auto gse::ide::agent::draw_question_body(const gui::draw_context& ctx, const question_block& laid, const rectf& area, const std::span<const gui::interaction::press> presses) -> void {
	const gui::style& sty = ctx.style;
	const float pad = sty.padding;
	const float fs = sty.font_size;
	const float line_h = laid.line_h;
	const auto text_view = ctx.fonts.text.resolve();

	ctx.queue_sprite({
		.rect = area,
		.color = sty.panel_alt.fill,
		.texture = ctx.blank_texture,
	});

	const auto push_lines = [&](const std::vector<std::string>& lines, const float left, const float top, const vec4f& color) {
		float y = top;
		for (const std::string& line : lines) {
			ctx.queue_text({
				.font = ctx.fonts.text,
				.text = line,
				.position = { left, y - line_h * 0.5f + text_view->vertical_center_offset(fs) },
				.scale = fs,
				.color = color,
				.clip_rect = area,
			});
			y -= line_h;
		}
		return y;
	};

	const float cursor = push_lines(laid.head, area.left() + pad, area.top() - pad, sty.color_text);
	push_lines(laid.hint, area.left() + pad, cursor, sty.color_text_secondary);

	const float box_extent = gui::checkbox_extent(sty);
	const gui::item_skin& skin = sty.list_item;
	const std::vector<rectf> rows = question_option_rows(ctx, laid, area);
	for (std::size_t at = 0; at < rows.size(); ++at) {
		const rectf& row = rows[at];
		const gui::interaction::press& press = presses[at];

		gui::draw::fill_surface(ctx, row, press.select(skin.surfaces), {
			.clip_rect = area,
			.anim = press.widget,
		});

		const rectf box = rectf::from_position_size(
			{ row.left() + pad, row.top() - pad * 0.5f - line_h * 0.5f + box_extent * 0.5f },
			{ box_extent, box_extent }
		);
		gui::draw_checkbox(ctx, box, press.selected, {
			.color_frame = press.select(sty.checkbox.frame),
			.color_mark = press.select(sty.checkbox.mark),
			.extent = box_extent,
			.clip_rect = area,
		});

		push_lines(laid.options[at], box.right() + pad, row.top() - pad * 0.5f, press.select(skin.label_colors));
	}
}

auto gse::ide::agent::draw_question(gui::builder& ui, session& s, const rectf& area, const question_block& laid) -> void {
	const gui::draw_context& ctx = ui.ctx;
	const bool settled = area.height() >= laid.extent;
	const std::vector<rectf> rows = question_option_rows(ctx, laid, area);

	std::vector<gui::interaction::press> presses;
	presses.reserve(rows.size());
	for (std::size_t at = 0; at < rows.size(); ++at) {
		const id row_id = gui::ids::make(std::format("##agent_option_{}_{}", s.id, at));
		const bool chosen = s.question->options[at].chosen;
		presses.push_back(settled
			? gui::interaction::press_in_rect(ctx, ui.hot_widget_id, ui.active_widget_id, row_id, {
				.rect = rows[at],
				.selected = chosen,
			})
			: gui::interaction::press{ .widget = row_id, .selected = chosen });
	}

	draw_question_body(ctx, laid, area, presses);

	for (std::size_t at = 0; at < presses.size(); ++at) {
		if (!presses[at].activated) {
			continue;
		}

		pending_question& asked = *s.question;
		question_option& option = asked.options[at];
		const bool was = option.chosen;
		if (!asked.multi) {
			for (question_option& other : asked.options) {
				other.chosen = false;
			}
		}
		option.chosen = !was;
	}
}

auto gse::ide::agent::sync_plan_summary(const gui::draw_context& ctx, transcript_view& v, const std::string_view summary, const float width) -> void {
	const gui::style& sty = ctx.style;
	const float wrap = std::max(0.f, width - sty.padding * 4.f - gui::scroll_config{}.scrollbar_width);
	const std::uint64_t key = gui::style_key(sty) ^ std::hash<std::string_view>{}(summary);
	if (!v.buffer.lines.empty() && std::abs(v.wrap_width - wrap) <= 0.5f && v.style_key == key) {
		return;
	}

	v = {};
	v.wrap_width = wrap;
	v.style_key = key;
	const auto code_view = ctx.fonts.code.resolve();
	const auto body_view = ctx.fonts.text.resolve();
	push_transcript_line(v, sty, {
		.text = summary,
		.color = sty.*annotation_from_enum<row_style>(row_kind::text, {}).color,
		.face = gui::text_face::text,
		.markdown = true,
	}, {
		.fonts = ctx.fonts,
		.face = *code_view,
		.body = *body_view,
		.width = wrap,
		.scale = sty.font_size,
	});
	if (v.buffer.lines.empty()) {
		v.buffer.lines.emplace_back();
		v.line_rows.push_back(0);
	}
}

auto gse::ide::agent::sync_error_page(const gui::draw_context& ctx, transcript_view& v, const build_fix* fix, const float width) -> void {
	const gui::style& sty = ctx.style;
	const float wrap = std::max(0.f, width - sty.padding * 4.f - gui::scroll_config{}.scrollbar_width);
	const std::vector<strip_line> strip = error_strip(fix);
	const std::string_view diagnosis = fix ? std::string_view(fix->diagnosis) : std::string_view{};
	std::string content(diagnosis);
	for (const strip_line& line : strip) {
		content += '\n';
		content += line.text;
	}
	const std::uint64_t key = gui::style_key(sty) ^ std::hash<std::string>{}(content);
	if (!v.buffer.lines.empty() && std::abs(v.wrap_width - wrap) <= 0.5f && v.style_key == key) {
		return;
	}

	v = {};
	v.wrap_width = wrap;
	v.style_key = key;
	const auto code_view = ctx.fonts.code.resolve();
	const auto body_view = ctx.fonts.text.resolve();
	const transcript_metrics metrics{
		.fonts = ctx.fonts,
		.face = *code_view,
		.body = *body_view,
		.width = wrap,
		.scale = sty.font_size,
	};
	for (const strip_line& line : strip) {
		push_transcript_line(v, sty, {
			.text = line.text,
			.color = sty.*line.color,
			.face = gui::text_face::code,
		}, metrics);
	}
	if (!v.buffer.lines.empty()) {
		v.blocks.push_back({
			.first_line = 0,
			.last_line = static_cast<std::uint32_t>(v.buffer.lines.size()) - 1,
			.fill = fade(sty.color_error, error_strip_fill),
			.border = fade(sty.color_error, error_strip_border),
		});
	}
	v.buffer.lines.emplace_back();
	v.line_rows.push_back(0);
	push_transcript_line(v, sty, {
		.text = diagnosis.empty() ? std::string_view("no diagnosis yet - the agent reports one with gse_phase_done when the fix is done") : diagnosis,
		.color = diagnosis.empty() ? sty.color_text_secondary : sty.*annotation_from_enum<row_style>(row_kind::text, {}).color,
		.face = gui::text_face::text,
		.markdown = !diagnosis.empty(),
	}, metrics);
}

auto gse::ide::agent::sync_ledger(const gui::draw_context& ctx, transcript_view& v, const goal_dashboard* goal, const float width) -> void {
	const gui::style& sty = ctx.style;
	const float wrap = std::max(0.f, width - sty.padding * 2.f - gui::scroll_config{}.scrollbar_width);
	const std::span<const ledger_entry> ledger = goal ? std::span<const ledger_entry>(goal->ledger) : std::span<const ledger_entry>{};
	std::uint64_t key = gui::style_key(sty) ^ hash_of(ledger.size());
	for (const ledger_entry& entry : ledger) {
		key ^= hash_of(entry.started, std::to_underlying(entry.verdict)) ^ std::hash<std::string_view>{}(entry.summary);
	}
	if (!v.buffer.lines.empty() && std::abs(v.wrap_width - wrap) <= 0.5f && v.style_key == key) {
		return;
	}

	v = {};
	v.wrap_width = wrap;
	v.style_key = key;
	const auto code_view = ctx.fonts.code.resolve();
	const auto body_view = ctx.fonts.text.resolve();
	const transcript_metrics metrics{
		.fonts = ctx.fonts,
		.face = *code_view,
		.body = *body_view,
		.width = wrap,
		.scale = sty.font_size,
	};
	if (ledger.empty()) {
		push_transcript_line(v, sty, {
			.text = "ledger empty - no steering run has reported a verdict yet",
			.color = sty.color_text_secondary,
			.face = gui::text_face::text,
		}, metrics);
		return;
	}
	for (const ledger_entry& entry : ledger) {
		push_transcript_line(v, sty, {
			.text = ledger_head(entry),
			.color = sty.*verdict_policy_of(entry.verdict).color,
			.face = gui::text_face::text,
		}, metrics);
		push_transcript_line(v, sty, {
			.text = entry.summary,
			.color = sty.*annotation_from_enum<row_style>(row_kind::text, {}).color,
			.face = gui::text_face::text,
			.markdown = true,
		}, metrics);
		v.buffer.lines.emplace_back();
		v.line_rows.push_back(0);
	}
}

auto gse::ide::agent::layout_goal(const gui::draw_context& ctx, plan_block& laid, const goal_dashboard* goal, const float room) -> void {
	const gui::style& sty = ctx.style;
	const float pad = sty.padding;
	if (!goal) {
		laid.head = "goal \xC2\xB7 this chat pursues no goal";
		return;
	}
	laid.head = goal_head(*goal);

	std::vector<std::pair<std::string, vec4f gui::style::*>> rows;
	if (!goal->aim.empty()) {
		rows.emplace_back(std::format("aim \xC2\xB7 {}", goal->aim), nullptr);
	}
	rows.emplace_back(criteria_tally(goal->criteria), nullptr);
	for (const criterion_report& criterion : goal->criteria) {
		const criterion_style style = annotation_from_enum<criterion_style>(criterion.state, {});
		rows.emplace_back(criterion.note.empty()
			? std::format("{} {}", std::string_view(style.mark), criterion.id)
			: std::format("{} {} \xC2\xB7 {}", std::string_view(style.mark), criterion.id, criterion.note), style.color);
	}
	rows.emplace_back(route_line(goal->route), nullptr);

	const float fixed = pad * 3.5f + laid.line_h + laid.button_h;
	const float fits = room - fixed - laid.line_h;
	float used = 0.f;
	std::size_t placed = 0;
	for (auto& [text, mark] : rows) {
		std::vector<std::string> lines = wrapped_block(ctx, text, std::max(sty.font_size, laid.steps_w - pad * 2.f), max_option_lines);
		const float row = static_cast<float>(lines.size()) * laid.line_h + pad * 1.25f;
		if (used + row > fits && !laid.steps.empty()) {
			break;
		}
		used += row;
		laid.steps.push_back(std::move(lines));
		laid.groups.push_back(placed);
		laid.marks.push_back(mark);
		++placed;
	}
	if (placed < rows.size()) {
		laid.overflow = std::format("{} more row(s) do not fit here - gse_panel_query Agent lists every criterion", rows.size() - placed);
	}
}

auto gse::ide::agent::sync_step_diffs(const gui::draw_context& ctx, transcript_view& v, std::vector<transcript_row>& rows, const std::span<const step_edit* const> edits, const std::optional<std::size_t> group, const float width) -> void {
	const gui::style& sty = ctx.style;
	const float wrap = std::max(0.f, width - sty.padding * 2.f - gui::scroll_config{}.scrollbar_width);
	const std::uint64_t key = gui::style_key(sty) ^ hash_of(group.value_or(std::numeric_limits<std::size_t>::max()), edits.size(), edits.empty() ? std::int64_t{} : edits.back()->stamped);
	const bool current = !v.buffer.lines.empty() && std::abs(v.wrap_width - wrap) <= 0.5f && v.style_key == key;
	if (current && v.flushed_rows >= rows.size()) {
		return;
	}

	if (!current) {
		v = {};
		v.filter = row_filter::changes;
		v.wrap_width = wrap;
		v.style_key = key;
		rows = edits | std::views::transform([](const step_edit* edit) {
			return transcript_row{
				.kind = row_kind::tool,
				.text = edit->file.generic_display_string(),
				.file = edit->file,
				.removed = edit->removed,
				.added = edit->added,
			};
		}) | std::ranges::to<std::vector>();
	}

	const auto code_view = ctx.fonts.code.resolve();
	const auto body_view = ctx.fonts.text.resolve();
	const transcript_metrics metrics = {
		.fonts = ctx.fonts,
		.face = *code_view,
		.body = *body_view,
		.width = wrap,
		.scale = sty.font_size,
	};
	for (auto at = static_cast<std::uint32_t>(v.flushed_rows); at < rows.size(); ++at) {
		push_transcript_line(v, sty, {
			.text = rows[at].text,
			.color = sty.color_file,
			.face = gui::text_face::code,
			.row = at,
		}, metrics);
		push_diff(v, sty, rows[at], at, metrics);
	}
	v.flushed_rows = rows.size();
	if (rows.empty() && v.buffer.lines.empty()) {
		push_transcript_line(v, sty, {
			.text = "No edits here yet.",
			.color = sty.color_text_secondary,
			.face = gui::text_face::text,
		}, metrics);
	}
}

auto gse::ide::agent::layout_plan(const gui::draw_context& ctx, const std::span<const build_inbox::plan_step> steps, const step_progress& progress, const std::size_t summary_lines, const plan_paging paging, const std::string_view approve, const float ratio, const float width, const float room) -> plan_block {
	plan_block laid;
	laid.paging = paging;
	const gui::style& sty = ctx.style;
	const float pad = sty.padding;
	const page_kind kind = paging.shown.kind;

	laid.line_h = gui::text_area_line_height(ctx, ctx.fonts.text);
	laid.gutter = sty.font_size * 3.f;
	laid.button_h = sty.font_size * 1.4f;
	laid.approve = approve;
	const float inner = std::max(sty.font_size, width - pad * 2.f - laid.gutter);
	laid.steps_w = std::clamp((width - pad * 2.f) * ratio - laid.gutter * 0.5f, std::min(sty.font_size, inner), inner);
	laid.files_w = inner - laid.steps_w;

	std::vector<std::filesystem::path> files;
	for (const build_inbox::plan_step& step : steps) {
		std::vector<std::size_t> linked;
		for (const std::filesystem::path& file : step.files) {
			auto at = std::ranges::find(files, file);
			if (at == files.end()) {
				files.push_back(file);
				at = std::prev(files.end());
			}
			linked.push_back(static_cast<std::size_t>(std::distance(files.begin(), at)));
		}
		laid.links.push_back(std::move(linked));
	}

	const float fixed = pad * 3.5f + laid.line_h + laid.button_h;
	const float file_row = laid.line_h + pad * 0.25f;
	const float fits = room - fixed - laid.line_h;

	std::vector<std::size_t> groups;
	if (kind == page_kind::steps || kind == page_kind::changes) {
		groups = std::views::iota(std::size_t{ 0 }, steps.size()) | std::ranges::to<std::vector>();
	}
	if (kind == page_kind::changes && progress.unattributed > 0) {
		groups.push_back(steps.size());
	}

	const auto row_text = [&](const std::size_t group) -> std::string {
		if (group == steps.size()) {
			return std::format("{} \xC2\xB7 {}", unattributed_label, edits_label(progress.unattributed));
		}
		if (kind == page_kind::changes && progress.reported) {
			return std::format("{} {}. {} \xC2\xB7 {}", std::string_view(annotation_from_enum<step_style>(progress.states[group], {}).glyph), group + 1, steps[group].text, edits_label(progress.edits[group]));
		}
		if (kind == page_kind::changes) {
			return std::format("{}. {} \xC2\xB7 {}", group + 1, steps[group].text, edits_label(progress.edits[group]));
		}
		return std::format("{}. {}", group + 1, steps[group].text);
	};

	float steps_h = 0.f;
	for (const std::size_t group : groups) {
		std::vector<std::string> lines = wrapped_block(ctx, row_text(group), std::max(sty.font_size, laid.steps_w - pad * 2.f), max_option_lines);
		const float row = static_cast<float>(lines.size()) * laid.line_h + pad * 1.25f;
		if (steps_h + row > fits && !laid.steps.empty()) {
			break;
		}
		steps_h += row;
		laid.steps.push_back(std::move(lines));
		laid.groups.push_back(group);
		laid.marks.push_back(kind == page_kind::changes && progress.reported && group < steps.size() ? annotation_from_enum<step_style>(progress.states[group], {}).color : nullptr);
	}

	if (kind == page_kind::steps) {
		const auto shown_files = std::min(files.size(), static_cast<std::size_t>(std::max(1.f, std::floor(fits / file_row))));
		for (std::size_t at = 0; at < shown_files; ++at) {
			laid.files.push_back(files[at].generic_display_string());
		}
	}

	const std::string_view stage = policy_of(paging.shown.phase).label;
	const auto done = std::ranges::count(progress.states, step_state::done);
	const std::size_t edits = std::ranges::fold_left(progress.edits, progress.unattributed, std::plus{});
	switch (kind) {
		case page_kind::summary:
			laid.head = std::format("{} \xC2\xB7 summary", stage);
			break;
		case page_kind::steps:
			laid.head = std::format("{} \xC2\xB7 plan \xC2\xB7 {} step{} \xC2\xB7 {} file{}", stage, steps.size(), steps.size() == 1 ? "" : "s", files.size(), files.size() == 1 ? "" : "s");
			break;
		case page_kind::changes:
			laid.head = progress.ended
				? progress.reported
					? std::format("{} \xC2\xB7 ended \xC2\xB7 {}/{} marked \xC2\xB7 {}", stage, done, steps.size(), edits_label(edits))
					: std::format("{} \xC2\xB7 ended \xC2\xB7 unreported \xC2\xB7 {}", stage, edits_label(edits))
				: progress.reported
				? std::format("{} \xC2\xB7 {}/{} done \xC2\xB7 {}", stage, done, steps.size(), edits_label(edits))
				: std::format("{} \xC2\xB7 unreported - the agent has not called gse_step \xC2\xB7 {}", stage, edits_label(edits));
			break;
		case page_kind::review_loop:
		case page_kind::report:
		case page_kind::errors:
		case page_kind::goal:
		case page_kind::verdict:
			break;
		case page_kind::empty:
			laid.head =std::format("{} \xC2\xB7 no screen for this stage yet", stage);
			break;
	}
	laid.page_label = std::format("{}/{}", paging.page, paging.pages);
	const std::size_t hidden_steps = groups.size() - laid.steps.size();
	const std::size_t hidden_files = kind == page_kind::steps ? files.size() - laid.files.size() : 0;
	if (hidden_steps > 0 || hidden_files > 0) {
		laid.overflow = std::format("{} more step(s) and {} more file(s) do not fit here - the summary above has them all", hidden_steps, hidden_files);
	}

	const float columns = std::max(steps_h, static_cast<float>(laid.files.size()) * file_row);
	const float overflow_h = laid.overflow.empty() ? 0.f : laid.line_h;
	laid.summary_h = std::min(static_cast<float>(summary_lines) * laid.line_h + pad, std::max(laid.line_h, room - fixed));
	switch (kind) {
		case page_kind::summary:
		case page_kind::review_loop:
		case page_kind::report:
		case page_kind::errors:
		case page_kind::verdict:
			laid.extent = fixed + laid.summary_h;
			break;
		case page_kind::steps:
			laid.extent = fixed + columns + overflow_h;
			break;
		case page_kind::changes:
		case page_kind::goal:
			laid.extent = fixed + std::max(columns + overflow_h, room - fixed);
			break;
		case page_kind::empty:
			laid.extent = fixed;
			break;
	}
	return laid;
}

auto gse::ide::agent::plan_extent(const gui::draw_context& ctx, const session& s, const chrome_plan& plan, const plan_block& laid) -> float {
	const rectf target = rectf::from_position_size({ 0.f, 0.f }, { 0.f, plan.plan_shown ? laid.extent : 0.f });
	return ctx.animated_rect(generate_temp_id(hash_of(s.id, "agent_plan")), target).height();
}

auto gse::ide::agent::plan_rects_of(const gui::draw_context& ctx, const plan_block& laid, const rectf& area) -> plan_rects {
	const gui::style& sty = ctx.style;
	const float pad = sty.padding;
	const auto text_view = ctx.fonts.text.resolve();
	const float top = area.top() - pad - laid.line_h - pad * 0.5f;
	const float files_left = area.left() + pad + laid.steps_w + laid.gutter;

	plan_rects out;
	float cursor = top;
	for (const std::vector<std::string>& lines : laid.steps) {
		const float row_h = static_cast<float>(lines.size()) * laid.line_h + pad;
		out.steps.push_back(rectf::from_position_size({ area.left() + pad, cursor }, { laid.steps_w, row_h }));
		cursor -= row_h + pad * 0.25f;
	}
	cursor = top;
	for (std::size_t at = 0; at < laid.files.size(); ++at) {
		out.files.push_back(rectf::from_position_size({ files_left, cursor }, { laid.files_w, laid.line_h }));
		cursor -= laid.line_h + pad * 0.25f;
	}

	const float button_top = area.bottom() + pad + laid.button_h;
	const float approve_w = text_view->width(laid.approve, sty.font_size) + pad * 2.f;
	const float revise_w = text_view->width(revise_label, sty.font_size) + pad * 2.f;
	out.approve = rectf::from_position_size({ area.right() - pad - approve_w, button_top }, { approve_w, laid.button_h });
	out.revise = rectf::from_position_size({ out.approve.left() - pad * 0.5f - revise_w, button_top }, { revise_w, laid.button_h });

	const float summary_bottom = out.approve.top() + pad * 0.5f;
	out.summary = rectf::from_position_size({ area.left() + pad, top }, { std::max(0.f, area.width() - pad * 2.f), std::max(0.f, top - summary_bottom) });
	out.diffs = rectf::from_position_size({ files_left, top }, { laid.files_w, std::max(0.f, top - summary_bottom) });

	const float nav_top = area.top() - pad * 0.25f;
	const float nav_h = laid.line_h + pad * 0.5f;
	if (!laid.diffs_link.empty()) {
		const float head_w = text_view->width(laid.head, sty.font_size);
		out.diffs_link = rectf::from_position_size({ area.left() + pad + head_w + pad, nav_top }, { text_view->width(laid.diffs_link, sty.font_size), nav_h });
	}
	const float label_w = text_view->width(laid.page_label, sty.font_size) + pad;
	out.forward = rectf::from_position_size({ area.right() - pad - laid.button_h, nav_top }, { laid.button_h, nav_h });
	out.page = rectf::from_position_size({ out.forward.left() - label_w, nav_top }, { label_w, nav_h });
	out.back = rectf::from_position_size({ out.page.left() - laid.button_h, nav_top }, { laid.button_h, nav_h });
	if (!laid.cycle_label.empty()) {
		const float stage_left = laid.paging.pages > 1 ? out.back.left() - pad : area.right() - pad;
		const float cycle_w = text_view->width(cycle_forward_label, sty.font_size) + pad;
		const float cycle_label_w = text_view->width(laid.cycle_label, sty.font_size) + pad;
		out.cycle_forward = rectf::from_position_size({ stage_left - cycle_w, nav_top }, { cycle_w, nav_h });
		out.cycle_page = rectf::from_position_size({ out.cycle_forward.left() - cycle_label_w, nav_top }, { cycle_label_w, nav_h });
		out.cycle_back = rectf::from_position_size({ out.cycle_page.left() - cycle_w, nav_top }, { cycle_w, nav_h });
	}
	return out;
}

auto gse::ide::agent::draw_plan_body(const gui::draw_context& ctx, const plan_block& laid, const rectf& area, const std::span<const gui::interaction::press> presses, const std::optional<std::size_t> focus, const float shown, const id key) -> void {
	const gui::style& sty = ctx.style;
	const float pad = sty.padding;
	const float fs = sty.font_size;
	const auto text_view = ctx.fonts.text.resolve();
	const float centering = text_view->vertical_center_offset(fs);
	const plan_rects rects = plan_rects_of(ctx, laid, area);

	ctx.queue_sprite({
		.rect = area,
		.color = sty.panel_alt.fill,
		.texture = ctx.blank_texture,
	});

	const auto put = [&](const std::string_view text, const vec2f at, const vec4f& color) {
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = text,
			.position = { at[0], at[1] - laid.line_h * 0.5f + centering },
			.scale = fs,
			.color = color,
			.clip_rect = area,
		});
	};

	put(laid.head, { area.left() + pad, area.top() - pad }, sty.color_text_secondary);
	if (laid.paging.pages > 1) {
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = laid.page_label,
			.position = { rects.page.left() + pad * 0.5f, rects.page.center().y() + centering },
			.scale = fs,
			.color = sty.color_text_secondary,
			.clip_rect = area,
		});
	}

	if (!laid.cycle_label.empty()) {
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = laid.cycle_label,
			.position = { rects.cycle_page.left() + pad * 0.5f, rects.cycle_page.center().y() + centering },
			.scale = fs,
			.color = sty.color_text_secondary,
			.clip_rect = area,
		});
	}

	const page_kind kind = laid.paging.shown.kind;
	if (!laid.diffs_link.empty()) {
		put(laid.diffs_link, { rects.diffs_link.left(), area.top() - pad }, sty.color_accent);
	}

	if (kind == page_kind::summary || kind == page_kind::review_loop || kind == page_kind::report || kind == page_kind::errors || kind == page_kind::verdict || kind == page_kind::empty) {
		return;
	}

	const gui::item_skin& skin = sty.list_item;
	for (std::size_t at = 0; at < rects.steps.size(); ++at) {
		const rectf& row = rects.steps[at];
		const gui::interaction::press& press = presses[at];
		gui::draw::fill_surface(ctx, row, press.select(skin.surfaces), {
			.clip_rect = area,
			.anim = press.widget,
		});
		const vec4f color = laid.marks[at] && !press.selected ? sty.*laid.marks[at]: press.select(skin.label_colors);
		float y = row.top() - pad * 0.5f;
		for (const std::string& line : laid.steps[at]) {
			put(line, { row.left() + pad, y }, fade(color, shown));
			y -= laid.line_h;
		}
	}

	if (!laid.overflow.empty()) {
		put(laid.overflow, { area.left() + pad, rects.approve.top() + laid.line_h + pad * 0.5f }, fade(sty.color_text_secondary, shown));
	}

	if (kind != page_kind::steps) {
		return;
	}

	const auto linked = [&](const std::size_t file) {
		return focus && *focus < laid.links.size() && std::ranges::contains(laid.links[*focus], file);
	};
	for (std::size_t at = 0; at < rects.files.size(); ++at) {
		const vec4f color = fade(ctx.animated_color(generate_temp_id(hash_of(key.number(), "file", at)), linked(at) ? sty.color_accent : sty.color_text_secondary), shown);
		const rectf& row = rects.files[at];
		ctx.queue_text({
			.font = ctx.fonts.code,
			.text = laid.files[at],
			.position = { row.left(), row.center().y() + ctx.fonts.code.resolve()->vertical_center_offset(fs) },
			.scale = fs,
			.color = color,
			.clip_rect = area.intersection(row),
		});
	}

	const float thickness = std::max(1.f, std::round(sty.scale_factor));
	for (std::size_t step = 0; step < rects.steps.size(); ++step) {
		for (const std::size_t file : laid.links[step]) {
			if (file >= rects.files.size()) {
				continue;
			}
			const float linked_shown = ctx.presence(generate_temp_id(hash_of(key.number(), "link", step, file)), focus == step);
			if (linked_shown <= 0.f) {
				continue;
			}
			const rectf& from = rects.steps[step];
			const rectf& to = rects.files[file];
			const vec4f color = fade(sty.color_accent, linked_shown * shown);
			queue_line(ctx, { from.right(), from.center().y() }, { to.left() - pad * 0.5f, to.center().y() }, thickness, color, area);
		}
	}
}

auto gse::ide::agent::draw_plan_summary(const gui::draw_context& ctx, transcript_view& v, const rectf& rect, const float shown, const id key, id& hot, id& active, id& focus) -> void {
	if (rect.height() <= 0.f || v.buffer.lines.empty()) {
		return;
	}

	std::vector<gui::text_fade> fades;
	if (shown < 1.f) {
		fades.reserve(v.buffer.lines.size());
		for (std::size_t line = 0; line < v.buffer.lines.size(); ++line) {
			fades.push_back({
				.line = static_cast<std::uint32_t>(line),
				.end_col = static_cast<std::uint32_t>(v.buffer.lines[line].size()),
				.alpha = shown,
			});
		}
	}

	const std::optional<std::uint32_t> stale_diff = update_diff_scroll(ctx, v, rect, ctx.fonts.code.resolve()->width("0", ctx.style.font_size));

	gui::text_area::draw(ctx, {
		.buffer = v.buffer,
		.state = v.state,
		.widget_id = key,
		.spans = v.spans,
		.fades = fades,
		.blocks = v.blocks,
		.stops = v.stops,
		.rules = v.rules,
		.rect = rect,
		.read_only = true,
		.indent_width = transcript_tab_width,
		.blink_interval = time{},
	}, hot, active, focus);

	if (stale_diff) {
		relayout_from(v, *stale_diff);
	}
}

auto gse::ide::agent::split_ratio(const std::span<const split_memory> splits, const split_kind kind) -> float {
	const auto at = static_cast<std::size_t>(std::to_underlying(kind));
	return at < splits.size() && splits[at].ratio > 0.f && splits[at].ratio < 1.f ? splits[at].ratio : annotation_from_enum<split_style>(kind, {}).ratio;
}

auto gse::ide::agent::split_of(data& d, const split_kind kind) -> split_memory& {
	d.splits.resize(std::max(d.splits.size(), enum_values<split_kind>().size()));
	split_memory& memory = d.splits[std::to_underlying(kind)];
	memory.ratio = split_ratio(d.splits, kind);
	return memory;
}

auto gse::ide::agent::draw_split(const gui::draw_context& ctx, const rectf& container, float& ratio, gui::layout::split_drag_state& drag, const float minimum, const float thickness, const channel_write<gui::menu_content, jump_to_request, set_cursor_shape_request> jump_out) -> gui::layout::split_result {
	const vec2f mouse = ctx.mouse_position();
	const bool blocked = ctx.hit_regions && ctx.hit_regions->is_resize_blocked(mouse);
	const gui::layout::split_result split = gui::layout::update_split({
		.container = container,
		.axis = gui::layout::split_axis::columns,
		.ratio = ratio,
		.min_first = minimum,
		.min_second = minimum,
		.divider_thickness = thickness,
	}, {
		.mouse = mouse,
		.pressed = ctx.mouse_pressed(mouse_button::button_1) && ctx.input_available(),
		.held = ctx.mouse_held(mouse_button::button_1),
		.blocked = blocked,
	}, drag);

	if (drag.dragging && container.width() >= minimum * 2.f + thickness) {
		ratio = split.ratio;
	}

	if ((split.divider.contains(mouse) && !blocked) || drag.dragging) {
		jump_out.push<set_cursor_shape_request>({
			.shape = cursor_shape::resize_ew,
		});
	}
	return split;
}

auto gse::ide::agent::plan_split_kind(const page_kind kind) -> split_kind {
	return kind == page_kind::changes ? split_kind::apply_diffs
		: kind == page_kind::goal ? split_kind::goal_ledger
		: split_kind::plan_files;
}

auto gse::ide::agent::followup_width(const gui::draw_context& ctx, const session& s, const chrome_plan& plan, const split_memory& split, const float width) -> float {
	const float target_w = plan.plan_split ? width * (1.f - split.ratio) : 0.f;
	const float eased = ctx.animated_rect(generate_temp_id(hash_of(s.id, "agent_followup")), rectf::from_position_size({ 0.f, 0.f }, { target_w, 0.f })).width();
	return split.drag.dragging ? target_w : eased;
}

auto gse::ide::agent::draw_followup(gui::builder& ui, session& s, const chrome_plan& plan, split_memory& split, const rectf& whole, const float column, const rectf& body, const channel_write<gui::menu_content, jump_to_request, set_cursor_shape_request> jump_out) -> void {
	const gui::draw_context& ctx = ui.ctx;
	const gui::style& sty = ctx.style;
	const rectf area = rectf::from_position_size({ whole.right() - column, whole.top() }, { column, whole.height() });
	if (area.width() < sty.font_size) {
		return;
	}
	if (plan.plan_split && s.followup.first_row != plan.followup_row) {
		s.followup.first_row = plan.followup_row;
		reset_view(s.followup);
	}
	if (plan.plan_split) {
		draw_split(ctx, whole, split.ratio, split.drag, sty.font_size * annotation_from_enum<split_style>(split_kind::plan_followup, {}).minimum, std::max(6.f, sty.resize_border_thickness) * 2.f, jump_out);
	}
	const float thickness = std::max(1.f, std::round(sty.scale_factor));
	ctx.queue_sprite({
		.rect = rectf::from_position_size({ area.left(), area.top() }, { thickness, area.height() }),
		.color = sty.color_text_disabled,
		.texture = ctx.blank_texture,
	});
	draw_transcript(ui, s, s.followup, rectf::from_position_size({ area.left() + thickness, area.top() }, { area.width() - thickness, area.height() }), body, jump_out);
}

auto gse::ide::agent::draw_plan(gui::builder& ui, session& s, split_memory& columns, const rectf& area, const plan_block& laid, std::uint32_t& page, const std::string_view key, const channel_write<gui::menu_content, jump_to_request, set_cursor_shape_request> jump_out) -> void {
	const gui::draw_context& ctx = ui.ctx;
	const bool settled = area.height() >= laid.extent;
	const plan_rects rects = plan_rects_of(ctx, laid, area);
	const float shown = ctx.presence(generate_temp_id(hash_of(s.id, key, laid.paging.page)), true);
	const page_kind kind = laid.paging.shown.kind;
	const bool selectable = kind == page_kind::steps || kind == page_kind::changes;

	if (settled && (selectable || kind == page_kind::goal)) {
		const float pad = ctx.style.padding;
		const rectf columns_area = rectf::from_position_size({ area.left() + pad, rects.diffs.top() }, { std::max(0.f, area.width() - pad * 2.f), rects.diffs.height() });
		draw_split(ctx, columns_area, columns.ratio, columns.drag, ctx.style.font_size * annotation_from_enum<split_style>(plan_split_kind(kind), {}).minimum, laid.gutter, jump_out);
	}

	std::vector<gui::interaction::press> presses;
	presses.reserve(rects.steps.size());
	std::optional<std::size_t> hovered;
	std::optional<std::size_t> selected;
	for (std::size_t at = 0; at < rects.steps.size(); ++at) {
		const id row_id = gui::ids::make(std::format("##agent_plan_step_{}_{}", s.id, at));
		const bool chosen = selectable && s.plan_selected == laid.groups[at];
		if (chosen) {
			selected = at;
		}
		presses.push_back(settled && selectable
			? gui::interaction::press_in_rect(ctx, ui.hot_widget_id, ui.active_widget_id, row_id, {
				.rect = rects.steps[at],
				.selected = chosen,
			})
			: gui::interaction::press{ .widget = row_id, .selected = chosen });
		if (presses.back().hovered) {
			hovered = at;
		}
	}

	draw_plan_body(ctx, laid, area, presses, hovered ? hovered : selected, shown, generate_temp_id(hash_of(s.id, "agent_plan_body")));
	if (kind == page_kind::summary || kind == page_kind::review_loop || kind == page_kind::report || kind == page_kind::errors || kind == page_kind::verdict) {
		draw_plan_summary(ctx, s.plan_summary, rects.summary, shown, gui::ids::make(std::format("##agent_plan_summary_{}", s.id)), ui.hot_widget_id, ui.active_widget_id, ui.focus_widget_id);
	}
	if (kind == page_kind::changes) {
		draw_plan_summary(ctx, s.step_diffs, rects.diffs, shown, gui::ids::make(std::format("##agent_step_diffs_{}", s.id)), ui.hot_widget_id, ui.active_widget_id, ui.focus_widget_id);
	}
	if (kind == page_kind::goal) {
		draw_plan_summary(ctx, s.plan_summary, rects.diffs, shown, gui::ids::make(std::format("##agent_goal_ledger_{}", s.id)), ui.hot_widget_id, ui.active_widget_id, ui.focus_widget_id);
	}

	for (std::size_t at = 0; at < presses.size(); ++at) {
		if (presses[at].activated) {
			const auto group = static_cast<std::uint32_t>(laid.groups[at]);
			s.plan_selected = s.plan_selected == group ? std::nullopt : std::optional(group);
		}
	}

	if (!settled) {
		return;
	}

	if (!laid.diffs_link.empty() && gui::interaction::press_in_rect(ctx, ui.hot_widget_id, ui.active_widget_id, gui::ids::make(std::format("##agent_report_diffs_{}", s.id)), {
		.rect = rects.diffs_link,
	}).activated) {
		s.changes_open = true;
	}

	if (laid.paging.pages > 1) {
		if (ui.draw<gui::button>({
			.text = page_back_label,
			.rect = rects.back,
			.key = std::format("##agent_plan_back_{}", s.id),
			.enabled = laid.paging.page > 1,
		})) {
			page = laid.paging.page - 2;
		}
		if (ui.draw<gui::button>({
			.text = page_forward_label,
			.rect = rects.forward,
			.key = std::format("##agent_plan_forward_{}", s.id),
			.enabled = laid.paging.page < laid.paging.pages,
		})) {
			page = laid.paging.page;
		}
	}

	if (laid.approve.empty()) {
		return;
	}

	if (ui.draw<gui::button>({
		.text = revise_label,
		.rect = rects.revise,
		.key = std::format("##agent_plan_revise_{}", s.id),
		.enabled = !input_empty(s.draft) || !s.attachments.items.empty(),
	})) {
		submit_draft(s);
	}
	if (ui.draw<gui::button>({
		.text = laid.approve,
		.rect = rects.approve,
		.key = std::format("##agent_phase_approve_{}", s.id),
		.role = gui::button_role::accent,
	})) {
		advance_phase(s, input_text(s.draft));
		reset_input(s);
	}
}

auto gse::ide::agent::submit_draft(session& s) -> void {
	if (!s.running && !agent_credentials().token) {
		request_token_prompt();
		return;
	}
	const std::string prompt = input_text(s.draft);

	if (current_phase(s) == task_phase::settled) {
		restart_task(s);
	}

	if (!s.running && !launch_session(s)) {
		append_row(s, {
			.kind = row_kind::failure,
			.text = s.info.failure,
		});
		return;
	}

	if (send_to_session(s, prompt, s.attachments.items)) {
		reset_input(s);
	}
}

auto gse::ide::agent::draw_input(gui::builder& ui, session& s, const input_layout& layout) -> void {
	const rectf& area = layout.area;
	const rectf& body = layout.body;
	const chrome_plan& plan = layout.plan;
	const gui::draw_context& ctx = ui.ctx;
	const gui::style& sty = ctx.style;
	const float pad = sty.padding;

	const id input_id = gui::ids::make(std::format("##agent_input_{}", s.id));
	const bool focused = ui.focus_widget_id == input_id;
	const bool ctrl = ctx.key_held(key::left_control) || ctx.key_held(key::right_control);
	const bool shift = ctx.key_held(key::left_shift) || ctx.key_held(key::right_shift);
	const bool enter = focused && !s.attachments.viewing && !ctrl && !shift && ctx.key_pressed_for(key::enter);
	const bool submit = enter && (!input_empty(s.draft) || !s.attachments.items.empty());

	ctx.queue_sprite({
		.rect = area,
		.color = sty.input.area.fill,
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
	const bool gated = plan.action == chrome_action::approve;
	const bool asking = plan.action == chrome_action::answer;
	const std::string& action_text = plan.action_label;
	const float action_width = gated || asking ? text_view->width(action_text, sty.font_size) + pad * 2.f : button_extent;
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

	if (show_controls) {
		draw_model_controls(ui, s, model_rect, effort_rect);
	}

	if (asking) {
		const bool answerable = !input_empty(s.draft) || std::ranges::any_of(s.question->options, [](const question_option& option) {
			return option.chosen;
		});
		const bool pressed = ui.draw<gui::button>({
			.text = action_text,
			.rect = stop,
			.key = std::format("##agent_answer_{}", s.id),
			.enabled = answerable,
		});
		if (answerable && (pressed || enter)) {
			answer_question(s, input_text(s.draft));
			reset_input(s);
		}
		return;
	}

	if (gated) {
		if (!plan.plan_shown && ui.draw<gui::button>({
			.text = action_text,
			.rect = stop,
			.key = std::format("##agent_phase_approve_{}", s.id),
		})) {
			advance_phase(s, input_text(s.draft));
			reset_input(s);
		}
	}
	else if (gui::caption_button(ui, stop, std::format("##agent_stop_{}", s.id), gui::symbol::stop(), sty.caption_button, plan.stop_enabled)) {
		if (plan.stop_interrupts) {
			interrupt_session(s);
		}
		else {
			pause_goal(s);
		}
	}

	if (submit) {
		submit_draft(s);
	}
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
		.color = sty.list_item.surfaces.idle.fill,
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

auto gse::ide::agent::draw_status(gui::builder& ui, session& s, const chrome_plan& plan, const rectf& area) -> void {
	const gui::draw_context& ctx = ui.ctx;
	const gui::style& sty = ctx.style;
	const float pad = sty.padding;

	ctx.queue_sprite({
		.rect = area,
		.color = sty.panel_alt.fill,
		.texture = ctx.blank_texture,
	});

	const float reserved = draw_view_toggles(ui, s, plan, area);
	const rectf row = rectf::from_position_size(
		{ area.left() + pad, area.top() },
		{ std::max(0.f, area.width() - pad - reserved), area.height() }
	);
	s.wait_shown = plan.waiting;
	draw_status_row(ctx, plan, generate_temp_id(hash_of(s.id, "agent_status")), row, row);
}

auto gse::ide::agent::shown_view(session& s) -> transcript_view& {
	return s.mode == transcript_mode::everything ? s.everything : s.full;
}

auto gse::ide::agent::draw_since_banner(const gui::draw_context& ctx, const std::string_view digest, const rectf& area, const rectf& button) -> void {
	const gui::style& sty = ctx.style;
	const auto text_view = ctx.fonts.text.resolve();
	ctx.queue_sprite({
		.rect = area,
		.color = fade(sty.color_warning, error_strip_fill),
		.texture = ctx.blank_texture,
		.clip_rect = area,
	});
	const rectf text = rectf::from_position_size(area.top_left(), { std::max(0.f, button.left() - area.left() - sty.padding), area.height() });
	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = digest,
		.position = { area.left() + sty.padding, area.center().y() + text_view->vertical_center_offset(sty.font_size) },
		.scale = sty.font_size,
		.color = sty.color_warning,
		.clip_rect = text,
	});
}

auto gse::ide::agent::fork_extent(const gui::draw_context& ctx, const session& s, const lifecycle& life, const plan_paging& paging, const float room) -> float {
	const float row = ctx.style.font_size * 1.4f + ctx.style.padding;
	if (s.fork.process) {
		return row + room * fork_answer_share;
	}
	if (!life.cycle.shown || !paging.shown.run || paging.shown.kind == page_kind::goal) {
		return 0.f;
	}
	return row;
}

auto gse::ide::agent::draw_fork(gui::builder& ui, session& s, const chrome_plan& plan, const lifecycle& life, const plan_paging& paging, const rectf& area) -> void {
	const gui::draw_context& ctx = ui.ctx;
	const gui::style& sty = ctx.style;
	const float pad = sty.padding;
	const float button_h = sty.font_size * 1.4f;
	if (area.height() < button_h) {
		return;
	}
	const auto text_view = ctx.fonts.text.resolve();
	const auto width_of = [&](const std::string_view label) {
		return text_view->width(label, sty.font_size) + pad * 2.f;
	};
	const float row_top = area.top() - pad * 0.5f;
	const rectf close = rectf::from_position_size({ area.right() - pad - width_of(fork_close_label), row_top }, { width_of(fork_close_label), button_h });

	ctx.queue_sprite({
		.rect = area,
		.color = sty.panel_alt.fill,
		.texture = ctx.blank_texture,
	});

	if (s.fork.process) {
		const bool pending = s.fork.process->verdict == inquiry_verdict::pending;
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = ctx.intern(std::format("{} {} \xC2\xB7 read-only fork", pending ? "asking about" : "answer about", s.fork.about)),
			.position = { area.left() + pad, close.center().y() + text_view->vertical_center_offset(sty.font_size) },
			.scale = sty.font_size,
			.color = sty.color_text_secondary,
			.clip_rect = area,
		});
		const std::string_view said = pending ? std::string_view("The fork is reading the chat...")
			: s.fork.process->answer.empty() ? std::string_view("The fork ended without an answer.")
			: std::string_view(s.fork.process->answer);
		sync_plan_summary(ctx, s.fork.answer, said, area.width());
		const rectf answer = rectf::from_position_size({ area.left() + pad, close.bottom() - pad * 0.5f }, { std::max(0.f, area.width() - pad * 2.f), std::max(0.f, close.bottom() - pad * 0.5f - area.bottom()) });
		draw_plan_summary(ctx, s.fork.answer, answer, 1.f, gui::ids::make(std::format("##agent_fork_answer_{}", s.id)), ui.hot_widget_id, ui.active_widget_id, ui.focus_widget_id);
		if (ui.draw<gui::button>({
			.text = fork_close_label,
			.rect = close,
			.key = std::format("##agent_fork_close_{}", s.id),
		})) {
			end_inquiry(*s.fork.process);
			s.fork = {};
		}
		return;
	}

	if (!s.fork.open) {
		const std::string_view label = plan.fork_allowed ? fork_open_label : fork_busy_label;
		if (ui.draw<gui::button>({
			.text = label,
			.rect = rectf::from_position_size({ area.left() + pad, row_top }, { width_of(label), button_h }),
			.key = std::format("##agent_fork_open_{}", s.id),
			.enabled = plan.fork_allowed,
		})) {
			s.fork.open = true;
		}
		return;
	}

	const rectf ask = rectf::from_position_size({ close.left() - pad * 0.5f - width_of(fork_ask_label), row_top }, { width_of(fork_ask_label), button_h });
	const rectf input = rectf::from_position_size({ area.left() + pad, row_top }, { std::max(0.f, ask.left() - pad - area.left() - pad), button_h });
	ui.draw<gui::text_input>({
		.buffer = s.fork.question,
		.state = s.fork.input,
		.rect = input,
		.widget_id = gui::ids::make(std::format("##agent_fork_input_{}", s.id)),
		.font = ctx.fonts.text,
	});
	if (ui.draw<gui::button>({
		.text = fork_ask_label,
		.rect = ask,
		.key = std::format("##agent_fork_ask_{}", s.id),
		.enabled = plan.fork_allowed && !s.fork.question.empty(),
		.role = gui::button_role::accent,
	})) {
		s.fork.about = fork_about(paging.shown, life.cycle);
		s.fork.process = launch_inquiry(s, fork_prompt(s, paging.shown, life.cycle, s.fork.question));
		s.fork.open = false;
		s.fork.answer = {};
	}
	if (ui.draw<gui::button>({
		.text = fork_close_label,
		.rect = close,
		.key = std::format("##agent_fork_close_{}", s.id),
	})) {
		s.fork = {};
	}
}

auto gse::ide::agent::draw_lifecycle(gui::builder& ui, data& d, session& s, const chrome_plan& plan, const rectf& whole, const rectf& body, const channel_write<gui::menu_content, jump_to_request, set_cursor_shape_request> jump_out) -> void {
	const gui::draw_context& ctx = ui.ctx;
	const gui::style& sty = ctx.style;
	const float pad = sty.padding;
	split_memory& followup = split_of(d, split_kind::plan_followup);
	const float column = followup_width(ctx, s, plan, followup, whole.width());
	const rectf full = rectf::from_position_size(whole.top_left(), { whole.width() - column, whole.height() });
	draw_followup(ui, s, plan, followup, whole, column, body, jump_out);

	const since_digest since = since_of(s, s.arrived, s.entered);
	if (s.arriving) {
		s.arriving = false;
		const summary_anchor opening = opening_page(s, since);
		s.lifecycle_cycle = shown_goal(s) ? std::optional(opening.position) : std::nullopt;
		s.lifecycle_page = opening.page;
	}
	const lifecycle life = lifecycle_of(s);
	const plan_paging paging = paging_of(life.pages, s.lifecycle_page);
	s.lifecycle_page = paging.page - 1;

	const std::string digest = life.cycle.shown && s.arrived > 0 ? digest_line(since) : std::string{};
	const float button_h = sty.font_size * 1.4f;
	const float banner = ctx.presence(generate_temp_id(hash_of(s.id, "agent_since")), !digest.empty()) * (button_h + pad);
	const rectf banner_area = rectf::from_position_size(full.top_left(), { full.width(), banner });
	const float fork_h = fork_extent(ctx, s, life, paging, full.height() - banner);
	const rectf area = rectf::from_position_size({ full.left(), full.top() - banner }, { full.width(), std::max(0.f, full.height() - banner - fork_h) });
	const rectf fork_area = rectf::from_position_size({ full.left(), area.bottom() }, { full.width(), fork_h });

	if (!digest.empty() && banner >= button_h) {
		const float catch_w = ctx.fonts.text.resolve()->width(catch_up_label, sty.font_size) + pad * 2.f;
		const rectf catch_up = rectf::from_position_size({ banner_area.right() - pad - catch_w, banner_area.top() - pad * 0.5f }, { catch_w, button_h });
		draw_since_banner(ctx, digest, banner_area, catch_up);
		if (ui.draw<gui::button>({
			.text = catch_up_label,
			.rect = catch_up,
			.key = std::format("##agent_catch_up_{}", s.id),
		})) {
			s.lifecycle_cycle.reset();
			s.lifecycle_page = std::numeric_limits<std::uint32_t>::max();
			s.arrived = 0;
		}
	}

	if (paging.shown.kind == page_kind::review_loop) {
		sync_plan_summary(ctx, s.plan_summary, review_loop_text(life.review), area.width());
	}
	else if (paging.shown.kind == page_kind::report) {
		sync_plan_summary(ctx, s.plan_summary, life.report, area.width());
	}
	else if (paging.shown.kind == page_kind::errors) {
		sync_error_page(ctx, s.plan_summary, life.fix, area.width());
	}
	else if (paging.shown.kind == page_kind::verdict && life.steered) {
		sync_plan_summary(ctx, s.plan_summary, life.steered->summary, area.width());
	}
	else if (paging.shown.kind != page_kind::goal && !life.summary.empty()) {
		sync_plan_summary(ctx, s.plan_summary, life.summary, area.width());
	}
	const bool actionable = plan.plan_shown && paging.shown.phase == task_phase::scope && life.cycle.position == life.cycle.cycles;
	split_memory& columns = split_of(d, plan_split_kind(paging.shown.kind));
	plan_block laid = layout_plan(ctx, life.steps, life.progress, s.plan_summary.buffer.lines.size(), paging, actionable ? std::string_view(plan.action_label) : std::string_view{}, columns.ratio, area.width(), area.height());
	if (paging.shown.kind == page_kind::review_loop) {
		laid.head = review_loop_head(life.review);
	}
	if (paging.shown.kind == page_kind::report) {
		laid.head = report_head(life.progress, life.review);
		laid.diffs_link = edits_label(task_edits(life));
	}
	if (paging.shown.kind == page_kind::errors) {
		laid.head = errors_head(life.fix);
	}
	if (paging.shown.kind == page_kind::verdict && life.steered) {
		laid.head = std::format("steer \xC2\xB7 {}", ledger_head(*life.steered));
	}
	if (paging.shown.kind == page_kind::goal) {
		const goal_dashboard* goal = life.goal ? &*life.goal : nullptr;
		layout_goal(ctx, laid, goal, area.height());
		sync_ledger(ctx, s.plan_summary, goal, laid.files_w);
	}
	if (paging.shown.kind == page_kind::changes) {
		const std::optional<std::size_t> group = s.plan_selected ? std::optional<std::size_t>(*s.plan_selected) : std::nullopt;
		sync_step_diffs(ctx, s.step_diffs, s.step_rows, edits_in_group(life.edits, life.steps.size(), group), group, laid.files_w);
	}
	if (fresh_page(paging.shown, s.arrived)) {
		laid.head += " \xC2\xB7 new";
	}
	if (life.cycle.shown) {
		laid.cycle_label = cycle_label(life.cycle);
	}
	draw_plan(ui, s, columns, area, laid, s.lifecycle_page, "agent_lifecycle_page", jump_out);

	if (life.cycle.shown && area.height() >= laid.extent) {
		const plan_rects rects = plan_rects_of(ctx, laid, area);
		const std::uint32_t position = life.cycle.position;
		const auto go = [&](const std::uint32_t to) {
			s.lifecycle_cycle = to >= life.cycle.cycles ? std::nullopt : std::optional(to);
			s.lifecycle_page = 0;
			s.plan_selected.reset();
			if (to >= life.cycle.cycles) {
				s.arrived = 0;
			}
		};
		if (ui.draw<gui::button>({
			.text = cycle_back_label,
			.rect = rects.cycle_back,
			.key = std::format("##agent_cycle_back_{}", s.id),
			.enabled = position > 0,
		})) {
			go(position - 1);
		}
		if (ui.draw<gui::button>({
			.text = cycle_forward_label,
			.rect = rects.cycle_forward,
			.key = std::format("##agent_cycle_forward_{}", s.id),
			.enabled = position < life.cycle.cycles,
		})) {
			go(position + 1);
		}
	}
	if (fork_h > 0.f) {
		draw_fork(ui, s, plan, life, paging, fork_area);
	}
}

auto gse::ide::agent::draw_body_view(gui::builder& ui, data& d, session& s, const chrome_plan& plan, const rectf& area, const rectf& body, channel_write<gui::menu_content, jump_to_request, set_cursor_shape_request> jump_out) -> void {
	if (s.mode == transcript_mode::summary && plan.summary_ready) {
		draw_lifecycle(ui, d, s, plan, area, body, jump_out);
		return;
	}
	draw_transcript(ui, s, shown_view(s), area, body, jump_out);
}

auto gse::ide::agent::draw_view_toggles(gui::builder& ui, session& s, const chrome_plan& plan, const rectf& area) -> float {
	const gui::style& sty = ui.ctx.style;
	const float pad = sty.padding;
	const auto text_view = ui.ctx.fonts.text.resolve();

	constexpr std::string_view diffs_label = "Diffs";
	const mode_style shown = annotation_from_enum<mode_style>(s.mode, {});

	float widest_mode = 0.f;
	for (const transcript_mode mode : enum_values<transcript_mode>()) {
		widest_mode = std::max(widest_mode, text_view->width(annotation_from_enum<mode_style>(mode, {}).label, sty.font_size));
	}

	const float diffs_w = text_view->width(diffs_label, sty.font_size) + pad * 2.f;
	const float mode_full = widest_mode + pad * 2.f;
	const float mode_w = mode_full * ui.ctx.presence(generate_temp_id(hash_of(s.id, "agent_view_mode")), plan.toggle_shown);
	const float height = std::min(area.height(), sty.font_size * 1.4f);
	const float top = area.center().y() + height * 0.5f;

	const rectf diffs = rectf::from_position_size({ area.right() - pad - diffs_w, top }, { diffs_w, height });
	const rectf mode = rectf::from_position_size({ diffs.left() - pad * 0.5f - mode_w, top }, { mode_w, height });

	if (mode_w >= mode_full && ui.draw<gui::button>({
		.text = std::string_view(shown.label),
		.rect = mode,
		.key = std::format("##agent_view_mode_{}", s.id),
		.role = s.mode == transcript_mode::summary ?gui::button_role::accent : gui::button_role::ghost,
	})) {
		const auto modes = enum_values<transcript_mode>();
		const auto at = std::ranges::find(modes, s.mode);
		s.mode = std::next(at) == std::ranges::end(modes) ? *std::ranges::begin(modes) : *std::next(at);
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

auto gse::ide::agent::cancel_sync(session& s) -> void {
	stop_session(s);
	append_row(s, {
		.kind = row_kind::note,
		.text = "cancelled from the panel",
	});
	s.sync_report = "the sync was cancelled from the panel before it finished";
}

auto gse::ide::agent::draw_sync_header(gui::builder& ui, session& s, const rectf& area) -> void {
	const gui::draw_context& ctx = ui.ctx;
	const gui::style& sty = ctx.style;
	const float pad = sty.padding;
	const auto text_view = ctx.fonts.text.resolve();

	ctx.queue_sprite({
		.rect = area,
		.color = sty.panel_alt.fill,
		.texture = ctx.blank_texture,
	});
	ctx.queue_sprite({
		.rect = rectf::from_position_size({ area.left(), area.top() }, { sty.accent_bar_width, area.height() }),
		.color = sty.color_accent,
		.texture = ctx.blank_texture,
	});

	constexpr std::string_view cancel_label = "Cancel";
	const float button_w = text_view->width(cancel_label, sty.font_size) + pad * 2.f;
	const float button_h = std::min(area.height() - pad * 0.5f, sty.font_size * 1.6f);
	const rectf cancel = rectf::from_position_size(
		{ area.right() - pad - button_w, area.center().y() + button_h * 0.5f },
		{ button_w, button_h }
	);
	if (ui.draw<gui::button>({
		.text = cancel_label,
		.rect = cancel,
		.key = std::format("##sync_cancel_{}", s.id),
		.role = gui::button_role::danger,
	})) {
		cancel_sync(s);
	}

	const float text_left = area.left() + sty.accent_bar_width + pad;
	const std::string title = std::format("Syncing {} \xC2\xB7 {}", s.sync->root.filename().generic_display_string(), s.sync->branch);
	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = title,
		.position = { text_left, area.center().y() + text_view->vertical_center_offset(sty.font_size) },
		.scale = sty.font_size,
		.color = sty.color_text,
		.clip_rect = rectf::from_position_size({ text_left, area.top() }, { std::max(0.f, cancel.left() - pad - text_left), area.height() }),
	});
}

auto gse::ide::agent::draw_sync_chat(gui::builder& ui, data& d, const rectf& body, const channel_write<gui::menu_content, jump_to_request, set_cursor_shape_request> jump_out) -> void {
	session* hosted = sync_session(d);
	if (!hosted) {
		return;
	}

	session& s = *hosted;
	const chrome_plan plan = plan_chrome(d, s);
	const gui::draw_context& ctx = ui.ctx;
	const gui::style& sty = ctx.style;
	const id status_key = generate_temp_id(hash_of(s.id, "agent_status"));

	const float input_line_h = gui::text_area_line_height(ctx, ctx.fonts.text);
	const auto input_rows = static_cast<float>(std::clamp<std::size_t>(s.draft.line_count(), 1, max_input_rows));
	const float input_h = std::max(sty.font_size * 2.f, input_rows * input_line_h + sty.padding * 2.f) + gui::image_strip_extent(ctx, s.attachments);
	const float activity_h = sty.font_size * 1.75f * ctx.presence(generate_temp_id(hash_of(status_key.number(), "activity")), plan.activity_shown);
	const float header_h = sty.font_size * 2.f;
	const float context_h = std::max(2.f, std::round(3.f * sty.scale_factor));
	const question_block question = plan.question_shown
		? layout_question(ctx, *s.question, body.width(), body.height() * question_body_share)
		: question_block{};
	const float question_h = question_extent(ctx, s, plan, question);

	const rectf context_area = rectf::from_position_size({ body.left(), body.bottom() + context_h }, { body.width(), context_h });
	const rectf input_area = rectf::from_position_size({ body.left(), context_area.top() + input_h }, { body.width(), input_h });
	const rectf question_area = rectf::from_position_size({ body.left(), input_area.top() + question_h }, { body.width(), question_h });
	const rectf activity_area = rectf::from_position_size({ body.left(), question_area.top() + activity_h }, { body.width(), activity_h });
	const rectf header_area = rectf::from_position_size(body.top_left(), { body.width(), header_h });
	const rectf transcript = rectf::from_position_size(
		{ body.left(), header_area.bottom() },
		{ body.width(), std::max(0.f, body.height() - header_h - context_h - input_h - question_h - activity_h) }
	);

	draw_transcript(ui, s, shown_view(s), transcript, body, jump_out);
	if (activity_h > 0.f) {
		ctx.queue_sprite({
			.rect = activity_area,
			.color = sty.panel_alt.fill,
			.texture = ctx.blank_texture,
		});
		const rectf row = rectf::from_position_size(
			{ activity_area.left() + sty.padding, activity_area.top() },
			{ std::max(0.f, activity_area.width() - sty.padding), activity_area.height() }
		);
		draw_status_row(ctx, plan, status_key, row, activity_area);
	}
	if (question_h > 0.f) {
		draw_question(ui, s, question_area, question);
	}
	draw_input(ui, s, {
		.area = input_area,
		.body = body,
		.plan = plan,
	});
	draw_context_bar(ctx, s, context_area);
	draw_sync_header(ui, s, header_area);
}

auto gse::ide::agent::note_visits(data& d) -> void {
	const std::int64_t now = unix_now();
	for (session& s : d.sessions) {
		if (d.overview_active || s.id != d.active) {
			s.visiting = false;
			continue;
		}
		if (!s.visiting) {
			s.visiting = true;
			s.arriving = true;
			s.arrived = s.seen;
			s.entered = now;
		}
		s.seen = now;
	}
}

auto gse::ide::agent::draw_token_prompt(gui::builder& ui, data& d, const rectf& body) -> void {
	if (d.token_prompt_open) {
		d.token_input.password = true;
		const gui::prompt_result result = ui.draw<gui::prompt_dialog>({
			.body = body,
			.title = "Claude agent token",
			.message = d.token_error.empty() ? "Paste from claude setup-token. Saved for your Windows user." : d.token_error,
			.value = d.token_draft,
			.state = d.token_input,
			.submit_label = "Save token",
			.key = "##agent_token_prompt",
		});
		if (result == gui::prompt_result::submitted) {
			const auto first = d.token_draft.find_first_not_of(" \t\r\n");
			const auto last = d.token_draft.find_last_not_of(" \t\r\n");
			const std::string token = first == std::string::npos ? "" : d.token_draft.substr(first, last - first + 1);
			if (token.empty() || std::ranges::any_of(token, [](const unsigned char c) { return c <= 32 || c >= 127; })) {
				d.token_error = "Enter a token without spaces or line breaks.";
			} else if (!save_agent_token(token)) {
				d.token_error = "Could not save the token. Try again.";
			} else {
				d.token_prompt_open = false;
				d.usage = {};
			}
		} else if (result == gui::prompt_result::cancelled) {
			d.token_requests_cancelled = true;
			d.token_prompt_open = false;
		}
		if (!d.token_prompt_open) {
			d.token_draft.clear();
			d.token_error.clear();
			d.token_input = {};
		}
		return;
	}
}

auto gse::ide::agent::draw_panel(gui::builder& ui, data& d, const channel_write<gui::menu_content, jump_to_request, set_cursor_shape_request> jump_out) -> void {
	const gui::draw_context& ctx = ui.ctx;
	if (ctx.clip_stack.empty()) {
		return;
	}

	const gui::style& sty = ctx.style;
	const rectf host = ctx.clip_stack.back();
	if (take_token_prompt_request()) {
		d.token_prompt_open = true;
	}
	const bool missing_token = !agent_credentials().token;
	const float banner_h = missing_token ? sty.font_size * 2.f : 0.f;
	if (missing_token && ui.draw<gui::button>({
		.text = "Set up Claude to start agents...",
		.rect = rectf::from_position_size({ host.left(), host.top() }, { host.width(), banner_h }),
		.key = "##agent_token_setup",
	})) {
		d.token_prompt_open = true;
	}
	const rectf body = rectf::from_position_size({ host.left(), host.top() - banner_h }, { host.width(), std::max(0.f, host.height() - banner_h) });
	d.motion = ctx.motion_counts();
	const float strip_h = draw_session_tabs(ui, d, body);
	note_visits(d);

	if (d.overview_active) {
		draw_overview(ui, d, rectf::from_position_size(
			{ body.left(), body.top() - strip_h },
			{ body.width(), std::max(0.f, body.height() - strip_h) }
		));
		draw_session_info(ui, d, body);
		draw_history(ui, d, body);
		draw_close_confirm(ui, d, body);
		draw_new_chat_prompt(ui, d, body);
		draw_token_prompt(ui, d, host);
		return;
	}

	session* shown = active_session(d);
	const float input_line_h = gui::text_area_line_height(ctx, ctx.fonts.text);
	const auto input_rows = static_cast<float>(std::clamp<std::size_t>(shown ? shown->draft.line_count() : 1, 1, max_input_rows));
	const float attachments_h = shown ? gui::image_strip_extent(ctx, shown->attachments) : 0.f;
	const float input_h = std::max(sty.font_size * 2.f, input_rows * input_line_h + sty.padding * 2.f) + attachments_h;
	const float phase_h = shown ? sty.font_size * 1.75f : 0.f;
	const std::optional<chrome_plan> plan = shown ? std::optional(plan_chrome(d, *shown)) : std::nullopt;
	const question_block question = plan && plan->question_shown
		? layout_question(ctx, *shown->question, body.width(), body.height() * question_body_share)
		: question_block{};
	const float question_h = plan ? question_extent(ctx, *shown, *plan, question) : 0.f;
	const bool gate_block = plan && plan->plan_shown && shown->mode != transcript_mode::summary;
	split_memory& followup = split_of(d, split_kind::plan_followup);
	const float followup_w = gate_block ? followup_width(ctx, *shown, *plan, followup, body.width()) : 0.f;
	const float gate_w = body.width() - followup_w;
	if (gate_block && !plan->plan_summary.empty()) {
		sync_plan_summary(ctx, shown->plan_summary, plan->plan_summary, gate_w);
	}
	const float gate_room = body.height() * (plan && plan->plan_split ? plan_split_share : plan_body_share);
	plan_block steps = gate_block
		? layout_plan(ctx, plan->plan_steps, {}, shown->plan_summary.buffer.lines.size(), paging_of(scope_pages(plan->plan_summary, plan->plan_steps.size()), shown->plan_page), plan->action_label, split_of(d, split_kind::plan_files).ratio, gate_w, gate_room)
		: plan_block{};
	if (gate_block && plan->plan_split) {
		steps.extent = std::max(steps.extent, gate_room);
	}
	const float steps_h = plan ? plan_extent(ctx, *shown, *plan, steps) : 0.f;

	const float context_h = std::max(2.f, std::round(3.f * sty.scale_factor));

	const rectf context_area = rectf::from_position_size({ body.left(), body.bottom() + context_h }, { body.width(), context_h });
	const rectf input_area = rectf::from_position_size({ body.left(), context_area.top() + input_h }, { body.width(), input_h });
	const rectf question_area = rectf::from_position_size(
		{ body.left(), input_area.top() + question_h },
		{ body.width(), question_h }
	);
	const rectf steps_area = rectf::from_position_size(
		{ body.left(), question_area.top() + steps_h },
		{ body.width(), steps_h }
	);
	const rectf phase_area = rectf::from_position_size(
		{ body.left(), steps_area.top() + phase_h },
		{ body.width(), phase_h }
	);
	const rectf transcript = rectf::from_position_size(
		{ body.left(), body.top() - strip_h },
		{ body.width(), std::max(0.f, body.height() - strip_h - context_h - input_h - question_h - steps_h - phase_h) }
	);

	if (question_h > 0.f) {
		draw_question(ui, *shown, question_area, question);
	}
	if (steps_h > 0.f) {
		const rectf gate_area = rectf::from_position_size(steps_area.top_left(), { gate_w, steps_h });
		draw_plan(ui, *shown, split_of(d, plan_split_kind(steps.paging.shown.kind)), gate_area, steps, shown->plan_page, "agent_plan_page", jump_out);
		draw_followup(ui, *shown, *plan, followup, steps_area, followup_w, body, jump_out);
	}
	if (shown) {
		draw_status(ui, *shown, *plan, phase_area);
		draw_input(ui, *shown, { .area = input_area, .body = body, .plan = *plan });
		draw_context_bar(ctx, *shown, context_area);
	}

	if (!shown) {
		draw_transcript_hint(ctx, "No agents yet. Type a prompt below, or press + to add a session, in " + config::primary().name + ".", transcript);
	}
	else if (!shown->changes_open) {
		draw_body_view(ui, d, *shown, *plan, transcript, body, jump_out);
	}
	else {
		const gui::layout::split_result split = draw_split(ctx, transcript, shown->changes_ratio, shown->changes_drag, sty.font_size * 14.f, std::max(6.f, sty.resize_border_thickness) * 2.f, jump_out);

		draw_body_view(ui, d, *shown, *plan, split.first, body, jump_out);
		draw_transcript(ui, *shown, shown->changes, split.second, body, jump_out);
	}
	draw_session_info(ui, d, body);
	draw_history(ui, d, body);
	draw_close_confirm(ui, d, body);
	draw_new_chat_prompt(ui, d, body);
	draw_token_prompt(ui, d, host);
}
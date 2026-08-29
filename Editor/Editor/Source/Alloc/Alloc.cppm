export module gse.ide.alloc;

import std;
import gse;

export namespace gse::ide {
	struct alloc_view_state {
		std::vector<alloc::site> sites;
		std::unordered_map<std::uint64_t, std::string> labels;
		alloc::address_space usage;
		interval_timer<> refresh{ milliseconds(500.f) };
		int top_rows = 200;
	};

	auto draw_alloc_panel(
		gui::builder& ui,
		const rectf& rect,
		alloc_view_state& state
	) -> void;
}

namespace gse::ide {
	constexpr double bytes_per_megabyte = 1024.0 * 1024.0;

	auto megabytes_of(
		std::int64_t bytes
	) -> double;

	auto refresh_alloc_sites(
		alloc_view_state& state
	) -> void;

	auto draw_alloc_header(
		gui::builder& ui,
		const rectf& row,
		alloc_view_state& state
	) -> void;

	auto draw_alloc_row(
		gui::draw_context& ctx,
		const rectf& row,
		const alloc::site& entry,
		const std::string& label
	) -> void;

	auto draw_action(
		gui::draw_context& ctx,
		const rectf& rect,
		std::string_view text
	) -> bool;
}

auto gse::ide::megabytes_of(const std::int64_t bytes) -> double {
	return static_cast<double>(bytes) / bytes_per_megabyte;
}

auto gse::ide::refresh_alloc_sites(alloc_view_state& state) -> void {
	state.usage = alloc::address_space_usage();
	alloc::snapshot(state.sites);
	std::ranges::sort(state.sites, std::ranges::greater{}, &alloc::site::live_bytes);

	for (const auto& entry : state.sites | std::views::take(state.top_rows)) {
		if (!state.labels.contains(entry.pc)) {
			state.labels.emplace(entry.pc, alloc::label_of(entry.pc));
		}
	}
}

auto gse::ide::draw_action(gui::draw_context& ctx, const rectf& rect, const std::string_view text) -> bool {
	const auto& sty = ctx.style;
	const auto text_view = ctx.fonts.text.resolve();
	const bool hovered = ctx.hovers(rect);

	ctx.queue_sprite({
		.rect = rect,
		.color = hovered ? sty.color_button_hovered : sty.color_button_background,
		.texture = ctx.blank_texture,
		.corner_radius = sty.corner_radius,
	});

	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = text,
		.position = { rect.center().x() - text_view->width(text, sty.font_size) * 0.5f, rect.center().y() + text_view->vertical_center_offset(sty.font_size) },
		.scale = sty.font_size,
		.color = sty.color_text,
		.clip_rect = rect,
	});

	return hovered && ctx.mouse_pressed_for(rect);
}

auto gse::ide::draw_alloc_header(gui::builder& ui, const rectf& row, alloc_view_state& state) -> void {
	auto& ctx = ui.ctx;
	const auto& sty = ctx.style;
	const auto text_view = ctx.fonts.text.resolve();
	const float pad = sty.padding;

	const float button_w = text_view->width("Reset baseline", sty.font_size) + pad * 2.f;
	const rectf mark_rect = rectf::from_position_size(
		{ row.right() - pad - button_w, row.top() - pad * 0.5f },
		{ button_w, std::max(0.f, row.height() - pad) }
	);
	if (draw_action(ctx, mark_rect, "Reset baseline")) {
		alloc::mark();
	}

	const float toggle_w = text_view->width("Sampling paused", sty.font_size) + pad * 2.f;
	const rectf toggle_rect = rectf::from_position_size(
		{ mark_rect.left() - pad - toggle_w, row.top() - pad * 0.5f },
		{ toggle_w, std::max(0.f, row.height() - pad) }
	);
	if (draw_action(ctx, toggle_rect, alloc::enabled() ? "Sampling on" : "Sampling paused")) {
		alloc::set_enabled(!alloc::enabled());
	}

	const float report_w = text_view->width("Write report to log", sty.font_size) + pad * 2.f;
	const rectf report_rect = rectf::from_position_size(
		{ toggle_rect.left() - pad - report_w, row.top() - pad * 0.5f },
		{ report_w, std::max(0.f, row.height() - pad) }
	);
	if (draw_action(ctx, report_rect, "Write report to log")) {
		alloc::log_report(state.top_rows);
	}

	const float line_h = text_view->line_height(sty.font_size);

	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = std::format(
			"process   {:.1f} MB private   {:.1f} MB image   {:.1f} MB mapped",
			megabytes_of(state.usage.private_committed),
			megabytes_of(state.usage.image),
			megabytes_of(state.usage.mapped)
		),
		.position = { row.left() + pad, row.top() - line_h },
		.scale = sty.font_size,
		.color = sty.color_text,
		.clip_rect = row,
	});

	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = std::format(
			"tracked   {:.1f} MB estimated live   {} samples   {} evicted   1 per {} KB",
			megabytes_of(alloc::estimated_live_bytes()),
			alloc::live_samples(),
			alloc::evicted_samples(),
			alloc::sample_interval() / 1024
		),
		.position = { row.left() + pad, row.top() - line_h * 2.f - pad * 0.5f },
		.scale = sty.font_size,
		.color = sty.color_text_secondary,
		.clip_rect = row,
	});
}

auto gse::ide::draw_alloc_row(gui::draw_context& ctx, const rectf& row, const alloc::site& entry, const std::string& label) -> void {
	const auto& sty = ctx.style;
	const auto code_view = ctx.fonts.code.resolve();
	const float pad = sty.padding;
	const float fs = sty.font_size;
	const float baseline = row.center().y() + code_view->vertical_center_offset(fs);

	const std::string live = std::format("{:>10.2f} MB", megabytes_of(entry.live_bytes));
	ctx.queue_text({
		.font = ctx.fonts.code,
		.text = live,
		.position = { row.left() + pad, baseline },
		.scale = fs,
		.color = sty.color_text,
		.clip_rect = row,
	});

	const std::string growth = std::format("{:>+10.2f} MB", megabytes_of(entry.since_mark_bytes));
	const float growth_x = row.left() + pad + code_view->width(live, fs) + pad * 2.f;
	ctx.queue_text({
		.font = ctx.fonts.code,
		.text = growth,
		.position = { growth_x, baseline },
		.scale = fs,
		.color = entry.since_mark_bytes > 0 ? sty.color_accent : sty.color_text_secondary,
		.clip_rect = row,
	});

	const float label_x = growth_x + code_view->width(growth, fs) + pad * 2.f;
	ctx.queue_text({
		.font = ctx.fonts.code,
		.text = label,
		.position = { label_x, baseline },
		.scale = fs,
		.color = sty.color_text_secondary,
		.clip_rect = row,
	});
}

auto gse::ide::draw_alloc_panel(gui::builder& ui, const rectf& rect, alloc_view_state& state) -> void {
	auto& ctx = ui.ctx;
	const auto& sty = ctx.style;
	const auto code_view = ctx.fonts.code.resolve();
	const float pad = sty.padding;
	const float row_h = code_view->line_height(sty.font_size) + pad;

	if (state.sites.empty() || state.refresh.tick()) {
		refresh_alloc_sites(state);
	}

	const rectf header_rect = rectf::from_position_size(
		{ rect.left(), rect.top() },
		{ rect.width(), std::min(row_h * 2.f + pad, rect.height()) }
	);
	draw_alloc_header(ui, header_rect, state);

	const rectf list_rect = rectf::from_position_size(
		{ rect.left(), header_rect.bottom() },
		{ rect.width(), std::max(0.f, rect.height() - header_rect.height()) }
	);

	if (state.sites.empty()) {
		const auto text_view = ctx.fonts.text.resolve();
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = alloc::enabled() ? "No sampled allocations yet." : "Sampling is paused.",
			.position = { list_rect.left() + pad, list_rect.top() - row_h },
			.scale = sty.font_size,
			.color = sty.color_text_secondary,
			.clip_rect = list_rect,
		});
		return;
	}

	ui.row_list({
		.id = "##alloc_site_list",
		.bounds = list_rect,
		.row_height = row_h,
		.row_count = std::min(state.sites.size(), static_cast<std::size_t>(state.top_rows)),
	}, [&](gui::builder& b, const gui::row& r) {
		const alloc::site& entry = state.sites[r.index];
		draw_alloc_row(b.ctx, r.rect, entry, state.labels.at(entry.pc));
	});
}
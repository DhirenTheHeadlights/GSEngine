export module gse.ide.lint_panel;

import std;
import gse;

import gse.ide.diagnostic;
import gse.ide.lint;
import gse.ide.search;
import gse.ide.navigation;

export namespace gse::ide {
	struct lint_row {
		std::uint32_t site = 0;
		std::string message;
		std::string location;
	};

	struct lint_rule_group {
		lint_rule rule = lint_rule::redundant_namespace_qualifier;
		std::vector<lint_row> rows;
		std::size_t file_count = 0;
		bool expanded = false;
	};

	struct lint_panel_state {
		std::shared_ptr<const search::lint_snapshot> snapshot;
		std::vector<lint_rule_group> groups;
		std::optional<lint_rule> confirming;
	};

	auto draw_lint_panel(
		gui::builder& ui,
		const rectf& rect,
		lint_panel_state& state,
		const search::index_state* index,
		channel_write<jump_to_request, apply_lint_request> channels
	) -> void;
}

namespace gse::ide {
	auto sync_lint_groups(
		lint_panel_state& state,
		const search::index_state* index
	) -> void;

	struct lint_row_ref {
		std::size_t group = 0;
		std::optional<std::size_t> site;
	};

	auto lint_row_count(
		std::span<const lint_rule_group> groups
	) -> std::size_t;

	auto resolve_lint_row(
		std::span<const lint_rule_group> groups,
		std::size_t index
	) -> lint_row_ref;

	auto edits_for_group(
		const search::lint_snapshot& snapshot,
		const lint_rule_group& group
	) -> std::vector<lint_file_edits>;

	auto draw_lint_rule_row(
		const gui::draw_context& ctx,
		const rectf& row,
		const lint_rule_group& group,
		bool hovered,
		float actions_width
	) -> void;

	auto draw_lint_site_row(
		const gui::draw_context& ctx,
		const rectf& row,
		const lint_row& item,
		bool hovered
	) -> void;
}

auto gse::ide::sync_lint_groups(lint_panel_state& state, const search::index_state* index) -> void {
	const std::shared_ptr<const search::search_snapshot> current = index->query_snapshot();
	const std::shared_ptr<const search::lint_snapshot> lints = current->lints;
	if (state.snapshot && state.snapshot->symbol_generation == lints->symbol_generation) {
		return;
	}

	std::vector<lint_rule_group> groups;
	for (const lint_rule rule : enum_values<lint_rule>()) {
		groups.push_back({
			.rule = rule,
		});
	}
	for (const lint_rule_group& previous : state.groups) {
		for (lint_rule_group& group : groups) {
			if (group.rule == previous.rule) {
				group.expanded = previous.expanded;
				break;
			}
		}
	}

	const std::span<const search::lint_site> sites = *lints->sites;
	for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(sites.size()); ++i) {
		const search::lint_site& site = sites[i];
		for (lint_rule_group& group : groups) {
			if (group.rule != site.rule) {
				continue;
			}
			group.rows.push_back({
				.site = i,
				.message = lint::message_for(site.rule, site.edit),
				.location = std::format("{}:{}", site.path.filename().generic_display_string(), site.edit.line + 1),
			});
			break;
		}
	}

	for (lint_rule_group& group : groups) {
		std::unordered_set<std::string> files;
		for (const lint_row& row : group.rows) {
			files.insert(sites[row.site].path.generic_native_encoded_string());
		}
		group.file_count = files.size();
	}

	state.snapshot = lints;
	state.groups = std::move(groups);
}

auto gse::ide::lint_row_count(const std::span<const lint_rule_group> groups) -> std::size_t {
	std::size_t count = 0;
	for (const lint_rule_group& group : groups) {
		count += 1 + (group.expanded ? group.rows.size() : 0);
	}
	return count;
}

auto gse::ide::resolve_lint_row(const std::span<const lint_rule_group> groups, const std::size_t index) -> lint_row_ref {
	std::size_t remaining = index;
	for (std::size_t g = 0; g < groups.size(); ++g) {
		if (remaining == 0) {
			return { .group = g };
		}
		--remaining;
		const std::size_t sites = groups[g].expanded ? groups[g].rows.size() : 0;
		if (remaining < sites) {
			return { .group = g, .site = remaining };
		}
		remaining -= sites;
	}
	return {};
}

auto gse::ide::edits_for_group(const search::lint_snapshot& snapshot, const lint_rule_group& group) -> std::vector<lint_file_edits> {
	const std::span<const search::lint_site> sites = *snapshot.sites;
	std::vector<lint_file_edits> out;
	for (const lint_row& row : group.rows) {
		const search::lint_site& site = sites[row.site];
		const auto existing = std::ranges::find_if(out, [&site](const lint_file_edits& entry) {
			return entry.path == site.path;
		});
		if (existing == out.end()) {
			out.push_back({
				.path = site.path,
				.edits = { site.edit },
			});
			continue;
		}
		existing->edits.push_back(site.edit);
	}
	return out;
}

auto gse::ide::draw_lint_rule_row(const gui::draw_context& ctx, const rectf& row, const lint_rule_group& group, const bool hovered, const float actions_width) -> void {
	const auto& sty = ctx.style;
	const auto text_view = ctx.fonts.text.resolve();
	const float pad = sty.padding;
	const lint_rule_info info = annotation_from_enum<lint_rule_info>(group.rule, {});

	ctx.queue_sprite({
		.rect = row,
		.color = hovered ? sty.color_widget_hovered : sty.color_panel_alt,
		.texture = ctx.blank_texture,
		.corner_radius = sty.corner_radius,
	});

	const std::string_view chevron = group.expanded ? "v" : ">";
	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = chevron,
		.position = { row.left() + pad, row.center().y() + text_view->vertical_center_offset(sty.font_size) },
		.scale = sty.font_size,
		.color = sty.color_text_secondary,
		.clip_rect = row,
	});

	const std::string count = group.rows.empty()
		? std::string("clean")
		: std::format("{} in {} files", group.rows.size(), group.file_count);
	const float count_w = text_view->width(count, sty.font_size);
	const float title_x = row.left() + pad * 3.f;
	const float content_right = row.right() - actions_width;
	const rectf content = rectf::from_position_size(
		{ row.left(), row.top() },
		{ std::max(0.f, content_right - row.left()), row.height() }
	);

	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = std::string_view(info.title),
		.position = { title_x, row.center().y() + text_view->vertical_center_offset(sty.font_size) },
		.scale = sty.font_size,
		.color = group.rows.empty() ? sty.color_text_secondary : sty.color_text,
		.clip_rect = rectf::from_position_size({ title_x, row.top() }, { std::max(0.f, content_right - count_w - pad * 2.f - title_x), row.height() }),
	});
	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = count,
		.position = { content_right - count_w - pad, row.center().y() + text_view->vertical_center_offset(sty.font_size) },
		.scale = sty.font_size,
		.color = sty.color_text_secondary,
		.clip_rect = content,
	});
}

auto gse::ide::draw_lint_site_row(const gui::draw_context& ctx, const rectf& row, const lint_row& item, const bool hovered) -> void {
	const auto& sty = ctx.style;
	const auto text_view = ctx.fonts.text.resolve();
	const float pad = sty.padding;

	if (hovered) {
		ctx.queue_sprite({
			.rect = row,
			.color = sty.color_widget_hovered,
			.texture = ctx.blank_texture,
		});
	}

	const float location_w = text_view->width(item.location, sty.font_size);
	const float text_x = row.left() + pad * 4.f;

	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = item.message,
		.position = { text_x, row.center().y() + text_view->vertical_center_offset(sty.font_size) },
		.scale = sty.font_size,
		.color = sty.color_text,
		.clip_rect = rectf::from_position_size({ text_x, row.top() }, { std::max(0.f, row.right() - location_w - pad * 2.f - text_x), row.height() }),
	});
	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = item.location,
		.position = { row.right() - location_w - pad, row.center().y() + text_view->vertical_center_offset(sty.font_size) },
		.scale = sty.font_size,
		.color = sty.color_text_secondary,
		.clip_rect = row,
	});
}

auto gse::ide::draw_lint_panel(gui::builder& ui, const rectf& rect, lint_panel_state& state, const search::index_state* index, const channel_write<jump_to_request, apply_lint_request> channels) -> void {
	auto& ctx = ui.ctx;
	const auto& sty = ctx.style;
	const auto text_view = ctx.fonts.text.resolve();
	const float pad = sty.padding;
	const float row_h = text_view->line_height(sty.font_size) + pad;

	if (!index) {
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = "The semantic index is not available yet.",
			.position = { rect.left() + pad, rect.top() - row_h },
			.scale = sty.font_size,
			.color = sty.color_text_secondary,
			.clip_rect = rect,
		});
		return;
	}

	sync_lint_groups(state, index);

	if (state.confirming) {
		const auto target = std::ranges::find_if(state.groups, [&state](const lint_rule_group& group) {
			return group.rule == *state.confirming;
		});
		const lint_rule_info info = annotation_from_enum<lint_rule_info>(*state.confirming, {});
		const gui::confirm_result result = ui.draw<gui::confirm_dialog>({
			.body = rect,
			.title = "Apply to whole project?",
			.message = std::format("{} rewrites {} sites across {} files.", std::string_view(info.title), target->rows.size(), target->file_count),
			.confirm_label = "Apply",
			.key = "##lint_apply_confirm",
		});
		if (result == gui::confirm_result::confirmed) {
			channels.push<apply_lint_request>({
				.files = edits_for_group(*state.snapshot, *target),
			});
			state.confirming.reset();
		}
		else if (result == gui::confirm_result::cancelled) {
			state.confirming.reset();
		}
		return;
	}

	std::size_t total = 0;
	for (const lint_rule_group& group : state.groups) {
		total += group.rows.size();
	}

	const rectf header_rect = rectf::from_position_size(
		{ rect.left(), rect.top() },
		{ rect.width(), std::min(row_h + pad * 0.5f, rect.height()) }
	);
	ctx.queue_sprite({
		.rect = header_rect,
		.color = sty.color_panel_alt,
		.texture = ctx.blank_texture,
	});
	const search::index_phase phase = index->phase.load(std::memory_order_acquire);
	const bool analyzing = phase != search::index_phase::idle;
	const bool never_analyzed = !state.snapshot || state.snapshot->symbol_generation == 0;
	const bool incomplete = analyzing || never_analyzed;
	std::string summary;
	if (analyzing) {
		const search::index_phase_info info = annotation_from_enum<search::index_phase_info>(phase, {
			.label = "Analyzing",
			.detail = "No explanation was recorded for this indexing stage.",
		});
		const std::size_t scanned_total = index->progress_total.load(std::memory_order_acquire);
		const std::size_t scanned_done = std::min(index->progress_done.load(std::memory_order_acquire), scanned_total);
		summary = std::string(info.label);
		if (scanned_total > 0) {
			summary += std::format(" {}/{}", scanned_done, scanned_total);
		}
		summary += std::format(" \xC2\xB7 {} findings so far", total);
	}
	else if (never_analyzed) {
		summary = std::format("Not analyzed yet \xC2\xB7 {} findings", total);
	}
	else {
		summary = std::format("{} findings in {} rules", total, state.groups.size());
	}
	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = summary,
		.position = { header_rect.left() + pad, header_rect.center().y() + text_view->vertical_center_offset(sty.font_size) },
		.scale = sty.font_size,
		.color = incomplete ? sty.color_accent : sty.color_text_secondary,
		.clip_rect = header_rect,
	});

	const rectf list_rect = rectf::from_position_size(
		{ rect.left(), header_rect.bottom() },
		{ rect.width(), std::max(0.f, rect.height() - header_rect.height()) }
	);
	if (total == 0) {
		const std::string_view empty_text = analyzing
			? "Still analyzing the project, so this list is incomplete."
			: never_analyzed
				? "The project has not been analyzed yet."
				: "No lint findings.";
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = empty_text,
			.position = { list_rect.left() + pad, list_rect.top() - row_h },
			.scale = sty.font_size,
			.color = incomplete ? sty.color_accent : sty.color_text_secondary,
			.clip_rect = list_rect,
		});
		return;
	}

	const float apply_w = text_view->width("Apply all", sty.font_size) + pad * 2.f;
	std::optional<std::size_t> toggled;

	ui.row_list({
		.id = "##lint_list",
		.bounds = list_rect,
		.row_height = row_h,
		.row_count = lint_row_count(state.groups),
	}, [&](gui::builder& b, const gui::row& r) {
		auto& c = b.ctx;
		const lint_row_ref ref = resolve_lint_row(state.groups, r.index);
		const lint_rule_group& group = state.groups[ref.group];

		if (ref.site) {
			const lint_row& item = group.rows[*ref.site];
			if (c.clicked_in_rect(r.visible)) {
				const search::lint_site& site = (*state.snapshot->sites)[item.site];
				channels.push<jump_to_request>({
					.path = site.path,
					.line = site.edit.line,
					.column = site.edit.start_col,
					.end_line = site.edit.end_line,
					.end_column = site.edit.end_col,
				});
			}
			draw_lint_site_row(c, r.rect, item, r.hovered);
			return;
		}

		const rectf apply_rect = rectf::from_position_size(
			{ r.rect.right() - apply_w - pad, r.rect.top() - pad * 0.25f },
			{ apply_w, std::max(0.f, r.rect.height() - pad * 0.5f) }
		);
		const bool actionable = !group.rows.empty();
		const bool over_apply = actionable && apply_rect.contains(c.mouse_position());

		draw_lint_rule_row(c, r.rect, group, r.hovered && !over_apply, actionable ? apply_w + pad * 2.f : 0.f);

		if (actionable && b.draw<gui::button>({
			.text = "Apply all",
			.rect = apply_rect,
			.key = std::format("##lint_apply_{}", enum_to_string(group.rule)),
			.role = gui::button_role::danger,
		})) {
			state.confirming = group.rule;
		}
		if (!over_apply && c.clicked_in_rect(r.visible)) {
			toggled = ref.group;
		}
	});

	if (toggled) {
		lint_rule_group& group = state.groups[*toggled];
		group.expanded = !group.expanded;
	}
}

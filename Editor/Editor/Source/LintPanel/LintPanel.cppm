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

	auto edits_for_group(
		const search::lint_snapshot& snapshot,
		const lint_rule_group& group
	) -> std::vector<lint_file_edits>;

	auto draw_lint_rule_row(
		const gui::draw_context& ctx,
		const rectf& row,
		const lint_rule_group& group,
		bool hovered
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

auto gse::ide::draw_lint_rule_row(const gui::draw_context& ctx, const rectf& row, const lint_rule_group& group, const bool hovered) -> void {
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

	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = std::string_view(info.title),
		.position = { title_x, row.center().y() + text_view->vertical_center_offset(sty.font_size) },
		.scale = sty.font_size,
		.color = group.rows.empty() ? sty.color_text_secondary : sty.color_text,
		.clip_rect = rectf::from_position_size({ title_x, row.top() }, { std::max(0.f, row.right() - count_w - pad * 2.f - title_x), row.height() }),
	});
	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = count,
		.position = { row.right() - count_w - pad, row.center().y() + text_view->vertical_center_offset(sty.font_size) },
		.scale = sty.font_size,
		.color = sty.color_text_secondary,
		.clip_rect = row,
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
		const gui::draw::confirm_result result = gui::draw::confirm_dialog(ui, {
			.body = rect,
			.title = "Apply to whole project?",
			.message = std::format("{} rewrites {} sites across {} files.", std::string_view(info.title), target->rows.size(), target->file_count),
			.confirm_label = "Apply",
			.key = "##lint_apply_confirm",
		});
		if (result == gui::draw::confirm_result::confirmed) {
			channels.push<apply_lint_request>({
				.files = edits_for_group(*state.snapshot, *target),
			});
			state.confirming.reset();
		}
		else if (result == gui::draw::confirm_result::cancelled) {
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
	const std::string summary = std::format("{} findings in {} rules", total, state.groups.size());
	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = summary,
		.position = { header_rect.left() + pad, header_rect.center().y() + text_view->vertical_center_offset(sty.font_size) },
		.scale = sty.font_size,
		.color = sty.color_text_secondary,
		.clip_rect = header_rect,
	});

	const rectf list_rect = rectf::from_position_size(
		{ rect.left(), header_rect.bottom() },
		{ rect.width(), std::max(0.f, rect.height() - header_rect.height()) }
	);
	ctx.layout_cursor = { list_rect.left(), list_rect.top() };

	ui.scroll_region({
		.id = "##lint_list",
		.size = list_rect.size(),
	}, [&](gui::builder& b) {
		auto& c = b.ctx;
		for (lint_rule_group& group : state.groups) {
			const rectf row = rectf::from_position_size(
				{ list_rect.left(), c.layout_cursor.y() },
				{ list_rect.width(), row_h }
			);
			const rectf visible = row.intersection(list_rect);
			const bool hovered = visible.height() > 0.f && c.hovers(visible);

			const float apply_w = text_view->width("Apply all", sty.font_size) + pad * 2.f;
			const rectf apply_rect = rectf::from_position_size(
				{ row.right() - apply_w - pad, row.top() - pad * 0.25f },
				{ apply_w, std::max(0.f, row.height() - pad * 0.5f) }
			);

			const bool actionable = !group.rows.empty() && visible.height() > 0.f;
			const bool over_apply = actionable && apply_rect.contains(c.mouse_position());

			draw_lint_rule_row(c, row, group, hovered && !over_apply);

			if (actionable && gui::draw::button_in_rect(c, {
				.rect = apply_rect,
				.label = "Apply all",
				.key = std::format("##lint_apply_{}", enum_to_string(group.rule)),
				.role = gui::button_role::danger,
			}, b.hot_widget_id, b.active_widget_id)) {
				state.confirming = group.rule;
			}
			if (hovered && !over_apply && c.mouse_pressed_for(visible)) {
				group.expanded = !group.expanded;
			}
			c.layout_cursor.y() -= row_h;

			if (!group.expanded) {
				continue;
			}
			for (const lint_row& item : group.rows) {
				const rectf site_row = rectf::from_position_size(
					{ list_rect.left(), c.layout_cursor.y() },
					{ list_rect.width(), row_h }
				);
				const rectf site_visible = site_row.intersection(list_rect);
				const bool site_hovered = site_visible.height() > 0.f && c.hovers(site_visible);
				if (site_hovered && c.mouse_pressed_for(site_visible)) {
					const search::lint_site& site = (*state.snapshot->sites)[item.site];
					channels.push<jump_to_request>({
						.path = site.path,
						.line = site.edit.line,
						.column = site.edit.start_col,
					});
				}
				draw_lint_site_row(c, site_row, item, site_hovered);
				c.layout_cursor.y() -= row_h;
			}
		}
	});
}

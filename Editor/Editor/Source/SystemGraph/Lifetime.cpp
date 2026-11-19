module gse.ide.graph;

import gse;
import gse.graph;
import gse.ide.navigation;
import std;

namespace gse::ide {
	constexpr auto access_info(
		data_access access
	) -> data_access_info;

	auto merge_access(
		data_access current,
		data_access incoming
	) -> data_access;
}

constexpr auto gse::ide::access_info(const data_access access) -> data_access_info {
	return annotation_from_enum(access, data_access_info{
		.verb = "accesses",
	});
}

auto gse::ide::merge_access(const data_access current, const data_access incoming) -> data_access {
	const bool reads = current == data_access::read || incoming == data_access::read || current == data_access::read_write;
	const bool writes = current == data_access::write || incoming == data_access::write || current == data_access::read_write;
	return reads && writes ? data_access::read_write : incoming;
}

auto gse::ide::build_lifetime(graph_data& gd) -> void {
	const std::size_t n = gd.snapshot.nodes.size();
	gd.lifetime_columns.resize(n);
	std::ranges::iota(gd.lifetime_columns, 0u);
	std::ranges::sort(gd.lifetime_columns, [&](const std::uint32_t a, const std::uint32_t b) {
		const graph::placement pa = gd.layout.placements[a];
		const graph::placement pb = gd.layout.placements[b];
		return std::tie(pa.layer, pa.order) < std::tie(pb.layer, pb.order);
	});

	std::map<std::string, std::size_t> row_of;
	gd.lifetime_rows.clear();
	const auto add = [&](const std::string& qualified, const std::uint32_t column, const data_access access, const bool channel) {
		auto [it, inserted] = row_of.try_emplace(qualified, gd.lifetime_rows.size());
		if (inserted) {
			gd.lifetime_rows.push_back({
				.qualified = qualified,
				.display = pretty_name(qualified),
				.channel = channel,
			});
		}
		std::vector<lifetime_cell>& cells = gd.lifetime_rows[it->second].cells;
		const auto existing = std::ranges::find(cells, column, &lifetime_cell::column);
		if (existing == cells.end()) {
			cells.push_back({
				.column = column,
				.access = access,
			});
			return;
		}
		existing->access = merge_access(existing->access, access);
	};

	for (std::uint32_t column = 0; column < n; ++column) {
		const introspection::graph_node& node = gd.snapshot.nodes[gd.lifetime_columns[column]];
		for (const std::string& name : node.reads) {
			add(name, column, data_access::read, false);
		}
		for (const std::string& name : node.writes) {
			add(name, column, data_access::write, false);
		}
		for (const std::string& name : node.publishes) {
			add(name, column, data_access::publish, true);
		}
		for (const std::string& name : node.consumes) {
			add(name, column, data_access::consume, true);
		}
	}
	for (lifetime_row& row : gd.lifetime_rows) {
		std::ranges::sort(row.cells, {}, &lifetime_cell::column);
	}
	std::ranges::sort(gd.lifetime_rows, [](const lifetime_row& a, const lifetime_row& b) {
		const std::uint32_t fa = a.cells.front().column;
		const std::uint32_t fb = b.cells.front().column;
		return std::tie(a.channel, fa, a.display) < std::tie(b.channel, fb, b.display);
	});
	gd.lifetime_selected_row = std::nullopt;
}

auto gse::ide::draw_lifetime(gui::builder& ui, const rectf& area, graph_data& gd, channel_write<jump_to_request, set_cursor_shape_request> channels) -> void {
	gui::draw_context& ctx = ui.ctx;
	const auto text_view = ctx.fonts.text.resolve();
	const float fs = ctx.style.font_size;
	const float pad = 10.f;
	const float row_h = fs + 8.f;
	const vec2f mouse = ctx.mouse_position();

	ctx.queue_sprite({
		.rect = area,
		.color = { 0.09f, 0.10f, 0.13f, 1.f },
		.texture = ctx.blank_texture,
	});

	float chip_x = area.left() + pad;
	const float chip_top = area.top() - pad;
	for (std::size_t g = 0; g < gd.groups.size(); ++g) {
		const graph_group& group = gd.groups[g];
		const float cw = text_view->width(group.name, fs) + 16.f;
		const rectf chip = rectf::from_position_size({ chip_x, chip_top }, { cw, row_h });
		const bool on = gd.lifetime_group_visible[g];
		if (ctx.clicked_in_rect(chip)) {
			gd.lifetime_group_visible[g] = !on;
		}
		const vec4f c = group.color;
		ctx.queue_sprite({
			.rect = chip,
			.color = on ? vec4f{ c.x(), c.y(), c.z(), ctx.hovers(chip) ? 0.75f : 0.55f } : vec4f{ c.x() * 0.3f, c.y() * 0.3f, c.z() * 0.3f, 0.5f },
			.texture = ctx.blank_texture,
			.corner_radius = row_h * 0.3f,
		});
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = group.name,
			.position = { chip.left() + 8.f, chip.center().y() + text_view->vertical_center_offset(fs) },
			.scale = fs,
			.color = on ? ctx.style.color_text : ctx.style.color_text_secondary,
		});
		chip_x += cw + 6.f;
	}

	std::vector<std::optional<std::uint32_t>> slot_of(gd.lifetime_columns.size());
	std::vector<std::uint32_t> shown;
	for (std::uint32_t column = 0; column < gd.lifetime_columns.size(); ++column) {
		if (gd.lifetime_group_visible[gd.group_of[gd.lifetime_columns[column]]]) {
			slot_of[column] = static_cast<std::uint32_t>(shown.size());
			shown.push_back(column);
		}
	}
	if (shown.empty()) {
		return;
	}

	float label_w = 0.f;
	for (const lifetime_row& row : gd.lifetime_rows) {
		label_w = std::max(label_w, text_view->width(row.display, fs));
	}
	label_w = std::min(label_w + pad * 2.f, area.width() * 0.35f);

	const float grid_left = area.left() + label_w;
	const float col_w = std::max(4.f, (area.right() - pad - grid_left) / static_cast<float>(shown.size()));
	const auto column_x = [&](const std::uint32_t slot) {
		return grid_left + (static_cast<float>(slot) + 0.5f) * col_w;
	};

	const std::optional<std::size_t> selected = gd.lifetime_selected_row;
	std::vector<bool> column_lit(gd.lifetime_columns.size(), false);
	if (selected) {
		for (const lifetime_cell& cell : gd.lifetime_rows[*selected].cells) {
			column_lit[cell.column] = true;
		}
	}

	const float header_top = chip_top - row_h - pad;
	const float header_h = row_h;
	std::optional<std::uint32_t> hovered_column;
	for (std::uint32_t slot = 0; slot < shown.size(); ++slot) {
		const std::uint32_t column = shown[slot];
		const std::uint32_t system = gd.lifetime_columns[column];
		const rectf cell = rectf::from_position_size({ grid_left + static_cast<float>(slot) * col_w, header_top }, { col_w, header_h });
		const vec4f c = gd.groups[gd.group_of[system]].color;
		const bool lit = !selected || column_lit[column];
		ctx.queue_sprite({
			.rect = cell.inset({ 1.f, 1.f }),
			.color = lit ? c : vec4f{ c.x() * 0.3f, c.y() * 0.3f, c.z() * 0.3f, 0.6f },
			.texture = ctx.blank_texture,
			.corner_radius = 2.f,
		});
		if (ctx.hovers(cell)) {
			hovered_column = column;
			channels.push<set_cursor_shape_request>({
				.shape = cursor_shape::hand,
			});
		}
		const introspection::graph_node& node = gd.snapshot.nodes[system];
		if (ctx.clicked_in_rect(cell) && !node.file.empty()) {
			channels.push<jump_to_request>({
				.path = node.file,
				.line = node.line,
				.column = node.column,
			});
		}
	}

	const rectf rows_area = rectf({
		.min = { area.left(), area.bottom() },
		.max = { area.right(), header_top - header_h - 4.f },
	});
	std::optional<std::size_t> hovered_row;
	{
		const gui::layout::within_scope _ = gui::layout::within(ctx, rows_area);
		const gui::scroll_handle _ = gui::scroll_region(ctx, {
			.id = "graph_lifetime",
		});
		for (std::size_t r = 0; r < gd.lifetime_rows.size(); ++r) {
			const lifetime_row& row = gd.lifetime_rows[r];
			std::optional<std::uint32_t> first;
			std::optional<std::uint32_t> last;
			for (const lifetime_cell& cell : row.cells) {
				if (const std::optional<std::uint32_t> slot = slot_of[cell.column]) {
					first = first ? first : slot;
					last = slot;
				}
			}
			if (!first) {
				continue;
			}
			const rectf line = gui::layout::reserve_row(ctx, row_h);
			const rectf full = rectf({
				.min = { area.left(), line.bottom() },
				.max = { area.right(), line.top() },
			});
			const bool hovered = rows_area.contains(mouse) && ctx.hovers(full);
			const bool is_selected = selected == r;
			if (hovered) {
				hovered_row = r;
			}
			if (hovered || is_selected || (hovered_column && std::ranges::contains(row.cells, *hovered_column, &lifetime_cell::column))) {
				ctx.queue_sprite({
					.rect = full,
					.color = { 1.f, 1.f, 1.f, is_selected ? 0.10f : 0.05f },
					.texture = ctx.blank_texture,
				});
			}
			if (hovered && ctx.clicked_in_rect(full)) {
				gd.lifetime_selected_row = is_selected ? std::nullopt : std::optional{ r };
			}
			ctx.queue_text({
				.font = ctx.fonts.text,
				.text = row.display,
				.position = { area.left() + pad, line.center().y() + text_view->vertical_center_offset(fs) },
				.scale = fs,
				.color = row.channel ? edge_color(introspection::edge_kind::channel) : ctx.style.color_text,
				.clip_rect = rectf::from_position_size({ area.left(), line.top() }, { label_w - 4.f, row_h }),
			});

			const float bar_h = row_h * 0.3f;
			const float x0 = column_x(*first);
			const float x1 = column_x(*last);
			ctx.queue_sprite({
				.rect = rectf::from_position_size({ x0, line.center().y() + bar_h * 0.5f }, { std::max(1.f, x1 - x0), bar_h }),
				.color = { 0.45f, 0.55f, 0.75f, 0.25f },
				.texture = ctx.blank_texture,
				.corner_radius = bar_h * 0.5f,
			});
			for (const lifetime_cell& cell : row.cells) {
				const std::optional<std::uint32_t> slot = slot_of[cell.column];
				if (!slot) {
					continue;
				}
				const data_access_info info = access_info(cell.access);
				const float radius = std::min(col_w * 0.45f, (info.marked ? 0.32f : 0.2f) * row_h);
				const vec2f center = { column_x(*slot), line.center().y() };
				ctx.queue_sprite({
					.rect = rectf::from_position_size({ center.x() - radius, center.y() + radius }, { radius * 2.f, radius * 2.f }),
					.color = edge_color(info.color_of),
					.texture = ctx.blank_texture,
					.corner_radius = radius,
				});
			}
		}
	}

	if (hovered_column) {
		const std::uint32_t system = gd.lifetime_columns[*hovered_column];
		const introspection::graph_node& node = gd.snapshot.nodes[system];
		const std::array<graph_text_line, 2> lines = { {
			{
				.text = std::string(node_label(node)),
				.color = ctx.style.color_text,
			},
			{
				.text = std::format("{}  -  click to open", node.category),
				.color = ctx.style.color_text_secondary,
			},
		} };
		draw_text_tooltip(ctx, area, lines, mouse);
		return;
	}
	if (!hovered_row) {
		return;
	}
	const lifetime_row& row = gd.lifetime_rows[*hovered_row];
	std::vector<graph_text_line> lines;
	lines.push_back({
		.text = row.display,
		.color = ctx.style.color_text,
	});
	lines.push_back({
		.text = row.qualified,
		.color = ctx.style.color_text_secondary,
	});
	for (const lifetime_cell& cell : row.cells) {
		if (!slot_of[cell.column]) {
			continue;
		}
		lines.push_back({
			.text = std::format("{}  {}", node_label(gd.snapshot.nodes[gd.lifetime_columns[cell.column]]), std::string_view(access_info(cell.access).verb)),
			.color = edge_color(access_info(cell.access).color_of),
			.indent = 10.f,
		});
	}
	draw_text_tooltip(ctx, area, lines, mouse);
}

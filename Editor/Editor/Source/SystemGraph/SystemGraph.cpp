module gse.ide.graph;

import std;

import gse;
import gse.graph;

import gse.ide.search;
import gse.ide.analysis;
import gse.ide.navigation;

namespace gse::ide {
	constexpr gse::vec4f category_palette[] = {
		{ 0.36f, 0.55f, 0.85f, 1.f },
		{ 0.55f, 0.42f, 0.80f, 1.f },
		{ 0.30f, 0.68f, 0.60f, 1.f },
		{ 0.82f, 0.55f, 0.35f, 1.f },
		{ 0.75f, 0.45f, 0.55f, 1.f },
		{ 0.45f, 0.62f, 0.38f, 1.f },
		{ 0.70f, 0.68f, 0.36f, 1.f },
		{ 0.42f, 0.52f, 0.62f, 1.f },
	};

	auto category_color(std::string_view category) -> gse::vec4f;
	auto edge_color(gse::introspection::edge_kind kind) -> gse::vec4f;
	auto short_label(std::string_view name) -> std::string_view;
	auto build_graph_from_snapshot(gse::introspection::system_graph snapshot) -> graph_data;
	auto legend_bounds(const gse::gui::draw_context& ctx, const gse::rectf& area) -> gse::rectf;
	auto draw_legend(const gse::gui::draw_context& ctx, const gse::rectf& area, graph_data& gd) -> void;
	auto draw_node_tooltip(const gse::gui::draw_context& ctx, const gse::rectf& area, const graph_data& gd, std::uint64_t key) -> void;
	auto draw_detail_panel(gse::gui::builder& ui, const gse::rectf& panel, graph_data& gd) -> void;
	auto merge_channels(graph_data& gd, const search::index_state& index) -> void;
}

auto gse::ide::category_color(const std::string_view category) -> gse::vec4f {
	return category_palette[stable_id(category) % std::size(category_palette)];
}

auto gse::ide::edge_color(const gse::introspection::edge_kind kind) -> gse::vec4f {
	switch (kind) {
		case gse::introspection::edge_kind::data_raw: return { 0.85f, 0.55f, 0.28f, 0.85f };
		case gse::introspection::edge_kind::data_waw: return { 0.82f, 0.35f, 0.35f, 0.85f };
		case gse::introspection::edge_kind::data_war: return { 0.80f, 0.72f, 0.35f, 0.85f };
		case gse::introspection::edge_kind::shared_view: return { 0.36f, 0.60f, 0.85f, 0.90f };
		case gse::introspection::edge_kind::lifecycle: return { 0.45f, 0.47f, 0.52f, 0.70f };
		case gse::introspection::edge_kind::channel: return { 0.40f, 0.72f, 0.48f, 0.85f };
	}
	return { 0.5f, 0.5f, 0.5f, 0.8f };
}

auto gse::ide::short_label(const std::string_view name) -> std::string_view {
	std::string_view trimmed = name;
	constexpr std::string_view data_suffix = "::data";
	if (trimmed.ends_with(data_suffix)) {
		trimmed = trimmed.substr(0, trimmed.size() - data_suffix.size());
	}
	const std::size_t pos = trimmed.rfind("::");
	return pos == std::string_view::npos ? trimmed : trimmed.substr(pos + 2);
}

auto gse::ide::build_graph(const gse::scheduler& sched) -> graph_data {
	return build_graph_from_snapshot(sched.snapshot_graph());
}

auto gse::ide::build_graph_from_file(const std::filesystem::path& path) -> graph_data {
	std::ifstream in(path, std::ios::binary);
	if (!in) {
		return graph_data{};
	}
	gse::binary_reader reader(in);
	std::uint32_t magic = 0;
	std::uint32_t version = 0;
	reader & magic & version;
	if (magic != gse::introspection::system_graph_magic || version != gse::introspection::system_graph_version) {
		return graph_data{};
	}
	gse::introspection::system_graph snapshot;
	reader & snapshot.nodes;
	reader & snapshot.edges;
	if (!in) {
		return graph_data{};
	}
	return build_graph_from_snapshot(std::move(snapshot));
}

auto gse::ide::build_graph_from_snapshot(gse::introspection::system_graph snapshot) -> graph_data {
	graph_data gd;
	gd.snapshot = std::move(snapshot);
	const std::size_t n = gd.snapshot.nodes.size();
	std::unordered_map<std::uint64_t, std::uint32_t> index_of;
	index_of.reserve(n);
	for (std::uint32_t i = 0; i < n; ++i) {
		index_of.emplace(gd.snapshot.nodes[i].id, i);
	}
	std::vector<std::pair<std::uint32_t, std::uint32_t>> pairs;
	pairs.reserve(gd.snapshot.edges.size());
	for (const auto& e : gd.snapshot.edges) {
		const auto ai = index_of.find(e.from);
		const auto bi = index_of.find(e.to);
		if (ai == index_of.end() || bi == index_of.end()) {
			continue;
		}
		gd.edges_draw.push_back({
			.a = ai->second,
			.b = bi->second,
			.kind = e.kind,
		});
		pairs.emplace_back(ai->second, bi->second);
	}
	gd.layout = gse::graph::layered_layout(n, pairs);
	gd.built = true;
	return gd;
}

auto gse::ide::merge_channels(graph_data& gd, const search::index_state& index) -> void {
	gd.channel_uses.clear();
	gd.channel_edges.clear();
	gd.channels_merged = true;

	const std::size_t n = gd.snapshot.nodes.size();
	std::unordered_map<std::string, std::uint32_t> system_to_node;
	for (std::uint32_t i = 0; i < n; ++i) {
		const std::string_view name = gd.snapshot.nodes[i].name;
		const std::size_t pos = name.rfind("::");
		system_to_node.emplace(std::string(pos == std::string_view::npos ? name : name.substr(0, pos)), i);
	}

	std::unordered_map<std::string, std::vector<std::uint32_t>> producers;
	std::unordered_map<std::string, std::vector<std::uint32_t>> consumers;
	for (const analysis::channel_use& c : index.channel_links()) {
		const auto it = system_to_node.find(c.system);
		if (it == system_to_node.end()) {
			continue;
		}
		gd.channel_uses.push_back({ .node = it->second, .produce = c.produce, .message = std::string(short_label(c.message)) });
		(c.produce ? producers : consumers)[c.message].push_back(it->second);
	}

	for (const auto& [message, prod] : producers) {
		const auto cit = consumers.find(message);
		if (cit == consumers.end()) {
			continue;
		}
		for (const std::uint32_t p : prod) {
			for (const std::uint32_t consumer : cit->second) {
				if (p != consumer) {
					gd.channel_edges.push_back({ .a = p, .b = consumer });
				}
			}
		}
	}
}

auto gse::ide::draw_graph(gse::gui::builder& ui, const gse::rectf& area, graph_data& gd, const search::index_state* index, gse::channel_writer channels) -> void {
	const gse::gui::draw_context& ctx = ui.ctx;
	const std::size_t n = gd.snapshot.nodes.size();
	if (n == 0) {
		ctx.queue_sprite({
			.rect = area,
			.color = { 0.09f, 0.10f, 0.13f, 1.f },
			.texture = ctx.blank_texture,
		});
		return;
	}

	if (index && !gd.channels_merged && index->symbols_ready.load(std::memory_order_acquire)) {
		merge_channels(gd, *index);
	}

	const gse::vec2f mouse = ctx.input.mouse_position();

	gse::rectf canvas = area;
	std::optional<gse::rectf> panel_area;
	std::optional<gse::rectf> panel_divider;
	if (gd.selected) {
		const float divider_thickness = std::max(6.f, ctx.style.resize_border_thickness) * 2.f;
		const gse::gui::layout::split_result sp = gse::gui::layout::update_split(
			{
				.container = area,
				.axis = gse::gui::layout::split_axis::columns,
				.ratio = std::clamp(1.f - gd.panel_ratio, 0.f, 1.f),
				.min_first = 240.f,
				.min_second = 220.f,
				.divider_thickness = divider_thickness,
			},
			{
				.mouse = mouse,
				.pressed = ctx.input.mouse_button_pressed(gse::mouse_button::button_1) && ctx.input_available(),
				.held = ctx.input.mouse_button_held(gse::mouse_button::button_1),
			},
			gd.resizing_panel
		);
		canvas = sp.first;
		panel_area = sp.second;
		panel_divider = sp.divider;
		gd.panel_ratio = 1.f - sp.ratio;
	}

	const bool over_panel = panel_area && panel_area->contains(mouse) && ctx.input_available();
	const bool over_divider = panel_divider && panel_divider->contains(mouse) && ctx.input_available();
	const bool over_legend = legend_bounds(ctx, canvas).contains(mouse) && ctx.input_available();
	const float reset_h = ctx.fonts.text->line_height(ctx.style.font_size) + 8.f;
	const float reset_w = ctx.fonts.text->width(std::string_view("reset view"), ctx.style.font_size) + 24.f;
	const gse::rectf reset_rect = gse::rectf::from_position_size({ canvas.left() + 10.f, canvas.bottom() + 10.f + reset_h }, { reset_w, reset_h });
	const bool over_reset = reset_rect.contains(mouse) && ctx.input_available();
	const bool over_area = canvas.contains(mouse) && ctx.input_available() && !over_panel && !over_divider && !over_legend && !over_reset;

	if (over_area && ctx.input.mouse_button_pressed(gse::mouse_button::button_1)) {
		gd.panning = true;
		gd.dragged = false;
		gd.pan_press = mouse;
		gd.pan_last = mouse;
	}
	if (gd.panning && ctx.input.mouse_button_held(gse::mouse_button::button_1)) {
		gd.pan += mouse - gd.pan_last;
		gd.pan_last = mouse;
		const gse::vec2f moved = mouse - gd.pan_press;
		const float drag_slop = 4.f;
		if (moved.x() * moved.x() + moved.y() * moved.y() > drag_slop * drag_slop) {
			gd.dragged = true;
		}
	}
	else {
		gd.panning = false;
	}

	const std::optional<std::uint64_t> focus = gd.selected ? gd.selected : gd.hovered;
	std::unordered_set<std::uint64_t> highlighted;
	std::vector<bool> edge_lit(gd.edges_draw.size(), false);
	if (focus) {
		highlighted.insert(*focus);
		for (std::size_t ei = 0; ei < gd.edges_draw.size(); ++ei) {
			const std::uint64_t a_id = gd.snapshot.nodes[gd.edges_draw[ei].a].id;
			const std::uint64_t b_id = gd.snapshot.nodes[gd.edges_draw[ei].b].id;
			if (a_id == *focus || b_id == *focus) {
				edge_lit[ei] = true;
				highlighted.insert(a_id);
				highlighted.insert(b_id);
			}
		}
	}

	std::unordered_set<std::uint64_t> isolate_set;
	if (gd.isolated) {
		isolate_set.insert(*gd.isolated);
		const auto add_neighbors = [&](const std::uint32_t ai, const std::uint32_t bi) {
			const std::uint64_t a_id = gd.snapshot.nodes[ai].id;
			const std::uint64_t b_id = gd.snapshot.nodes[bi].id;
			if (a_id == *gd.isolated) { isolate_set.insert(b_id); }
			if (b_id == *gd.isolated) { isolate_set.insert(a_id); }
		};
		for (const edge_draw& e : gd.edges_draw) { add_neighbors(e.a, e.b); }
		for (const channel_link_draw& ce : gd.channel_edges) { add_neighbors(ce.a, ce.b); }
	}

	const float base_font = ctx.style.font_size;
	const float pill_pad_x = 14.f;
	const float pill_h = base_font + 14.f;
	const float gap_x = 24.f;
	const float step_y = pill_h + 56.f;

	std::vector<std::string_view> labels(n);
	std::vector<float> widths(n);
	for (std::uint32_t i = 0; i < n; ++i) {
		const gse::introspection::graph_node& gn = gd.snapshot.nodes[i];
		labels[i] = gn.display.empty() ? short_label(gn.name) : std::string_view(gn.display);
		widths[i] = std::max(72.f, ctx.fonts.text->width(labels[i], base_font) + pill_pad_x * 2.f);
	}

	std::vector<std::vector<std::uint32_t>> by_layer(gd.layout.layer_count);
	for (std::uint32_t i = 0; i < n; ++i) {
		const gse::graph::placement pl = gd.layout.placements[i];
		if (pl.layer < gd.layout.layer_count) {
			by_layer[pl.layer].push_back(i);
		}
	}
	for (std::vector<std::uint32_t>& row : by_layer) {
		std::ranges::sort(row, [&](const std::uint32_t a, const std::uint32_t b) {
			return gd.layout.placements[a].order < gd.layout.placements[b].order;
		});
	}

	std::vector<gse::vec2f> world(n);
	gse::vec2f world_min = { std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
	gse::vec2f world_max = { std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest() };
	for (std::uint32_t l = 0; l < gd.layout.layer_count; ++l) {
		float total = 0.f;
		for (const std::uint32_t idx : by_layer[l]) {
			total += widths[idx] + gap_x;
		}
		total -= by_layer[l].empty() ? 0.f : gap_x;
		float x = -total * 0.5f;
		const float y = static_cast<float>(l) * step_y;
		for (const std::uint32_t idx : by_layer[l]) {
			const float cx = x + widths[idx] * 0.5f;
			world[idx] = { cx, y };
			world_min = { std::min(world_min.x(), cx - widths[idx] * 0.5f), std::min(world_min.y(), y - pill_h * 0.5f) };
			world_max = { std::max(world_max.x(), cx + widths[idx] * 0.5f), std::max(world_max.y(), y + pill_h * 0.5f) };
			x += widths[idx] + gap_x;
		}
	}

	const gse::vec2f world_center = (world_min + world_max) * 0.5f;
	const float world_w = std::max(1.f, world_max.x() - world_min.x());
	const float world_h = std::max(1.f, world_max.y() - world_min.y());
	const float fit = std::max(1e-4f, std::min((canvas.width() - 60.f) / world_w, (canvas.height() - 60.f) / world_h));
	const float min_scale = 0.15f;
	const float max_scale = 3.f;
	if (over_area) {
		const gse::vec2f scroll = ctx.scroll_delta_for(canvas);
		if (scroll.y() != 0.f) {
			const float old_scale = std::clamp(fit * gd.zoom, min_scale, max_scale);
			const float new_scale = std::clamp(old_scale * (scroll.y() > 0.f ? 1.12f : 1.f / 1.12f), min_scale, max_scale);
			const float ratio = new_scale / old_scale;
			gd.zoom = new_scale / fit;
			gd.pan = gd.pan * ratio + (mouse - canvas.center()) * (1.f - ratio);
		}
	}
	const float scale = std::clamp(fit * gd.zoom, min_scale, max_scale);

	std::vector<gse::vec2f> centers(n);
	std::vector<gse::gui::graph_canvas::node> nodes;
	nodes.reserve(n);
	for (std::uint32_t i = 0; i < n; ++i) {
		const float sx = canvas.center().x() + gd.pan.x() + (world[i].x() - world_center.x()) * scale;
		const float sy = canvas.center().y() + gd.pan.y() - (world[i].y() - world_center.y()) * scale;
		const float nw = widths[i] * scale;
		const float nh = pill_h * scale;
		centers[i] = { sx, sy };
		gse::vec4f color = category_color(gd.snapshot.nodes[i].category);
		const bool ghosted = gd.isolated && !isolate_set.contains(gd.snapshot.nodes[i].id);
		if (ghosted) {
			color = { color.x() * 0.16f, color.y() * 0.16f, color.z() * 0.18f, 0.07f };
		}
		else if (focus) {
			if (gd.snapshot.nodes[i].id == *focus) {
				color = { std::min(1.f, color.x() + 0.22f), std::min(1.f, color.y() + 0.22f), std::min(1.f, color.z() + 0.22f), 1.f };
			}
			else if (!highlighted.contains(gd.snapshot.nodes[i].id)) {
				color = { color.x() * 0.32f, color.y() * 0.32f, color.z() * 0.34f, 0.5f };
			}
		}
		nodes.push_back({
			.key = gd.snapshot.nodes[i].id,
			.label = ghosted ? std::string_view{} : labels[i],
			.rect = gse::rectf::from_position_size({ sx - nw * 0.5f, sy + nh * 0.5f }, { nw, nh }),
			.color = color,
		});
	}

	const auto border_point = [](const gse::vec2f center, const gse::vec2f dir, const gse::rectf& r) -> gse::vec2f {
		const float hw = r.width() * 0.5f;
		const float hh = r.height() * 0.5f;
		float t = std::numeric_limits<float>::max();
		if (std::abs(dir.x()) > 1e-4f) {
			t = std::min(t, hw / std::abs(dir.x()));
		}
		if (std::abs(dir.y()) > 1e-4f) {
			t = std::min(t, hh / std::abs(dir.y()));
		}
		return center + dir * t;
	};

	std::vector<gse::gui::graph_canvas::edge> edges;
	edges.reserve(gd.edges_draw.size());
	for (std::size_t ei = 0; ei < gd.edges_draw.size(); ++ei) {
		const edge_draw& e = gd.edges_draw[ei];
		if (!gd.kind_visible[static_cast<std::size_t>(e.kind)]) {
			continue;
		}
		if (gd.isolated && (!isolate_set.contains(gd.snapshot.nodes[e.a].id) || !isolate_set.contains(gd.snapshot.nodes[e.b].id))) {
			continue;
		}
		gse::vec4f color = edge_color(e.kind);
		if (focus && !edge_lit[ei]) {
			color = { color.x() * 0.4f, color.y() * 0.4f, color.z() * 0.4f, 0.22f };
		}
		const gse::vec2f delta = centers[e.b] - centers[e.a];
		const float dist = std::sqrt(delta.x() * delta.x() + delta.y() * delta.y());
		if (dist < 1.f) {
			continue;
		}
		const gse::vec2f dir = delta * (1.f / dist);
		edges.push_back({
			.from = border_point(centers[e.a], dir, nodes[e.a].rect),
			.to = border_point(centers[e.b], dir * -1.f, nodes[e.b].rect),
			.color = color,
		});
	}

	if (focus && gd.kind_visible[static_cast<std::size_t>(gse::introspection::edge_kind::channel)]) {
		for (const channel_link_draw& ce : gd.channel_edges) {
			if (gd.snapshot.nodes[ce.a].id != *focus && gd.snapshot.nodes[ce.b].id != *focus) {
				continue;
			}
			const gse::vec2f delta = centers[ce.b] - centers[ce.a];
			const float dist = std::sqrt(delta.x() * delta.x() + delta.y() * delta.y());
			if (dist < 1.f) {
				continue;
			}
			const gse::vec2f dir = delta * (1.f / dist);
			edges.push_back({
				.from = border_point(centers[ce.a], dir, nodes[ce.a].rect),
				.to = border_point(centers[ce.b], dir * -1.f, nodes[ce.b].rect),
				.color = edge_color(gse::introspection::edge_kind::channel),
			});
		}
	}

	const gse::gui::graph_canvas::result picked = ui.draw<gse::gui::graph_canvas>({
		.area = canvas,
		.nodes = nodes,
		.edges = edges,
		.label_scale = scale,
	});

	draw_legend(ctx, canvas, gd);

	gse::gui::draw::panel_backdrop(ctx, {
		.rect = reset_rect,
		.background = over_reset ? gse::vec4f{ 0.18f, 0.20f, 0.26f, 0.95f } : gse::vec4f{ 0.09f, 0.10f, 0.13f, 0.9f },
		.clip = reset_rect,
		.layer = gse::render_layer::overlay,
	});
	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = "reset view",
		.position = { reset_rect.left() + 12.f, reset_rect.center().y() + ctx.fonts.text->vertical_center_offset(ctx.style.font_size) },
		.scale = ctx.style.font_size,
		.color = ctx.style.color_text,
		.layer = gse::render_layer::overlay,
	});
	if (over_reset && ctx.mouse_pressed_for(reset_rect)) {
		gd.pan = { 0.f, 0.f };
		gd.zoom = 1.f;
		gd.isolated = std::nullopt;
		gd.selected = std::nullopt;
	}

	gd.hovered = picked.hovered;
	if (gd.selected && panel_area) {
		draw_detail_panel(ui, *panel_area, gd);
	}
	else if (picked.hovered) {
		draw_node_tooltip(ctx, canvas, gd, *picked.hovered);
	}

	if (panel_divider && (over_divider || gd.resizing_panel)) {
		channels.push<gse::set_cursor_shape_request>({ .shape = gse::cursor_shape::resize_ew });
	}

	if (over_legend || over_reset || gd.resizing_panel || gd.dragged) {
		return;
	}
	if (picked.background_clicked) {
		if (gd.isolated) {
			gd.isolated = std::nullopt;
		}
		else {
			gd.selected = std::nullopt;
		}
	}
	if (!picked.clicked) {
		return;
	}
	if (gse::gui::interaction::register_click(gd.click, mouse) >= 2) {
		gd.isolated = *picked.clicked;
		gd.selected = *picked.clicked;
		return;
	}
	if (gd.selected != picked.clicked) {
		gd.selected = picked.clicked;
		return;
	}
	const gse::introspection::graph_node* clicked_node = nullptr;
	for (const auto& nd : gd.snapshot.nodes) {
		if (nd.id == *picked.clicked) {
			clicked_node = &nd;
			break;
		}
	}
	if (!clicked_node || clicked_node->file.empty()) {
		return;
	}
	channels.push<gse::ide::jump_to_request>({
		.path = clicked_node->file,
		.line = clicked_node->line,
		.column = clicked_node->column,
	});
}

constexpr std::pair<gse::introspection::edge_kind, std::string_view> legend_rows[] = {
	{ gse::introspection::edge_kind::data_raw, "read after write" },
	{ gse::introspection::edge_kind::data_waw, "write after write" },
	{ gse::introspection::edge_kind::data_war, "write after read" },
	{ gse::introspection::edge_kind::shared_view, "shared view" },
	{ gse::introspection::edge_kind::channel, "channel (on select)" },
	{ gse::introspection::edge_kind::lifecycle, "init / frame order" },
};

auto gse::ide::legend_bounds(const gse::gui::draw_context& ctx, const gse::rectf& area) -> gse::rectf {
	const float pad = 10.f;
	const float row_h = ctx.style.font_size + 8.f;
	const float sw = 12.f;
	float max_text = 0.f;
	for (const auto& [kind, label] : legend_rows) {
		max_text = std::max(max_text, ctx.fonts.text->width(label, ctx.style.font_size));
	}
	const float w = pad + sw + 8.f + max_text + pad + 4.f;
	const float h = row_h * static_cast<float>(std::size(legend_rows)) + pad * 2.f;
	return gse::rectf::from_position_size({ area.left() + pad, area.top() - pad }, { w, h });
}

auto gse::ide::draw_legend(const gse::gui::draw_context& ctx, const gse::rectf& area, graph_data& gd) -> void {
	const float pad = 10.f;
	const float row_h = ctx.style.font_size + 8.f;
	const float sw = 12.f;
	const gse::rectf panel = legend_bounds(ctx, area);
	gse::gui::draw::panel_backdrop(ctx, {
		.rect = panel,
		.background = { 0.05f, 0.06f, 0.08f, 0.9f },
		.clip = panel,
		.layer = gse::render_layer::overlay,
	});
	const gse::vec2f mouse = ctx.input.mouse_position();
	for (std::size_t i = 0; i < std::size(legend_rows); ++i) {
		const float ry = panel.top() - pad - static_cast<float>(i) * row_h;
		const gse::rectf row_rect = gse::rectf::from_position_size({ panel.left() + pad * 0.5f, ry }, { panel.width() - pad, row_h });
		const std::size_t ki = static_cast<std::size_t>(legend_rows[i].first);
		const bool hovered = row_rect.contains(mouse) && ctx.input_available();
		if (hovered && ctx.mouse_pressed_for(row_rect)) {
			gd.kind_visible[ki] = !gd.kind_visible[ki];
		}
		const bool on = gd.kind_visible[ki];
		if (hovered) {
			ctx.queue_sprite({
				.rect = row_rect,
				.color = { 1.f, 1.f, 1.f, 0.07f },
				.texture = ctx.blank_texture,
				.layer = gse::render_layer::overlay,
				.corner_radius = 3.f,
			});
		}
		gse::vec4f swatch = edge_color(legend_rows[i].first);
		if (!on) {
			swatch = { swatch.x() * 0.28f, swatch.y() * 0.28f, swatch.z() * 0.30f, 0.5f };
		}
		ctx.queue_sprite({
			.rect = gse::rectf::from_position_size({ panel.left() + pad, ry - (row_h - sw) * 0.5f }, { sw, sw }),
			.color = swatch,
			.texture = ctx.blank_texture,
			.layer = gse::render_layer::overlay,
			.corner_radius = 2.f,
		});
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = std::string(legend_rows[i].second),
			.position = { panel.left() + pad + sw + 8.f, ry - row_h * 0.5f + ctx.fonts.text->vertical_center_offset(ctx.style.font_size) },
			.scale = ctx.style.font_size,
			.color = on ? ctx.style.color_text : ctx.style.color_text_secondary,
			.layer = gse::render_layer::overlay,
		});
	}
}

auto gse::ide::draw_detail_panel(gse::gui::builder& ui, const gse::rectf& panel, graph_data& gd) -> void {
	gse::gui::draw_context& ctx = ui.ctx;
	if (!gd.selected) {
		return;
	}
	const gse::introspection::graph_node* node = nullptr;
	for (const auto& candidate : gd.snapshot.nodes) {
		if (candidate.id == *gd.selected) {
			node = &candidate;
			break;
		}
	}
	if (!node) {
		return;
	}
	const std::uint32_t sel_idx = static_cast<std::uint32_t>(node - gd.snapshot.nodes.data());

	const float fs = ctx.style.font_size;
	const float pad = 12.f;
	const float line_h = fs + 6.f;

	gse::gui::draw::panel_backdrop(ctx, {
		.rect = panel,
		.background = { 0.06f, 0.07f, 0.10f, 0.97f },
		.accent = gse::gui::panel_accent{
			.edge = gse::gui::panel_edge::left,
			.width = 3.f,
			.color = category_color(node->category),
		},
		.clip = panel,
		.layer = gse::render_layer::overlay,
	});

	const gse::gui::layer_scope detail_layer = ctx.scoped_layer(gse::render_layer::overlay);
	const gse::gui::layout::within_scope detail_area = gse::gui::layout::within(ctx, panel);
	const std::string scroll_id = std::format("graph_detail::{}", *gd.selected);
	const gse::gui::scroll_handle detail_view = gse::gui::scroll_region(ctx, { .id = scroll_id });
	gse::gui::layout::skip(ctx, pad);

	const float x = panel.left() + pad + 4.f;
	const auto text_line = [&](const std::string& text, const gse::vec4f color, const float indent) {
		const gse::rectf row = gse::gui::layout::reserve_row(ctx, line_h);
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = text,
			.position = { x + indent, row.center().y() + ctx.fonts.text->vertical_center_offset(fs) },
			.scale = fs,
			.color = color,
		});
	};

	const std::string_view title = node->display.empty() ? short_label(node->name) : std::string_view(node->display);
	text_line(std::string(title), ctx.style.color_text, 0.f);
	text_line(node->category, ctx.style.color_text_secondary, 0.f);
	gse::gui::layout::skip(ctx, line_h * 0.4f);

	const std::pair<bool, std::string_view> phase_chips[] = {
		{ node->has_init, "init" },
		{ node->has_run, "run" },
		{ node->has_frame, "frame" },
		{ node->deferred, "deferred" },
	};
	const gse::rectf chip_row = gse::gui::layout::reserve_row(ctx, line_h);
	float cx = x;
	for (const auto& [on, label] : phase_chips) {
		if (!on) {
			continue;
		}
		const float cw = ctx.fonts.text->width(label, fs) + 12.f;
		const gse::rectf chip = gse::rectf::from_position_size({ cx, chip_row.top() }, { cw, line_h });
		ctx.queue_sprite({
			.rect = chip,
			.color = { 0.17f, 0.21f, 0.28f, 1.f },
			.texture = ctx.blank_texture,
			.corner_radius = line_h * 0.3f,
		});
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = std::string(label),
			.position = { cx + 6.f, chip.center().y() + ctx.fonts.text->vertical_center_offset(fs) },
			.scale = fs,
			.color = ctx.style.color_text,
		});
		cx += cw + 6.f;
	}
	gse::gui::layout::skip(ctx, line_h * 0.6f);

	const auto section = [&](const std::string& header, std::span<const std::string> items) {
		if (items.empty()) {
			return;
		}
		text_line(header, ctx.style.color_accent, 0.f);
		for (const std::string& item : items) {
			text_line(std::string(short_label(item)), ctx.style.color_text, 12.f);
		}
		gse::gui::layout::skip(ctx, line_h * 0.5f);
	};

	section(std::format("Reads ({})", node->reads.size()), node->reads);
	section(std::format("Writes ({})", node->writes.size()), node->writes);

	std::vector<std::string> deps;
	std::vector<std::string> feeds;
	for (const edge_draw& e : gd.edges_draw) {
		if (gd.snapshot.nodes[e.b].id == *gd.selected) {
			const auto& src = gd.snapshot.nodes[e.a];
			deps.push_back(std::string(src.display.empty() ? short_label(src.name) : std::string_view(src.display)));
		}
		if (gd.snapshot.nodes[e.a].id == *gd.selected) {
			const auto& dst = gd.snapshot.nodes[e.b];
			feeds.push_back(std::string(dst.display.empty() ? short_label(dst.name) : std::string_view(dst.display)));
		}
	}
	std::ranges::sort(deps);
	const auto dup_deps = std::ranges::unique(deps);
	deps.erase(dup_deps.begin(), dup_deps.end());
	std::ranges::sort(feeds);
	const auto dup_feeds = std::ranges::unique(feeds);
	feeds.erase(dup_feeds.begin(), dup_feeds.end());
	section(std::format("Depends on ({})", deps.size()), deps);
	section(std::format("Feeds ({})", feeds.size()), feeds);

	std::vector<std::string> publishes;
	std::vector<std::string> consumes;
	for (const channel_use_draw& cu : gd.channel_uses) {
		if (cu.node != sel_idx) {
			continue;
		}
		(cu.produce ? publishes : consumes).push_back(cu.message);
	}
	std::ranges::sort(publishes);
	publishes.erase(std::ranges::unique(publishes).begin(), publishes.end());
	std::ranges::sort(consumes);
	consumes.erase(std::ranges::unique(consumes).begin(), consumes.end());
	section(std::format("Publishes ({})", publishes.size()), publishes);
	section(std::format("Consumes ({})", consumes.size()), consumes);
}

auto gse::ide::draw_node_tooltip(const gse::gui::draw_context& ctx, const gse::rectf& area, const graph_data& gd, const std::uint64_t key) -> void {
	const gse::introspection::graph_node* node = nullptr;
	for (const auto& candidate : gd.snapshot.nodes) {
		if (candidate.id == key) {
			node = &candidate;
			break;
		}
	}
	if (!node) {
		return;
	}

	std::vector<std::string> lines;
	lines.push_back(node->display.empty() ? std::string(short_label(node->name)) : node->display);

	std::string phases;
	if (node->has_init) {
		phases += "init ";
	}
	if (node->has_run) {
		phases += "run ";
	}
	if (node->has_frame) {
		phases += "frame ";
	}
	if (node->deferred) {
		phases += "deferred ";
	}
	if (!phases.empty()) {
		lines.push_back(phases);
	}

	if (!node->reads.empty()) {
		std::string line = "reads: ";
		for (const auto& r : node->reads) {
			line += short_label(r);
			line += " ";
		}
		lines.push_back(line);
	}
	if (!node->writes.empty()) {
		std::string line = "writes: ";
		for (const auto& w : node->writes) {
			line += short_label(w);
			line += " ";
		}
		lines.push_back(line);
	}

	const float pad = 8.f;
	const float line_h = ctx.style.font_size + 4.f;
	float max_w = 0.f;
	for (const auto& line : lines) {
		max_w = std::max(max_w, ctx.fonts.text->width(line, ctx.style.font_size));
	}

	const gse::vec2f mouse = ctx.input.mouse_position();
	const float pw = max_w + pad * 2.f;
	const float ph = line_h * static_cast<float>(lines.size()) + pad * 2.f;
	float px = mouse.x() + 16.f;
	float py = mouse.y() - 16.f;
	if (px + pw > area.right()) {
		px = area.right() - pw;
	}
	if (px < area.left()) {
		px = area.left();
	}
	if (py > area.top()) {
		py = area.top();
	}
	if (py - ph < area.bottom()) {
		py = area.bottom() + ph;
	}
	const gse::rectf panel = gse::rectf::from_position_size({ px, py }, { pw, ph });
	ctx.queue_sprite({
		.rect = panel.inset({ -1.f, -1.f }),
		.color = { 0.32f, 0.36f, 0.44f, 0.9f },
		.texture = ctx.blank_texture,
		.layer = gse::render_layer::popup,
		.corner_radius = 5.f,
	});
	ctx.queue_sprite({
		.rect = panel,
		.color = { 0.06f, 0.07f, 0.10f, 0.97f },
		.texture = ctx.blank_texture,
		.layer = gse::render_layer::popup,
		.corner_radius = 4.f,
	});
	for (std::size_t i = 0; i < lines.size(); ++i) {
		const float ly = panel.top() - pad - static_cast<float>(i) * line_h - line_h * 0.5f;
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = lines[i],
			.position = { panel.left() + pad, ly + ctx.fonts.text->vertical_center_offset(ctx.style.font_size) },
			.scale = ctx.style.font_size,
			.color = i == 0 ? ctx.style.color_text : ctx.style.color_text_secondary,
			.layer = gse::render_layer::popup,
		});
	}
}
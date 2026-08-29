module gse.ide.graph;

import std;

import gse;
import gse.graph;

import gse.ide.search;
import gse.ide.analysis;
import gse.ide.navigation;

namespace gse::ide {
	constexpr vec4f category_palette[] = {
		{ 0.36f, 0.55f, 0.85f, 1.f },
		{ 0.55f, 0.42f, 0.80f, 1.f },
		{ 0.30f, 0.68f, 0.60f, 1.f },
		{ 0.82f, 0.55f, 0.35f, 1.f },
		{ 0.75f, 0.45f, 0.55f, 1.f },
		{ 0.45f, 0.62f, 0.38f, 1.f },
		{ 0.70f, 0.68f, 0.36f, 1.f },
		{ 0.42f, 0.52f, 0.62f, 1.f },
	};

	constexpr auto edge_kinds = [] consteval {
		std::array<introspection::edge_kind, introspection::edge_kind_count> kinds;
		std::size_t index = 0;
		template for (constexpr auto enumerator : std::define_static_array(std::meta::enumerators_of(^^introspection::edge_kind))) {
			kinds[index++] = [:enumerator:];
		}
		return kinds;
	}();

	auto category_color(
		std::string_view category
	) -> vec4f;

	auto edge_color(
		introspection::edge_kind kind
	) -> vec4f;

	auto short_label(
		std::string_view name
	) -> std::string_view;

	auto pretty_name(
		std::string_view name
	) -> std::string;

	auto node_label(
		const introspection::graph_node& node
	) -> std::string_view;

	auto find_node(
		const graph_data& gd,
		std::uint64_t id
	) -> const introspection::graph_node*;

	auto sort_unique(
		std::vector<list_item>& items
	) -> void;

	auto collect_relations(
		const graph_data& gd,
		std::uint64_t id
	) -> node_relations;

	auto resolve_targets(
		std::vector<list_item>& items,
		const search::index_state& index,
		const std::filesystem::path& from
	) -> void;

	auto ensure_relations(
		graph_data& gd,
		std::uint64_t id,
		const search::index_state* index
	) -> node_relations&;

	auto wrap_joined(
		const gui::draw_context& ctx,
		std::span<const std::string> items,
		float max_width,
		float font_size
	) -> std::vector<std::string>;

	auto wrap_displays(
		const gui::draw_context& ctx,
		std::span<const list_item> items,
		float max_width,
		float font_size
	) -> std::vector<std::string>;

	auto build_graph_from_snapshot(
		introspection::system_graph snapshot
	) -> graph_data;

	auto prepare_presentation(
		graph_data& gd,
		const gui::draw_context& ctx
	) -> void;

	auto legend_bounds(
		const gui::draw_context& ctx,
		const rectf& area
	) -> rectf;

	auto draw_legend(
		const gui::draw_context& ctx,
		const rectf& area,
		graph_data& gd
	) -> void;

	auto draw_node_tooltip(
		const gui::draw_context& ctx,
		const rectf& area,
		graph_data& gd,
		const search::index_state* index,
		std::uint64_t key,
		vec2f anchor
	) -> void;

	auto draw_detail_panel(
		gui::builder& ui,
		const rectf& panel,
		graph_data& gd,
		const search::index_state* index,
		channel_write<jump_to_request, set_cursor_shape_request> channels
	) -> void;

	auto merge_channels(
		graph_data& gd
	) -> void;
}

auto gse::ide::category_color(const std::string_view category) -> vec4f {
	return category_palette[stable_id(category) % std::size(category_palette)];
}

auto gse::ide::edge_color(const introspection::edge_kind kind) -> vec4f {
	const introspection::edge_kind_info info = introspection::edge_info(kind);
	constexpr float byte_scale = 1.f / 255.f;
	return {
		static_cast<float>(info.color >> 16 & 0xff) * byte_scale,
		static_cast<float>(info.color >> 8 & 0xff) * byte_scale,
		static_cast<float>(info.color & 0xff) * byte_scale,
		info.alpha,
	};
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

auto gse::ide::pretty_name(const std::string_view name) -> std::string {
	std::string_view trimmed = short_label(name);
	constexpr std::string_view component_suffix = "_component";
	if (trimmed.size() > component_suffix.size() && trimmed.ends_with(component_suffix)) {
		trimmed = trimmed.substr(0, trimmed.size() - component_suffix.size());
	}
	std::string out;
	out.reserve(trimmed.size());
	bool word_start = true;
	for (const char c : trimmed) {
		if (c == '_') {
			if (!out.empty() && out.back() != ' ') {
				out.push_back(' ');
			}
			word_start = true;
			continue;
		}
		out.push_back(word_start && c >= 'a' && c <= 'z' ? static_cast<char>(c - ('a' - 'A')) : c);
		word_start = false;
	}
	return out;
}

auto gse::ide::node_label(const introspection::graph_node& node) -> std::string_view {
	return node.display.empty() ? short_label(node.name) : std::string_view(node.display);
}

auto gse::ide::find_node(const graph_data& gd, const std::uint64_t id) -> const introspection::graph_node* {
	const auto found = gd.index_of.find(id);
	if (found == gd.index_of.end()) {
		return nullptr;
	}
	return &gd.snapshot.nodes[found->second];
}

auto gse::ide::sort_unique(std::vector<list_item>& items) -> void {
	std::ranges::sort(items, [](const list_item& a, const list_item& b) {
		return std::tie(a.node_id, a.qualified) < std::tie(b.node_id, b.qualified);
	});
	const auto duplicates = std::ranges::unique(items, [](const list_item& a, const list_item& b) {
		return a.node_id == b.node_id && a.qualified == b.qualified;
	});
	items.erase(duplicates.begin(), duplicates.end());
	std::ranges::sort(items, [](const list_item& a, const list_item& b) {
		return std::tie(a.display, a.node_id, a.qualified) < std::tie(b.display, b.node_id, b.qualified);
	});
}

auto gse::ide::collect_relations(const graph_data& gd, const std::uint64_t id) -> node_relations {
	node_relations out;
	const introspection::graph_node* node = find_node(gd, id);
	if (!node) {
		return out;
	}
	const std::uint32_t node_index = gd.index_of.at(id);

	const auto to_items = [](const std::span<const std::string> raw) {
		std::vector<list_item> items;
		items.reserve(raw.size());
		for (const std::string& value : raw) {
			items.push_back({
				.display = pretty_name(value),
				.qualified = value,
			});
		}
		sort_unique(items);
		return items;
	};
	out.reads = to_items(node->reads);
	out.writes = to_items(node->writes);

	const auto add_peer = [&](std::vector<list_item>& into, const std::uint64_t peer_id, const std::span<const std::string> via) {
		const introspection::graph_node* peer = find_node(gd, peer_id);
		if (!peer) {
			return;
		}
		const std::string_view label = node_label(*peer);
		auto existing = std::ranges::find_if(into, [&](const list_item& item) {
			return item.node_id == peer_id;
		});
		if (existing == into.end()) {
			const bool has_file = !peer->file.empty();
			into.push_back({
				.node_id = peer_id,
				.display = std::string(label),
				.qualified = peer->name,
				.target = has_file
					? std::optional{ search::location{
						.path = peer->file,
						.line = peer->line,
						.column = peer->column,
					} }
					: std::nullopt,
				.linkable = has_file,
			});
			existing = into.end() - 1;
		}
		for (const std::string& component : via) {
			if (std::ranges::find(existing->via, component) == existing->via.end()) {
				existing->via.push_back(component);
			}
		}
	};

	for (const introspection::graph_edge& e : gd.snapshot.edges) {
		if (e.to == id) {
			add_peer(out.depends, e.from, e.via);
		}
		if (e.from == id) {
			add_peer(out.feeds, e.to, e.via);
		}
	}
	for (list_item& item : out.depends) {
		std::ranges::sort(item.via);
	}
	for (list_item& item : out.feeds) {
		std::ranges::sort(item.via);
	}
	sort_unique(out.depends);
	sort_unique(out.feeds);

	for (const channel_use_draw& cu : gd.channel_uses) {
		if (cu.node != node_index) {
			continue;
		}
		(cu.produce ? out.publishes : out.consumes).push_back({
			.display = pretty_name(cu.qualified),
			.qualified = cu.qualified,
		});
	}
	sort_unique(out.publishes);
	sort_unique(out.consumes);
	out.reads_header = std::format("Reads  {}", out.reads.size());
	out.writes_header = std::format("Writes  {}", out.writes.size());
	out.depends_header = std::format("Depends on  {}", out.depends.size());
	out.feeds_header = std::format("Feeds  {}", out.feeds.size());
	out.publishes_header = std::format("Publishes  {}", out.publishes.size());
	out.consumes_header = std::format("Consumes  {}", out.consumes.size());

	return out;
}

auto gse::ide::resolve_targets(std::vector<list_item>& items, const search::index_state& index, const std::filesystem::path& from) -> void {
	for (list_item& item : items) {
		if (item.linkable || item.qualified.empty()) {
			continue;
		}
		const std::string_view qualified = item.qualified;
		const std::size_t pos = qualified.rfind("::");
		const std::string_view name = pos == std::string_view::npos ? qualified : qualified.substr(pos + 2);
		const std::string_view qualifier = pos == std::string_view::npos ? std::string_view{} : qualified.substr(0, pos + 2);
		if (const auto found = index.symbol_definition(name, qualifier, from)) {
			item.target = *found;
			item.linkable = true;
		}
	}
}

auto gse::ide::ensure_relations(graph_data& gd, const std::uint64_t id, const search::index_state* index) -> node_relations& {
	if (gd.detail_for != id) {
		gd.detail = collect_relations(gd, id);
		gd.detail_for = id;
		gd.detail_generation = std::nullopt;
		gd.detail_wrap_width = 0.f;
	}
	if (index && index->symbols_ready.load(std::memory_order_acquire)) {
		const std::uint64_t generation = index->symbol_generation.load(std::memory_order_acquire);
		if (gd.detail_generation == generation) {
			return gd.detail;
		}
		gd.detail = collect_relations(gd, id);
		gd.detail_wrap_width = 0.f;
		const introspection::graph_node* node = find_node(gd, id);
		const std::filesystem::path from = node ? std::filesystem::path(node->file) : std::filesystem::path{};
		resolve_targets(gd.detail.reads, *index, from);
		resolve_targets(gd.detail.writes, *index, from);
		resolve_targets(gd.detail.publishes, *index, from);
		resolve_targets(gd.detail.consumes, *index, from);
		gd.detail_generation = generation;
	}
	return gd.detail;
}

auto gse::ide::wrap_joined(const gui::draw_context& ctx, const std::span<const std::string> items, const float max_width, const float font_size) -> std::vector<std::string> {
	const auto text_view = ctx.fonts.text.resolve();
	std::vector<std::string> lines;
	std::string current;
	for (std::size_t i = 0; i < items.size(); ++i) {
		const std::string display = pretty_name(items[i]);
		const std::string_view separator = i + 1 < items.size() ? ", " : std::string_view{};
		const float candidate_width = text_view->width(current, font_size) + text_view->width(display, font_size) + text_view->width(separator, font_size);
		if (!current.empty() && candidate_width > max_width) {
			lines.push_back(std::move(current));
			current.clear();
		}
		current += display;
		current += separator;
	}
	if (!current.empty()) {
		lines.push_back(std::move(current));
	}
	return lines;
}

auto gse::ide::wrap_displays(const gui::draw_context& ctx, const std::span<const list_item> items, const float max_width, const float font_size) -> std::vector<std::string> {
	const auto text_view = ctx.fonts.text.resolve();
	std::vector<std::string> lines;
	std::string current;
	for (std::size_t i = 0; i < items.size(); ++i) {
		const std::string_view separator = i + 1 < items.size() ? ", " : std::string_view{};
		const float candidate_width = text_view->width(current, font_size) + text_view->width(items[i].display, font_size) + text_view->width(separator, font_size);
		if (!current.empty() && candidate_width > max_width) {
			lines.push_back(std::move(current));
			current.clear();
		}
		current += items[i].display;
		current += separator;
	}
	if (!current.empty()) {
		lines.push_back(std::move(current));
	}
	return lines;
}

auto gse::ide::build_graph(const scheduler& sched) -> graph_data {
	return build_graph_from_snapshot(sched.snapshot_graph());
}

auto gse::ide::build_graph_from_file(const std::filesystem::path& path) -> std::expected<graph_data, graph_load_error> {
	std::ifstream in(path, std::ios::binary);
	if (!in) {
		return std::unexpected(graph_load_error::unavailable);
	}
	std::expected<binary_reader, archive_mismatch> opened = binary_reader::open(in, introspection::system_graph_magic, introspection::system_graph_version);
	if (!opened) {
		return std::unexpected(opened.error().readable ? graph_load_error::incompatible : graph_load_error::incomplete);
	}
	introspection::system_graph snapshot;
	*opened & snapshot.nodes;
	*opened & snapshot.edges;
	if (!in) {
		return std::unexpected(graph_load_error::incomplete);
	}
	return build_graph_from_snapshot(std::move(snapshot));
}

auto gse::ide::build_graph_from_snapshot(introspection::system_graph snapshot) -> graph_data {
	graph_data gd;
	gd.snapshot = std::move(snapshot);
	merge_channels(gd);
	const std::size_t n = gd.snapshot.nodes.size();
	gd.index_of.reserve(n);
	for (std::uint32_t i = 0; i < n; ++i) {
		gd.index_of.emplace(gd.snapshot.nodes[i].id, i);
	}
	std::vector<std::pair<std::uint32_t, std::uint32_t>> pairs;
	pairs.reserve(gd.snapshot.edges.size());
	for (const auto& e : gd.snapshot.edges) {
		const auto ai = gd.index_of.find(e.from);
		const auto bi = gd.index_of.find(e.to);
		if (ai == gd.index_of.end() || bi == gd.index_of.end()) {
			continue;
		}
		gd.edges_draw.push_back({
			.a = ai->second,
			.b = bi->second,
			.kind = e.kind,
		});
		pairs.emplace_back(ai->second, bi->second);
	}
	gd.layout = graph::layered_layout(n, pairs);
	gd.built = true;
	return gd;
}

auto gse::ide::prepare_presentation(graph_data& gd, const gui::draw_context& ctx) -> void {
	const auto text_view = ctx.fonts.text.resolve();
	const id font_id = ctx.fonts.text.id();
	const std::uint32_t font_version = ctx.fonts.text.version();
	const float font_size = ctx.style.font_size;
	if (gd.presentation_ready && gd.presentation_font == font_id && gd.presentation_font_version == font_version && gd.presentation_font_size == font_size) {
		return;
	}

	const std::size_t n = gd.snapshot.nodes.size();
	const float pill_pad_x = 14.f;
	const float pill_h = font_size + 14.f;
	const float gap_x = 24.f;
	const float step_y = pill_h + 56.f;

	gd.labels.clear();
	gd.labels.reserve(n);
	gd.widths.resize(n);
	gd.base_colors.resize(n);
	for (std::uint32_t i = 0; i < n; ++i) {
		const introspection::graph_node& node = gd.snapshot.nodes[i];
		gd.labels.emplace_back(node_label(node));
		gd.widths[i] = std::max(72.f, text_view->width(gd.labels.back(), font_size) + pill_pad_x * 2.f);
		gd.base_colors[i] = category_color(node.category);
	}

	std::vector<std::vector<std::uint32_t>> by_layer(gd.layout.layer_count);
	for (std::uint32_t i = 0; i < n; ++i) {
		const graph::placement placement = gd.layout.placements[i];
		if (placement.layer < gd.layout.layer_count) {
			by_layer[placement.layer].push_back(i);
		}
	}
	for (std::vector<std::uint32_t>& row : by_layer) {
		std::ranges::sort(row, [&](const std::uint32_t a, const std::uint32_t b) {
			return gd.layout.placements[a].order < gd.layout.placements[b].order;
		});
	}

	gd.world.resize(n);
	gd.world_min = { std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
	gd.world_max = { std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest() };
	for (std::uint32_t layer = 0; layer < gd.layout.layer_count; ++layer) {
		float total = 0.f;
		for (const std::uint32_t index : by_layer[layer]) {
			total += gd.widths[index] + gap_x;
		}
		total -= by_layer[layer].empty() ? 0.f : gap_x;
		float x = -total * 0.5f;
		const float y = static_cast<float>(layer) * step_y;
		for (const std::uint32_t index : by_layer[layer]) {
			const float center_x = x + gd.widths[index] * 0.5f;
			gd.world[index] = { center_x, y };
			gd.world_min = {
				std::min(gd.world_min.x(), center_x - gd.widths[index] * 0.5f),
				std::min(gd.world_min.y(), y - pill_h * 0.5f),
			};
			gd.world_max = {
				std::max(gd.world_max.x(), center_x + gd.widths[index] * 0.5f),
				std::max(gd.world_max.y(), y + pill_h * 0.5f),
			};
			x += gd.widths[index] + gap_x;
		}
	}

	gd.centers.resize(n);
	gd.nodes.clear();
	gd.nodes.reserve(n);
	gd.edges.clear();
	gd.edges.reserve(gd.edges_draw.size() + gd.channel_edges.size());
	gd.highlighted.assign(n, false);
	gd.isolate_set.assign(n, false);
	gd.edge_lit.assign(gd.edges_draw.size(), false);
	gd.presentation_font = font_id;
	gd.presentation_font_version = font_version;
	gd.presentation_font_size = font_size;
	gd.presentation_ready = true;
}

auto gse::ide::merge_channels(graph_data& gd) -> void {
	gd.channel_uses.clear();
	gd.channel_edges.clear();

	const std::size_t n = gd.snapshot.nodes.size();
	std::unordered_map<std::string, std::vector<std::uint32_t>> producers;
	std::unordered_map<std::string, std::vector<std::uint32_t>> consumers;
	for (std::uint32_t i = 0; i < n; ++i) {
		const introspection::graph_node& node = gd.snapshot.nodes[i];
		for (const std::string& message : node.publishes) {
			gd.channel_uses.push_back({
				.node = i,
				.produce = true,
				.qualified = message,
			});
			producers[message].push_back(i);
		}
		for (const std::string& message : node.consumes) {
			gd.channel_uses.push_back({
				.node = i,
				.produce = false,
				.qualified = message,
			});
			consumers[message].push_back(i);
		}
	}

	for (const auto& [message, prod] : producers) {
		const auto cit = consumers.find(message);
		if (cit == consumers.end()) {
			continue;
		}
		for (const std::uint32_t p : prod) {
			for (const std::uint32_t consumer : cit->second) {
				if (p != consumer) {
					gd.channel_edges.push_back({
						.a = p,
						.b = consumer,
					});
				}
			}
		}
	}

	gd.detail_for = std::nullopt;
	gd.detail_generation = std::nullopt;
	gd.tooltip_for = std::nullopt;
	gd.edges.reserve(gd.edges_draw.size() + gd.channel_edges.size());
}

auto gse::ide::draw_graph(gui::builder& ui, const rectf& area, graph_data& gd, const search::index_state* index, channel_write<jump_to_request, set_cursor_shape_request> channels) -> void {
	const gui::draw_context& ctx = ui.ctx;
	const auto text_view = ctx.fonts.text.resolve();
	const std::size_t n = gd.snapshot.nodes.size();
	if (n == 0) {
		ctx.queue_sprite({
			.rect = area,
			.color = { 0.09f, 0.10f, 0.13f, 1.f },
			.texture = ctx.blank_texture,
		});
		return;
	}

	prepare_presentation(gd, ctx);

	const vec2f mouse = ctx.mouse_position();

	rectf canvas = area;
	std::optional<rectf> panel_area;
	std::optional<rectf> panel_divider;
	if (gd.selected) {
		const float divider_thickness = std::max(6.f, ctx.style.resize_border_thickness) * 2.f;
		const gui::layout::split_result split = gui::layout::update_split(
			{
				.container = area,
				.axis = gui::layout::split_axis::columns,
				.ratio = std::clamp(1.f - gd.panel_ratio, 0.f, 1.f),
				.min_first = 240.f,
				.min_second = 220.f,
				.divider_thickness = divider_thickness,
			},
			{
				.mouse = mouse,
				.pressed = ctx.mouse_pressed(mouse_button::button_1) && ctx.input_available(),
				.held = ctx.mouse_held(mouse_button::button_1),
			},
			gd.resizing_panel
		);
		canvas = split.first;
		panel_area = split.second;
		panel_divider = split.divider;
		gd.panel_ratio = 1.f - split.ratio;
	}

	const bool over_panel = panel_area && ctx.hovers(*panel_area);
	const bool over_divider = panel_divider && ctx.hovers(*panel_divider);
	const bool over_legend = ctx.hovers(legend_bounds(ctx, canvas));
	const float reset_h = text_view->line_height(ctx.style.font_size) + 8.f;
	const float reset_w = text_view->width(std::string_view("reset view"), ctx.style.font_size) + 24.f;
	const rectf reset_rect = rectf::from_position_size({ canvas.left() + 10.f, canvas.bottom() + 10.f + reset_h }, { reset_w, reset_h });
	const bool over_reset = ctx.hovers(reset_rect);
	const bool over_area = ctx.hovers(canvas) && !over_panel && !over_divider && !over_legend && !over_reset;

	if (gd.panning && ctx.mouse_held(mouse_button::button_1)) {
		gd.pan += mouse - gd.pan_last;
		gd.pan_last = mouse;
		const vec2f moved = mouse - gd.pan_press;
		const float drag_slop = 4.f;
		if (moved.x() * moved.x() + moved.y() * moved.y() > drag_slop * drag_slop) {
			gd.dragged = true;
		}
	}
	else {
		gd.panning = false;
	}

	const auto edge_visible = [&](const introspection::edge_kind kind) {
		const auto found = std::ranges::find(edge_kinds, kind);
		return found != edge_kinds.end() && gd.kind_visible[static_cast<std::size_t>(found - edge_kinds.begin())];
	};
	std::ranges::fill(gd.isolate_set, false);
	if (gd.isolated) {
		const auto isolated = gd.index_of.find(*gd.isolated);
		if (isolated != gd.index_of.end()) {
			gd.isolate_set[isolated->second] = true;
		}
		const auto add_neighbors = [&](const std::uint32_t ai, const std::uint32_t bi) {
			const std::uint64_t a_id = gd.snapshot.nodes[ai].id;
			const std::uint64_t b_id = gd.snapshot.nodes[bi].id;
			if (a_id == *gd.isolated) {
				gd.isolate_set[bi] = true;
			}
			if (b_id == *gd.isolated) {
				gd.isolate_set[ai] = true;
			}
		};
		for (const edge_draw& edge : gd.edges_draw) {
			if (edge_visible(edge.kind)) {
				add_neighbors(edge.a, edge.b);
			}
		}
		if (edge_visible(introspection::edge_kind::channel)) {
			for (const channel_link_draw& edge : gd.channel_edges) {
				add_neighbors(edge.a, edge.b);
			}
		}
	}

	const float pill_h = ctx.style.font_size + 14.f;
	const vec2f world_center = (gd.world_min + gd.world_max) * 0.5f;
	const float world_w = std::max(1.f, gd.world_max.x() - gd.world_min.x());
	const float world_h = std::max(1.f, gd.world_max.y() - gd.world_min.y());
	const float fit = std::max(1e-4f, std::min((area.width() - 60.f) / world_w, (area.height() - 60.f) / world_h));
	const float min_scale = 0.15f;
	const float max_scale = 3.f;
	if (over_area) {
		const vec2f scroll = ctx.scroll_delta_for(canvas);
		if (scroll.y() != 0.f) {
			const float old_scale = std::clamp(fit * gd.zoom, min_scale, max_scale);
			const float new_scale = std::clamp(old_scale * (scroll.y() > 0.f ? 1.12f : 1.f / 1.12f), min_scale, max_scale);
			const float ratio = new_scale / old_scale;
			gd.zoom = new_scale / fit;
			gd.pan = gd.pan * ratio + (mouse - area.center()) * (1.f - ratio);
		}
	}
	const float scale = std::clamp(fit * gd.zoom, min_scale, max_scale);

	gd.nodes.clear();
	std::optional<std::uint64_t> hovered;
	for (std::uint32_t i = 0; i < n; ++i) {
		const float sx = area.center().x() + gd.pan.x() + (gd.world[i].x() - world_center.x()) * scale;
		const float sy = area.center().y() + gd.pan.y() - (gd.world[i].y() - world_center.y()) * scale;
		const float nw = gd.widths[i] * scale;
		const float nh = pill_h * scale;
		gd.centers[i] = { sx, sy };
		const bool ghosted = gd.isolated && !gd.isolate_set[i];
		const rectf node_rect = rectf::from_position_size({ sx - nw * 0.5f, sy + nh * 0.5f }, { nw, nh });
		if (!ghosted && over_area && ctx.hovers(node_rect)) {
			hovered = gd.snapshot.nodes[i].id;
		}
		gd.nodes.push_back({
			.key = gd.snapshot.nodes[i].id,
			.label = ghosted ? std::string_view{} : std::string_view(gd.labels[i]),
			.rect = node_rect,
			.color = gd.base_colors[i],
			.interactive = !ghosted && over_area,
		});
	}

	const std::optional<std::uint64_t> focus = gd.selected ? gd.selected : hovered;
	std::ranges::fill(gd.highlighted, false);
	std::ranges::fill(gd.edge_lit, false);
	if (focus) {
		const auto focused = gd.index_of.find(*focus);
		if (focused != gd.index_of.end()) {
			gd.highlighted[focused->second] = true;
		}
		for (std::size_t edge_index = 0; edge_index < gd.edges_draw.size(); ++edge_index) {
			const edge_draw& edge = gd.edges_draw[edge_index];
			if (!edge_visible(edge.kind)) {
				continue;
			}
			const std::uint64_t a_id = gd.snapshot.nodes[edge.a].id;
			const std::uint64_t b_id = gd.snapshot.nodes[edge.b].id;
			if (a_id == *focus || b_id == *focus) {
				gd.edge_lit[edge_index] = true;
				gd.highlighted[edge.a] = true;
				gd.highlighted[edge.b] = true;
			}
		}
		if (edge_visible(introspection::edge_kind::channel)) {
			for (const channel_link_draw& edge : gd.channel_edges) {
				const std::uint64_t a_id = gd.snapshot.nodes[edge.a].id;
				const std::uint64_t b_id = gd.snapshot.nodes[edge.b].id;
				if (a_id == *focus || b_id == *focus) {
					gd.highlighted[edge.a] = true;
					gd.highlighted[edge.b] = true;
				}
			}
		}
	}

	for (std::uint32_t i = 0; i < n; ++i) {
		vec4f color = gd.base_colors[i];
		const bool ghosted = gd.isolated && !gd.isolate_set[i];
		if (ghosted) {
			color = { color.x() * 0.16f, color.y() * 0.16f, color.z() * 0.18f, 0.07f };
		}
		else if (focus) {
			if (gd.snapshot.nodes[i].id == *focus) {
				color = { std::min(1.f, color.x() + 0.22f), std::min(1.f, color.y() + 0.22f), std::min(1.f, color.z() + 0.22f), 1.f };
			}
			else if (!gd.highlighted[i]) {
				color = { color.x() * 0.32f, color.y() * 0.32f, color.z() * 0.34f, 0.5f };
			}
		}
		gd.nodes[i].color = color;
	}

	const auto border_point = [](const vec2f center, const vec2f dir, const rectf& rect) -> vec2f {
		const float hw = rect.width() * 0.5f;
		const float hh = rect.height() * 0.5f;
		float t = std::numeric_limits<float>::max();
		if (std::abs(dir.x()) > 1e-4f) {
			t = std::min(t, hw / std::abs(dir.x()));
		}
		if (std::abs(dir.y()) > 1e-4f) {
			t = std::min(t, hh / std::abs(dir.y()));
		}
		return center + dir * t;
	};

	gd.edges.clear();
	for (std::size_t edge_index = 0; edge_index < gd.edges_draw.size(); ++edge_index) {
		const edge_draw& edge = gd.edges_draw[edge_index];
		if (!edge_visible(edge.kind)) {
			continue;
		}
		if (gd.isolated && (!gd.isolate_set[edge.a] || !gd.isolate_set[edge.b])) {
			continue;
		}
		vec4f color = edge_color(edge.kind);
		if (focus && !gd.edge_lit[edge_index]) {
			color = { color.x() * 0.4f, color.y() * 0.4f, color.z() * 0.4f, 0.22f };
		}
		const vec2f delta = gd.centers[edge.b] - gd.centers[edge.a];
		const float dist = std::sqrt(delta.x() * delta.x() + delta.y() * delta.y());
		if (dist < 1.f) {
			continue;
		}
		const vec2f dir = delta * (1.f / dist);
		gd.edges.push_back({
			.from = border_point(gd.centers[edge.a], dir, gd.nodes[edge.a].rect),
			.to = border_point(gd.centers[edge.b], dir * -1.f, gd.nodes[edge.b].rect),
			.color = color,
		});
	}

	if (focus && edge_visible(introspection::edge_kind::channel)) {
		for (const channel_link_draw& edge : gd.channel_edges) {
			if (gd.snapshot.nodes[edge.a].id != *focus && gd.snapshot.nodes[edge.b].id != *focus) {
				continue;
			}
			if (gd.isolated && (!gd.isolate_set[edge.a] || !gd.isolate_set[edge.b])) {
				continue;
			}
			const vec2f delta = gd.centers[edge.b] - gd.centers[edge.a];
			const float dist = std::sqrt(delta.x() * delta.x() + delta.y() * delta.y());
			if (dist < 1.f) {
				continue;
			}
			const vec2f dir = delta * (1.f / dist);
			gd.edges.push_back({
				.from = border_point(gd.centers[edge.a], dir, gd.nodes[edge.a].rect),
				.to = border_point(gd.centers[edge.b], dir * -1.f, gd.nodes[edge.b].rect),
				.color = edge_color(introspection::edge_kind::channel),
			});
		}
	}

	const gui::graph_canvas::result picked = ui.draw<gui::graph_canvas>({
		.area = area,
		.nodes = gd.nodes,
		.edges = gd.edges,
		.label_scale = scale,
	});

	draw_legend(ctx, canvas, gd);

	gui::draw::panel_backdrop(ctx, {
		.rect = reset_rect,
		.background = over_reset ? vec4f{ 0.18f, 0.20f, 0.26f, 0.95f } : vec4f{ 0.09f, 0.10f, 0.13f, 0.9f },
		.clip = reset_rect,
		.layer = render_layer::overlay,
	});
	ctx.queue_text({
		.font = ctx.fonts.text,
		.text = "reset view",
		.position = { reset_rect.left() + 12.f, reset_rect.center().y() + text_view->vertical_center_offset(ctx.style.font_size) },
		.scale = ctx.style.font_size,
		.color = ctx.style.color_text,
		.layer = render_layer::overlay,
	});
	if (over_reset && ctx.mouse_pressed_for(reset_rect)) {
		gd.pan = { 0.f, 0.f };
		gd.zoom = 1.f;
		gd.isolated = std::nullopt;
		gd.selected = std::nullopt;
	}

	if (over_area && ctx.mouse_pressed_for(canvas)) {
		gd.panning = true;
		gd.dragged = false;
		gd.pan_press = mouse;
		gd.pan_last = mouse;
	}

	if (gd.selected && panel_area) {
		draw_detail_panel(ui, *panel_area, gd, index, channels);
	}
	else if (picked.hovered) {
		draw_node_tooltip(ctx, canvas, gd, index, *picked.hovered, mouse);
	}

	if (panel_divider && (over_divider || gd.resizing_panel.dragging)) {
		channels.push<set_cursor_shape_request>({
			.shape = cursor_shape::resize_ew,
		});
	}

	if (over_panel || over_divider || over_legend || over_reset || gd.resizing_panel.dragging || gd.dragged) {
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
	if (gui::interaction::register_click(gd.click, mouse) >= 2) {
		gd.isolated = *picked.clicked;
		gd.selected = *picked.clicked;
		return;
	}
	if (gd.selected != picked.clicked) {
		gd.selected = picked.clicked;
		return;
	}
	const introspection::graph_node* clicked_node = find_node(gd, *picked.clicked);
	if (!clicked_node || clicked_node->file.empty()) {
		return;
	}
	channels.push<jump_to_request>({
		.path = clicked_node->file,
		.line = clicked_node->line,
		.column = clicked_node->column,
	});
}

auto gse::ide::legend_bounds(const gui::draw_context& ctx, const rectf& area) -> rectf {
	const auto text_view = ctx.fonts.text.resolve();
	const float pad = 10.f;
	const float row_h = ctx.style.font_size + 8.f;
	const float sw = 12.f;
	float max_text = 0.f;
	for (const introspection::edge_kind kind : edge_kinds) {
		const introspection::edge_kind_info info = introspection::edge_info(kind);
		max_text = std::max(max_text, text_view->width(info.label, ctx.style.font_size));
	}
	const float w = pad + sw + 8.f + max_text + pad + 4.f;
	const float h = row_h * static_cast<float>(edge_kinds.size()) + pad * 2.f;
	return rectf::from_position_size({ area.left() + pad, area.top() - pad }, { w, h });
}

auto gse::ide::draw_legend(const gui::draw_context& ctx, const rectf& area, graph_data& gd) -> void {
	const auto text_view = ctx.fonts.text.resolve();
	const float pad = 10.f;
	const float row_h = ctx.style.font_size + 8.f;
	const float sw = 12.f;
	const rectf panel = legend_bounds(ctx, area);
	gui::draw::panel_backdrop(ctx, {
		.rect = panel,
		.background = { 0.05f, 0.06f, 0.08f, 0.9f },
		.clip = panel,
		.layer = render_layer::overlay,
	});
	for (std::size_t i = 0; i < edge_kinds.size(); ++i) {
		const introspection::edge_kind kind = edge_kinds[i];
		const introspection::edge_kind_info info = introspection::edge_info(kind);
		const float ry = panel.top() - pad - static_cast<float>(i) * row_h;
		const rectf row_rect = rectf::from_position_size({ panel.left() + pad * 0.5f, ry }, { panel.width() - pad, row_h });
		const bool hovered = ctx.hovers(row_rect);
		if (hovered && ctx.mouse_pressed_for(row_rect)) {
			gd.kind_visible[i] = !gd.kind_visible[i];
		}
		const bool on = gd.kind_visible[i];
		if (hovered) {
			ctx.queue_sprite({
				.rect = row_rect,
				.color = { 1.f, 1.f, 1.f, 0.07f },
				.texture = ctx.blank_texture,
				.layer = render_layer::overlay,
				.corner_radius = 3.f,
			});
		}
		vec4f swatch = edge_color(kind);
		if (!on) {
			swatch = { swatch.x() * 0.28f, swatch.y() * 0.28f, swatch.z() * 0.30f, 0.5f };
		}
		ctx.queue_sprite({
			.rect = rectf::from_position_size({ panel.left() + pad, ry - (row_h - sw) * 0.5f }, { sw, sw }),
			.color = swatch,
			.texture = ctx.blank_texture,
			.layer = render_layer::overlay,
			.corner_radius = 2.f,
		});
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = info.label,
			.position = { panel.left() + pad + sw + 8.f, ry - row_h * 0.5f + text_view->vertical_center_offset(ctx.style.font_size) },
			.scale = ctx.style.font_size,
			.color = on ? ctx.style.color_text : ctx.style.color_text_secondary,
			.layer = render_layer::overlay,
		});
	}
}

auto gse::ide::draw_detail_panel(gui::builder& ui, const rectf& panel, graph_data& gd, const search::index_state* index, channel_write<jump_to_request, set_cursor_shape_request> channels) -> void {
	gui::draw_context& ctx = ui.ctx;
	if (!gd.selected) {
		return;
	}
	const auto text_view = ctx.fonts.text.resolve();
	const introspection::graph_node* node = find_node(gd, *gd.selected);
	if (!node) {
		return;
	}

	const float fs = ctx.style.font_size;
	const float pad = 12.f;
	const float line_h = fs + 6.f;

	gui::draw::panel_backdrop(ctx, {
		.rect = panel,
		.background = { 0.06f, 0.07f, 0.10f, 0.97f },
		.accent = gui::panel_accent{
			.edge = gui::panel_edge::left,
			.width = 3.f,
			.color = category_color(node->category),
		},
		.clip = panel,
		.layer = render_layer::overlay,
	});

	const gui::layer_scope detail_layer = ctx.scoped_layer(render_layer::overlay);
	const gui::layout::within_scope detail_area = gui::layout::within(ctx, panel);
	const gui::ids::scope detail_id_scope(*gd.selected);
	const gui::scroll_handle detail_view = gui::scroll_region(ctx, {
		.id = "graph_detail",
	});
	gui::layout::skip(ctx, pad);

	const float x = panel.left() + pad + 4.f;
	const auto text_line = [&](const std::string_view text, const vec4f color, const float indent, const float size) {
		const rectf row = gui::layout::reserve_row(ctx, size + 6.f);
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = text,
			.position = { x + indent, row.center().y() + text_view->vertical_center_offset(size) },
			.scale = size,
			.color = color,
		});
	};

	text_line(node_label(*node), ctx.style.color_text, 0.f, fs);
	text_line(node->category, ctx.style.color_text_secondary, 0.f, fs);
	gui::layout::skip(ctx, line_h * 0.4f);

	const std::pair<bool, std::string_view> phase_chips[] = {
		{ node->has_init, "init" },
		{ node->has_run, "run" },
		{ node->has_frame, "frame" },
		{ node->deferred, "deferred" },
	};
	const rectf chip_row = gui::layout::reserve_row(ctx, line_h);
	float cx = x;
	for (const auto& [on, label] : phase_chips) {
		if (!on) {
			continue;
		}
		const float cw = text_view->width(label, fs) + 12.f;
		const rectf chip = rectf::from_position_size({ cx, chip_row.top() }, { cw, line_h });
		ctx.queue_sprite({
			.rect = chip,
			.color = { 0.17f, 0.21f, 0.28f, 1.f },
			.texture = ctx.blank_texture,
			.corner_radius = line_h * 0.3f,
		});
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = label,
			.position = { cx + 6.f, chip.center().y() + text_view->vertical_center_offset(fs) },
			.scale = fs,
			.color = ctx.style.color_text,
		});
		cx += cw + 6.f;
	}
	gui::layout::skip(ctx, line_h * 0.6f);

	const float detail_size = fs * 0.9f;
	const float via_width = std::max(60.f, panel.width() - pad * 2.f - 30.f);
	node_relations& rel = ensure_relations(gd, *gd.selected, index);
	if (gd.detail_wrap_width != via_width || gd.detail_wrap_font_size != detail_size) {
		const auto wrap_items = [&](std::vector<list_item>& items) {
			for (list_item& item : items) {
				item.via_lines = wrap_joined(ctx, item.via, via_width, detail_size);
			}
		};
		wrap_items(rel.reads);
		wrap_items(rel.writes);
		wrap_items(rel.depends);
		wrap_items(rel.feeds);
		wrap_items(rel.publishes);
		wrap_items(rel.consumes);
		gd.detail_wrap_width = via_width;
		gd.detail_wrap_font_size = detail_size;
	}

	const auto link_row = [&](const std::string_view header, const list_item& item) {
		const rectf row = gui::layout::reserve_row(ctx, line_h);
		std::uint64_t row_key = hash_combine(*gd.selected, stable_id(header));
		row_key = hash_combine(row_key, item.node_id.value_or(stable_id(item.qualified)));
		const id row_id = gui::ids::make_from_key(row_key);
		const auto hit = gui::interaction::press_in_rect(ctx, ui.hot_widget_id, ui.active_widget_id, row_id, row, item.linkable);
		const bool hovered = hit.hovered;
		const bool clicked = hit.activated;

		const float text_x = x + 12.f;
		if (hovered) {
			ctx.queue_sprite({
				.rect = row,
				.color = { 1.f, 1.f, 1.f, 0.06f },
				.texture = ctx.blank_texture,
				.corner_radius = 3.f,
			});
			ctx.queue_sprite({
				.rect = rectf::from_position_size(
					{ text_x, row.center().y() - fs * 0.45f },
					{ text_view->width(item.display, fs), std::max(1.f, ctx.style.scale_factor) }
				),
				.color = ctx.style.color_accent,
				.texture = ctx.blank_texture,
			});
			channels.push<set_cursor_shape_request>({
				.shape = cursor_shape::hand,
			});
		}
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = item.display,
			.position = { text_x, row.center().y() + text_view->vertical_center_offset(fs) },
			.scale = fs,
			.color = hovered ? ctx.style.color_accent : (item.linkable ? ctx.style.color_text : ctx.style.color_text_secondary),
		});
		if (clicked && item.target) {
			channels.push<jump_to_request>({
				.path = item.target->path,
				.line = item.target->line,
				.column = item.target->column,
			});
		}
	};

	const auto section = [&](const std::string_view header, const std::span<const list_item> items) {
		if (items.empty()) {
			return;
		}
		text_line(header, ctx.style.color_accent, 0.f, fs);
		for (const list_item& item : items) {
			link_row(header, item);
			for (const std::string& line : item.via_lines) {
				text_line(line, ctx.style.color_text_secondary, 22.f, detail_size);
			}
		}
		gui::layout::skip(ctx, line_h * 0.5f);
	};

	section(rel.reads_header, rel.reads);
	section(rel.writes_header, rel.writes);
	section(rel.depends_header, rel.depends);
	section(rel.feeds_header, rel.feeds);
	section(rel.publishes_header, rel.publishes);
	section(rel.consumes_header, rel.consumes);
}

auto gse::ide::draw_node_tooltip(const gui::draw_context& ctx, const rectf& area, graph_data& gd, const search::index_state* index, const std::uint64_t key, const vec2f anchor) -> void {
	const introspection::graph_node* node = find_node(gd, key);
	if (!node) {
		return;
	}
	const auto text_view = ctx.fonts.text.resolve();

	const float fs = ctx.style.font_size;
	const float wrap_width = std::clamp(area.width() * 0.4f, 200.f, 380.f);
	const float indent = 10.f;
	const node_relations& rel = ensure_relations(gd, key, index);
	const bool rebuild = gd.tooltip_for != key || gd.tooltip_generation != gd.detail_generation || gd.tooltip_width != wrap_width || gd.tooltip_font_size != fs || gd.tooltip_text_color != ctx.style.color_text || gd.tooltip_secondary_color != ctx.style.color_text_secondary || gd.tooltip_accent_color != ctx.style.color_accent;
	if (rebuild) {
		gd.tooltip_lines.clear();
		gd.tooltip_lines.push_back({
			.text = std::string(node_label(*node)),
			.color = ctx.style.color_text,
		});

		std::string tags;
		const auto append_tag = [&](const std::string_view tag) {
			if (tag.empty()) {
				return;
			}
			if (!tags.empty()) {
				tags += " / ";
			}
			tags += tag;
		};
		append_tag(node->category);
		append_tag(node->has_init ? "init" : std::string_view{});
		append_tag(node->has_run ? "run" : std::string_view{});
		append_tag(node->has_frame ? "frame" : std::string_view{});
		append_tag(node->deferred ? "deferred" : std::string_view{});
		if (!tags.empty()) {
			gd.tooltip_lines.push_back({
				.text = std::move(tags),
				.color = ctx.style.color_text_secondary,
			});
		}

		const auto section = [&](const std::string_view header, const std::span<const list_item> items) {
			if (items.empty()) {
				return;
			}
			gd.tooltip_lines.push_back({
				.text = std::string(header),
				.color = ctx.style.color_accent,
			});
			std::vector<std::string> wrapped = wrap_displays(ctx, items, wrap_width - indent, fs);
			constexpr std::size_t max_lines = 2;
			if (wrapped.size() > max_lines) {
				wrapped.resize(max_lines);
				wrapped.back() += " ...";
			}
			for (std::string& line : wrapped) {
				gd.tooltip_lines.push_back({
					.text = std::move(line),
					.color = ctx.style.color_text_secondary,
					.indent = indent,
				});
			}
		};
		section(rel.reads_header, rel.reads);
		section(rel.writes_header, rel.writes);
		section(rel.depends_header, rel.depends);
		section(rel.feeds_header, rel.feeds);
		section(rel.publishes_header, rel.publishes);
		section(rel.consumes_header, rel.consumes);
		gd.tooltip_for = key;
		gd.tooltip_generation = gd.detail_generation;
		gd.tooltip_width = wrap_width;
		gd.tooltip_font_size = fs;
		gd.tooltip_text_color = ctx.style.color_text;
		gd.tooltip_secondary_color = ctx.style.color_text_secondary;
		gd.tooltip_accent_color = ctx.style.color_accent;
	}

	const float pad = 8.f;
	const float line_h = fs + 4.f;
	float max_w = 0.f;
	for (const graph_text_line& line : gd.tooltip_lines) {
		max_w = std::max(max_w, text_view->width(line.text, fs) + line.indent);
	}

	const float pw = max_w + pad * 2.f;
	const float ph = line_h * static_cast<float>(gd.tooltip_lines.size()) + pad * 2.f;
	float px = anchor.x() + 16.f;
	float py = anchor.y() - 16.f;
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
	const rectf panel = rectf::from_position_size({ px, py }, { pw, ph });
	ctx.queue_sprite({
		.rect = panel.inset({ -1.f, -1.f }),
		.color = { 0.32f, 0.36f, 0.44f, 0.9f },
		.texture = ctx.blank_texture,
		.layer = render_layer::popup,
		.corner_radius = 5.f,
	});
	ctx.queue_sprite({
		.rect = panel,
		.color = { 0.06f, 0.07f, 0.10f, 0.97f },
		.texture = ctx.blank_texture,
		.layer = render_layer::popup,
		.corner_radius = 4.f,
	});
	for (std::size_t i = 0; i < gd.tooltip_lines.size(); ++i) {
		const float ly = panel.top() - pad - static_cast<float>(i) * line_h - line_h * 0.5f;
		ctx.queue_text({
			.font = ctx.fonts.text,
			.text = gd.tooltip_lines[i].text,
			.position = { panel.left() + pad + gd.tooltip_lines[i].indent, ly + text_view->vertical_center_offset(fs) },
			.scale = fs,
			.color = gd.tooltip_lines[i].color,
			.layer = render_layer::popup,
		});
	}
}

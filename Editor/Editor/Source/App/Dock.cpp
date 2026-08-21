module gse.ide.app:dock_impl;

import std;
import gse;

import :dock;

auto gse::ide::is_leaf(const dock_node& node) -> bool {
	return !node.first.exists() && !node.second.exists();
}

auto gse::ide::panel_desc_for(const std::span<const panel_desc> panels, const id id) -> const panel_desc* {
	const auto it = std::ranges::find(panels, id, &panel_desc::id);
	return it == panels.end() ? nullptr : &*it;
}

auto gse::ide::any_leaf(const dock_tree& tree) -> id {
	for (std::size_t i = 0; i < tree.nodes.size(); ++i) {
		if (is_leaf(tree.nodes.items()[i])) {
			return tree.nodes.ids()[i];
		}
	}
	return {};
}

auto gse::ide::make_node(dock_tree& tree) -> id {
	std::uint64_t next = 1;
	for (const id existing : tree.nodes.ids()) {
		next = std::max(next, existing.number() + 1);
	}
	const id node_id = generate_temp_id(next);
	tree.nodes.add(node_id, {});
	return node_id;
}

auto gse::ide::find_leaf(const dock_tree& tree, const id panel) -> id {
	for (std::size_t i = 0; i < tree.nodes.size(); ++i) {
		if (const dock_node& node = tree.nodes.items()[i]; is_leaf(node) && std::ranges::find(node.panels, panel) != node.panels.end()) {
			return tree.nodes.ids()[i];
		}
	}
	return {};
}

auto gse::ide::contains_panel(const dock_tree& tree, const id panel) -> bool {
	return find_leaf(tree, panel).exists();
}

auto gse::ide::panel_count(const dock_tree& tree) -> std::size_t {
	std::size_t total = 0;
	for (const dock_node& node : tree.nodes.items()) {
		total += node.panels.size();
	}
	return total;
}

auto gse::ide::activate_panel(dock_tree& tree, const id panel) -> void {
	const id leaf_id = find_leaf(tree, panel);
	dock_node* leaf = tree.nodes.try_get(leaf_id);
	if (!leaf) {
		return;
	}
	const auto it = std::ranges::find(leaf->panels, panel);
	leaf->active_panel = static_cast<std::uint32_t>(std::distance(leaf->panels.begin(), it));
}

auto gse::ide::replace_child(dock_tree& tree, const id parent, const id from, const id to) -> void {
	dock_node* node = tree.nodes.try_get(parent);
	if (!node) {
		tree.root = to;
	}
	else if (node->first == from) {
		node->first = to;
	}
	else if (node->second == from) {
		node->second = to;
	}

	if (dock_node* moved = tree.nodes.try_get(to)) {
		moved->parent = parent;
	}
}

auto gse::ide::detach_child(dock_tree& tree, const id parent, const id child) -> void {
	dock_node* node = tree.nodes.try_get(parent);
	if (!node) {
		return;
	}
	const id sibling = node->first == child ? node->second : node->first;
	const id grandparent = node->parent;
	replace_child(tree, grandparent, parent, sibling);
	tree.nodes.remove(parent);
	tree.nodes.remove(child);
}

auto gse::ide::prune_empty_leaves(dock_tree& tree) -> void {
	for (bool pruned = true; pruned;) {
		pruned = false;
		for (std::size_t i = 0; i < tree.nodes.size(); ++i) {
			const dock_node& node = tree.nodes.items()[i];
			if (!is_leaf(node) || !node.panels.empty()) {
				continue;
			}
			const id leaf_id = tree.nodes.ids()[i];
			if (leaf_id == tree.root) {
				tree.nodes.remove(leaf_id);
				tree.root.reset();
			}
			else {
				detach_child(tree, node.parent, leaf_id);
			}
			pruned = true;
			break;
		}
	}
}

auto gse::ide::collect_nodes(const dock_tree& tree, const id node, std::vector<id>& out) -> void {
	const dock_node* current = tree.nodes.try_get(node);
	if (!current) {
		return;
	}
	out.push_back(node);
	collect_nodes(tree, current->first, out);
	collect_nodes(tree, current->second, out);
}

auto gse::ide::insert_panel(dock_tree& tree, const dock_insert& what) -> void {
	remove_panel(tree, what.panel);

	if (!tree.root.exists()) {
		const id leaf_id = make_node(tree);
		tree.nodes.try_get(leaf_id)->panels.push_back(what.panel);
		tree.root = leaf_id;
		return;
	}

	const id anchor = tree.nodes.contains(what.target) ? what.target : tree.root;

	if (what.location == gui::dock::location::center || what.location == gui::dock::location::none) {
		const id host = is_leaf(*tree.nodes.try_get(anchor)) ? anchor : any_leaf(tree);
		dock_node* leaf = tree.nodes.try_get(host);
		leaf->panels.push_back(what.panel);
		leaf->active_panel = static_cast<std::uint32_t>(leaf->panels.size() - 1);
		return;
	}

	const id new_leaf = make_node(tree);
	tree.nodes.try_get(new_leaf)->panels.push_back(what.panel);

	const id split_id = make_node(tree);
	const id anchor_parent = tree.nodes.try_get(anchor)->parent;

	const bool new_first = what.location == gui::dock::location::left || what.location == gui::dock::location::top;
	dock_node* split = tree.nodes.try_get(split_id);
	split->axis = what.location == gui::dock::location::left || what.location == gui::dock::location::right
		? gui::layout::split_axis::columns
		: gui::layout::split_axis::rows;
	split->first = new_first ? new_leaf : anchor;
	split->second = new_first ? anchor : new_leaf;
	split->ratio = std::clamp(new_first ? what.ratio : 1.f - what.ratio, 0.05f, 0.95f);

	replace_child(tree, anchor_parent, anchor, split_id);
	tree.nodes.try_get(new_leaf)->parent = split_id;
	tree.nodes.try_get(anchor)->parent = split_id;
}

auto gse::ide::remove_panel(dock_tree& tree, const id panel) -> void {
	const id leaf_id = find_leaf(tree, panel);
	dock_node* leaf = tree.nodes.try_get(leaf_id);
	if (!leaf) {
		return;
	}

	const auto it = std::ranges::find(leaf->panels, panel);
	const auto removed = static_cast<std::uint32_t>(std::distance(leaf->panels.begin(), it));
	leaf->panels.erase(it);

	if (tree.maximized == panel) {
		tree.maximized.reset();
	}

	if (!leaf->panels.empty()) {
		if (leaf->active_panel >= leaf->panels.size()) {
			leaf->active_panel = static_cast<std::uint32_t>(leaf->panels.size() - 1);
		}
		else if (leaf->active_panel > removed) {
			leaf->active_panel -= 1;
		}
		return;
	}

	if (leaf_id == tree.root) {
		tree.nodes.remove(leaf_id);
		tree.root.reset();
		return;
	}

	detach_child(tree, leaf->parent, leaf_id);
}

auto gse::ide::min_size_of(const dock_tree& tree, const id node, const dock_metrics& metrics, const std::span<const panel_desc> panels) -> vec2f {
	const dock_node* current = tree.nodes.try_get(node);
	if (!current) {
		return {};
	}

	if (is_leaf(*current)) {
		vec2f smallest{ 0.f, 0.f };
		for (const id panel : current->panels) {
			if (const panel_desc* desc = panel_desc_for(panels, panel)) {
				smallest.x() = std::max(smallest.x(), desc->min_size.x() * metrics.scale);
				smallest.y() = std::max(smallest.y(), desc->min_size.y() * metrics.scale);
			}
		}
		smallest.y() += metrics.header_height * metrics.scale;
		return smallest;
	}

	const vec2f a = min_size_of(tree, current->first, metrics, panels);
	const vec2f b = min_size_of(tree, current->second, metrics, panels);
	const float gutter = metrics.divider_thickness * metrics.scale;

	if (current->axis == gui::layout::split_axis::columns) {
		return { a.x() + b.x() + gutter, std::max(a.y(), b.y()) };
	}
	return { std::max(a.x(), b.x()), a.y() + b.y() + gutter };
}

auto gse::ide::split_params_for(const dock_tree& tree, const dock_node& node, const rectf& area, const dock_metrics& metrics, const std::span<const panel_desc> panels) -> gui::layout::split_params {
	const vec2f first_min = min_size_of(tree, node.first, metrics, panels);
	const vec2f second_min = min_size_of(tree, node.second, metrics, panels);
	const bool columns = node.axis == gui::layout::split_axis::columns;
	return {
		.container = area,
		.axis = node.axis,
		.ratio = node.ratio,
		.min_first = columns ? first_min.x() : first_min.y(),
		.min_second = columns ? second_min.x() : second_min.y(),
		.divider_thickness = metrics.divider_thickness * metrics.scale,
		.divider_bias = columns ? 0.f : 1.f,
	};
}

auto gse::ide::place_node(const dock_tree& tree, const id node, const rectf& area, const dock_metrics& metrics, const std::span<const panel_desc> panels, dock_layout& out) -> void {
	const dock_node* current = tree.nodes.try_get(node);
	if (!current) {
		return;
	}

	if (is_leaf(*current)) {
		out.leaves.push_back({
			.node = node,
			.rect = area,
		});
		return;
	}

	const gui::layout::split_result split = gui::layout::resolve_split(split_params_for(tree, *current, area, metrics, panels));
	out.dividers.push_back({
		.node = node,
		.rect = split.divider,
		.axis = current->axis,
	});
	place_node(tree, current->first, split.first, metrics, panels, out);
	place_node(tree, current->second, split.second, metrics, panels, out);
}

auto gse::ide::resolve(const dock_tree& tree, const rectf& frame, const dock_metrics& metrics, const std::span<const panel_desc> panels) -> dock_layout {
	dock_layout out;
	if (const id leaf = find_leaf(tree, tree.maximized); leaf.exists()) {
		out.leaves.push_back({
			.node = leaf,
			.rect = frame,
		});
		return out;
	}
	place_node(tree, tree.root, frame, metrics, panels, out);
	return out;
}

auto gse::ide::drag_split_ratios(dock_tree& tree, const id node, const rectf& area, const dock_metrics& metrics, const std::span<const panel_desc> panels, const gui::layout::split_drag& drag, bool& consumed) -> void {
	dock_node* current = tree.nodes.try_get(node);
	if (!current || is_leaf(*current)) {
		return;
	}

	gui::layout::split_drag local = drag;
	local.blocked = drag.blocked || (consumed && !current->divider_drag.dragging);

	const gui::layout::split_result split = gui::layout::update_split(split_params_for(tree, *current, area, metrics, panels), local, current->divider_drag);
	if (current->divider_drag.dragging) {
		current->ratio = split.ratio;
	}
	consumed = consumed || current->divider_drag.dragging;

	const id first = current->first;
	const id second = current->second;
	drag_split_ratios(tree, first, split.first, metrics, panels, drag, consumed);
	drag_split_ratios(tree, second, split.second, metrics, panels, drag, consumed);
}

auto gse::ide::update_dividers(dock_tree& tree, const rectf& frame, const dock_metrics& metrics, const std::span<const panel_desc> panels, const gui::layout::split_drag& drag) -> dock_layout {
	if (!find_leaf(tree, tree.maximized).exists()) {
		bool consumed = false;
		drag_split_ratios(tree, tree.root, frame, metrics, panels, drag, consumed);
	}
	return resolve(tree, frame, metrics, panels);
}

auto gse::ide::dragging_axis(const dock_tree& tree) -> std::optional<gui::layout::split_axis> {
	const auto it = std::ranges::find_if(tree.nodes.items(), [](const dock_node& node) {
		return node.divider_drag.dragging;
	});
	if (it == tree.nodes.items().end()) {
		return std::nullopt;
	}
	return it->axis;
}

auto gse::ide::divider_at(const dock_layout& layout, const vec2f mouse) -> const dock_divider* {
	const auto it = std::ranges::find_if(layout.dividers, [mouse](const dock_divider& divider) {
		return divider.rect.contains(mouse);
	});
	return it == layout.dividers.end() ? nullptr : &*it;
}

auto gse::ide::drop_target(const dock_tree& tree, const dock_layout& layout, const dock_metrics& metrics, const id panel, const vec2f mouse) -> std::optional<dock_drop> {
	const auto hit = std::ranges::find_if(layout.leaves, [mouse](const dock_placement& leaf) {
		return leaf.rect.contains(mouse);
	});
	if (hit == layout.leaves.end()) {
		return std::nullopt;
	}

	const id source = find_leaf(tree, panel);
	const bool own_leaf = hit->node == source;
	if (own_leaf && tree.nodes.try_get(source)->panels.size() == 1) {
		return std::nullopt;
	}

	dock_drop drop{
		.node = hit->node,
		.space = gui::layout::dock_space(hit->rect, metrics.scale),
	};

	if (own_leaf) {
		for (gui::dock::area& area : drop.space.areas) {
			if (area.dock_location == gui::dock::location::center) {
				area.rect = {};
				break;
			}
		}
	}

	for (const gui::dock::area& area : drop.space.areas) {
		if (area.rect.width() > 0.f && area.rect.height() > 0.f && area.rect.contains(mouse)) {
			drop.location = area.dock_location;
			break;
		}
	}
	return drop;
}

auto gse::ide::slot_of_node(const std::unordered_map<id, std::size_t>& index_of, const id node) -> std::string {
	const auto it = index_of.find(node);
	return it == index_of.end() ? std::string() : std::to_string(it->second);
}

auto gse::ide::node_at_slot(const std::unordered_map<std::size_t, id>& id_of, const std::string& value) -> id {
	std::size_t slot = 0;
	if (std::from_chars(value.data(), value.data() + value.size(), slot).ec != std::errc{}) {
		return {};
	}
	const auto it = id_of.find(slot);
	return it == id_of.end() ? id{} : it->second;
}

auto gse::ide::serialize_tree(const dock_tree& tree, const std::span<const panel_desc> panels) -> std::string {
	std::vector<id> order;
	collect_nodes(tree, tree.root, order);

	std::unordered_map<id, std::size_t> index_of;
	for (std::size_t i = 0; i < order.size(); ++i) {
		index_of.emplace(order[i], i);
	}

	std::string known;
	for (const panel_desc& desc : panels) {
		if (!known.empty()) {
			known.push_back(',');
		}
		known.append(desc.name);
	}

	std::string out;
	out.append("[dock]\n");
	out.append(std::format("root = {}\n", slot_of_node(index_of, tree.root)));
	out.append(std::format("maximized = {}\n", tree.maximized.exists() ? tree.maximized.tag() : std::string_view()));
	out.append(std::format("known = {}\n", known));

	for (std::size_t i = 0; i < order.size(); ++i) {
		const dock_node& node = *tree.nodes.try_get(order[i]);
		out.push_back('\n');
		out.append(std::format("[{}{}]\n", dock_node_section_prefix, i));
		if (is_leaf(node)) {
			std::string names;
			for (const id panel : node.panels) {
				if (!names.empty()) {
					names.push_back(',');
				}
				names.append(panel.tag());
			}
			out.append("kind = leaf\n");
			out.append(std::format("panels = {}\n", names));
			out.append(std::format("active = {}\n", node.active_panel));
		}
		else {
			out.append("kind = split\n");
			out.append(std::format("axis = {}\n", enum_to_string(node.axis)));
			out.append(std::format("ratio = {}\n", node.ratio));
			out.append(std::format("first = {}\n", slot_of_node(index_of, node.first)));
			out.append(std::format("second = {}\n", slot_of_node(index_of, node.second)));
		}
	}

	return out;
}

auto gse::ide::deserialize_tree(const std::span<const layout_store::section> sections, const std::span<const panel_desc> panels) -> std::optional<dock_tree> {
	std::map<std::size_t, const layout_store::section*> raw;
	const layout_store::section* header = nullptr;

	for (const layout_store::section& section : sections) {
		if (section.name == "dock") {
			header = &section;
			continue;
		}
		if (!section.name.starts_with(dock_node_section_prefix)) {
			continue;
		}
		const std::string_view digits = std::string_view(section.name).substr(dock_node_section_prefix.size());
		std::size_t slot = 0;
		if (std::from_chars(digits.data(), digits.data() + digits.size(), slot).ec != std::errc{}) {
			continue;
		}
		raw.emplace(slot, &section);
	}

	if (!header || raw.empty()) {
		return std::nullopt;
	}

	dock_tree tree;
	std::unordered_map<std::size_t, id> id_of;
	for (const std::size_t slot : std::views::keys(raw)) {
		id_of.emplace(slot, make_node(tree));
	}

	std::size_t restored_panels = 0;
	for (const auto& [slot, section] : raw) {
		dock_node& node = *tree.nodes.try_get(id_of.at(slot));
		const auto kind = section->values.find("kind");
		if (kind == section->values.end()) {
			return std::nullopt;
		}

		if (kind->second == "leaf") {
			if (const auto it = section->values.find("panels"); it != section->values.end()) {
				for (const auto& part : std::views::split(std::string_view(it->second), ',')) {
					const auto name = std::string_view(part);
					const auto desc = std::ranges::find(panels, name, &panel_desc::name);
					if (desc == panels.end() || std::ranges::find(node.panels, desc->id) != node.panels.end()) {
						continue;
					}
					node.panels.push_back(desc->id);
					++restored_panels;
				}
			}
			if (const auto it = section->values.find("active"); it != section->values.end()) {
				std::uint32_t active = 0;
				if (std::from_chars(it->second.data(), it->second.data() + it->second.size(), active).ec == std::errc{}) {
					node.active_panel = active;
				}
			}
			if (node.active_panel >= node.panels.size()) {
				node.active_panel = node.panels.empty() ? 0 : static_cast<std::uint32_t>(node.panels.size() - 1);
			}
			continue;
		}

		if (const auto it = section->values.find("axis"); it != section->values.end()) {
			enum_from_string(it->second, node.axis);
		}
		if (const auto it = section->values.find("ratio"); it != section->values.end()) {
			float ratio = 0.5f;
			if (std::from_chars(it->second.data(), it->second.data() + it->second.size(), ratio).ec == std::errc{}) {
				node.ratio = std::clamp(ratio, 0.05f, 0.95f);
			}
		}
		if (const auto it = section->values.find("first"); it != section->values.end()) {
			node.first = node_at_slot(id_of, it->second);
		}
		if (const auto it = section->values.find("second"); it != section->values.end()) {
			node.second = node_at_slot(id_of, it->second);
		}
		if (!node.first.exists() || !node.second.exists()) {
			return std::nullopt;
		}
		tree.nodes.try_get(node.first)->parent = id_of.at(slot);
		tree.nodes.try_get(node.second)->parent = id_of.at(slot);
	}

	if (const auto it = header->values.find("root"); it != header->values.end()) {
		tree.root = node_at_slot(id_of, it->second);
	}
	if (!tree.root.exists() || restored_panels == 0) {
		return std::nullopt;
	}

	prune_empty_leaves(tree);
	if (!tree.root.exists()) {
		return std::nullopt;
	}

	std::vector<std::string_view> known;
	if (const auto it = header->values.find("known"); it != header->values.end()) {
		for (const auto& part : std::views::split(std::string_view(it->second), ',')) {
			known.push_back(std::string_view(part));
		}
	}

	for (const panel_desc& desc : panels) {
		const bool was_known = std::ranges::find(known, desc.name) != known.end();
		if (!contains_panel(tree, desc.id) && !was_known && !desc.start_hidden) {
			insert_panel(tree, {
				.panel = desc.id,
				.target = any_leaf(tree),
				.location = gui::dock::location::center,
			});
		}
	}

	if (const auto it = header->values.find("maximized"); it != header->values.end() && !it->second.empty()) {
		if (const auto desc = std::ranges::find(panels, it->second, &panel_desc::name); desc != panels.end() && contains_panel(tree, desc->id)) {
			tree.maximized = desc->id;
		}
	}

	return tree;
}
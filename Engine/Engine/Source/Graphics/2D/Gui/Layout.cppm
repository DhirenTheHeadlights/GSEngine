export module gse.graphics:layout;

import std;

import gse.utility;

import :types;

export namespace gse::gui::layout {
	auto dock(
		id_mapped_collection<menu>& menus,
		id child_id,
		id parent_id,
		dock::location location
	) -> void;

	auto undock(
		id_mapped_collection<menu>& menus,
		id child_id
	) -> void;

	auto update(
		id_mapped_collection<menu>& menus,
		id root_id
	) -> void;

	auto dock_space(
		const ui_rect& target_area
	) -> dock::space;
}

namespace gse::gui::layout {
	auto dock_target_rect(
		const ui_rect& parent,
		dock::location location,
		float ratio
	) -> ui_rect;

	auto remaining_rect_for_parent(
		const ui_rect& parent,
		dock::location child_location,
		float ratio
	) -> ui_rect;
}

auto gse::gui::layout::dock(id_mapped_collection<menu>& menus, const id child_id, const id parent_id, const dock::location location) -> void {
	menu* parent = menus.try_get(parent_id);
	menu* child = menus.try_get(child_id);

	if (!parent || !child || location == dock::location::none || location == dock::location::center) {
		return;
	}

	const ui_rect parent_original_rect = parent->rect;

	parent->rect = remaining_rect_for_parent(parent_original_rect, location, 0.5f);
	child->rect = dock_target_rect(parent_original_rect, location, 0.5f);

	child->swap_parent(*parent);
	child->docked_to = location;
	child->dock_split_ratio = 0.5f;
}

auto gse::gui::layout::undock(id_mapped_collection<menu>& menus, const id child_id) -> void {
	menu* child = menus.try_get(child_id);

	if (!child || child->docked_to == dock::location::none) {
		return;
	}

	const id parent_id = child->owner_id();
	std::optional<ui_rect> combined_rect;
	if (parent_id.exists()) {
		if (const menu* parent = menus.try_get(parent_id)) {
			combined_rect = ui_rect::bounding_box(parent->rect, child->rect);
		}
	}

	child->swap_parent(id());
	child->docked_to = dock::location::none;

	if (parent_id.exists() && combined_rect) {
		if (menu* parent = menus.try_get(parent_id)) {
			parent->rect = *combined_rect;
			update(menus, parent_id);
		}
	}

	auto calculate_group_bounds = [](id_mapped_collection<menu>& input_menus, const id root_id) -> ui_rect {
		const menu* root = input_menus.try_get(root_id);

		if (!root) {
			return {};
		}
		ui_rect bounds = root->rect;

		std::function<void(id)> expand = [&](
			const id parent
			) {
				for (const auto& item : input_menus.items()) {
					if (item.owner_id() == parent) {
						bounds = ui_rect::bounding_box(bounds, item.rect);
						expand(item.id());
					}
				}
			};

		expand(root_id);
		return bounds;
	};

	const ui_rect group_bounds = calculate_group_bounds(menus, child_id);
	if (group_bounds.width() <= 0.f || group_bounds.height() <= 0.f) {
		return; 
	}

	constexpr unitless::vec2 min_menu_size = { 200.f, 200.f };

	const unitless::vec2 target_size = min_menu_size * 1.5f;
	const float scale_x = target_size.x() / group_bounds.width();
	const float scale_y = target_size.y() / group_bounds.height();

	std::function<void(id)> scale_group = [&](const id current_id) {
		if (menu* item = menus.try_get(current_id)) {
			unitless::vec2 offset = item->rect.top_left() - group_bounds.top_left();
			offset.x() *= scale_x;
			offset.y() *= scale_y;
			
			unitless::vec2 new_size = item->rect.size();
			new_size.x() *= scale_x;
			new_size.y() *= scale_y;

			item->rect = ui_rect::from_position_size(group_bounds.top_left() + offset, new_size);

			for (auto& potential_child : menus.items()) {
				if (potential_child.owner_id() == current_id) {
					scale_group(potential_child.id());
				}
			}
		}
	};
	
	scale_group(child_id);
}

auto gse::gui::layout::update(id_mapped_collection<menu>& menus, const id root_id) -> void {
	menu* root = menus.try_get(root_id);
	if (!root) {
		return;
	}

	menu* child = nullptr;
	for (auto& potential_child : menus.items()) {
		if (potential_child.owner_id() == root_id) {
			if (potential_child.docked_to != dock::location::none && potential_child.docked_to != dock::location::center) {
				child = &potential_child;
				break;
			}
		}
	}

	if (!child) {
		return;
	}

	const float ratio = std::clamp(child->dock_split_ratio, 0.05f, 0.95f);
	const ui_rect total_area = root->rect;

	child->rect = dock_target_rect(total_area, child->docked_to, ratio);
	root->rect = remaining_rect_for_parent(total_area, child->docked_to, ratio);

	update(menus, child->id());
}

auto gse::gui::layout::dock_space(const ui_rect& target_area) -> dock::space {
	dock::space space;

	const auto center = target_area.center();
	const unitless::vec2 widget_size = {
		std::min(50.f, target_area.width() * 0.2f),
		std::min(50.f, target_area.height() * 0.2f)
	};

	const float half_widget_w = widget_size.x() / 2.f;
	const float half_widget_h = widget_size.y() / 2.f;

	const unitless::vec2 center_pos = { center.x() - half_widget_w, center.y() + half_widget_h };
	const unitless::vec2 top_pos = { center.x() - half_widget_w, center.y() + widget_size.y() * 1.5f };
	const unitless::vec2 bottom_pos = { center.x() - half_widget_w, center.y() - widget_size.y() * 0.5f };
	const unitless::vec2 left_pos = { center.x() - widget_size.x() * 1.5f, center.y() + half_widget_h };
	const unitless::vec2 right_pos = { center.x() + widget_size.x() * 0.5f, center.y() + half_widget_h };

	// Center
	space.areas[0] = {
		.rect = ui_rect::from_position_size(center_pos, widget_size),
		.target = target_area,
		.dock_location = dock::location::center
	};

	// Left
	space.areas[1] = {
		.rect = ui_rect::from_position_size(left_pos, widget_size),
		.target = dock_target_rect(target_area, dock::location::left, 0.5f),
		.dock_location = dock::location::left
	};

	// Right
	space.areas[2] = {
		.rect = ui_rect::from_position_size(right_pos, widget_size),
		.target = dock_target_rect(target_area, dock::location::right, 0.5f),
		.dock_location = dock::location::right
	};

	// Top
	space.areas[3] = {
		.rect = ui_rect::from_position_size(top_pos, widget_size),
		.target = dock_target_rect(target_area, dock::location::top, 0.5f),
		.dock_location = dock::location::top
	};

	// Bottom
	space.areas[4] = {
		.rect = ui_rect::from_position_size(bottom_pos, widget_size),
		.target = dock_target_rect(target_area, dock::location::bottom, 0.5f),
		.dock_location = dock::location::bottom
	};

	return space;
}

auto gse::gui::layout::dock_target_rect(const ui_rect& parent, const dock::location location, const float ratio) -> ui_rect {
	const float split_width = parent.width() * ratio;
    const float split_height = parent.height() * ratio;
    const float remaining_width = parent.width() - split_width;
    const float remaining_height = parent.height() - split_height;

	switch (location) {
		case dock::location::left:   return ui_rect::from_position_size(parent.top_left(), { split_width, parent.height() });
		case dock::location::right:  return ui_rect::from_position_size({ parent.left() + remaining_width, parent.top() }, { split_width, parent.height() });
		case dock::location::top:    return ui_rect::from_position_size({ parent.left(), parent.top() }, { parent.width(), split_height });
		case dock::location::bottom: return ui_rect::from_position_size({ parent.left(), parent.top() - remaining_height }, { parent.width(), split_height });
		case dock::location::center:
		default:                     return parent;
	}
}

auto gse::gui::layout::remaining_rect_for_parent(const ui_rect& parent, const dock::location child_location, const float ratio) -> ui_rect {
	const float split_width = parent.width() * ratio;
    const float split_height = parent.height() * ratio;
    const float remaining_width = parent.width() - split_width;
    const float remaining_height = parent.height() - split_height;

	switch (child_location) {
		case dock::location::left:   return ui_rect::from_position_size({ parent.left() + split_width, parent.top() }, { remaining_width, parent.height() });
		case dock::location::right:  return ui_rect::from_position_size(parent.top_left(), { remaining_width, parent.height() });
		case dock::location::top:    return ui_rect::from_position_size({ parent.left(), parent.top() - split_height }, { parent.width(), remaining_height });
		case dock::location::bottom: return ui_rect::from_position_size(parent.top_left(), { parent.width(), remaining_height });
		case dock::location::center:
		default:                     return ui_rect::from_position_size(parent.top_left(), { 0.f, 0.f });
	}
}
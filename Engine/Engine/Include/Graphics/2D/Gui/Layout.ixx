export module gse.graphics:layout;

import std;

import gse.utility;

import :types;

export namespace gse::gui::layout {
	auto dock(
		id_mapped_collection<menu>& menus,
		const id& child_id,
		const id& parent_id,
		dock::location location
	) -> void;

	auto undock(
		id_mapped_collection<menu>& menus,
		const id& child_id
	) -> void;

	auto update(
		id_mapped_collection<menu>& menus,
		const id& root_id
	) -> void;

	auto dock_space(
		const ui_rect& target_area
	) -> dock::space;
}

namespace gse::gui::layout {
	auto dock_target_rect(
		const ui_rect& parent,
		dock::location location
	) -> ui_rect;

	auto remaining_rect_for_parent(
		const ui_rect& parent,
		dock::location child_location
	) -> ui_rect;
}

auto gse::gui::layout::dock(id_mapped_collection<menu>& menus, const id& child_id, const id& parent_id, const dock::location location) -> void {
	menu* parent = menus.try_get(parent_id);
	menu* child = menus.try_get(child_id);

	if (!parent || !child || location == dock::location::none || location == dock::location::center) {
		return;
	}

	if (child->docked_to == dock::location::none) {
		child->pre_docked_size = child->rect.size();
	}

	const ui_rect parent_original_rect = parent->rect;

	parent->rect = remaining_rect_for_parent(parent_original_rect, location);
	child->rect = dock_target_rect(parent_original_rect, location);

	child->swap(*parent);
	child->docked_to = location;
}

auto gse::gui::layout::undock(id_mapped_collection<menu>& menus, const id& child_id) -> void {
	menu* child = menus.try_get(child_id);

	if (!child || child->docked_to == dock::location::none) {
		return;
	}

	if (child->owner_id().exists()) {
		const id parent_id = child->owner_id();
		if (menu* parent = menus.try_get(parent_id)) {
			parent->rect = ui_rect::bounding_box(parent->rect, child->rect);
			update(menus, parent_id);
		}
	}

	child->rect = ui_rect::from_position_size(child->rect.top_left(), child->pre_docked_size);
	child->swap(id());
	child->docked_to = dock::location::none;
}

auto gse::gui::layout::update(id_mapped_collection<menu>& menus, const id& root_id) -> void {
	const menu* root = menus.try_get(root_id);

	if (!root) {
		return;
	}

	menu* child = nullptr;
	for (auto& potential_child : menus.items()) {
		if (potential_child.owner_id() == root_id) {
			child = &potential_child;
			break;
		}
	}

	if (child) {
		const ui_rect total_area = root->rect;

		child->rect = dock_target_rect(total_area, child->docked_to);

		if (menu* mutable_root = menus.try_get(root_id)) {
			mutable_root->rect = remaining_rect_for_parent(total_area, child->docked_to);
		}

		update(menus, child->id());
	}
}

auto gse::gui::layout::dock_space(const ui_rect& target_area) -> dock::space {
	dock::space space;

	const auto center = target_area.center();
	const unitless::vec2 widget_size = {
		std::min(50.f, target_area.width() * 0.2f),
		std::min(50.f, target_area.height() * 0.2f)
	};

	const float half_widget_w = widget_size.x / 2.f;
	const float half_widget_h = widget_size.y / 2.f;

	const unitless::vec2 center_pos = { center.x - half_widget_w, center.y + half_widget_h };
	const unitless::vec2 top_pos = { center.x - half_widget_w, center.y + widget_size.y * 1.5f };
	const unitless::vec2 bottom_pos = { center.x - half_widget_w, center.y - widget_size.y * 0.5f };
	const unitless::vec2 left_pos = { center.x - widget_size.x * 1.5f, center.y + half_widget_h };
	const unitless::vec2 right_pos = { center.x + widget_size.x * 0.5f, center.y + half_widget_h };

	// Center
	space.areas[0] = {
		.rect = ui_rect::from_position_size(center_pos, widget_size),
		.target = target_area,
		.dock_location = dock::location::center
	};

	// Left
	space.areas[1] = {
		.rect = ui_rect::from_position_size(left_pos, widget_size),
		.target = dock_target_rect(target_area, dock::location::left),
		.dock_location = dock::location::left
	};

	// Right
	space.areas[2] = {
		.rect = ui_rect::from_position_size(right_pos, widget_size),
		.target = dock_target_rect(target_area, dock::location::right),
		.dock_location = dock::location::right
	};

	// Top
	space.areas[3] = {
		.rect = ui_rect::from_position_size(top_pos, widget_size),
		.target = dock_target_rect(target_area, dock::location::top),
		.dock_location = dock::location::top
	};

	// Bottom
	space.areas[4] = {
		.rect = ui_rect::from_position_size(bottom_pos, widget_size),
		.target = dock_target_rect(target_area, dock::location::bottom),
		.dock_location = dock::location::bottom
	};

	return space;
}

auto gse::gui::layout::dock_target_rect(const ui_rect& parent, const dock::location location) -> ui_rect {
	const float half_width = parent.width() / 2.f;
	const float half_height = parent.height() / 2.f;

	switch (location) {
	case dock::location::left:   return ui_rect::from_position_size(parent.top_left(), { half_width, parent.height() });
	case dock::location::right:  return ui_rect::from_position_size({ parent.left() + half_width, parent.top() }, { half_width, parent.height() });
	case dock::location::top:    return ui_rect::from_position_size({ parent.left(), parent.top() }, { parent.width(), half_height });
	case dock::location::bottom: return ui_rect::from_position_size({ parent.left(), parent.top() - half_height }, { parent.width(), half_height });
	case dock::location::center:
	default:                     return parent;
	}
}

auto gse::gui::layout::remaining_rect_for_parent(const ui_rect& parent, const dock::location child_location) -> ui_rect {
	const float half_width = parent.width() / 2.f;
	const float half_height = parent.height() / 2.f;

	switch (child_location) {
	case dock::location::left:   return ui_rect::from_position_size({ parent.left() + half_width, parent.top() }, { half_width, parent.height() });
	case dock::location::right:  return ui_rect::from_position_size(parent.top_left(), { half_width, parent.height() });
	case dock::location::top:    return ui_rect::from_position_size({ parent.left(), parent.top() - half_height }, { parent.width(), half_height });
	case dock::location::bottom: return ui_rect::from_position_size(parent.top_left(), { parent.width(), half_height });
	case dock::location::center:
	default:                     return ui_rect::from_position_size(parent.top_left(), { 0.f, 0.f });
	}
}
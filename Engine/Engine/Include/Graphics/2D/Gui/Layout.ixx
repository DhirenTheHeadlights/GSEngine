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

auto gse::gui::layout::dock(id_mapped_collection<menu>& menus, const id& child_id, const id& parent_id, dock::location location) -> void {
	menu* parent = menus.try_get(parent_id);
	menu* child = menus.try_get(child_id);

	if (!parent || !child) {
		return;
	}

	const ui_rect parent_original_rect = parent->rect;

	parent->rect = remaining_rect_for_parent(parent_original_rect, location);
	child->rect = remaining_rect_for_parent(parent_original_rect, location);

	child->swap(*parent);
	child->docked_to = location;

	update(menus, parent_id);
}

auto gse::gui::layout::undock(id_mapped_collection<menu>& menus, const id& child_id) -> void {
	menu* child = menus.try_get(child_id);

	if (!child || !child->owner_id().exists()) {
		return;
	}

	const id parent_id = child->owner_id();

	child->swap(parent_id);
	child->docked_to = dock::location::none;
}

auto gse::gui::layout::update(id_mapped_collection<menu>& menus, const id& root_id) -> void {
	const menu* root = menus.try_get(root_id);

	if (!root) {
		return;
	}

	for (auto& potential_child : menus.items()) {
		if (potential_child.owner_id() == root_id) {
			potential_child.rect = dock_target_rect(root->rect, potential_child.docked_to);

			update(menus, potential_child.id());
		}
	}
}

auto gse::gui::layout::dock_space(const ui_rect& target_area) -> dock::space {
	dock::space space;

	const auto center = target_area.center();
	const unitless::vec2 widget_size = {
		std::min(50.f, target_area.width() * 0.2f),
		std::min(50.f, target_area.height() * 0.2f)
	};

	const float half_width = target_area.width() / 2.f;
	const float half_height = target_area.height() / 2.f;

	// Center
	space.areas[0] = {
		.rect = ui_rect::from_position_size({ center.x - widget_size.x / 2.f, center.y - widget_size.y / 2.f }, widget_size),
		.target = target_area,
		.dock_location = dock::location::center
	};

	// Left
	space.areas[1] = {
		.rect = ui_rect::from_position_size({ center.x - half_width * 0.5f - widget_size.x / 2.f, center.y - widget_size.y / 2.f }, widget_size),
		.target = dock_target_rect(target_area, dock::location::left),
		.dock_location = dock::location::left
	};

	// Right
	space.areas[2] = {
		.rect = ui_rect::from_position_size({ center.x + half_width * 0.5f - widget_size.x / 2.f, center.y - widget_size.y / 2.f }, widget_size),
		.target = dock_target_rect(target_area, dock::location::right),
		.dock_location = dock::location::right
	};

	// Top
	space.areas[3] = {
		.rect = ui_rect::from_position_size({ center.x - widget_size.x / 2.f, center.y + half_height * 0.5f - widget_size.y / 2.f }, widget_size),
		.target = dock_target_rect(target_area, dock::location::top),
		.dock_location = dock::location::top
	};

	// Bottom
	space.areas[4] = {
		.rect = ui_rect::from_position_size({ center.x - widget_size.x / 2.f, center.y - half_height * 0.5f - widget_size.y / 2.f }, widget_size),
		.target = dock_target_rect(target_area, dock::location::bottom),
		.dock_location = dock::location::bottom
	};

	return space;
}

auto gse::gui::layout::dock_target_rect(const ui_rect& parent, const dock::location location) -> ui_rect {
	const float half_width  = parent.width() / 2.f;
	const float half_height = parent.height() / 2.f;

	switch (location) {
		case dock::location::left:   return ui_rect::from_position_size(  parent.top_left(),							{ half_width, parent.height() });
		case dock::location::right:  return ui_rect::from_position_size({ parent.left() + half_width, parent.top() },	{ half_width, parent.height() });
		case dock::location::top:    return ui_rect::from_position_size(  parent.top_left(),							{ parent.width(), half_height });
		case dock::location::bottom: return ui_rect::from_position_size({ parent.left(), parent.top() - half_height },	{ parent.width(), half_height });
		case dock::location::center:
		default:                     return parent;
	}
}

auto gse::gui::layout::remaining_rect_for_parent(const ui_rect& parent, dock::location child_location) -> ui_rect {
	const float half_width  = parent.width() / 2.f;
	const float half_height = parent.height() / 2.f;

	switch (child_location) {
	case dock::location::left:   return ui_rect::from_position_size({ parent.left() + half_width, parent.top() },	{ half_width, parent.height() });
	case dock::location::right:  return ui_rect::from_position_size(  parent.top_left(),							{ half_width, parent.height() });
	case dock::location::top:    return ui_rect::from_position_size({ parent.left(), parent.top() - half_height },	{ parent.width(), half_height });
	case dock::location::bottom: return ui_rect::from_position_size(  parent.top_left(),							{ parent.width(), half_height });
	case dock::location::center:
	default:                     return ui_rect::from_position_size(  parent.top_left(),							{ 0, 0 }); 
	}
}






module gse.graphics:gui_drag_resize_impl;

import std;

import gse.os;
import gse.config;
import gse.assets;
import gse.gpu;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.math;
import gse.meta;
import gse.save;

import :gui;
import :gui_drag_resize;
import :gui_chrome;

import :types;
import :layout;
import :font;
import :ui_renderer;
import :texture;
import :cursor;
import :save;
import :ids;
import :input_layers;
import :settings;
import :styles;
import :builder;
import :menu_stack;
import :render_layer;
import :interaction;
import :symbols;
import :tab_strip;
import :widget_context;

auto gse::gui::handle_idle_state(const font_set& fonts, viewport_state& vp, const gse::input::state& input_state, vec2f mouse_position, const bool mouse_held, const style& style) -> gui::state {
	if (vp.menu_stack.captures_input() || vp.active_widget_id.exists()) {
		set_style(cursor::style::arrow);
		return states::idle{};
	}

	struct interaction_candidate {
		std::variant<states::resizing, states::dragging, states::resizing_divider, states::pending_drag> future_state;
		cursor::style cursor;
	};

	struct resize_rule {
		std::function<bool(const rectf&, const vec2f&)> condition;
		resize_handle handle;
		cursor::style cursor;
	};

	const std::array<resize_rule, 8> resize_rules = { {
		{ [style](
		const rectf& r,
		const vec2f& p
	) {
			 const float t = style.resize_border_thickness;
			 return std::abs(p.y() - r.top()) < t && std::abs(p.x() - r.left()) < t;
		 },
		  resize_handle::top_left,
		  cursor::style::resize_nw },
		{ [style](
		const rectf& r,
		const vec2f& p
	) {
			 const float t = style.resize_border_thickness;
			 return std::abs(p.y() - r.top()) < t && std::abs(p.x() - r.right()) < t;
		 },
		  resize_handle::top_right,
		  cursor::style::resize_ne },
		{ [style](
		const rectf& r,
		const vec2f& p
	) {
			 const float t = style.resize_border_thickness;
			 return std::abs(p.y() - r.bottom()) < t && std::abs(p.x() - r.left()) < t;
		 },
		  resize_handle::bottom_left,
		  cursor::style::resize_sw },
		{ [style](
		const rectf& r,
		const vec2f& p
	) {
			 const float t = style.resize_border_thickness;
			 return std::abs(p.y() - r.bottom()) < t && std::abs(p.x() - r.right()) < t;
		 },
		  resize_handle::bottom_right,
		  cursor::style::resize_se },
		{ [style](
		const rectf& r,
		const vec2f& p
	) {
			 const float t = style.resize_border_thickness;
			 return std::abs(p.x() - r.left()) < t && p.y() <= r.top() + t && p.y() >= r.bottom() - t;
		 },
		  resize_handle::left,
		  cursor::style::resize_w },
		{ [style](
		const rectf& r,
		const vec2f& p
	) {
			 const float t = style.resize_border_thickness;
			 return std::abs(p.x() - r.right()) < t && p.y() <= r.top() + t && p.y() >= r.bottom() - t;
		 },
		  resize_handle::right,
		  cursor::style::resize_e },
		{ [style](
		const rectf& r,
		const vec2f& p
	) {
			 const float t = style.resize_border_thickness;
			 return std::abs(p.y() - r.top()) < t && p.x() >= r.left() - t && p.x() <= r.right() + t;
		 },
		  resize_handle::top,
		  cursor::style::resize_n },
		{ [style](
		const rectf& r,
		const vec2f& p
	) {
			 const float t = style.resize_border_thickness;
			 return std::abs(p.y() - r.bottom()) < t && p.x() >= r.left() - t && p.x() <= r.right() + t;
		 },
		  resize_handle::bottom,
		  cursor::style::resize_s },
	} };

	auto calculate_group_bounds = [&vp](const id root_id) -> rectf {
		const menu* root = vp.menus.try_get(root_id);
		if (!root) {
			return {};
		}

		rectf bounds = root->rect;

		auto expand = [&](this auto& self, const id parent_id) -> void {
			for (const menu& item : vp.menus.items()) {
				if (item.owner_id() == parent_id && item.was_visible_last_frame) {
					bounds = rectf::bounding_box(bounds, item.rect);
					self(item.id());
				}
			}
		};

		expand(root_id);
		return bounds;
	};

	std::vector<menu*> visible_menus;
	visible_menus.reserve(vp.visible_menu_ids_last_frame.size());
	for (const id& mid : vp.visible_menu_ids_last_frame) {
		if (menu* m = vp.menus.try_get(mid)) {
			visible_menus.push_back(m);
		}
	}

	auto hot_item = [&]() -> std::optional<interaction_candidate> {
		const bool resize_blocked = vp.input_layers_data.is_resize_blocked(mouse_position);
		for (auto it = visible_menus.rbegin(); it != visible_menus.rend(); ++it) {
			menu& current_menu = **it;

			if (current_menu.fixed) {
				continue;
			}

			if (!current_menu.owner_id().exists()) {
				if (current_menu.docked_to == dock::location::none) {
					const rectf group_rect = calculate_group_bounds(current_menu.id());

					for (const auto& [condition, handle, cursor] : resize_rules) {
						if (condition(group_rect, mouse_position) && !resize_blocked) {
							return interaction_candidate{
								.future_state =
									states::resizing{
										.menu_id = current_menu.id(),
										.handle = handle
									},
								.cursor = cursor
							};
						}
					}
				}
				else {
					const rectf& rect = current_menu.rect;

					switch (current_menu.docked_to) {
						case dock::location::left:
							if (std::abs(mouse_position.x() - rect.right()) < style.resize_border_thickness && !resize_blocked) {
								return interaction_candidate{ states::resizing{ current_menu.id(),
																				resize_handle::right },
															  cursor::style::resize_e };
							}
							break;
						case dock::location::right:
							if (std::abs(mouse_position.x() - rect.left()) < style.resize_border_thickness && !resize_blocked) {
								return interaction_candidate{ states::resizing{ current_menu.id(),
																				resize_handle::left },
															  cursor::style::resize_w };
							}
							break;
						case dock::location::top:
							if (std::abs(mouse_position.y() - rect.bottom()) < style.resize_border_thickness && !resize_blocked) {
								return interaction_candidate{ states::resizing{ current_menu.id(),
																				resize_handle::bottom },
															  cursor::style::resize_s };
							}
							break;
						case dock::location::bottom:
							if (std::abs(mouse_position.y() - rect.top()) < style.resize_border_thickness && !resize_blocked) {
								return interaction_candidate{ states::resizing{ current_menu.id(), resize_handle::top },
															  cursor::style::resize_n };
							}
							break;
						default:
							break;
					}
				}
			}
			else {
				if (const menu* parent = vp.menus.try_get(current_menu.owner_id())) {
					bool hovering = false;
					auto new_cursor = cursor::style::arrow;
					const rectf& r = current_menu.rect;

					switch (current_menu.docked_to) {
						case dock::location::left:
							if (std::abs(mouse_position.x() - r.right()) < style.resize_border_thickness && mouse_position.y() < r.top() && mouse_position.y() > r.bottom() && !resize_blocked) {
								hovering = true;
								new_cursor = cursor::style::resize_e;
							}
							break;
						case dock::location::right:
							if (std::abs(mouse_position.x() - r.left()) < style.resize_border_thickness && mouse_position.y() < r.top() && mouse_position.y() > r.bottom() && !resize_blocked) {
								hovering = true;
								new_cursor = cursor::style::resize_w;
							}
							break;
						case dock::location::top:
							if (std::abs(mouse_position.y() - r.bottom()) < style.resize_border_thickness && mouse_position.x() > r.left() && mouse_position.x() < r.right() && !resize_blocked) {
								hovering = true;
								new_cursor = cursor::style::resize_s;
							}
							break;
						case dock::location::bottom:
							if (std::abs(mouse_position.y() - r.top()) < style.resize_border_thickness && mouse_position.x() > r.left() && mouse_position.x() < r.right() && !resize_blocked) {
								hovering = true;
								new_cursor = cursor::style::resize_n;
							}
							break;
						default:
							break;
					}

					if (hovering) {
						return interaction_candidate{
							.future_state =
								states::resizing_divider{
									.parent_id = parent->id(),
									.child_id = current_menu.id()
								},
							.cursor = new_cursor
						};
					}
				}
			}

			const rectf title_bar_rect = rectf::from_position_size(
				{ current_menu.rect.left(), current_menu.rect.top() },
				{ current_menu.rect.width(), menu_chrome_height(fonts, current_menu, vp.fstate.sty, current_menu.rect.width()) }
			);

			if (title_bar_rect.contains(mouse_position)) {
				if (is_popout_menu_tag(current_menu.id().tag())) {
					const rectf close_rect = popout_close_button_rect(title_bar_rect, style);
					if (close_rect.contains(mouse_position)) {
						return std::nullopt;
					}
				}

				const std::optional<std::uint32_t> clicked_tab = tab_index_at(fonts, current_menu, vp.fstate.sty, title_bar_rect, mouse_position);

				return interaction_candidate{
					.future_state =
						states::pending_drag{
							.menu_id = current_menu.id(),
							.start_position = mouse_position,
							.offset = current_menu.rect.top_left() - mouse_position,
							.tab_index = clicked_tab
						},
					.cursor = cursor::style::arrow
				};
			}
		}

		return std::nullopt;
	}();

	if (hot_item) {
		set_style(hot_item->cursor);

		if (mouse_held) {
			if (std::holds_alternative<states::dragging>(hot_item->future_state)) {
				const auto& [menu_id, offset] = std::get<states::dragging>(hot_item->future_state);
				if (const menu* m = vp.menus.try_get(menu_id); m && m->docked_to != dock::location::none) {
					layout::undock(vp.menus, m->id());
				}
			}

			return std::visit(
				[](auto&& arg) -> gui::state {
					return arg;
				},
				hot_item->future_state
			);
		}
	}
	else {
		set_style(cursor::style::arrow);
	}

	return states::idle{};
}

auto gse::gui::handle_dragging_state(viewport_state& vp, const states::dragging& current, const shared_view<window::data> window_s, const vec2f mouse_position, const bool mouse_held) -> gui::state {
	menu* m = vp.menus.try_get(current.menu_id);
	if (!m) {
		set_style(cursor::style::arrow);
		return states::idle{};
	}

	std::vector<menu*> visible_menus;
	visible_menus.reserve(vp.visible_menu_ids_last_frame.size());
	for (const id& mid : vp.visible_menu_ids_last_frame) {
		if (menu* vm = vp.menus.try_get(mid)) {
			visible_menus.push_back(vm);
		}
	}

	if (!mouse_held) {
		if (vp.active_dock_space) {
			id potential_dock_parent_id;

			for (auto it = visible_menus.rbegin(); it != visible_menus.rend(); ++it) {
				if (const menu& other_menu = **it; other_menu.id() != current.menu_id && other_menu.rect.contains(mouse_position)) {
					potential_dock_parent_id = other_menu.id();
					break;
				}
			}

			for (const dock::area& area : vp.active_dock_space->areas) {
				if (area.rect.contains(mouse_position)) {
					if (potential_dock_parent_id.exists()) {
						if (area.dock_location == dock::location::center) {
							if (menu* parent = vp.menus.try_get(potential_dock_parent_id)) {
								parent->tab_contents.insert(
									parent->tab_contents.end(),
									std::make_move_iterator(m->tab_contents.begin()),
									std::make_move_iterator(m->tab_contents.end())
								);
								m->tab_contents.clear();
								parent->active_tab_index = static_cast<std::uint32_t>(parent->tab_contents.size() - 1);
								vp.menus.remove(current.menu_id);
							}
						}
						else {
							layout::dock(vp.menus, current.menu_id, potential_dock_parent_id, area.dock_location);
							layout::update(vp.menus, m->id());
						}
					}
					else {
						const rectf screen_rect = vp.rect;

						if (area.dock_location == dock::location::center) {
							m->rect = screen_rect;
							m->docked_to = dock::location::center;
							m->swap_parent(id());
							layout::update(vp.menus, m->id());
						}
						else {
							m->rect = layout::dock_target_rect(screen_rect, area.dock_location, 0.5f);
							m->docked_to = area.dock_location;
							m->swap_parent(id());
							layout::update(vp.menus, m->id());
						}
					}

					break;
				}
			}
		}

		vp.active_dock_space.reset();
		set_style(cursor::style::arrow);
		return states::idle{};
	}

	set_style(cursor::style::omni_move);

	const rectf screen_rect = vp.rect;
	const vec2f old_top_left = m->rect.top_left();
	vec2f new_top_left = mouse_position + current.offset;

	const float max_x = std::max(0.f, screen_rect.width() - m->rect.width());
	new_top_left.x() = std::clamp(new_top_left.x(), 0.f, max_x);

	const float min_y = std::min(m->rect.height(), screen_rect.top());
	new_top_left.y() = std::clamp(new_top_left.y(), min_y, screen_rect.top());

	if (const vec2f delta = new_top_left - old_top_left; delta.x() != 0 || delta.y() != 0) {
		auto move_group = [&](this auto& self, const id current_id) -> void {
			if (menu* item = vp.menus.try_get(current_id)) {
				item->rect = rectf::from_position_size(item->rect.top_left() + delta, item->rect.size());

				for (menu& potential_child : vp.menus.items()) {
					if (potential_child.owner_id() == current_id) {
						self(potential_child.id());
					}
				}
			}
		};

		move_group(current.menu_id);
	}

	vp.active_dock_space.reset();
	bool found_parent_menu = false;

	for (auto it = visible_menus.rbegin(); it != visible_menus.rend(); ++it) {
		menu& other_menu = **it;

		if (other_menu.id() == current.menu_id) {
			continue;
		}

		if (other_menu.rect.contains(mouse_position)) {
			vp.active_dock_space = layout::dock_space(other_menu.rect, vp.fstate.sty.scale_factor);
			found_parent_menu = true;
			break;
		}
	}

	if (!found_parent_menu) {
		vp.active_dock_space = layout::dock_space(screen_rect, vp.fstate.sty.scale_factor);
	}

	return current;
}

auto gse::gui::handle_resizing_state(viewport_state& vp, const states::resizing& current, const vec2f mouse_position, const bool mouse_held, const style& style, const shared_view<window::data> window_s) -> gui::state {
	if (!mouse_held) {
		vp.active_dock_space.reset();
		set_style(cursor::style::arrow);
		return states::idle{};
	}

	menu* m = vp.menus.try_get(current.menu_id);
	if (!m) {
		set_style(cursor::style::arrow);
		return states::idle{};
	}

	auto handle_to_cursor = [](const resize_handle h) -> cursor::style {
		switch (h) {
			case resize_handle::top_left:
				return cursor::style::resize_nw;
			case resize_handle::top_right:
				return cursor::style::resize_ne;
			case resize_handle::bottom_left:
				return cursor::style::resize_sw;
			case resize_handle::bottom_right:
				return cursor::style::resize_se;
			case resize_handle::left:
				return cursor::style::resize_w;
			case resize_handle::right:
				return cursor::style::resize_e;
			case resize_handle::top:
				return cursor::style::resize_n;
			case resize_handle::bottom:
				return cursor::style::resize_s;
			default:
				return cursor::style::arrow;
		}
	};

	set_style(handle_to_cursor(current.handle));

	auto calculate_group_bounds = [&vp](const id root_id) -> rectf {
		const menu* root = vp.menus.try_get(root_id);
		if (!root) {
			return {};
		}

		rectf bounds = root->rect;

		auto expand = [&](this auto& self, const id parent_id) -> void {
			for (const menu& item : vp.menus.items()) {
				if (item.owner_id() == parent_id && item.was_visible_last_frame) {
					bounds = rectf::bounding_box(bounds, item.rect);
					self(item.id());
				}
			}
		};

		expand(root_id);
		return bounds;
	};

	auto calculate_min_required_size = [&vp, &style](const id root_id) -> vec2f {
		auto rec = [&](this auto& self, const id node_id) -> vec2f {
			vec2f req = style.min_menu_size;

			for (const menu& child : vp.menus.items()) {
				if (child.owner_id() != node_id || !child.was_visible_last_frame) {
					continue;
				}

				const vec2f c = self(child.id());

				switch (child.docked_to) {
					case dock::location::left:
					case dock::location::right:
						req.x() += c.x();
						req.y() = std::max(req.y(), c.y());
						break;
					case dock::location::top:
					case dock::location::bottom:
						req.y() += c.y();
						req.x() = std::max(req.x(), c.x());
						break;
					default:
						req.x() = std::max(req.x(), c.x());
						req.y() = std::max(req.y(), c.y());
						break;
				}
			}

			return req;
		};

		return rec(root_id);
	};

	const rectf group_rect = calculate_group_bounds(m->id());
	vec2f min_corner = group_rect.min();
	vec2f max_corner = group_rect.max();

	const vec2f subtree_min = calculate_min_required_size(m->id());
	const float min_w = subtree_min.x();
	const float min_h = subtree_min.y();

	float opposing_left = 0.f;
	float opposing_right = std::numeric_limits<float>::max();
	float opposing_top = std::numeric_limits<float>::max();
	float opposing_bottom = 0.f;

	constexpr float dock_gap = 8.f;

	if (m->docked_to != dock::location::none) {
		for (const menu& other : vp.menus.items()) {
			if (other.id() == m->id()) {
				continue;
			}
			if (other.owner_id() != m->owner_id()) {
				continue;
			}
			if (other.docked_to == dock::location::none) {
				continue;
			}
			if (!other.was_visible_last_frame) {
				continue;
			}

			const rectf other_bounds = calculate_group_bounds(other.id());

			switch (other.docked_to) {
				case dock::location::left:
					opposing_left = std::max(opposing_left, other_bounds.right() + dock_gap);
					break;
				case dock::location::right:
					opposing_right = std::min(opposing_right, other_bounds.left() - dock_gap);
					break;
				case dock::location::top:
					opposing_top = std::min(opposing_top, other_bounds.bottom() - dock_gap);
					break;
				case dock::location::bottom:
					opposing_bottom = std::max(opposing_bottom, other_bounds.top() + dock_gap);
					break;
				default:
					break;
			}
		}
	}

	auto clamp_left = [&](const float x) -> float {
		float result = std::min(x, max_corner.x() - min_w);
		result = std::max(result, opposing_left);
		return result;
	};

	auto clamp_right = [&](const float x) -> float {
		float result = std::max(x, min_corner.x() + min_w);
		result = std::min(result, opposing_right);
		return result;
	};

	auto clamp_bottom = [&](const float y) -> float {
		float result = std::min(y, max_corner.y() - min_h);
		result = std::max(result, opposing_bottom);
		return result;
	};

	auto clamp_top = [&](const float y) -> float {
		float result = std::max(y, min_corner.y() + min_h);
		result = std::min(result, opposing_top);
		return result;
	};

	switch (current.handle) {
		case resize_handle::left:
			min_corner.x() = clamp_left(mouse_position.x());
			break;
		case resize_handle::right:
			max_corner.x() = clamp_right(mouse_position.x());
			break;
		case resize_handle::bottom:
			min_corner.y() = clamp_bottom(mouse_position.y());
			break;
		case resize_handle::top:
			max_corner.y() = clamp_top(mouse_position.y());
			break;
		case resize_handle::bottom_left:
			min_corner.x() = clamp_left(mouse_position.x());
			min_corner.y() = clamp_bottom(mouse_position.y());
			break;
		case resize_handle::bottom_right:
			min_corner.y() = clamp_bottom(mouse_position.y());
			max_corner.x() = clamp_right(mouse_position.x());
			break;
		case resize_handle::top_left:
			min_corner.x() = clamp_left(mouse_position.x());
			max_corner.y() = clamp_top(mouse_position.y());
			break;
		case resize_handle::top_right:
			max_corner.x() = clamp_right(mouse_position.x());
			max_corner.y() = clamp_top(mouse_position.y());
			break;
		default:
			break;
	}

	m->rect = rectf({
		.min = min_corner,
		.max = max_corner
	});

	if (!m->owner_id().exists()) {
		const rectf screen_rect = vp.rect;

		switch (m->docked_to) {
			case dock::location::left:
			case dock::location::right: {
				const float denom = screen_rect.width();
				const float ratio = denom > 0.f ? (m->rect.width() / denom) : 1.f;
				const float min_ratio = denom > 0.f ? std::min(1.f, min_w / denom) : 0.f;
				m->dock_split_ratio = std::clamp(ratio, min_ratio, 1.f);
				break;
			}
			case dock::location::top:
			case dock::location::bottom: {
				const float denom = screen_rect.height();
				const float ratio = denom > 0.f ? (m->rect.height() / denom) : 1.f;
				const float min_ratio = denom > 0.f ? std::min(1.f, min_h / denom) : 0.f;
				m->dock_split_ratio = std::clamp(ratio, min_ratio, 1.f);
				break;
			}
			default:
				break;
		}
	}

	layout::update(vp.menus, m->id());

	return current;
}

auto gse::gui::handle_resizing_divider_state(viewport_state& vp, const states::resizing_divider& current, const vec2f mouse_position, const bool mouse_held, const style& style) -> gui::state {
	menu* parent = vp.menus.try_get(current.parent_id);
	menu* child = vp.menus.try_get(current.child_id);

	if (!parent || !child) {
		set_style(cursor::style::arrow);
		return states::idle{};
	}

	if (!mouse_held) {
		set_style(cursor::style::arrow);
		return states::idle{};
	}

	const dock::location location = child->docked_to;

	auto location_to_cursor = [](const dock::location loc) -> cursor::style {
		switch (loc) {
			case dock::location::left:
				return cursor::style::resize_e;
			case dock::location::right:
				return cursor::style::resize_w;
			case dock::location::top:
				return cursor::style::resize_s;
			case dock::location::bottom:
				return cursor::style::resize_n;
			default:
				return cursor::style::arrow;
		}
	};

	set_style(location_to_cursor(location));

	const rectf combined_rect = rectf::bounding_box(parent->rect, child->rect);

	switch (location) {
		case dock::location::left:
		case dock::location::right: {
			if (combined_rect.width() < style.min_menu_size.x() * 2.f) {
				return current;
			}

			const float min_clamp = combined_rect.left() + style.min_menu_size.x();
			const float max_clamp = combined_rect.right() - style.min_menu_size.x();
			const float divider_x = std::clamp(mouse_position.x(), min_clamp, max_clamp);

			if (location == dock::location::left) {
				child->rect = rectf({
					.min = { combined_rect.left(), combined_rect.bottom() },
					.max = { divider_x, combined_rect.top() }
				});
				parent->rect = rectf({
					.min = { divider_x, combined_rect.bottom() },
					.max = { combined_rect.right(), combined_rect.top() }
				});
			}
			else {
				parent->rect = rectf({
					.min = { combined_rect.left(), combined_rect.bottom() },
					.max = { divider_x, combined_rect.top() }
				});
				child->rect = rectf({
					.min = { divider_x, combined_rect.bottom() },
					.max = { combined_rect.right(), combined_rect.top() }
				});
			}

			if (combined_rect.width() > 0.f) {
				const float child_width = (location == dock::location::left) ? (divider_x - combined_rect.left())
																			 : (combined_rect.right() - divider_x);
				child->dock_split_ratio = child_width / combined_rect.width();
			}
			break;
		}
		case dock::location::top:
		case dock::location::bottom: {
			if (combined_rect.height() < style.min_menu_size.y() * 2.f) {
				return current;
			}

			const float min_clamp = combined_rect.bottom() + style.min_menu_size.y();
			const float max_clamp = combined_rect.top() - style.min_menu_size.y();
			const float divider_y = std::clamp(mouse_position.y(), min_clamp, max_clamp);

			if (location == dock::location::top) {
				child->rect = rectf({
					.min = { combined_rect.left(), divider_y },
					.max = { combined_rect.right(), combined_rect.top() }
				});
				parent->rect = rectf({
					.min = { combined_rect.left(), combined_rect.bottom() },
					.max = { combined_rect.right(), divider_y }
				});
			}
			else {
				parent->rect = rectf({
					.min = { combined_rect.left(), divider_y },
					.max = { combined_rect.right(), combined_rect.top() }
				});
				child->rect = rectf({
					.min = { combined_rect.left(), combined_rect.bottom() },
					.max = { combined_rect.right(), divider_y }
				});
			}

			if (combined_rect.height() > 0.f) {
				const float child_height = (location == dock::location::top) ? (combined_rect.top() - divider_y)
																			 : (divider_y - combined_rect.bottom());
				child->dock_split_ratio = child_height / combined_rect.height();
			}
			break;
		}
		default:
			break;
	}

	layout::update(vp.menus, child->id());

	return current;
}

auto gse::gui::handle_pending_drag_state(viewport_state& vp, const states::pending_drag& current, const vec2f mouse_position, const bool mouse_held) -> gui::state {
	if (!mouse_held) {
		return states::idle{};
	}

	const float distance = magnitude(mouse_position - current.start_position);

	if (constexpr float drag_threshold = 5.0f; distance > drag_threshold) {
		menu* m = vp.menus.try_get(current.menu_id);
		if (!m) {
			return states::idle{};
		}

		id drag_menu_id = current.menu_id;
		vec2f drag_offset = current.offset;

		if (current.tab_index.has_value() && m->tab_contents.size() > 1) {
			if (const std::uint32_t tab_idx = current.tab_index.value(); tab_idx < m->tab_contents.size()) {
				std::string tab_name = m->tab_contents[tab_idx];

				m->tab_contents.erase(m->tab_contents.begin() + tab_idx);

				if (m->active_tab_index >= m->tab_contents.size()) {
					m->active_tab_index = static_cast<std::uint32_t>(m->tab_contents.size() - 1);
				}
				else if (m->active_tab_index > tab_idx) {
					m->active_tab_index--;
				}

				constexpr vec2f default_size = { 300.f, 200.f };

				const style sty = vp.fstate.sty;
				const vec2f new_top_left = { mouse_position.x() - default_size.x() * 0.5f,
											 mouse_position.y() + sty.title_bar_height * 0.5f };

				menu new_menu(
					tab_name,
					menu_data{
						.rect = rectf::from_position_size(new_top_left, default_size),
						.parent_id = id()
					}
				);

				const id new_id = new_menu.id();
				vp.menus.add(new_id, std::move(new_menu));

				drag_menu_id = new_id;
				drag_offset = { -default_size.x() * 0.5f, sty.title_bar_height * 0.5f };
			}
		}
		else if (m->docked_to != dock::location::none) {
			layout::undock(vp.menus, m->id());
		}

		set_style(cursor::style::omni_move);
		return states::dragging{
			.menu_id = drag_menu_id,
			.offset = drag_offset
		};
	}

	return current;
}

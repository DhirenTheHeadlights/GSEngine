export module gse.graphics:gui;

import std;

import :types;
import :layout;
import :font;
import :rendering_context;
import :sprite_renderer;
import :texture;
import :text_renderer;
import :cursor;
import :save;

import gse.platform;
import gse.utility;
import gse.physics.math;

export namespace gse::gui {
	auto initialize(
		renderer::context& context
	) -> void;

	auto update(
		const window& window
	) -> void;

	auto render(
		const renderer::context& context,
		renderer::sprite& sprite_renderer,
		renderer::text& text_renderer
	) -> void;

	auto start(
		const std::string& name,
		const menu_data& data
	) -> void;

	auto text(
		const std::string& text
	) -> void;

	template <is_arithmetic T>
	auto value(
		const std::string& name, 
		T value
	) -> void; 

	template <is_quantity T, typename Unit = typename T::default_unit>
	auto value(
		const std::string& name,
		T value
	) -> void;

	template <typename T, int N>
	auto vec(
		const std::string& name,
		unitless::vec_t<T, N> vec
	) -> void;

	template <typename T, int N, typename Unit = typename T::default_unit>
	auto vec(
		const std::string& name,
		vec_t<T, N> vec
	) -> void;
}

namespace gse::gui {
	id_mapped_collection<menu> menus;
	menu* current_menu = nullptr;
	resource::handle<font> font;
	resource::handle<texture> blank_texture;
	std::optional<dock::space> active_dock_space;
	state current_state;

	constexpr float padding = 10.f;
	constexpr float title_bar_height = 30.f;
	constexpr float font_size = 16.f;
	constexpr unitless::vec4 title_bar_color = { 0.15f, 0.15f, 0.15f, 0.9f };
	constexpr unitless::vec4 menu_body_color = { 0.1f, 0.1f, 0.1f, 0.85f };
	constexpr float resize_border_thickness = 8.f;
	constexpr unitless::vec2 min_menu_size = { 150.f, 100.f };
	constexpr unitless::vec4 dock_preview_color = { 0.2f, 0.2f, 0.8f, 0.4f };
	constexpr unitless::vec4 dock_widget_color = { 0.8f, 0.8f, 0.8f, 0.5f };

	auto begin(const std::string& name) -> bool;
	auto end() -> void;

	auto handle_idle_state(unitless::vec2 mouse_position, bool mouse_held) -> state;
	auto handle_dragging_state(const states::dragging& current, const window& window, unitless::vec2 mouse_position, bool mouse_held) -> state;
	auto handle_resizing_state(const states::resizing& current, unitless::vec2 mouse_position, bool mouse_held) -> state;
	auto handle_resizing_divider_state(const states::resizing_divider& current, unitless::vec2 mouse_position, bool mouse_held) -> state;
}

auto gse::gui::initialize(renderer::context& context) -> void {
	font = context.get<gse::font>("MonaspaceNeon-Regular");
	blank_texture = context.queue<texture>("blank", unitless::vec4(1, 1, 1, 1));
}

auto gse::gui::update(const window& window) -> void {
	const auto mouse_position = mouse::position();
	const bool mouse_held = mouse::held(mouse_button::button_1);

	current_state = std::visit([&]<typename T0>(
		T0&& state
	) -> gui::state {
			using t = std::decay_t<T0>;

			if constexpr (std::is_same_v<t, states::idle>) {
				return handle_idle_state(mouse_position, mouse_held);
			}
			else if constexpr (std::is_same_v<t, states::dragging>) {
				return handle_dragging_state(state, window, mouse_position, mouse_held);
			}
			else if constexpr (std::is_same_v<t, states::resizing>) {
				return handle_resizing_state(state, mouse_position, mouse_held);
			}
			else if constexpr (std::is_same_v<t, states::resizing_divider>) {
				return handle_resizing_divider_state(state, mouse_position, mouse_held);
			}

			return states::idle{};
		}, current_state);
}

auto gse::gui::start(const std::string& name, const menu_data& data) -> void {
	if (exists(name) && menus.contains(find(name))) {
		return;
	}

	menu new_menu(name, data);

	menus.add(new_menu.id(), std::move(new_menu));
}

auto gse::gui::render(const renderer::context& context, renderer::sprite& sprite_renderer, renderer::text& text_renderer) -> void {
	if (font.valid()) {
		for (auto& menu : menus.items()) {
			const auto title_bar_rect = ui_rect::from_position_size(
				menu.rect.top_left(),
				{ menu.rect.width(), title_bar_height }
			);
			const auto body_rect = ui_rect::from_position_size(
				{ menu.rect.left(), menu.rect.top() - title_bar_height },
				{ menu.rect.width(), menu.rect.height() - title_bar_height }
			);
			const auto content_rect = body_rect.inset({ padding, padding });

			sprite_renderer.queue({ .rect = body_rect, .color = menu_body_color, .texture = blank_texture });
			sprite_renderer.queue({ .rect = title_bar_rect, .color = title_bar_color, .texture = blank_texture });

			text_renderer.draw_text({
				.font = font,
				.text = menu.id().tag(),
				.position = {
					title_bar_rect.left() + padding,
					title_bar_rect.center().y() + font_size / 2.f
				},
				.scale = font_size,
				.clip_rect = title_bar_rect
			});

			current_menu = &menu;
			if (!menu.tab_contents.empty()) {
				menu.tab_contents[menu.active_tab_index];
			}

			const float line_height = font->line_height(font_size);
			const auto text_start_pos = content_rect.top_left();

			for (std::size_t i = 0; i < menu.items.size(); ++i) {
				const unitless::vec2 item_pos = {
					text_start_pos.x(),
					text_start_pos.y() - i * line_height
				};
				text_renderer.draw_text({
					.font = font,
					.text = menu.items[i],
					.position = item_pos,
					.scale = font_size,
					.clip_rect = content_rect
				});
			}
			menu.items.clear();
		}

		current_menu = nullptr;
	}

	if (active_dock_space) {
		const auto mouse_pos = mouse::position();
		for (const auto& area : active_dock_space->areas) {
			if (area.rect.contains(mouse_pos)) {
				sprite_renderer.queue({ 
					.rect = area.target,
					.color = dock_preview_color,
					.texture = blank_texture
				});
				break;
			}
		}
		for (const auto& area : active_dock_space->areas) {
			sprite_renderer.queue({
				.rect = area.rect,
				.color = dock_widget_color,
				.texture = blank_texture
			});
		}
	}

	if (context.ui_focus()) {
		cursor::render(context, sprite_renderer);
	}
}

auto gse::gui::text(const std::string& text) -> void {
	if (current_menu) {
		current_menu->items.emplace_back(text);
	}
}

template <gse::is_arithmetic T>
auto gse::gui::value(const std::string& name, T value) -> void {
	if (current_menu) {
		current_menu->items.emplace_back(std::format("{} {}", value, name));
	}
}

template <gse::is_quantity T, typename Unit>
auto gse::gui::value(const std::string& name, T value) -> void {
	if (current_menu) {
		current_menu->items.emplace_back(std::format("{} {} {}", value.template as<Unit>(), Unit::unit_name, name));
	}
}

template <typename T, int N>
auto gse::gui::vec(const std::string& name, unitless::vec_t<T, N> vec) -> void {
	if (current_menu) {
		current_menu->items.emplace_back(std::format("{} {}", vec, name));
	}
}

template <typename T, int N, typename Unit>
auto gse::gui::vec(const std::string& name, vec_t<T, N> vec) -> void {
	if (current_menu) {
		current_menu->items.emplace_back(std::format("{} {} {}", vec.template as<Unit>(), Unit::unit_name, name));
	}
}

auto gse::gui::begin(const std::string& name) -> bool {
	if (auto* menu = menus.try_get(find(name))) {
		current_menu = menu;

		const auto title_bar_rect = ui_rect::from_position_size(
			menu->rect.top_left(),
			{ menu->rect.width(), title_bar_height }
		);

		const auto body_rect = ui_rect::from_position_size(
			{ menu->rect.left(), menu->rect.top() - title_bar_height },
			{ menu->rect.width(), menu->rect.height() - title_bar_height }
		);



		return true;
	}

	return false;
}

auto gse::gui::handle_idle_state(const unitless::vec2 mouse_position, const bool mouse_held) -> state {
	struct interaction_candidate {
		std::variant<states::resizing, states::dragging, states::resizing_divider> future_state;
		cursor::style cursor;
	};

	struct resize_rule {
		std::function<bool(const ui_rect&, const unitless::vec2&)> condition;
		resize_handle handle;
		cursor::style cursor;
	};

	static const std::array<resize_rule, 8> resize_rules = { {
		{ [](auto r, auto p) {
			constexpr float t = resize_border_thickness;
			return std::abs(p.y() - r.top()) < t && std::abs(p.x() - r.left()) < t;
		}, resize_handle::top_left, cursor::style::resize_nw },
		{ [](auto r, auto p) {
			constexpr float t = resize_border_thickness;
			return std::abs(p.y() - r.top()) < t && std::abs(p.x() - r.right()) < t;
		}, resize_handle::top_right, cursor::style::resize_ne },
		{ [](auto r, auto p) {
			constexpr float t = resize_border_thickness;
			return std::abs(p.y() - r.bottom()) < t && std::abs(p.x() - r.left()) < t;
		}, resize_handle::bottom_left, cursor::style::resize_sw },
		{ [](auto r, auto p) {
			constexpr float t = resize_border_thickness;
			return std::abs(p.y() - r.bottom()) < t && std::abs(p.x() - r.right()) < t;
		}, resize_handle::bottom_right, cursor::style::resize_se },
		{ [](auto r, auto p) {
			constexpr float t = resize_border_thickness;
			return std::abs(p.x() - r.left()) < t;
		}, resize_handle::left, cursor::style::resize_w },
		{ [](auto r, auto p) {
			constexpr float t = resize_border_thickness;
			return std::abs(p.x() - r.right()) < t;
		}, resize_handle::right, cursor::style::resize_e },
		{ [](auto r, auto p) {
			constexpr float t = resize_border_thickness;
			return std::abs(p.y() - r.top()) < t;
		}, resize_handle::top, cursor::style::resize_n },
		{ [](auto r, auto p) {
			constexpr float t = resize_border_thickness;
			return std::abs(p.y() - r.bottom()) < t;
		}, resize_handle::bottom, cursor::style::resize_s },
	} };

	auto calculate_group_bounds = [](
		const id& root_id
		) -> ui_rect {
			const menu* root = menus.try_get(root_id);
			if (!root) return {};
			ui_rect bounds = root->rect;
			std::function<void(const id&)> expand = [&](const id& parent_id) {
				for (const auto& item : menus.items()) {
					if (item.owner_id() == parent_id) {
						bounds = ui_rect::bounding_box(bounds, item.rect);
						expand(item.id());
					}
				}
				};
			expand(root_id);
			return bounds;
		};

	set_style(cursor::style::arrow);

	auto hot_item = [&](
		) -> std::optional<interaction_candidate> {
			for (auto& menu : std::views::reverse(menus.items())) {
				if (!menu.owner_id().exists()) { 
					if (menu.docked_to == dock::location::none) { 
						const ui_rect group_rect = calculate_group_bounds(menu.id());
						for (const auto& [condition, handle, cursor] : resize_rules) {
							if (condition(group_rect, mouse_position)) {
								return interaction_candidate{
									.future_state = { states::resizing{.menu_id = menu.id(), .handle = handle } },
									.cursor = cursor
								};
							}
						}
					}
					else { 
						const ui_rect& rect = menu.rect;
						switch (menu.docked_to) {
							case dock::location::left:
								if (std::abs(mouse_position.x() - rect.right()) < resize_border_thickness)
									return interaction_candidate{ states::resizing{ menu.id(), resize_handle::right }, cursor::style::resize_e };
								break;
							case dock::location::right:
								if (std::abs(mouse_position.x() - rect.left()) < resize_border_thickness)
									return interaction_candidate{ states::resizing{ menu.id(), resize_handle::left }, cursor::style::resize_w };
								break;
							case dock::location::top:
								if (std::abs(mouse_position.y() - rect.bottom()) < resize_border_thickness)
									return interaction_candidate{ states::resizing{ menu.id(), resize_handle::bottom }, cursor::style::resize_s };
								break;
							case dock::location::bottom:
								if (std::abs(mouse_position.y() - rect.top()) < resize_border_thickness)
									return interaction_candidate{ states::resizing{ menu.id(), resize_handle::top }, cursor::style::resize_n };
								break;
							default: break;
						}
					}
				}
				else { 
					if (const auto* parent = menus.try_get(menu.owner_id())) {
						bool hovering = false;
						auto new_cursor = cursor::style::arrow;
						const auto& r = menu.rect;

						switch (menu.docked_to) {
							case dock::location::left:
								if (std::abs(mouse_position.x() - r.right()) < resize_border_thickness && mouse_position.y() < r.top() && mouse_position.y() > r.bottom()) {
									hovering = true; new_cursor = cursor::style::resize_e;
								} break;
							case dock::location::right:
								if (std::abs(mouse_position.x() - r.left()) < resize_border_thickness && mouse_position.y() < r.top() && mouse_position.y() > r.bottom()) {
									hovering = true; new_cursor = cursor::style::resize_w;
								} break;
							case dock::location::top:
								if (std::abs(mouse_position.y() - r.bottom()) < resize_border_thickness && mouse_position.x() > r.left() && mouse_position.x() < r.right()) {
									hovering = true; new_cursor = cursor::style::resize_s;
								} break;
							case dock::location::bottom:
								if (std::abs(mouse_position.y() - r.top()) < resize_border_thickness && mouse_position.x() > r.left() && mouse_position.x() < r.right()) {
									hovering = true; new_cursor = cursor::style::resize_n;
								} break;
							default: break;
						}

						if (hovering) {
							return interaction_candidate{
								.future_state = {
									states::resizing_divider{
										.parent_id = parent->id(),
										.child_id = menu.id()
									}
								},
								.cursor = new_cursor
							};
						}
					}
				}

				if (const auto title_bar_rect = ui_rect::from_position_size(
					{ menu.rect.left(), menu.rect.top() }, { menu.rect.width(), title_bar_height }
				); title_bar_rect.contains(mouse_position)) {
					return interaction_candidate{
						.future_state = {
							states::dragging{
								.menu_id = menu.id(),
								.offset = menu.rect.top_left() - mouse_position
							}
						},
						.cursor = cursor::style::omni_move
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
				if (const menu* m = menus.try_get(menu_id); m && m->docked_to != dock::location::none) {
					layout::undock(menus, m->id());
				}
			}
			return std::visit(
				[](auto&& arg) -> state {
					return arg;
				}, 
				hot_item->future_state
			);
		}
	}

	return states::idle{};
}

auto gse::gui::handle_resizing_state(const states::resizing& current, const unitless::vec2 mouse_position, const bool mouse_held) -> gui::state {
	if (!mouse_held) {
		active_dock_space.reset();
		return states::idle{};
	}

	menu* m = menus.try_get(current.menu_id);
	if (!m) return states::idle{};

	auto calculate_group_bounds = [](const id& root_id) -> ui_rect {
		const menu* root = menus.try_get(root_id);
		if (!root) return {};
		ui_rect bounds = root->rect;
		std::function<void(const id&)> expand = [&](const id& parent_id) {
			for (const auto& item : menus.items()) {
				if (item.owner_id() == parent_id) {
					bounds = ui_rect::bounding_box(bounds, item.rect);
					expand(item.id());
				}
			}
			};
		expand(root_id);
		return bounds;
		};

	const ui_rect group_rect = calculate_group_bounds(m->id());
	auto min_corner = group_rect.min();
	auto max_corner = group_rect.max();

	switch (current.handle) {
		case resize_handle::left:         min_corner.x() = mouse_position.x(); break;
		case resize_handle::right:        max_corner.x() = mouse_position.x(); break;
		case resize_handle::bottom:       min_corner.y() = mouse_position.y(); break;
		case resize_handle::top:          max_corner.y() = mouse_position.y(); break;
		case resize_handle::bottom_left:  min_corner = mouse_position; break;
		case resize_handle::bottom_right: min_corner.y() = mouse_position.y(); max_corner.x() = mouse_position.x(); break;
		case resize_handle::top_left:     min_corner.x() = mouse_position.x(); max_corner.y() = mouse_position.y(); break;
		case resize_handle::top_right:    max_corner = mouse_position; break;
		default: break;
	}

	if (max_corner.x() - min_corner.x() < min_menu_size.x()) {
		if (current.handle == resize_handle::left || current.handle == resize_handle::top_left || current.handle == resize_handle::bottom_left) {
			min_corner.x() = max_corner.x() - min_menu_size.x();
		}
		else {
			max_corner.x() = min_corner.x() + min_menu_size.x();
		}
	}
	if (max_corner.y() - min_corner.y() < min_menu_size.y()) {
		if (current.handle == resize_handle::bottom || current.handle == resize_handle::bottom_left || current.handle == resize_handle::bottom_right) {
			min_corner.y() = max_corner.y() - min_menu_size.y();
		}
		else {
			max_corner.y() = min_corner.y() + min_menu_size.y();
		}
	}

	m->rect = ui_rect({ .min = min_corner, .max = max_corner });
	layout::update(menus, m->id());

	return current;
}

auto gse::gui::handle_dragging_state(const states::dragging& current, const window& window, const unitless::vec2 mouse_position, const bool mouse_held) -> state {
	menu* m = menus.try_get(current.menu_id);
	if (!m) return states::idle{};

	if (!mouse_held) {
		if (active_dock_space) {
			id potential_dock_parent_id;
			for (const auto& other_menu : menus.items()) {
				if (other_menu.rect.contains(mouse_position) && other_menu.id() != current.menu_id) {
					potential_dock_parent_id = other_menu.id();
					break;
				}
			}
			for (const auto& area : active_dock_space->areas) {
				if (area.rect.contains(mouse_position)) {
					if (potential_dock_parent_id.exists()) {
						if (area.dock_location == dock::location::center) {
							if (menu* parent = menus.try_get(potential_dock_parent_id)) {
								parent->tab_contents.insert(
									parent->tab_contents.end(),
									std::make_move_iterator(m->tab_contents.begin()),
									std::make_move_iterator(m->tab_contents.end())
								);
								m->tab_contents.clear();
								parent->active_tab_index = parent->tab_contents.size() - 1;
								menus.remove(current.menu_id);
							}
						}
						else {
							layout::dock(menus, current.menu_id, potential_dock_parent_id, area.dock_location);
							layout::update(menus, m->id());
						}
					}
					else {
						if (area.dock_location != dock::location::center) {
							const auto viewport_size = unitless::vec2(window.viewport());
							const auto screen_rect = ui_rect::from_position_size({ 0.f, viewport_size.y() }, viewport_size);
							m->rect = layout::dock_target_rect(screen_rect, area.dock_location, 0.5f);
							m->docked_to = area.dock_location;
							m->swap(id());
							layout::update(menus, m->id());
						}
					}
					break;
				}
			}
		}
		active_dock_space.reset();
		return states::idle{};
	}

	const auto old_top_left = m->rect.top_left();
	auto new_top_left = mouse_position + current.offset;
	const auto window_size = unitless::vec2(window.viewport());
	new_top_left.x() = std::clamp(new_top_left.x(), 0.f, window_size.x() - m->rect.width());
	new_top_left.y() = std::clamp(new_top_left.y(), m->rect.height(), window_size.y());

	if (const auto delta = new_top_left - old_top_left; delta.x() != 0 || delta.y() != 0) {
		std::function<void(const id&)> move_group = [&](const id& current_id) {
			if (menu* item = menus.try_get(current_id)) {
				item->rect = ui_rect::from_position_size(item->rect.top_left() + delta, item->rect.size());
				for (auto& potential_child : menus.items()) {
					if (potential_child.owner_id() == current_id) {
						move_group(potential_child.id());
					}
				}
			}
			};
		move_group(current.menu_id);
	}

	active_dock_space.reset();
	bool found_parent_menu = false;
	for (auto& other_menu : std::views::reverse(menus.items())) {
		if (other_menu.id() == current.menu_id) continue;
		if (other_menu.rect.contains(mouse_position)) {
			active_dock_space = layout::dock_space(other_menu.rect);
			found_parent_menu = true;
			break;
		}
	}
	if (!found_parent_menu) {
		const auto screen_rect = ui_rect::from_position_size({ 0.f, window_size.y() }, window_size);
		active_dock_space = layout::dock_space(screen_rect);
	}

	return current;
}

auto gse::gui::handle_resizing_divider_state(const states::resizing_divider& current, const unitless::vec2 mouse_position, const bool mouse_held) -> state {
	if (!mouse_held) {
		return states::idle{};
	}

	menu* parent = menus.try_get(current.parent_id);
	menu* child = menus.try_get(current.child_id);

	if (!parent || !child) {
		return states::idle{};
	}

	const auto location = child->docked_to;
	const ui_rect combined_rect = ui_rect::bounding_box(parent->rect, child->rect);

	switch (location) {
		case dock::location::left:
		case dock::location::right: {
			if (combined_rect.width() < min_menu_size.x() * 2.f) {
				return current;
			}

			const float min_clamp = combined_rect.left() + min_menu_size.x();
			const float max_clamp = combined_rect.right() - min_menu_size.x();
			const float divider_x = std::clamp(mouse_position.x(), min_clamp, max_clamp);

			if (location == dock::location::left) {
				child->rect = ui_rect({
					.min = { combined_rect.left(), combined_rect.bottom() },
					.max = { divider_x, combined_rect.top() }
				});
				parent->rect = ui_rect({
					.min = { divider_x, combined_rect.bottom() },
					.max = { combined_rect.right(), combined_rect.top() }
				});
			}
			else {
				parent->rect = ui_rect({
					.min = { combined_rect.left(), combined_rect.bottom() },
					.max = { divider_x, combined_rect.top() }
				});
				child->rect = ui_rect({
					.min = { divider_x, combined_rect.bottom() },
					.max = { combined_rect.right(), combined_rect.top() }
				});
			}

			if (combined_rect.width() > 0.f) {
				const float child_width = (location == dock::location::left) ? (divider_x - combined_rect.left()) : (combined_rect.right() - divider_x);
				parent->dock_split_ratio = child_width / combined_rect.width();
			}
			break;
		}
		case dock::location::top:
		case dock::location::bottom: {
			if (combined_rect.height() < min_menu_size.y() * 2.f) {
				return current;
			}

			const float min_clamp = combined_rect.bottom() + min_menu_size.y();
			const float max_clamp = combined_rect.top() - min_menu_size.y();
			const float divider_y = std::clamp(mouse_position.y(), min_clamp, max_clamp);

			if (location == dock::location::top) {
				child->rect = ui_rect({
					.min = { combined_rect.left(), divider_y },
					.max = { combined_rect.right(), combined_rect.top() }
				});
				parent->rect = ui_rect({
					.min = { combined_rect.left(), combined_rect.bottom() },
					.max = { combined_rect.right(), divider_y }
				});
			}
			else {
				parent->rect = ui_rect({
					.min = { combined_rect.left(), divider_y },
					.max = { combined_rect.right(), combined_rect.top() }
				});
				child->rect = ui_rect({
					.min = { combined_rect.left(), combined_rect.bottom() },
					.max = { combined_rect.right(), divider_y }
				});
			}

			if (combined_rect.height() > 0.f) {
				const float child_height = (location == dock::location::top) ? (combined_rect.top() - divider_y) : (divider_y - combined_rect.bottom());
				parent->dock_split_ratio = child_height / combined_rect.height();
			}
			break;
		}
		default:
			break;
	}

	layout::update(menus, child->id());

	return current;
}
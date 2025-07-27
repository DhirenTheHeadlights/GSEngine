module;

#include <GLFW/glfw3.h>

export module gse.graphics:gui;

import std;

import :font;
import :rendering_context;
import :sprite_renderer;
import :texture;
import :text_renderer;

import gse.platform;
import gse.utility;
import gse.physics.math;

export namespace gse::gui {
	auto initialize(renderer::context& context) -> void;
	auto update() -> void;
	auto render(renderer::context& context, renderer::sprite& sprite_renderer, renderer::text& text_renderer) -> void;

	auto create_menu(const std::string& name, const unitless::vec2& top_left, const unitless::vec2& size, const std::function<void()>& contents) -> void;

	auto text(const std::string& text) -> void;

	template <is_arithmetic T>
	auto value(const std::string& name, T value) -> void;

	template <is_quantity T, typename Unit = typename T::default_unit>
	auto value(const std::string& name, T value) -> void;

	template <typename T, int N>
	auto vec(const std::string& name, unitless::vec_t<T, N> vec) -> void;

	template <typename T, int N, typename Unit = typename T::default_unit>
	auto vec(const std::string& name, vec_t<T, N> vec) -> void;
}

namespace gse::gui {
	using ui_rect = rect_t<unitless::vec2>;

	enum class resize_handle {
		none, left, right, top, bottom,
		top_left, top_right, bottom_left, bottom_right
	};

	enum class dock_location {
		none, center, left, right, top, bottom
	};

	struct docking_area {
		ui_rect rect;
		ui_rect target_rect;
		dock_location location;
	};

	struct dockspace {
		std::array<docking_area, 5> areas;
	};

	struct menu {
		ui_rect rect;
		unitless::vec2 pre_docked_size;
		std::string name = "";
		quota_timed_lock<bool, 2> grabbed{ false, seconds(0.05) };
		unitless::vec2 offset;
		timed_lock<bool> docked{ false, seconds(0.25) };
		resize_handle resizing{ resize_handle::none };
		menu* parent = nullptr;
		dock_location docked_to = dock_location::none;

		std::function<void()> contents;
		std::vector<std::string> items;
		dockspace dock;
	};

	static std::vector<menu> menus;
	static dockspace main_dockspace;
	static menu* current_menu = nullptr;
	static resource::handle<font> font;
	static resource::handle<texture> blank;

	// UI Style Constants
	constexpr float padding = 10.f;
	constexpr float resize_border_thickness = 8.f;
	constexpr float title_bar_height = 30.f;
	constexpr float font_size = 16.f;
	constexpr unitless::vec2 min_menu_size = { 150.f, 100.f };
	constexpr unitless::vec4 title_bar_color = { 0.15f, 0.15f, 0.15f, 0.9f };
	constexpr unitless::vec4 menu_body_color = { 0.1f, 0.1f, 0.1f, 0.85f };
	constexpr unitless::vec4 dock_preview_color = { 0.1f, 0.4f, 0.9f, 0.5f };

	auto update_dockspace_areas(menu& m) -> void {
		const auto& parent_rect = m.rect;
		const auto center = parent_rect.center();
		const unitless::vec2 dock_widget_size = {
			std::min(50.f, parent_rect.width() * 0.2f),
			std::min(50.f, parent_rect.height() * 0.2f)
		};

		const float half_width = parent_rect.width() / 2.f;
		const float half_height = parent_rect.height() / 2.f;

		m.dock.areas = {
			docking_area {
				.rect = ui_rect::from_position_size({ center.x - dock_widget_size.x / 2.f, center.y + dock_widget_size.y / 2.f }, dock_widget_size),
				.target_rect = parent_rect,
				.location = dock_location::center
			},
			docking_area {
				.rect = ui_rect::from_position_size({ parent_rect.left() + dock_widget_size.x / 2.f, center.y + dock_widget_size.y / 2.f }, dock_widget_size),
				.target_rect = ui_rect::from_position_size(parent_rect.top_left(), {half_width, parent_rect.height()}),
				.location = dock_location::left
			},
			docking_area {
				.rect = ui_rect::from_position_size({ parent_rect.right() - dock_widget_size.x * 1.5f, center.y + dock_widget_size.y / 2.f }, dock_widget_size),
				.target_rect = ui_rect::from_position_size({parent_rect.left() + half_width, parent_rect.top()}, {half_width, parent_rect.height()}),
				.location = dock_location::right
			},
			docking_area {
				.rect = ui_rect::from_position_size({ center.x - dock_widget_size.x / 2.f, parent_rect.top() - dock_widget_size.y / 2.f }, dock_widget_size),
				.target_rect = ui_rect::from_position_size(parent_rect.top_left(), {parent_rect.width(), half_height}),
				.location = dock_location::top
			},
			docking_area {
				.rect = ui_rect::from_position_size({ center.x - dock_widget_size.x / 2.f, parent_rect.bottom() + dock_widget_size.y * 1.5f }, dock_widget_size),
				.target_rect = ui_rect::from_position_size({parent_rect.left(), parent_rect.top() - half_height}, {parent_rect.width(), half_height}),
				.location = dock_location::bottom
			}
		};
	}
}

auto gse::gui::initialize(renderer::context& context) -> void {
	font = context.get<gse::font>("MonaspaceNeon-Regular");
	blank = context.queue<texture>("blank", unitless::vec4(1, 1, 1, 1));

	const auto window_size = window::get_window_size();
	const auto window_rect = ui_rect::from_position_size({ 0, static_cast<float>(window_size.y) }, window_size);
	const auto center = window_rect.center();
	constexpr unitless::vec2 dock_widget_size = { 50.f, 50.f };
	const float half_width = window_size.x / 2.f;
	const float half_height = window_size.y / 2.f;

	main_dockspace = {
		.areas = {
			docking_area {
				.rect = ui_rect::from_position_size({ center.x - dock_widget_size.x / 2.f, center.y + dock_widget_size.y / 2.f }, dock_widget_size),
				.target_rect = window_rect,
				.location = dock_location::center
			},
			docking_area {
				.rect = ui_rect::from_position_size({ dock_widget_size.x / 2.f, center.y + dock_widget_size.y / 2.f }, dock_widget_size),
				.target_rect = ui_rect::from_position_size(window_rect.top_left(), {half_width, static_cast<float>(window_size.y)}),
				.location = dock_location::left
			},
			docking_area {
				.rect = ui_rect::from_position_size({ window_size.x - dock_widget_size.x * 1.5f, center.y + dock_widget_size.y / 2.f }, dock_widget_size),
				.target_rect = ui_rect::from_position_size({half_width, window_rect.top()}, {half_width, static_cast<float>(window_size.y)}),
				.location = dock_location::right
			},
			docking_area {
				.rect = ui_rect::from_position_size({ center.x - dock_widget_size.x / 2.f, window_size.y - dock_widget_size.y / 2.f }, dock_widget_size),
				.target_rect = ui_rect::from_position_size(window_rect.top_left(), {static_cast<float>(window_size.x), half_height}),
				.location = dock_location::top
			},
			docking_area {
				.rect = ui_rect::from_position_size({ center.x - dock_widget_size.x / 2.f, dock_widget_size.y * 1.5f }, dock_widget_size),
				.target_rect = ui_rect::from_position_size({0, half_height}, {static_cast<float>(window_size.x), half_height}),
				.location = dock_location::bottom
			}
		}
	};
}

auto gse::gui::update() -> void {
	auto get_target_rect_for_dock = [](const ui_rect& parent_rect, dock_location location) -> ui_rect {
		const float half_width = parent_rect.width() / 2.f;
		const float half_height = parent_rect.height() / 2.f;

		switch (location) {
		case dock_location::left:   return ui_rect::from_position_size(parent_rect.top_left(), { half_width, parent_rect.height() });
		case dock_location::right:  return ui_rect::from_position_size({ parent_rect.left() + half_width, parent_rect.top() }, { half_width, parent_rect.height() });
		case dock_location::top:    return ui_rect::from_position_size(parent_rect.top_left(), { parent_rect.width(), half_height });
		case dock_location::bottom: return ui_rect::from_position_size({ parent_rect.left(), parent_rect.top() - half_height }, { parent_rect.width(), half_height });
		case dock_location::center:
		default:                    return parent_rect;
		}
		};

	std::unordered_map<menu*, std::vector<menu*>> dock_graph;
	std::vector<menu*> root_menus;

	for (auto& m : menus) {
		if (m.parent) {
			dock_graph[m.parent].push_back(&m);
		}
		else {
			root_menus.push_back(&m);
		}
	}

	std::function<void(menu*)> update_children =
		[&](menu* parent_menu) {
		if (dock_graph.count(parent_menu)) {
			for (auto* child_menu : dock_graph[parent_menu]) {
				child_menu->rect = get_target_rect_for_dock(parent_menu->rect, child_menu->docked_to);
				update_dockspace_areas(*child_menu);
				update_children(child_menu);
			}
		}
		};

	for (auto* root : root_menus) {
		update_children(root);
	}

	const auto mouse_position = window::get_mouse_position_rel_bottom_left();
	const menu* interacting_menu = nullptr;

	for (auto& m : menus) {
		if (m.grabbed.value() || m.resizing != resize_handle::none) {
			interacting_menu = &m;
			break;
		}
	}

	for (auto& m : std::views::reverse(menus)) {
		if (interacting_menu && interacting_menu != &m) continue;

		if (input::get_mouse().buttons[GLFW_MOUSE_BUTTON_LEFT].held) {
			if (m.resizing != resize_handle::none) {
				auto min_corner = m.rect.min();
				auto max_corner = m.rect.max();

				switch (m.resizing) {
				case resize_handle::left:         min_corner.x = mouse_position.x; break;
				case resize_handle::right:        max_corner.x = mouse_position.x; break;
				case resize_handle::bottom:       min_corner.y = mouse_position.y; break;
				case resize_handle::top:          max_corner.y = mouse_position.y; break;
				case resize_handle::bottom_left:  min_corner = mouse_position; break;
				case resize_handle::bottom_right: min_corner.y = mouse_position.y; max_corner.x = mouse_position.x; break;
				case resize_handle::top_left:     min_corner.x = mouse_position.x; max_corner.y = mouse_position.y; break;
				case resize_handle::top_right:    max_corner = mouse_position; break;
				default:;
				}

				m.rect = ui_rect({ .min = min_corner, .max = max_corner });

				if (m.rect.width() < min_menu_size.x) {
					if (m.resizing == resize_handle::left || m.resizing == resize_handle::top_left || m.resizing == resize_handle::bottom_left) {
						min_corner.x = max_corner.x - min_menu_size.x;
					}
					else {
						max_corner.x = min_corner.x + min_menu_size.x;
					}
				}
				if (m.rect.height() < min_menu_size.y) {
					if (m.resizing == resize_handle::bottom || m.resizing == resize_handle::bottom_left || m.resizing == resize_handle::bottom_right) {
						min_corner.y = max_corner.y - min_menu_size.y;
					}
					else {
						max_corner.y = min_corner.y + min_menu_size.y;
					}
				}
				m.rect = ui_rect({ .min = min_corner, .max = max_corner });
				update_dockspace_areas(m);
			}
			else if (m.grabbed.value()) {
				auto new_top_left = mouse_position + m.offset;
				new_top_left = clamp(new_top_left, { 0.f, m.rect.height() }, unitless::vec2(window::get_window_size()));
				m.rect = ui_rect::from_position_size(new_top_left, m.rect.size());
				update_dockspace_areas(m);
			}
			else if (m.rect.contains(mouse_position)) {
				const bool on_left = std::abs(mouse_position.x - m.rect.left()) < resize_border_thickness;
				const bool on_right = std::abs(mouse_position.x - m.rect.right()) < resize_border_thickness;
				const bool on_bottom = std::abs(mouse_position.y - m.rect.bottom()) < resize_border_thickness;
				const bool on_top = std::abs(mouse_position.y - m.rect.top()) < resize_border_thickness;

				if (on_top && on_left) m.resizing = resize_handle::top_left;
				else if (on_top && on_right) m.resizing = resize_handle::top_right;
				else if (on_bottom && on_left) m.resizing = resize_handle::bottom_left;
				else if (on_bottom && on_right) m.resizing = resize_handle::bottom_right;
				else if (on_left) m.resizing = resize_handle::left;
				else if (on_right) m.resizing = resize_handle::right;
				else if (on_top) m.resizing = resize_handle::top;
				else if (on_bottom) m.resizing = resize_handle::bottom;

				if (m.resizing == resize_handle::none) {
					const auto title_bar_rect = ui_rect({
						.min = { m.rect.left(), m.rect.top() - title_bar_height },
						.max = { m.rect.right(), m.rect.top() }
					});

					if (title_bar_rect.contains(mouse_position)) {
						m.grabbed = true;
						m.offset = m.rect.top_left() - mouse_position;

						if (m.docked.value() && magnitude(m.offset) > 10.f) {
							m.docked = false;
							m.parent = nullptr;
							m.docked_to = dock_location::none;
							m.rect = ui_rect::from_position_size(m.rect.top_left(), m.pre_docked_size);
							m.offset = {};
							m.rect = ui_rect::from_position_size(mouse_position, m.rect.size());
							update_dockspace_areas(m);
						}
					}
				}

				if (m.resizing != resize_handle::none || m.grabbed.value()) {
					interacting_menu = &m;
					break;
				}
			}
		}
		else {
			if (m.grabbed.value()) {
				const docking_area* target_area = nullptr;
				menu* target_parent = nullptr;

				for (const auto& dock : main_dockspace.areas) {
					if (dock.rect.contains(mouse_position)) {
						target_area = &dock;
						break;
					}
				}
				if (!target_area) {
					for (auto& other_menu : menus) {
						if (&other_menu == &m || !other_menu.rect.contains(mouse_position)) continue;
						for (const auto& dock : other_menu.dock.areas) {
							if (dock.rect.contains(mouse_position)) {
								target_area = &dock;
								target_parent = &other_menu;
								break;
							}
						}
						if (target_area) break;
					}
				}

				if (target_area) {
					m.pre_docked_size = m.rect.size();
					m.rect = target_area->target_rect;
					m.docked = true;
					m.docked_to = target_area->location;
					m.parent = target_parent;
					update_dockspace_areas(m);
				}
			}
			m.grabbed = false;
			m.resizing = resize_handle::none;
		}
	}
}

auto gse::gui::render(renderer::context& context, renderer::sprite& sprite_renderer, renderer::text& text_renderer) -> void {
	if (font.state() != resource::state::loaded) {
		return;
	}

	const menu* grabbed_menu = nullptr;
	for (auto& m : menus) {
		if (m.grabbed.value()) {
			grabbed_menu = &m;
			break;
		}
	}

	for (auto& m : menus) {
		const auto title_bar_rect = ui_rect({
			.min = { m.rect.left(), m.rect.top() - title_bar_height },
			.max = { m.rect.right(), m.rect.top() }
		});
		const auto body_rect = ui_rect({
			.min = { m.rect.left(), m.rect.bottom() },
			.max = { m.rect.right(), m.rect.top() - title_bar_height }
		});

		const auto content_rect = body_rect.inset({ padding, padding });

		sprite_renderer.queue({ .rect = body_rect, .color = menu_body_color, .texture = blank });
		sprite_renderer.queue({ .rect = title_bar_rect, .color = title_bar_color, .texture = blank });

		current_menu = &m;
		m.contents();

		const float line_height = font->line_height(font_size);

		const auto title_pos = unitless::vec2{ title_bar_rect.left() + padding, title_bar_rect.center().y + font->line_height(font_size) };
		text_renderer.draw_text({
			.font = font,
			.text = m.name,
			.position = title_pos,
			.scale = font_size,
			.color = { 1.f, 1.f, 1.f, 1.f },
			.clip_rect = title_bar_rect
			});

		const auto text_start_pos = content_rect.top_left();
		for (size_t i = 0; i < m.items.size(); ++i) {
			const unitless::vec2 item_pos = { text_start_pos.x, text_start_pos.y - ((i + 1) * line_height) };
			text_renderer.draw_text({
				.font = font,
				.text = m.items[i],
				.position = item_pos,
				.scale = font_size,
				.color = { 1.f, 1.f, 1.f, 1.f },
				.clip_rect = content_rect
				});
		}
		m.items.clear();
	}

	if (grabbed_menu) {
		const auto mouse_position = window::get_mouse_position_rel_bottom_left();
		bool over_menu_dock = false;
		for (auto& m : menus) {
			if (&m == grabbed_menu) continue;
			if (m.rect.contains(mouse_position)) {
				for (const auto& [rect, target_rect, location] : m.dock.areas) {
					sprite_renderer.queue({ .rect = rect, .color = dock_preview_color, .texture = blank });
				}
				over_menu_dock = true;
				break;
			}
		}
		if (!over_menu_dock) {
			for (const auto& [rect, target_rect, location] : main_dockspace.areas) {
				sprite_renderer.queue({ .rect = rect, .color = dock_preview_color, .texture = blank });
			}
		}
	}
	current_menu = nullptr;
}

auto gse::gui::create_menu(const std::string& name, const unitless::vec2& top_left, const unitless::vec2& size, const std::function<void()>& contents) -> void {
	if (const auto it = std::ranges::find_if(menus, [&](const menu& m) { return m.name == name; }); it != menus.end()) {
		return;
	}

	menus.push_back({
		.rect = ui_rect::from_position_size(top_left, size),
		.pre_docked_size = size,
		.name = name,
		.contents = contents
		});

	update_dockspace_areas(menus.back());
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
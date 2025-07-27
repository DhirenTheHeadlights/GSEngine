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
		none,
		left,
		right,
		top,
		bottom,
		top_left,
		top_right,
		bottom_left,
		bottom_right
	};

	struct menu {
		ui_rect rect;
		unitless::vec2 pre_docked_size;
		std::string name = "";
		quota_timed_lock<bool, 2> grabbed{ false, seconds(0.05) };
		unitless::vec2 offset;
		timed_lock<bool> docked{ false, seconds(0.25) };
		bool docked_waiting_for_release = false;
		resize_handle resizing{ resize_handle::none };

		std::function<void()> contents;
		std::vector<std::string> items;
	};

	struct docking_area {
		ui_rect rect;
		static unitless::vec4 color;

		unitless::vec2 docked_position;
		unitless::vec2 docked_size;
	};

	std::vector<menu> menus;
	static constinit std::array<docking_area, 5> docks;
	menu* current_menu = nullptr;
	resource::handle<font> font;
	resource::handle<texture> blank;

	unitless::vec4 docking_area::color = { 0.f, 1.f, 0.f, 1.f };

	static constinit bool render_docking_preview = false;
	constexpr float padding = 10.f;
	constexpr float resize_border_thickness = 8.f;
	constexpr unitless::vec2 min_menu_size = { 150.f, 100.f };
}

auto gse::gui::initialize(renderer::context& context) -> void {
	font = context.get<gse::font>("MonaspaceNeon-Regular");
	blank = context.queue<texture>("blank", unitless::vec4(1, 1, 1, 1));

	const auto window_size = window::get_window_size();
	const auto top_left = unitless::vec2(0.f, window_size.y);
	constexpr unitless::vec2 dock_widget_size = { 50.f, 50.f };

	docks = {
		docking_area {
			.rect = ui_rect({.top_left_position = {0.f, window_size.y / 2.f + dock_widget_size.y / 2.f}, .size = dock_widget_size}),
			.docked_position = top_left,
			.docked_size = { static_cast<float>(window_size.x) / 2.f, static_cast<float>(window_size.y) }
		},
		docking_area {
			.rect = ui_rect({.top_left_position = {window_size.x / 2.f - dock_widget_size.x / 2.f, static_cast<float>(window_size.y)}, .size = dock_widget_size}),
			.docked_position = top_left,
			.docked_size = { static_cast<float>(window_size.x), static_cast<float>(window_size.y) / 2.f }
		},
		docking_area {
			.rect = ui_rect({.top_left_position = {window_size.x - dock_widget_size.x, window_size.y / 2.f + dock_widget_size.y / 2.f}, .size = dock_widget_size}),
			.docked_position = { static_cast<float>(window_size.x) / 2.f, static_cast<float>(window_size.y) },
			.docked_size = { static_cast<float>(window_size.x) / 2.f, static_cast<float>(window_size.y) }
		},
		docking_area {
			.rect = ui_rect({.top_left_position = {window_size.x / 2.f - dock_widget_size.x / 2.f, dock_widget_size.y}, .size = dock_widget_size}),
			.docked_position = { 0.f, static_cast<float>(window_size.y) / 2.f },
			.docked_size = { static_cast<float>(window_size.x), static_cast<float>(window_size.y) / 2.f }
		},
		docking_area {
			.rect = ui_rect({.top_left_position = {window_size.x / 2.f - dock_widget_size.x / 2.f, window_size.y / 2.f + dock_widget_size.y / 2.f}, .size = dock_widget_size}),
			.docked_position = top_left,
			.docked_size = window_size
		}
	};
}

auto gse::gui::update() -> void {
	for (auto& m : menus) {
		const auto mouse_position = window::get_mouse_position_rel_bottom_left();
		const bool is_interacting_with_this_menu = m.grabbed.value() || m.resizing != resize_handle::none;

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

			}
			else if (m.grabbed.value()) {
				auto new_top_left = mouse_position + m.offset;
				new_top_left = clamp(new_top_left, { 0.f, m.rect.height() }, unitless::vec2(window::get_window_size()));
				m.rect = ui_rect({ .top_left_position = new_top_left, .size = m.rect.size() });
			}
			else if (!is_interacting_with_this_menu) {
				bool on_left = std::abs(mouse_position.x - m.rect.left()) < resize_border_thickness;
				bool on_right = std::abs(mouse_position.x - m.rect.right()) < resize_border_thickness;
				bool on_bottom = std::abs(mouse_position.y - m.rect.bottom()) < resize_border_thickness;
				bool on_top = std::abs(mouse_position.y - m.rect.top()) < resize_border_thickness;

				if (on_top && on_left) m.resizing = resize_handle::top_left;
				else if (on_top && on_right) m.resizing = resize_handle::top_right;
				else if (on_bottom && on_left) m.resizing = resize_handle::bottom_left;
				else if (on_bottom && on_right) m.resizing = resize_handle::bottom_right;
				else if (on_left) m.resizing = resize_handle::left;
				else if (on_right) m.resizing = resize_handle::right;
				else if (on_top) m.resizing = resize_handle::top;
				else if (on_bottom) m.resizing = resize_handle::bottom;
				else if (m.rect.contains(mouse_position)) {
					m.grabbed = true;
					m.offset = m.rect.top_left() - mouse_position;

					if (m.docked.value() && m.docked.value_mutable() && magnitude(m.offset) > 10.f) {
						m.docked = false;
						m.rect = ui_rect({ .top_left_position = m.rect.top_left(), .size = m.pre_docked_size });
						m.offset = {};
						m.rect = ui_rect({ .top_left_position = mouse_position, .size = m.rect.size() });
					}
				}
			}
		}
		else {
			if (m.grabbed.value()) {
				for (const auto& [rect, docked_position, docked_size] : docks) {
					if (rect.contains(mouse_position)) {
						m.pre_docked_size = m.rect.size();
						m.rect = ui_rect({
							.top_left_position = docked_position,
							.size = docked_size
							});
						m.docked = true;
						break;
					}
				}
			}

			m.grabbed = false;
			m.docked_waiting_for_release = false;
			m.resizing = resize_handle::none;
		}
	}
}

auto gse::gui::render(renderer::context& context, renderer::sprite& sprite_renderer, renderer::text& text_renderer) -> void {
	if (font.state() != resource::state::loaded) {
		return;
	}

	for (auto& m : menus) {
		constexpr float font_size = 16.f;
		constexpr unitless::vec2 padding_vec = { padding, padding };

		const auto content_rect = m.rect.inset(padding_vec);

		sprite_renderer.queue({ .rect = m.rect, .color = { 0.1f, 0.1f, 0.1f, 0.85f }, .texture = blank });

		current_menu = &m;
		m.contents();

		const float line_height = font->line_height(font_size);
		const auto text_start_pos = content_rect.top_left();

		text_renderer.draw_text({
			.font = font,
			.text = m.name,
			.position = text_start_pos,
			.scale = font_size,
			.color = { 1.f, 1.f, 1.f, 1.f },
			.clip_rect = content_rect
			});

		for (size_t i = 0; i < m.items.size(); ++i) {
			const unitless::vec2 item_pos = {
				text_start_pos.x,
				text_start_pos.y - ((i + 1) * line_height)
			};
			text_renderer.draw_text({
				.font = font,
				.text = m.items[i],
				.position = item_pos,
				.scale = font_size,
				.color = { 1.f, 1.f, 1.f, 1.f },
				.clip_rect = content_rect
				});
		}

		if (render_docking_preview) {
			for (const auto& dock : docks) {
				sprite_renderer.queue({ .rect = dock.rect, .color = { 0.f, 0.f, 1.f, 1.f }, .texture = blank });
			}
		}

		if (m.grabbed.value()) {
			for (const auto& dock : docks) {
				sprite_renderer.queue({ .rect = dock.rect, .color = docking_area::color, .texture = blank });
			}
		}

		m.items.clear();
	}
	current_menu = nullptr;
}

auto gse::gui::create_menu(const std::string& name, const unitless::vec2& top_left, const unitless::vec2& size, const std::function<void()>& contents) -> void {
	if (const auto it = std::ranges::find_if(menus, [&](const menu& m) { return m.name == name; }); it != menus.end()) {
		current_menu = &*it;
		return;
	}

	menus.push_back({
		.rect = ui_rect({.top_left_position = top_left, .size = size }),
		.pre_docked_size = size,
		.name = name,
		.contents = contents,
		.items = {}
		});

	current_menu = &menus.back();
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
		current_menu->items.emplace_back(std::format("{}: {} {}", name, value.template as<Unit>(), Unit::unit_name));
	}
}

template <typename T, int N>
auto gse::gui::vec(const std::string& name, unitless::vec_t<T, N> vec) -> void {
	if (current_menu) {
		current_menu->items.emplace_back(std::format("{}: {}", name, vec));
	}
}

template <typename T, int N, typename Unit>
auto gse::gui::vec(const std::string& name, vec_t<T, N> vec) -> void {
	if (current_menu) {
		current_menu->items.emplace_back(std::format("{}: {} {}", name, vec.template as<Unit>(), Unit::unit_name));
	}
}
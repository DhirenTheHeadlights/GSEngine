module;

#include <GLFW/glfw3.h>

export module gse.graphics:gui;

import std;

import :types;
import :layout;
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
	auto update(const window& window) -> void;
	auto render(renderer::context& context, renderer::sprite& sprite_renderer, renderer::text& text_renderer) -> void;
	auto create_menu(const std::string& name, const menu_data& data) -> void;
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
	id_mapped_collection<menu> menus;
	menu* current_menu = nullptr;
	resource::handle<font> font;
	resource::handle<texture> blank_texture;
	id interacting_menu_id;

	constexpr float padding = 10.f;
	constexpr float title_bar_height = 30.f;
	constexpr float font_size = 16.f;
	constexpr unitless::vec4 title_bar_color = { 0.15f, 0.15f, 0.15f, 0.9f };
	constexpr unitless::vec4 menu_body_color = { 0.1f, 0.1f, 0.1f, 0.85f };
}

auto gse::gui::initialize(renderer::context& context) -> void {
	font = context.get<gse::font>("MonaspaceNeon-Regular");
	blank_texture = context.queue<texture>("blank", unitless::vec4(1, 1, 1, 1));
}

auto gse::gui::update(const window& window) -> void {
	const auto mouse_position = mouse::position();
	const bool mouse_held = mouse::held(mouse_button::button_1);

	if (!mouse_held) {
		if (interacting_menu_id.exists()) {
			if (menu* m = menus.try_get(interacting_menu_id)) {
				m->grabbed = false;
			}
			interacting_menu_id = id();
			std::println("Menu interaction ended.");
		}
		return;
	}

	if (interacting_menu_id.exists()) {
		if (menu* m = menus.try_get(interacting_menu_id)) {
			if (m->grabbed) {
				auto new_top_left = mouse_position + m->grab_offset;
				const auto window_size = unitless::vec2(window.viewport());
				new_top_left.x = std::clamp(new_top_left.x, 0.f, window_size.x - m->rect.width());
				new_top_left.y = std::clamp(new_top_left.y, m->rect.height(), window_size.y);
				m->rect = ui_rect::from_position_size(new_top_left, m->rect.size());
				std::println("Menu '{}' moved to: {}", m->id().tag(), m->rect.top_left());
			}
		}
	}
	else {
		for (auto& menu_to_check : std::views::reverse(menus.items())) {
			const auto title_bar_rect = ui_rect::from_position_size(
				menu_to_check.rect.top_left(),
				{ menu_to_check.rect.width(), title_bar_height }
			);

			if (title_bar_rect.contains(mouse_position)) {
				if (menu_to_check.owner_id().exists()) {
					layout::undock(menus, menu_to_check.id());
				}

				menu_to_check.grabbed = true;
				menu_to_check.grab_offset = menu_to_check.rect.top_left() - mouse_position;
				interacting_menu_id = menu_to_check.id();

				std::println("Menu '{}' grabbed at position: {}", menu_to_check.id().tag(), mouse_position);

				break;
			}
		}
	}
}

auto gse::gui::create_menu(const std::string& name, const menu_data& data) -> void {
	if (exists(name)) {
		return;
	}

	menus.add(generate_id(name), menu(name, data));
}

auto gse::gui::render(renderer::context& context, renderer::sprite& sprite_renderer, renderer::text& text_renderer) -> void {
	if (font.state() != resource::state::loaded) {
		return;
	}

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
			.position = { title_bar_rect.left() + padding, title_bar_rect.center().y + font_size / 2.f },
			.scale = font_size,
			.clip_rect = title_bar_rect
			});

		current_menu = &menu;
		if (!menu.tab_contents.empty()) {
			menu.tab_contents[menu.active_tab_index].second();
		}

		const float line_height = font->line_height(font_size);
		const auto text_start_pos = content_rect.top_left();

		for (size_t i = 0; i < menu.items.size(); ++i) {
			const unitless::vec2 item_pos = { text_start_pos.x, text_start_pos.y - (i * line_height) };
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

auto gse::gui::text(const std::string& text) -> void {
	if (current_menu) {
		current_menu->items.emplace_back(text);
	}
}

template <gse::is_arithmetic T>
auto gse::gui::value(const std::string& name, T value) -> void {
	if (current_menu) {
		current_menu->items.emplace_back(std::format("{}: {}", name, value));
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
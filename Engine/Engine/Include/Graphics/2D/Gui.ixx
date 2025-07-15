module;

#include "GLFW/glfw3.h"

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

struct menu {
	gse::id texture_id;
	gse::unitless::vec2 position;
	gse::unitless::vec2 size;
	gse::unitless::vec2 pre_docked_size;
	std::string name = "";
	gse::quota_timed_lock<bool, 2> grabbed{ false, gse::seconds(0.05) };
	gse::unitless::vec2 offset;
	gse::timed_lock<bool> docked{ false, gse::seconds(0.25) };
	bool docked_waiting_for_release = false;

	std::function<void()> contents;
	std::vector<std::string> items;
};

struct docking_area {
	gse::unitless::vec2 position;
	static gse::unitless::vec2 size;
	static gse::unitless::vec4 color;

	gse::unitless::vec2 docked_position;
	gse::unitless::vec2 docked_size;
};

namespace gse::gui {
	static constinit std::vector<menu> menus;
	static constinit std::array<docking_area, 5> docks;
	static constinit menu* current_menu = nullptr;
	static constinit id font_id;

	unitless::vec2 docking_area::size = { 10.f, 10.f };
	unitless::vec4 docking_area::color = { 0.f, 1.f, 0.f, 1.f };

	static constinit bool render_docking_preview = false;
	constexpr float padding = 10.f;
}

auto gse::gui::initialize(renderer::context& context) -> void {
	font_id = context.queue<font>(config::resource_path / "Fonts/MonaspaceNeon-Regular.otf");

	const auto window_size = window::get_window_size();
	const auto top_left = unitless::vec2(0.f, window_size.y);

	docks = {
		docking_area {
			.position = unitless::vec2(0.f, window_size.y / 2 - docking_area::size.y),
			.docked_position = top_left,
			.docked_size = { static_cast<float>(window_size.x) / 2, static_cast<float>(window_size.y) }
		},
		docking_area {
			.position = unitless::vec2(window_size.x / 2 - docking_area::size.x / 2, window_size.y),
			.docked_position = top_left,
			.docked_size = { static_cast<float>(window_size.x), static_cast<float>(window_size.y) / 2 }
		},
		docking_area {
			.position = unitless::vec2(window_size.x - docking_area::size.x, window_size.y / 2 - docking_area::size.y / 2),
			.docked_position = { static_cast<float>(window_size.x) / 2, static_cast<float>(window_size.y) },
			.docked_size = { static_cast<float>(window_size.x) / 2, static_cast<float>(window_size.y) }
		},
		docking_area {
			.position = unitless::vec2(window_size.x / 2 - docking_area::size.x / 2, docking_area::size.y),
			.docked_position = { 0.f, static_cast<float>(window_size.y) / 2 },
			.docked_size = { static_cast<float>(window_size.x), static_cast<float>(window_size.y) / 2 }
		},
		docking_area {
			.position = unitless::vec2(window_size.x / 2 - docking_area::size.x / 2, window_size.y / 2 - docking_area::size.y / 2),
			.docked_position = top_left,
			.docked_size = window_size
		}
	};
}

auto intersects(const gse::unitless::vec2& pos1, const gse::unitless::vec2& size1, const gse::unitless::vec2& pos2, const gse::unitless::vec2& size2) -> bool {
	return pos1.x < pos2.x + size2.x && pos1.x + size1.x > pos2.x && pos1.y > pos2.y - size2.y && pos1.y - size1.y < pos2.y;
}

auto gse::gui::update() -> void {
	for (auto& m : menus) {
		const auto mouse_position = window::get_mouse_position_rel_bottom_left();

		if (input::get_mouse().buttons[GLFW_MOUSE_BUTTON_LEFT].held) {
			if (!m.grabbed.value() && !(m.docked.value() && m.docked_waiting_for_release)) {
				if (intersects(m.position, m.size, mouse_position, { 0.f, 0.f })) {
					m.grabbed = true;
					m.offset = m.position - mouse_position;

					if (m.docked.value() && m.docked.value_mutable() && magnitude(m.offset) > 10.f) {
						m.docked = false;
						m.size = m.pre_docked_size;
						m.offset = {};
						m.position = mouse_position;
					}
				}
			}

			if (m.grabbed.value()) {
				m.position = mouse_position + m.offset;
				m.position = clamp(m.position, unitless::vec2(0.f, 0.f), unitless::vec2(window::get_window_size()));

				for (auto& [position, docked_position, docked_size] : docks) {
					if (intersects(m.position, m.size, position, docking_area::size) &&
						!m.docked.value() && m.docked.value_mutable()) {
						clock hover_clock;

						while (hover_clock.elapsed() < seconds(0.25)) {
							render_docking_preview = true;
							if (!intersects(m.position, m.size, position, docking_area::size)) {
								render_docking_preview = false;
								break;
							}
						}

						m.docked = true;
						m.grabbed = false;
						m.position = docked_position;
						m.size = docked_size;
						m.docked_waiting_for_release = true;

						render_docking_preview = false;
					}
				}
			}
		}
		else {
			m.grabbed = false;
			m.docked_waiting_for_release = false;
		}
	}
}

auto gse::gui::render(renderer::context& context, renderer::sprite& sprite_renderer, renderer::text& text_renderer) -> void {
	for (auto& m : menus) {
		constexpr float font_size = 16.f;
		sprite_renderer.queue({ m.position, m.size, { 1.f, 0.f, 0.f, 1.f } });

		current_menu = &m;
		m.contents();

		const float line_height = context.resource<font>(font_id).font->line_height(font_size);

		text_renderer.draw_text({ font_id, m.name, m.position + unitless::vec2(padding, -padding), font_size, { 1.f, 1.f, 1.f, 1.f } });

		for (size_t i = 0; i < m.items.size(); ++i) {
			unitless::vec2 text_pos = m.position;
			text_pos.x += padding;
			text_pos.y -= padding + ((i + 1) * line_height);

			text_renderer.draw_text({ font_id, m.items[i], text_pos, font_size, { 1.f, 1.f, 1.f, 1.f } });
		}

		if (render_docking_preview) {
			for (const auto& dock : docks) {
				sprite_renderer.queue({ dock.position, docking_area::size, { 0.f, 0.f, 1.f, 1.f } });
			}
		}

		if (m.grabbed.value()) {
			for (const auto& dock : docks) {
				sprite_renderer.queue({ dock.position, docking_area::size, docking_area::color });
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
		.texture_id = {},
		.position = top_left,
		.size = size,
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
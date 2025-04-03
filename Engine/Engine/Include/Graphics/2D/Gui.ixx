module;

#include "GLFW/glfw3.h"

export module gse.graphics.gui;

import std;

import gse.core.config;
import gse.core.clock;
import gse.physics.math;
import gse.core.timed_lock;
import gse.graphics.renderer2d;
import gse.graphics.texture;
import gse.graphics.font;
import gse.platform.glfw.window;
import gse.platform.glfw.input;

export namespace gse::gui {
	auto initialize() -> void;
	auto update() -> void;
	auto render() -> void;
	auto shutdown() -> void;

	auto create_menu(const std::string& name, const unitless::vec2& top_left, const unitless::vec2& size, const std::function<void()>& contents) -> void;

	auto text(const std::string& text) -> void;
}

struct menu {
	gse::texture texture;
	gse::unitless::vec2 position;
	gse::unitless::vec2 size;
	gse::unitless::vec2 pre_docked_size;
	std::string name = "";
	gse::quota_timed_lock<bool, 2> grabbed{ false, gse::seconds(0.05) };
	gse::unitless::vec2 offset;
	gse::timed_lock<bool> docked{ false, gse::seconds(0.25) };
	bool docked_waiting_for_release = false;

	std::function<void()> contents;
};

struct docking_area {
	gse::unitless::vec2 position;
	static gse::unitless::vec2 size;
	static gse::unitless::vec4 color;

	gse::unitless::vec2 docked_position;
	gse::unitless::vec2 docked_size;
};

std::vector<menu> g_menus;
std::array<docking_area, 5> g_docks;

menu* g_current_menu = nullptr;

gse::font g_font;

bool g_render_docking_preview = false;

gse::unitless::vec2 docking_area::size = { 10.f, 10.f };
gse::unitless::vec4 docking_area::color = { 0.f, 1.f, 0.f, 1.f }; // Green

auto gse::gui::initialize() -> void {
	g_font.load(config::resource_path / "Fonts/MonaspaceNeon-Regular.otf");

	const auto window_size = window::get_window_size();
	const auto top_left = unitless::vec2(0.f, window_size.y);

	g_docks = {
		docking_area { // Left
			.position = unitless::vec2(0.f, window_size.y / 2 - docking_area::size.y),
			.docked_position = top_left,
			.docked_size = { static_cast<float>(window_size.x) / 2, static_cast<float>(window_size.y) }
		},
		docking_area { // Top
			.position = unitless::vec2(window_size.x / 2 - docking_area::size.x / 2, window_size.y),
			.docked_position = top_left,
			.docked_size = { static_cast<float>(window_size.x), static_cast<float>(window_size.y) / 2 }
		},
		docking_area { // Right
			.position = unitless::vec2(window_size.x - docking_area::size.x, window_size.y / 2 - docking_area::size.y / 2),
			.docked_position = { static_cast<float>(window_size.x) / 2, static_cast<float>(window_size.y) },
			.docked_size = { static_cast<float>(window_size.x) / 2, static_cast<float>(window_size.y) }
		},
		docking_area { // Bottom
			.position = unitless::vec2(window_size.x / 2 - docking_area::size.x / 2, docking_area::size.y),
			.docked_position = { 0.f, static_cast<float>(window_size.y) / 2 },
			.docked_size = { static_cast<float>(window_size.x), static_cast<float>(window_size.y) / 2 }
		},
		docking_area { // Center
			.position = unitless::vec2(window_size.x / 2 - docking_area::size.x / 2, window_size.y / 2 - docking_area::size.y / 2),
			.docked_position = top_left,
			.docked_size = window_size
		}
	};
}

auto render_docking_preview() -> void {
	for (const auto& dock : g_docks) {
		gse::renderer2d::draw_quad(dock.position, docking_area::size, { 0.f, 0.f, 1.f, 1.f });
	}
}

auto render_docking_visual() {
	for (const auto& dock : g_docks) {
		gse::renderer2d::draw_quad(dock.position, docking_area::size, docking_area::color);
	}
}

auto intersects(const gse::unitless::vec2& pos1, const gse::unitless::vec2& size1, const gse::unitless::vec2& pos2, const gse::unitless::vec2& size2) -> bool {
	return pos1.x < pos2.x + size2.x && pos1.x + size1.x > pos2.x && pos1.y > pos2.y - size2.y && pos1.y - size1.y < pos2.y;
}

auto gse::gui::update() -> void {
	for (auto& m : g_menus) {
		const auto mouse_position = window::get_mouse_position_rel_bottom_left();

		if (input::get_mouse().buttons[GLFW_MOUSE_BUTTON_LEFT].held) {
			if (!m.grabbed.get_value() && !(m.docked.get_value() && m.docked_waiting_for_release)) {
				if (intersects(m.position, m.size, mouse_position, { 0.f, 0.f })) {
					m.grabbed = true;
					m.offset = m.position - mouse_position;

					if (m.docked.get_value() && m.docked.is_value_mutable() && magnitude(m.offset) > 10.f) {
						m.docked = false;
						m.size = m.pre_docked_size;
						m.offset = {};
						m.position = mouse_position;
					}
				}
			}

			if (m.grabbed.get_value()) {
				m.position = mouse_position + m.offset;
				m.position = clamp(m.position, unitless::vec2(0.f, 0.f), unitless::vec2(window::get_window_size()));

				for (auto& dock : g_docks) {
					if (intersects(m.position, m.size, dock.position, docking_area::size) &&
						!m.docked.get_value() && m.docked.is_value_mutable()) {
						clock hover_clock;

						while (hover_clock.get_elapsed_time() < seconds(0.25)) {
							g_render_docking_preview = true;
							if (!intersects(m.position, m.size, dock.position, docking_area::size)) {
								g_render_docking_preview = false;
								break;
							}
						}

						m.docked = true;
						m.grabbed = false;
						m.position = dock.docked_position;
						m.size = dock.docked_size;
						m.docked_waiting_for_release = true;

						g_render_docking_preview = false;
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

auto gse::gui::render() -> void {
	for (const auto& m : g_menus) {
		renderer2d::draw_quad(m.position, m.size, { 1.f, 0.f, 0.f, 1.f });
		m.contents();

		if (g_render_docking_preview) {
			render_docking_preview();
		}

		if (m.grabbed.get_value()) {
			render_docking_visual();
		}
	}
}

auto gse::gui::shutdown() -> void {
	g_menus.clear();
}

auto gse::gui::create_menu(const std::string& name, const unitless::vec2& top_left, const unitless::vec2& size, const std::function<void()>& contents) -> void {
	if (const auto it = std::ranges::find_if(g_menus, [&](const menu& m) { return m.name == name; }); it != g_menus.end()) {
		g_current_menu = &*it;
		return;
	}

	g_menus.push_back({
		.texture = texture(name),
		.position = top_left,
		.size = size,
		.pre_docked_size = size,
		.name = name,
		.contents = contents
		});

	g_current_menu = &g_menus.back();
}


auto gse::gui::text(const std::string& text) -> void {
	renderer2d::draw_text(g_font, text, g_current_menu->position, 1.f, { 1.f, 1.f, 1.f, 1.f });
}

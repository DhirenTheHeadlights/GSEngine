module;

#include "Core/ResourcePaths.h"
#include "GLFW/glfw3.h"

export module gse.graphics.gui;

import std;

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
	gse::quota_timed_lock<bool, 2> grabbed{ false, gse::seconds(0.1)};
	gse::unitless::vec2 offset;
	gse::timed_lock<bool> docked{ true, gse::seconds(0.5) };

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

gse::unitless::vec2 docking_area::size = { 10.f, 10.f };
gse::unitless::vec4 docking_area::color = { 0.f, 1.f, 0.f, 1.f }; // Green

auto gse::gui::initialize() -> void {
	g_font.load(ENGINE_RESOURCES_PATH "Fonts/MonaspaceNeon-Regular.otf");

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
			.docked_size = { static_cast<float>(window_size.x) / 2, static_cast<float>(window_size.y) }
		},
		docking_area { // Right
			.position = unitless::vec2(window_size.x - docking_area::size.x, window_size.y / 2 - docking_area::size.y / 2),
			.docked_position = { static_cast<float>(window_size.x) / 2, static_cast<float>(window_size.y) / 2 },
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

auto render_docking_visual() {
	for (const auto& dock : g_docks) {
		gse::renderer2d::draw_quad(dock.position, docking_area::size, docking_area::color);
	}
}

auto intersects(const gse::unitless::vec2& pos1, const gse::unitless::vec2& size1, const gse::unitless::vec2& pos2, const gse::unitless::vec2& size2) -> bool {
	return pos1.x < pos2.x + size2.x && pos1.x + size1.x > pos2.x && pos1.y > pos2.y - size2.y && pos1.y - size1.y < pos2.y;
}

auto gse::gui::update() -> void {
	for (auto& [texture, position, size, pre_docked_size, name, grabbed, grab_offset, docked, contents] : g_menus) {
		const auto mouse_position = window::get_mouse_position_rel_bottom_left();

		if (input::get_mouse().buttons[GLFW_MOUSE_BUTTON_LEFT].toggled) {
			if (grabbed.get_value() == false) {
				if (intersects(position, size, mouse_position, { 0.f, 0.f })) {
					grabbed = true;
					grab_offset = position - mouse_position;

					if (docked.get_value() == true && docked.is_value_mutable() && magnitude(grab_offset) > 10.f) {
						docked = false;
						size = pre_docked_size;
						position = mouse_position + grab_offset;
					}
				}
			}

			if (grabbed.get_value()) {
				position = mouse_position + grab_offset;
				position = clamp(position, unitless::vec2(0.f, 0.f), unitless::vec2(window::get_window_size()));

				for (auto& dock : g_docks) {
					if (intersects(position, size, dock.position, docking_area::size) && !docked.get_value() && docked.is_value_mutable()) {
						docked = true;
						grabbed = false;
						position = dock.docked_position;
						size = dock.docked_size;
					}
				}
			}
		}
		else {
			grabbed = false;
		}
	}
}

auto gse::gui::render() -> void {
	for (const auto& [texture, position, size, pre_docked_size, name, grabbed, offset, docked, contents] : g_menus) {
		renderer2d::draw_quad(position, size, { 1.f, 0.f, 0.f, 1.f });
		contents();

		if (grabbed.get_value()) {
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

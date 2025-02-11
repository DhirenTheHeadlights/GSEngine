module;

#include "Core/ResourcePaths.h"
#include "GLFW/glfw3.h"

export module gse.graphics.gui;

import std;

import gse.physics.math;

export namespace gse::gui {
	auto initialize() -> void;
	auto update() -> void;
	auto render() -> void;
	auto shutdown() -> void;

	auto create_menu(const std::string& name, const unitless::vec2& position, const unitless::vec2& size, const std::function<void()>& contents) -> void;

	auto text(const std::string& text) -> void;
}

import gse.graphics.renderer2d;
import gse.graphics.texture;
import gse.graphics.font;
import gse.platform.glfw.window;
import gse.platform.glfw.input;

struct menu {
	gse::texture texture;
	gse::unitless::vec2 position;
	gse::unitless::vec2 size;
	std::string name;

	std::function<void()> contents;
};

std::vector<menu> g_menus;

menu* g_current_menu = nullptr;

gse::font g_font;

auto gse::gui::initialize() -> void {
	g_font.load(ENGINE_RESOURCES_PATH "Fonts/MonaspaceNeon-Regular.otf");
}

auto gse::gui::update() -> void {
	for (auto& [texture, position, size, name, contents] : g_menus) {
		if (input::get_mouse().buttons[GLFW_MOUSE_BUTTON_LEFT].pressed) {
			const auto mouse_position = window::get_rel_mouse_position();
			if (mouse_position.x >= position.x && mouse_position.x <= position.x + size.x &&
				mouse_position.y >= position.y && mouse_position.y <= position.y + size.y) {
				position += window::get_mouse_delta();
			}
		}
	}
}

auto gse::gui::render() -> void {
	for (const auto& [texture, position, size, name, contents] : g_menus) {
		renderer2d::draw_quad(position, size, { 1.f, 0.f, 0.f, 1.f });
		contents();
	}
}

auto gse::gui::shutdown() -> void {
	g_menus.clear();
}

auto gse::gui::create_menu(const std::string& name, const unitless::vec2& position, const unitless::vec2& size, const std::function<void()>& contents) -> void {
	if (std::ranges::find_if(g_menus, [&](const menu& m) { return m.name == name; }) != g_menus.end()) {
		return;
	}

	g_menus.push_back({ .texture = texture(name), .position = position, .size = size, .name = name, .contents = contents });
	g_current_menu = &g_menus.back();
}

auto gse::gui::text(const std::string& text) -> void {
	renderer2d::draw_text(g_font, text, g_current_menu->position, 1.f, { 1.f, 1.f, 1.f, 1.f });
}
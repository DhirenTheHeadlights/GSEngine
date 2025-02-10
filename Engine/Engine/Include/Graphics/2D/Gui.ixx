module;

#include "Core/ResourcePaths.h"

export module gse.graphics.gui;

import std;

import gse.physics.math;

export namespace gse::gui {
	auto initialize() -> void;
	auto render() -> void;
	auto shutdown() -> void;

	auto create_menu(const std::string& name, const vec2<length>& position, const gse::vec2<length>& size, const std::function<void()>& contents) -> void;

	auto text(const std::string& text) -> void;
}

import gse.graphics.renderer2d;
import gse.graphics.texture;
import gse.graphics.font;
import gse.platform.glfw.window;
import gse.platform.glfw.input;

struct menu {
	gse::texture texture;
	gse::vec2<gse::length> position;
	gse::vec2<gse::length> size;

	std::function<void()> contents;
};

std::vector<menu> g_menus;

menu* g_current_menu = nullptr;

gse::font g_font;

auto gse::gui::initialize() -> void {
	g_font.load(ENGINE_RESOURCES_PATH "Fonts/MonaspaceNeon-Regular.otf");
}

auto gse::gui::render() -> void {
	for (const auto& [texture, position, size, contents] : g_menus) {
		renderer2d::draw_quad(position, size, { 1.f, 0.f, 0.f, 1.f });
		contents();
	}
}

auto gse::gui::shutdown() -> void {
	g_menus.clear();
}

auto gse::gui::create_menu(const std::string& name, const vec2<length>& position, const vec2<length>& size, const std::function<void()>& contents) -> void {
	g_menus.push_back({ .texture = texture(name), .position = position, .size = size, .contents = contents });
	g_current_menu = &g_menus.back();
}

auto gse::gui::text(const std::string& text) -> void {
	renderer2d::draw_text(g_font, text, g_current_menu->position, 1.f, { 1.f, 1.f, 1.f, 1.f });
}
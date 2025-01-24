module;

#include "Core/ResourcePaths.h"

module gse.graphics.gui;

import std;
import glm;

import gse.graphics.renderer2d;
import gse.graphics.texture;
import gse.graphics.font;
import gse.platform.glfw.window;
import gse.platform.glfw.input;

namespace {
	struct menu {
		gse::texture texture;
		glm::vec2 position;
		glm::vec2 size;

		std::function<void()> contents;
	};

	std::vector<menu> g_menus;

	menu* g_current_menu = nullptr;

	gse::font g_font;
}

void gse::gui::initialize() {
	g_font.load(ENGINE_RESOURCES_PATH "Fonts/MonaspaceNeon-Regular.otf");
}

void gse::gui::render() {
	for (const auto& [texture, position, size, contents] : g_menus) {
		renderer2d::draw_quad(position, size, { 1.f, 0.f, 0.f, 1.f });
		contents();
	}
}

void gse::gui::shutdown() {
	g_menus.clear();
}

void gse::gui::create_menu(const std::string& name, const glm::vec2& position, const glm::vec2& size, const std::function<void()>& contents) {
	g_menus.push_back({ .texture = texture(name), .position = position, .size = size, .contents = contents});
	g_current_menu = &g_menus.back();
}

void gse::gui::text(const std::string& text) {
	renderer2d::draw_text(g_font, text, g_current_menu->position, 1.f, { 1.f, 1.f, 1.f, 1.f });
}
#include "Graphics/2D/Gui.h"

#include <memory>
#include <vector>

#include "Core/ResourcePaths.h"
#include "Graphics/2D/Font.h"
#include "Graphics/2D/Renderer2D.h"
#include "Graphics/2D/Texture.h"
#include "Platform/GLFW/Input.h"
#include "Platform/GLFW/Window.h"

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
	g_font.load(ENGINE_RESOURCES_PATH "Fonts/MonaspaceNeon-Regular.ttf");
}

void gse::gui::render() {
	for (const auto& [texture, position, size, contents] : g_menus) {
		renderer2d::draw_quad(position, size, texture);
		contents();
	}
}

void gse::gui::shutdown() {
	g_menus.clear();
}

void gse::gui::create_menu(const std::string& name, const std::function<void()>& contents, const glm::vec2& position, const glm::vec2& size) {
	g_menus.push_back({ texture(name), position, size, contents });
	g_current_menu = &g_menus.back();
}

void gse::gui::text(const std::string& text) {
	renderer2d::draw_text(g_font, text, { 100, 100 }, 1.f, glm::vec4(1.f));
}
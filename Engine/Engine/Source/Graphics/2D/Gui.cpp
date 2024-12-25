#include "Graphics/2D/Gui.h"

#include <vector>
#include <memory>

#include "Graphics/2D/Texture.h"
#include "Graphics/2D/Renderer2D.h"
#include "Platform/GLFW/Input.h"
#include "Platform/GLFW/Window.h"

namespace {
	struct menu {
		gse::texture texture;
		glm::vec2 position;
		glm::vec2 size;

		std::vector<std::unique_ptr<menu>> submenus;

		void draw() {
			gse::renderer2d::draw_quad(position, size, texture);
			for (const auto& submenu : submenus) {
				submenu->draw();
			}
		}

		void update() {
			for (const auto& submenu : submenus) {
				submenu->update();
			}
		}
	};

	struct root_menu {
		menu root;
		glm::vec2 position;
		glm::vec2 size;
	};

	root_menu g_root_menu;
	menu* current_menu = &g_root_menu.root;
}
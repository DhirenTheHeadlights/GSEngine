module;

#include "GLFW/glfw3.h"

export module gs;

export import game.config;

import std;
import gse;

import :main_test_scene;
import :skybox_scene;
import :second_test_scene;

export namespace gs {
	class game final : public gse::hook<gse::engine> {
	public:
		explicit game(gse::engine* owner) : hook(owner) {}

		auto initialize() -> void  override {
			auto scene1 = std::make_unique<gse::scene>("Scene1");
			scene1->add_hook(std::make_unique<main_test_scene>(scene1.get()));

			auto scene2 = std::make_unique<gse::scene>("Scene2");
			scene2->add_hook(std::make_unique<skybox_scene>(scene2.get()));

			auto scene3 = std::make_unique<gse::scene>("Scene3");
			scene3->add_hook(std::make_unique<second_test_scene>(scene3.get()));

			gse::scene_loader::add(scene1);
			gse::scene_loader::add(scene2);
			gse::scene_loader::add(scene3);

			gse::scene_loader::queue(gse::find("Scene1"), [] { return gse::input::get_keyboard().keys[GLFW_KEY_F1].pressed; });
			gse::scene_loader::queue(gse::find("Scene2"), [] { return gse::input::get_keyboard().keys[GLFW_KEY_F2].pressed; });
			gse::scene_loader::queue(gse::find("Scene3"), [] { return gse::input::get_keyboard().keys[GLFW_KEY_F3].pressed; });
		}

		auto update() -> void  override {
			gse::window::set_mouse_visible(gse::input::get_mouse().buttons[GLFW_MOUSE_BUTTON_MIDDLE].toggled || gse::input::get_keyboard().keys[GLFW_KEY_N].toggled);

			if (gse::input::get_keyboard().keys[GLFW_KEY_ENTER].pressed && gse::input::get_keyboard().keys[GLFW_KEY_LEFT_ALT].held) {
				gse::window::set_full_screen(!gse::window::is_full_screen());
			}

			if (gse::input::get_keyboard().keys[GLFW_KEY_ESCAPE].pressed) {
				gse::shutdown();
			}
		}

		auto render() -> void  override {
			gse::gui::create_menu(
				"Test", { 100.f, 100.f }, { 200.f, 200.f }, [] {
					gse::gui::text("Hello, world!");
					gse::gui::value("Test Value", 42);
					gse::gui::value("Test Quantity", gse::meters(5.0f));
					gse::gui::vec("Test Vec", gse::vec::meters(1.f, 2.f, 3.f));
					gse::gui::vec("Test Vec2", gse::window::get_mouse_position_rel_bottom_left());
				}
			);

			gse::gui::create_menu("Test2", { 300.f, 100.f }, { 200.f, 200.f }, [] {
				gse::gui::vec("Test Vec2", gse::window::get_mouse_position_rel_bottom_left());
				gse::gui::text("Hello, world!");
				gse::gui::value("Test Value", 42);
				gse::gui::value("Test Quantity", gse::meters(5.0f));
				gse::gui::vec("Test Vec", gse::vec::meters(1.f, 2.f, 3.f));
				gse::gui::vec("Test Vec2", gse::window::get_mouse_position_rel_bottom_left());
			});
		}
	};
}

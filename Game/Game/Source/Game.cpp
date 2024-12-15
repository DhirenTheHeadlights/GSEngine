#include "Game.h"

#include <imgui.h>

#include "Arena.h"
#include "Engine.h"
#include "Player.h"
#include "SphereLight.h"

bool g_input_handling_enabled = true;

void game::set_input_handling_flag(const bool enabled) {
	g_input_handling_enabled = enabled;
}

bool game::initialize() {
	auto new_arena   = std::make_unique<arena>();
	auto new_player  = std::make_unique<player>();
	auto new_box     = std::make_unique<gse::box>(gse::vec3<gse::units::meters>(20.f, -400.f, 20.f), gse::vec3<gse::units::meters>(20.f, 20.f, 20.f));
	auto new_box2    = std::make_unique<gse::box>(gse::vec3<gse::units::meters>(-20.f, -400.f, 20.f), gse::vec3<gse::units::meters>(40.f, 40.f, 40.f));
	auto new_sphere  = std::make_unique<sphere_light>(gse::vec3<gse::units::meters>(0.f, -300.f, 0.f), gse::meters(10.f));
	auto new_sphere3 = std::make_unique<gse::sphere>(gse::vec3<gse::units::meters>(0.f, -00.f, 200.f), gse::meters(10.f));

	auto scene1 = std::make_unique<gse::scene>();
	auto scene2 = std::make_unique<gse::scene>();

	scene1->add_object(std::move(new_arena));
	scene1->add_object(std::move(new_player));
	scene1->add_object(std::move(new_box));
	scene1->add_object(std::move(new_box2));
	scene1->add_object(std::move(new_sphere));
	scene1->add_object(std::move(new_sphere3));

	gse::g_scene_handler.add_scene(scene1, "Scene1");
	gse::g_scene_handler.add_scene(scene2, "Scene2");

	gse::g_scene_handler.queue_scene_trigger(gse::grab_id("Scene1").lock(), [] { return true; });

	return true;
}

bool game::update() {
	if (g_input_handling_enabled) {
		gse::window::set_mouse_visible(gse::input::get_mouse().buttons[GLFW_MOUSE_BUTTON_MIDDLE].toggled || gse::input::get_keyboard().keys[GLFW_KEY_N].toggled);

		if (gse::input::get_keyboard().keys[GLFW_KEY_ENTER].pressed && gse::input::get_keyboard().keys[GLFW_KEY_LEFT_ALT].held) {
			gse::window::set_full_screen(!gse::window::is_full_screen());
		}

		if (gse::input::get_keyboard().keys[GLFW_KEY_ESCAPE].pressed) {
			if (close()) {
				std::cerr << "Game closed properly." << '\n';
				return false;
			}
			std::cerr << "Failed to close game properly." << '\n';
			return false;
		}
	}

	return true;
}

bool game::render() {
	gse::debug::add_imgui_callback([] {
		if (gse::g_scene_handler.get_scene(gse::grab_id("Scene1").lock())->get_active()) {
			ImGui::Begin("Game Data");

			ImGui::Text("FPS: %d", gse::main_clock::get_frame_rate());

			ImGui::End();
		}
		});

	return true;
}

bool game::close() {
	return true;
}

#include "Game.h"

#include <imgui.h>

#include "Arena.h"
#include "Engine.h"
#include "Player.h"
#include "SphereLight.h"

std::shared_ptr<game::arena> g_arena;
std::shared_ptr<game::player> g_player;
std::shared_ptr<gse::box> g_box;
std::shared_ptr<gse::box> g_box2;
std::shared_ptr<game::sphere_light> g_sphere;
std::shared_ptr<game::sphere_light> g_sphere2;
std::shared_ptr<gse::sphere> g_sphere3;

bool g_input_handling_enabled = true;

void game::set_input_handling_flag(const bool enabled) {
	g_input_handling_enabled = enabled;
}

bool game::initialize() {
	g_arena  = std::make_shared<arena>();
	g_player = std::make_shared<player>();
	g_box    = std::make_shared<gse::box>(gse::vec3<gse::units::meters>(20.f, -400.f, 20.f), gse::vec3<gse::units::meters>(20.f, 20.f, 20.f));
	g_box2   = std::make_shared<gse::box>(gse::vec3<gse::units::meters>(-20.f, -400.f, 20.f), gse::vec3<gse::units::meters>(40.f, 40.f, 40.f));
	g_sphere = std::make_shared<sphere_light>(gse::vec3<gse::units::meters>(0.f, -300.f, 0.f), gse::meters(10.f));
	//sphere2 = std::make_shared<SphereLight>(Engine::Vec3<Engine::Meters>(0.f, 0.f, 0.f), Engine::meters(1.f));
	g_sphere3 = std::make_shared<gse::sphere>(gse::vec3<gse::units::meters>(0.f, -400.f, 200.f), gse::meters(10.f));

	const auto scene1 = std::make_shared<gse::scene>();

	scene1->add_object(g_arena);
	scene1->add_object(g_player);
	scene1->add_object(g_box);
	scene1->add_object(g_box2);
	scene1->add_object(g_sphere);
	scene1->add_object(g_sphere2);
	scene1->add_object(g_sphere3);

	gse::g_scene_handler.add_scene(scene1, "Scene1");
	gse::g_scene_handler.add_scene(std::make_shared<gse::scene>(), "Scene2");

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

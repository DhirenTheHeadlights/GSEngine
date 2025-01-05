#include "Game.h"

#include <imgui.h>

#include "Arena.h"
#include "Engine.h"
#include "Player.h"
#include "SphereLight.h"
#include "Graphics/2D/Renderer2D.h"

namespace {
	bool g_input_handling_enabled = true;
}

void game::set_input_handling_flag(const bool enabled) {
	g_input_handling_enabled = enabled;
}

struct scene1_hook final : gse::hook<gse::scene> {
	using hook::hook;

	void initialize() override {
		game::arena::create(m_owner);

		m_owner->add_object(std::make_unique<game::player>());
		m_owner->add_object(std::make_unique<gse::box>(gse::vec3<gse::units::meters>(20.f, -400.f, 20.f), gse::vec3<gse::units::meters>(20.f, 20.f, 20.f)));
		m_owner->add_object(std::make_unique<gse::box>(gse::vec3<gse::units::meters>(-20.f, -400.f, 20.f), gse::vec3<gse::units::meters>(40.f, 40.f, 40.f)));
		m_owner->add_object(std::make_unique<game::sphere_light>(gse::vec3<gse::units::meters>(0.f, -300.f, 0.f), gse::meters(10.f)));
		m_owner->add_object(std::make_unique<gse::sphere>(gse::vec3<gse::units::meters>(0.f, -00.f, 200.f), gse::meters(10.f)));
	}

	void render() override {
		gse::debug::add_imgui_callback([] {
			if (gse::scene_loader::get_scene(gse::grab_id("Scene1").lock().get())->get_active()) {
				ImGui::Begin("Game Data");

				ImGui::Text("FPS: %d", gse::main_clock::get_frame_rate());

				ImGui::End();
			}
			});
	}
};

bool game::initialize() {
	auto scene1 = std::make_unique<gse::scene>("Scene1");
	scene1->add_hook(std::make_unique<scene1_hook>(scene1.get()));

	auto scene2 = std::make_unique<gse::scene>("Scene2");

	gse::scene_loader::add_scene(scene1);
	gse::scene_loader::add_scene(scene2);

	gse::scene_loader::queue_scene_trigger(gse::grab_id("Scene1").lock().get(), [] { return true; });

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
	/*gse::gui::create_menu("Test", { 100.f, 100.f }, { 200.f, 200.f }, [] {
		gse::gui::text("Hello, World!");
		});*/
	return true;
}

bool game::close() {
	return true;
}

#include "Game.h"

#include <imgui.h>

#include "Arena.h"
#include "Engine.h"
#include "Player.h"
#include "ResourcePaths.h"
#include "Skybox.h"
#include "SphereLight.h"
#include "Graphics/2D/Renderer2D.h"

namespace {
	bool g_input_handling_enabled = true;
}

auto game::set_input_handling_flag(const bool enabled) -> void {
	g_input_handling_enabled = enabled;
}

struct scene1_hook final : gse::hook<gse::scene> {
	using hook::hook;

	auto initialize() -> void override {
		game::arena::create(m_owner);

		m_owner->add_object(game::create_player(), "Player");
		m_owner->add_object(create_box(gse::vec3<gse::units::meters>(20.f, -400.f, 20.f), gse::vec3<gse::units::meters>(20.f, 20.f, 20.f)), "Bigger Box");
		m_owner->add_object(create_box(gse::vec3<gse::units::meters>(-20.f, -400.f, 20.f), gse::vec3<gse::units::meters>(40.f, 40.f, 40.f)), "Smaller Box");
		m_owner->add_object(game::create_sphere_light(gse::vec3<gse::units::meters>(0.f, -300.f, 0.f), gse::meters(10.f), 18), "Center Sphere Light");
		m_owner->add_object(create_sphere(gse::vec3<gse::units::meters>(0.f, -00.f, 200.f), gse::meters(10.f)), "Second Sphere");

		/*gse::object iron_man;
		gse::registry::add_component(gse::render_component(iron_man.index, GOONSQUAD_RESOURCES_PATH "Models/IronMan/iron_man.obj"));
		gse::registry::get_component<gse::render_component>(iron_man.index).set_all_mesh_material_strings("NULL");
		m_owner->add_object(std::move(iron_man), "Iron Man");*/
	}

	auto render() -> void override {
		gse::debug::add_imgui_callback([] {
			if (gse::scene_loader::get_scene(gse::get_id("Scene1"))->get_active()) {
				ImGui::Begin("Game Data");

				ImGui::Text("FPS: %d", gse::main_clock::get_frame_rate());

				ImGui::End();
			}
			});
	}
};

struct scene2_hook final : gse::hook<gse::scene> {
	struct floor_hook final : hook<gse::object> {
		using hook::hook;

		auto initialize() -> void override {
			gse::registry::get_component<gse::physics::collision_component>(owner_id).resolve_collisions = false;
			gse::registry::get_component<gse::physics::motion_component>(owner_id).affected_by_gravity = false;
		}
	};

	using hook::hook;

	auto initialize() -> void override {
		game::skybox::create(m_owner);
		m_owner->add_object(game::create_player(), "Player");
		gse::object* floor = create_box(gse::vec3<gse::units::meters>(0.f, -500.f, 0.f), gse::vec3<gse::units::meters>(1000.f, 10.f, 1000.f));
		gse::registry::add_object_hook(floor->index, floor_hook());
		m_owner->add_object(floor, "Floor");
	}

	auto render() -> void override {
		gse::registry::get_component<gse::light_source_component>(gse::registry::get_object_id("Floor")).get_lights().front()->show_debug_menu("Skybox Light", gse::registry::get_object_id("Skybox"));
        gse::debug::add_imgui_callback([] {
            if (gse::scene_loader::get_scene(gse::get_id("Scene2"))->get_active()) {
                ImGui::Begin("Game Data");

                ImGui::Text("FPS: %d", gse::main_clock::get_frame_rate());

                ImGui::End();
            }
        });
	}
};

auto game::initialize() -> bool {
	auto scene1 = std::make_unique<gse::scene>("Scene1");
	scene1->add_hook(std::make_unique<scene1_hook>(scene1.get(), scene1->get_id()));

	auto scene2 = std::make_unique<gse::scene>("Scene2");
	scene2->add_hook(std::make_unique<scene2_hook>(scene2.get(), scene2->get_id()));

	gse::scene_loader::add_scene(scene1);
	gse::scene_loader::add_scene(scene2);

	gse::scene_loader::queue_scene_trigger(gse::get_id("Scene1"), [] { return gse::input::get_keyboard().keys[GLFW_KEY_F1].pressed; });
	gse::scene_loader::queue_scene_trigger(gse::get_id("Scene2"), [] { return gse::input::get_keyboard().keys[GLFW_KEY_F2].pressed; });

	return true;
}

auto game::update() -> bool {
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

auto game::render() -> bool {
	/*gse::gui::create_menu("Test", { 100.f, 100.f }, { 200.f, 200.f }, [] {
		gse::gui::text("Hello, World!");
		});*/
	return true;
}

auto game::close() -> bool {
	return true;
}

module;

#include <imgui.h>
#include <GLFW/glfw3.h>

module game;

import gse;
import std;

import game.config;
import game.arena;
import game.player;
import game.skybox;
import game.sphere_light;

bool g_input_handling_enabled = true;

auto game::set_input_handling_flag(const bool enabled) -> void {
	g_input_handling_enabled = enabled;
}

struct iron_man_hook final : gse::hook<gse::entity> {
	using hook::hook;

	auto initialize() -> void override {
		gse::model_loader::load_obj_file(game::config::resource_path / "Models/IronMan/iron_man.obj", "Iron Man");
		gse::registry::add_component(gse::render_component(owner_id, gse::get_id("Iron Man")));
		gse::registry::get_component<gse::render_component>(owner_id).set_model_material("NULL");
		gse::registry::get_component<gse::render_component>(owner_id).models[0].set_position(gse::vec::meters(0.f, 0.f, 0.f));
	}
};

struct black_knight_hook final : gse::hook<gse::entity> {
	using hook::hook;

	auto initialize() -> void override {
		gse::model_loader::load_obj_file(game::config::resource_path / "Models/BlackKnight/base.obj", "Black Knight");
		gse::registry::add_component(gse::render_component(owner_id, gse::get_id("Black Knight")));
		gse::registry::get_component<gse::render_component>(owner_id).set_model_material("NULL");
		gse::registry::get_component<gse::render_component>(owner_id).models[0].set_position(gse::vec::meters(0.f, 0.f, 0.f));
	}
};

struct raw_backpack_hook final : gse::hook<gse::entity> {
	using hook::hook;
	//std::vector<std::uint32_t> m_texture_ids = { gse::texture_loader::get_texture_by_path(game::config::resource_path / "Models/Backpack/diffuse.jpg") };

	auto initialize() -> void override {
		gse::model_loader::load_obj_file(game::config::resource_path / "Models/Backpack/backpack.obj", "Backpack");
		gse::registry::add_component(gse::render_component(owner_id, gse::get_id("Backpack")));
		gse::registry::get_component<gse::render_component>(owner_id).set_model_material("NULL");
		gse::registry::get_component<gse::render_component>(owner_id).models[0].set_position(gse::vec::meters(0.f, 0.f, 0.f));
		//gse::registry::get_component<gse::render_component>(owner_id).models[0].set_all_mesh_textures(m_texture_ids);
	}
};

struct scene1_hook final : gse::hook<gse::scene> {
	using hook::hook;

	auto initialize() -> void override {
		game::arena::create(m_owner);

		m_owner->add_entity(game::create_player(), "Player");
		m_owner->add_entity(create_box(gse::vec::meters(20.f, -400.f, 20.f), gse::vec::meters(20.f, 20.f, 20.f)), "Bigger Box");
		m_owner->add_entity(create_box(gse::vec::meters(-20.f, -400.f, 20.f), gse::vec::meters(40.f, 40.f, 40.f)), "Smaller Box");
		m_owner->add_entity(game::create_sphere_light(gse::vec::meters(0.f, -300.f, 0.f), gse::meters(10.f), 18), "Center Sphere Light");
		m_owner->add_entity(create_sphere(gse::vec::meters(0.f, -00.f, 200.f), gse::meters(10.f)), "Second Sphere");



		const std::uint32_t iron_man = gse::registry::create_entity();
		gse::registry::add_entity_hook(iron_man, std::make_unique<iron_man_hook>());
		m_owner->add_entity(iron_man, "Iron Man");
		//const std::uint32_t raw_backpack = gse::registry::create_entity();
		//gse::registry::add_entity_hook(raw_backpack, std::make_unique<raw_backpack_hook>());
		//m_owner->add_entity(raw_backpack, "Backpack");
		//const std::uint32_t black_knight = gse::registry::create_entity();
		//gse::registry::add_entity_hook(black_knight, std::make_unique<black_knight_hook>());
		//m_owner->add_entity(black_knight, "Black Knight");
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
	struct floor_hook final : hook<gse::entity> {
		using hook::hook;

		auto initialize() -> void override {
			gse::registry::get_component<gse::physics::collision_component>(owner_id).resolve_collisions = false;
			gse::registry::get_component<gse::physics::motion_component>(owner_id).affected_by_gravity = false;
		}
	};

	using hook::hook;

	auto initialize() -> void override {
		game::skybox::create(m_owner);
		m_owner->add_entity(game::create_player(), "Player");
		const std::uint32_t floor_uuid = create_box(gse::vec::meters(0.f, -500.f, 0.f), gse::vec::meters(20000.f, 10.f, 20000.f));
		gse::registry::add_entity_hook(floor_uuid, std::make_unique<floor_hook>());
		m_owner->add_entity(floor_uuid, "Floor");
	}

	auto render() -> void override {
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
	gse::gui::create_menu("Test", { 100.f, 100.f }, { 200.f, 200.f }, [] {
		gse::gui::text("Hello, World!");
		});
	return true;
}

auto game::close() -> bool {
	return true;
}
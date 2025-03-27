export module game.oregon;

import std;
import gse;

const uint32_t max_players = 6;


export namespace game::oregon {
	

}

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
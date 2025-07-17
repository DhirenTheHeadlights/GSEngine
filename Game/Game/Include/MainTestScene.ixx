module;

#include "imgui.h"

export module gs:main_test_scene;

import gse;

export namespace gs {
	class main_test_scene final : gse::hook<gse::scene> {
	public:
		using hook::hook;

		auto initialize() -> void override {
			game::arena::create(m_owner);

			m_owner->add_entity(game::create_player(), "Player");
			m_owner->registry().add_link<box_mesh>(m_owner->add_entity("Smaller Box"), this, gse::vec::meters(20.f, -400.f, 20.f), gse::vec::meters(20.f, 20.f, 20.f));
			m_owner->add_entity(gse::create_box(gse::vec::meters(20.f, -400.f, 20.f), gse::vec::meters(20.f, 20.f, 20.f)), "Smaller Box");
			m_owner->add_entity(gse::create_box(gse::vec::meters(-20.f, -400.f, 20.f), gse::vec::meters(40.f, 40.f, 40.f)), "Bigger Box");
			m_owner->add_entity(game::create_sphere_light(gse::vec::meters(0.f, -300.f, 0.f), gse::meters(10.f), 18), "Center Sphere Light");
			m_owner->add_entity(gse::create_sphere(gse::vec::meters(0.f, -00.f, 200.f), gse::meters(10.f)), "Second Sphere");
		}

		auto render() -> void override {
			gse::debug::add_imgui_callback(
				[] {
					if (gse::scene_loader::scene(gse::find("Scene1"))->active()) {
						ImGui::Begin("Game Data");

						ImGui::Text("FPS: %d", gse::main_clock::frame_rate());

						ImGui::End();
					}
				}
			);
		}
	};
} 
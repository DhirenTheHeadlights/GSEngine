module;

#include "GLFW/glfw3.h"
#include "imgui.h"

export module gs:player;

import gse;
import std;

namespace gs {
	const std::unordered_map<int, gse::unitless::vec3> g_wasd{
		{ GLFW_KEY_W, { 0.f, 0.f, 1.f } },
		{ GLFW_KEY_S, { 0.f, 0.f, -1.f } },
		{ GLFW_KEY_A, { -1.f, 0.f, 0.f } },
		{ GLFW_KEY_D, { 1.f, 0.f, 0.f } }
	};

	class jetpack_hook final : public gse::hook<gse::entity> {
	public:
		using hook::hook;

		auto update() -> void override {
			if (gse::input::get_keyboard().keys[GLFW_KEY_J].pressed) {
				m_jetpack = !m_jetpack;
			}

			if (m_jetpack && gse::input::get_keyboard().keys[GLFW_KEY_SPACE].held) {
				gse::force boost_force;
				if (gse::input::get_keyboard().keys[GLFW_KEY_LEFT_SHIFT].held && m_boost_fuel > 0) {
					boost_force = gse::newtons(2000.f);
					m_boost_fuel -= 1;
				}
				else {
					m_boost_fuel += 1;
					m_boost_fuel = std::min(m_boost_fuel, 1000);
				}

				auto motion_component = component<gse::physics::motion_component>();

				apply_force(motion_component, gse::vec3<gse::force>(0.f, m_jetpack_force + boost_force, 0.f));

				for (auto& [key, direction] : g_wasd) {
					if (gse::input::get_keyboard().keys[key].held) {
						apply_force(motion_component, gse::vec3<gse::force>(m_jetpack_side_force + boost_force, 0.f, m_jetpack_side_force + boost_force) * gse::renderer::camera().direction_relative_to_origin(direction));
					}
				}
			}
		}

		auto render() -> void override {
			gse::debug::add_imgui_callback([this] {
				ImGui::Begin("Jetpack");
				gse::debug::print_boolean("Player Jetpack", m_jetpack);
				gse::debug::print_value("Jetpack Fuel", static_cast<float>(m_boost_fuel), "");
				ImGui::End();
				});
		}
	private:
		gse::force m_jetpack_force = gse::newtons(1000.f);
		gse::force m_jetpack_side_force = gse::newtons(500.f);

		int m_boost_fuel = 1000;
		bool m_jetpack = false;
	};

	class player_hook final : public gse::hook<gse::entity> {
	public:
		using hook::hook;

		auto initialize() -> void override {
			
			const auto [mc_id, mc] = add_component<gse::physics::motion_component>({
				.current_position = gse::vec3<gse::length>(0.f, 0.f, 0.f),
				.max_speed = m_max_speed,
				.mass = gse::pounds(180.f),
				.self_controlled = true,
			});

			gse::length height = gse::feet(6.0f);
			gse::length width = gse::feet(3.0f);

			const auto [cc_id, cc] = add_component<gse::physics::collision_component>({
				.bounding_box = { gse::vec::meters(-10.f, -10.f, -10.f), height, width, width }
			});
			cc->oriented_bounding_box = { cc->bounding_box };

			const auto [rc_id, rc] = add_component<gse::render_component>({
				.bounding_box_meshes = { gse::generate_bounding_box_mesh(cc->bounding_box.upper_bound, cc->bounding_box.lower_bound) }
			});
		}

		auto update() -> void override {
			auto& motion_component = component<gse::physics::motion_component>();

			for (auto& [key, direction] : g_wasd) {
				if (gse::input::get_keyboard().keys[key].held && !motion_component.airborne) {
					apply_force(motion_component, m_move_force * gse::renderer::camera().direction_relative_to_origin(direction) * gse::unitless::vec3(1.f, 0.f, 1.f));
				}
			}

			motion_component.max_speed = gse::input::get_keyboard().keys[GLFW_KEY_LEFT_SHIFT].held ? m_shift_max_speed : m_max_speed;

			if (gse::input::get_keyboard().keys[GLFW_KEY_SPACE].pressed && !motion_component.airborne) {
				apply_impulse(motion_component, gse::vec3<gse::force>(0.f, m_jump_force, 0.f), gse::seconds(0.5f));
			}

			gse::renderer::camera().set_position(motion_component.current_position + gse::vec::feet(0.f, 6.f, 0.f));
		}

		auto render() -> void override {
			gse::debug::add_imgui_callback([this] {
				ImGui::Begin("Player");

				const auto motion_component = component<gse::physics::motion_component>();
				const auto collision_component = component<gse::physics::collision_component>();

				gse::debug::print_vector("Player Position", motion_component.current_position.as<gse::units::meters>(), gse::units::meters::unit_name);
				gse::debug::print_vector("Player Bounding Box Position", collision_component.bounding_box.get_center().as<gse::units::meters>(), gse::units::meters::unit_name);
				gse::debug::print_vector("Player Velocity", motion_component.current_velocity.as<gse::units::meters_per_second>(), gse::units::meters_per_second::unit_name);
				gse::debug::print_vector("Player Acceleration", motion_component.current_acceleration.as<gse::units::meters_per_second_squared>(), gse::units::meters_per_second_squared::unit_name);

				gse::debug::print_value("Player Speed", gse::magnitude(motion_component.current_velocity.as<gse::units::miles_per_hour>()), gse::units::miles_per_hour::unit_name);

				ImGui::Text("Player Collision Information");

				const auto [colliding, collision_normal, penetration, collision_point] = collision_component.collision_information;
				gse::debug::print_boolean("Player Colliding", colliding);
				gse::debug::print_vector("Collision Normal", collision_normal, "");
				gse::debug::print_value("Penetration", penetration.as<gse::units::meters>(), gse::units::meters::unit_name);
				gse::debug::print_vector("Collision Point", collision_point.as<gse::units::meters>(), gse::units::meters::unit_name);
				gse::debug::print_boolean("Player Airborne", motion_component.airborne);
				gse::debug::print_boolean("Player Moving", motion_component.moving);
				ImGui::End();
				});
		}
	private:
		gse::velocity m_max_speed = gse::miles_per_hour(20.f);
		gse::velocity m_shift_max_speed = gse::miles_per_hour(40.f);

		gse::force m_move_force = gse::newtons(100000.f);
		gse::force m_jump_force = gse::newtons(1000.f);
	};

	class player final : public gse::hook<gse::entity> {
	public:
		using hook::hook;
		auto initialize() -> void override {
			add_hook<jetpack_hook>();
			add_hook<player_hook>();
		}
	};
}
#include "Player.h"

#include "Engine.h"

struct game::jetpack_hook final : gse::hook<player> {
	using hook::hook;

	void update() override {
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

			auto& motion_component = gse::registry::get_component<gse::physics::motion_component>(m_id);

			apply_force(motion_component, gse::vec3<gse::units::newtons>(0.f, m_jetpack_force + boost_force, 0.f));

			for (auto& [key, direction] : m_owner->m_wasd) {
				if (gse::input::get_keyboard().keys[key].held) {
					apply_force(gse::registry::get_component<gse::physics::motion_component>(m_id), gse::vec3<gse::units::newtons>(m_jetpack_side_force + boost_force, 0.f, m_jetpack_side_force + boost_force) * gse::get_camera().get_camera_direction_relative_to_origin(direction));
				}
			}
		}
	}

	void render() override {
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

struct game::player_hook final : gse::hook<player> {
	using hook::hook;

	void initialize() override {
		gse::render_component render_component(m_id);
		gse::physics::motion_component motion_component(m_id);
		gse::physics::collision_component collision_component(m_id);

		gse::length height = gse::feet(6.0f);
		gse::length width = gse::feet(3.0f);
		collision_component.bounding_boxes.emplace_back(gse::vec3<gse::units::meters>(-10.f, -10.f, -10.f), height, width, width);

		m_owner->m_wasd.insert({ GLFW_KEY_W, { 0.f, 0.f, 1.f } });
		m_owner->m_wasd.insert({ GLFW_KEY_S, { 0.f, 0.f, -1.f } });
		m_owner->m_wasd.insert({ GLFW_KEY_A, { -1.f, 0.f, 0.f } });
		m_owner->m_wasd.insert({ GLFW_KEY_D, { 1.f, 0.f, 0.f } });

		motion_component.mass = gse::pounds(180.f);
		motion_component.max_speed = m_max_speed;
		motion_component.self_controlled = true;

		for (auto& bb : collision_component.bounding_boxes) {
			auto bounding_box_mesh = std::make_unique<gse::bounding_box_mesh>(bb.upper_bound, bb.lower_bound);
			render_component.add_bounding_box_mesh(std::move(bounding_box_mesh));
		}

		render_component.set_render(true, true);

		gse::registry::add_component<gse::render_component>(std::move(render_component));
		gse::registry::add_component<gse::physics::motion_component>(std::move(motion_component));
		gse::registry::add_component<gse::physics::collision_component>(std::move(collision_component));
	}

	void update() override {
		auto& motion_component = gse::registry::get_component<gse::physics::motion_component>(m_id);

		for (auto& [key, direction] : m_owner->m_wasd) {
			if (gse::input::get_keyboard().keys[key].held && !motion_component.airborne) {
				apply_force(motion_component, m_move_force * gse::get_camera().get_camera_direction_relative_to_origin(direction) * gse::vec3(1.f, 0.f, 1.f));
			}
		}

		if (gse::input::get_keyboard().keys[GLFW_KEY_LEFT_SHIFT].held) {
			motion_component.max_speed = m_shift_max_speed;
		}
		else {
			motion_component.max_speed = m_max_speed;
		}

		if (gse::input::get_keyboard().keys[GLFW_KEY_SPACE].pressed && !motion_component.airborne) {
			apply_impulse(motion_component, gse::vec3<gse::units::newtons>(0.f, m_jump_force, 0.f), gse::seconds(0.5f));
		}

		for (auto& bb : gse::registry::get_component<gse::physics::collision_component>(m_id).bounding_boxes) {
			bb.set_position(motion_component.current_position);
		}

		gse::get_camera().set_position(motion_component.current_position + gse::vec3<gse::units::feet>(0.f, 6.f, 0.f));

		gse::registry::get_component<gse::render_component>(m_id).update_bounding_box_meshes();
	}

	void render() override {
		gse::debug::add_imgui_callback([this] {
			ImGui::Begin("Player");

			const auto motion_component = gse::registry::get_component<gse::physics::motion_component>(m_id);
			const auto collision_component = gse::registry::get_component<gse::physics::collision_component>(m_id);

			gse::debug::print_vector("Player Position", motion_component.current_position.as<gse::units::meters>(), gse::units::meters::unit_name);
			gse::debug::print_vector("Player Bounding Box Position", collision_component.bounding_boxes[0].get_center().as<gse::units::meters>(), gse::units::meters::unit_name);
			gse::debug::print_vector("Player Velocity", motion_component.current_velocity.as<gse::units::meters_per_second>(), gse::units::meters_per_second::unit_name);
			gse::debug::print_vector("Player Acceleration", motion_component.current_acceleration.as<gse::units::meters_per_second_squared>(), gse::units::meters_per_second_squared::unit_name);

			gse::debug::print_value("Player Speed", motion_component.get_speed().as<gse::units::miles_per_hour>(), gse::units::miles_per_hour::unit_name);

			ImGui::Text("Player Collision Information");

			const auto [colliding, collision_normal, penetration, collision_point] = collision_component.bounding_boxes[0].collision_information;
			gse::debug::print_boolean("Player Colliding", colliding);
			gse::debug::print_vector("Collision Normal", collision_normal.as_default_units(), "");
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

game::player::player() : object("Player") {
	add_hook(std::make_unique<player_hook>(this));
	add_hook(std::make_unique<jetpack_hook>(this));
}

#include "Player.h"

void game::player::initialize() {
	const auto motion_component = std::make_shared<gse::physics::motion_component>(m_id.get());
	const auto collision_component = std::make_shared<gse::physics::collision_component>(m_id.get());
	const auto render_component = std::make_shared<gse::render_component>(m_id.get());

	gse::length height = gse::feet(6.0f);
	gse::length width = gse::feet(3.0f);
	collision_component->bounding_boxes.emplace_back(gse::vec3<gse::units::meters>(-10.f, -10.f, -10.f), height, width, width);

	m_wasd.insert({ GLFW_KEY_W, { 0.f, 0.f, 1.f } });
	m_wasd.insert({ GLFW_KEY_S, { 0.f, 0.f, -1.f } });
	m_wasd.insert({ GLFW_KEY_A, { -1.f, 0.f, 0.f } });
	m_wasd.insert({ GLFW_KEY_D, { 1.f, 0.f, 0.f } });

	motion_component->mass = gse::pounds(180.f);
	motion_component->max_speed = m_max_speed;
	motion_component->self_controlled = true;

	for (auto& bb : collision_component->bounding_boxes) {
		const auto bounding_box_mesh = std::make_shared<gse::bounding_box_mesh>(bb.upper_bound, bb.lower_bound);
		render_component->add_bounding_box_mesh(bounding_box_mesh);
	}

	add_component(render_component);
	add_component(motion_component);
	add_component(collision_component);
}

void game::player::update_jetpack() {
	if (gse::input::get_keyboard().keys[GLFW_KEY_J].pressed) {
		jetpack = !jetpack;
	}

	if (jetpack && gse::input::get_keyboard().keys[GLFW_KEY_SPACE].held) {
		gse::force boost_force;
		if (gse::input::get_keyboard().keys[GLFW_KEY_LEFT_SHIFT].held && m_boost_fuel > 0) {
			boost_force = gse::newtons(2000.f);
			m_boost_fuel -= 1;
		}
		else {
			m_boost_fuel += 1;
			m_boost_fuel = std::min(m_boost_fuel, 1000);
		}

		apply_force(get_component<gse::physics::motion_component>().get(), gse::vec3<gse::units::newtons>(0.f, m_jetpack_force + boost_force, 0.f));

		for (auto& [key, direction] : m_wasd) {
			if (gse::input::get_keyboard().keys[key].held) {
				apply_force(get_component<gse::physics::motion_component>().get(), gse::vec3<gse::units::newtons>(m_jetpack_side_force + boost_force, 0.f, m_jetpack_side_force + boost_force) * gse::get_camera().get_camera_direction_relative_to_origin(direction));
			}
		}
	}
}

void game::player::update_movement() {
	for (auto& [key, direction] : m_wasd) {
		if (gse::input::get_keyboard().keys[key].held && !get_component<gse::physics::motion_component>()->airborne) {
			apply_force(get_component<gse::physics::motion_component>().get(), m_move_force * gse::get_camera().get_camera_direction_relative_to_origin(direction) * gse::vec3(1.f, 0.f, 1.f));
		}
	}

	if (gse::input::get_keyboard().keys[GLFW_KEY_LEFT_SHIFT].held) {
		get_component<gse::physics::motion_component>()->max_speed = m_shift_max_speed;
	}
	else {
		get_component<gse::physics::motion_component>()->max_speed = m_max_speed;
	}

	if (gse::input::get_keyboard().keys[GLFW_KEY_SPACE].pressed && !get_component<gse::physics::motion_component>()->airborne) {
		apply_impulse(get_component<gse::physics::motion_component>().get(), gse::vec3<gse::units::newtons>(0.f, m_jump_force, 0.f), gse::seconds(0.5f));
		get_component<gse::physics::motion_component>()->airborne = true;
	}
}

void game::player::update() {
	update_jetpack();
	update_movement();

	for (auto& bb : get_component<gse::physics::collision_component>()->bounding_boxes) {
		bb.set_position(get_component<gse::physics::motion_component>()->current_position);
	}
	
	gse::get_camera().set_position(get_component<gse::physics::motion_component>()->current_position + gse::vec3<gse::units::feet>(0.f, 6.f, 0.f));

	get_component<gse::render_component>()->update_bounding_box_meshes();
	get_component<gse::render_component>()->set_render(true, true);
}

void game::player::render() {
	gse::debug::add_imgui_callback([this] {
		ImGui::Begin("Player");
		gse::debug::print_vector("Player Position", get_component<gse::physics::motion_component>()->current_position.as<gse::units::meters>(), gse::units::meters::unit_name);
		gse::debug::print_vector("Player Bounding Box Position", get_component<gse::physics::collision_component>()->bounding_boxes[0].get_center().as<gse::units::meters>(), gse::units::meters::unit_name);
		gse::debug::print_vector("Player Velocity", get_component<gse::physics::motion_component>()->current_velocity.as<gse::units::meters_per_second>(), gse::units::meters_per_second::unit_name);
		gse::debug::print_vector("Player Acceleration", get_component<gse::physics::motion_component>()->current_acceleration.as<gse::units::meters_per_second_squared>(), gse::units::meters_per_second_squared::unit_name);

		gse::debug::print_value("Player Speed", get_component<gse::physics::motion_component>()->get_speed().as<gse::units::miles_per_hour>(), gse::units::miles_per_hour::unit_name);

		gse::debug::print_boolean("Player Jetpack [J]", jetpack);
		gse::debug::print_value("Player Boost Fuel", static_cast<float>(m_boost_fuel), "");

		ImGui::Text("Player Collision Information");

		const auto [colliding, collisionNormal, penetration, collisionPoint] = get_component<gse::physics::collision_component>()->bounding_boxes[0].collision_information;
		gse::debug::print_boolean("Player Colliding", colliding);
		gse::debug::print_vector("Collision Normal", collisionNormal.as_default_units(), "");
		gse::debug::print_value("Penetration", penetration.as<gse::units::meters>(), gse::units::meters::unit_name);
		gse::debug::print_vector("Collision Point", collisionPoint.as<gse::units::meters>(), gse::units::meters::unit_name);
		gse::debug::print_boolean("Player Airborne", get_component<gse::physics::motion_component>()->airborne);
		gse::debug::print_boolean("Player Moving", get_component<gse::physics::motion_component>()->moving);
		ImGui::End();
		});
}

#include "Player.h"

void Game::Player::initialize() {
	const auto motionComponent = std::make_shared<gse::physics::motion_component>(m_id.get());
	const auto collisionComponent = std::make_shared<gse::physics::collision_component>(m_id.get());
	const auto renderComponent = std::make_shared<gse::render_component>(m_id.get());

	gse::length height = gse::feet(6.0f);
	gse::length width = gse::feet(3.0f);
	collisionComponent->bounding_boxes.emplace_back(gse::vec3<gse::Meters>(-10.f, -10.f, -10.f), height, width, width);

	wasd.insert({ GLFW_KEY_W, { 0.f, 0.f, 1.f } });
	wasd.insert({ GLFW_KEY_S, { 0.f, 0.f, -1.f } });
	wasd.insert({ GLFW_KEY_A, { -1.f, 0.f, 0.f } });
	wasd.insert({ GLFW_KEY_D, { 1.f, 0.f, 0.f } });

	motionComponent->mass = gse::pounds(180.f);
	motionComponent->max_speed = maxSpeed;
	motionComponent->self_controlled = true;

	for (auto& bb : collisionComponent->bounding_boxes) {
		const auto boundingBoxMesh = std::make_shared<gse::bounding_box_mesh>(bb.upper_bound, bb.lower_bound);
		renderComponent->add_bounding_box_mesh(boundingBoxMesh);
	}

	add_component(renderComponent);
	add_component(motionComponent);
	add_component(collisionComponent);
}

void Game::Player::updateJetpack() {
	if (gse::input::get_keyboard().keys[GLFW_KEY_J].pressed) {
		jetpack = !jetpack;
	}

	if (jetpack && gse::input::get_keyboard().keys[GLFW_KEY_SPACE].held) {
		gse::force boostForce;
		if (gse::input::get_keyboard().keys[GLFW_KEY_LEFT_SHIFT].held && boostFuel > 0) {
			boostForce = gse::newtons(2000.f);
			boostFuel -= 1;
		}
		else {
			boostFuel += 1;
			boostFuel = std::min(boostFuel, 1000);
		}

		applyForce(get_component<gse::physics::motion_component>().get(), gse::vec3<gse::newtons>(0.f, jetpackForce + boostForce, 0.f));

		for (auto& [key, direction] : wasd) {
			if (gse::input::get_keyboard().keys[key].held) {
				apply_force(get_component<gse::physics::motion_component>().get(), gse::vec3<gse::newtons>(jetpackSideForce + boostForce, 0.f, jetpackSideForce + boostForce) * gse::get_camera().get_camera_direction_relative_to_origin(direction));
			}
		}
	}
}

void Game::Player::updateMovement() {
	for (auto& [key, direction] : wasd) {
		if (gse::input::get_keyboard().keys[key].held && !get_component<gse::physics::motion_component>()->airborne) {
			apply_force(get_component<gse::physics::motion_component>().get(), moveForce * gse::get_camera().get_camera_direction_relative_to_origin(direction) * gse::vec3(1.f, 0.f, 1.f));
		}
	}

	if (gse::input::get_keyboard().keys[GLFW_KEY_LEFT_SHIFT].held) {
		get_component<gse::physics::motion_component>()->max_speed = shiftMaxSpeed;
	}
	else {
		get_component<gse::physics::motion_component>()->max_speed = maxSpeed;
	}

	if (gse::input::get_keyboard().keys[GLFW_KEY_SPACE].pressed && !get_component<gse::physics::motion_component>()->airborne) {
		applyImpulse(get_component<gse::physics::motion_component>().get(), gse::vec3<gse::newtons>(0.f, jumpForce, 0.f), gse::seconds(0.5f));
		get_component<gse::physics::motion_component>()->airborne = true;
	}
}

void Game::Player::update() {
	updateJetpack();
	updateMovement();

	for (auto& bb : get_component<gse::physics::collision_component>()->bounding_boxes) {
		bb.set_position(get_component<gse::physics::motion_component>()->current_position);
	}
	
	gse::get_camera().set_position(get_component<gse::physics::motion_component>()->current_position + gse::vec3<gse::Feet>(0.f, 6.f, 0.f));

	get_component<gse::render_component>()->update_bounding_box_meshes();
	get_component<gse::render_component>()->set_render(true, true);
}

void Game::Player::render() {
	gse::debug::add_imgui_callback([this] {
		ImGui::Begin("Player");
		gse::debug::print_vector("Player Position", get_component<gse::physics::motion_component>()->current_position.as<gse::Meters>(), gse::Meters::UnitName);
		gse::debug::print_vector("Player Bounding Box Position", get_component<gse::physics::collision_component>()->bounding_boxes[0].get_center().as<gse::Meters>(), gse::Meters::UnitName);
		gse::debug::print_vector("Player Velocity", get_component<gse::physics::motion_component>()->current_velocity.as<gse::meters_per_second>(), gse::meters_per_second::unit_name);
		gse::debug::print_vector("Player Acceleration", get_component<gse::physics::motion_component>()->current_acceleration.as<gse::meters_per_second_squared>(), gse::meters_per_second_squared::unit_name);

		gse::debug::print_value("Player Speed", get_component<gse::physics::motion_component>()->get_speed().as<gse::miles_per_hour>(), gse::miles_per_hour::unit_name);

		gse::debug::print_boolean("Player Jetpack [J]", jetpack);
		gse::debug::print_value("Player Boost Fuel", static_cast<float>(boostFuel), "");

		ImGui::Text("Player Collision Information");

		const auto [colliding, collisionNormal, penetration, collisionPoint] = get_component<gse::physics::collision_component>()->bounding_boxes[0].collision_information;
		gse::debug::print_boolean("Player Colliding", colliding);
		gse::debug::print_vector("Collision Normal", collisionNormal.as_default_units(), "");
		gse::debug::print_value("Penetration", penetration.as<gse::Meters>(), gse::Meters::UnitName);
		gse::debug::print_vector("Collision Point", collisionPoint.as<gse::Meters>(), gse::Meters::UnitName);
		gse::debug::print_boolean("Player Airborne", get_component<gse::physics::motion_component>()->airborne);
		gse::debug::print_boolean("Player Moving", get_component<gse::physics::motion_component>()->moving);
		ImGui::End();
		});
}

#include "Player.h"

void Game::Player::initialize() {
	const auto motionComponent = std::make_shared<gse::Physics::MotionComponent>(id.get());
	const auto collisionComponent = std::make_shared<gse::Physics::CollisionComponent>(id.get());
	const auto renderComponent = std::make_shared<gse::RenderComponent>(id.get());

	gse::Length height = gse::feet(6.0f);
	gse::Length width = gse::feet(3.0f);
	collisionComponent->boundingBoxes.emplace_back(gse::Vec3<gse::Meters>(-10.f, -10.f, -10.f), height, width, width);

	wasd.insert({ GLFW_KEY_W, { 0.f, 0.f, 1.f } });
	wasd.insert({ GLFW_KEY_S, { 0.f, 0.f, -1.f } });
	wasd.insert({ GLFW_KEY_A, { -1.f, 0.f, 0.f } });
	wasd.insert({ GLFW_KEY_D, { 1.f, 0.f, 0.f } });

	motionComponent->mass = gse::pounds(180.f);
	motionComponent->maxSpeed = maxSpeed;
	motionComponent->selfControlled = true;

	for (auto& bb : collisionComponent->boundingBoxes) {
		const auto boundingBoxMesh = std::make_shared<gse::BoundingBoxMesh>(bb.upperBound, bb.lowerBound);
		renderComponent->addBoundingBoxMesh(boundingBoxMesh);
	}

	addComponent(renderComponent);
	addComponent(motionComponent);
	addComponent(collisionComponent);
}

void Game::Player::updateJetpack() {
	if (gse::Input::getKeyboard().keys[GLFW_KEY_J].pressed) {
		jetpack = !jetpack;
	}

	if (jetpack && gse::Input::getKeyboard().keys[GLFW_KEY_SPACE].held) {
		gse::Force boostForce;
		if (gse::Input::getKeyboard().keys[GLFW_KEY_LEFT_SHIFT].held && boostFuel > 0) {
			boostForce = gse::newtons(2000.f);
			boostFuel -= 1;
		}
		else {
			boostFuel += 1;
			boostFuel = std::min(boostFuel, 1000);
		}

		applyForce(getComponent<gse::Physics::MotionComponent>().get(), gse::Vec3<gse::Newtons>(0.f, jetpackForce + boostForce, 0.f));

		for (auto& [key, direction] : wasd) {
			if (gse::Input::getKeyboard().keys[key].held) {
				applyForce(getComponent<gse::Physics::MotionComponent>().get(), gse::Vec3<gse::Newtons>(jetpackSideForce + boostForce, 0.f, jetpackSideForce + boostForce) * gse::get_camera().getCameraDirectionRelativeToOrigin(direction));
			}
		}
	}
}

void Game::Player::updateMovement() {
	for (auto& [key, direction] : wasd) {
		if (gse::Input::getKeyboard().keys[key].held && !getComponent<gse::Physics::MotionComponent>()->airborne) {
			applyForce(getComponent<gse::Physics::MotionComponent>().get(), moveForce * gse::get_camera().getCameraDirectionRelativeToOrigin(direction) * gse::Vec3(1.f, 0.f, 1.f));
		}
	}

	if (gse::Input::getKeyboard().keys[GLFW_KEY_LEFT_SHIFT].held) {
		getComponent<gse::Physics::MotionComponent>()->maxSpeed = shiftMaxSpeed;
	}
	else {
		getComponent<gse::Physics::MotionComponent>()->maxSpeed = maxSpeed;
	}

	if (gse::Input::getKeyboard().keys[GLFW_KEY_SPACE].pressed && !getComponent<gse::Physics::MotionComponent>()->airborne) {
		applyImpulse(getComponent<gse::Physics::MotionComponent>().get(), gse::Vec3<gse::Newtons>(0.f, jumpForce, 0.f), gse::seconds(0.5f));
		getComponent<gse::Physics::MotionComponent>()->airborne = true;
	}
}

void Game::Player::update() {
	updateJetpack();
	updateMovement();

	for (auto& bb : getComponent<gse::Physics::CollisionComponent>()->boundingBoxes) {
		bb.setPosition(getComponent<gse::Physics::MotionComponent>()->position);
	}
	
	gse::get_camera().setPosition(getComponent<gse::Physics::MotionComponent>()->position + gse::Vec3<gse::Feet>(0.f, 6.f, 0.f));

	getComponent<gse::RenderComponent>()->updateBoundingBoxMeshes();
	getComponent<gse::RenderComponent>()->setRender(true, true);
}

void Game::Player::render() {
	gse::Debug::addImguiCallback([this] {
		ImGui::Begin("Player");
		gse::Debug::printVector("Player Position", getComponent<gse::Physics::MotionComponent>()->position.as<gse::Meters>(), gse::Meters::UnitName);
		gse::Debug::printVector("Player Bounding Box Position", getComponent<gse::Physics::CollisionComponent>()->boundingBoxes[0].getCenter().as<gse::Meters>(), gse::Meters::UnitName);
		gse::Debug::printVector("Player Velocity", getComponent<gse::Physics::MotionComponent>()->velocity.as<gse::MetersPerSecond>(), gse::MetersPerSecond::UnitName);
		gse::Debug::printVector("Player Acceleration", getComponent<gse::Physics::MotionComponent>()->acceleration.as<gse::MetersPerSecondSquared>(), gse::MetersPerSecondSquared::UnitName);

		gse::Debug::printValue("Player Speed", getComponent<gse::Physics::MotionComponent>()->getSpeed().as<gse::MilesPerHour>(), gse::MilesPerHour::UnitName);

		gse::Debug::printBoolean("Player Jetpack [J]", jetpack);
		gse::Debug::printValue("Player Boost Fuel", static_cast<float>(boostFuel), "");

		ImGui::Text("Player Collision Information");

		const auto [colliding, collisionNormal, penetration, collisionPoint] = getComponent<gse::Physics::CollisionComponent>()->boundingBoxes[0].collisionInformation;
		gse::Debug::printBoolean("Player Colliding", colliding);
		gse::Debug::printVector("Collision Normal", collisionNormal.asDefaultUnits(), "");
		gse::Debug::printValue("Penetration", penetration.as<gse::Meters>(), gse::Meters::UnitName);
		gse::Debug::printVector("Collision Point", collisionPoint.as<gse::Meters>(), gse::Meters::UnitName);
		gse::Debug::printBoolean("Player Airborne", getComponent<gse::Physics::MotionComponent>()->airborne);
		gse::Debug::printBoolean("Player Moving", getComponent<gse::Physics::MotionComponent>()->moving);
		ImGui::End();
		});
}

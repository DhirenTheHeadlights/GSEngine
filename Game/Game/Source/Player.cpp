#include "Player.h"

void Game::Player::initialize() {
	const auto motionComponent = std::make_shared<Engine::Physics::MotionComponent>(id.get());
	const auto collisionComponent = std::make_shared<Engine::Physics::CollisionComponent>(id.get());
	const auto renderComponent = std::make_shared<Engine::RenderComponent>(id.get());

	Engine::Length height = Engine::feet(6.0f);
	Engine::Length width = Engine::feet(3.0f);
	collisionComponent->boundingBoxes.emplace_back(Engine::Vec3<Engine::Meters>(-10.f, -10.f, -10.f), height, width, width);

	wasd.insert({ GLFW_KEY_W, { 0.f, 0.f, 1.f } });
	wasd.insert({ GLFW_KEY_S, { 0.f, 0.f, -1.f } });
	wasd.insert({ GLFW_KEY_A, { -1.f, 0.f, 0.f } });
	wasd.insert({ GLFW_KEY_D, { 1.f, 0.f, 0.f } });

	motionComponent->mass = Engine::pounds(180.f);
	motionComponent->maxSpeed = maxSpeed;
	motionComponent->selfControlled = true;

	for (auto& bb : collisionComponent->boundingBoxes) {
		const auto boundingBoxMesh = std::make_shared<Engine::BoundingBoxMesh>(bb.upperBound, bb.lowerBound);
		renderComponent->addBoundingBoxMesh(boundingBoxMesh);
	}

	addComponent(renderComponent);
	addComponent(motionComponent);
	addComponent(collisionComponent);
}

void Game::Player::updateJetpack() {
	if (Engine::Input::getKeyboard().keys[GLFW_KEY_J].pressed) {
		jetpack = !jetpack;
	}

	if (jetpack && Engine::Input::getKeyboard().keys[GLFW_KEY_SPACE].held) {
		Engine::Force boostForce;
		if (Engine::Input::getKeyboard().keys[GLFW_KEY_LEFT_SHIFT].held && boostFuel > 0) {
			boostForce = Engine::newtons(2000.f);
			boostFuel -= 1;
		}
		else {
			boostFuel += 1;
			boostFuel = std::min(boostFuel, 1000);
		}

		applyForce(getComponent<Engine::Physics::MotionComponent>().get(), Engine::Vec3<Engine::Newtons>(0.f, jetpackForce + boostForce, 0.f));

		for (auto& [key, direction] : wasd) {
			if (Engine::Input::getKeyboard().keys[key].held) {
				applyForce(getComponent<Engine::Physics::MotionComponent>().get(), Engine::Vec3<Engine::Newtons>(jetpackSideForce + boostForce, 0.f, jetpackSideForce + boostForce) * Engine::getCamera().getCameraDirectionRelativeToOrigin(direction));
			}
		}
	}
}

void Game::Player::updateMovement() {
	for (auto& [key, direction] : wasd) {
		if (Engine::Input::getKeyboard().keys[key].held && !getComponent<Engine::Physics::MotionComponent>()->airborne) {
			applyForce(getComponent<Engine::Physics::MotionComponent>().get(), moveForce * Engine::getCamera().getCameraDirectionRelativeToOrigin(direction) * Engine::Vec3(1.f, 0.f, 1.f));
		}
	}

	if (Engine::Input::getKeyboard().keys[GLFW_KEY_LEFT_SHIFT].held) {
		getComponent<Engine::Physics::MotionComponent>()->maxSpeed = shiftMaxSpeed;
	}
	else {
		getComponent<Engine::Physics::MotionComponent>()->maxSpeed = maxSpeed;
	}

	if (Engine::Input::getKeyboard().keys[GLFW_KEY_SPACE].pressed && !getComponent<Engine::Physics::MotionComponent>()->airborne) {
		applyImpulse(getComponent<Engine::Physics::MotionComponent>().get(), Engine::Vec3<Engine::Newtons>(0.f, jumpForce, 0.f), Engine::seconds(0.5f));
		getComponent<Engine::Physics::MotionComponent>()->airborne = true;
	}
}

void Game::Player::update() {
	updateJetpack();
	updateMovement();

	for (auto& bb : getComponent<Engine::Physics::CollisionComponent>()->boundingBoxes) {
		bb.setPosition(getComponent<Engine::Physics::MotionComponent>()->position);
	}
	
	Engine::getCamera().setPosition(getComponent<Engine::Physics::MotionComponent>()->position + Engine::Vec3<Engine::Feet>(0.f, 6.f, 0.f));

	getComponent<Engine::RenderComponent>()->updateBoundingBoxMeshes();
	getComponent<Engine::RenderComponent>()->setRender(true, true);
}

void Game::Player::render() {
	Engine::Debug::addImguiCallback([this] {
		ImGui::Begin("Player");
		Engine::Debug::printVector("Player Position", getComponent<Engine::Physics::MotionComponent>()->position.as<Engine::Meters>(), Engine::Meters::UnitName);
		Engine::Debug::printVector("Player Bounding Box Position", getComponent<Engine::Physics::CollisionComponent>()->boundingBoxes[0].getCenter().as<Engine::Meters>(), Engine::Meters::UnitName);
		Engine::Debug::printVector("Player Velocity", getComponent<Engine::Physics::MotionComponent>()->velocity.as<Engine::MetersPerSecond>(), Engine::MetersPerSecond::UnitName);
		Engine::Debug::printVector("Player Acceleration", getComponent<Engine::Physics::MotionComponent>()->acceleration.as<Engine::MetersPerSecondSquared>(), Engine::MetersPerSecondSquared::UnitName);

		Engine::Debug::printValue("Player Speed", getComponent<Engine::Physics::MotionComponent>()->getSpeed().as<Engine::MilesPerHour>(), Engine::MilesPerHour::UnitName);

		Engine::Debug::printBoolean("Player Jetpack [J]", jetpack);
		Engine::Debug::printValue("Player Boost Fuel", static_cast<float>(boostFuel), "");

		ImGui::Text("Player Collision Information");

		const auto [colliding, collisionNormal, penetration, collisionPoint] = getComponent<Engine::Physics::CollisionComponent>()->boundingBoxes[0].collisionInformation;
		Engine::Debug::printBoolean("Player Colliding", colliding);
		Engine::Debug::printVector("Collision Normal", collisionNormal.asDefaultUnits(), "");
		Engine::Debug::printValue("Penetration", penetration.as<Engine::Meters>(), Engine::Meters::UnitName);
		Engine::Debug::printVector("Collision Point", collisionPoint.as<Engine::Meters>(), Engine::Meters::UnitName);
		Engine::Debug::printBoolean("Player Airborne", getComponent<Engine::Physics::MotionComponent>()->airborne);
		Engine::Debug::printBoolean("Player Moving", getComponent<Engine::Physics::MotionComponent>()->moving);
		ImGui::End();
		});
}

#include "Player.h"

void Game::Player::initialize() {
	const auto motionComponent = std::make_shared<Engine::Physics::MotionComponent>();
	const auto collisionComponent = std::make_shared<Engine::Physics::CollisionComponent>();
	const auto renderComponent = std::make_shared<Engine::RenderComponent>();

	Engine::Units::Feet height = 6.0f;
	Engine::Units::Feet width = 3.0f;
	collisionComponent->boundingBoxes.emplace_back(Engine::Vec3<Engine::Units::Meters>(-10.f, -10.f, -10.f), height, width, width);

	wasd.insert({ GLFW_KEY_W, {0.f, 0.f, 1.f} });
	wasd.insert({ GLFW_KEY_S, {0.f, 0.f, -1.f} });
	wasd.insert({ GLFW_KEY_A, {-1.f, 0.f, 0.f} });
	wasd.insert({ GLFW_KEY_D, {1.f, 0.f, 0.f} });

	motionComponent->mass = Engine::Units::Pounds(180.f);
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
		applyForce(getComponent<Engine::Physics::MotionComponent>().get(), Engine::Vec3<Engine::Units::Newtons>(0.f, jetpackForce, 0.f));

		for (auto& [key, direction] : wasd) {
			if (Engine::Input::getKeyboard().keys[key].held) {
				applyForce(getComponent<Engine::Physics::MotionComponent>().get(), Engine::Vec3<Engine::Units::Newtons>(jetpackSideForce, 0, jetpackSideForce) * Engine::getCamera().getCameraDirectionRelativeToOrigin(direction));
			}
		}
	}
}

void Game::Player::updateMovement() {
	for (auto& [key, direction] : wasd) {
		if (Engine::Input::getKeyboard().keys[key].held && !getComponent<Engine::Physics::MotionComponent>()->airborne) {
			applyForce(getComponent<Engine::Physics::MotionComponent>().get(), moveForce * Engine::getCamera().getCameraDirectionRelativeToOrigin(direction) * Engine::Vec3<Engine::Unitless>(1.f, 0.f, 1.f));
		}
	}

	if (Engine::Input::getKeyboard().keys[GLFW_KEY_LEFT_SHIFT].held) {
		getComponent<Engine::Physics::MotionComponent>()->maxSpeed = shiftMaxSpeed;
	}
	else {
		getComponent<Engine::Physics::MotionComponent>()->maxSpeed = maxSpeed;
	}

	if (Engine::Input::getKeyboard().keys[GLFW_KEY_SPACE].pressed && !getComponent<Engine::Physics::MotionComponent>()->airborne) {
		applyImpulse(getComponent<Engine::Physics::MotionComponent>().get(), Engine::Vec3<Engine::Units::Newtons>(0.f, jumpForce, 0.f), Engine::Units::Seconds(0.5f));
		getComponent<Engine::Physics::MotionComponent>()->airborne = true;
	}
}

void Game::Player::update() {
	updateJetpack();
	updateMovement();

	for (auto& bb : getComponent<Engine::Physics::CollisionComponent>()->boundingBoxes) {
		bb.setPosition(getComponent<Engine::Physics::MotionComponent>()->position);
	}
	
	Engine::getCamera().setPosition(getComponent<Engine::Physics::MotionComponent>()->position + Engine::Vec3<Engine::Units::Feet>(0.f, 6.f, 0.f));

	getComponent<Engine::RenderComponent>()->updateBoundingBoxMeshes();
	getComponent<Engine::RenderComponent>()->setRender(true, true);
}

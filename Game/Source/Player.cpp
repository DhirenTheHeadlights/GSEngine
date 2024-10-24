#include "Player.h"

void Game::Player::initialize() {
	auto motionComponent = std::make_shared<Engine::Physics::MotionComponent>();
	auto collisionComponent = std::make_shared<Engine::Physics::CollisionComponent>();

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

	addComponent(motionComponent);
	addComponent(collisionComponent);
}

void Game::Player::updateJetpack() {
	if (Engine::Input::getKeyboard().keys[GLFW_KEY_J].pressed) {
		jetpack = !jetpack;
	}

	if (jetpack && Engine::Input::getKeyboard().keys[GLFW_KEY_SPACE].held) {
		for (auto& [key, direction] : wasd) {
			if (Engine::Input::getKeyboard().keys[key].held) {
				applyForce(getComponent<Engine::Physics::MotionComponent>().get(), Engine::Vec3<Engine::Units::Newtons>(jetpackSideForce, jetpackForce, jetpackSideForce) * Engine::getCamera().getCameraDirectionRelativeToOrigin(direction));
			}
		}
	}
}

void Game::Player::updateMovement() {
	for (auto& [key, direction] : wasd) {
		if (Engine::Input::getKeyboard().keys[key].held && !motionComponent->airborne) {
			applyForce(motionComponent.get(), moveForce * Engine::getCamera().getCameraDirectionRelativeToOrigin(direction) * Engine::Vec3<Engine::Unitless>(1.f, 0.f, 1.f));
		}
	}

	if (Engine::Input::getKeyboard().keys[GLFW_KEY_LEFT_SHIFT].held) {
		motionComponent->maxSpeed = shiftMaxSpeed;
	}
	else {
		motionComponent->maxSpeed = maxSpeed;
	}

	if (Engine::Input::getKeyboard().keys[GLFW_KEY_SPACE].pressed && !motionComponent->airborne) {
		applyImpulse(motionComponent.get(), Engine::Vec3<Engine::Units::Newtons>(0.f, jumpForce, 0.f), Engine::Units::Seconds(0.5f));
		motionComponent->airborne = true;
	}
}

void Game::Player::update() {
	updateJetpack();
	updateMovement();

	for (auto& bb : boundingBoxes) {
		bb.setPosition(motionComponent->position);
	}
	
	Engine::getCamera().setPosition(motionComponent->position + Engine::Vec3<Engine::Units::Feet>(0.f, 6.f, 0.f));
}

void Game::Player::render() {
	for (auto& bb : boundingBoxes) {
		bb.render(true);
	}
}
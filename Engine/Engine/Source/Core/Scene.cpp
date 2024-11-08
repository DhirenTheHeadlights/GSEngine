#include "Core/Scene.h"

template <typename... ComponentTypes, typename Action>
void handleComponent(const std::shared_ptr<Engine::Object>& object, Action action) {
	if ((object->getComponent<ComponentTypes>() && ...)) { // Namespace
		action(object->getComponent<ComponentTypes>()...);
	}
}

template <typename... ComponentTypes, typename SystemType, typename Action>
void handleComponent(const std::shared_ptr<Engine::Object>& object, SystemType& system, Action action) {
	if ((object->getComponent<ComponentTypes>() && ...)) { // Class
		(system.*action)(object->getComponent<ComponentTypes>()...);
	}
}

void Engine::Scene::addObject(const std::weak_ptr<Object>& object) {
	objects.push_back(object);

	if (const auto objectPtr = object.lock()) {
		handleComponent<Physics::MotionComponent>(objectPtr, physicsSystem, &Physics::System::addMotionComponent);
		handleComponent<Physics::CollisionComponent, Physics::MotionComponent>(objectPtr, broadPhaseCollisionHandler, &BroadPhaseCollisionHandler::addDynamicComponents);
		handleComponent<Physics::CollisionComponent>(objectPtr, broadPhaseCollisionHandler, &BroadPhaseCollisionHandler::addObjectComponent);
		handleComponent<RenderComponent>(objectPtr, renderer, &Renderer::addRenderComponent);
		handleComponent<LightSourceComponent>(objectPtr, renderer, &Renderer::addLightSourceComponent);
	}
}

void Engine::Scene::removeObject(const std::weak_ptr<Object>& object) {
	if (const auto objectPtr = object.lock()) {
		if (objectPtr->getSceneId().lock() == id) {
			handleComponent<Physics::MotionComponent>(objectPtr, physicsSystem, &Physics::System::removeMotionComponent);
			handleComponent<Physics::CollisionComponent>(objectPtr, broadPhaseCollisionHandler, &BroadPhaseCollisionHandler::removeComponents);
			handleComponent<RenderComponent>(objectPtr, renderer, &Renderer::removeRenderComponent);
			handleComponent<LightSourceComponent>(objectPtr, renderer, &Renderer::removeLightSourceComponent);
		}
	}

	std::erase_if(objects, [&object](const std::weak_ptr<Object>& requiredObject) {
		return requiredObject.lock() == object.lock();
		});
}

void Engine::Scene::initialize() {
	renderer.initialize();
}

void Engine::Scene::update() {
	physicsSystem.update();
	broadPhaseCollisionHandler.update();

	for (auto& object : objects) {
		if (const auto objectPtr = object.lock()) {
			objectPtr->update();
		}
	}
}

void Engine::Scene::render() {
	renderer.renderObjects();

	for (auto& object : objects) {
		if (const auto objectPtr = object.lock()) {
			objectPtr->render();
		}
	}
}

void Engine::Scene::exit() {
	// TODO
}


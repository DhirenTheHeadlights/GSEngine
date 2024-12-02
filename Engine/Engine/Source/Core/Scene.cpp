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

	/// Components are not added here; it is assumed that the object will only be ready
	///	to be initialized after all components have been added (when the scene is activated)
}

void Engine::Scene::removeObject(const std::weak_ptr<Object>& object) {
	if (const auto objectPtr = object.lock()) {
		if (objectPtr->getSceneId().lock() == id) {
			handleComponent<Physics::MotionComponent>(objectPtr, physicsSystem, &Physics::System::removeMotionComponent);
			handleComponent<Physics::CollisionComponent>(objectPtr, broadPhaseCollisionHandler, &BroadPhaseCollisionHandler::removeComponents);
			handleComponent<RenderComponent>(objectPtr, renderGroup, &Renderer::Group::removeRenderComponent);
			handleComponent<LightSourceComponent>(objectPtr, renderGroup, &Renderer::Group::removeLightSourceComponent);
		}
	}

	std::erase_if(objects, [&object](const std::weak_ptr<Object>& requiredObject) {
		return requiredObject.lock() == object.lock();
		});
}

void Engine::Scene::initialize() {
	for (auto& object : objects) {
		if (const auto objectPtr = object.lock()) {
			objectPtr->processInitialize();

			handleComponent<Physics::MotionComponent>(objectPtr, physicsSystem, &Physics::System::addMotionComponent);
			handleComponent<Physics::CollisionComponent, Physics::MotionComponent>(objectPtr, broadPhaseCollisionHandler, &BroadPhaseCollisionHandler::addDynamicComponents);
			handleComponent<Physics::CollisionComponent>(objectPtr, broadPhaseCollisionHandler, &BroadPhaseCollisionHandler::addObjectComponent);
			handleComponent<RenderComponent>(objectPtr, renderer, &Renderer::addRenderComponent);
			handleComponent<LightSourceComponent>(objectPtr, renderer, &Renderer::addLightSourceComponent);
		}
	}
}

void Engine::Scene::update() {
	physicsSystem.update();
	BroadPhaseCollisions::update(collisionObjects);

	for (auto& object : objects) {
		if (const auto objectPtr = object.lock()) {
			objectPtr->processUpdate();
		}
	}
}

void Engine::Scene::render() {
	Renderer::renderObjects(renderGroup);

	for (auto& object : objects) {
		if (const auto objectPtr = object.lock()) {
			objectPtr->processRender();
		}
	}
}

void Engine::Scene::exit() {
	// TODO
}


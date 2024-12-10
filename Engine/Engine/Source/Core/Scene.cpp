#include "Core/Scene.h"

template <typename... ComponentTypes, typename Action>
void handleComponent(const std::shared_ptr<gse::object>& object, Action action) {
	if ((object->getComponent<ComponentTypes>() && ...)) { // Namespace
		action(object->getComponent<ComponentTypes>()...);
	}
}

template <typename... ComponentTypes, typename SystemType, typename Action>
void handleComponent(const std::shared_ptr<gse::object>& object, SystemType& system, Action action) {
	if ((object->getComponent<ComponentTypes>() && ...)) { // Class
		(system.*action)(object->getComponent<ComponentTypes>()...);
	}
}

void gse::scene::addObject(const std::weak_ptr<object>& object) {
	objects.push_back(object);

	/// Components are not added here; it is assumed that the object will only be ready
	///	to be initialized after all components have been added (when the scene is activated)
}

void gse::scene::removeObject(const std::weak_ptr<object>& object) {
	if (const auto objectPtr = object.lock()) {
		if (objectPtr->getSceneId().lock() == id) {
			handleComponent<Physics::MotionComponent>(objectPtr, physicsSystem, &Physics::Group::removeMotionComponent);
			handleComponent<Physics::CollisionComponent>(objectPtr, collisionGroup, &BroadPhaseCollision::Group::removeObject);
			handleComponent<RenderComponent>(objectPtr, renderGroup, &renderer::Group::removeRenderComponent);
			handleComponent<LightSourceComponent>(objectPtr, renderGroup, &renderer::Group::removeLightSourceComponent);
		}
	}

	std::erase_if(objects, [&object](const std::weak_ptr<object>& requiredObject) {
		return requiredObject.lock() == object.lock();
		});
}

void gse::scene::initialize() {
	for (auto& object : objects) {
		if (const auto objectPtr = object.lock()) {
			objectPtr->process_initialize();

			handleComponent<Physics::MotionComponent>(objectPtr, physicsSystem, &Physics::Group::addMotionComponent);
			handleComponent<Physics::CollisionComponent, Physics::MotionComponent>(objectPtr, collisionGroup, &BroadPhaseCollision::Group::addDynamicObject);
			handleComponent<Physics::CollisionComponent>(objectPtr, collisionGroup, &BroadPhaseCollision::Group::addObject);
			handleComponent<RenderComponent>(objectPtr, renderGroup, &renderer::Group::addRenderComponent);
			handleComponent<LightSourceComponent>(objectPtr, renderGroup, &renderer::Group::addLightSourceComponent);
		}
	}
}

void gse::scene::update() {
	physicsSystem.update();
	BroadPhaseCollision::update(collisionGroup);

	for (auto& object : objects) {
		if (const auto objectPtr = object.lock()) {
			objectPtr->process_update();
		}
	}
}

void gse::scene::render() {
	renderObjects(renderGroup);

	for (auto& object : objects) {
		if (const auto objectPtr = object.lock()) {
			objectPtr->process_render();
		}
	}
}

void gse::scene::exit() {
	// TODO
}


#include "Core/Scene.h"

void Engine::Scene::addObject(const std::shared_ptr<Object>& object) {
	objects.push_back(object);

	if (isActive) {

	}
}

void Engine::Scene::removeObject(const std::shared_ptr<Object>& object) {
	std::erase_if(objects, [&object](const std::weak_ptr<Object>& requiredObject) {
		return requiredObject.lock() == object;
		});
}

void Engine::Scene::initialize() {
	for (const auto& object : objects) {
		
	}
}
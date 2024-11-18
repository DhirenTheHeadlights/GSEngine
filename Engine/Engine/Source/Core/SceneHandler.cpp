#include "Core/SceneHandler.h"

#include <ranges>

void Engine::SceneHandler::addScene(const std::shared_ptr<Scene>& scene, const std::string& tag) {
	if (!scene->getId()) {
		scene->setId(idHandler.generateID(tag));
	}

	scenes.insert({ scene->getId(), scene });
}

void Engine::SceneHandler::removeScene(const std::shared_ptr<ID>& sceneId) {
	if (const auto scene = scenes.find(sceneId); scene != scenes.end()) {
		if (scene->second->getActive()) {
			scene->second->exit();
		}
		scenes.erase(scene);
	}
}

void Engine::SceneHandler::activateScene(const std::shared_ptr<ID>& sceneId) {
	permaAssertComment(engineInitialized, "You are trying to activate a scene before the engine is initialized");
	if (const auto scene = scenes.find(sceneId); scene != scenes.end()) {
		if (!scene->second->getActive()) {
			scene->second->initialize(fbo);
			scene->second->setActive(true);
		}
	}
}

void Engine::SceneHandler::deactivateScene(const std::shared_ptr<ID>& sceneId) {
	if (const auto scene = scenes.find(sceneId); scene != scenes.end()) {
		if (scene->second->getActive()) {
			scene->second->exit();
			scene->second->setActive(false);
		}
	}
}

void Engine::SceneHandler::update() {
	for (const auto& scene : scenes | std::views::values) {
		if (scene->getActive()) {
			scene->update();
		}
	}
}

void Engine::SceneHandler::render() {
	for (const auto& scene : scenes | std::views::values) {
		if (scene->getActive()) {
			scene->render();
		}
	}
}

void Engine::SceneHandler::exit() {
	for (const auto& scene : scenes | std::views::values) {
		if (scene->getActive()) {
			scene->exit();
		}
	}
}

std::vector<std::shared_ptr<Engine::Scene>> Engine::SceneHandler::getActiveScenes() const {
	std::vector<std::shared_ptr<Scene>> activeScenes;
	activeScenes.reserve(scenes.size());
	for (const auto& scene : scenes | std::views::values) {
		if (scene->getActive()) {
			activeScenes.push_back(scene);
		}
	}
	return activeScenes;
}

std::vector<std::shared_ptr<Engine::ID>> Engine::SceneHandler::getActiveSceneIds() const {
	std::vector<std::shared_ptr<ID>> activeSceneIds;
	activeSceneIds.reserve(scenes.size());
	for (const auto& id : scenes | std::views::keys) {
		if (scenes.at(id)->getActive()) {
			activeSceneIds.push_back(id);
		}
	}
	return activeSceneIds;
}

std::shared_ptr<Engine::Scene> Engine::SceneHandler::getScene(const std::shared_ptr<ID>& sceneId) const {
	if (const auto scene = scenes.find(sceneId); scene != scenes.end()) {
		return scene->second;
	}
	return nullptr;
}


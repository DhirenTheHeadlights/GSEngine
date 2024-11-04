#pragma once

#include "Core/Scene.h"

namespace Engine {
	class SceneHandler {
	public:
		SceneHandler(IDHandler& idHandler) : idHandler(idHandler) {}

		void addScene(const std::shared_ptr<Scene>& scene, const std::string& tag);
		void removeScene(const std::shared_ptr<ID>& sceneId);

		void activateScene(const std::shared_ptr<ID>& sceneId);
		void deactivateScene(const std::shared_ptr<ID>& sceneId);
		void update();
		void render();
		void exit();

		std::vector<std::shared_ptr<Scene>> getActiveScenes() const;
		std::vector<std::shared_ptr<ID>> getActiveSceneIds() const;
		std::shared_ptr<Scene> getScene(const std::shared_ptr<ID>& sceneId) const;
	private:
		IDHandler& idHandler;

		std::unordered_map<std::shared_ptr<ID>, std::shared_ptr<Scene>> scenes;
	};
}
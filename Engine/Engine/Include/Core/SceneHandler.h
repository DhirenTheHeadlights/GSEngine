#pragma once

#include "Core/Scene.h"

namespace gse {
	class SceneHandler {
	public:
		SceneHandler() {}

		void addScene(const std::shared_ptr<scene>& scene, const std::string& tag);
		void removeScene(const std::shared_ptr<ID>& sceneId);

		void activateScene(const std::shared_ptr<ID>& sceneId);
		void deactivateScene(const std::shared_ptr<ID>& sceneId);
		void update();
		void render();
		void exit();

		void setEngineInitialized(const bool initialized) { this->engineInitialized = initialized; }
		void queueSceneTrigger(const std::shared_ptr<ID>& id, const std::function<bool()>& trigger);

		std::vector<std::shared_ptr<scene>> getActiveScenes() const;
		std::vector<std::shared_ptr<ID>> getAllScenes() const;
		std::vector<std::shared_ptr<ID>> getActiveSceneIds() const;
		std::shared_ptr<scene> getScene(const std::shared_ptr<ID>& sceneId) const;
	private:
		std::optional<GLuint> fbo = std::nullopt;
		bool engineInitialized = false;

		std::unordered_map<std::shared_ptr<ID>, std::shared_ptr<scene>> scenes;
		std::unordered_map<std::shared_ptr<ID>, std::function<bool()>> sceneTriggers;
	};
}
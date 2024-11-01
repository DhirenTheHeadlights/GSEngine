#pragma once

#include "Core/Scene.h"

namespace Engine {
	class SceneHandler {
	public:
		void addScene(const std::shared_ptr<Scene>& scene);
		void removeScene(const std::shared_ptr<Scene>& scene);

		void setActiveScene(const std::shared_ptr<Scene>& scene);

		void initialize();
		void run();
		void exit();
	private:
		std::unordered_map<std::shared_ptr<ID>, std::shared_ptr<Scene>> scenes;
	};
}
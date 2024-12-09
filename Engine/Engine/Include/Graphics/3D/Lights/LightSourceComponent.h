#pragma once

#include <glm/glm.hpp>

#include "Graphics/3D/Lights/Light.h"
#include "Core/EngineComponent.h"

namespace Engine {
	class LightSourceComponent : public EngineComponent {
	public:
		LightSourceComponent(ID* id) : EngineComponent(id) {}
		LightSourceComponent(ID* id, const std::vector<std::shared_ptr<Light>>& lights) : EngineComponent(id), lights(lights) {}

		void addLight(const std::shared_ptr<Light>& light) {
			lights.push_back(light);
		}

		void removeLight(const std::shared_ptr<Light>& light) {
			std::erase(lights, light);
		}

		std::vector<LightRenderQueueEntry> getRenderQueueEntries() const {
			std::vector<LightRenderQueueEntry> entries;
			entries.reserve(lights.size());
			for (const auto& light : lights) {
				entries.push_back(light->getRenderQueueEntry());
			}
			return entries;
		}

		std::vector<std::shared_ptr<Light>>& getLights() {
			return lights;
		}
	private:
		std::vector<std::shared_ptr<Light>> lights;
	};
}
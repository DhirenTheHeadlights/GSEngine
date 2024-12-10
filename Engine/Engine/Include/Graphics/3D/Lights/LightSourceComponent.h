#pragma once

#include <glm/glm.hpp>

#include "Graphics/3D/Lights/Light.h"
#include "Core/EngineComponent.h"

namespace gse {
	class LightSourceComponent : public engine_component {
	public:
		LightSourceComponent(ID* id) : engine_component(id) {}
		LightSourceComponent(ID* id, const std::vector<std::shared_ptr<Light>>& lights) : engine_component(id), lights(lights) {}

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
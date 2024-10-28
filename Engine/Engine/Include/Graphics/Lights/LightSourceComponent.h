#pragma once

#include <glm/glm.hpp>

#include "Graphics/Lights/Light.h"

namespace Engine {
	class LightSourceComponent {
	public:
		LightSourceComponent() = default;
		LightSourceComponent(const std::vector<std::shared_ptr<Light>>& lights) : lights(lights) {}

		std::vector<LightRenderQueueEntry> getRenderQueueEntries() const {
			std::vector<LightRenderQueueEntry> entries;
			for (const auto& light : lights) {
				entries.push_back(light->getRenderQueueEntry());
			}
			return entries;
		}
	private:
		std::vector<std::shared_ptr<Light>> lights;
	};
}
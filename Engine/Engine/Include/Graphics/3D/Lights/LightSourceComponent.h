#pragma once

#include <glm/glm.hpp>

#include "Core/EngineComponent.h"
#include "Graphics/3D/Lights/Light.h"

namespace gse {
	class light_source_component final : public component {
	public:
		light_source_component(id* id) : component(id) {}
		light_source_component(id* id, const std::vector<std::shared_ptr<light>>& lights) : component(id), m_lights(lights) {}

		void add_light(const std::shared_ptr<light>& light) {
			m_lights.push_back(light);
		}

		void remove_light(const std::shared_ptr<light>& light) {
			std::erase(m_lights, light);
		}

		std::vector<light_render_queue_entry> get_render_queue_entries() const {
			std::vector<light_render_queue_entry> entries;
			entries.reserve(m_lights.size());
			for (const auto& light : m_lights) {
				entries.push_back(light->get_render_queue_entry());
			}
			return entries;
		}

		std::vector<std::shared_ptr<light>>& get_lights() {
			return m_lights;
		}
	private:
		std::vector<std::shared_ptr<light>> m_lights;
	};
}
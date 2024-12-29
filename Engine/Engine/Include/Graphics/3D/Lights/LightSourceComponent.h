#pragma once

#include <glm/glm.hpp>

#include "Core/EngineComponent.h"
#include "Graphics/3D/Lights/Light.h"
#include "Physics/Vector/Math.h"

namespace gse {
	class light_source_component final : public component {
	public:
		light_source_component(id* id) : component(id) {}
		light_source_component(id* id, std::vector<std::unique_ptr<light>> lights) : component(id), m_lights(std::move(lights)) {}

		light_source_component(light_source_component&&) noexcept = default;
		light_source_component& operator=(light_source_component&&) noexcept = default;

		light_source_component(const light_source_component&) = delete;
		light_source_component& operator=(const light_source_component&) = delete;

		void add_light(std::unique_ptr<light> light) {
			m_lights.push_back(std::move(light));
		}

		void remove_light(const std::unique_ptr<light>& light) {
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

		std::vector<light*> get_lights() const {
			return get_pointers(m_lights);
		}
	private:
		std::vector<std::unique_ptr<light>> m_lights;
	};
}

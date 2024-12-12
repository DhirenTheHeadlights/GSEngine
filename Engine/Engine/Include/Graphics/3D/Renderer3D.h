#pragma once

#include "Graphics/RenderComponent.h"
#include "Graphics/3D/Camera.h"
#include "Lights/LightSourceComponent.h"

namespace gse::renderer {
	class group {
	public:
		void add_render_component(const std::shared_ptr<render_component>& new_render_component);
		void add_light_source_component(const std::shared_ptr<light_source_component>& new_light_source_component);
		void remove_render_component(const std::shared_ptr<render_component>& render_component_to_remove);
		void remove_light_source_component(const std::shared_ptr<light_source_component>& light_source_component_to_remove);

		auto get_render_components() -> std::vector<std::weak_ptr<render_component>>& {
			return m_render_components;
		}
		auto get_light_source_components() -> std::vector<std::weak_ptr<light_source_component>>& {
			return m_light_source_components;
		}
	private:
		std::vector<std::weak_ptr<render_component>> m_render_components;
		std::vector<std::weak_ptr<light_source_component>> m_light_source_components;
	};

	void initialize3d();
	void render_objects(group& group);

	camera& get_camera();
}

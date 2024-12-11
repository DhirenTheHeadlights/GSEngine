#pragma once

#include <vector>
#include <glm/glm.hpp>

#include "MotionComponent.h"
#include "Core/Object/Object.h"
#include "Graphics/3D/BoundingBox.h"

namespace gse::physics {
	class group {
	public:
		void add_motion_component(const std::shared_ptr<motion_component>& object);
		void remove_motion_component(const std::shared_ptr<motion_component>& object);

		void update();
	private:
		std::vector<std::weak_ptr<motion_component>> m_motion_components;
	};

	void apply_force(motion_component* component, const vec3<force>& force);
	void apply_impulse(motion_component* component, const vec3<force>& force, const time& duration);
	void update_entity(motion_component* component);
	void resolve_collision(bounding_box& dynamic_bounding_box, const std::weak_ptr<motion_component>& dynamic_motion_component, const collision_information& collision_info);
}

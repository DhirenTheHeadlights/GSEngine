#pragma once
#include <vector>

#include "CollisionComponent.h"
#include "Core/Object/Object.h"
#include "Physics/MotionComponent.h"

namespace gse::broad_phase_collision {
	class group {
		struct object {
			object(const std::weak_ptr<physics::collision_component>& collision_component) : collision_component(collision_component) {}
			std::weak_ptr<physics::collision_component> collision_component;
		};

		struct dynamic_object : object {
			dynamic_object(const std::weak_ptr<physics::collision_component>& collision_component, const std::weak_ptr<physics::motion_component>& motion_component) :
				object(collision_component), motion_component(motion_component) {}
			std::weak_ptr<physics::motion_component> motion_component;
		};
	public:
		void add_object(const std::shared_ptr<physics::collision_component>& collision_component);
		void remove_object(const std::shared_ptr<physics::collision_component>& collision_component);

		void add_dynamic_object(const std::shared_ptr<physics::collision_component>& collision_component, const std::shared_ptr<physics::motion_component>& motion_component);

		auto get_dynamic_objects() -> std::vector<dynamic_object>& { return m_dynamic_objects; }
		auto get_objects() -> std::vector<object>& { return m_objects; }
	private:
		std::vector<dynamic_object> m_dynamic_objects;
		std::vector<object> m_objects;
	};

	bool check_collision(const bounding_box& box1, const bounding_box& box2);
	bool check_collision(const bounding_box& dynamic_box, const std::shared_ptr<physics::motion_component>& dynamic_motion_component, const bounding_box& other_box);
	bool check_collision(const std::shared_ptr<physics::collision_component>& dynamic_object_collision_component, const std::shared_ptr<physics::motion_component>& dynamic_object_motion_component, const std::shared_ptr<physics::collision_component>& other_collision_component);
	bool check_collision(const vec3<length>& point, const bounding_box& box);
	collision_information calculate_collision_information(const bounding_box& box1, const bounding_box& box2);
	void set_collision_information(const bounding_box& box1, const bounding_box& box2);

	void update(group& group);
}


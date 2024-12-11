#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Physics/Units/Units.h"
#include "Physics/Vector/Vec3.h"

namespace gse {
	class camera {
	public:
		camera(const vec3<length>& initial_position = { 0.f }) : m_position(initial_position) {}

		void move_relative_to_origin(const vec3<>& direction, float distance, float delta_time);

		void process_mouse_movement(glm::vec2& offset);
		void update_camera_vectors();

		void set_position(const vec3<length>& position) { this->m_position = position; }

		glm::mat4 get_view_matrix() const;
		glm::mat4 get_projection_matrix();
		vec3<length> get_position() const;
		vec3<> get_camera_direction_relative_to_origin(const vec3<>& direction) const;
	private:
		vec3<length> m_position;
		vec3<length> m_front = vec3<units::meters>(0.0f, 0.0f, -1.0f);
		vec3<length> m_up = vec3<units::meters>(0.0f, 1.0f, 0.0f);
		vec3<length> m_right = vec3<units::meters>(1.0f, 0.0f, 0.0f);
		vec3<length> m_world_up = vec3<units::meters>(0.0f, 1.0f, 0.0f);

		float m_yaw = -90.0f;
		float m_pitch = 0.0f;

		float m_movement_speed = 2.5f;
		float m_mouse_sensitivity = 0.1f;
	};
}

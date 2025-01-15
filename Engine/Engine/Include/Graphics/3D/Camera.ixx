export module gse.graphics.camera;

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

import gse.physics.math;

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

import gse.platform.glfw.window;

void gse::camera::move_relative_to_origin(const vec3<>& direction, const float distance, const float delta_time) {
	const auto norm_direction = normalize(direction).as_default_units();
	const vec3<length> camera_direction =
		m_right * norm_direction.x +
		m_up * norm_direction.y +
		m_front * norm_direction.z;
	m_position += camera_direction * distance * m_movement_speed * delta_time;
}

void gse::camera::process_mouse_movement(glm::vec2& offset) {
	offset *= m_mouse_sensitivity;
	m_yaw += offset.x;
	m_pitch -= offset.y;
	if (m_pitch > 89.0f) m_pitch = 89.0f;
	if (m_pitch < -89.0f) m_pitch = -89.0f;
	update_camera_vectors();
}

void gse::camera::update_camera_vectors() {
	const vec3<length> newFront(
		cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch)),
		sin(glm::radians(m_pitch)),
		sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch))
	);
	m_front = normalize(newFront);
	m_right = normalize(cross(m_front, m_world_up));
	m_up = normalize(cross(m_right, m_front));
}

glm::mat4 gse::camera::get_view_matrix() const {
	return lookAt(m_position.as<units::meters>(), (m_position + m_front).as<units::meters>(), m_up.as<units::meters>());
}

glm::mat4 gse::camera::get_projection_matrix() {
	const glm::vec2 view_port_size = window::get_frame_buffer_size();
	const float aspect_ratio = view_port_size.x / view_port_size.y;
	return glm::perspective(glm::radians(45.0f), aspect_ratio, 0.1f, 10000000.f);
}

gse::vec3<gse::length> gse::camera::get_position() const {
	return m_position;
}

gse::vec3<> gse::camera::get_camera_direction_relative_to_origin(const vec3<>& direction) const {
	return { m_right.as<units::meters>() * direction.as_default_units().x + m_up.as<units::meters>() * direction.as_default_units().y + m_front.as<units::meters>() * direction.as_default_units().z };
}

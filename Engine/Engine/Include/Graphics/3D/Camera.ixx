module;

#include <glm/gtc/matrix_transform.hpp>

export module gse.graphics.camera;

import glm;

import gse.physics.math.units;
import gse.physics.math.vector;

export namespace gse {
	class camera {
	public:
		camera(const vec3<length>& initial_position = { 0.f }) : m_position(initial_position) {}

		auto move_relative_to_origin(const vec3<>& direction, float distance, float delta_time) -> void;

		auto process_mouse_movement(const glm::vec2& offset) -> void;
		auto update_camera_vectors() -> void;

		auto set_position(const vec3<length>& position) -> void { this->m_position = position; }

		auto get_view_matrix() const -> glm::mat4;
		auto get_projection_matrix() -> glm::mat4;
		auto get_position() const -> vec3<length>;
		auto get_camera_direction_relative_to_origin(const vec3<>& direction) const -> vec3<>;
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

import gse.physics.math.vector_math;
import gse.platform.glfw.window;

auto gse::camera::move_relative_to_origin(const vec3<>& direction, const float distance, const float delta_time) -> void {
	const auto norm_direction = normalize(direction).as_default_units();
	const vec3<length> camera_direction =
		m_right * norm_direction.x +
		m_up * norm_direction.y +
		m_front * norm_direction.z;
	m_position += camera_direction * distance * m_movement_speed * delta_time;
}

auto gse::camera::process_mouse_movement(const glm::vec2& offset) -> void {
	const glm::vec2 transformed_offset = offset * m_mouse_sensitivity;
	m_yaw += transformed_offset.x;
	m_pitch -= transformed_offset.y;
	if (m_pitch > 89.0f) m_pitch = 89.0f;
	if (m_pitch < -89.0f) m_pitch = -89.0f;
	update_camera_vectors();
}

auto gse::camera::update_camera_vectors() -> void {
	const vec3<length> newFront(
		cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch)),
		sin(glm::radians(m_pitch)),
		sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch))
	);
	m_front = normalize(newFront);
	m_right = normalize(cross(m_front, m_world_up));
	m_up = normalize(cross(m_right, m_front));
}

auto gse::camera::get_view_matrix() const -> glm::mat4 {
	return lookAt(m_position.as<units::meters>(), (m_position + m_front).as<units::meters>(), m_up.as<units::meters>());
}

auto gse::camera::get_projection_matrix() -> glm::mat4 {
	const glm::vec2 view_port_size = window::get_frame_buffer_size();
	const float aspect_ratio = view_port_size.x / view_port_size.y;
	return glm::perspective(glm::radians(45.0f), aspect_ratio, 0.1f, 10000000.f);
}

auto gse::camera::get_position() const -> gse::vec3<gse::length> {
	return m_position;
}

auto gse::camera::get_camera_direction_relative_to_origin(const vec3<>& direction) const -> gse::vec3<> {
	return { m_right.as<units::meters>() * direction.as_default_units().x + m_up.as<units::meters>() * direction.as_default_units().y + m_front.as<units::meters>() * direction.as_default_units().z };
}

